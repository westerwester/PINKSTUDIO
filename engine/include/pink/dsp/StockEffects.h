// PINKSTUDIO - first stock effects used by the engine and the demo session.
// Every effect does real DSP; none of them is a decorative placeholder.
//   PinkEQ        8-band parametric EQ (RBJ biquads)
//   PinkComp      feed-forward compressor with soft knee, RMS/peak detector,
//                 external sidechain and dry/wet mix (parallel compression)
//   PinkLimiter   look-ahead brick-wall limiter (sample-peak safe)
//   PinkDelay     tempo-synced stereo / ping-pong delay with damping
//   PinkUtility   gain, phase invert, mono maker, stereo width
#pragma once

#include "pink/dsp/Biquad.h"
#include "pink/dsp/Processor.h"

#include <array>
#include <vector>

namespace pink
{
// ---------------------------------------------------------------------------
class ParametricEq final : public Processor
{
public:
    static constexpr int kNumBands = 8;

    ParametricEq();
    std::string getName() const override { return "PinkEQ"; }
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (AudioBuffer& io, int numFrames, const ProcessContext& context) noexcept override;

    /** Total response of all enabled bands at a frequency, in dB (for the UI curve and tests). */
    double getResponseDb (double frequency) const noexcept;

    /** Helpers to configure a band in code (presets, tests, demo). */
    void setBand (int band, bool enabled, FilterType type, float frequency, float gainDb, float q);

private:
    struct BandParams { Parameter* on; Parameter* type; Parameter* freq; Parameter* gain; Parameter* q; };
    struct BandState
    {
        std::array<Biquad, 2> filters;
        float lastFreq = -1, lastGain = -1000, lastQ = -1;
        int lastType = -1;
    };

    void updateCoefficients (bool force) noexcept;

    std::array<BandParams, kNumBands> bands {};
    std::array<BandState, kNumBands> state {};
    Parameter* outputGain = nullptr;
    LinearSmoother outSmoother;
    double sampleRate = 48000.0;
};

// ---------------------------------------------------------------------------
class Compressor final : public Processor
{
public:
    Compressor();
    std::string getName() const override { return "PinkComp"; }
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (AudioBuffer& io, int numFrames, const ProcessContext& context) noexcept override;
    bool acceptsSidechain() const noexcept override { return true; }

    /** Current gain reduction in dB (positive number), readable from the UI thread. */
    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }

    /** Static gain computer (exposed for tests): output level for a given input level, in dB. */
    static float computeOutputLevelDb (float inputDb, float thresholdDb, float ratio, float kneeDb) noexcept;

private:
    Parameter *threshold, *ratio, *attack, *release, *knee, *makeup, *mix, *detector, *externalSidechain;
    double sampleRate = 48000.0;
    float smoothedReductionDb = 0.0f;
    float rmsState = 0.0f;
    AtomicFloat gainReductionDb { 0.0f };
};

// ---------------------------------------------------------------------------
class Limiter final : public Processor
{
public:
    Limiter();
    std::string getName() const override { return "PinkLimiter"; }
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (AudioBuffer& io, int numFrames, const ProcessContext& context) noexcept override;
    int getLatencySamples() const noexcept override { return lookahead - 1; }

    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }

private:
    Parameter *inputGain, *ceiling, *release;
    double sampleRate = 48000.0;
    int lookahead = 72;

    // delay line for the audio (per channel) and gain pipeline
    std::array<std::vector<float>, 2> delay;
    int delayPos = 0;

    // sliding-window minimum (monotonic deque in a ring)
    std::vector<float> dequeValue;
    std::vector<int64_t> dequeIndex;
    int dequeHead = 0, dequeCount = 0;

    // box filter over the sliding minimum
    std::vector<float> boxRing;
    int boxPos = 0;
    double boxSum = 0.0;

    int64_t sampleCounter = 0;
    float releasedGain = 1.0f;
    AtomicFloat gainReductionDb { 0.0f };
};

// ---------------------------------------------------------------------------
class StereoDelay final : public Processor
{
public:
    StereoDelay();
    std::string getName() const override { return "PinkDelay"; }
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (AudioBuffer& io, int numFrames, const ProcessContext& context) noexcept override;

    static const std::vector<std::string>& syncNames();
    static double syncToBeats (int index) noexcept;

private:
    Parameter *sync, *timeMs, *feedback, *mix, *pingPong, *damping;
    double sampleRate = 48000.0;
    std::array<std::vector<float>, 2> lines;
    int writePos = 0;
    std::array<float, 2> dampState { 0.0f, 0.0f };
    LinearSmoother delaySmoother;
};

// ---------------------------------------------------------------------------
class Utility final : public Processor
{
public:
    Utility();
    std::string getName() const override { return "PinkUtility"; }
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override {}
    void process (AudioBuffer& io, int numFrames, const ProcessContext& context) noexcept override;

private:
    Parameter *gain, *invertL, *invertR, *mono, *width;
    LinearSmoother gainSmoother;
};

} // namespace pink
