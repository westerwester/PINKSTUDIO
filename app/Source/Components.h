// PINKSTUDIO - main window sections: transport bar, arrangement overview, mixer, master section,
// audio settings panel. Every control here drives the real engine; nothing is decorative.
#pragma once

#include "AudioHost.h"
#include "PinkTheme.h"

class MainComponent;

// ---------------------------------------------------------------------------------------
/** Button that paints a vector icon (play, stop, loop, metronome, record ...). */
class IconButton final : public juce::Button
{
public:
    enum class Icon { Play, Pause, Stop, Loop, Metronome };

    IconButton (const juce::String& name, Icon icon, pinkui::PinkLookAndFeel& lnf);
    void setIcon (Icon newIcon) { icon = newIcon; repaint(); }
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Icon icon;
    pinkui::PinkLookAndFeel& laf;
};

// ---------------------------------------------------------------------------------------
/** Vertical stereo meter with RMS body, peak line and peak-hold. Values are linear gains. */
class LevelMeter final : public juce::Component
{
public:
    explicit LevelMeter (pinkui::PinkLookAndFeel& lnf) : laf (lnf) {}
    void setLevels (float peakL, float peakR, float rmsL, float rmsR);
    void paint (juce::Graphics&) override;

    static float dbToProportion (float db) noexcept; // -60..+6 dB -> 0..1

private:
    pinkui::PinkLookAndFeel& laf;
    float peak[2] {}, rms[2] {}, hold[2] {};
    int holdCounter[2] {};
};

// ---------------------------------------------------------------------------------------
class TransportBar final : public juce::Component
{
public:
    TransportBar (AudioHost& host, pinkui::PinkLookAndFeel& lnf);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    void applyBpm (double bpm);

    AudioHost& host;
    pinkui::PinkLookAndFeel& laf;
    IconButton playButton, stopButton, loopButton, metronomeButton;
    juce::Slider bpmSlider;
    juce::Label bpmLabel, positionLabel, timeLabel, cpuLabel, deviceLabel;
    double bpmAtDragStart = 0.0;
    bool draggingBpm = false;
};

// ---------------------------------------------------------------------------------------
/** Read-only playlist overview with clip previews, loop region and playhead.
    Click to move the playhead, drag in the ruler to set the loop, click "M" to mute a track. */
class ArrangementView final : public juce::Component
{
public:
    ArrangementView (AudioHost& host, pinkui::PinkLookAndFeel& lnf);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh() { repaint(); }

private:
    static constexpr int kHeaderWidth = 150;
    static constexpr int kRulerHeight = 26;
    double xToPpq (float x) const;
    float ppqToX (double ppq) const;
    double visibleLength() const;
    int laneHeight() const;

    AudioHost& host;
    pinkui::PinkLookAndFeel& laf;
    double dragStartPpq = -1.0;
    bool draggingLoop = false;
};

// ---------------------------------------------------------------------------------------
class ChannelStrip final : public juce::Component, private juce::Timer
{
public:
    ChannelStrip (AudioHost& host, pinkui::PinkLookAndFeel& lnf, int channelIndex);
    void paint (juce::Graphics&) override;
    void resized() override;
    void syncFromModel();

private:
    void timerCallback() override;

    AudioHost& host;
    pinkui::PinkLookAndFeel& laf;
    const int index;
    juce::Label nameLabel, dbLabel, infoLabel;
    juce::Slider fader, panKnob;
    juce::TextButton muteButton { "M" }, soloButton { "S" };
    LevelMeter meter;
};

class MixerPanel final : public juce::Component
{
public:
    MixerPanel (AudioHost& host, pinkui::PinkLookAndFeel& lnf);
    void paint (juce::Graphics&) override;
    void resized() override;
    void rebuild();
    void syncFromModel();

private:
    AudioHost& host;
    pinkui::PinkLookAndFeel& laf;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<ChannelStrip> strips;
    juce::Label title;
};

// ---------------------------------------------------------------------------------------
class MasterSection final : public juce::Component, private juce::Timer
{
public:
    MasterSection (AudioHost& host, pinkui::PinkLookAndFeel& lnf);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    AudioHost& host;
    pinkui::PinkLookAndFeel& laf;
    LevelMeter meter;
    juce::TextButton resetButton { "Reset" };
    pink::MasterMeterReadings readings;
    float limiterGr = 0.0f, cpu = 0.0f;
    float tpHoldDb = -144.0f;
};

// ---------------------------------------------------------------------------------------
/** Device selection (driver type, in/out devices & channels, sample rate, buffer size,
    MIDI inputs, test tone) plus live input meters and latency information. */
class AudioSettingsPanel final : public juce::Component, private juce::Timer
{
public:
    AudioSettingsPanel (AudioHost& host, pinkui::PinkLookAndFeel& lnf);
    ~AudioSettingsPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    AudioHost& host;
    pinkui::PinkLookAndFeel& laf;
    juce::AudioDeviceSelectorComponent selector;
    juce::Label infoLabel;
    float inputLevels[8] {};
};
