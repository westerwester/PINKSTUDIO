// PINKSTUDIO - main window content: menus, commands (customisable shortcuts), layout.
#pragma once

#include "Components.h"

class MainComponent final : public juce::Component,
                            public juce::ApplicationCommandTarget,
                            public juce::MenuBarModel,
                            private juce::Timer
{
public:
    enum CommandIDs
    {
        cmdPlayPause = 0x2001,
        cmdStop,
        cmdGoToStart,
        cmdToggleLoop,
        cmdToggleMetronome,
        cmdAudioSettings,
        cmdExportWav,
        cmdUndo,
        cmdRedo,
        cmdKeyMappings,
        cmdResetLoudness,
        cmdPanic,
        cmdAbout,
        cmdThemeFirst = 0x2100 // + theme index
    };

    MainComponent (AudioHost& host, pinkui::PinkLookAndFeel& lnf, juce::ApplicationProperties& props);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setStatus (const juce::String& text);

    // ApplicationCommandTarget
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;
    bool perform (const InvocationInfo& info) override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected (int, int) override {}

private:
    void timerCallback() override;
    void applyTheme (int index);
    void showAudioSettings();
    void showKeyMappings();
    void exportMixdown();
    void showAbout();
    void afterUndoRedo();

    AudioHost& host;
    pinkui::PinkLookAndFeel& laf;
    juce::ApplicationProperties& properties;
    juce::ApplicationCommandManager commandManager;
    juce::MenuBarComponent menuBar;
    TransportBar transport;
    ArrangementView arrangement;
    MixerPanel mixer;
    MasterSection master;
    juce::Label status;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::DialogWindow> settingsWindow, keysWindow;
    bool lastCanUndo = false, lastCanRedo = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
