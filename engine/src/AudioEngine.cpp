#include "pink/engine/AudioEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace pink
{
// ===========================================================================
// Immutable render snapshot (built on the message thread, read by the audio thread)
// ===========================================================================
struct TrackPlan
{
    TrackType type = TrackType::Instrument;
    Instrument* instrument = nullptr;
    TrackRuntime* rt = nullptr;
    int channel = 0;
    bool muted = false;
    std::vector<const MidiClip*> midiClips;
    std::vector<const AudioClip*> audioClips;
};

struct ChannelPlan
{
    struct Send { int target; int slot; bool preFader; };

    int index = 0;
    MixerChannel* ch = nullptr;
    std::vector<Processor*> inserts;
    std::vector<Send> sends;
    int output = -1;
    int sidechainSource = -1;
    bool soloMuted = false;
};

struct EngineSnapshot
{
    TempoMap tempo;
    uint64_t tempoVersion = 0;
    std::vector<TrackPlan> tracks;
    std::vector<ChannelPlan> order;              // topological processing order
    std::vector<MixerChannel*> channelByIndex;   // nullptr = channel not processed
    int liveInputTrack = -1;
    std::vector<std::shared_ptr<const void>> keepAlive;
};

// ===========================================================================
namespace
{
void addWithRamp (const AudioBuffer& src, AudioBuffer& dst, int n, float g0, float g1) noexcept
{
    const int channels = std::min (src.getNumChannels(), dst.getNumChannels());
    const float step = (g1 - g0) / (float) std::max (1, n);
    for (int c = 0; c < channels; ++c)
    {
        const float* s = src.getReadPointer (c);
        float* d = dst.getWritePointer (c);
        float g = g0;
        for (int i = 0; i < n; ++i)
        {
            g += step;
            d[i] += s[i] * g;
        }
    }
}

void emitActiveNoteOffs (TrackRuntime& rt, int offset) noexcept
{
    for (int ch = 0; ch < 16; ++ch)
        for (int key = 0; key < 128; ++key)
        {
            auto& count = rt.activeNotes[(size_t) ch][(size_t) key];
            if (count > 0)
            {
                rt.events.add (MidiEvent::noteOff (offset, ch, key));
                count = 0;
            }
        }
}

inline double cubicHermite (const float* d, int64_t numFrames, double pos) noexcept
{
    const int64_t i1 = (int64_t) pos;
    const double t = pos - (double) i1;
    auto at = [&] (int64_t i) -> double { return (i >= 0 && i < numFrames) ? d[i] : 0.0; };
    const double y0 = at (i1 - 1), y1 = at (i1), y2 = at (i1 + 1), y3 = at (i1 + 2);
    const double c1 = 0.5 * (y2 - y0);
    const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
    const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}
} // namespace

// ===========================================================================
AudioEngine::AudioEngine() = default;
AudioEngine::~AudioEngine() = default;

void AudioEngine::prepare (double newSampleRate, int newMaxBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maxBlockSize = std::clamp (newMaxBlockSize, 16, 8192);
    ++generation;

    clickBuffer.setSize (2, maxBlockSize);
    metronome.prepare (sampleRate);
    loudness.prepare (sampleRate);
    truePeak.prepare();
    correlation.prepare (sampleRate);

    exchange.collectAllWhileStopped();
    commit();
}

void AudioEngine::prepareObjects()
{
    if (generation == 0)
    {
        // commit() before prepare(): use sensible defaults so the engine is always usable
        sampleRate = 48000.0;
        maxBlockSize = 512;
        generation = 1;
        clickBuffer.setSize (2, maxBlockSize);
        metronome.prepare (sampleRate);
        loudness.prepare (sampleRate);
        truePeak.prepare();
        correlation.prepare (sampleRate);
    }

    auto prepareProcessor = [this] (Processor& p)
    {
        if (p.enginePreparedGeneration != generation)
        {
            p.prepare (sampleRate, maxBlockSize);
            p.enginePreparedGeneration = generation;
        }
    };

    for (auto& t : project.tracks)
    {
        if (t.instrument != nullptr)
            prepareProcessor (*t.instrument);
        if (t.runtime == nullptr)
            t.runtime = std::make_shared<TrackRuntime>();
        if (t.runtime->enginePreparedGeneration != generation)
        {
            t.runtime->prepare (maxBlockSize);
            t.runtime->enginePreparedGeneration = generation;
        }
    }

    for (int i = 0; i < project.mixer.getNumChannels(); ++i)
    {
        auto& c = project.mixer.channel (i);
        if (c.rt->enginePreparedGeneration != generation)
        {
            c.rt->prepare (sampleRate, maxBlockSize);
            c.rt->enginePreparedGeneration = generation;
        }
        for (auto& p : c.inserts)
            prepareProcessor (*p);
    }
}

void AudioEngine::commit()
{
    prepareObjects();

    auto snap = std::make_unique<EngineSnapshot>();
    auto sameTempo = [] (const TempoMap& a, const TempoMap& b)
    {
        const auto& ta = a.getTempoPoints(); const auto& tb = b.getTempoPoints();
        if (ta.size() != tb.size()) return false;
        for (size_t i = 0; i < ta.size(); ++i)
            if (ta[i].ppq != tb[i].ppq || ta[i].bpm != tb[i].bpm) return false;
        return true;
    };
    if (! sameTempo (project.tempo, uiTempo))
        ++tempoVersion;
    snap->tempo = project.tempo;
    snap->tempoVersion = tempoVersion;
    uiTempo = project.tempo;

    auto& mixer = project.mixer;
    const int numChannels = mixer.getNumChannels();

    // ---- which channels must be processed ---------------------------------
    std::vector<char> active ((size_t) numChannels, 0);
    active[MixerModel::kMaster] = 1;
    for (const auto& t : project.tracks)
        if (t.mixerChannel >= 0 && t.mixerChannel < numChannels)
            active[(size_t) t.mixerChannel] = 1;
    for (int i = 0; i < numChannels; ++i)
        if (! mixer.channel (i).inserts.empty())
            active[(size_t) i] = 1;

    std::vector<std::vector<int>> downstream ((size_t) numChannels);
    for (int i = 0; i < numChannels; ++i)
        downstream[(size_t) i] = mixer.getDownstream (i);

    for (bool changed = true; changed;)
    {
        changed = false;
        for (int i = 0; i < numChannels; ++i)
        {
            if (! active[(size_t) i]) continue;
            for (int d : downstream[(size_t) i])
                if (! active[(size_t) d]) { active[(size_t) d] = 1; changed = true; }
            const int sc = mixer.channel (i).sidechainSource;
            if (sc >= 0 && ! active[(size_t) sc]) { active[(size_t) sc] = 1; changed = true; }
        }
    }

    // ---- topological order (Kahn) -------------------------------------------
    std::vector<int> inDegree ((size_t) numChannels, 0);
    for (int i = 0; i < numChannels; ++i)
        if (active[(size_t) i])
            for (int d : downstream[(size_t) i])
                if (active[(size_t) d]) ++inDegree[(size_t) d];

    std::vector<int> order, queue;
    for (int i = 0; i < numChannels; ++i)
        if (active[(size_t) i] && inDegree[(size_t) i] == 0) queue.push_back (i);
    while (! queue.empty())
    {
        const int n = queue.front();
        queue.erase (queue.begin());
        order.push_back (n);
        for (int d : downstream[(size_t) n])
            if (active[(size_t) d] && --inDegree[(size_t) d] == 0) queue.push_back (d);
    }
    // channels caught in a (should-be-impossible) cycle are simply not processed

    // ---- solo ----------------------------------------------------------------
    std::vector<char> soloKeep ((size_t) numChannels, 1);
    bool anySolo = false;
    for (int i = 1; i < numChannels; ++i)
        anySolo = anySolo || mixer.channel (i).solo;

    if (anySolo)
    {
        std::fill (soloKeep.begin(), soloKeep.end(), 0);
        std::vector<std::vector<int>> upstream ((size_t) numChannels);
        for (int i = 0; i < numChannels; ++i)
        {
            const auto& c = mixer.channel (i);
            if (c.output >= 0) upstream[(size_t) c.output].push_back (i);
            for (const auto& s : c.sends)
                if (s.target >= 0) upstream[(size_t) s.target].push_back (i);
        }

        auto flood = [&] (int start, bool forward)
        {
            std::vector<int> stack { start };
            std::vector<char> seen ((size_t) numChannels, 0);
            while (! stack.empty())
            {
                const int n = stack.back();
                stack.pop_back();
                if (seen[(size_t) n]) continue;
                seen[(size_t) n] = 1;
                soloKeep[(size_t) n] = 1;
                if (forward)
                {
                    const auto& c = mixer.channel (n);
                    if (c.output >= 0) stack.push_back (c.output);
                    for (const auto& s : c.sends) if (s.target >= 0) stack.push_back (s.target);
                }
                else
                {
                    for (int u : upstream[(size_t) n]) stack.push_back (u);
                }
            }
        };

        for (int i = 1; i < numChannels; ++i)
            if (mixer.channel (i).solo)
            {
                flood (i, true);
                flood (i, false);
            }
        soloKeep[MixerModel::kMaster] = 1;
    }

    // ---- channel plans --------------------------------------------------------
    snap->channelByIndex.assign ((size_t) numChannels, nullptr);
    for (int i = 0; i < numChannels; ++i)
        if (active[(size_t) i])
            snap->channelByIndex[(size_t) i] = mixer.channel (i).rt.get();

    for (int index : order)
    {
        const auto& model = mixer.channel (index);
        ChannelPlan cp;
        cp.index = index;
        cp.ch = model.rt.get();
        cp.output = model.output;
        cp.sidechainSource = model.sidechainSource;
        cp.soloMuted = ! soloKeep[(size_t) index];
        for (const auto& p : model.inserts)
        {
            cp.inserts.push_back (p.get());
            snap->keepAlive.push_back (p);
        }
        for (int s = 0; s < (int) model.sends.size(); ++s)
            if (model.sends[(size_t) s].target >= 0)
                cp.sends.push_back ({ model.sends[(size_t) s].target, s, model.sends[(size_t) s].preFader });
        snap->keepAlive.push_back (model.rt);
        snap->order.push_back (std::move (cp));
    }

    // ---- tracks ---------------------------------------------------------------
    for (int ti = 0; ti < (int) project.tracks.size(); ++ti)
    {
        const auto& t = project.tracks[(size_t) ti];
        TrackPlan tp;
        tp.type = t.type;
        tp.instrument = t.type == TrackType::Instrument ? t.instrument.get() : nullptr;
        tp.rt = t.runtime.get();
        tp.channel = (t.mixerChannel >= 0 && t.mixerChannel < numChannels) ? t.mixerChannel : 0;
        tp.muted = t.muted;
        for (const auto& c : t.midiClips)  { tp.midiClips.push_back (c.get());  snap->keepAlive.push_back (c); }
        for (const auto& c : t.audioClips) { tp.audioClips.push_back (c.get()); snap->keepAlive.push_back (c); }
        if (t.instrument != nullptr) snap->keepAlive.push_back (t.instrument);
        snap->keepAlive.push_back (t.runtime);
        snap->tracks.push_back (std::move (tp));
    }

    if (project.liveInputTrack >= 0 && project.liveInputTrack < (int) project.tracks.size()
        && project.tracks[(size_t) project.liveInputTrack].type == TrackType::Instrument)
        snap->liveInputTrack = project.liveInputTrack;

    exchange.publish (std::move (snap));
}

void AudioEngine::collectGarbage()
{
    exchange.collectGarbage();
}

int AudioEngine::getActiveChannelCount() const noexcept
{
    const auto* s = exchange.getPublished();
    return s != nullptr ? (int) s->order.size() : 0;
}

// ===========================================================================
// Transport commands
// ===========================================================================
void AudioEngine::push (CommandType type, double a, double b)
{
    commands.push ({ type, a, b });
}

void AudioEngine::play()                         { push (CommandType::Play); }
void AudioEngine::stop()                         { push (CommandType::Stop); }
void AudioEngine::togglePlay()                   { push (CommandType::TogglePlay); }
void AudioEngine::locate (double ppq)            { push (CommandType::Locate, ppq); }
void AudioEngine::setLoop (double s, double e)   { push (CommandType::SetLoop, s, e); uiLoopStart = s; uiLoopEnd = e; }
void AudioEngine::setLoopEnabled (bool on)       { push (CommandType::SetLoopEnabled, on ? 1.0 : 0.0); }
void AudioEngine::setMetronomeEnabled (bool on)  { push (CommandType::SetMetronome, on ? 1.0 : 0.0); }
void AudioEngine::setMetronomeLevelDb (float db) { push (CommandType::SetMetronomeLevel, db); }
void AudioEngine::resetLoudnessMeters()          { push (CommandType::ResetMeters); }
void AudioEngine::panic()                        { push (CommandType::Panic); }

TransportInfo AudioEngine::getTransportInfo() const noexcept
{
    TransportInfo info;
    info.playing = uiPlaying.load();
    info.ppq = uiPpq.load();
    info.seconds = uiTempo.ppqToSeconds (info.ppq);
    info.bpm = uiTempo.bpmAt (info.ppq);
    info.position = uiTempo.ppqToBarBeatTick (info.ppq);
    info.loopEnabled = uiLoopEnabled.load();
    info.loopStart = uiLoopStart.load();
    info.loopEnd = uiLoopEnd.load();
    info.metronome = uiMetronome.load();
    return info;
}

MasterMeterReadings AudioEngine::getMasterMeters() const noexcept
{
    MasterMeterReadings m;
    const auto& rt = *project.mixer.channel (MixerModel::kMaster).rt;
    m.peakL = rt.meter.getPeak (0);
    m.peakR = rt.meter.getPeak (1);
    m.rmsL = rt.meter.getRms (0);
    m.rmsR = rt.meter.getRms (1);
    m.momentaryLufs = loudness.getMomentaryLufs();
    m.shortTermLufs = loudness.getShortTermLufs();
    m.integratedLufs = loudness.getIntegratedLufs();
    m.truePeakMax = truePeak.getMaxTruePeak();
    m.truePeakBlock = truePeak.getBlockTruePeak();
    m.correlation = correlation.getCorrelation();
    return m;
}

float AudioEngine::getInputPeak (int channel) const noexcept
{
    return (channel >= 0 && channel < kMaxInputMeters) ? inputPeaks[(size_t) channel].load() : 0.0f;
}

void AudioEngine::handleCommands() noexcept
{
    Command c;
    while (commands.pop (c))
    {
        auto& t = transport;
        switch (c.type)
        {
            case CommandType::TogglePlay:
                c.type = t.playing ? CommandType::Stop : CommandType::Play;
                [[fallthrough]];
            case CommandType::Play:
            case CommandType::Stop:
                if (c.type == CommandType::Play)
                {
                    if (! t.playing)
                    {
                        t.playing = true;
                        t.playStartPpq = t.ppq;
                        t.discontinuity = true;
                    }
                }
                else if (t.playing)
                {
                    t.playing = false;
                    t.ppq = t.playStartPpq;      // return to where playback started
                    t.allNotesOffPending = true;
                    t.discontinuity = true;
                }
                else
                {
                    t.ppq = 0.0;                 // second stop: back to the start
                    t.playStartPpq = 0.0;
                }
                t.tempoVersion = 0;
                break;
            case CommandType::Locate:
                t.ppq = std::max (0.0, c.a);
                t.tempoVersion = 0; // forces samplePos to be re-derived from ppq
                if (t.playing) t.discontinuity = true;
                else t.playStartPpq = t.ppq;
                break;
            case CommandType::SetLoop:
                if (c.b > c.a + 1.0e-3)
                {
                    t.loopStart = std::max (0.0, c.a);
                    t.loopEnd = c.b;
                }
                break;
            case CommandType::SetLoopEnabled: t.loopEnabled = c.a != 0.0; break;
            case CommandType::SetMetronome:   t.metronome = c.a != 0.0; break;
            case CommandType::SetMetronomeLevel: t.metronomeGain = dbToGain ((float) c.a); break;
            case CommandType::ResetMeters:
                loudness.requestReset();
                truePeak.requestReset();
                break;
            case CommandType::Panic:
                t.allNotesOffPending = true;
                t.panicPending = true;
                break;
        }
    }
}

int64_t AudioEngine::ppqToSample (const TempoMap& tempo, double ppq) const noexcept
{
    return (int64_t) std::llround (tempo.ppqToSeconds (ppq) * sampleRate);
}

int AudioEngine::computeRanges (const TempoMap& tempo, int numFrames) noexcept
{
    numRanges = 0;
    if (! transport.playing)
        return 0;

    auto& t = transport;
    const bool looping = t.loopEnabled && t.loopEnd > t.loopStart + 1.0e-3;
    const int64_t loopEndSample = looping ? (int64_t) std::ceil (tempo.ppqToSeconds (t.loopEnd) * sampleRate - 1.0e-6) : 0;
    if (looping && t.samplePos >= loopEndSample)
    {
        t.ppq = t.loopStart;
        t.samplePos = ppqToSample (tempo, t.loopStart);
        t.discontinuity = true;
    }

    int offset = 0;
    int remaining = numFrames;
    while (remaining > 0)
    {
        TimeRange r;
        r.offset = offset;
        r.secStart = (double) t.samplePos / sampleRate;
        r.ppqStart = t.discontinuity ? t.ppq : tempo.secondsToPpq (r.secStart);
        r.discontinuity = t.discontinuity;
        t.discontinuity = false;

        bool wrap = false;
        int len = remaining;
        if (looping && numRanges < kMaxRangesPerBlock - 1)
        {
            const int64_t k = std::max<int64_t> (1, loopEndSample - t.samplePos);
            if (k <= remaining)
            {
                len = (int) k;
                wrap = true;
            }
        }

        r.length = len;
        r.ppqEnd = wrap ? t.loopEnd : tempo.secondsToPpq ((double) (t.samplePos + len) / sampleRate);
        ranges[(size_t) numRanges++] = r;

        offset += len;
        remaining -= len;
        if (wrap)
        {
            t.ppq = t.loopStart;
            t.samplePos = ppqToSample (tempo, t.loopStart);
            t.discontinuity = true;
        }
        else
        {
            t.samplePos += len;
            t.ppq = r.ppqEnd;
        }

        if (numRanges >= kMaxRangesPerBlock)
            break;
    }
    return numRanges;
}

// ===========================================================================
// Audio thread
// ===========================================================================
void AudioEngine::processBlock (const float* const* inputs, int numInputs, float* const* outputs, int numOutputs,
                                int numFrames, const MidiEvent* liveMidi, int numLiveMidi) noexcept
{
    ScopedNoDenormals noDenormals;
    const auto start = std::chrono::steady_clock::now();

    if (maxBlockSize <= 0 || numFrames <= 0)
    {
        for (int c = 0; c < numOutputs; ++c)
            if (outputs[c] != nullptr)
                std::fill (outputs[c], outputs[c] + std::max (0, numFrames), 0.0f);
        return;
    }

    int done = 0;
    while (done < numFrames)
    {
        const int n = std::min (maxBlockSize, numFrames - done);
        processChunk (inputs, numInputs, outputs, numOutputs, done, n, liveMidi, numLiveMidi);
        done += n;
    }

    const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    const double budget = numFrames / sampleRate;
    const float load = (float) (elapsed / budget);
    cpuLoad.store (cpuLoad.load() * 0.9f + load * 0.1f);
}

void AudioEngine::processChunk (const float* const* inputs, int numInputs, float* const* outputs, int numOutputs,
                                int frameOffset, int n, const MidiEvent* liveMidi, int numLiveMidi) noexcept
{
    const EngineSnapshot* snap = exchange.acquire();
    handleCommands();

    for (int c = 0; c < std::min (numInputs, kMaxInputMeters); ++c)
    {
        float p = 0.0f;
        if (inputs != nullptr && inputs[c] != nullptr)
            for (int i = 0; i < n; ++i)
                p = std::max (p, std::abs (inputs[c][frameOffset + i]));
        inputPeaks[(size_t) c].store (std::max (p, inputPeaks[(size_t) c].load() * 0.9f));
    }

    if (snap == nullptr)
    {
        for (int c = 0; c < numOutputs; ++c)
            if (outputs[c] != nullptr)
                std::fill (outputs[c] + frameOffset, outputs[c] + frameOffset + n, 0.0f);
        exchange.blockFinished();
        return;
    }

    const TempoMap& tempo = snap->tempo;
    if (transport.tempoVersion != snap->tempoVersion)
    {
        // tempo map edited or position moved: keep the musical position, re-derive the sample clock
        transport.samplePos = ppqToSample (tempo, transport.ppq);
        transport.tempoVersion = snap->tempoVersion;
    }
    const double blockStartPpq = transport.ppq;
    computeRanges (tempo, n);

    ProcessContext ctx;
    ctx.sampleRate = sampleRate;
    ctx.tempo = &tempo;
    ctx.ppqPosition = numRanges > 0 ? ranges[0].ppqStart : blockStartPpq;
    ctx.bpm = tempo.bpmAt (ctx.ppqPosition);
    ctx.isPlaying = transport.playing;

    for (const auto& cp : snap->order)
        cp.ch->buffer.clear (0, n);

    // ---- tracks ---------------------------------------------------------------
    for (int ti = 0; ti < (int) snap->tracks.size(); ++ti)
    {
        const auto& tp = snap->tracks[(size_t) ti];
        auto& rt = *tp.rt;

        if (tp.type == TrackType::Instrument && tp.instrument != nullptr)
        {
            rt.events.clear();
            if (transport.allNotesOffPending)
                emitActiveNoteOffs (rt, 0);
            if (transport.panicPending)
            {
                for (int ch = 0; ch < 16; ++ch)
                    rt.events.add (MidiEvent::controller (0, ch, 120, 0));
            }

            for (int r = 0; r < numRanges; ++r)
            {
                const auto& range = ranges[(size_t) r];
                if (range.discontinuity)
                    emitActiveNoteOffs (rt, range.offset);

                // an event belongs to the range whose samples contain its (rounded-up) sample index;
                // this keeps every event on exactly one block boundary regardless of block size
                auto indexOf = [&] (double ppq)
                {
                    const double s = (tempo.ppqToSeconds (ppq) - range.secStart) * sampleRate;
                    return (int) std::ceil (s - 1.0e-6);
                };
                constexpr double kSlack = 1.0e-6; // PPQ search margin around the range

                for (const MidiClip* clip : tp.midiClips)
                {
                    const double clipStart = clip->start, clipEnd = clip->start + clip->length;
                    if (clipEnd <= range.ppqStart - kSlack || clipStart >= range.ppqEnd + kSlack)
                        continue;

                    const double lo = range.ppqStart - kSlack - clipStart + clip->offset - clip->maxNoteLength;
                    const double hi = range.ppqEnd + kSlack - clipStart + clip->offset;
                    auto it = std::lower_bound (clip->notes.begin(), clip->notes.end(), lo,
                                                [] (const Note& note, double v) { return note.start < v; });

                    for (; it != clip->notes.end() && it->start < hi; ++it)
                    {
                        const double ts = clipStart + it->start - clip->offset;
                        if (ts < clipStart || ts >= clipEnd)
                            continue;
                        const double te = std::min (ts + it->length, clipEnd);
                        auto& count = rt.activeNotes[(size_t) (it->channel & 15)][(size_t) (it->key & 127)];

                        const bool wrapsHere = range.ppqEnd == transport.loopEnd && transport.loopEnabled;
                        const int on = indexOf (ts);
                        if (on >= 0 && on < range.length && ! (wrapsHere && ts >= range.ppqEnd))
                        {
                            rt.events.add (MidiEvent::noteOn (range.offset + on, it->channel, it->key, std::max<int> (1, it->velocity)));
                            if (count < 255) ++count;
                        }
                        const int off = indexOf (te);
                        if (off >= 0 && off < range.length && count > 0 && ! (wrapsHere && te >= range.ppqEnd))
                        {
                            rt.events.add (MidiEvent::noteOff (range.offset + off, it->channel, it->key));
                            --count;
                        }
                    }
                }
            }

            if (ti == snap->liveInputTrack && liveMidi != nullptr)
            {
                for (int e = 0; e < numLiveMidi; ++e)
                {
                    const int off = liveMidi[e].sampleOffset - frameOffset;
                    if (off < 0 || off >= n) continue;
                    MidiEvent ev = liveMidi[e];
                    ev.sampleOffset = off;
                    rt.events.add (ev);
                }
            }

            tp.instrument->render (rt.buffer, n, rt.events, ctx);
        }
        else
        {
            rt.buffer.clear (0, n);
            for (int r = 0; r < numRanges; ++r)
            {
                const auto& range = ranges[(size_t) r];
                for (const AudioClip* clip : tp.audioClips)
                {
                    const double clipEnd = clip->start + clip->length;
                    if (clip->sample == nullptr || clipEnd <= range.ppqStart || clip->start >= range.ppqEnd)
                        continue;

                    const auto& sd = *clip->sample;
                    const double secStart = tempo.ppqToSeconds (clip->start);
                    const double secEnd = tempo.ppqToSeconds (clipEnd);
                    const int i0 = std::clamp ((int) std::ceil ((secStart - range.secStart) * sampleRate - 1.0e-6), 0, range.length);
                    const int i1 = std::clamp ((int) std::ceil ((secEnd - range.secStart) * sampleRate - 1.0e-6), 0, range.length);
                    const float gain = dbToGain (clip->gainDb);

                    for (int i = i0; i < i1; ++i)
                    {
                        const double t = range.secStart + i / sampleRate;
                        const double rel = t - secStart;
                        const double srcPos = (rel + clip->sourceOffsetSeconds) * sd.sampleRate;
                        if (srcPos < 0.0 || srcPos >= (double) sd.numFrames)
                            continue;

                        double g = gain;
                        if (clip->fadeInSeconds > 0.0 && rel < clip->fadeInSeconds) g *= rel / clip->fadeInSeconds;
                        const double tail = secEnd - t;
                        if (clip->fadeOutSeconds > 0.0 && tail < clip->fadeOutSeconds) g *= tail / clip->fadeOutSeconds;

                        for (int c = 0; c < 2; ++c)
                        {
                            const int srcCh = std::min (c, sd.numChannels - 1);
                            const double v = cubicHermite (sd.channels[(size_t) srcCh].data(), sd.numFrames, srcPos);
                            rt.buffer.getWritePointer (c)[range.offset + i] += (float) (v * g);
                        }
                    }
                }
            }
        }

        if (! tp.muted)
            if (auto* ch = snap->channelByIndex[(size_t) tp.channel])
                ch->buffer.addFrom (rt.buffer, n);
    }
    transport.allNotesOffPending = false;
    transport.panicPending = false;

    // ---- mixer ------------------------------------------------------------------
    for (const auto& cp : snap->order)
    {
        auto& ch = *cp.ch;
        auto& buf = ch.buffer;

        ProcessContext local = ctx;
        if (cp.sidechainSource >= 0)
            if (auto* src = snap->channelByIndex[(size_t) cp.sidechainSource])
                local.sidechain = &src->buffer;

        for (Processor* p : cp.inserts)
            if (! p->isBypassed())
                p->process (buf, n, local);

        auto doSends = [&] (bool pre)
        {
            for (const auto& s : cp.sends)
            {
                if (s.preFader != pre) continue;
                auto* target = snap->channelByIndex[(size_t) s.target];
                if (target == nullptr) continue;
                const float g = dbToGain (ch.sendLevelDb[(size_t) s.slot].load());
                addWithRamp (buf, target->buffer, n, ch.lastSendGain[(size_t) s.slot], g);
                ch.lastSendGain[(size_t) s.slot] = g;
            }
        };

        doSends (true);

        float gl, gr;
        const bool isMono = ch.mono.load (std::memory_order_relaxed);
        MixerChannel::computePanGains (ch.pan.load(), isMono, gl, gr);
        float v = dbToGain (ch.volumeDb.load());
        if (ch.muted.load (std::memory_order_relaxed) || cp.soloMuted) v = 0.0f;
        if (ch.phaseInvert.load (std::memory_order_relaxed)) v = -v;
        gl *= v;
        gr *= v;

        float* L = buf.getWritePointer (0);
        float* R = buf.getWritePointer (1);
        const float stepL = (gl - ch.lastGainL) / (float) n, stepR = (gr - ch.lastGainR) / (float) n;
        float curL = ch.lastGainL, curR = ch.lastGainR;
        for (int i = 0; i < n; ++i)
        {
            curL += stepL;
            curR += stepR;
            if (isMono)
            {
                const float m = 0.5f * (L[i] + R[i]);
                L[i] = m * curL;
                R[i] = m * curR;
            }
            else
            {
                L[i] *= curL;
                R[i] *= curR;
            }
        }
        ch.lastGainL = gl;
        ch.lastGainR = gr;

        const float* meterChannels[2] = { L, R };
        ch.meter.process (meterChannels, 2, n);

        doSends (false);

        if (cp.output >= 0)
            if (auto* target = snap->channelByIndex[(size_t) cp.output])
                target->buffer.addFrom (buf, n);
    }

    // ---- master metering + output -------------------------------------------
    MixerChannel* master = snap->channelByIndex[MixerModel::kMaster];
    const float* mL = master->buffer.getReadPointer (0);
    const float* mR = master->buffer.getReadPointer (1);
    loudness.process (mL, mR, n);
    const float* tpChannels[2] = { mL, mR };
    truePeak.process (tpChannels, 2, n);
    correlation.process (mL, mR, n);

    clickBuffer.clear (0, n);
    if (transport.metronome && transport.playing)
    {
        for (int r = 0; r < numRanges; ++r)
        {
            const auto& range = ranges[(size_t) r];
            for (double b = tempo.nextBeatAtOrAfter (range.ppqStart - 1.0e-6); b < range.ppqEnd + 1.0e-6;
                 b = tempo.nextBeatAtOrAfter (b + 1.0e-6))
            {
                const int idx = (int) std::ceil ((tempo.ppqToSeconds (b) - range.secStart) * sampleRate - 1.0e-6);
                if (idx >= 0 && idx < range.length)
                    metronome.schedule (range.offset + idx, tempo.isBarStart (b));
            }
        }
    }
    metronome.render (clickBuffer.getWritePointer (0), clickBuffer.getWritePointer (1), n, transport.metronomeGain);

    const float* cL = clickBuffer.getReadPointer (0);
    const float* cR = clickBuffer.getReadPointer (1);
    for (int c = 0; c < numOutputs; ++c)
    {
        float* out = outputs[c];
        if (out == nullptr) continue;
        out += frameOffset;
        if (numOutputs == 1)
            for (int i = 0; i < n; ++i) out[i] = 0.5f * (mL[i] + mR[i]) + cL[i];
        else if (c == 0)
            for (int i = 0; i < n; ++i) out[i] = mL[i] + cL[i];
        else if (c == 1)
            for (int i = 0; i < n; ++i) out[i] = mR[i] + cR[i];
        else
            std::fill (out, out + n, 0.0f);
    }

    // ---- publish transport -------------------------------------------------
    uiPlaying.store (transport.playing);
    uiPpq.store (transport.ppq);
    uiLoopEnabled.store (transport.loopEnabled);
    uiLoopStart.store (transport.loopStart);
    uiLoopEnd.store (transport.loopEnd);
    uiMetronome.store (transport.metronome);
    blockCount.fetch_add (1);

    exchange.blockFinished();
}

} // namespace pink
