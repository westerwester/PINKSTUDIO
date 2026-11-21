// PINKSTUDIO - biquad filter (RBJ Audio EQ Cookbook designs), transposed direct form II,
// double-precision state for low noise at low frequencies.
#pragma once

#include "pink/core/RtUtils.h"

namespace pink
{
enum class FilterType
{
    LowPass, HighPass, BandPass, Notch, Peak, LowShelf, HighShelf, AllPass
};

struct BiquadCoefficients
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    static BiquadCoefficients design (FilterType type, double sampleRate, double frequency, double q, double gainDb) noexcept
    {
        frequency = std::clamp (frequency, 1.0, sampleRate * 0.499);
        q = std::max (q, 0.025);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = kTwoPi * frequency / sampleRate;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

        switch (type)
        {
            case FilterType::LowPass:
                b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
                break;
            case FilterType::HighPass:
                b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
                break;
            case FilterType::BandPass: // constant 0 dB peak gain
                b0 = alpha; b1 = 0; b2 = -alpha;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
                break;
            case FilterType::Notch:
                b0 = 1; b1 = -2 * cw; b2 = 1;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
                break;
            case FilterType::Peak:
                b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
                a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
                break;
            case FilterType::LowShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) - (A - 1) * cw + sq);
                b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                b2 = A * ((A + 1) - (A - 1) * cw - sq);
                a0 = (A + 1) + (A - 1) * cw + sq;
                a1 = -2 * ((A - 1) + (A + 1) * cw);
                a2 = (A + 1) + (A - 1) * cw - sq;
                break;
            }
            case FilterType::HighShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) + (A - 1) * cw + sq);
                b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                b2 = A * ((A + 1) + (A - 1) * cw - sq);
                a0 = (A + 1) - (A - 1) * cw + sq;
                a1 = 2 * ((A - 1) - (A + 1) * cw);
                a2 = (A + 1) - (A - 1) * cw - sq;
                break;
            }
            case FilterType::AllPass:
                b0 = 1 - alpha; b1 = -2 * cw; b2 = 1 + alpha;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
                break;
        }

        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    /** Magnitude response in dB at a given frequency (used by EQ displays and tests). */
    double magnitudeDb (double frequency, double sampleRate) const noexcept
    {
        const double w = kTwoPi * frequency / sampleRate;
        const double c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const double dr = 1 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        const double mag2 = (nr * nr + ni * ni) / (dr * dr + di * di);
        return 10.0 * std::log10 (std::max (mag2, 1.0e-30));
    }
};

class Biquad
{
public:
    void setCoefficients (const BiquadCoefficients& c) noexcept { coeffs = c; }
    const BiquadCoefficients& getCoefficients() const noexcept { return coeffs; }
    void reset() noexcept { z1 = z2 = 0.0; }

    inline float processSample (float input) noexcept
    {
        const double x = input;
        const double y = coeffs.b0 * x + z1;
        z1 = coeffs.b1 * x - coeffs.a1 * y + z2;
        z2 = coeffs.b2 * x - coeffs.a2 * y;
        return (float) y;
    }

    void process (float* data, int numFrames) noexcept
    {
        for (int i = 0; i < numFrames; ++i)
            data[i] = processSample (data[i]);
    }

private:
    BiquadCoefficients coeffs;
    double z1 = 0.0, z2 = 0.0;
};

} // namespace pink
