#include "TestFramework.h"
#include "TestHelpers.h"

#include "pink/dsp/Meters.h"

#include <random>

using namespace pink;
using namespace pinktest;

namespace
{
/** Feeds stereo signal through a loudness meter in 480-sample blocks. */
template <typename GenL, typename GenR>
void feed (LoudnessMeter& m, double seconds, long long& clock, GenL&& gl, GenR&& gr)
{
    std::vector<float> l (480), r (480);
    const long long total = (long long) (seconds * kSR);
    for (long long done = 0; done < total; done += 480)
    {
        for (int i = 0; i < 480; ++i) { l[(size_t) i] = gl (clock + i); r[(size_t) i] = gr (clock + i); }
        m.process (l.data(), r.data(), 480);
        clock += 480;
    }
}
} // namespace

PINK_TEST (k_weighting_matches_bs1770_at_48k)
{
    BiquadCoefficients shelf, hp;
    LoudnessMeter::designKWeighting (48000.0, shelf, hp);
    // Coefficients published in ITU-R BS.1770-4, Tables 1 and 2
    CHECK_NEAR (shelf.b0, 1.53512485958697, 1e-6);
    CHECK_NEAR (shelf.b1, -2.69169618940638, 1e-6);
    CHECK_NEAR (shelf.b2, 1.19839281085285, 1e-6);
    CHECK_NEAR (shelf.a1, -1.69065929318241, 1e-6);
    CHECK_NEAR (shelf.a2, 0.73248077421585, 1e-6);
    CHECK_NEAR (hp.a1, -1.99004745483398, 1e-6);
    CHECK_NEAR (hp.a2, 0.99007225036621, 1e-6);
}

PINK_TEST (lufs_calibration_1khz_sine)
{
    const float amp = dbToGain (-20.0f);
    auto sine = [&] (long long i) { return amp * (float) std::sin (2 * kPi * 1000.0 * i / kSR); };
    auto silence = [] (long long) { return 0.0f; };

    LoudnessMeter stereo;
    stereo.prepare (kSR);
    long long clock = 0;
    feed (stereo, 10.0, clock, sine, sine);
    CHECK_NEAR (stereo.getIntegratedLufs(), -20.0, 0.1);
    CHECK_NEAR (stereo.getShortTermLufs(), -20.0, 0.1);
    CHECK_NEAR (stereo.getMomentaryLufs(), -20.0, 0.1);
    REPORT ("stereo 1 kHz sine @ -20 dBFS: integrated (LUFS, ideal -20.00)", stereo.getIntegratedLufs());

    LoudnessMeter mono;
    mono.prepare (kSR);
    clock = 0;
    feed (mono, 10.0, clock, sine, silence);
    CHECK_NEAR (mono.getIntegratedLufs(), -23.01, 0.1);
    REPORT ("1 kHz sine @ -20 dBFS left only (LUFS, ideal -23.01)", mono.getIntegratedLufs());
}

PINK_TEST (lufs_gating_absolute_and_relative)
{
    const float loud = dbToGain (-20.0f), quiet = dbToGain (-40.0f);
    auto tone = [] (float a) { return [a] (long long i) { return a * (float) std::sin (2 * kPi * 1000.0 * i / kSR); }; };
    auto silence = [] (long long) { return 0.0f; };

    LoudnessMeter m;
    m.prepare (kSR);
    long long clock = 0;
    feed (m, 10.0, clock, tone (loud), tone (loud));
    feed (m, 10.0, clock, silence, silence);       // removed by the -70 LUFS absolute gate
    CHECK_NEAR (m.getIntegratedLufs(), -20.0, 0.1);

    feed (m, 10.0, clock, tone (quiet), tone (quiet)); // removed by the -10 LU relative gate
    CHECK_NEAR (m.getIntegratedLufs(), -20.0, 0.15);
    REPORT ("10 s @-20 + 10 s silence + 10 s @-40: integrated (LUFS, ideal -20)", m.getIntegratedLufs());

    m.requestReset();
    feed (m, 5.0, clock, tone (quiet), tone (quiet));
    CHECK_NEAR (m.getIntegratedLufs(), -40.0, 0.1);
}

PINK_TEST (true_peak_detects_intersample_overs)
{
    TruePeakMeter tp;
    tp.prepare();

    // fs/4 sine with a 45 degree phase: every sample sits at +-0.707 but the waveform peaks at 1.0
    std::vector<float> l (4800), r (4800);
    for (int i = 0; i < 4800; ++i)
        l[(size_t) i] = r[(size_t) i] = (float) std::sin (2 * kPi * 12000.0 * i / kSR + kPi / 4);
    const float* ch[2] = { l.data(), r.data() };
    tp.process (ch, 2, 4800);

    float samplePeak = 0.0f;
    for (float v : l) samplePeak = std::max (samplePeak, std::abs (v));
    const float tpDb = gainToDb (tp.getMaxTruePeak());
    CHECK_NEAR (gainToDb (samplePeak), -3.01, 0.02);
    CHECK_NEAR (tpDb, 0.0, 0.3);
    REPORT ("fs/4 sine: sample peak (dBFS)", gainToDb (samplePeak));
    REPORT ("fs/4 sine: true peak (dBTP, ideal 0.0)", tpDb);

    tp.requestReset();
    for (int i = 0; i < 4800; ++i) l[(size_t) i] = r[(size_t) i] = 0.5f * (float) std::sin (2 * kPi * 997.0 * i / kSR);
    tp.process (ch, 2, 4800);
    CHECK_NEAR (gainToDb (tp.getMaxTruePeak()), -6.02, 0.1);
}

PINK_TEST (phase_correlation)
{
    CorrelationMeter cm;
    cm.prepare (kSR);
    std::vector<float> l (48000), r (48000);
    for (int i = 0; i < 48000; ++i) l[(size_t) i] = r[(size_t) i] = (float) std::sin (2 * kPi * 300.0 * i / kSR);
    cm.process (l.data(), r.data(), 48000);
    CHECK_NEAR (cm.getCorrelation(), 1.0, 0.01);

    for (auto& v : r) v = -v;
    cm.reset();
    cm.process (l.data(), r.data(), 48000);
    CHECK_NEAR (cm.getCorrelation(), -1.0, 0.01);

    std::mt19937 rng (3);
    std::normal_distribution<float> n (0.0f, 0.3f);
    for (size_t i = 0; i < l.size(); ++i) { l[i] = n (rng); r[i] = n (rng); }
    cm.reset();
    cm.process (l.data(), r.data(), 48000);
    CHECK (std::abs (cm.getCorrelation()) < 0.15);
    REPORT ("correlation of independent noise", cm.getCorrelation());
}
