// PINKSTUDIO - metering: peak/RMS, ITU-R BS.1770-4 / EBU R128 loudness (LUFS),
// 4x-oversampled true peak and stereo phase correlation.
// process() runs on the audio thread; readings are published through atomics for the UI.
#pragma once

#include "pink/core/RtUtils.h"
#include "pink/dsp/Biquad.h"

#include <array>
#include <atomic>
#include <vector>

namespace pink
{
/** Per-channel block peak and 300 ms RMS (linear values). */
class LevelMeter
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate);
    void reset() noexcept;
    void process (const float* const* channels, int numChannels, int numFrames) noexcept;

    float getPeak (int channel) const noexcept { return peak[(size_t) channel].load(); }
    float getRms (int channel) const noexcept  { return rms[(size_t) channel].load(); }

private:
    float rmsCoef = 0.0f;
    std::array<float, kMaxChannels> msState {};
    std::array<AtomicFloat, kMaxChannels> peak {}, rms {};
};

/** Momentary (400 ms), short-term (3 s) and gated integrated loudness. */
class LoudnessMeter
{
public:
    static constexpr float kSilence = -144.0f;

    void prepare (double sampleRate);
    /** Message thread or audio thread: clears the integrated measurement. */
    void requestReset() noexcept { resetRequested.store (true); }
    void process (const float* left, const float* right, int numFrames) noexcept;

    float getMomentaryLufs() const noexcept  { return momentary.load(); }
    float getShortTermLufs() const noexcept  { return shortTerm.load(); }
    float getIntegratedLufs() const noexcept { return integrated.load(); }

    /** K-weighting filter coefficients for a given rate (stage 1 = shelf, stage 2 = high-pass). */
    static void designKWeighting (double sampleRate, BiquadCoefficients& shelf, BiquadCoefficients& highPass) noexcept;

private:
    static constexpr int kHopsShortTerm = 30;   // 30 x 100 ms = 3 s
    static constexpr int kHopsMomentary = 4;    // 4 x 100 ms = 400 ms, 75 % overlap
    static constexpr int kBins = 800;           // 0.1 LU bins from -70 to +10 LUFS

    void clearMeasurement() noexcept;
    static float energyToLufs (double energy) noexcept;

    std::array<std::array<Biquad, 2>, 2> filters {}; // [channel][stage]
    int hopLength = 4800, hopFill = 0;
    std::array<double, 2> hopAccum {};
    std::array<double, kHopsShortTerm> hopEnergies {};
    int hopsWritten = 0;
    std::vector<double> binEnergy;
    std::vector<uint32_t> binCount;
    std::atomic<bool> resetRequested { false };
    AtomicFloat momentary { kSilence }, shortTerm { kSilence }, integrated { kSilence };
};

/** True-peak estimate by 4x polyphase windowed-sinc interpolation (BS.1770-4 Annex 2 method). */
class TruePeakMeter
{
public:
    static constexpr int kOversampling = 4;
    static constexpr int kTapsPerPhase = 16;

    void prepare();
    void requestReset() noexcept { resetRequested.store (true); }
    void process (const float* const* channels, int numChannels, int numFrames) noexcept;

    /** Highest true peak since the last reset (linear). */
    float getMaxTruePeak() const noexcept { return maxPeak.load(); }
    /** Highest true peak of the most recent block (linear), for moving meters. */
    float getBlockTruePeak() const noexcept { return blockPeak.load(); }

private:
    std::array<std::array<float, kTapsPerPhase>, kOversampling> phases {};
    std::array<std::array<float, kTapsPerPhase * 2>, 2> history {}; // doubled ring for contiguous reads
    int historyPos = 0;
    std::atomic<bool> resetRequested { false };
    AtomicFloat maxPeak { 0.0f }, blockPeak { 0.0f };
    float runningMax = 0.0f;
};

/** Stereo phase correlation in [-1, +1] with ~300 ms integration. */
class CorrelationMeter
{
public:
    void prepare (double sampleRate);
    void reset() noexcept { lr = ll = rr = 0.0; correlation.store (0.0f); }
    void process (const float* left, const float* right, int numFrames) noexcept;
    float getCorrelation() const noexcept { return correlation.load(); }

private:
    double coef = 0.0, lr = 0.0, ll = 0.0, rr = 0.0;
    AtomicFloat correlation { 0.0f };
};

} // namespace pink
