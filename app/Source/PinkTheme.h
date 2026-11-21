// PINKSTUDIO - colour themes and the custom pink LookAndFeel.
#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace pinkui
{
struct Palette
{
    juce::String name;
    juce::Colour background, panel, panelAlt, outline;
    juce::Colour accent, accentHot, accentAlt;   // barbie pink, neon pink, pastel violet
    juce::Colour text, textDim, chrome;
    juce::Colour meterLow, meterMid, meterHigh;
    bool dark = false;
};

/** Built-in themes: Pink Default, Dark Pink, Cyber Pink, Pastel. */
const std::vector<Palette>& getThemes();

class PinkLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PinkLookAndFeel();

    void setTheme (int index);
    int getThemeIndex() const noexcept { return themeIndex; }
    const Palette& palette() const noexcept { return getThemes()[(size_t) themeIndex]; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawMenuBarBackground (juce::Graphics&, int width, int height, bool isMouseOverBar, juce::MenuBarComponent&) override;

private:
    int themeIndex = 0;
};

/** Rounded "glass" panel background used by all sections. */
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const Palette& p, float corner = 12.0f);

} // namespace pinkui
