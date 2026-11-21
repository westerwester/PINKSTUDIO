#include "pink/dsp/Meters.h"

#include <algorithm>
#include <cmath>

namespace pink
{
// ===========================================================================
// LevelMeter
// ===========================================================================
void LevelMeter::prepare (double sampleRate)
{
    rmsCoef = (float) std::exp (-1.0 / (0.3 * sampleRate));
    reset();
}

void LevelMeter::reset() noexcept
{
    msState.fill (0.0f);
    for (auto& p : peak) p.store (0.0f);
    for (auto& r : rms) r.store (0.0f);
}

void LevelMeter::process (const float* const* channels, int numChannels, int numFrames) noexcept
{
    const int n = std::min (numChannels, kMaxChannels);
    for (int c = 0; c < n; ++c)
    {
        const float* s = channels[c];
        float p = 0.0f, ms = msState[(size_t) c];
        for (int i = 0; i < numFrames; ++i)
        {
            const float x = s[i];
            p = std::max (p, std::abs (x));
            ms = rmsCoef * ms + (1.0f - rmsCoef) * x * x;
        }
        msState[(size_t) c] = ms;
        peak[(size_t) c].store (p);
        rms[(size_t) c].store (std::sqrt (ms));
    }
}

// ===========================================================================
// LoudnessMeter
// ===========================================================================
void LoudnessMeter::designKWeighting (double sampleRate, BiquadCoefficients& shelf, BiquadCoefficients& highPass) noexcept
{
    // Analog prototype parameters from BS.1770 re-derived for any sample rate
    // (same approach as libebur128). At 48 kHz these reproduce the published coefficients.
    double f0 = 1681.974450955533;
    const double G = 3.999843853973347;
    double Q = 0.7071752369554196;
    double K = std::tan (kPi * f0 / sampleRate);
    const double Vh = std::pow (10.0, G / 20.0);
    const double Vb = std::pow (Vh, 0.4996667741545416);
    double a0 = 1.0 + K / Q + K * K;
    shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
    shelf.b1 = 2.0 * (K * K - Vh) / a0;
    shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
    shelf.a1 = 2.0 * (K * K - 1.0) / a0;
    shelf.a2 = (1.0 - K / Q + K * K) / a0;

    f0 = 38.13547087602444;
    Q = 0.5003270373238773;
    K = std::tan (kPi * f0 / sampleRate);
    a0 = 1.0 + K / Q + K * K;
    highPass.b0 = 1.0;
    highPass.b1 = -2.0;
    highPass.b2 = 1.0;
    highPass.a1 = 2.0 * (K * K - 1.0) / a0;
    highPass.a2 = (1.0 - K / Q + K * K) / a0;
}

void LoudnessMeter::prepare (double sampleRate)
{
    BiquadCoefficients shelf, hp;
    designKWeighting (sampleRate, shelf, hp);
    for (auto& ch : filters)
    {
        ch[0].setCoefficients (shelf);
        ch[1].setCoefficients (hp);
    }

    hopLength = std::max (1, (int) std::lround (0.1 * sampleRate));
    binEnergy.assign (kBins, 0.0);
    binCount.assign (kBins, 0);
    clearMeasurement();
}

void LoudnessMeter::clearMeasurement() noexcept
{
    for (auto& ch : filters)
        for (auto& f : ch)
            f.reset();
    hopFill = 0;
    hopAccum.fill (0.0);
    hopEnergies.fill (0.0);
    hopsWritten = 0;
    std::fill (binEnergy.begin(), binEnergy.end(), 0.0);
    std::fill (binCount.begin(), binCount.end(), 0u);
    momentary.store (kSilence);
    shortTerm.store (kSilence);
    integrated.store (kSilence);
}

float LoudnessMeter::energyToLufs (double energy) noexcept
{
    return energy > 1.0e-20 ? (float) (-0.691 + 10.0 * std::log10 (energy)) : kSilence;
}

void LoudnessMeter::process (const float* left, const float* right, int numFrames) noexcept
{
    if (resetRequested.exchange (false))
        clearMeasurement();

    if (binEnergy.empty())
        return;

    for (int i = 0; i < numFrames; ++i)
    {
        const float zl = filters[0][1].processSample (filters[0][0].processSample (left[i]));
        const float zr = filters[1][1].processSample (filters[1][0].processSample (right[i]));
        hopAccum[0] += (double) zl * zl;
        hopAccum[1] += (double) zr * zr;

        if (++hopFill < hopLength)
            continue;

        // one 100 ms hop finished: channel-summed mean square
        const double hopEnergy = (hopAccum[0] + hopAccum[1]) / hopLength;
        hopAccum.fill (0.0);
        hopFill = 0;
        hopEnergies[(size_t) (hopsWritten % kHopsShortTerm)] = hopEnergy;
        ++hopsWritten;

        if (hopsWritten >= kHopsMomentary)
        {
            double m = 0.0;
            for (int k = 1; k <= kHopsMomentary; ++k)
                m += hopEnergies[(size_t) ((hopsWritten - k) % kHopsShortTerm)];
            m /= kHopsMomentary;
            const float mLufs = energyToLufs (m);
            momentary.store (mLufs);

            // gating block for the integrated measurement (absolute gate -70 LUFS)
            if (mLufs > -70.0f)
            {
                const int bin = std::clamp ((int) ((mLufs + 70.0f) * 10.0f), 0, kBins - 1);
                binEnergy[(size_t) bin] += m;
                ++binCount[(size_t) bin];

                double sum = 0.0;
                uint64_t count = 0;
                for (int b = 0; b < kBins; ++b) { sum += binEnergy[(size_t) b]; count += binCount[(size_t) b]; }
                const float relativeGate = energyToLufs (sum / (double) count) - 10.0f;

                double gatedSum = 0.0;
                uint64_t gatedCount = 0;
                for (int b = 0; b < kBins; ++b)
                {
                    const float binCentre = -70.0f + ((float) b + 0.5f) * 0.1f;
                    if (binCentre > relativeGate) { gatedSum += binEnergy[(size_t) b]; gatedCount += binCount[(size_t) b]; }
                }
                if (gatedCount > 0)
                    integrated.store (energyToLufs (gatedSum / (double) gatedCount));
            }
        }

        if (hopsWritten >= kHopsShortTerm)
        {
            double s = 0.0;
            for (double e : hopEnergies) s += e;
            shortTerm.store (energyToLufs (s / kHopsShortTerm));
        }
    }
}

// ===========================================================================
// TruePeakMeter
// ===========================================================================
static double besselI0 (double x) noexcept
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 50; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1.0e-12 * sum) break;
    }
    return sum;
}

void TruePeakMeter::prepare()
{
    constexpr int N = kOversampling * kTapsPerPhase;
    constexpr double beta = 7.0;
    const double centre = (N - 1) * 0.5;
    const double i0Beta = besselI0 (beta);

    for (int n = 0; n < N; ++n)
    {
        const double t = (n - centre) / kOversampling;
        const double sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (kPi * t) / (kPi * t);
        const double r = (n - centre) / centre;
        const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0Beta;
        phases[(size_t) (n % kOversampling)][(size_t) (n / kOversampling)] = (float) (sinc * w);
    }

    // normalise each phase to unity DC gain
    for (auto& ph : phases)
    {
        double s = 0.0;
        for (float v : ph) s += v;
        for (float& v : ph) v = (float) (v / s);
    }

    for (auto& h : history) h.fill (0.0f);
    historyPos = 0;
    runningMax = 0.0f;
    maxPeak.store (0.0f);
    blockPeak.store (0.0f);
}

void TruePeakMeter::process (const float* const* channels, int numChannels, int numFrames) noexcept
{
    if (resetRequested.exchange (false))
    {
        runningMax = 0.0f;
        maxPeak.store (0.0f);
        for (auto& h : history) h.fill (0.0f);
    }

    const int nch = std::min (numChannels, 2);
    float blockMax = 0.0f;

    for (int i = 0; i < numFrames; ++i)
    {
        historyPos = (historyPos + kTapsPerPhase - 1) % kTapsPerPhase;
        for (int c = 0; c < nch; ++c)
        {
            auto& h = history[(size_t) c];
            const float x = channels[c][i];
            h[(size_t) historyPos] = x;
            h[(size_t) historyPos + kTapsPerPhase] = x;
            blockMax = std::max (blockMax, std::abs (x));

            const float* recent = h.data() + historyPos; // recent[0] = newest sample
            for (const auto& ph : phases)
            {
                float acc = 0.0f;
                for (int k = 0; k < kTapsPerPhase; ++k)
                    acc += ph[(size_t) k] * recent[k];
                blockMax = std::max (blockMax, std::abs (acc));
            }
        }
    }

    runningMax = std::max (runningMax, blockMax);
    blockPeak.store (blockMax);
    maxPeak.store (runningMax);
}

// ===========================================================================
// CorrelationMeter
// ===========================================================================
void CorrelationMeter::prepare (double sampleRate)
{
    coef = std::exp (-1.0 / (0.3 * sampleRate));
    reset();
}

void CorrelationMeter::process (const float* left, const float* right, int numFrames) noexcept
{
    for (int i = 0; i < numFrames; ++i)
    {
        const double l = left[i], r = right[i];
        lr = coef * lr + (1.0 - coef) * l * r;
        ll = coef * ll + (1.0 - coef) * l * l;
        rr = coef * rr + (1.0 - coef) * r * r;
    }

    const double denom = std::sqrt (ll * rr);
    correlation.store (denom > 1.0e-12 ? (float) std::clamp (lr / denom, -1.0, 1.0) : 0.0f);
}

} // namespace pink
