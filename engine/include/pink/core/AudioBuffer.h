// PINKSTUDIO - non-interleaved float audio buffer.
// Memory is allocated only by setSize() (message thread). All other methods are real-time safe.
#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

namespace pink
{
class AudioBuffer
{
public:
    AudioBuffer() = default;
    AudioBuffer (int numChannels, int numFrames) { setSize (numChannels, numFrames); }

    /** Allocates storage. NOT real-time safe. */
    void setSize (int numChannels, int numFrames)
    {
        channels = std::max (0, numChannels);
        capacity = std::max (0, numFrames);
        storage.assign ((size_t) channels * (size_t) capacity, 0.0f);
        pointers.resize ((size_t) channels);
        for (int c = 0; c < channels; ++c)
            pointers[(size_t) c] = storage.data() + (size_t) c * (size_t) capacity;
    }

    int getNumChannels() const noexcept { return channels; }
    int getCapacity() const noexcept    { return capacity; }

    float* getWritePointer (int channel) noexcept
    {
        assert (channel >= 0 && channel < channels);
        return pointers[(size_t) channel];
    }

    const float* getReadPointer (int channel) const noexcept
    {
        assert (channel >= 0 && channel < channels);
        return pointers[(size_t) channel];
    }

    float* const* getArrayOfWritePointers() noexcept { return pointers.data(); }

    void clear() noexcept { std::fill (storage.begin(), storage.end(), 0.0f); }

    void clear (int startFrame, int numFrames) noexcept
    {
        assert (startFrame >= 0 && startFrame + numFrames <= capacity);
        for (int c = 0; c < channels; ++c)
            std::memset (pointers[(size_t) c] + startFrame, 0, sizeof (float) * (size_t) numFrames);
    }

    /** this[ch] += source[ch] * gain for the first numFrames samples. */
    void addFrom (const AudioBuffer& source, int numFrames, float gain = 1.0f) noexcept
    {
        const int n = std::min (numFrames, std::min (capacity, source.capacity));
        const int ch = std::min (channels, source.channels);
        for (int c = 0; c < ch; ++c)
        {
            float* d = pointers[(size_t) c];
            const float* s = source.pointers[(size_t) c];
            for (int i = 0; i < n; ++i)
                d[i] += s[i] * gain;
        }
    }

    void copyFrom (const AudioBuffer& source, int numFrames) noexcept
    {
        const int n = std::min (numFrames, std::min (capacity, source.capacity));
        const int ch = std::min (channels, source.channels);
        for (int c = 0; c < ch; ++c)
            std::memcpy (pointers[(size_t) c], source.pointers[(size_t) c], sizeof (float) * (size_t) n);
    }

    void applyGain (int numFrames, float gain) noexcept
    {
        for (int c = 0; c < channels; ++c)
        {
            float* d = pointers[(size_t) c];
            for (int i = 0; i < numFrames; ++i)
                d[i] *= gain;
        }
    }

    float getPeak (int channel, int numFrames) const noexcept
    {
        float peak = 0.0f;
        const float* s = pointers[(size_t) channel];
        for (int i = 0; i < numFrames; ++i)
            peak = std::max (peak, std::abs (s[i]));
        return peak;
    }

    bool isSilent (int numFrames, float threshold = 1.0e-7f) const noexcept
    {
        for (int c = 0; c < channels; ++c)
            if (getPeak (c, numFrames) > threshold)
                return false;
        return true;
    }

private:
    std::vector<float> storage;
    std::vector<float*> pointers;
    int channels = 0, capacity = 0;
};

} // namespace pink
