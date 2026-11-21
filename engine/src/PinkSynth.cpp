#include "pink/instruments/PinkSynth.h"

#include <algorithm>
#include <cmath>

namespace pink
{
namespace
{
constexpr float kLn1000 = 6.907755279f; // envelope segments reach -60 dB in the given time

inline double polyBlep (double t, double dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

inline float oscillator (PinkSynth::Wave w, double t, double dt) noexcept
{
    switch (w)
    {
        case PinkSynth::Wave::Saw:
            return (float) (2.0 * t - 1.0 - polyBlep (t, dt));
        case PinkSynth::Wave::Square:
        {
            double v = t < 0.5 ? 1.0 : -1.0;
            v += polyBlep (t, dt);
            double t2 = t + 0.5;
            if (t2 >= 1.0) t2 -= 1.0;
            v -= polyBlep (t2, dt);
            return (float) v;
        }
        case PinkSynth::Wave::Triangle:
            return (float) (4.0 * std::abs (t - 0.5) - 1.0);
        case PinkSynth::Wave::Sine:
        default:
            return (float) std::sin (kTwoPi * t);
    }
}

constexpr int kChunk = 16; // control-rate interval for pitch / cutoff modulation
} // namespace

// ===========================================================================
// Envelope
// ===========================================================================
void Envelope::setParameters (float attackMs, float decayMs, float sustainLevel, float releaseMs) noexcept
{
    attackInc = attackMs <= 0.05f ? 1.0f : (float) (1.0 / (attackMs * 0.001 * sampleRate));
    decayCoef = (float) std::exp (-kLn1000 / (std::max (decayMs, 0.1f) * 0.001 * sampleRate));
    sustain = std::clamp (sustainLevel, 0.0f, 1.0f);
    releaseCoef = (float) std::exp (-kLn1000 / (std::max (releaseMs, 0.1f) * 0.001 * sampleRate));
}

void Envelope::noteOn() noexcept { stage = Stage::Attack; }

void Envelope::noteOff() noexcept
{
    if (stage != Stage::Idle)
        stage = Stage::Release;
}

float Envelope::next() noexcept
{
    switch (stage)
    {
        case Stage::Attack:
            level += attackInc;
            if (level >= 1.0f) { level = 1.0f; stage = Stage::Decay; }
            break;
        case Stage::Decay:
            level = sustain + (level - sustain) * decayCoef;
            if (level - sustain < 1.0e-4f) { level = sustain; stage = Stage::Sustain; }
            break;
        case Stage::Sustain:
            level = sustain;
            if (sustain <= 1.0e-5f) { level = 0.0f; stage = Stage::Idle; }
            break;
        case Stage::Release:
            level *= releaseCoef;
            if (level < 1.0e-5f) { level = 0.0f; stage = Stage::Idle; }
            break;
        case Stage::Idle:
        default:
            level = 0.0f;
            break;
    }
    return level;
}

// ===========================================================================
// StateVariableFilter
// ===========================================================================
void StateVariableFilter::setCoefficients (double sampleRate, double cutoffHz, float resonance) noexcept
{
    cutoffHz = std::clamp (cutoffHz, 16.0, sampleRate * 0.45);
    const double g = std::tan (kPi * cutoffHz / sampleRate);
    k = 2.0f - 1.96f * std::clamp (resonance, 0.0f, 1.0f);
    a1 = (float) (1.0 / (1.0 + g * (g + k)));
    a2 = (float) g * a1;
    a3 = (float) g * a2;
}

// ===========================================================================
// PinkSynth
// ===========================================================================
PinkSynth::PinkSynth()
{
    const std::vector<std::string> waves { "Saw", "Square", "Triangle", "Sine" };
    using S = Parameter::Scale;

    pWave1  = &params.addChoice ("osc1_wave", "Osc 1 Wave", waves, 0);
    pLevel1 = &params.add ("osc1_level", "Osc 1 Level", 0.0f, 1.0f, 0.8f);
    pWave2  = &params.addChoice ("osc2_wave", "Osc 2 Wave", waves, 0);
    pLevel2 = &params.add ("osc2_level", "Osc 2 Level", 0.0f, 1.0f, 0.0f);
    pSemi2  = &params.add ("osc2_semi", "Osc 2 Semitones", -24.0f, 24.0f, 0.0f, "st", S::Choice);
    pFine2  = &params.add ("osc2_fine", "Osc 2 Fine", -100.0f, 100.0f, 7.0f, "ct");
    pSub    = &params.add ("sub_level", "Sub Level", 0.0f, 1.0f, 0.0f);
    pNoise  = &params.add ("noise_level", "Noise Level", 0.0f, 1.0f, 0.0f);

    pUnison = &params.add ("unison", "Unison Voices", 1.0f, (float) kMaxUnison, 1.0f, {}, S::Choice);
    pDetune = &params.add ("unison_detune", "Unison Detune", 0.0f, 100.0f, 20.0f, "ct");
    pSpread = &params.add ("unison_spread", "Unison Spread", 0.0f, 100.0f, 50.0f, "%");
    pVoices = &params.add ("voices", "Polyphony", 1.0f, (float) kMaxVoices, (float) kMaxVoices, {}, S::Choice);

    pFilterType = &params.addChoice ("filter_type", "Filter Type", { "Low Pass", "High Pass", "Band Pass" }, 0);
    pCutoff     = &params.add ("cutoff", "Cutoff", 20.0f, 20000.0f, 6000.0f, "Hz", S::Logarithmic);
    pResonance  = &params.add ("resonance", "Resonance", 0.0f, 1.0f, 0.2f);
    pFilterEnv  = &params.add ("filter_env", "Filter Env Amount", -48.0f, 48.0f, 0.0f, "st");
    pKeyTrack   = &params.add ("key_track", "Key Tracking", 0.0f, 100.0f, 0.0f, "%");

    pAmpA = &params.add ("amp_attack", "Amp Attack", 0.0f, 5000.0f, 5.0f, "ms");
    pAmpD = &params.add ("amp_decay", "Amp Decay", 1.0f, 5000.0f, 300.0f, "ms");
    pAmpS = &params.add ("amp_sustain", "Amp Sustain", 0.0f, 1.0f, 0.8f);
    pAmpR = &params.add ("amp_release", "Amp Release", 1.0f, 8000.0f, 250.0f, "ms");
    pFltA = &params.add ("flt_attack", "Filter Attack", 0.0f, 5000.0f, 5.0f, "ms");
    pFltD = &params.add ("flt_decay", "Filter Decay", 1.0f, 5000.0f, 300.0f, "ms");
    pFltS = &params.add ("flt_sustain", "Filter Sustain", 0.0f, 1.0f, 0.5f);
    pFltR = &params.add ("flt_release", "Filter Release", 1.0f, 8000.0f, 300.0f, "ms");

    pLfoRate   = &params.add ("lfo_rate", "LFO Rate", 0.05f, 20.0f, 5.0f, "Hz", S::Logarithmic);
    pLfoPitch  = &params.add ("lfo_pitch", "LFO > Pitch", 0.0f, 100.0f, 0.0f, "ct");
    pLfoCutoff = &params.add ("lfo_cutoff", "LFO > Cutoff", 0.0f, 48.0f, 0.0f, "st");
    pVelocity  = &params.add ("velocity", "Velocity Sensitivity", 0.0f, 100.0f, 70.0f, "%");
    pGain      = &params.add ("gain", "Master", -36.0f, 6.0f, -6.0f, "dB");
    pBend      = &params.add ("bend_range", "Pitch Bend Range", 0.0f, 24.0f, 2.0f, "st", S::Choice);
}

void PinkSynth::loadPreset (const std::string& name)
{
    for (size_t i = 0; i < params.size(); ++i)
        params[i].resetToDefault();

    if (name == "Supersaw Pad")
    {
        params.set ("unison", 7); params.set ("unison_detune", 24); params.set ("unison_spread", 85);
        params.set ("osc2_level", 0.35f); params.set ("osc2_semi", 12); params.set ("osc2_fine", 4);
        params.set ("cutoff", 3200); params.set ("resonance", 0.15f); params.set ("filter_env", 14);
        params.set ("amp_attack", 25); params.set ("amp_decay", 900); params.set ("amp_sustain", 0.75f); params.set ("amp_release", 650);
        params.set ("flt_attack", 3); params.set ("flt_decay", 900); params.set ("flt_sustain", 0.35f);
        params.set ("lfo_rate", 0.4f); params.set ("lfo_cutoff", 5); params.set ("gain", -15);
    }
    else if (name == "Kawaii Pluck")
    {
        params.set ("osc1_wave", (float) Wave::Square); params.set ("osc2_level", 0.55f); params.set ("osc2_semi", 12);
        params.set ("unison", 3); params.set ("unison_detune", 10); params.set ("unison_spread", 60);
        params.set ("cutoff", 900); params.set ("resonance", 0.35f); params.set ("filter_env", 38); params.set ("key_track", 50);
        params.set ("flt_attack", 0); params.set ("flt_decay", 170); params.set ("flt_sustain", 0.0f); params.set ("flt_release", 160);
        params.set ("amp_attack", 1); params.set ("amp_decay", 260); params.set ("amp_sustain", 0.0f); params.set ("amp_release", 180);
        params.set ("gain", -9);
    }
    else if (name == "Sub Bass")
    {
        params.set ("osc1_wave", (float) Wave::Sine); params.set ("osc1_level", 0.9f);
        params.set ("osc2_wave", (float) Wave::Triangle); params.set ("osc2_level", 0.25f); params.set ("osc2_fine", 0);
        params.set ("cutoff", 900); params.set ("resonance", 0.05f); params.set ("voices", 1);
        params.set ("amp_attack", 3); params.set ("amp_decay", 400); params.set ("amp_sustain", 0.9f); params.set ("amp_release", 90);
        params.set ("velocity", 20); params.set ("gain", -5);
    }
}

void PinkSynth::prepare (double sr, int)
{
    sampleRate = sr;
    for (auto& v : voices)
    {
        v.amp.setSampleRate (sr);
        v.filterEnv.setSampleRate (sr);
    }
    reset();
}

void PinkSynth::reset() noexcept
{
    for (auto& v : voices)
    {
        v.active = false;
        v.held = v.sustained = false;
        v.amp.kill();
        v.filterEnv.kill();
        for (auto& f : v.filters) f.reset();
    }
    pitchBendSemis = 0.0f;
    modWheel = 0.0f;
    sustainPedal = false;
    lfoPhase = 0.0;
}

int PinkSynth::getActiveVoiceCount() const noexcept
{
    int n = 0;
    for (const auto& v : voices)
        n += v.active ? 1 : 0;
    return n;
}

void PinkSynth::refreshBlockParameters() noexcept
{
    cache.wave1 = (Wave) pWave1->getInt();
    cache.wave2 = (Wave) pWave2->getInt();
    cache.level1 = pLevel1->get();
    cache.level2 = pLevel2->get();
    cache.semi2 = pSemi2->get();
    cache.fine2 = pFine2->get();
    cache.subLevel = pSub->get();
    cache.noiseLevel = pNoise->get();
    cache.unison = std::clamp (pUnison->getInt(), 1, kMaxUnison);
    cache.detuneCents = pDetune->get();
    cache.spread = pSpread->get() * 0.01f;
    cache.maxVoices = std::clamp (pVoices->getInt(), 1, kMaxVoices);
    cache.filterMode = (StateVariableFilter::Mode) pFilterType->getInt();
    cache.cutoff = pCutoff->get();
    cache.resonance = pResonance->get();
    cache.envAmountSemis = pFilterEnv->get();
    cache.keyTrack = pKeyTrack->get() * 0.01f;
    cache.lfoRate = pLfoRate->get();
    cache.lfoPitchCents = pLfoPitch->get();
    cache.lfoCutoffSemis = pLfoCutoff->get();
    cache.velocitySens = pVelocity->get() * 0.01f;
    cache.gain = dbToGain (pGain->get());
    cache.bendRange = pBend->get();

    for (auto& v : voices)
    {
        v.amp.setParameters (pAmpA->get(), pAmpD->get(), pAmpS->get(), pAmpR->get());
        v.filterEnv.setParameters (pFltA->get(), pFltD->get(), pFltS->get(), pFltR->get());
    }
}

void PinkSynth::startVoice (int key, int velocity, int channel) noexcept
{
    Voice* target = nullptr;

    for (auto& v : voices)
        if (v.active && v.key == key && v.channel == channel)
            { target = &v; break; }

    if (target == nullptr)
    {
        int activeCount = 0;
        for (auto& v : voices)
            activeCount += v.active ? 1 : 0;

        if (activeCount >= cache.maxVoices)
        {
            // steal: prefer the oldest releasing voice, otherwise the oldest voice
            Voice* oldestReleased = nullptr;
            Voice* oldest = nullptr;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                if (oldest == nullptr || v.age < oldest->age) oldest = &v;
                if (v.amp.isReleasing() && (oldestReleased == nullptr || v.age < oldestReleased->age)) oldestReleased = &v;
            }
            target = oldestReleased != nullptr ? oldestReleased : oldest;
        }
        else
        {
            for (auto& v : voices)
                if (! v.active) { target = &v; break; }
        }

        if (target == nullptr)
            return;

        if (! target->active)
        {
            for (int u = 0; u < kMaxUnison; ++u)
            {
                target->phase1[(size_t) u] = cache.unison > 1 ? random.nextUnipolar() : 0.0;
                target->phase2[(size_t) u] = cache.unison > 1 ? random.nextUnipolar() : 0.0;
            }
            target->subPhase = 0.0;
            for (auto& f : target->filters) f.reset();
            target->filterCounter = 0;
        }
    }

    const float vel = (float) velocity / 127.0f;
    target->velocityGain = (1.0f - cache.velocitySens) + cache.velocitySens * vel * vel;
    target->active = true;
    target->held = true;
    target->sustained = false;
    target->key = key;
    target->channel = channel;
    target->age = ++ageCounter;
    target->amp.noteOn();
    target->filterEnv.noteOn();
}

void PinkSynth::releaseKey (int key, int channel) noexcept
{
    for (auto& v : voices)
    {
        if (! v.active || ! v.held || v.key != key || v.channel != channel)
            continue;

        v.held = false;
        if (sustainPedal)
            v.sustained = true;
        else
        {
            v.amp.noteOff();
            v.filterEnv.noteOff();
        }
    }
}

void PinkSynth::handleEvent (const MidiEvent& e) noexcept
{
    if (e.isNoteOn())
        startVoice (e.data1, e.data2, e.channel());
    else if (e.isNoteOff())
        releaseKey (e.data1, e.channel());
    else if (e.isPitchBend())
        pitchBendSemis = (float) (e.pitchBendValue() - 8192) / 8192.0f * cache.bendRange;
    else if (e.isController())
    {
        switch (e.data1)
        {
            case 1: modWheel = (float) e.data2 / 127.0f; break;
            case 64:
                sustainPedal = e.data2 >= 64;
                if (! sustainPedal)
                    for (auto& v : voices)
                        if (v.active && v.sustained)
                        {
                            v.sustained = false;
                            v.amp.noteOff();
                            v.filterEnv.noteOff();
                        }
                break;
            case 120: // all sound off
                for (auto& v : voices) { v.active = false; v.amp.kill(); v.filterEnv.kill(); }
                break;
            case 123: // all notes off
                for (auto& v : voices)
                    if (v.active) { v.held = v.sustained = false; v.amp.noteOff(); v.filterEnv.noteOff(); }
                break;
            default: break;
        }
    }
}

void PinkSynth::render (AudioBuffer& out, int numFrames, const MidiEventList& events, const ProcessContext&) noexcept
{
    refreshBlockParameters();
    out.clear (0, numFrames);
    if (out.getNumChannels() < 2)
        return;

    float* left = out.getWritePointer (0);
    float* right = out.getWritePointer (1);
    int pos = 0;

    for (const auto& e : events)
    {
        const int offset = std::clamp (e.sampleOffset, pos, numFrames);
        if (offset > pos)
        {
            renderVoices (left + pos, right + pos, offset - pos);
            pos = offset;
        }
        handleEvent (e);
    }

    if (pos < numFrames)
        renderVoices (left + pos, right + pos, numFrames - pos);
}

void PinkSynth::renderVoices (float* left, float* right, int numFrames) noexcept
{
    // unison layout for this block
    const int U = cache.unison;
    std::array<double, kMaxUnison> ratio {};
    std::array<float, kMaxUnison> panL {}, panR {};
    for (int u = 0; u < U; ++u)
    {
        const double x = U > 1 ? 2.0 * u / (U - 1) - 1.0 : 0.0;
        ratio[(size_t) u] = std::pow (2.0, cache.detuneCents * x / 1200.0);
        const double angle = (cache.spread * x + 1.0) * kPi * 0.25;
        panL[(size_t) u] = (float) (std::cos (angle) * std::numbers::sqrt2);
        panR[(size_t) u] = (float) (std::sin (angle) * std::numbers::sqrt2);
    }
    const float unisonNorm = 1.0f / std::sqrt ((float) U);
    const double osc2Ratio = std::pow (2.0, (cache.semi2 + cache.fine2 * 0.01) / 12.0);
    const bool useOsc2 = cache.level2 > 0.0f;
    const double nyquistDt = 0.49;

    for (int start = 0; start < numFrames; start += kChunk)
    {
        const int n = std::min (kChunk, numFrames - start);
        const float lfo = (float) std::sin (kTwoPi * lfoPhase);
        lfoPhase += cache.lfoRate * n / sampleRate;
        lfoPhase -= std::floor (lfoPhase);

        const float vibratoCents = lfo * (cache.lfoPitchCents + modWheel * 50.0f);

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            const double note = v.key + pitchBendSemis + vibratoCents * 0.01;
            const double freq1 = midiNoteToHz (note);
            const double freq2 = freq1 * osc2Ratio;

            const double cutoffSemis = cache.envAmountSemis * v.filterEnv.getLevel()
                                     + cache.lfoCutoffSemis * lfo
                                     + cache.keyTrack * (v.key - 60);
            const double cutoffHz = cache.cutoff * std::pow (2.0, cutoffSemis / 12.0);
            for (auto& f : v.filters)
                f.setCoefficients (sampleRate, cutoffHz, cache.resonance);

            const double subDt = std::min (freq1 * 0.5 / sampleRate, nyquistDt);

            for (int i = 0; i < n; ++i)
            {
                float sl = 0.0f, sr = 0.0f;
                for (int u = 0; u < U; ++u)
                {
                    const double dt1 = std::min (freq1 * ratio[(size_t) u] / sampleRate, nyquistDt);
                    double& p1 = v.phase1[(size_t) u];
                    float o = cache.level1 * oscillator (cache.wave1, p1, dt1);
                    p1 += dt1;
                    if (p1 >= 1.0) p1 -= 1.0;

                    if (useOsc2)
                    {
                        const double dt2 = std::min (freq2 * ratio[(size_t) u] / sampleRate, nyquistDt);
                        double& p2 = v.phase2[(size_t) u];
                        o += cache.level2 * oscillator (cache.wave2, p2, dt2);
                        p2 += dt2;
                        if (p2 >= 1.0) p2 -= 1.0;
                    }

                    sl += o * panL[(size_t) u];
                    sr += o * panR[(size_t) u];
                }

                sl *= unisonNorm;
                sr *= unisonNorm;

                float mono = 0.0f;
                if (cache.subLevel > 0.0f)
                    mono += cache.subLevel * (float) std::sin (kTwoPi * v.subPhase);
                v.subPhase += subDt;
                if (v.subPhase >= 1.0) v.subPhase -= 1.0;
                if (cache.noiseLevel > 0.0f)
                    mono += cache.noiseLevel * random.nextBipolar();

                const float fl = v.filters[0].process (sl + mono, cache.filterMode);
                const float fr = v.filters[1].process (sr + mono, cache.filterMode);
                const float a = v.amp.next() * v.velocityGain * cache.gain;
                v.filterEnv.next();

                left[start + i] += fl * a;
                right[start + i] += fr * a;
            }

            if (! v.amp.isActive())
            {
                v.active = false;
                v.held = v.sustained = false;
            }
        }
    }
}

} // namespace pink
