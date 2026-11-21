#include "pink/demo/DemoProject.h"

#include "pink/dsp/StockEffects.h"
#include "pink/instruments/PinkDrum.h"
#include "pink/instruments/PinkSynth.h"

namespace pink
{
namespace
{
std::shared_ptr<MidiClip> makeClip (std::string name, double start, double length, uint32_t colour)
{
    auto c = std::make_shared<MidiClip>();
    c->name = std::move (name);
    c->start = start;
    c->length = length;
    c->colour = colour;
    return c;
}

void add (MidiClip& c, double start, double length, int key, int velocity)
{
    c.notes.push_back ({ start, length, (uint8_t) key, (uint8_t) velocity, 0 });
}
} // namespace

void buildDemoProject (Project& p)
{
    p = Project();
    p.name = "Pink Sugar Rush (demo)";
    p.tempo.setConstantTempo (150.0);
    p.tempo.setTimeSignature (4, 4);

    constexpr int bars = 8;
    constexpr double barLen = 4.0;
    const double songLen = bars * barLen;

    // ---- mixer layout ------------------------------------------------------
    auto& mx = p.mixer;
    const char* names[] = { "Master", "Kick", "Drums", "Bass", "Chords", "Lead", "Delay Bus" };
    const uint32_t colours[] = { 0xFFFFFFFF, 0xFFFF4FA3, 0xFFFF7AC2, 0xFFC77DFF, 0xFFFFA6D9, 0xFFFF6EB4, 0xFFB98CFF };
    for (int i = 0; i <= 6; ++i)
    {
        mx.channel (i).name = names[i];
        mx.channel (i).colour = colours[i];
    }

    mx.channel (1).rt->volumeDb = -2.0f;
    mx.channel (2).rt->volumeDb = -4.0f;
    mx.channel (3).rt->volumeDb = -3.0f;
    mx.channel (4).rt->volumeDb = -6.5f;
    mx.channel (5).rt->volumeDb = -8.0f;
    mx.channel (5).rt->pan = 0.15f;
    mx.channel (6).rt->volumeDb = -5.0f;

    auto makeDucker = []
    {
        auto c = std::make_shared<Compressor>();
        auto& ps = c->getParameters();
        ps.set ("threshold", -32.0f); ps.set ("ratio", 6.0f); ps.set ("attack", 1.0f);
        ps.set ("release", 140.0f); ps.set ("knee", 6.0f); ps.set ("sidechain", 1.0f);
        return c;
    };

    // bass and chords duck under the kick (sidechain from channel 1)
    mx.addInsert (3, makeDucker());
    mx.setSidechainSource (3, 1);

    auto chordEq = std::make_shared<ParametricEq>();
    chordEq->setBand (0, true, FilterType::HighPass, 180.0f, 0.0f, 0.707f);
    chordEq->setBand (5, true, FilterType::Peak, 3200.0f, 2.0f, 0.9f);
    mx.addInsert (4, chordEq);
    mx.addInsert (4, makeDucker());
    mx.setSidechainSource (4, 1);

    auto delay = std::make_shared<StereoDelay>();
    delay->getParameters().set ("sync", 7.0f);      // 1/8 dotted
    delay->getParameters().set ("mix", 100.0f);
    delay->getParameters().set ("feedback", 42.0f);
    delay->getParameters().set ("damping", 45.0f);
    mx.addInsert (6, delay);
    mx.addSend (5, 6, -4.0f, false);
    mx.addSend (4, 6, -16.0f, false);

    auto masterEq = std::make_shared<ParametricEq>();
    masterEq->setBand (0, true, FilterType::HighPass, 28.0f, 0.0f, 0.707f);
    masterEq->setBand (6, true, FilterType::HighShelf, 10000.0f, 1.5f, 0.707f);
    mx.addInsert (0, masterEq);
    auto limiter = std::make_shared<Limiter>();
    limiter->getParameters().set ("input", 4.0f);
    limiter->getParameters().set ("ceiling", -1.0f);
    limiter->getParameters().set ("release", 60.0f);
    mx.addInsert (0, limiter);

    // ---- harmony -------------------------------------------------------------
    struct Chord { int root; int tones[4]; };
    const Chord progression[4] = {
        { 38, { 57, 62, 65, 69 } }, // Dm
        { 34, { 58, 62, 65, 70 } }, // Bb
        { 41, { 57, 60, 65, 69 } }, // F
        { 36, { 55, 60, 64, 67 } }, // C
    };

    // ---- kick ------------------------------------------------------------------
    {
        auto& t = p.addTrack ("Kick", TrackType::Instrument, 1);
        t.colour = colours[1];
        t.instrument = std::make_shared<PinkDrum>();
        auto clip = makeClip ("Kick", 0.0, songLen, t.colour);
        for (int b = 0; b < bars; ++b)
        {
            const double o = b * barLen;
            for (double pos : { 0.0, 1.5, 2.5 })
                add (*clip, o + pos, 0.25, 36, 120);
            if (b == bars - 1)
                add (*clip, o + 3.5, 0.25, 36, 100);
        }
        clip->finalise();
        t.midiClips.push_back (clip);
    }

    // ---- drums -----------------------------------------------------------------
    {
        auto& t = p.addTrack ("Drums", TrackType::Instrument, 2);
        t.colour = colours[2];
        t.instrument = std::make_shared<PinkDrum>();
        auto clip = makeClip ("Drums", 0.0, songLen, t.colour);
        for (int b = 0; b < bars; ++b)
        {
            const double o = b * barLen;
            add (*clip, o + 1.0, 0.25, 38, 112);
            add (*clip, o + 3.0, 0.25, 38, 112);
            add (*clip, o + 1.0, 0.25, 39, 96);
            add (*clip, o + 3.0, 0.25, 39, 96);

            const bool roll = (b % 4) == 3;
            for (int h = 0; h < 8; ++h)
            {
                const double pos = h * 0.5;
                if (roll && pos >= 3.0) continue;
                add (*clip, o + pos, 0.1, 42, h % 2 == 0 ? 104 : 76);
            }
            if (roll)
                for (int r = 0; r < 8; ++r)
                    add (*clip, o + 3.0 + r * 0.125, 0.06, 42, 70 + r * 6);
            else if (b % 2 == 0)
                add (*clip, o + 3.5, 0.4, 46, 90);
        }
        clip->finalise();
        t.midiClips.push_back (clip);
    }

    // ---- bass ------------------------------------------------------------------
    {
        auto& t = p.addTrack ("Bass", TrackType::Instrument, 3);
        t.colour = colours[3];
        auto synth = std::make_shared<PinkSynth>();
        synth->loadPreset ("Sub Bass");
        t.instrument = synth;
        auto clip = makeClip ("Bass", 0.0, songLen, t.colour);
        for (int b = 0; b < bars; ++b)
        {
            const double o = b * barLen;
            const int root = progression[(b / 2) % 4].root;
            add (*clip, o + 0.0, 1.4, root, 110);
            add (*clip, o + 1.5, 0.45, root, 96);
            add (*clip, o + 2.0, 1.4, root, 110);
            add (*clip, o + 3.5, 0.45, root + 12, 100);
        }
        clip->finalise();
        t.midiClips.push_back (clip);
    }

    // ---- chords ------------------------------------------------------------------
    {
        auto& t = p.addTrack ("Chords", TrackType::Instrument, 4);
        t.colour = colours[4];
        auto synth = std::make_shared<PinkSynth>();
        synth->loadPreset ("Supersaw Pad");
        t.instrument = synth;
        auto clip = makeClip ("Chords", 0.0, songLen, t.colour);
        for (int b = 0; b < bars; ++b)
        {
            const auto& ch = progression[(b / 2) % 4];
            for (int k : ch.tones)
                add (*clip, b * barLen, barLen - 0.05, k, 92);
        }
        clip->finalise();
        t.midiClips.push_back (clip);
    }

    // ---- lead arp ----------------------------------------------------------------
    {
        auto& t = p.addTrack ("Lead", TrackType::Instrument, 5);
        t.colour = colours[5];
        auto synth = std::make_shared<PinkSynth>();
        synth->loadPreset ("Kawaii Pluck");
        t.instrument = synth;
        auto clip = makeClip ("Lead Arp", 0.0, songLen, t.colour);
        const int pattern[8] = { 0, 1, 2, 3, 2, 1, 2, 3 };
        for (int b = 0; b < bars; ++b)
        {
            const auto& ch = progression[(b / 2) % 4];
            for (int s = 0; s < 16; ++s)
            {
                const int tone = ch.tones[pattern[s % 8]] + 12 + (s >= 12 && (b % 2) == 1 ? 12 : 0);
                add (*clip, b * barLen + s * 0.25, 0.2, tone, s % 4 == 0 ? 112 : 88);
            }
        }
        clip->finalise();
        t.midiClips.push_back (clip);
    }

    p.liveInputTrack = 4; // play the lead pluck from a MIDI keyboard
}

} // namespace pink
