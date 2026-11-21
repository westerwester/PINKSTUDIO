// PINKSTUDIO - project data model (message-thread side).
//
// The model is edited only on the message thread (UI, undo actions, file loading).
// AudioEngine::commit() compiles it into an immutable EngineSnapshot for the audio thread.
// Clip contents are immutable shared objects (copy-on-write): editing a clip means
// replacing its shared_ptr, so snapshots can share clip data without copying.
#pragma once

#include "pink/core/AudioBuffer.h"
#include "pink/core/MidiEvent.h"
#include "pink/dsp/Processor.h"
#include "pink/mixer/Mixer.h"
#include "pink/transport/TempoMap.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pink
{
struct Note
{
    double start = 0.0;     // PPQ, relative to the clip content
    double length = 1.0;    // PPQ
    uint8_t key = 60;
    uint8_t velocity = 100;
    uint8_t channel = 0;
};

struct MidiClip
{
    std::string name;
    double start = 0.0;     // timeline position (PPQ)
    double length = 4.0;    // visible length (PPQ)
    double offset = 0.0;    // content offset (PPQ)
    uint32_t colour = 0xFFFF6EB4;
    std::vector<Note> notes;
    double maxNoteLength = 0.0;

    /** Sorts notes by start and caches maxNoteLength. Call after editing notes. */
    void finalise();
};

struct SampleData
{
    double sampleRate = 48000.0;
    int numChannels = 0;
    int64_t numFrames = 0;
    std::vector<std::vector<float>> channels;
    std::string sourcePath;
};

struct AudioClip
{
    std::string name;
    double start = 0.0;            // timeline position (PPQ)
    double length = 4.0;           // PPQ
    double sourceOffsetSeconds = 0.0;
    float gainDb = 0.0f;
    double fadeInSeconds = 0.003;
    double fadeOutSeconds = 0.003;
    uint32_t colour = 0xFFB98CFF;
    std::shared_ptr<const SampleData> sample;
};

enum class TrackType { Instrument, Audio };

/** Per-track state owned by the audio thread that must survive snapshot swaps. */
struct TrackRuntime
{
    AudioBuffer buffer;                                 // stereo render buffer
    MidiEventList events;                               // events for the current block
    std::array<std::array<uint8_t, 128>, 16> activeNotes {}; // notes we sent but did not end yet
    uint64_t enginePreparedGeneration = 0;              // message-thread bookkeeping

    void prepare (int maxBlockSize) { buffer.setSize (2, maxBlockSize); }
};

struct Track
{
    uint32_t id = 0;
    std::string name;
    TrackType type = TrackType::Instrument;
    int mixerChannel = 1;
    bool muted = false;
    uint32_t colour = 0xFFFF6EB4;

    std::shared_ptr<Instrument> instrument;             // Instrument tracks only
    std::vector<std::shared_ptr<const MidiClip>> midiClips;
    std::vector<std::shared_ptr<const AudioClip>> audioClips;
    std::shared_ptr<TrackRuntime> runtime = std::make_shared<TrackRuntime>();
};

struct Project
{
    std::string name = "Untitled";
    TempoMap tempo;
    std::vector<Track> tracks;
    MixerModel mixer;
    uint32_t nextTrackId = 1;
    /** Track that receives live MIDI input (keyboard/controller), -1 for none. */
    int liveInputTrack = 0;

    Track& addTrack (std::string trackName, TrackType type, int mixerChannel);
    /** End of the last clip in PPQ (at least one bar). */
    double getSongEndPpq() const;
};

} // namespace pink
