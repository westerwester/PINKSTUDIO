#include "Components.h"

#include "pink/dsp/StockEffects.h"

namespace
{
juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

juce::String formatDb (float linear)
{
    const float db = pink::gainToDb (linear, -144.0f);
    return db <= -143.0f ? juce::String ("-inf") : juce::String (db, 1);
}

juce::String formatLufs (float lufs)
{
    return lufs <= -143.0f ? juce::String ("--.-") : juce::String (lufs, 1);
}

double snap (double ppq, double grid) { return std::floor (ppq / grid + 0.5) * grid; }
} // namespace

// =======================================================================================
// IconButton
// =======================================================================================
IconButton::IconButton (const juce::String& name, Icon i, pinkui::PinkLookAndFeel& lnf)
    : juce::Button (name), icon (i), laf (lnf)
{
    setTooltip (name);
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto& p = laf.palette();
    laf.drawButtonBackground (g, *this, p.panelAlt, over, down);

    auto r = getLocalBounds().toFloat().reduced ((float) getHeight() * 0.3f);
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (s, s);
    const auto colour = getToggleState() ? juce::Colours::white : p.accentHot;
    g.setColour (colour);

    juce::Path path;
    switch (icon)
    {
        case Icon::Play:
            path.addTriangle (r.getX() + s * 0.12f, r.getY(), r.getX() + s * 0.12f, r.getBottom(), r.getRight(), r.getCentreY());
            g.fillPath (path);
            break;
        case Icon::Pause:
            g.fillRoundedRectangle (r.withWidth (s * 0.36f), 2.0f);
            g.fillRoundedRectangle (r.withLeft (r.getRight() - s * 0.36f), 2.0f);
            break;
        case Icon::Stop:
            g.fillRoundedRectangle (r.reduced (s * 0.06f), 3.0f);
            break;
        case Icon::Loop:
        {
            const auto c = r.getCentre();
            const float rad = s * 0.42f;
            path.addCentredArc (c.x, c.y, rad, rad * 0.75f, 0.0f, 0.5f, juce::MathConstants<float>::pi - 0.2f, true);
            path.startNewSubPath (c.x - rad, c.y + 0.5f);
            path.addCentredArc (c.x, c.y, rad, rad * 0.75f, 0.0f, juce::MathConstants<float>::pi + 0.5f,
                                juce::MathConstants<float>::twoPi - 0.2f, true);
            g.strokePath (path, juce::PathStrokeType (s * 0.13f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path heads;
            heads.addTriangle (c.x + rad + s * 0.16f, c.y - s * 0.02f, c.x + rad - s * 0.16f, c.y - s * 0.02f, c.x + rad, c.y + s * 0.2f);
            heads.addTriangle (c.x - rad - s * 0.16f, c.y + s * 0.02f, c.x - rad + s * 0.16f, c.y + s * 0.02f, c.x - rad, c.y - s * 0.2f);
            g.fillPath (heads);
            break;
        }
        case Icon::Metronome:
        {
            path.startNewSubPath (r.getX() + s * 0.3f, r.getY());
            path.lineTo (r.getRight() - s * 0.3f, r.getY());
            path.lineTo (r.getRight(), r.getBottom());
            path.lineTo (r.getX(), r.getBottom());
            path.closeSubPath();
            g.strokePath (path, juce::PathStrokeType (s * 0.11f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
            g.drawLine (r.getCentreX(), r.getBottom() - s * 0.15f, r.getRight() - s * 0.1f, r.getY() + s * 0.1f, s * 0.11f);
            break;
        }
    }
}

// =======================================================================================
// LevelMeter
// =======================================================================================
float LevelMeter::dbToProportion (float db) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
}

void LevelMeter::setLevels (float pl, float pr, float rl, float rr)
{
    const float pk[2] = { pl, pr }, rm[2] = { rl, rr };
    for (int c = 0; c < 2; ++c)
    {
        peak[c] = juce::jmax (pk[c], peak[c] * 0.80f);
        rms[c] = juce::jmax (rm[c], rms[c] * 0.88f);
        if (pk[c] >= hold[c]) { hold[c] = pk[c]; holdCounter[c] = 45; }
        else if (holdCounter[c] > 0) --holdCounter[c];
        else hold[c] *= 0.94f;
    }
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto& p = laf.palette();
    auto area = getLocalBounds().toFloat();
    const float gap = 2.0f;
    const float barW = (area.getWidth() - gap) * 0.5f;

    for (int c = 0; c < 2; ++c)
    {
        auto bar = juce::Rectangle<float> (area.getX() + (float) c * (barW + gap), area.getY(), barW, area.getHeight());
        g.setColour (p.dark ? p.background.darker (0.3f) : p.chrome.withAlpha (0.6f));
        g.fillRoundedRectangle (bar, 3.0f);

        juce::ColourGradient grad (p.meterLow, bar.getX(), bar.getBottom(), p.meterHigh, bar.getX(), bar.getY(), false);
        grad.addColour (1.0 - (6.0 / 66.0) - 0.08, p.meterMid);
        g.setGradientFill (grad);

        const float peakH = bar.getHeight() * dbToProportion (pink::gainToDb (peak[c]));
        const float rmsH = bar.getHeight() * dbToProportion (pink::gainToDb (rms[c]));
        g.setOpacity (0.45f);
        g.fillRoundedRectangle (bar.withTop (bar.getBottom() - peakH), 3.0f);
        g.setOpacity (1.0f);
        g.fillRoundedRectangle (bar.withTop (bar.getBottom() - rmsH), 3.0f);

        const float holdY = bar.getBottom() - bar.getHeight() * dbToProportion (pink::gainToDb (hold[c]));
        if (hold[c] > 1.0e-4f)
        {
            g.setColour (hold[c] >= 1.0f ? p.meterHigh : p.accentHot);
            g.fillRect (bar.getX(), holdY - 1.0f, bar.getWidth(), 2.0f);
        }
    }

    // scale ticks: 0, -6, -12, -24, -48 dBFS
    g.setColour (p.text.withAlpha (0.35f));
    for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
    {
        const float y = area.getBottom() - area.getHeight() * dbToProportion (db);
        g.drawHorizontalLine ((int) y, area.getX(), area.getX() + 3.0f);
        g.drawHorizontalLine ((int) y, area.getRight() - 3.0f, area.getRight());
    }
}

// =======================================================================================
// TransportBar
// =======================================================================================
TransportBar::TransportBar (AudioHost& h, pinkui::PinkLookAndFeel& lnf)
    : host (h), laf (lnf),
      playButton ("Play / Pause (Space)", IconButton::Icon::Play, lnf),
      stopButton ("Stop (second press: back to start)", IconButton::Icon::Stop, lnf),
      loopButton ("Loop", IconButton::Icon::Loop, lnf),
      metronomeButton ("Metronome", IconButton::Icon::Metronome, lnf)
{
    for (auto* b : { &playButton, &stopButton, &loopButton, &metronomeButton })
        addAndMakeVisible (b);

    playButton.onClick = [this] { host.engine().togglePlay(); };
    stopButton.onClick = [this] { host.engine().stop(); };
    loopButton.setClickingTogglesState (true);
    loopButton.onClick = [this] { host.engine().setLoopEnabled (loopButton.getToggleState()); };
    metronomeButton.setClickingTogglesState (true);
    metronomeButton.onClick = [this] { host.engine().setMetronomeEnabled (metronomeButton.getToggleState()); };

    bpmSlider.setSliderStyle (juce::Slider::LinearBar);
    bpmSlider.setRange (40.0, 300.0, 0.1);
    bpmSlider.setTextValueSuffix (" BPM");
    bpmSlider.setNumDecimalPlacesToDisplay (1);
    bpmSlider.setValue (host.engine().getProject().tempo.bpmAt (0.0), juce::dontSendNotification);
    bpmSlider.setTooltip ("Tempo: drag, scroll or double-click to type");
    bpmSlider.onDragStart = [this] { bpmAtDragStart = bpmSlider.getValue(); draggingBpm = true; };
    bpmSlider.onValueChange = [this]
    {
        const double previous = host.engine().getProject().tempo.bpmAt (0.0);
        const double now = bpmSlider.getValue();
        applyBpm (now);
        if (! draggingBpm && std::abs (previous - now) > 1.0e-9)
            host.undo().perform ([this, now] { applyBpm (now); bpmSlider.setValue (now, juce::dontSendNotification); },
                                 [this, previous] { applyBpm (previous); bpmSlider.setValue (previous, juce::dontSendNotification); },
                                 "Change tempo");
    };
    bpmSlider.onDragEnd = [this]
    {
        draggingBpm = false;
        const double before = bpmAtDragStart, after = bpmSlider.getValue();
        if (std::abs (before - after) > 1.0e-9)
            host.undo().perform ([this, after] { applyBpm (after); bpmSlider.setValue (after, juce::dontSendNotification); },
                                 [this, before] { applyBpm (before); bpmSlider.setValue (before, juce::dontSendNotification); },
                                 "Change tempo");
    };
    addAndMakeVisible (bpmSlider);

    for (auto* l : { &positionLabel, &timeLabel, &cpuLabel, &deviceLabel, &bpmLabel })
    {
        addAndMakeVisible (l);
        l->setJustificationType (juce::Justification::centredLeft);
    }
    positionLabel.setFont (font (22.0f, true));
    timeLabel.setFont (font (14.0f));
    cpuLabel.setFont (font (13.0f, true));
    deviceLabel.setFont (font (12.0f));
    bpmLabel.setFont (font (11.0f, true));
    bpmLabel.setText ("TEMPO", juce::dontSendNotification);
    refresh();
}

void TransportBar::applyBpm (double bpm)
{
    auto& project = host.engine().getProject();
    project.tempo.setConstantTempo (bpm);
    host.engine().commit();
}

void TransportBar::paint (juce::Graphics& g)
{
    const auto& p = laf.palette();
    pinkui::drawPanel (g, getLocalBounds().toFloat().reduced (4.0f), p, 14.0f);

    // wordmark
    auto logo = juce::Rectangle<float> (18.0f, 8.0f, 190.0f, (float) getHeight() - 16.0f);
    g.setGradientFill ({ p.accentHot, logo.getX(), logo.getY(), p.accentAlt, logo.getRight(), logo.getBottom(), false });
    g.setFont (font (25.0f, true));
    g.drawText ("PINKSTUDIO", logo.translated (0.0f, -6.0f), juce::Justification::centredLeft);
    g.setColour (p.textDim);
    g.setFont (font (11.0f));
    g.drawText ("v" + juce::String (PINKSTUDIO_VERSION) + "  first build", logo.translated (1.0f, 15.0f), juce::Justification::centredLeft);
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    r.removeFromLeft (200);
    const int b = r.getHeight();
    playButton.setBounds (r.removeFromLeft (b + 12));
    r.removeFromLeft (6);
    stopButton.setBounds (r.removeFromLeft (b + 12));
    r.removeFromLeft (14);
    loopButton.setBounds (r.removeFromLeft (b));
    r.removeFromLeft (6);
    metronomeButton.setBounds (r.removeFromLeft (b));
    r.removeFromLeft (18);

    auto tempo = r.removeFromLeft (130);
    bpmLabel.setBounds (tempo.removeFromTop (14));
    bpmSlider.setBounds (tempo);
    r.removeFromLeft (18);

    auto pos = r.removeFromLeft (200);
    positionLabel.setBounds (pos.removeFromTop (pos.getHeight() * 3 / 5));
    timeLabel.setBounds (pos);

    auto right = r.removeFromRight (juce::jmin (420, r.getWidth()));
    cpuLabel.setBounds (right.removeFromTop (right.getHeight() / 2));
    deviceLabel.setBounds (right);
}

void TransportBar::refresh()
{
    const auto info = host.engine().getTransportInfo();
    playButton.setToggleState (info.playing, juce::dontSendNotification);
    playButton.setIcon (info.playing ? IconButton::Icon::Pause : IconButton::Icon::Play);
    if (! loopButton.isMouseButtonDown())
        loopButton.setToggleState (info.loopEnabled, juce::dontSendNotification);
    if (! metronomeButton.isMouseButtonDown())
        metronomeButton.setToggleState (info.metronome, juce::dontSendNotification);

    positionLabel.setText (juce::String::formatted ("%03d . %d . %03d", info.position.bar + 1, info.position.beat + 1, info.position.tick),
                           juce::dontSendNotification);
    const int ms = (int) std::floor (info.seconds * 1000.0);
    timeLabel.setText (juce::String::formatted ("%02d:%02d.%03d   %d/%d", ms / 60000, (ms / 1000) % 60, ms % 1000,
                                                info.position.numerator, info.position.denominator),
                       juce::dontSendNotification);
    cpuLabel.setText ("CPU " + juce::String (juce::roundToInt (host.engine().getCpuLoad() * 100.0f)) + " %   |   XRUN "
                          + juce::String (host.getXrunCount()),
                      juce::dontSendNotification);
    deviceLabel.setText (host.describeCurrentDevice(), juce::dontSendNotification);
    deviceLabel.setColour (juce::Label::textColourId, laf.palette().textDim);

    if (! bpmSlider.isMouseButtonDown())
        bpmSlider.setValue (host.engine().getProject().tempo.bpmAt (info.ppq), juce::dontSendNotification);
}

// =======================================================================================
// ArrangementView
// =======================================================================================
ArrangementView::ArrangementView (AudioHost& h, pinkui::PinkLookAndFeel& lnf) : host (h), laf (lnf) {}

double ArrangementView::visibleLength() const
{
    const auto info = host.engine().getTransportInfo();
    return juce::jmax (host.engine().getProject().getSongEndPpq(), info.loopEnd) + 4.0;
}

int ArrangementView::laneHeight() const
{
    const int n = juce::jmax (1, (int) host.engine().getProject().tracks.size());
    return juce::jlimit (26, 70, (getHeight() - kRulerHeight - 8) / n);
}

float ArrangementView::ppqToX (double ppq) const
{
    const float w = (float) (getWidth() - kHeaderWidth - 12);
    return (float) kHeaderWidth + (float) (ppq / visibleLength()) * w;
}

double ArrangementView::xToPpq (float x) const
{
    const float w = (float) (getWidth() - kHeaderWidth - 12);
    return juce::jmax (0.0, (double) (x - (float) kHeaderWidth) / w * visibleLength());
}

void ArrangementView::paint (juce::Graphics& g)
{
    const auto& p = laf.palette();
    const auto& project = host.engine().getProject();
    const auto info = host.engine().getTransportInfo();
    pinkui::drawPanel (g, getLocalBounds().toFloat().reduced (4.0f), p, 14.0f);

    const auto area = getLocalBounds().reduced (6);
    const double len = visibleLength();
    const int lane = laneHeight();
    const int top = area.getY() + kRulerHeight;

    // ruler + grid
    g.setFont (font (11.0f, true));
    for (int bar = 0;; ++bar)
    {
        const double ppq = project.tempo.barToPpq (bar);
        if (ppq > len) break;
        const float x = ppqToX (ppq);
        g.setColour (p.outline);
        g.drawVerticalLine ((int) x, (float) area.getY() + 12.0f, (float) area.getBottom());
        g.setColour (p.textDim);
        g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x + 3.0f, (float) area.getY() + 2.0f, 30.0f, 14.0f),
                    juce::Justification::centredLeft);
        const auto bbt = project.tempo.ppqToBarBeatTick (ppq);
        const double beatLen = 4.0 / bbt.denominator;
        g.setColour (p.outline.withAlpha (0.35f));
        for (int b = 1; b < bbt.numerator; ++b)
            g.drawVerticalLine ((int) ppqToX (ppq + b * beatLen), (float) top, (float) area.getBottom());
    }

    // loop region
    {
        const float lx = ppqToX (info.loopStart), rx = ppqToX (info.loopEnd);
        const auto ruler = juce::Rectangle<float> (lx, (float) area.getY() + 16.0f, rx - lx, 8.0f);
        g.setColour (info.loopEnabled ? p.accentAlt : p.textDim.withAlpha (0.5f));
        g.fillRoundedRectangle (ruler, 4.0f);
        if (info.loopEnabled)
        {
            g.setColour (p.accentAlt.withAlpha (0.08f));
            g.fillRect (juce::Rectangle<float> (lx, (float) top, rx - lx, (float) (area.getBottom() - top)));
        }
    }

    // tracks
    for (int i = 0; i < (int) project.tracks.size(); ++i)
    {
        const auto& t = project.tracks[(size_t) i];
        const auto row = juce::Rectangle<int> (area.getX(), top + i * lane, area.getWidth(), lane);
        if (row.getBottom() > area.getBottom() + 2) break;

        // header
        auto header = row.withWidth (kHeaderWidth - 10).reduced (4, 3).toFloat();
        g.setColour (p.panelAlt.withAlpha (p.dark ? 0.6f : 0.9f));
        g.fillRoundedRectangle (header, 8.0f);
        g.setColour (juce::Colour (t.colour));
        g.fillRoundedRectangle (header.withWidth (6.0f), 3.0f);
        auto muteBox = header.removeFromRight (24.0f).reduced (2.0f, juce::jmax (2.0f, (header.getHeight() - 20.0f) * 0.5f));
        g.setColour (t.muted ? p.accentHot : p.chrome);
        g.fillRoundedRectangle (muteBox, 5.0f);
        g.setColour (t.muted ? juce::Colours::white : p.text);
        g.setFont (font (11.0f, true));
        g.drawText ("M", muteBox, juce::Justification::centred);
        g.setColour (t.muted ? p.textDim : p.text);
        g.setFont (font (13.0f, true));
        g.drawText (juce::String (t.name), header.withTrimmedLeft (12.0f), juce::Justification::centredLeft, true);

        // clips
        for (const auto& clip : t.midiClips)
        {
            const float x0 = ppqToX (clip->start), x1 = ppqToX (clip->start + clip->length);
            auto box = juce::Rectangle<float> (x0, (float) row.getY() + 3.0f, x1 - x0, (float) lane - 6.0f);
            const auto colour = juce::Colour (clip->colour).withMultipliedAlpha (t.muted ? 0.35f : 0.9f);
            g.setGradientFill ({ colour.brighter (0.15f), box.getX(), box.getY(), colour.darker (0.1f), box.getX(), box.getBottom(), false });
            g.fillRoundedRectangle (box, 6.0f);

            int lo = 127, hi = 0;
            for (const auto& n : clip->notes) { lo = juce::jmin (lo, (int) n.key); hi = juce::jmax (hi, (int) n.key); }
            const float range = (float) juce::jmax (12, hi - lo + 1);
            const auto inner = box.reduced (3.0f, 5.0f);
            const bool brightClip = juce::Colour (clip->colour).getPerceivedBrightness() > 0.72f;
            const auto ink = brightClip ? p.text : juce::Colours::white;
            g.setColour (ink.withAlpha (t.muted ? 0.4f : 0.85f));
            for (const auto& n : clip->notes)
            {
                const double s = clip->start + n.start - clip->offset;
                if (s < clip->start || s >= clip->start + clip->length) continue;
                const float nx = ppqToX (s), nw = juce::jmax (1.5f, ppqToX (s + n.length) - nx);
                const float ny = inner.getBottom() - ((float) (n.key - lo) + 1.0f) / range * inner.getHeight();
                g.fillRect (nx, ny, nw, juce::jmax (1.5f, inner.getHeight() / range));
            }
            g.setColour (ink);
            g.setFont (font (10.0f, true));
            g.drawText (juce::String (clip->name), box.reduced (6.0f, 2.0f).withHeight (12.0f), juce::Justification::topLeft, true);
        }
    }

    // playhead
    const float px = ppqToX (info.ppq);
    g.setColour (p.accentHot);
    g.fillRect (px - 1.0f, (float) area.getY() + 12.0f, 2.0f, (float) (area.getBottom() - area.getY() - 12));
    juce::Path tri;
    tri.addTriangle (px - 6.0f, (float) area.getY() + 8.0f, px + 6.0f, (float) area.getY() + 8.0f, px, (float) area.getY() + 16.0f);
    g.fillPath (tri);
}

void ArrangementView::mouseDown (const juce::MouseEvent& e)
{
    const auto area = getLocalBounds().reduced (6);
    const int lane = laneHeight();
    const int top = area.getY() + kRulerHeight;
    draggingLoop = false;
    dragStartPpq = -1.0;

    if (e.x < kHeaderWidth && e.y >= top)
    {
        const int index = (e.y - top) / lane;
        auto& tracks = host.engine().getProject().tracks;
        if (index >= 0 && index < (int) tracks.size() && e.x > kHeaderWidth - 44)
        {
            const uint32_t id = tracks[(size_t) index].id;
            auto setMuted = [this, id] (bool m)
            {
                for (auto& t : host.engine().getProject().tracks)
                    if (t.id == id) t.muted = m;
                host.engine().commit();
                repaint();
            };
            const bool newState = ! tracks[(size_t) index].muted;
            host.undo().perform ([setMuted, newState] { setMuted (newState); }, [setMuted, newState] { setMuted (! newState); },
                                 newState ? "Mute track" : "Unmute track");
        }
        return;
    }

    if (e.x < kHeaderWidth)
        return;

    const auto& tempo = host.engine().getProject().tempo;
    if (e.y < top)
    {
        draggingLoop = true;
        const auto bbt = tempo.ppqToBarBeatTick (xToPpq ((float) e.x));
        dragStartPpq = tempo.barToPpq (bbt.bar);
    }
    else
    {
        host.engine().locate (snap (xToPpq ((float) e.x), 1.0));
    }
}

void ArrangementView::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingLoop || dragStartPpq < 0.0 || e.getDistanceFromDragStart() < 4)
        return;

    const auto& tempo = host.engine().getProject().tempo;
    const auto bbt = tempo.ppqToBarBeatTick (xToPpq ((float) e.x));
    const double barStart = tempo.barToPpq (bbt.bar), barEnd = tempo.barToPpq (bbt.bar + 1);
    const double a = juce::jmin (dragStartPpq, barStart);
    const double b = juce::jmax (dragStartPpq + (barEnd - barStart), barEnd);
    host.engine().setLoop (a, b);
    host.engine().setLoopEnabled (true);
    repaint();
}

void ArrangementView::mouseUp (const juce::MouseEvent& e)
{
    if (draggingLoop && e.getDistanceFromDragStart() < 4)
        host.engine().locate (snap (xToPpq ((float) e.x), 1.0));
    draggingLoop = false;
}

// =======================================================================================
// ChannelStrip / MixerPanel
// =======================================================================================
ChannelStrip::ChannelStrip (AudioHost& h, pinkui::PinkLookAndFeel& lnf, int channelIndex)
    : host (h), laf (lnf), index (channelIndex), meter (lnf)
{
    auto& model = host.engine().getProject().mixer.channel (index);

    nameLabel.setText (juce::String (model.name), juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (font (12.0f, true));
    addAndMakeVisible (nameLabel);

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange (-60.0, 12.0, 0.1);
    fader.setSkewFactorFromMidPoint (-12.0);
    fader.setDoubleClickReturnValue (true, 0.0);
    fader.setTooltip ("Volume (double-click: 0 dB)");
    fader.onValueChange = [this]
    {
        host.engine().getProject().mixer.channel (index).rt->volumeDb = (float) fader.getValue();
        dbLabel.setText (juce::String (fader.getValue(), 1) + " dB", juce::dontSendNotification);
    };
    addAndMakeVisible (fader);

    panKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    panKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panKnob.setRange (-1.0, 1.0, 0.01);
    panKnob.setDoubleClickReturnValue (true, 0.0);
    panKnob.setTooltip ("Pan (double-click: centre)");
    panKnob.onValueChange = [this] { host.engine().getProject().mixer.channel (index).rt->pan = (float) panKnob.getValue(); };
    addAndMakeVisible (panKnob);

    muteButton.setClickingTogglesState (true);
    muteButton.setTooltip ("Mute");
    muteButton.onClick = [this] { host.engine().getProject().mixer.channel (index).rt->muted = muteButton.getToggleState(); };
    addAndMakeVisible (muteButton);

    soloButton.setClickingTogglesState (true);
    soloButton.setTooltip ("Solo (keeps everything routed to/from this channel)");
    soloButton.onClick = [this]
    {
        const bool on = soloButton.getToggleState();
        auto apply = [this] (bool state)
        {
            host.engine().getProject().mixer.channel (index).solo = state;
            host.engine().commit();
            soloButton.setToggleState (state, juce::dontSendNotification);
        };
        host.undo().perform ([apply, on] { apply (on); }, [apply, on] { apply (! on); }, on ? "Solo" : "Unsolo");
    };
    if (index != pink::MixerModel::kMaster)
        addAndMakeVisible (soloButton);

    dbLabel.setJustificationType (juce::Justification::centred);
    dbLabel.setFont (font (11.0f));
    addAndMakeVisible (dbLabel);

    infoLabel.setJustificationType (juce::Justification::topLeft);
    infoLabel.setFont (font (10.0f));
    infoLabel.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (infoLabel);

    addAndMakeVisible (meter);
    syncFromModel();
    startTimerHz (30);
}

void ChannelStrip::syncFromModel()
{
    const auto& mixer = host.engine().getProject().mixer;
    const auto& model = mixer.channel (index);
    fader.setValue (model.rt->volumeDb.load(), juce::dontSendNotification);
    panKnob.setValue (model.rt->pan.load(), juce::dontSendNotification);
    muteButton.setToggleState (model.rt->muted.load(), juce::dontSendNotification);
    soloButton.setToggleState (model.solo, juce::dontSendNotification);
    dbLabel.setText (juce::String (fader.getValue(), 1) + " dB", juce::dontSendNotification);

    juce::StringArray lines;
    for (const auto& p : model.inserts)
    {
        juce::String s (p->getName());
        if (p->acceptsSidechain() && model.sidechainSource >= 0)
            s << " (SC " << juce::String (mixer.channel (model.sidechainSource).name) << ")";
        lines.add (s);
    }
    for (const auto& s : model.sends)
        if (s.target >= 0)
            lines.add ("> " + juce::String (mixer.channel (s.target).name) + (s.preFader ? " (pre)" : ""));
    if (index != pink::MixerModel::kMaster)
        lines.add (model.output >= 0 ? "out: " + juce::String (mixer.channel (model.output).name) : juce::String ("out: none"));
    infoLabel.setText (lines.joinIntoString ("\n"), juce::dontSendNotification);
}

void ChannelStrip::timerCallback()
{
    const auto& m = host.engine().getProject().mixer.channel (index).rt->meter;
    meter.setLevels (m.getPeak (0), m.getPeak (1), m.getRms (0), m.getRms (1));
}

void ChannelStrip::paint (juce::Graphics& g)
{
    const auto& p = laf.palette();
    const auto& model = host.engine().getProject().mixer.channel (index);
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    g.setColour (index == 0 ? p.accent.withAlpha (0.12f) : p.panelAlt.withAlpha (p.dark ? 0.5f : 0.85f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (juce::Colour (model.colour));
    g.fillRoundedRectangle (r.removeFromTop (5.0f), 2.5f);
    g.setColour (p.textDim);
    g.setFont (font (9.0f, true));
    g.drawText (index == 0 ? juce::String ("MASTER") : "INSERT " + juce::String (index),
                getLocalBounds().withHeight (20).withY (6), juce::Justification::centred);
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced (6);
    r.removeFromTop (16);
    nameLabel.setBounds (r.removeFromTop (18));
    panKnob.setBounds (r.removeFromTop (40).withSizeKeepingCentre (40, 40));
    auto buttons = r.removeFromTop (24);
    if (index != 0)
    {
        muteButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (2, 0));
        soloButton.setBounds (buttons.reduced (2, 0));
    }
    else
    {
        muteButton.setBounds (buttons.withSizeKeepingCentre (buttons.getWidth() / 2, buttons.getHeight()));
    }
    infoLabel.setBounds (r.removeFromBottom (54));
    dbLabel.setBounds (r.removeFromBottom (16));
    r.removeFromTop (6);
    meter.setBounds (r.removeFromRight (16));
    fader.setBounds (r);
}

MixerPanel::MixerPanel (AudioHost& h, pinkui::PinkLookAndFeel& lnf) : host (h), laf (lnf)
{
    title.setText ("MIXER", juce::dontSendNotification);
    title.setFont (font (13.0f, true));
    addAndMakeVisible (title);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);
    rebuild();
}

void MixerPanel::rebuild()
{
    strips.clear();
    const auto& project = host.engine().getProject();
    const auto& mixer = project.mixer;

    // show channels that carry signal in this project (the other inserts exist but are idle)
    std::vector<bool> used ((size_t) mixer.getNumChannels(), false);
    for (const auto& t : project.tracks) used[(size_t) t.mixerChannel] = true;
    for (int i = 1; i < mixer.getNumChannels(); ++i)
    {
        const auto& c = mixer.channel (i);
        if (! c.inserts.empty() || c.solo) used[(size_t) i] = true;
        if (used[(size_t) i])
        {
            if (c.output > 0) used[(size_t) c.output] = true;
            for (const auto& s : c.sends) if (s.target > 0) used[(size_t) s.target] = true;
        }
    }

    for (int i = 1; i < mixer.getNumChannels(); ++i)
        if (used[(size_t) i])
            content.addAndMakeVisible (strips.add (new ChannelStrip (host, laf, i)));
    content.addAndMakeVisible (strips.add (new ChannelStrip (host, laf, pink::MixerModel::kMaster)));
    resized();
}

void MixerPanel::syncFromModel()
{
    for (auto* s : strips)
        s->syncFromModel();
}

void MixerPanel::paint (juce::Graphics& g)
{
    pinkui::drawPanel (g, getLocalBounds().toFloat().reduced (4.0f), laf.palette(), 14.0f);
}

void MixerPanel::resized()
{
    auto r = getLocalBounds().reduced (10);
    title.setBounds (r.removeFromTop (18));
    viewport.setBounds (r);
    const int stripW = 96;
    const int h = juce::jmax (100, r.getHeight() - (strips.size() * stripW > r.getWidth() ? 10 : 0));
    content.setSize (juce::jmax (r.getWidth(), strips.size() * stripW), h);
    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setBounds (i * stripW, 0, stripW, h);
}

// =======================================================================================
// MasterSection
// =======================================================================================
MasterSection::MasterSection (AudioHost& h, pinkui::PinkLookAndFeel& lnf) : host (h), laf (lnf), meter (lnf)
{
    addAndMakeVisible (meter);
    resetButton.setTooltip ("Reset the integrated loudness and true-peak hold");
    resetButton.onClick = [this] { host.engine().resetLoudnessMeters(); tpHoldDb = -144.0f; };
    addAndMakeVisible (resetButton);
    startTimerHz (20);
}

void MasterSection::timerCallback()
{
    readings = host.engine().getMasterMeters();
    meter.setLevels (readings.peakL, readings.peakR, readings.rmsL, readings.rmsR);
    tpHoldDb = pink::gainToDb (readings.truePeakMax);
    cpu = host.engine().getCpuLoad();
    limiterGr = 0.0f;
    for (const auto& p : host.engine().getProject().mixer.channel (0).inserts)
        if (auto* lim = dynamic_cast<pink::Limiter*> (p.get()))
            limiterGr = lim->getGainReductionDb();
    repaint();
}

void MasterSection::paint (juce::Graphics& g)
{
    const auto& p = laf.palette();
    pinkui::drawPanel (g, getLocalBounds().toFloat().reduced (4.0f), p, 14.0f);

    auto r = getLocalBounds().reduced (16);
    g.setColour (p.text);
    g.setFont (font (13.0f, true));
    g.drawText ("MASTER  |  LOUDNESS", r.removeFromTop (20), juce::Justification::centredLeft);
    r.removeFromLeft (60); // meter column

    struct Row { const char* label; juce::String value; const char* unit; bool hot; };
    const float tp = pink::gainToDb (readings.truePeakBlock);
    const Row rows[] = {
        { "Momentary", formatLufs (readings.momentaryLufs), "LUFS", false },
        { "Short-term", formatLufs (readings.shortTermLufs), "LUFS", false },
        { "Integrated", formatLufs (readings.integratedLufs), "LUFS", false },
        { "True peak", tp <= -143.0f ? juce::String ("-inf") : juce::String (tp, 1), "dBTP", tp > -1.0f },
        { "TP max", tpHoldDb <= -143.0f ? juce::String ("-inf") : juce::String (tpHoldDb, 1), "dBTP", tpHoldDb > -1.0f },
        { "Peak L / R", formatDb (readings.peakL) + " / " + formatDb (readings.peakR), "dBFS", false },
        { "Limiter GR", juce::String (limiterGr, 1), "dB", false },
        { "Audio CPU", juce::String (juce::roundToInt (cpu * 100.0f)), "%", cpu > 0.8f },
    };

    const int rowH = juce::jmin (24, (r.getHeight() - 60) / (int) std::size (rows));
    for (const auto& row : rows)
    {
        auto line = r.removeFromTop (rowH);
        g.setColour (p.textDim);
        g.setFont (font (12.0f));
        g.drawText (row.label, line.removeFromLeft (90), juce::Justification::centredLeft);
        g.setColour (row.hot ? p.meterHigh : p.text);
        g.setFont (font (15.0f, true));
        g.drawText (row.value, line.removeFromLeft (110), juce::Justification::centredRight);
        g.setColour (p.textDim);
        g.setFont (font (11.0f));
        g.drawText (row.unit, line.withTrimmedLeft (6), juce::Justification::centredLeft);
    }

    // phase correlation bar (-1 .. +1)
    r.removeFromTop (8);
    g.setColour (p.textDim);
    g.setFont (font (12.0f));
    g.drawText ("Phase correlation", r.removeFromTop (16), juce::Justification::centredLeft);
    auto bar = r.removeFromTop (12).toFloat().withWidth (juce::jmin (220.0f, (float) r.getWidth()));
    g.setColour (p.chrome);
    g.fillRoundedRectangle (bar, 6.0f);
    const float cx = bar.getCentreX();
    const float vx = cx + readings.correlation * bar.getWidth() * 0.5f;
    g.setColour (readings.correlation < 0.0f ? p.meterHigh : p.accent);
    g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (cx, vx), bar.getY(), std::abs (vx - cx) + 2.0f, bar.getHeight()), 6.0f);
    g.setColour (p.text);
    g.fillRect (cx - 0.5f, bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);
    g.setFont (font (11.0f, true));
    g.drawText (juce::String (readings.correlation, 2), bar.translated (bar.getWidth() + 8.0f, 0.0f).withWidth (50.0f),
                juce::Justification::centredLeft);
}

void MasterSection::resized()
{
    auto r = getLocalBounds().reduced (16);
    r.removeFromTop (24);
    meter.setBounds (r.removeFromLeft (44).withTrimmedBottom (30));
    resetButton.setBounds (getLocalBounds().reduced (16).removeFromBottom (26).removeFromRight (80));
}

// =======================================================================================
// AudioSettingsPanel
// =======================================================================================
AudioSettingsPanel::AudioSettingsPanel (AudioHost& h, pinkui::PinkLookAndFeel& lnf)
    : host (h), laf (lnf),
      selector (h.deviceManager(), 0, 16, 2, 16, true, false, true, false)
{
    addAndMakeVisible (selector);
    infoLabel.setFont (font (12.0f));
    infoLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (infoLabel);
    setSize (640, 640);
    startTimerHz (20);
}

AudioSettingsPanel::~AudioSettingsPanel()
{
    host.saveDeviceSettings();
}

void AudioSettingsPanel::timerCallback()
{
    for (int i = 0; i < 8; ++i)
        inputLevels[i] = host.engine().getInputPeak (i);

    juce::String info = host.describeCurrentDevice();
    if (auto* d = host.deviceManager().getCurrentAudioDevice())
    {
        info << "\nInput latency: " << d->getInputLatencyInSamples() << " samples  |  Output latency: "
             << d->getOutputLatencyInSamples() << " samples  |  Bit depth: " << d->getCurrentBitDepth()
             << "  |  XRUNs: " << d->getXRunCount();
        info << "\nDirect monitoring is handled by your interface's own mixer/software when it offers it.";
    }
    infoLabel.setText (info, juce::dontSendNotification);
    repaint (getLocalBounds().removeFromBottom (120));
}

void AudioSettingsPanel::paint (juce::Graphics& g)
{
    const auto& p = laf.palette();
    g.fillAll (p.background);

    auto r = getLocalBounds().reduced (16).removeFromBottom (110).removeFromTop (60);
    g.setColour (p.text);
    g.setFont (font (12.0f, true));
    g.drawText ("INPUT TEST (peak)", r.removeFromTop (16), juce::Justification::centredLeft);
    const int w = r.getWidth() / 8;
    for (int i = 0; i < 8; ++i)
    {
        auto cell = r.removeFromLeft (w).reduced (4, 2);
        g.setColour (p.textDim);
        g.setFont (font (10.0f));
        g.drawText ("IN " + juce::String (i + 1), cell.removeFromTop (14), juce::Justification::centred);
        auto bar = cell.toFloat().withHeight (12.0f);
        g.setColour (p.chrome);
        g.fillRoundedRectangle (bar, 4.0f);
        const float prop = LevelMeter::dbToProportion (pink::gainToDb (inputLevels[i]));
        g.setColour (inputLevels[i] >= 0.99f ? p.meterHigh : p.accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * prop), 4.0f);
    }
}

void AudioSettingsPanel::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto bottom = r.removeFromBottom (110);
    selector.setBounds (r);
    infoLabel.setBounds (bottom.removeFromBottom (50));
}
