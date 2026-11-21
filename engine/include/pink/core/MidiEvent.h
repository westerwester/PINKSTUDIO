// PINKSTUDIO - sample-accurate MIDI 1.0 events and a fixed-capacity event list.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pink
{
struct MidiEvent
{
    int32_t sampleOffset = 0;
    uint8_t status = 0;
    uint8_t data1 = 0;
    uint8_t data2 = 0;

    uint8_t type() const noexcept    { return (uint8_t) (status & 0xF0); }
    uint8_t channel() const noexcept { return (uint8_t) (status & 0x0F); }
    bool isNoteOn() const noexcept   { return type() == 0x90 && data2 > 0; }
    bool isNoteOff() const noexcept  { return type() == 0x80 || (type() == 0x90 && data2 == 0); }
    bool isController() const noexcept { return type() == 0xB0; }
    bool isPitchBend() const noexcept   { return type() == 0xE0; }
    int pitchBendValue() const noexcept { return ((int) data2 << 7) | (int) data1; } // 0..16383, centre 8192

    static MidiEvent noteOn (int offset, int channel, int key, int velocity) noexcept
    {
        return { offset, (uint8_t) (0x90 | (channel & 0x0F)), (uint8_t) (key & 0x7F), (uint8_t) (velocity & 0x7F) };
    }

    static MidiEvent noteOff (int offset, int channel, int key, int velocity = 0) noexcept
    {
        return { offset, (uint8_t) (0x80 | (channel & 0x0F)), (uint8_t) (key & 0x7F), (uint8_t) (velocity & 0x7F) };
    }

    static MidiEvent controller (int offset, int channel, int cc, int value) noexcept
    {
        return { offset, (uint8_t) (0xB0 | (channel & 0x0F)), (uint8_t) (cc & 0x7F), (uint8_t) (value & 0x7F) };
    }

    static MidiEvent allNotesOff (int offset, int channel) noexcept { return controller (offset, channel, 123, 0); }
};

/** Fixed-capacity, always time-ordered event list. add() never allocates. */
class MidiEventList
{
public:
    static constexpr int kCapacity = 2048;

    void clear() noexcept { count = 0; }
    int size() const noexcept { return count; }
    bool isFull() const noexcept { return count >= kCapacity; }
    const MidiEvent& operator[] (int i) const noexcept { return events[(size_t) i]; }
    const MidiEvent* begin() const noexcept { return events.data(); }
    const MidiEvent* end() const noexcept { return events.data() + count; }

    /** Inserts keeping the list sorted by sampleOffset; events with equal offsets keep insertion order. */
    bool add (const MidiEvent& e) noexcept
    {
        if (count >= kCapacity)
            return false;

        int i = count;
        while (i > 0 && events[(size_t) i - 1].sampleOffset > e.sampleOffset)
        {
            events[(size_t) i] = events[(size_t) i - 1];
            --i;
        }

        events[(size_t) i] = e;
        ++count;
        return true;
    }

private:
    std::array<MidiEvent, kCapacity> events {};
    int count = 0;
};

} // namespace pink
