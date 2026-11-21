// PINKSTUDIO - the audio engine. Completely independent from any GUI or device API:
// the app layer (JUCE) only forwards the device callback to processBlock().
//
// Threading contract
//   message thread : prepare(), getProject(), commit(), transport commands, getters
//   audio thread   : processBlock() only
// prepare() must be called while the audio callback is stopped.
// processBlock() never allocates, never locks and never frees memory.
#pragma once

#include "pink/core/MidiEvent.h"
#include "pink/core/SnapshotExchange.h"
#include "pink/core/SpscQueue.h"
#include "pink/dsp/Meters.h"
#include "pink/instruments/Metronome.h"
#include "pink/model/Project.h"

#include <atomic>
#include <memory>

namespace pink
{
struct EngineSnapshot;

struct TransportInfo
{
    bool playing = false;
    double ppq = 0.0;
    double seconds = 0.0;
    double bpm = 120.0;
    BarBeatTick position;
    bool loopEnabled = false;
    double loopStart = 0.0, loopEnd = 16.0;
    bool metronome = false;
};

struct MasterMeterReadings
{
    float peakL = 0, peakR = 0, rmsL = 0, rmsR = 0;      // linear
    float momentaryLufs = -144, shortTermLufs = -144, integratedLufs = -144;
    float truePeakMax = 0, truePeakBlock = 0;             // linear
    float correlation = 0;
};

class AudioEngine
{
public:
    static constexpr int kMaxInputMeters = 16;
    static constexpr int kMaxRangesPerBlock = 32;

    AudioEngine();
    ~AudioEngine();
    AudioEngine (const AudioEngine&) = delete;
    AudioEngine& operator= (const AudioEngine&) = delete;

    // ---- setup -------------------------------------------------------------
    void prepare (double sampleRate, int maxBlockSize);
    double getSampleRate() const noexcept { return sampleRate; }
    int getMaxBlockSize() const noexcept { return maxBlockSize; }

    // ---- model -------------------------------------------------------------
    Project& getProject() noexcept { return project; }
    const Project& getProject() const noexcept { return project; }
    /** Compiles the project into a new render snapshot and hands it to the audio thread. */
    void commit();
    /** Frees snapshots the audio thread no longer uses. Call regularly (e.g. from a UI timer). */
    void collectGarbage();
    /** For tests/diagnostics: channels processed by the current snapshot. */
    int getActiveChannelCount() const noexcept;

    // ---- transport (message thread; applied at the next audio block) --------
    void play();
    void stop();
    void togglePlay();
    void locate (double ppq);
    void setLoop (double startPpq, double endPpq);
    void setLoopEnabled (bool enabled);
    void setMetronomeEnabled (bool enabled);
    void setMetronomeLevelDb (float db);
    void resetLoudnessMeters();
    void panic();

    TransportInfo getTransportInfo() const noexcept;
    MasterMeterReadings getMasterMeters() const noexcept;
    float getCpuLoad() const noexcept { return cpuLoad.load(); }
    float getInputPeak (int channel) const noexcept;
    uint64_t getProcessedBlockCount() const noexcept { return blockCount.load(); }

    // ---- audio thread ----------------------------------------------------------
    /** liveMidi must be sorted by sampleOffset (offsets relative to this call). */
    void processBlock (const float* const* inputs, int numInputs,
                       float* const* outputs, int numOutputs, int numFrames,
                       const MidiEvent* liveMidi = nullptr, int numLiveMidi = 0) noexcept;

private:
    enum class CommandType : uint8_t
    {
        Play, Stop, TogglePlay, Locate, SetLoop, SetLoopEnabled, SetMetronome, SetMetronomeLevel, ResetMeters, Panic
    };

    struct Command
    {
        CommandType type = CommandType::Play;
        double a = 0.0, b = 0.0;
    };

    struct TimeRange
    {
        int offset = 0, length = 0;
        double ppqStart = 0.0, ppqEnd = 0.0, secStart = 0.0;
        bool discontinuity = false;
    };

    struct TransportState
    {
        bool playing = false;
        double ppq = 0.0;              // musical position (derived from samplePos while playing)
        int64_t samplePos = 0;         // integer timeline position: keeps block boundaries exact
        uint64_t tempoVersion = 0;
        double playStartPpq = 0.0;
        bool loopEnabled = false;
        double loopStart = 0.0, loopEnd = 16.0;
        bool metronome = false;
        float metronomeGain = 0.5f;
        bool discontinuity = true;
        bool allNotesOffPending = false;
        bool panicPending = false;
    };

    void push (CommandType type, double a = 0.0, double b = 0.0);
    void handleCommands() noexcept;
    int computeRanges (const TempoMap& tempo, int numFrames) noexcept;
    int64_t ppqToSample (const TempoMap& tempo, double ppq) const noexcept;
    void processChunk (const float* const* inputs, int numInputs, float* const* outputs, int numOutputs,
                       int frameOffset, int numFrames, const MidiEvent* liveMidi, int numLiveMidi) noexcept;
    void prepareObjects();

    Project project;
    SnapshotExchange<EngineSnapshot> exchange;
    SpscQueue<Command, 256> commands;

    double sampleRate = 0.0;
    int maxBlockSize = 0;
    uint64_t generation = 0;
    uint64_t tempoVersion = 1;

    // audio-thread state
    TransportState transport;
    std::array<TimeRange, kMaxRangesPerBlock> ranges {};
    int numRanges = 0;
    Metronome metronome;
    AudioBuffer clickBuffer;
    LoudnessMeter loudness;
    TruePeakMeter truePeak;
    CorrelationMeter correlation;

    // published to the UI
    std::atomic<bool> uiPlaying { false }, uiLoopEnabled { false }, uiMetronome { false };
    std::atomic<double> uiPpq { 0.0 }, uiLoopStart { 0.0 }, uiLoopEnd { 16.0 };
    AtomicFloat cpuLoad { 0.0f };
    std::array<AtomicFloat, kMaxInputMeters> inputPeaks {};
    std::atomic<uint64_t> blockCount { 0 };
    TempoMap uiTempo; // message-thread copy for position formatting

    static_assert (std::atomic<double>::is_always_lock_free, "double atomics must be lock-free");
};

} // namespace pink
