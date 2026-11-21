#include "pink/instruments/PinkDrum.h"

#include <algorithm>
#include <cmath>

namespace pink
{
namespace
{
inline float decayCoef (double seconds, double sampleRate) noexcept
{
    return (float) std::exp (-1.0 / (std::max (seconds, 1.0e-4) * sampleRate));
}

constexpr double kMetalFreqs[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 }; // classic 808 hat oscillators
} // namespace

PinkDrum::PinkDrum()
{
    const float defaultLevel[kNumPieces] = { 0.0f, -3.0f, -4.0f, -9.0f, -11.0f, -2.0f };
    const float defaultPan[kNumPieces]   = { 0.0f, 0.0f, -0.1f, 0.2f, 0.25f, 0.0f };

    for (int p = 0; p < kNumPieces; ++p)
    {
        const std::string id = std::string (pieceName (p));
        auto& pp = pieceParams[(size_t) p];
        pp.level = &params.add (id + "_level", id + " Level", -60.0f, 6.0f, defaultLevel[p], "dB");
        pp.tune  = &params.add (id + "_tune", id + " Tune", -24.0f, 24.0f, 0.0f, "st");
        pp.decay = &params.add (id + "_decay", id + " Decay", 0.1f, 4.0f, 1.0f, "x", Parameter::Scale::Logarithmic);
        pp.pan   = &params.add (id + "_pan", id + " Pan", -1.0f, 1.0f, defaultPan[p]);
    }

    drive  = &params.add ("drive", "Kick/808 Drive", 1.0f, 8.0f, 1.6f, "x");
    master = &params.add ("master", "Master", -36.0f, 6.0f, -3.0f, "dB");
}

const char* PinkDrum::pieceName (int piece) noexcept
{
    static const char* names[kNumPieces] = { "kick", "snare", "clap", "hat_closed", "hat_open", "bass808" };
    return (piece >= 0 && piece < kNumPieces) ? names[piece] : "unknown";
}

int PinkDrum::pieceForNote (int note) noexcept
{
    switch (note)
    {
        case 35: return Bass808;
        case 36: return Kick;
        case 38: case 40: return Snare;
        case 39: return Clap;
        case 42: case 44: return ClosedHat;
        case 46: return OpenHat;
        default: return -1;
    }
}

void PinkDrum::prepare (double sr, int)
{
    sampleRate = sr;
    reset();
}

void PinkDrum::reset() noexcept
{
    for (auto& v : state)
    {
        v.active = false;
        v.filterA.reset();
        v.filterB.reset();
    }
}

int PinkDrum::getActiveVoiceCount() const noexcept
{
    int n = 0;
    for (const auto& v : state)
        n += v.active ? 1 : 0;
    return n;
}

void PinkDrum::trigger (int piece, float velocity) noexcept
{
    auto& v = state[(size_t) piece];
    const auto& pp = pieceParams[(size_t) piece];
    const double decayScale = pp.decay->get();
    const double tune = std::pow (2.0, pp.tune->get() / 12.0);

    v.active = true;
    v.velocity = velocity;
    v.age = 0;
    v.phase = v.phase2 = 0.0;
    v.chokeGain = 1.0f;
    v.choking = false;
    v.ampEnv = 1.0f;
    v.auxEnv = 1.0f;
    v.filterA.reset();
    v.filterB.reset();

    switch (piece)
    {
        case Kick:
            v.ampCoef = decayCoef (0.34 * decayScale, sampleRate);
            v.auxCoef = decayCoef (0.028, sampleRate);
            break;
        case Bass808:
            v.ampCoef = decayCoef (0.85 * decayScale, sampleRate);
            v.auxCoef = decayCoef (0.045, sampleRate);
            break;
        case Snare:
            v.ampCoef = decayCoef (0.055 * decayScale, sampleRate);   // body
            v.auxCoef = decayCoef (0.150 * decayScale, sampleRate);   // noise
            v.filterA.setCoefficients (BiquadCoefficients::design (FilterType::HighPass, sampleRate, 1400.0 * tune, 0.7, 0.0));
            break;
        case Clap:
            v.ampCoef = decayCoef (0.006, sampleRate);                // bursts
            v.auxCoef = decayCoef (0.170 * decayScale, sampleRate);   // tail
            v.filterA.setCoefficients (BiquadCoefficients::design (FilterType::BandPass, sampleRate, 1150.0 * tune, 1.3, 0.0));
            break;
        case ClosedHat:
        case OpenHat:
        {
            v.ampCoef = decayCoef ((piece == ClosedHat ? 0.042 : 0.33) * decayScale, sampleRate);
            v.filterA.setCoefficients (BiquadCoefficients::design (FilterType::HighPass, sampleRate, 7200.0 * tune, 0.8, 0.0));
            v.filterB.setCoefficients (BiquadCoefficients::design (FilterType::Peak, sampleRate, 10500.0 * tune, 1.0, 6.0));
            for (auto& m : v.metal) m = random.nextUnipolar();
            if (piece == ClosedHat && state[OpenHat].active)
                state[OpenHat].choking = true; // choke group
            break;
        }
        default: break;
    }
}

float PinkDrum::renderPiece (int piece, VoiceState& v) noexcept
{
    const auto& pp = pieceParams[(size_t) piece];
    const double tune = std::pow (2.0, pp.tune->get() / 12.0); // cheap enough: pieces are few
    float out = 0.0f;

    switch (piece)
    {
        case Kick:
        case Bass808:
        {
            const bool is808 = piece == Bass808;
            const double base = (is808 ? 49.0 : 52.0) * tune;
            const double freq = base * (1.0 + (is808 ? 0.7 : 3.2) * v.auxEnv);
            v.phase += freq / sampleRate;
            if (v.phase >= 1.0) v.phase -= 1.0;
            float s = (float) std::sin (kTwoPi * v.phase);
            const float d = drive->get();
            s = std::tanh (d * s) / std::tanh (d);
            out = s * v.ampEnv;
            if (! is808 && v.age < (int) (0.004 * sampleRate))
                out += 0.35f * random.nextBipolar() * (1.0f - (float) v.age / (float) (0.004 * sampleRate));
            v.ampEnv *= v.ampCoef;
            v.auxEnv *= v.auxCoef;
            break;
        }
        case Snare:
        {
            v.phase += 185.0 * tune / sampleRate;
            v.phase2 += 330.0 * tune / sampleRate;
            if (v.phase >= 1.0) v.phase -= 1.0;
            if (v.phase2 >= 1.0) v.phase2 -= 1.0;
            const float body = 0.55f * (float) std::sin (kTwoPi * v.phase) + 0.3f * (float) std::sin (kTwoPi * v.phase2);
            const float noise = v.filterA.processSample (random.nextBipolar());
            out = body * v.ampEnv * 0.9f + noise * v.auxEnv * 0.75f;
            v.ampEnv *= v.ampCoef;
            v.auxEnv *= v.auxCoef;
            if (v.auxEnv < 1.0e-4f && v.ampEnv < 1.0e-4f) v.active = false;
            break;
        }
        case Clap:
        {
            const double t = (double) v.age / sampleRate;
            const int burst = (int) (t / 0.0105);
            float env;
            if (burst < 4)
            {
                const double local = t - burst * 0.0105;
                env = (float) std::exp (-local / 0.0045);
            }
            else
            {
                env = 0.8f * v.auxEnv;
                v.auxEnv *= v.auxCoef;
            }
            out = 1.6f * v.filterA.processSample (random.nextBipolar()) * env;
            if (burst >= 4 && v.auxEnv < 1.0e-4f) v.active = false;
            break;
        }
        case ClosedHat:
        case OpenHat:
        {
            float metal = 0.0f;
            for (int k = 0; k < 6; ++k)
            {
                double& ph = v.metal[(size_t) k];
                ph += kMetalFreqs[k] * 1.62 * tune / sampleRate;
                if (ph >= 1.0) ph -= 1.0;
                metal += ph < 0.5 ? 1.0f : -1.0f;
            }
            float s = metal * (1.0f / 6.0f) + 0.35f * random.nextBipolar();
            s = v.filterB.processSample (v.filterA.processSample (s));
            if (v.choking)
                v.chokeGain *= 0.995f;
            out = s * v.ampEnv * v.chokeGain;
            v.ampEnv *= v.ampCoef;
            if (v.chokeGain < 1.0e-3f) v.active = false;
            break;
        }
        default: break;
    }

    if (v.ampEnv < 1.0e-4f && (piece == Kick || piece == Bass808 || piece == ClosedHat || piece == OpenHat))
        v.active = false;

    ++v.age;
    return out * v.velocity;
}

void PinkDrum::renderSegment (float* left, float* right, int numFrames) noexcept
{
    for (int p = 0; p < kNumPieces; ++p)
    {
        auto& v = state[(size_t) p];
        if (! v.active)
            continue;

        const float gl = gainL[(size_t) p], gr = gainR[(size_t) p];
        for (int i = 0; i < numFrames && v.active; ++i)
        {
            const float s = renderPiece (p, v);
            left[i] += s * gl;
            right[i] += s * gr;
        }
    }
}

void PinkDrum::render (AudioBuffer& out, int numFrames, const MidiEventList& events, const ProcessContext&) noexcept
{
    out.clear (0, numFrames);
    if (out.getNumChannels() < 2)
        return;

    const float masterGain = dbToGain (master->get());
    for (int p = 0; p < kNumPieces; ++p)
    {
        const auto& pp = pieceParams[(size_t) p];
        const float g = dbToGain (pp.level->get()) * masterGain;
        const double angle = (pp.pan->get() + 1.0) * kPi * 0.25;
        gainL[(size_t) p] = g * (float) (std::cos (angle) * std::numbers::sqrt2);
        gainR[(size_t) p] = g * (float) (std::sin (angle) * std::numbers::sqrt2);
    }

    float* left = out.getWritePointer (0);
    float* right = out.getWritePointer (1);
    int pos = 0;

    for (const auto& e : events)
    {
        const int offset = std::clamp (e.sampleOffset, pos, numFrames);
        if (offset > pos)
        {
            renderSegment (left + pos, right + pos, offset - pos);
            pos = offset;
        }

        if (e.isNoteOn())
        {
            const int piece = pieceForNote (e.data1);
            if (piece >= 0)
                trigger (piece, (float) e.data2 / 127.0f);
        }
        else if (e.isController() && e.data1 == 120)
        {
            reset();
        }
    }

    if (pos < numFrames)
        renderSegment (left + pos, right + pos, numFrames - pos);
}

} // namespace pink
