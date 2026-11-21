#include "AllocTracker.h"
#include "TestFramework.h"
#include "TestHelpers.h"

#include "pink/demo/DemoProject.h"
#include "pink/dsp/StockEffects.h"
#include "pink/engine/AudioEngine.h"
#include "pink/engine/OfflineRenderer.h"
#include "pink/instruments/PinkSynth.h"
#include "pink/io/WavFile.h"

#include <atomic>
#include <cstdio>
#include <random>
#include <thread>

using namespace pink;
using namespace pinktest;

namespace
{
struct Runner
{
    explicit Runner (AudioEngine& e, int block) : engine (e), blockSize (block), l ((size_t) block), r ((size_t) block) {}

    /** Renders 'frames' samples, appending the output. */
    void run (long long frames)
    {
        float* outs[2] = { l.data(), r.data() };
        while (frames > 0)
        {
            const int n = (int) std::min<long long> (blockSize, frames);
            engine.processBlock (nullptr, 0, outs, 2, n);
            left.insert (left.end(), l.begin(), l.begin() + n);
            right.insert (right.end(), r.begin(), r.begin() + n);
            frames -= n;
        }
    }

    AudioEngine& engine;
    int blockSize;
    std::vector<float> l, r, left, right;
};

std::shared_ptr<MidiClip> clipWith (std::vector<Note> notes, double start = 0.0, double length = 64.0)
{
    auto c = std::make_shared<MidiClip>();
    c->start = start;
    c->length = length;
    c->notes = std::move (notes);
    c->finalise();
    return c;
}

/** Minimal project: one track feeding 'channel', master untouched. */
Track& addSourceTrack (Project& p, std::shared_ptr<Instrument> inst, int channel)
{
    auto& t = p.addTrack ("t", TrackType::Instrument, channel);
    t.instrument = std::move (inst);
    return t;
}
} // namespace

PINK_TEST (engine_events_are_sample_accurate_for_any_block_size)
{
    std::vector<long long> reference;
    for (int block : { 64, 512, 1000, 4096 })
    {
        AudioEngine engine;
        auto probe = std::make_shared<ProbeInstrument>();
        auto& t = addSourceTrack (engine.getProject(), probe, 1);
        t.midiClips.push_back (clipWith ({ { 1.0, 0.5, 60, 100, 0 }, { 2.5, 1.0, 62, 100, 0 }, { 7.25, 0.25, 64, 100, 0 } }));
        engine.getProject().tempo.setConstantTempo (120.0);
        engine.prepare (kSR, block);
        engine.play();
        Runner run (engine, block);
        run.run ((long long) (5.0 * kSR));

        std::vector<long long> got;
        for (auto& e : probe->log) got.push_back (e.sample);
        // at 120 BPM / 48 kHz one beat is exactly 24000 samples
        const std::vector<long long> expected { 24000, 36000, 60000, 84000, 174000, 180000 };
        CHECK (got == expected);
        if (reference.empty()) reference = got;
        CHECK (got == reference);
    }
    REPORT ("note on/off positions identical for blocks 64/512/1000/4096", "yes");
}

PINK_TEST (engine_loop_wrap_sends_note_off_and_retriggers)
{
    AudioEngine engine;
    auto probe = std::make_shared<ProbeInstrument>();
    auto& t = addSourceTrack (engine.getProject(), probe, 1);
    t.midiClips.push_back (clipWith ({ { 1.5, 1.0, 60, 100, 0 } })); // crosses the loop end at ppq 2
    engine.getProject().tempo.setConstantTempo (120.0);
    engine.prepare (kSR, 480);
    engine.setLoop (0.0, 2.0);
    engine.setLoopEnabled (true);
    engine.play();
    Runner run (engine, 480);
    run.run (100000);

    // loop = 48000 samples; note on at 36000, forced note off at 48000 (loop end), on again at 84000
    CHECK (probe->log.size() >= 3);
    if (probe->log.size() >= 3)
    {
        CHECK (probe->log[0].sample == 36000 && probe->log[0].status == 0x90);
        CHECK (probe->log[1].sample == 48000 && probe->log[1].status == 0x80);
        CHECK (probe->log[2].sample == 84000 && probe->log[2].status == 0x90);
    }
    const auto info = engine.getTransportInfo();
    CHECK (info.ppq >= 0.0 && info.ppq < 2.0);
}

PINK_TEST (engine_stop_releases_notes_and_returns_to_start)
{
    AudioEngine engine;
    auto probe = std::make_shared<ProbeInstrument>();
    auto& t = addSourceTrack (engine.getProject(), probe, 1);
    t.midiClips.push_back (clipWith ({ { 0.0, 32.0, 60, 100, 0 } }));
    engine.prepare (kSR, 512);
    engine.locate (4.0);
    engine.play();
    Runner run (engine, 512);
    run.run (24000);
    engine.stop();
    run.run (512);
    CHECK (probe->log.size() == 0); // note started before the play position: no chase, no events
    CHECK_NEAR (engine.getTransportInfo().ppq, 4.0, 1e-9);

    engine.locate (0.0);
    engine.play();
    run.run (2048);
    engine.stop();
    run.run (512);
    CHECK (probe->log.size() == 2);
    if (probe->log.size() == 2) CHECK (probe->log[1].status == 0x80);
}

PINK_TEST (engine_fader_pan_mute_and_sends)
{
    AudioEngine engine;
    auto& p = engine.getProject();
    addSourceTrack (p, std::make_shared<ConstantSource> (0.5f), 1);
    p.mixer.channel (1).rt->volumeDb = -6.0f;
    engine.prepare (kSR, 256);
    Runner run (engine, 256);
    run.run (2048);
    CHECK_NEAR (run.left.back(), 0.5 * dbToGain (-6.0f), 1e-5);
    CHECK_NEAR (run.right.back(), 0.5 * dbToGain (-6.0f), 1e-5);

    // mono channel, centre pan: constant power -3 dB per side
    p.mixer.channel (1).rt->volumeDb = 0.0f;
    p.mixer.channel (1).rt->mono = true;
    run.run (1024);
    CHECK_NEAR (run.left.back(), 0.5 * std::cos (kPi / 4), 1e-5);

    // stereo hard right: left disappears
    p.mixer.channel (1).rt->mono = false;
    p.mixer.channel (1).rt->pan = 1.0f;
    run.run (1024);
    CHECK_NEAR (run.left.back(), 0.0, 1e-6);
    CHECK_NEAR (run.right.back(), 0.5, 1e-5);

    p.mixer.channel (1).rt->pan = 0.0f;
    p.mixer.channel (1).rt->muted = true;
    run.run (1024);
    CHECK_NEAR (run.left.back(), 0.0, 1e-7);
    p.mixer.channel (1).rt->muted = false;

    // post-fader send to channel 2 while channel 1 has no direct output
    p.mixer.setOutput (1, -1);
    const int slot = p.mixer.addSend (1, 2, 0.0f, false);
    CHECK (slot == 0);
    p.mixer.channel (1).rt->volumeDb = -6.0f;
    engine.commit();
    run.run (2048);
    CHECK_NEAR (run.left.back(), 0.5 * dbToGain (-6.0f), 1e-5);

    // pre-fader send ignores the fader
    p.mixer.channel (1).sends[0].preFader = true;
    engine.commit();
    run.run (2048);
    CHECK_NEAR (run.left.back(), 0.5, 1e-5);
}

PINK_TEST (engine_solo_mutes_unrelated_channels)
{
    AudioEngine engine;
    auto& p = engine.getProject();
    addSourceTrack (p, std::make_shared<ConstantSource> (0.25f), 1);
    addSourceTrack (p, std::make_shared<ConstantSource> (0.125f), 2);
    engine.prepare (kSR, 256);
    Runner run (engine, 256);
    run.run (1024);
    CHECK_NEAR (run.left.back(), 0.375, 1e-5);

    p.mixer.channel (2).solo = true;
    engine.commit();
    run.run (1024);
    CHECK_NEAR (run.left.back(), 0.125, 1e-5);

    // a bus fed by the soloed channel stays audible
    p.mixer.setOutput (2, 5);
    engine.commit();
    run.run (1024);
    CHECK_NEAR (run.left.back(), 0.125, 1e-5);
}

PINK_TEST (mixer_rejects_feedback_routing)
{
    MixerModel m;
    CHECK (m.getNumChannels() == MixerModel::kDefaultInsertChannels + 1);
    CHECK (m.setOutput (1, 2));
    CHECK (m.setOutput (2, 3));
    CHECK (! m.setOutput (3, 1));            // 1 -> 2 -> 3 -> 1
    CHECK (m.addSend (3, 1, 0.0f, false) == -1);
    CHECK (! m.setSidechainSource (1, 3));   // sidechain would also close the loop
    CHECK (m.setSidechainSource (3, 1));
    CHECK (! m.setOutput (0, 1));            // master has no output
    CHECK (! m.setOutput (4, 4));
    m.setNumInsertChannels (256);
    CHECK (m.getNumChannels() == 257);
}

PINK_TEST (engine_sidechain_ducks_target)
{
    auto measure = [] (bool sidechainOn)
    {
        AudioEngine engine;
        auto& p = engine.getProject();
        addSourceTrack (p, std::make_shared<ConstantSource> (0.5f), 1);   // trigger (loud)
        addSourceTrack (p, std::make_shared<ConstantSource> (0.02f), 2);  // target (below threshold)
        p.mixer.setOutput (1, -1);                                        // trigger is not heard
        auto comp = std::make_shared<Compressor>();
        comp->getParameters().set ("threshold", -30.0f);
        comp->getParameters().set ("ratio", 8.0f);
        comp->getParameters().set ("attack", 0.5f);
        comp->getParameters().set ("sidechain", sidechainOn ? 1.0f : 0.0f);
        p.mixer.addInsert (2, comp);
        p.mixer.setSidechainSource (2, 1);
        engine.prepare (kSR, 256);
        Runner run (engine, 256);
        run.run (9600);
        return run.left.back();
    };
    const float ducked = measure (true), plain = measure (false);
    CHECK (ducked < plain * 0.2f);
    REPORT ("sidechain ducking in mixer (dB)", gainToDb (ducked) - gainToDb (plain));
}

PINK_TEST (engine_metronome_clicks_on_beats)
{
    AudioEngine engine;
    engine.getProject().tempo.setConstantTempo (120.0);
    engine.prepare (kSR, 512);
    engine.setMetronomeEnabled (true);
    engine.play();
    Runner run (engine, 512);
    run.run ((long long) (2.1 * kSR));
    std::vector<long long> onsets;
    for (size_t i = 0; i < run.left.size(); ++i)
        if (std::abs (run.left[i]) > 0.05f && (onsets.empty() || (long long) i - onsets.back() > 12000))
            onsets.push_back ((long long) i);
    // onset = first loud sample after the scheduled beat (the click starts at sin(0) = 0)
    CHECK (onsets.size() == 5);
    for (size_t k = 0; k < onsets.size(); ++k)
        CHECK (std::abs (onsets[k] - (long long) k * 24000) <= 2);
}

PINK_TEST (engine_audio_callback_does_not_allocate)
{
    AudioEngine engine;
    buildDemoProject (engine.getProject());
    engine.prepare (kSR, 512);
    engine.setLoop (0.0, 32.0);
    engine.setLoopEnabled (true);
    engine.setMetronomeEnabled (true);
    engine.play();

    std::vector<float> l (512), r (512);
    float* outs[2] = { l.data(), r.data() };
    std::vector<MidiEvent> live { MidiEvent::noteOn (10, 0, 72, 100), MidiEvent::noteOff (400, 0, 72) };

    pinktest::rtAllocations = 0;
    for (int b = 0; b < 3000; ++b)
    {
        if (b == 500) engine.locate (7.0);
        if (b == 900) engine.stop();
        if (b == 950) engine.play();
        if (b == 1500) engine.panic();
        ScopedRtCheck check;
        engine.processBlock (nullptr, 0, outs, 2, 512, live.data(), (int) live.size());
    }
    CHECK (pinktest::rtAllocations.load() == 0);
    REPORT ("heap allocations inside 3000 audio callbacks (demo session)", pinktest::rtAllocations.load());
    REPORT ("channels processed (of 129)", engine.getActiveChannelCount());
    CHECK (engine.getActiveChannelCount() == 7);
}

PINK_TEST (engine_survives_concurrent_edits)
{
    AudioEngine engine;
    buildDemoProject (engine.getProject());
    engine.prepare (kSR, 256);
    engine.setLoop (0.0, 32.0);
    engine.setLoopEnabled (true);
    engine.play();

    std::atomic<bool> running { true };
    std::atomic<long> blocks { 0 };
    std::atomic<bool> badSample { false };
    std::thread audio ([&]
    {
        std::vector<float> l (256), r (256);
        float* outs[2] = { l.data(), r.data() };
        while (running.load())
        {
            engine.processBlock (nullptr, 0, outs, 2, 256);
            for (float v : l) if (! std::isfinite (v)) badSample = true;
            ++blocks;
        }
    });

    std::mt19937 rng (11);
    auto& p = engine.getProject();
    for (int i = 0; i < 400; ++i)
    {
        const int action = (int) (rng() % 5);
        if (action == 0)
        {
            auto& t = p.addTrack ("extra", TrackType::Instrument, 7 + (int) (rng() % 5));
            auto s = std::make_shared<PinkSynth>();
            s->loadPreset ("Kawaii Pluck");
            t.instrument = s;
            t.midiClips.push_back (clipWith ({ { 0.0, 1.0, 72, 100, 0 }, { 2.0, 1.0, 75, 100, 0 } }, 0.0, 32.0));
        }
        else if (action == 1 && p.tracks.size() > 5)
            p.tracks.pop_back();
        else if (action == 2)
            p.mixer.channel (1 + (int) (rng() % 6)).solo = ! p.mixer.channel (1 + (int) (rng() % 6)).solo;
        else if (action == 3)
            p.mixer.channel (1 + (int) (rng() % 6)).rt->volumeDb = -12.0f + (float) (rng() % 12);
        else
            p.mixer.addSend (1 + (int) (rng() % 5), 6, -10.0f, rng() % 2 == 0);

        engine.commit();
        if (i % 7 == 0) engine.locate ((double) (rng() % 32));
        if (i % 3 == 0) std::this_thread::sleep_for (std::chrono::microseconds (200));
    }

    const long target = blocks.load() + 4;
    while (blocks.load() < target) std::this_thread::yield();
    engine.collectGarbage();
    running = false;
    audio.join();

    CHECK (! badSample.load());
    CHECK (blocks.load() > 50);
    REPORT ("audio blocks processed during 400 live commits", blocks.load());
}

PINK_TEST (offline_render_demo_to_wav_and_read_back)
{
    AudioEngine engine;
    buildDemoProject (engine.getProject());
    engine.prepare (44100.0, 256); // the renderer must re-prepare to its own settings

    OfflineRenderSettings s;
    s.sampleRate = 48000.0;
    s.blockSize = 512;
    s.tailSeconds = 1.5;
    s.format = WavFormat::Float32;
    const std::string path = "pinkstudio_test_render.wav";
    const auto r = renderToWav (engine, s, path);
    CHECK (r.ok);
    // 8 bars @150 BPM = 12.8 s + 1.5 s tail
    CHECK (r.frames == (long long) std::llround (14.3 * 48000.0));
    CHECK (r.samplePeakDb <= -1.0 + 1e-3);   // limiter ceiling
    CHECK (r.integratedLufs > -20.0 && r.integratedLufs < -4.0);
    CHECK (engine.getSampleRate() == 44100.0); // previous device settings restored

    std::string err;
    auto sd = readWavFile (path, err);
    CHECK (sd != nullptr);
    if (sd)
    {
        CHECK (sd->numChannels == 2);
        CHECK (sd->numFrames == r.frames);
        float pk = 0.0f;
        bool finite = true;
        for (auto& ch : sd->channels)
            for (float v : ch) { pk = std::max (pk, std::abs (v)); finite = finite && std::isfinite (v); }
        CHECK (finite);
        CHECK_NEAR (gainToDb (pk), r.samplePeakDb, 1e-3);
    }
    REPORT ("demo render length (s)", r.seconds);
    REPORT ("demo render speed (x real time)", r.seconds / r.renderSeconds);
    REPORT ("demo sample peak (dBFS)", r.samplePeakDb);
    REPORT ("demo true peak (dBTP)", r.truePeakDb);
    REPORT ("demo integrated loudness (LUFS)", r.integratedLufs);
    std::remove (path.c_str());
}

PINK_TEST (wav_round_trip_all_formats)
{
    std::vector<float> l (1000), r (1000);
    for (int i = 0; i < 1000; ++i)
    {
        l[(size_t) i] = 0.9f * (float) std::sin (i * 0.05);
        r[(size_t) i] = -0.5f * (float) std::cos (i * 0.031);
    }
    const float* ch[2] = { l.data(), r.data() };

    struct Case { WavFormat f; double tol; const char* name; };
    for (auto c : { Case { WavFormat::Float32, 1e-7, "f32" }, Case { WavFormat::Pcm24, 2.0 / 8388608.0, "24" },
                    Case { WavFormat::Pcm16, 2.5 / 32768.0, "16" } })
    {
        // UTF-8 file name with an accented letter: exercises the wide-char path on Windows
        const std::string path = std::string ("pink_rt_\xC3\xA0_") + c.name + ".wav";
        WavWriter w;
        CHECK (w.open (path, 44100.0, 2, c.f));
        CHECK (w.write (ch, 1000));
        CHECK (w.close());
        std::string err;
        auto sd = readWavFile (path, err);
        CHECK (sd != nullptr);
        if (sd)
        {
            CHECK (sd->sampleRate == 44100.0 && sd->numFrames == 1000 && sd->numChannels == 2);
            double worst = 0.0;
            for (int i = 0; i < 1000; ++i)
            {
                worst = std::max (worst, (double) std::abs (sd->channels[0][(size_t) i] - l[(size_t) i]));
                worst = std::max (worst, (double) std::abs (sd->channels[1][(size_t) i] - r[(size_t) i]));
            }
            CHECK (worst <= c.tol);
        }
        std::remove (path.c_str());
    }
}

PINK_TEST (audio_clip_playback_with_resampling)
{
    AudioEngine engine;
    auto& p = engine.getProject();
    p.tempo.setConstantTempo (120.0);
    auto sd = std::make_shared<SampleData>();
    sd->sampleRate = 44100.0;
    sd->numChannels = 1;
    sd->numFrames = 44100;
    sd->channels.assign (1, std::vector<float> (44100));
    for (int i = 0; i < 44100; ++i) sd->channels[0][(size_t) i] = 0.5f * (float) std::sin (2 * kPi * 441.0 * i / 44100.0);

    auto& t = p.addTrack ("audio", TrackType::Audio, 1);
    auto clip = std::make_shared<AudioClip>();
    clip->start = 1.0;     // 0.5 s
    clip->length = 2.0;    // 1 s
    clip->sample = sd;
    t.audioClips.push_back (clip);

    engine.prepare (kSR, 512);
    engine.play();
    Runner run (engine, 512);
    run.run ((long long) (2.0 * kSR));

    CHECK (peakDb (std::vector<float> (run.left.begin(), run.left.begin() + 23990)) < -200.0); // silent before the clip
    const double f = estimateFrequency (std::vector<float> (run.left.begin() + 26000, run.left.begin() + 70000), kSR);
    CHECK_NEAR (f, 441.0, 0.2);                       // resampled 44.1k -> 48k keeps the pitch
    CHECK (peakDb (run.left, 72100) < -200.0);        // silent after the clip end (1.5 s)
    REPORT ("441 Hz clip recorded at 44.1 kHz, played at 48 kHz (Hz)", f);
}
