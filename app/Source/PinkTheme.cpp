#include "PinkTheme.h"

namespace pinkui
{
const std::vector<Palette>& getThemes()
{
    static const std::vector<Palette> themes = []
    {
        std::vector<Palette> t;
        auto c = [] (juce::uint32 argb) { return juce::Colour (argb); };

        Palette def;
        def.name = "Pink Default";
        def.background = c (0xFFFFF0F6); def.panel = c (0xFFFFE1EE); def.panelAlt = c (0xFFFFFFFF);
        def.outline = c (0xFFF3B6D2);
        def.accent = c (0xFFFF4FA3); def.accentHot = c (0xFFFF1493); def.accentAlt = c (0xFFC77DFF);
        def.text = c (0xFF5A2340); def.textDim = c (0xFFA0708A); def.chrome = c (0xFFE9D3E0);
        def.meterLow = c (0xFFC77DFF); def.meterMid = c (0xFFFF4FA3); def.meterHigh = c (0xFFFF2E4C);
        t.push_back (def);

        Palette dark;
        dark.name = "Dark Pink"; dark.dark = true;
        dark.background = c (0xFF170D14); dark.panel = c (0xFF27141F); dark.panelAlt = c (0xFF33192A);
        dark.outline = c (0xFF5C2A47);
        dark.accent = c (0xFFFF5FB0); dark.accentHot = c (0xFFFF2D95); dark.accentAlt = c (0xFFB98CFF);
        dark.text = c (0xFFFFE6F2); dark.textDim = c (0xFFB48AA3); dark.chrome = c (0xFF8E6B80);
        dark.meterLow = c (0xFFB98CFF); dark.meterMid = c (0xFFFF5FB0); dark.meterHigh = c (0xFFFF3D5A);
        t.push_back (dark);

        Palette cyber;
        cyber.name = "Cyber Pink"; cyber.dark = true;
        cyber.background = c (0xFF0C0A1E); cyber.panel = c (0xFF17123A); cyber.panelAlt = c (0xFF221A52);
        cyber.outline = c (0xFF4B2E8C);
        cyber.accent = c (0xFFFF2BD6); cyber.accentHot = c (0xFFFF006E); cyber.accentAlt = c (0xFF00E5FF);
        cyber.text = c (0xFFF6E8FF); cyber.textDim = c (0xFF9C8CC9); cyber.chrome = c (0xFF7A6FB0);
        cyber.meterLow = c (0xFF00E5FF); cyber.meterMid = c (0xFFFF2BD6); cyber.meterHigh = c (0xFFFF2E4C);
        t.push_back (cyber);

        Palette pastel;
        pastel.name = "Pastel";
        pastel.background = c (0xFFFBF7FF); pastel.panel = c (0xFFF3E8FF); pastel.panelAlt = c (0xFFFFFFFF);
        pastel.outline = c (0xFFDCC8F2);
        pastel.accent = c (0xFFF59AC8); pastel.accentHot = c (0xFFEF6FAE); pastel.accentAlt = c (0xFFAE9BF0);
        pastel.text = c (0xFF4B3B5C); pastel.textDim = c (0xFF9A89AD); pastel.chrome = c (0xFFE6DDF0);
        pastel.meterLow = c (0xFFAE9BF0); pastel.meterMid = c (0xFFF59AC8); pastel.meterHigh = c (0xFFF0607E);
        t.push_back (pastel);
        return t;
    }();
    return themes;
}

PinkLookAndFeel::PinkLookAndFeel()
{
    setTheme (0);
}

void PinkLookAndFeel::setTheme (int index)
{
    themeIndex = juce::jlimit (0, (int) getThemes().size() - 1, index);
    const auto& p = palette();

    setColourScheme ({ p.background, p.panel, p.panelAlt, p.outline, p.text, p.accent,
                       p.dark ? p.background : juce::Colours::white, p.accentAlt, p.text });

    setColour (juce::ResizableWindow::backgroundColourId, p.background);
    setColour (juce::DocumentWindow::backgroundColourId, p.background);
    setColour (juce::TextButton::buttonColourId, p.panelAlt);
    setColour (juce::TextButton::buttonOnColourId, p.accent);
    setColour (juce::TextButton::textColourOffId, p.text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Label::textColourId, p.text);
    setColour (juce::Slider::thumbColourId, p.accent);
    setColour (juce::Slider::trackColourId, p.accent);
    setColour (juce::Slider::backgroundColourId, p.chrome);
    setColour (juce::Slider::rotarySliderFillColourId, p.accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, p.chrome);
    setColour (juce::Slider::textBoxTextColourId, p.text);
    setColour (juce::Slider::textBoxBackgroundColourId, p.panelAlt);
    setColour (juce::Slider::textBoxOutlineColourId, p.outline);
    setColour (juce::PopupMenu::backgroundColourId, p.panelAlt);
    setColour (juce::PopupMenu::textColourId, p.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ComboBox::backgroundColourId, p.panelAlt);
    setColour (juce::ComboBox::textColourId, p.text);
    setColour (juce::ComboBox::outlineColourId, p.outline);
    setColour (juce::ComboBox::arrowColourId, p.accent);
    setColour (juce::ListBox::backgroundColourId, p.panelAlt);
    setColour (juce::ListBox::textColourId, p.text);
    setColour (juce::TextEditor::backgroundColourId, p.panelAlt);
    setColour (juce::TextEditor::textColourId, p.text);
    setColour (juce::TextEditor::outlineColourId, p.outline);
    setColour (juce::TextEditor::highlightColourId, p.accent.withAlpha (0.35f));
    setColour (juce::ToggleButton::textColourId, p.text);
    setColour (juce::ToggleButton::tickColourId, p.accent);
    setColour (juce::ToggleButton::tickDisabledColourId, p.textDim);
    setColour (juce::ScrollBar::thumbColourId, p.accent.withAlpha (0.6f));
    setColour (juce::AlertWindow::backgroundColourId, p.panelAlt);
    setColour (juce::AlertWindow::textColourId, p.text);
    setColour (juce::AlertWindow::outlineColourId, p.accent);
    setColour (juce::GroupComponent::outlineColourId, p.outline);
    setColour (juce::GroupComponent::textColourId, p.text);
    setColour (juce::TreeView::backgroundColourId, p.panelAlt);
    setColour (juce::CaretComponent::caretColourId, p.accentHot);
}

void PinkLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                                            bool over, bool down)
{
    const auto& p = palette();
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = juce::jmin (r.getHeight() * 0.5f, 10.0f);
    const bool on = b.getToggleState();

    juce::Colour top = on ? p.accent.brighter (0.15f) : bg.brighter (0.06f);
    juce::Colour bottom = on ? p.accentHot : bg.darker (0.06f);
    if (over) { top = top.brighter (0.08f); bottom = bottom.brighter (0.08f); }
    if (down) std::swap (top, bottom);

    g.setGradientFill ({ top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false });
    g.fillRoundedRectangle (r, corner);

    // chrome rim + glossy highlight
    g.setColour ((on ? p.accentHot : p.outline).withAlpha (0.9f));
    g.drawRoundedRectangle (r, corner, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (p.dark ? 0.06f : 0.35f));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f).reduced (2.0f, 1.0f), corner * 0.8f);
}

juce::Font PinkLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (15.0f, (float) buttonHeight * 0.55f), juce::Font::bold));
}

void PinkLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    const auto& p = palette();
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float lineW = juce::jmax (2.5f, radius * 0.18f);
    const float arcR = radius - lineW * 0.5f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (p.chrome);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bipolar parameters (e.g. pan) draw their arc from the centre
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? startAngle + (endAngle - startAngle) * (float) s.valueToProportionOfLength (0.0) : startAngle;
    const float angle = startAngle + pos * (endAngle - startAngle);
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
    g.setColour (s.isEnabled() ? p.accent : p.textDim);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = arcR - lineW * 1.1f;
    g.setGradientFill ({ p.panelAlt.brighter (0.2f), centre.x, centre.y - knobR, p.chrome, centre.x, centre.y + knobR, false });
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (p.outline);
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobR + 2.0f, 3.0f, knobR * 0.55f, 1.5f);
    g.setColour (p.accentHot);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void PinkLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                                        float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
        return;
    }

    const auto& p = palette();
    const float cx = (float) x + (float) w * 0.5f;
    const auto groove = juce::Rectangle<float> (cx - 2.5f, (float) y, 5.0f, (float) h);
    g.setColour (p.chrome);
    g.fillRoundedRectangle (groove, 2.5f);
    g.setColour (p.accent.withAlpha (0.8f));
    g.fillRoundedRectangle (groove.withTop (sliderPos), 2.5f);

    // fader cap
    const auto cap = juce::Rectangle<float> (cx - 13.0f, sliderPos - 8.0f, 26.0f, 16.0f);
    g.setGradientFill ({ p.panelAlt.brighter (0.3f), cap.getX(), cap.getY(), p.chrome, cap.getX(), cap.getBottom(), false });
    g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (p.outline.darker (0.1f));
    g.drawRoundedRectangle (cap, 4.0f, 1.0f);
    g.setColour (p.accentHot);
    g.fillRect (cap.withSizeKeepingCentre (18.0f, 2.0f));
}

void PinkLookAndFeel::drawMenuBarBackground (juce::Graphics& g, int w, int h, bool, juce::MenuBarComponent&)
{
    const auto& p = palette();
    g.setGradientFill ({ p.panelAlt, 0.0f, 0.0f, p.panel, 0.0f, (float) h, false });
    g.fillRect (0, 0, w, h);
    g.setColour (p.outline);
    g.drawHorizontalLine (h - 1, 0.0f, (float) w);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const Palette& p, float corner)
{
    g.setGradientFill ({ p.panelAlt.withAlpha (p.dark ? 1.0f : 0.95f), r.getX(), r.getY(),
                         p.panel, r.getX(), r.getBottom(), false });
    g.fillRoundedRectangle (r, corner);
    g.setColour (p.outline);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

} // namespace pinkui
