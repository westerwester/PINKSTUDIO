#include "pink/model/Project.h"

#include <algorithm>
#include <cmath>

namespace pink
{
void MidiClip::finalise()
{
    std::stable_sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    maxNoteLength = 0.0;
    for (const auto& n : notes)
        maxNoteLength = std::max (maxNoteLength, n.length);
}

Track& Project::addTrack (std::string trackName, TrackType type, int mixerChannel)
{
    Track t;
    t.id = nextTrackId++;
    t.name = std::move (trackName);
    t.type = type;
    t.mixerChannel = std::clamp (mixerChannel, 0, mixer.getNumChannels() - 1);
    tracks.push_back (std::move (t));
    return tracks.back();
}

double Project::getSongEndPpq() const
{
    double end = 0.0;
    for (const auto& t : tracks)
    {
        for (const auto& c : t.midiClips) end = std::max (end, c->start + c->length);
        for (const auto& c : t.audioClips) end = std::max (end, c->start + c->length);
    }

    // round up to a whole bar (at least one)
    const auto bbt = tempo.ppqToBarBeatTick (std::max (0.0, end - 1.0e-6));
    const int bars = std::max (1, bbt.bar + 1);
    return tempo.barToPpq (bars);
}

} // namespace pink
