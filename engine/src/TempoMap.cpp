#include "pink/transport/TempoMap.h"

#include <algorithm>
#include <cmath>

namespace pink
{
TempoMap::TempoMap()
{
    tempos.push_back ({ 0.0, 120.0 });
    signatures.push_back ({ 0, 4, 4, 0.0 });
    rebuildTempoCache();
    rebuildSignatureCache();
}

double TempoMap::clampBpm (double bpm) noexcept
{
    return std::clamp (bpm, kMinBpm, kMaxBpm);
}

bool TempoMap::isValidDenominator (int d) noexcept
{
    return d == 1 || d == 2 || d == 4 || d == 8 || d == 16 || d == 32;
}

void TempoMap::setConstantTempo (double bpm)
{
    tempos.clear();
    tempos.push_back ({ 0.0, clampBpm (bpm) });
    rebuildTempoCache();
}

void TempoMap::setTempoAt (double ppq, double bpm)
{
    ppq = std::max (0.0, ppq);
    bpm = clampBpm (bpm);

    auto it = std::find_if (tempos.begin(), tempos.end(),
                            [ppq] (const TempoPoint& p) { return std::abs (p.ppq - ppq) < 1.0e-9; });
    if (it != tempos.end())
        it->bpm = bpm;
    else
        tempos.push_back ({ ppq, bpm });

    std::sort (tempos.begin(), tempos.end(), [] (const TempoPoint& a, const TempoPoint& b) { return a.ppq < b.ppq; });
    rebuildTempoCache();
}

void TempoMap::removeTempoChangesAfter (double ppq)
{
    std::erase_if (tempos, [ppq] (const TempoPoint& p) { return p.ppq > ppq && p.ppq > 0.0; });
    rebuildTempoCache();
}

void TempoMap::rebuildTempoCache()
{
    if (tempos.empty() || tempos.front().ppq > 0.0)
        tempos.insert (tempos.begin(), { 0.0, tempos.empty() ? 120.0 : tempos.front().bpm });

    secondsAtPoint.resize (tempos.size());
    secondsAtPoint[0] = 0.0;
    for (size_t i = 1; i < tempos.size(); ++i)
        secondsAtPoint[i] = secondsAtPoint[i - 1] + (tempos[i].ppq - tempos[i - 1].ppq) * 60.0 / tempos[i - 1].bpm;
}

double TempoMap::ppqToSeconds (double ppq) const noexcept
{
    // index of last point with point.ppq <= ppq
    auto it = std::upper_bound (tempos.begin(), tempos.end(), ppq,
                                [] (double value, const TempoPoint& p) { return value < p.ppq; });
    const size_t i = it == tempos.begin() ? 0 : (size_t) (it - tempos.begin()) - 1;
    return secondsAtPoint[i] + (ppq - tempos[i].ppq) * 60.0 / tempos[i].bpm;
}

double TempoMap::secondsToPpq (double seconds) const noexcept
{
    auto it = std::upper_bound (secondsAtPoint.begin(), secondsAtPoint.end(), seconds);
    const size_t i = it == secondsAtPoint.begin() ? 0 : (size_t) (it - secondsAtPoint.begin()) - 1;
    return tempos[i].ppq + (seconds - secondsAtPoint[i]) * tempos[i].bpm / 60.0;
}

double TempoMap::bpmAt (double ppq) const noexcept
{
    auto it = std::upper_bound (tempos.begin(), tempos.end(), ppq,
                                [] (double value, const TempoPoint& p) { return value < p.ppq; });
    const size_t i = it == tempos.begin() ? 0 : (size_t) (it - tempos.begin()) - 1;
    return tempos[i].bpm;
}

// ---------------------------------------------------------------------------
void TempoMap::setTimeSignature (int numerator, int denominator)
{
    signatures.clear();
    signatures.push_back ({ 0, std::clamp (numerator, 1, 32), isValidDenominator (denominator) ? denominator : 4, 0.0 });
    rebuildSignatureCache();
}

void TempoMap::setTimeSignatureAt (int bar, int numerator, int denominator)
{
    bar = std::max (0, bar);
    numerator = std::clamp (numerator, 1, 32);
    denominator = isValidDenominator (denominator) ? denominator : 4;

    auto it = std::find_if (signatures.begin(), signatures.end(), [bar] (const TimeSignaturePoint& s) { return s.bar == bar; });
    if (it != signatures.end())
    {
        it->numerator = numerator;
        it->denominator = denominator;
    }
    else
    {
        signatures.push_back ({ bar, numerator, denominator, 0.0 });
    }

    std::sort (signatures.begin(), signatures.end(), [] (const auto& a, const auto& b) { return a.bar < b.bar; });
    rebuildSignatureCache();
}

void TempoMap::rebuildSignatureCache()
{
    if (signatures.empty() || signatures.front().bar != 0)
        signatures.insert (signatures.begin(), { 0, 4, 4, 0.0 });

    signatures[0].ppq = 0.0;
    for (size_t i = 1; i < signatures.size(); ++i)
    {
        const auto& prev = signatures[i - 1];
        const double barLen = prev.numerator * 4.0 / prev.denominator;
        signatures[i].ppq = prev.ppq + (signatures[i].bar - prev.bar) * barLen;
    }
}

static size_t signatureIndexForPpq (const std::vector<TimeSignaturePoint>& sigs, double ppq) noexcept
{
    auto it = std::upper_bound (sigs.begin(), sigs.end(), ppq,
                                [] (double value, const TimeSignaturePoint& s) { return value < s.ppq; });
    return it == sigs.begin() ? 0 : (size_t) (it - sigs.begin()) - 1;
}

BarBeatTick TempoMap::ppqToBarBeatTick (double ppq) const noexcept
{
    ppq = std::max (0.0, ppq);
    const auto& sig = signatures[signatureIndexForPpq (signatures, ppq)];
    const double beatLen = 4.0 / sig.denominator;
    const double barLen = sig.numerator * beatLen;
    const double rel = ppq - sig.ppq;

    BarBeatTick r;
    r.numerator = sig.numerator;
    r.denominator = sig.denominator;
    const double barsF = rel / barLen + 1.0e-9;
    const int barsIn = (int) std::floor (barsF);
    r.bar = sig.bar + barsIn;
    const double inBar = std::max (0.0, rel - barsIn * barLen);
    const double beatsF = inBar / beatLen + 1.0e-9;
    r.beat = std::min (sig.numerator - 1, (int) std::floor (beatsF));
    const double inBeat = std::max (0.0, inBar - r.beat * beatLen);
    r.tick = std::clamp ((int) std::floor (inBeat / beatLen * 960.0 + 1.0e-6), 0, 959);
    return r;
}

double TempoMap::barToPpq (int bar) const noexcept
{
    bar = std::max (0, bar);
    auto it = std::upper_bound (signatures.begin(), signatures.end(), bar,
                                [] (int value, const TimeSignaturePoint& s) { return value < s.bar; });
    const auto& sig = *(it == signatures.begin() ? signatures.begin() : it - 1);
    return sig.ppq + (bar - sig.bar) * sig.numerator * 4.0 / sig.denominator;
}

double TempoMap::beatLengthAt (double ppq) const noexcept
{
    return 4.0 / signatures[signatureIndexForPpq (signatures, ppq)].denominator;
}

bool TempoMap::isBarStart (double ppq) const noexcept
{
    const auto& sig = signatures[signatureIndexForPpq (signatures, ppq + 1.0e-9)];
    const double barLen = sig.numerator * 4.0 / sig.denominator;
    const double rel = (ppq - sig.ppq) / barLen;
    return std::abs (rel - std::round (rel)) * barLen < 1.0e-6;
}

double TempoMap::nextBeatAtOrAfter (double ppq) const noexcept
{
    const size_t idx = signatureIndexForPpq (signatures, ppq + 1.0e-9);
    const auto& sig = signatures[idx];
    const double beatLen = 4.0 / sig.denominator;
    const double k = std::ceil ((ppq - sig.ppq) / beatLen - 1.0e-9);
    const double beat = sig.ppq + k * beatLen;
    if (idx + 1 < signatures.size() && signatures[idx + 1].ppq < beat)
        return signatures[idx + 1].ppq;
    return beat;
}

} // namespace pink
