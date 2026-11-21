// PINKSTUDIO - PinkDrum core: synthesized drum kit (no samples required).
// Pieces and default MIDI notes (General MIDI layout):
//   35 808 bass   36 kick   38/40 snare   39 clap   42/44 closed hat   46 open hat
// Closed hat chokes the open hat. Sample import / drum sampler mode is planned for Phase 10.
#pragma once

#include "pink/dsp/Biquad.h"
#include "pink/dsp/Processor.h"

#include <array>

namespace pink
{
class PinkDrum final : public Instrument
{
public:
    enum Piece { Kick = 0, Snare, Clap, ClosedHat, OpenHat, Bass808, kNumPieces };

    PinkDrum();
    std::string getName() const override { return "PinkDrum"; }
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void render (AudioBuffer& out, int numFrames, const MidiEventList& events, const ProcessContext& context) noexcept override;
    int getActiveVoiceCount() const noexcept override;

    static int pieceForNote (int note) noexcept;
    static const char* pieceName (int piece) noexcept;

private:
    struct PieceParams { Parameter *level, *tune, *decay, *pan; };
    struct VoiceState
    {
        bool active = false;
        float velocity = 1.0f;
        int age = 0;              // samples since trigger
        double phase = 0.0, phase2 = 0.0;
        std::array<double, 6> metal {};
        float ampEnv = 0.0f, ampCoef = 0.0f;
        float auxEnv = 0.0f, auxCoef = 0.0f;   // pitch env / noise env / tail
        float chokeGain = 1.0f;
        bool choking = false;
        Biquad filterA, filterB;
    };

    void trigger (int piece, float velocity) noexcept;
    float renderPiece (int piece, VoiceState& v) noexcept;
    void renderSegment (float* left, float* right, int numFrames) noexcept;

    std::array<PieceParams, kNumPieces> pieceParams {};
    std::array<VoiceState, kNumPieces> state {};
    std::array<float, kNumPieces> gainL {}, gainR {};
    Parameter *drive = nullptr, *master = nullptr;
    double sampleRate = 48000.0;
    FastRandom random { 0xC0FFEEu };
};

} // namespace pink
