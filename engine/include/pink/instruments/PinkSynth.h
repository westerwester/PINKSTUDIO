// PINKSTUDIO - PinkSynth core: polyphonic subtractive synthesizer.
// First real version (Phase 2): 2 band-limited (polyBLEP) oscillators with unison/detune,
// sub oscillator, noise, TPT state-variable filter (LP/HP/BP) with its own envelope,
// amp envelope, LFO to pitch/cutoff, velocity, pitch bend, mod wheel, sustain pedal.
// Wavetables and the modulation matrix are planned for Phase 10 (see docs/ROADMAP.md).
#pragma once

#include "pink/dsp/Processor.h"

#include <array>

namespace pink
{
/** Linear-attack, exponential decay/release envelope. Real-time safe. */
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate (double sr) noexcept { sampleRate = sr; }
    void setParameters (float attackMs, float decayMs, float sustainLevel, float releaseMs) noexcept;
    void noteOn() noexcept;
    void noteOff() noexcept;
    void kill() noexcept { stage = Stage::Idle; level = 0.0f; }
    float next() noexcept;
    bool isActive() const noexcept { return stage != Stage::Idle; }
    bool isReleasing() const noexcept { return stage == Stage::Release; }
    float getLevel() const noexcept { return level; }

private:
    double sampleRate = 48000.0;
    Stage stage = Stage::Idle;
    float level = 0.0f, attackInc = 1.0f, decayCoef = 0.0f, sustain = 1.0f, releaseCoef = 0.0f;
};

/** Topology-preserving-transform state-variable filter (Zavalishin / Simper). */
class StateVariableFilter
{
public:
    enum class Mode { LowPass, HighPass, BandPass };

    void setCoefficients (double sampleRate, double cutoffHz, float resonance) noexcept;
    void reset() noexcept { ic1 = ic2 = 0.0f; }

    inline float process (float v0, Mode mode) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        switch (mode)
        {
            case Mode::HighPass: return v0 - k * v1 - v2;
            case Mode::BandPass: return v1;
            case Mode::LowPass:
            default:             return v2;
        }
    }

private:
    float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k = 2.0f, ic1 = 0.0f, ic2 = 0.0f;
};

class PinkSynth final : public Instrument
{
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kMaxUnison = 7;

    enum class Wave { Saw = 0, Square, Triangle, Sine };

    PinkSynth();
    std::string getName() const override { return "PinkSynth"; }
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void render (AudioBuffer& out, int numFrames, const MidiEventList& events, const ProcessContext& context) noexcept override;
    int getActiveVoiceCount() const noexcept override;

    /** Built-in starting presets used by the demo. */
    void loadPreset (const std::string& name);

private:
    struct Voice
    {
        bool active = false;
        bool held = false;        // key physically down
        bool sustained = false;   // released while the sustain pedal was down
        int key = 0;
        int channel = 0;
        float velocityGain = 1.0f;
        uint64_t age = 0;
        std::array<double, kMaxUnison> phase1 {}, phase2 {};
        double subPhase = 0.0;
        Envelope amp, filterEnv;
        std::array<StateVariableFilter, 2> filters;
        int filterCounter = 0;
    };

    void handleEvent (const MidiEvent& e) noexcept;
    void startVoice (int key, int velocity, int channel) noexcept;
    void releaseKey (int key, int channel) noexcept;
    void renderVoices (float* left, float* right, int numFrames) noexcept;
    void refreshBlockParameters() noexcept;

    std::array<Voice, kMaxVoices> voices {};
    double sampleRate = 48000.0;
    uint64_t ageCounter = 0;
    FastRandom random;
    float pitchBendSemis = 0.0f, modWheel = 0.0f;
    bool sustainPedal = false;
    double lfoPhase = 0.0;

    // cached per-block parameter values
    struct Cache
    {
        Wave wave1 = Wave::Saw, wave2 = Wave::Saw;
        float level1 = 0.8f, level2 = 0.0f, semi2 = 0.0f, fine2 = 0.0f, subLevel = 0.0f, noiseLevel = 0.0f;
        int unison = 1, maxVoices = kMaxVoices;
        float detuneCents = 0.0f, spread = 0.0f;
        StateVariableFilter::Mode filterMode = StateVariableFilter::Mode::LowPass;
        float cutoff = 8000.0f, resonance = 0.2f, envAmountSemis = 0.0f, keyTrack = 0.0f;
        float lfoRate = 5.0f, lfoPitchCents = 0.0f, lfoCutoffSemis = 0.0f;
        float velocitySens = 0.7f, gain = 1.0f, bendRange = 2.0f;
    } cache;

    Parameter *pWave1, *pLevel1, *pWave2, *pLevel2, *pSemi2, *pFine2, *pSub, *pNoise;
    Parameter *pUnison, *pDetune, *pSpread, *pVoices;
    Parameter *pFilterType, *pCutoff, *pResonance, *pFilterEnv, *pKeyTrack;
    Parameter *pAmpA, *pAmpD, *pAmpS, *pAmpR, *pFltA, *pFltD, *pFltS, *pFltR;
    Parameter *pLfoRate, *pLfoPitch, *pLfoCutoff, *pVelocity, *pGain, *pBend;
};

} // namespace pink
