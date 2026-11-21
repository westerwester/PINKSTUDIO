#include "TestFramework.h"
#include "TestHelpers.h"

#include "pink/dsp/StockEffects.h"

#include <random>

using namespace pink;
using namespace pinktest;

namespace
{
double measureGainDb (Processor& p, double freq, float amp = 0.25f)
{
    p.prepare (kSR, 512);
    const int total = (int) kSR; // 1 s
    auto out = runProcessor (p, total, 512, [&] (long long i) { return amp * (float) std::sin (2 * kPi * freq * i / kSR); });
    return rmsDb (out, (size_t) (total / 2)) - 20.0 * std::log10 (amp / std::sqrt (2.0));
}
} // namespace

PINK_TEST (biquad_peak_and_lowpass_responses)
{
    const auto c = BiquadCoefficients::design (FilterType::Peak, kSR, 1000.0, 1.0, 6.0);
    CHECK_NEAR (c.magnitudeDb (1000.0, kSR), 6.0, 1e-6);
    CHECK_NEAR (c.magnitudeDb (20.0, kSR), 0.0, 0.05);

    const auto lp = BiquadCoefficients::design (FilterType::LowPass, kSR, 1000.0, 0.7071, 0.0);
    CHECK_NEAR (lp.magnitudeDb (1000.0, kSR), -3.01, 0.05);
    CHECK (lp.magnitudeDb (10000.0, kSR) < -35.0);
    REPORT ("2nd-order LP 1 kHz: attenuation at 10 kHz (dB)", lp.magnitudeDb (10000.0, kSR));

    const auto hs = BiquadCoefficients::design (FilterType::HighShelf, kSR, 4000.0, 0.7071, -6.0);
    CHECK_NEAR (hs.magnitudeDb (18000.0, kSR), -6.0, 0.4);
    CHECK_NEAR (hs.magnitudeDb (100.0, kSR), 0.0, 0.05);
}

PINK_TEST (parametric_eq_measured_matches_design)
{
    for (double f : { 40.0, 200.0, 1000.0, 5000.0 })
    {
        ParametricEq copy;
        copy.setBand (2, true, FilterType::Peak, 1000.0f, 6.0f, 1.0f);
        copy.setBand (0, true, FilterType::HighPass, 80.0f, 0.0f, 0.707f);
        const double measured = measureGainDb (copy, f);
        copy.prepare (kSR, 512);
        CHECK_NEAR (measured, copy.getResponseDb (f), 0.1);
        REPORT ("EQ measured gain at " + std::to_string ((int) f) + " Hz (dB)", measured);
    }
}

PINK_TEST (compressor_static_curve)
{
    CHECK_NEAR (Compressor::computeOutputLevelDb (-10.0f, -20.0f, 4.0f, 0.0f), -17.5, 1e-5);
    CHECK_NEAR (Compressor::computeOutputLevelDb (-30.0f, -20.0f, 4.0f, 0.0f), -30.0, 1e-5);
    CHECK_NEAR (Compressor::computeOutputLevelDb (-20.0f, -20.0f, 4.0f, 6.0f), -20.5625, 1e-4); // soft knee centre
    CHECK_NEAR (Compressor::computeOutputLevelDb (-24.0f, -20.0f, 4.0f, 6.0f), -24.0, 1e-5);
    CHECK_NEAR (Compressor::computeOutputLevelDb (0.0f, -20.0f, 1.0f, 6.0f), 0.0, 1e-5);
}

PINK_TEST (compressor_steady_state_gain_reduction)
{
    Compressor comp;
    auto& p = comp.getParameters();
    p.set ("threshold", -20.0f); p.set ("ratio", 4.0f); p.set ("attack", 1.0f);
    p.set ("release", 100.0f); p.set ("knee", 0.0f); p.set ("makeup", 0.0f);
    comp.prepare (kSR, 256);

    const float amp = dbToGain (-10.0f);
    auto out = runProcessor (comp, (int) kSR, 256, [&] (long long i) { return amp * (float) std::sin (2 * kPi * 1000.0 * i / kSR); });
    const double outPeak = peakDb (out, out.size() / 2);
    CHECK_NEAR (outPeak, -17.5, 0.5);
    CHECK_NEAR (comp.getGainReductionDb(), 7.5, 0.5);
    REPORT ("-10 dBFS sine, T=-20, R=4: output peak (dBFS, ideal -17.5)", outPeak);

    // parallel compression: 50 % mix lands between dry and fully compressed
    p.set ("mix", 50.0f);
    comp.prepare (kSR, 256);
    auto par = runProcessor (comp, (int) kSR, 256, [&] (long long i) { return amp * (float) std::sin (2 * kPi * 1000.0 * i / kSR); });
    const double parPeak = peakDb (par, par.size() / 2);
    CHECK (parPeak > outPeak + 1.0 && parPeak < -10.0);
    REPORT ("same with 50 % parallel mix: output peak (dBFS)", parPeak);
}

PINK_TEST (compressor_external_sidechain)
{
    Compressor comp;
    auto& p = comp.getParameters();
    p.set ("threshold", -30.0f); p.set ("ratio", 10.0f); p.set ("attack", 0.5f);
    p.set ("release", 50.0f); p.set ("knee", 0.0f); p.set ("sidechain", 1.0f);
    comp.prepare (kSR, 256);

    AudioBuffer sc (2, 256);
    for (int c = 0; c < 2; ++c) std::fill (sc.getWritePointer (c), sc.getWritePointer (c) + 256, 0.5f);
    const float amp = dbToGain (-40.0f); // well below threshold on its own
    auto ducked = runProcessor (comp, 24000, 256, [&] (long long i) { return amp * (float) std::sin (2 * kPi * 440.0 * i / kSR); }, &sc);
    const double duckedDb = peakDb (ducked, 12000);

    AudioBuffer silent (2, 256);
    silent.clear();
    comp.prepare (kSR, 256);
    auto clean = runProcessor (comp, 24000, 256, [&] (long long i) { return amp * (float) std::sin (2 * kPi * 440.0 * i / kSR); }, &silent);
    const double cleanDb = peakDb (clean, 12000);

    CHECK_NEAR (cleanDb, -40.0, 0.1);
    CHECK (duckedDb < cleanDb - 20.0);
    REPORT ("sidechain ducking depth (dB)", duckedDb - cleanDb);
}

PINK_TEST (limiter_never_exceeds_ceiling)
{
    Limiter lim;
    lim.getParameters().set ("ceiling", -1.0f);
    lim.getParameters().set ("release", 50.0f);
    lim.prepare (kSR, 512);
    const float ceiling = dbToGain (-1.0f);

    std::mt19937 rng (42);
    std::normal_distribution<float> noise (0.0f, 0.8f);
    auto out = runProcessor (lim, (int) kSR * 2, 512, [&] (long long i)
    {
        const float sine = 2.0f * (float) std::sin (2 * kPi * 97.0 * i / kSR);   // +6 dBFS
        const float burst = (i / 4800) % 3 == 0 ? noise (rng) * 3.0f : 0.0f;      // spiky bursts
        return sine + burst;
    });

    float maxOut = 0.0f;
    for (float v : out) maxOut = std::max (maxOut, std::abs (v));
    CHECK (maxOut <= ceiling + 1.0e-6f);
    REPORT ("limiter: max output (dBFS, ceiling -1.0)", gainToDb (maxOut));
    REPORT ("limiter: latency (samples @48k)", lim.getLatencySamples());

    // a quiet signal passes untouched (after the look-ahead delay)
    lim.prepare (kSR, 512);
    auto quiet = runProcessor (lim, 4800, 512, [&] (long long i) { return 0.1f * (float) std::sin (2 * kPi * 500.0 * i / kSR); });
    const int lat = lim.getLatencySamples();
    double worst = 0.0;
    for (int i = lat + 10; i < 4800; ++i)
        worst = std::max (worst, std::abs (quiet[(size_t) i] - 0.1 * std::sin (2 * kPi * 500.0 * (i - lat) / kSR)));
    CHECK (worst < 1e-5);
}

PINK_TEST (delay_tempo_sync_position)
{
    StereoDelay d;
    auto& p = d.getParameters();
    p.set ("sync", 7.0f);        // 1/8 dotted = 0.75 beats
    p.set ("mix", 100.0f);
    p.set ("feedback", 0.0f);
    p.set ("pingpong", 0.0f);
    p.set ("damping", 0.0f);
    d.prepare (kSR, 512);

    AudioBuffer buf (2, 512);
    ProcessContext ctx;
    ctx.sampleRate = kSR;
    ctx.bpm = 120.0;
    std::vector<float> out;
    for (int b = 0; b < 60; ++b)
    {
        buf.clear();
        if (b == 0) { buf.getWritePointer (0)[0] = 1.0f; buf.getWritePointer (1)[0] = 1.0f; }
        d.process (buf, 512, ctx);
        out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + 512);
    }
    const auto it = std::max_element (out.begin(), out.end());
    const long pos = (long) (it - out.begin());
    CHECK (std::abs (pos - 18000) <= 1); // 0.75 beats @120 BPM = 0.375 s
    REPORT ("1/8 dotted @120 BPM echo position (samples, ideal 18000)", pos);
}

PINK_TEST (utility_mono_and_phase)
{
    Utility u;
    u.prepare (kSR, 64);
    AudioBuffer b (2, 64);
    ProcessContext ctx;
    for (int i = 0; i < 64; ++i) { b.getWritePointer (0)[i] = 1.0f; b.getWritePointer (1)[i] = -1.0f; }
    u.getParameters().set ("mono", 1.0f);
    u.process (b, 64, ctx);
    CHECK_NEAR (b.getPeak (0, 64), 0.0, 1e-6);

    for (int i = 0; i < 64; ++i) { b.getWritePointer (0)[i] = 0.5f; b.getWritePointer (1)[i] = 0.5f; }
    u.getParameters().set ("mono", 0.0f);
    u.getParameters().set ("invert_l", 1.0f);
    u.process (b, 64, ctx);
    CHECK_NEAR (b.getReadPointer (0)[10], -0.5, 1e-6); // left polarity flipped, right untouched
    CHECK_NEAR (b.getReadPointer (1)[10], 0.5, 1e-6);
}
