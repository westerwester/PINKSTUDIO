#include "pink/dsp/StockEffects.h"
#include "pink/transport/TempoMap.h"

#include <algorithm>
#include <cmath>

namespace pink
{
namespace
{
const std::vector<std::string>& filterTypeNames()
{
    static const std::vector<std::string> names { "Low Pass", "High Pass", "Band Pass", "Notch",
                                                  "Peak", "Low Shelf", "High Shelf", "All Pass" };
    return names;
}

inline float onePoleCoef (double seconds, double sampleRate) noexcept
{
    return seconds <= 0.0 ? 0.0f : (float) std::exp (-1.0 / (seconds * sampleRate));
}
} // namespace

// ===========================================================================
// ParametricEq
// ===========================================================================
ParametricEq::ParametricEq()
{
    const float defFreq[kNumBands] = { 30, 100, 250, 800, 2500, 6000, 10000, 18000 };
    const FilterType defType[kNumBands] = { FilterType::HighPass, FilterType::LowShelf, FilterType::Peak, FilterType::Peak,
                                            FilterType::Peak, FilterType::Peak, FilterType::HighShelf, FilterType::LowPass };

    for (int b = 0; b < kNumBands; ++b)
    {
        const std::string p = "b" + std::to_string (b + 1) + "_";
        const std::string n = "Band " + std::to_string (b + 1) + " ";
        bands[(size_t) b].on   = &params.addToggle (p + "on", n + "On", false);
        bands[(size_t) b].type = &params.addChoice (p + "type", n + "Type", filterTypeNames(), (int) defType[b]);
        bands[(size_t) b].freq = &params.add (p + "freq", n + "Freq", 20.0f, 20000.0f, defFreq[b], "Hz", Parameter::Scale::Logarithmic);
        bands[(size_t) b].gain = &params.add (p + "gain", n + "Gain", -24.0f, 24.0f, 0.0f, "dB");
        bands[(size_t) b].q    = &params.add (p + "q", n + "Q", 0.1f, 18.0f, 0.707f, {}, Parameter::Scale::Logarithmic);
    }

    outputGain = &params.add ("output", "Output", -24.0f, 24.0f, 0.0f, "dB");
}

void ParametricEq::setBand (int band, bool enabled, FilterType type, float frequency, float gainDb, float q)
{
    auto& b = bands[(size_t) std::clamp (band, 0, kNumBands - 1)];
    b.on->set (enabled ? 1.0f : 0.0f);
    b.type->set ((float) type);
    b.freq->set (frequency);
    b.gain->set (gainDb);
    b.q->set (q);
}

void ParametricEq::prepare (double sr, int)
{
    sampleRate = sr;
    outSmoother.reset (sr, 0.02);
    outSmoother.setCurrentAndTarget (dbToGain (outputGain->get()));
    updateCoefficients (true);
    reset();
}

void ParametricEq::reset() noexcept
{
    for (auto& s : state)
        for (auto& f : s.filters)
            f.reset();
}

void ParametricEq::updateCoefficients (bool force) noexcept
{
    for (int b = 0; b < kNumBands; ++b)
    {
        auto& p = bands[(size_t) b];
        auto& s = state[(size_t) b];
        const float f = p.freq->get(), g = p.gain->get(), q = p.q->get();
        const int t = p.type->getInt();

        if (force || f != s.lastFreq || g != s.lastGain || q != s.lastQ || t != s.lastType)
        {
            const auto c = BiquadCoefficients::design ((FilterType) t, sampleRate, f, q, g);
            for (auto& filter : s.filters)
                filter.setCoefficients (c);
            s.lastFreq = f; s.lastGain = g; s.lastQ = q; s.lastType = t;
        }
    }
}

void ParametricEq::process (AudioBuffer& io, int numFrames, const ProcessContext&) noexcept
{
    updateCoefficients (false);
    const int channels = std::min (2, io.getNumChannels());

    for (int b = 0; b < kNumBands; ++b)
    {
        if (! bands[(size_t) b].on->getBool())
            continue;

        for (int c = 0; c < channels; ++c)
            state[(size_t) b].filters[(size_t) c].process (io.getWritePointer (c), numFrames);
    }

    outSmoother.setTarget (dbToGain (outputGain->get()));
    if (outSmoother.isSmoothing() || outSmoother.getTarget() != 1.0f)
    {
        for (int i = 0; i < numFrames; ++i)
        {
            const float g = outSmoother.next();
            for (int c = 0; c < channels; ++c)
                io.getWritePointer (c)[i] *= g;
        }
    }
}

double ParametricEq::getResponseDb (double frequency) const noexcept
{
    double total = outputGain->get();
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& p = bands[(size_t) b];
        if (! p.on->getBool())
            continue;
        const auto c = BiquadCoefficients::design ((FilterType) p.type->getInt(), sampleRate, p.freq->get(), p.q->get(), p.gain->get());
        total += c.magnitudeDb (frequency, sampleRate);
    }
    return total;
}

// ===========================================================================
// Compressor
// ===========================================================================
Compressor::Compressor()
{
    threshold = &params.add ("threshold", "Threshold", -60.0f, 0.0f, -18.0f, "dB");
    ratio     = &params.add ("ratio", "Ratio", 1.0f, 20.0f, 4.0f, ":1", Parameter::Scale::Logarithmic);
    attack    = &params.add ("attack", "Attack", 0.05f, 200.0f, 10.0f, "ms", Parameter::Scale::Logarithmic);
    release   = &params.add ("release", "Release", 5.0f, 2000.0f, 120.0f, "ms", Parameter::Scale::Logarithmic);
    knee      = &params.add ("knee", "Knee", 0.0f, 24.0f, 6.0f, "dB");
    makeup    = &params.add ("makeup", "Makeup", 0.0f, 24.0f, 0.0f, "dB");
    mix       = &params.add ("mix", "Mix", 0.0f, 100.0f, 100.0f, "%");
    detector  = &params.addChoice ("detector", "Detector", { "Peak", "RMS" }, 0);
    externalSidechain = &params.addToggle ("sidechain", "External Sidechain", false);
}

float Compressor::computeOutputLevelDb (float x, float T, float R, float W) noexcept
{
    if (R <= 1.0f)
        return x;

    const float over = x - T;
    if (W > 0.0f && 2.0f * std::abs (over) <= W)
    {
        const float k = over + W * 0.5f;
        return x + (1.0f / R - 1.0f) * k * k / (2.0f * W);
    }

    if (2.0f * over < -W || (W <= 0.0f && over <= 0.0f))
        return x;

    return T + over / R;
}

void Compressor::prepare (double sr, int)
{
    sampleRate = sr;
    reset();
}

void Compressor::reset() noexcept
{
    smoothedReductionDb = 0.0f;
    rmsState = 0.0f;
    gainReductionDb.store (0.0f);
}

void Compressor::process (AudioBuffer& io, int numFrames, const ProcessContext& context) noexcept
{
    const float T = threshold->get(), R = ratio->get(), W = knee->get();
    const float aA = onePoleCoef (attack->get() * 0.001, sampleRate);
    const float aR = onePoleCoef (release->get() * 0.001, sampleRate);
    const float rmsCoef = onePoleCoef (0.010, sampleRate);
    const float makeupGain = dbToGain (makeup->get());
    const float wet = mix->get() * 0.01f, dry = 1.0f - wet;
    const bool useRms = detector->getInt() == 1;

    const int channels = std::min (2, io.getNumChannels());
    float* l = io.getWritePointer (0);
    float* r = channels > 1 ? io.getWritePointer (1) : l;

    const AudioBuffer* sc = (externalSidechain->getBool() && context.sidechain != nullptr) ? context.sidechain : nullptr;
    const float* scl = sc != nullptr ? sc->getReadPointer (0) : l;
    const float* scr = sc != nullptr ? sc->getReadPointer (std::min (1, sc->getNumChannels() - 1)) : r;

    constexpr float kLn10Over20 = 0.11512925464970229f;
    float maxReduction = 0.0f;

    for (int i = 0; i < numFrames; ++i)
    {
        const float dl = scl[i], dr = scr[i];
        float levelDb;

        if (useRms)
        {
            const float sq = std::max (dl * dl, dr * dr);
            rmsState = rmsCoef * rmsState + (1.0f - rmsCoef) * sq;
            levelDb = rmsState > 1.0e-14f ? 10.0f * std::log10 (rmsState) : -140.0f;
        }
        else
        {
            const float peak = std::max (std::abs (dl), std::abs (dr));
            levelDb = peak > 1.0e-7f ? 20.0f * std::log10 (peak) : -140.0f;
        }

        const float target = levelDb - computeOutputLevelDb (levelDb, T, R, W);
        const float coef = target > smoothedReductionDb ? aA : aR;
        smoothedReductionDb = coef * smoothedReductionDb + (1.0f - coef) * target;
        maxReduction = std::max (maxReduction, smoothedReductionDb);

        const float g = std::exp (-smoothedReductionDb * kLn10Over20) * makeupGain;
        const float gw = dry + wet * g;
        l[i] *= gw;
        if (channels > 1)
            r[i] *= gw;
    }

    gainReductionDb.store (maxReduction);
}

// ===========================================================================
// Limiter
// ===========================================================================
Limiter::Limiter()
{
    inputGain = &params.add ("input", "Input Gain", 0.0f, 24.0f, 0.0f, "dB");
    ceiling   = &params.add ("ceiling", "Ceiling", -12.0f, 0.0f, -1.0f, "dB");
    release   = &params.add ("release", "Release", 1.0f, 1000.0f, 80.0f, "ms", Parameter::Scale::Logarithmic);
}

void Limiter::prepare (double sr, int)
{
    sampleRate = sr;
    lookahead = std::max (1, (int) std::lround (0.0015 * sr));
    for (auto& d : delay)
        d.assign ((size_t) lookahead, 0.0f);
    dequeValue.assign ((size_t) lookahead + 1, 1.0f);
    dequeIndex.assign ((size_t) lookahead + 1, 0);
    boxRing.assign ((size_t) lookahead, 1.0f);
    reset();
}

void Limiter::reset() noexcept
{
    for (auto& d : delay)
        std::fill (d.begin(), d.end(), 0.0f);
    std::fill (boxRing.begin(), boxRing.end(), 1.0f);
    boxSum = (double) lookahead;
    boxPos = 0;
    delayPos = 0;
    dequeHead = 0;
    dequeCount = 0;
    sampleCounter = 0;
    releasedGain = 1.0f;
    gainReductionDb.store (0.0f);
}

void Limiter::process (AudioBuffer& io, int numFrames, const ProcessContext&) noexcept
{
    const float inG = dbToGain (inputGain->get());
    // tiny safety margin so floating-point rounding can never produce an over
    const float ceil = dbToGain (ceiling->get()) * 0.99999f;
    const float relCoef = onePoleCoef (release->get() * 0.001, sampleRate);
    const int channels = std::min (2, io.getNumChannels());
    const int L = lookahead;
    const int cap = L + 1;
    float minGain = 1.0f;

    float* ch0 = io.getWritePointer (0);
    float* ch1 = channels > 1 ? io.getWritePointer (1) : nullptr;

    for (int i = 0; i < numFrames; ++i)
    {
        const float x0 = ch0[i] * inG;
        const float x1 = ch1 != nullptr ? ch1[i] * inG : 0.0f;
        const float peak = std::max (std::abs (x0), std::abs (x1));
        const float required = peak > ceil ? ceil / peak : 1.0f;

        // sliding minimum over the last L required gains
        while (dequeCount > 0 && dequeValue[(size_t) ((dequeHead + dequeCount - 1) % cap)] >= required)
            --dequeCount;
        const int slot = (dequeHead + dequeCount) % cap;
        dequeValue[(size_t) slot] = required;
        dequeIndex[(size_t) slot] = sampleCounter;
        ++dequeCount;
        while (dequeIndex[(size_t) dequeHead] <= sampleCounter - L)
        {
            dequeHead = (dequeHead + 1) % cap;
            --dequeCount;
        }
        const float windowMin = dequeValue[(size_t) dequeHead];

        // moving average of the sliding minimum (length L) => smooth gain that still reaches
        // the required value exactly when the delayed peak sample is output
        boxSum += (double) windowMin - (double) boxRing[(size_t) boxPos];
        boxRing[(size_t) boxPos] = windowMin;
        boxPos = (boxPos + 1) % L;
        if (boxPos == 0) // periodically remove accumulated rounding error
        {
            boxSum = 0.0;
            for (float v : boxRing)
                boxSum += v;
        }
        const float smoothGain = (float) (boxSum / L);

        // release: attack follows instantly, recovery is exponential (never above smoothGain)
        releasedGain = smoothGain < releasedGain ? smoothGain
                                                 : releasedGain + (smoothGain - releasedGain) * (1.0f - relCoef);
        minGain = std::min (minGain, releasedGain);

        // delay the audio by L - 1 samples
        delay[0][(size_t) delayPos] = x0;
        delay[1][(size_t) delayPos] = x1;
        const int readPos = (delayPos + 1) % L;
        delayPos = readPos;

        ch0[i] = delay[0][(size_t) readPos] * releasedGain;
        if (ch1 != nullptr)
            ch1[i] = delay[1][(size_t) readPos] * releasedGain;

        ++sampleCounter;
    }

    gainReductionDb.store (-gainToDb (minGain, -60.0f));
}

// ===========================================================================
// StereoDelay
// ===========================================================================
const std::vector<std::string>& StereoDelay::syncNames()
{
    static const std::vector<std::string> names { "Free (ms)", "1/1", "1/2", "1/4", "1/8", "1/16",
                                                  "1/4 D", "1/8 D", "1/16 D", "1/4 T", "1/8 T", "1/16 T" };
    return names;
}

double StereoDelay::syncToBeats (int index) noexcept
{
    static constexpr double beats[] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25, 1.5, 0.75, 0.375, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    return (index >= 0 && index < (int) std::size (beats)) ? beats[index] : 1.0;
}

StereoDelay::StereoDelay()
{
    sync     = &params.addChoice ("sync", "Sync", syncNames(), 7);
    timeMs   = &params.add ("time", "Time", 1.0f, 2000.0f, 300.0f, "ms", Parameter::Scale::Logarithmic);
    feedback = &params.add ("feedback", "Feedback", 0.0f, 95.0f, 35.0f, "%");
    mix      = &params.add ("mix", "Mix", 0.0f, 100.0f, 30.0f, "%");
    pingPong = &params.addToggle ("pingpong", "Ping Pong", true);
    damping  = &params.add ("damping", "Damping", 0.0f, 100.0f, 35.0f, "%");
}

void StereoDelay::prepare (double sr, int)
{
    sampleRate = sr;
    const size_t size = (size_t) std::ceil (4.0 * sr) + 8;
    for (auto& l : lines)
        l.assign (size, 0.0f);
    delaySmoother.reset (sr, 0.08);
    delaySmoother.setCurrentAndTarget ((float) (0.3 * sr));
    reset();
}

void StereoDelay::reset() noexcept
{
    for (auto& l : lines)
        std::fill (l.begin(), l.end(), 0.0f);
    writePos = 0;
    dampState = { 0.0f, 0.0f };
}

void StereoDelay::process (AudioBuffer& io, int numFrames, const ProcessContext& context) noexcept
{
    if (lines[0].empty())
        return;

    const int size = (int) lines[0].size();
    const int syncIndex = sync->getInt();
    double delaySamples = syncIndex == 0 ? timeMs->get() * 0.001 * sampleRate
                                         : syncToBeats (syncIndex) * 60.0 / std::max (1.0, context.bpm) * sampleRate;
    delaySamples = std::clamp (delaySamples, 1.0, (double) size - 4.0);
    delaySmoother.setTarget ((float) delaySamples);

    const float fb = feedback->get() * 0.01f;
    const float wet = mix->get() * 0.01f, dry = 1.0f - wet;
    const bool pp = pingPong->getBool();
    const double cutoff = 20000.0 * std::pow (0.05, damping->get() * 0.01);
    const float a = (float) std::exp (-kTwoPi * std::min (cutoff, sampleRate * 0.45) / sampleRate);

    const int channels = std::min (2, io.getNumChannels());
    float* l = io.getWritePointer (0);
    float* r = channels > 1 ? io.getWritePointer (1) : l;

    for (int i = 0; i < numFrames; ++i)
    {
        const float d = delaySmoother.next();
        double readPos = (double) writePos - (double) d;
        if (readPos < 0.0) readPos += size;
        const int i0 = (int) readPos;
        const int i1 = (i0 + 1) % size;
        const float frac = (float) (readPos - i0);

        float y[2];
        for (int c = 0; c < 2; ++c)
        {
            const auto& line = lines[(size_t) c];
            y[c] = line[(size_t) i0] + frac * (line[(size_t) i1] - line[(size_t) i0]);
            dampState[(size_t) c] = (1.0f - a) * y[c] + a * dampState[(size_t) c];
        }

        const float inL = l[i], inR = channels > 1 ? r[i] : l[i];
        if (pp)
        {
            lines[0][(size_t) writePos] = 0.5f * (inL + inR) + fb * dampState[1];
            lines[1][(size_t) writePos] = fb * dampState[0];
        }
        else
        {
            lines[0][(size_t) writePos] = inL + fb * dampState[0];
            lines[1][(size_t) writePos] = inR + fb * dampState[1];
        }
        writePos = (writePos + 1) % size;

        l[i] = inL * dry + y[0] * wet;
        if (channels > 1)
            r[i] = inR * dry + y[1] * wet;
    }
}

// ===========================================================================
// Utility
// ===========================================================================
Utility::Utility()
{
    gain    = &params.add ("gain", "Gain", -60.0f, 24.0f, 0.0f, "dB");
    invertL = &params.addToggle ("invert_l", "Invert L", false);
    invertR = &params.addToggle ("invert_r", "Invert R", false);
    mono    = &params.addToggle ("mono", "Mono", false);
    width   = &params.add ("width", "Width", 0.0f, 200.0f, 100.0f, "%");
}

void Utility::prepare (double sr, int)
{
    gainSmoother.reset (sr, 0.02);
    gainSmoother.setCurrentAndTarget (dbToGain (gain->get()));
}

void Utility::process (AudioBuffer& io, int numFrames, const ProcessContext&) noexcept
{
    if (io.getNumChannels() < 2)
        return;

    float* l = io.getWritePointer (0);
    float* r = io.getWritePointer (1);
    const float pl = invertL->getBool() ? -1.0f : 1.0f;
    const float pr = invertR->getBool() ? -1.0f : 1.0f;
    const float w = mono->getBool() ? 0.0f : width->get() * 0.01f;
    gainSmoother.setTarget (dbToGain (gain->get()));

    for (int i = 0; i < numFrames; ++i)
    {
        const float g = gainSmoother.next();
        const float a = l[i] * pl, b = r[i] * pr;
        const float m = 0.5f * (a + b), s = 0.5f * (a - b) * w;
        l[i] = (m + s) * g;
        r[i] = (m - s) * g;
    }
}

} // namespace pink
