#pragma once

#include "pink/core/AudioBuffer.h"
#include "pink/dsp/Processor.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace pinktest
{
constexpr double kSR = 48000.0;

inline void fillSine (pink::AudioBuffer& b, int n, double freq, float amp, double sr = kSR, double phase0 = 0.0, long long startSample = 0)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < n; ++i)
            b.getWritePointer (c)[i] = amp * (float) std::sin (2.0 * pink::kPi * freq * (double) (startSample + i) / sr + phase0);
}

inline double rmsDb (const std::vector<float>& v, size_t from = 0)
{
    double s = 0.0;
    for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
    const double ms = s / std::max<size_t> (1, v.size() - from);
    return ms > 0 ? 10.0 * std::log10 (ms) : -300.0;
}

inline double peakDb (const std::vector<float>& v, size_t from = 0)
{
    float p = 0.0f;
    for (size_t i = from; i < v.size(); ++i) p = std::max (p, std::abs (v[i]));
    return p > 0 ? 20.0 * std::log10 (p) : -300.0;
}

/** Frequency estimate from rising zero crossings with linear interpolation. */
inline double estimateFrequency (const std::vector<float>& v, double sr, size_t from = 0)
{
    double first = -1, last = -1;
    int crossings = 0;
    for (size_t i = std::max<size_t> (from, 1); i < v.size(); ++i)
    {
        if (v[i - 1] < 0.0f && v[i] >= 0.0f)
        {
            const double t = (double) (i - 1) + (double) (-v[i - 1]) / (double) (v[i] - v[i - 1]);
            if (first < 0) first = t;
            last = t;
            ++crossings;
        }
    }
    return crossings > 1 ? (crossings - 1) * sr / (last - first) : 0.0;
}

/** Runs a mono test signal through a stereo processor in blocks; returns the left output. */
template <typename Gen>
std::vector<float> runProcessor (pink::Processor& p, int totalFrames, int block, Gen&& gen,
                                 const pink::AudioBuffer* sidechain = nullptr, double sr = kSR)
{
    pink::AudioBuffer buf (2, block);
    std::vector<float> out;
    out.reserve ((size_t) totalFrames);
    pink::ProcessContext ctx;
    ctx.sampleRate = sr;
    ctx.sidechain = sidechain;
    long long pos = 0;
    while (pos < totalFrames)
    {
        const int n = (int) std::min<long long> (block, totalFrames - pos);
        for (int i = 0; i < n; ++i)
        {
            const float x = gen (pos + i);
            buf.getWritePointer (0)[i] = x;
            buf.getWritePointer (1)[i] = x;
        }
        p.process (buf, n, ctx);
        out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + n);
        pos += n;
    }
    return out;
}

/** Instrument that records every received event with its absolute sample position. */
class ProbeInstrument final : public pink::Instrument
{
public:
    struct Logged { long long sample; uint8_t status, data1; };

    ProbeInstrument() { log.reserve (100000); }
    std::string getName() const override { return "Probe"; }
    void prepare (double, int) override { rendered = 0; }
    void reset() noexcept override {}
    int getActiveVoiceCount() const noexcept override { return 0; }

    void render (pink::AudioBuffer& out, int n, const pink::MidiEventList& events, const pink::ProcessContext&) noexcept override
    {
        out.clear (0, n);
        for (const auto& e : events)
            if (log.size() < log.capacity())
                log.push_back ({ rendered + e.sampleOffset, (uint8_t) (e.status & 0xF0), e.data1 });
        rendered += n;
    }

    std::vector<Logged> log;
    long long rendered = 0;
};

/** Instrument that outputs a constant value on both channels (for routing/gain tests). */
class ConstantSource final : public pink::Instrument
{
public:
    explicit ConstantSource (float v) : value (v) {}
    std::string getName() const override { return "Constant"; }
    void prepare (double, int) override {}
    void reset() noexcept override {}
    int getActiveVoiceCount() const noexcept override { return 1; }
    void render (pink::AudioBuffer& out, int n, const pink::MidiEventList&, const pink::ProcessContext&) noexcept override
    {
        for (int c = 0; c < out.getNumChannels(); ++c)
            std::fill (out.getWritePointer (c), out.getWritePointer (c) + n, value);
    }
    float value;
};
} // namespace pinktest
