// PINKSTUDIO - mixer model and per-channel runtime objects.
//
// Channel 0 is the master. Insert channels are 1..N (default 128, configurable up to 512).
// Every channel has: insert chain (10 slots), fader, pan (balance for stereo, constant
// power for mono), mute, solo, phase invert, mono switch, up to 8 sends (pre/post fader),
// one output target and an optional sidechain source. Routing is validated so it can never
// form a cycle; the audio thread processes channels in topological order.
#pragma once

#include "pink/core/AudioBuffer.h"
#include "pink/core/RtUtils.h"
#include "pink/dsp/Meters.h"
#include "pink/dsp/Processor.h"

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace pink
{
/** Real-time side of a mixer channel. Parameter members are atomics written by the UI. */
class MixerChannel
{
public:
    static constexpr int kMaxSends = 8;

    AtomicFloat volumeDb { 0.0f };
    AtomicFloat pan { 0.0f };              // -1 (left) .. +1 (right)
    std::atomic<bool> muted { false };
    std::atomic<bool> phaseInvert { false };
    std::atomic<bool> mono { false };
    std::array<AtomicFloat, kMaxSends> sendLevelDb {};

    LevelMeter meter;

    // ---- audio-thread state ----
    AudioBuffer buffer;
    float lastGainL = 1.0f, lastGainR = 1.0f;
    std::array<float, kMaxSends> lastSendGain {};
    uint64_t enginePreparedGeneration = 0; // message-thread bookkeeping

    void prepare (double sampleRate, int maxBlockSize)
    {
        buffer.setSize (2, maxBlockSize);
        meter.prepare (sampleRate);
        lastGainL = lastGainR = 1.0f;
        lastSendGain.fill (0.0f);
    }

    /** Target fader gains for the current parameter values (exposed for tests). */
    static void computePanGains (float panValue, bool isMono, float& gl, float& gr) noexcept;
};

struct SendModel
{
    int target = -1;
    bool preFader = false;
};

struct MixerChannelModel
{
    static constexpr int kMaxInserts = 10;

    std::string name;
    uint32_t colour = 0xFFFF6EB4;
    std::shared_ptr<MixerChannel> rt = std::make_shared<MixerChannel>();
    std::vector<std::shared_ptr<Processor>> inserts;
    std::vector<SendModel> sends;          // index == send slot (level in rt->sendLevelDb)
    int output = 0;                        // 0 = master, -1 = none (master uses -1)
    int sidechainSource = -1;              // channel whose post-fader output feeds sidechain inputs
    bool solo = false;
};

class MixerModel
{
public:
    static constexpr int kDefaultInsertChannels = 128;
    static constexpr int kMaxInsertChannels = 512;
    static constexpr int kMaster = 0;

    MixerModel() { setNumInsertChannels (kDefaultInsertChannels); }

    /** Resizes the mixer. Routing to removed channels is cleared. */
    void setNumInsertChannels (int count);
    int getNumChannels() const noexcept { return (int) channels.size(); }   // including master

    MixerChannelModel& channel (int index) { return channels.at ((size_t) index); }
    const MixerChannelModel& channel (int index) const { return channels.at ((size_t) index); }

    /** True if routing from -> to (output, send or sidechain) would create a feedback loop. */
    bool wouldCreateCycle (int from, int to) const;

    bool setOutput (int channelIndex, int target);
    /** Returns the send slot or -1 if not possible (cycle, invalid, slots full). */
    int addSend (int channelIndex, int target, float levelDb, bool preFader);
    bool removeSend (int channelIndex, int slot);
    bool setSidechainSource (int channelIndex, int sourceChannel);
    bool addInsert (int channelIndex, std::shared_ptr<Processor> processor);

    /** All direct downstream connections of a channel (output + sends + sidechain consumers). */
    std::vector<int> getDownstream (int channelIndex) const;

private:
    bool isValid (int index) const noexcept { return index >= 0 && index < (int) channels.size(); }
    std::vector<MixerChannelModel> channels;
};

} // namespace pink
