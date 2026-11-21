// PINKSTUDIO - real-time utilities shared by the whole engine.
// Everything in this header is safe to use on the audio thread.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
  #include <xmmintrin.h>
  #define PINK_HAS_SSE 1
#else
  #define PINK_HAS_SSE 0
#endif

namespace pink
{
constexpr double kPi = std::numbers::pi;
constexpr double kTwoPi = 2.0 * std::numbers::pi;

inline float dbToGain (float db) noexcept
{
    return db <= -144.0f ? 0.0f : std::pow (10.0f, db * 0.05f);
}

inline float gainToDb (float gain, float floorDb = -144.0f) noexcept
{
    return gain > 0.0f ? std::max (floorDb, 20.0f * std::log10 (gain)) : floorDb;
}

inline double midiNoteToHz (double note) noexcept
{
    return 440.0 * std::pow (2.0, (note - 69.0) / 12.0);
}

/** Flushes denormals to zero for the lifetime of the object (x86 FTZ + DAZ).
    Create one at the top of every audio callback. */
class ScopedNoDenormals
{
public:
    ScopedNoDenormals() noexcept
    {
       #if PINK_HAS_SSE
        previous = _mm_getcsr();
        _mm_setcsr (previous | 0x8040u); // FTZ (bit 15) | DAZ (bit 6)
       #endif
    }

    ~ScopedNoDenormals() noexcept
    {
       #if PINK_HAS_SSE
        _mm_setcsr (previous);
       #endif
    }

    ScopedNoDenormals (const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator= (const ScopedNoDenormals&) = delete;

private:
   #if PINK_HAS_SSE
    unsigned int previous = 0;
   #endif
};

/** A float that can be written by the UI and read by the audio thread without locks. */
class AtomicFloat
{
public:
    AtomicFloat (float initial = 0.0f) noexcept : value (initial) {}
    float load() const noexcept               { return value.load (std::memory_order_relaxed); }
    void store (float v) noexcept             { value.store (v, std::memory_order_relaxed); }
    operator float() const noexcept           { return load(); }
    AtomicFloat& operator= (float v) noexcept { store (v); return *this; }

private:
    std::atomic<float> value;
    static_assert (std::atomic<float>::is_always_lock_free, "float atomics must be lock-free");
};

/** Linear ramp used to de-zipper gain, pan and other continuous parameters. */
class LinearSmoother
{
public:
    void reset (double sampleRate, double rampSeconds) noexcept
    {
        rampLength = std::max (1, (int) std::lround (sampleRate * rampSeconds));
        countdown = 0;
        current = target;
    }

    void setCurrentAndTarget (float v) noexcept { current = target = v; countdown = 0; }

    void setTarget (float newTarget) noexcept
    {
        if (newTarget == target)
            return;

        target = newTarget;
        countdown = rampLength;
        step = (target - current) / (float) rampLength;
    }

    float next() noexcept
    {
        if (countdown <= 0)
            return target;

        --countdown;
        current = countdown == 0 ? target : current + step;
        return current;
    }

    bool isSmoothing() const noexcept { return countdown > 0; }
    float getTarget() const noexcept  { return target; }
    float getCurrent() const noexcept { return current; }

private:
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int rampLength = 1, countdown = 0;
};

/** Cheap deterministic white-noise source (xorshift32), audio-thread safe. */
class FastRandom
{
public:
    explicit FastRandom (uint32_t seed = 0x9E3779B9u) noexcept : state (seed ? seed : 1u) {}

    uint32_t nextU32() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    /** Uniform in [-1, 1). */
    float nextBipolar() noexcept { return (float) ((double) nextU32() * (2.0 / 4294967296.0) - 1.0); }
    /** Uniform in [0, 1). */
    float nextUnipolar() noexcept { return (float) ((double) nextU32() * (1.0 / 4294967296.0)); }

private:
    uint32_t state;
};

} // namespace pink
