#include "TestFramework.h"
#include "TestHelpers.h"

#include "pink/instruments/PinkDrum.h"
#include "pink/instruments/PinkSynth.h"

using namespace pink;
using namespace pinktest;

namespace
{
/** Renders an instrument; events are given with absolute sample positions. */
std::vector<float> renderInstrument (Instrument& inst, int totalFrames, std::vector<MidiEvent> events, int block = 256,
                                     std::vector<float>* rightOut = nullptr)
{
    inst.prepare (kSR, block);
    AudioBuffer buf (2, block);
    MidiEventList list;
    ProcessContext ctx;
    ctx.sampleRate = kSR;
    std::vector<float> out;
    for (int pos = 0; pos < totalFrames; pos += block)
    {
        const int n = std::min (block, totalFrames - pos);
        list.clear();
        for (const auto& e : events)
            if (e.sampleOffset >= pos && e.sampleOffset < pos + n)
            {
                MidiEvent local = e;
                local.sampleOffset -= pos;
                list.add (local);
            }
        inst.render (buf, n, list, ctx);
        out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + n);
        if (rightOut) rightOut->insert (rightOut->end(), buf.getReadPointer (1), buf.getReadPointer (1) + n);
    }
    return out;
}

void makePureSine (PinkSynth& s)
{
    auto& p = s.getParameters();
    p.set ("osc1_wave", 3.0f);   // sine
    p.set ("osc1_level", 1.0f);
    p.set ("cutoff", 20000.0f);
    p.set ("resonance", 0.0f);
    p.set ("amp_attack", 0.0f);
    p.set ("amp_sustain", 1.0f);
}
} // namespace

PINK_TEST (synth_pitch_is_accurate)
{
    PinkSynth s;
    makePureSine (s);
    auto out = renderInstrument (s, (int) kSR, { MidiEvent::noteOn (0, 0, 69, 100) });
    const double f = estimateFrequency (out, kSR, 4800);
    CHECK_NEAR (f, 440.0, 0.1);
    REPORT ("A4 (MIDI 69) measured frequency (Hz)", f);

    PinkSynth bent;
    makePureSine (bent);
    MidiEvent bend { 0, 0xE0, 0x7F, 0x7F }; // max up, range 2 st
    auto outB = renderInstrument (bent, (int) kSR, { bend, MidiEvent::noteOn (0, 0, 69, 100) });
    const double fb = estimateFrequency (outB, kSR, 4800);
    CHECK_NEAR (fb, 440.0 * std::pow (2.0, (8191.0 / 8192.0) * 2.0 / 12.0), 0.2);
    REPORT ("A4 + pitch bend +2 st (Hz, ideal 493.86)", fb);
}

PINK_TEST (synth_polyphony_and_voice_limit)
{
    PinkSynth s;
    std::vector<MidiEvent> ev;
    for (int k = 0; k < 8; ++k) ev.push_back (MidiEvent::noteOn (0, 0, 48 + k * 3, 100));
    renderInstrument (s, 4800, ev);
    CHECK (s.getActiveVoiceCount() == 8);

    PinkSynth limited;
    limited.getParameters().set ("voices", 4.0f);
    renderInstrument (limited, 4800, ev);
    CHECK (limited.getActiveVoiceCount() == 4);

    PinkSynth many;
    std::vector<MidiEvent> lots;
    for (int k = 0; k < 40; ++k) lots.push_back (MidiEvent::noteOn (k * 10, 0, 30 + k, 100)); // more than 16: stealing
    renderInstrument (many, 4800, lots);
    CHECK (many.getActiveVoiceCount() == PinkSynth::kMaxVoices);
}

PINK_TEST (synth_release_reaches_silence)
{
    PinkSynth s;
    s.getParameters().set ("amp_release", 100.0f);
    auto out = renderInstrument (s, (int) kSR, { MidiEvent::noteOn (0, 0, 60, 100), MidiEvent::noteOff (4800, 0, 60) });
    CHECK (peakDb (std::vector<float> (out.begin(), out.begin() + 4800)) > -30.0);
    CHECK (peakDb (out, 24000) < -80.0);
    CHECK (s.getActiveVoiceCount() == 0);
}

PINK_TEST (synth_sustain_pedal_holds_notes)
{
    PinkSynth s;
    s.getParameters().set ("amp_release", 50.0f);
    auto out = renderInstrument (s, (int) kSR, { MidiEvent::controller (0, 0, 64, 127), MidiEvent::noteOn (0, 0, 60, 100),
                                                 MidiEvent::noteOff (2400, 0, 60) });
    CHECK (peakDb (out, 40000) > -30.0);       // still sounding: pedal down
    CHECK (s.getActiveVoiceCount() == 1);
    PinkSynth s2;
    s2.getParameters().set ("amp_release", 50.0f);
    auto out2 = renderInstrument (s2, (int) kSR, { MidiEvent::controller (0, 0, 64, 127), MidiEvent::noteOn (0, 0, 60, 100),
                                                   MidiEvent::noteOff (2400, 0, 60), MidiEvent::controller (9600, 0, 64, 0) });
    CHECK (peakDb (out2, 24000) < -80.0);
}

PINK_TEST (drum_pieces_trigger_and_choke)
{
    for (int note : { 35, 36, 38, 39, 42, 46 })
    {
        PinkDrum d;
        auto out = renderInstrument (d, 9600, { MidiEvent::noteOn (0, 0, note, 120) });
        const double pk = peakDb (out);
        CHECK (pk > -30.0);
        REPORT (std::string ("PinkDrum note ") + std::to_string (note) + " (" + PinkDrum::pieceName (PinkDrum::pieceForNote (note)) + ") peak dBFS", pk);
    }

    PinkDrum none;
    auto silent = renderInstrument (none, 4800, { MidiEvent::noteOn (0, 0, 70, 120) });
    CHECK (peakDb (silent) < -200.0);

    // open hat alone vs. open hat choked by a closed hat 20 ms later
    PinkDrum a, b;
    auto open = renderInstrument (a, 24000, { MidiEvent::noteOn (0, 0, 46, 120) });
    auto choked = renderInstrument (b, 24000, { MidiEvent::noteOn (0, 0, 46, 120), MidiEvent::noteOn (960, 0, 42, 1) });
    const double openTail = rmsDb (open, 9600), chokedTail = rmsDb (choked, 9600);
    CHECK (chokedTail < openTail - 30.0);
    REPORT ("open hat tail energy reduction by choke (dB)", chokedTail - openTail);
}
