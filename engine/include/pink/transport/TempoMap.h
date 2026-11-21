// PINKSTUDIO - tempo and time-signature map.
// Musical time is measured in PPQ (quarter notes, as double). The map converts between
// PPQ, seconds and bars/beats. Tempo changes are step changes at PPQ positions;
// time-signature changes happen at bar boundaries.
// Read-only methods are real-time safe (binary search, no allocation).
#pragma once

#include <cstdint>
#include <vector>

namespace pink
{
struct TempoPoint
{
    double ppq = 0.0;
    double bpm = 120.0;
};

struct TimeSignaturePoint
{
    int bar = 0;          // zero-based bar index where this signature starts
    int numerator = 4;
    int denominator = 4;
    double ppq = 0.0;     // computed: PPQ position of that bar
};

struct BarBeatTick
{
    int bar = 0;          // zero-based
    int beat = 0;         // zero-based, in units of the signature denominator
    int tick = 0;         // 0..959 (960 PPQN per beat-unit)
    int numerator = 4;
    int denominator = 4;
};

class TempoMap
{
public:
    static constexpr double kMinBpm = 10.0;
    static constexpr double kMaxBpm = 999.0;

    TempoMap();

    // ---- editing (message thread) ----------------------------------------
    void setConstantTempo (double bpm);
    /** Adds or replaces a tempo change at ppq (>= 0). A change at ppq 0 replaces the start tempo. */
    void setTempoAt (double ppq, double bpm);
    void removeTempoChangesAfter (double ppq);

    void setTimeSignature (int numerator, int denominator);
    /** Adds or replaces a time-signature change starting at the given zero-based bar. */
    void setTimeSignatureAt (int bar, int numerator, int denominator);

    // ---- queries (real-time safe) ----------------------------------------
    double ppqToSeconds (double ppq) const noexcept;
    double secondsToPpq (double seconds) const noexcept;
    double bpmAt (double ppq) const noexcept;

    BarBeatTick ppqToBarBeatTick (double ppq) const noexcept;
    double barToPpq (int bar) const noexcept;
    /** Length of one beat (signature denominator unit) in PPQ at the given position. */
    double beatLengthAt (double ppq) const noexcept;
    /** True if ppq lies exactly (within tolerance) on the start of a bar. */
    bool isBarStart (double ppq) const noexcept;
    /** First beat position (signature denominator grid) at or after ppq. */
    double nextBeatAtOrAfter (double ppq) const noexcept;

    const std::vector<TempoPoint>& getTempoPoints() const noexcept { return tempos; }
    const std::vector<TimeSignaturePoint>& getTimeSignatures() const noexcept { return signatures; }

private:
    void rebuildTempoCache();
    void rebuildSignatureCache();
    static double clampBpm (double bpm) noexcept;
    static bool isValidDenominator (int d) noexcept;

    std::vector<TempoPoint> tempos;       // sorted by ppq, tempos[0].ppq == 0
    std::vector<double> secondsAtPoint;   // cached seconds for each tempo point
    std::vector<TimeSignaturePoint> signatures; // sorted by bar, signatures[0].bar == 0
};

} // namespace pink
