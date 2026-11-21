// PINKSTUDIO - metronome click generator (accented downbeat), rendered by the engine
// directly into the device output, after the master bus.
#pragma once

#include "pink/core/RtUtils.h"

#include <array>
#include <cmath>

namespace pink
{
class Metronome
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        decay = (float) std::exp (-1.0 / (0.018 * sr));
        reset();
    }

    void reset() noexcept { pendingCount = 0; level = 0.0f; }

    /** Schedules a click at a sample offset of the next render() call. */
    void schedule (int offset, bool accent) noexcept
    {
        if (pendingCount < (int) pending.size())
            pending[(size_t) pendingCount++] = { offset, accent };
    }

    /** Adds the clicks to left/right. gain is linear. */
    void render (float* left, float* right, int numFrames, float gain) noexcept
    {
        int next = 0;
        for (int i = 0; i < numFrames; ++i)
        {
            while (next < pendingCount && pending[(size_t) next].offset <= i)
            {
                level = 1.0f;
                phase = 0.0;
                freq = pending[(size_t) next].accent ? 1760.0 : 1175.0;
                ++next;
            }

            if (level > 1.0e-4f)
            {
                const float s = (float) std::sin (kTwoPi * phase) * level * gain;
                phase += freq / sampleRate;
                if (phase >= 1.0) phase -= 1.0;
                level *= decay;
                left[i] += s;
                right[i] += s;
            }
        }
        pendingCount = 0;
    }

private:
    struct Click { int offset; bool accent; };
    std::array<Click, 32> pending {};
    int pendingCount = 0;
    double sampleRate = 48000.0, phase = 0.0, freq = 1000.0;
    float level = 0.0f, decay = 0.999f;
};

} // namespace pink
