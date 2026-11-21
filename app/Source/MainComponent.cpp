#include "MainComponent.h"

MainComponent::MainComponent (AudioHost& h, pinkui::PinkLookAndFeel& lnf, juce::ApplicationProperties& props)
    : host (h), laf (lnf), properties (props),
      transport (h, lnf), arrangement (h, lnf), mixer (h, lnf), master (h, lnf)
{
    commandManager.registerAllCommandsForTarget (this);
    if (auto* app = juce::JUCEApplication::getInstance())
        commandManager.registerAllCommandsForTarget (app);
    commandManager.setFirstCommandTarget (this);

    if (auto* settings = properties.getUserSettings())
        if (auto xml = settings->getXmlValue ("keyMappings"))
            commandManager.getKeyMappings()->restoreFromXml (*xml);

    addKeyListener (commandManager.getKeyMappings());
    setWantsKeyboardFocus (true);

    menuBar.setModel (this);
    setApplicationCommandManagerToWatch (&commandManager);
    addAndMakeVisible (menuBar);
    addAndMakeVisible (transport);
    addAndMakeVisible (arrangement);
    addAndMakeVisible (mixer);
    addAndMakeVisible (master);

    status.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (status);
    setStatus ("Demo session \"" + juce::String (host.engine().getProject().name)
               + "\" loaded  |  Space = play/pause  |  F10 = audio settings  |  Ctrl+R = export WAV");

    setSize (1500, 920);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    if (auto* settings = properties.getUserSettings())
        if (auto xml = commandManager.getKeyMappings()->createXml (true))
            settings->setValue ("keyMappings", xml.get());

    if (settingsWindow != nullptr) settingsWindow->exitModalState (0), delete settingsWindow.getComponent();
    if (keysWindow != nullptr) keysWindow->exitModalState (0), delete keysWindow.getComponent();

    setApplicationCommandManagerToWatch (nullptr);
    menuBar.setModel (nullptr);
}

void MainComponent::setStatus (const juce::String& text)
{
    status.setText (text, juce::dontSendNotification);
}

void MainComponent::paint (juce::Graphics& g)
{
    const auto& p = laf.palette();
    g.fillAll (p.background);

    // soft background glow
    auto r = getLocalBounds().toFloat();
    g.setGradientFill ({ p.accent.withAlpha (p.dark ? 0.10f : 0.08f), r.getRight(), r.getY(),
                         p.background.withAlpha (0.0f), r.getCentreX(), r.getCentreY(), true });
    g.fillRect (r);
    status.setColour (juce::Label::textColourId, p.textDim);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    menuBar.setBounds (r.removeFromTop (24));
    status.setBounds (r.removeFromBottom (22).reduced (10, 0));
    transport.setBounds (r.removeFromTop (78).reduced (4, 2));

    auto bottom = r.removeFromBottom (juce::jmax (330, r.getHeight() * 44 / 100));
    mixer.setBounds (bottom.reduced (4, 2));
    master.setBounds (r.removeFromRight (360).reduced (4, 2));
    arrangement.setBounds (r.reduced (4, 2));
}

void MainComponent::timerCallback()
{
    transport.refresh();
    arrangement.refresh();
    host.engine().collectGarbage();

    if (host.undo().canUndo() != lastCanUndo || host.undo().canRedo() != lastCanRedo)
    {
        lastCanUndo = host.undo().canUndo();
        lastCanRedo = host.undo().canRedo();
        commandManager.commandStatusChanged();
    }
}

void MainComponent::applyTheme (int index)
{
    laf.setTheme (index);
    if (auto* settings = properties.getUserSettings())
        settings->setValue ("theme", index);
    if (auto* top = getTopLevelComponent())
    {
        top->sendLookAndFeelChange();
        top->repaint();
    }
    commandManager.commandStatusChanged();
}

// ---- commands ----------------------------------------------------------------------------
void MainComponent::getAllCommands (juce::Array<juce::CommandID>& c)
{
    c.addArray ({ cmdPlayPause, cmdStop, cmdGoToStart, cmdToggleLoop, cmdToggleMetronome, cmdAudioSettings, cmdExportWav,
                  cmdUndo, cmdRedo, cmdKeyMappings, cmdResetLoudness, cmdPanic, cmdAbout });
    for (int i = 0; i < (int) pinkui::getThemes().size(); ++i)
        c.add (cmdThemeFirst + i);
}

void MainComponent::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    const auto cmd = juce::ModifierKeys::commandModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;
    const auto t = host.engine().getTransportInfo();

    switch (id)
    {
        case cmdPlayPause:
            info.setInfo ("Play / Pause", "Start or pause playback", "Transport", 0);
            info.addDefaultKeypress (juce::KeyPress::spaceKey, 0);
            break;
        case cmdStop:
            info.setInfo ("Stop", "Stop and return to the play start (twice: go to the start)", "Transport", 0);
            info.addDefaultKeypress (juce::KeyPress::returnKey, 0);
            break;
        case cmdGoToStart:
            info.setInfo ("Go to start", "Move the playhead to bar 1", "Transport", 0);
            info.addDefaultKeypress (juce::KeyPress::homeKey, 0);
            break;
        case cmdToggleLoop:
            info.setInfo ("Loop", "Toggle the loop region", "Transport", 0);
            info.setTicked (t.loopEnabled);
            info.addDefaultKeypress ('l', 0);
            break;
        case cmdToggleMetronome:
            info.setInfo ("Metronome", "Toggle the metronome click", "Transport", 0);
            info.setTicked (t.metronome);
            info.addDefaultKeypress ('m', cmd);
            break;
        case cmdAudioSettings:
            info.setInfo ("Audio settings...", "Driver, devices, sample rate, buffer size, MIDI inputs", "Options", 0);
            info.addDefaultKeypress (juce::KeyPress::F10Key, 0);
            break;
        case cmdExportWav:
            info.setInfo ("Export mixdown (WAV)...", "Render the song offline to a 24-bit WAV file", "File", 0);
            info.addDefaultKeypress ('r', cmd);
            break;
        case cmdUndo:
            info.setInfo ("Undo " + juce::String (host.undo().getUndoDescription()), "Undo the last action", "Edit", 0);
            info.setActive (host.undo().canUndo());
            info.addDefaultKeypress ('z', cmd);
            break;
        case cmdRedo:
            info.setInfo ("Redo " + juce::String (host.undo().getRedoDescription()), "Redo the last undone action", "Edit", 0);
            info.setActive (host.undo().canRedo());
            info.addDefaultKeypress ('y', cmd);
            info.addDefaultKeypress ('z', cmd | shift);
            break;
        case cmdKeyMappings:
            info.setInfo ("Keyboard shortcuts...", "Customise every shortcut", "Options", 0);
            break;
        case cmdResetLoudness:
            info.setInfo ("Reset loudness meter", "Clear integrated LUFS and true-peak hold", "Options", 0);
            break;
        case cmdPanic:
            info.setInfo ("MIDI panic", "Silence every instrument immediately", "Options", 0);
            info.addDefaultKeypress (juce::KeyPress::escapeKey, shift);
            break;
        case cmdAbout:
            info.setInfo ("About PINKSTUDIO", "Version and build information", "Help", 0);
            break;
        default:
            if (id >= cmdThemeFirst && id < cmdThemeFirst + (int) pinkui::getThemes().size())
            {
                const int index = id - cmdThemeFirst;
                info.setInfo (pinkui::getThemes()[(size_t) index].name, "Switch colour theme", "View", 0);
                info.setTicked (laf.getThemeIndex() == index);
            }
            break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    auto& engine = host.engine();
    switch (info.commandID)
    {
        case cmdPlayPause:       engine.togglePlay(); break;
        case cmdStop:            engine.stop(); break;
        case cmdGoToStart:       engine.locate (0.0); break;
        case cmdToggleLoop:      engine.setLoopEnabled (! engine.getTransportInfo().loopEnabled); break;
        case cmdToggleMetronome: engine.setMetronomeEnabled (! engine.getTransportInfo().metronome); break;
        case cmdAudioSettings:   showAudioSettings(); break;
        case cmdExportWav:       exportMixdown(); break;
        case cmdUndo:            host.undo().undo(); afterUndoRedo(); break;
        case cmdRedo:            host.undo().redo(); afterUndoRedo(); break;
        case cmdKeyMappings:     showKeyMappings(); break;
        case cmdResetLoudness:   engine.resetLoudnessMeters(); break;
        case cmdPanic:           engine.panic(); setStatus ("MIDI panic sent"); break;
        case cmdAbout:           showAbout(); break;
        default:
            if (info.commandID >= cmdThemeFirst && info.commandID < cmdThemeFirst + (int) pinkui::getThemes().size())
            {
                applyTheme (info.commandID - cmdThemeFirst);
                break;
            }
            return false;
    }
    commandManager.commandStatusChanged();
    return true;
}

void MainComponent::afterUndoRedo()
{
    mixer.syncFromModel();
    arrangement.repaint();
    transport.refresh();
}

// ---- menus -------------------------------------------------------------------------------
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "View", "Options", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu m;
    auto* cm = &commandManager;
    switch (index)
    {
        case 0:
            m.addCommandItem (cm, cmdExportWav);
            m.addSeparator();
            m.addCommandItem (cm, juce::StandardApplicationCommandIDs::quit);
            break;
        case 1:
            m.addCommandItem (cm, cmdUndo);
            m.addCommandItem (cm, cmdRedo);
            break;
        case 2:
            for (int i = 0; i < (int) pinkui::getThemes().size(); ++i)
                m.addCommandItem (cm, cmdThemeFirst + i);
            break;
        case 3:
            m.addCommandItem (cm, cmdAudioSettings);
            m.addCommandItem (cm, cmdKeyMappings);
            m.addSeparator();
            m.addCommandItem (cm, cmdToggleLoop);
            m.addCommandItem (cm, cmdToggleMetronome);
            m.addCommandItem (cm, cmdResetLoudness);
            m.addCommandItem (cm, cmdPanic);
            break;
        case 4:
            m.addCommandItem (cm, cmdAbout);
            break;
        default: break;
    }
    return m;
}

// ---- dialogs -----------------------------------------------------------------------------
void MainComponent::showAudioSettings()
{
    if (settingsWindow != nullptr)
    {
        settingsWindow->toFront (true);
        return;
    }

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new AudioSettingsPanel (host, laf));
    o.dialogTitle = "PINKSTUDIO - Audio Settings";
    o.dialogBackgroundColour = laf.palette().background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.componentToCentreAround = this;
    settingsWindow = o.launchAsync();
}

void MainComponent::showKeyMappings()
{
    if (keysWindow != nullptr)
    {
        keysWindow->toFront (true);
        return;
    }

    auto* editor = new juce::KeyMappingEditorComponent (*commandManager.getKeyMappings(), true);
    editor->setSize (560, 520);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (editor);
    o.dialogTitle = "PINKSTUDIO - Keyboard Shortcuts";
    o.dialogBackgroundColour = laf.palette().background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.componentToCentreAround = this;
    keysWindow = o.launchAsync();
}

void MainComponent::exportMixdown()
{
    auto folder = host.paths().projects;
    if (! folder.isDirectory() && ! folder.createDirectory())
        folder = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    const auto initial = folder.getChildFile (juce::File::createLegalFileName (juce::String (host.engine().getProject().name)) + ".wav");
    chooser = std::make_unique<juce::FileChooser> ("Export mixdown", initial, "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File())
                                  return;
                              if (! file.hasFileExtension ("wav"))
                                  file = file.withFileExtension ("wav");
                              setStatus ("Rendering " + file.getFileName() + " ...");
                              juce::String message;
                              host.exportMixdown (file, message);
                              setStatus (message);
                          });
}

void MainComponent::showAbout()
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::InfoIcon)
                                      .withTitle ("PINKSTUDIO " + juce::String (PINKSTUDIO_VERSION))
                                      .withMessage ("First build (development phases 1-2).\n\n"
                                                    "Real-time engine: C++20, lock-free, no allocation in the audio callback.\n"
                                                    "Drivers: ASIO, WASAPI (shared/exclusive), DirectSound via JUCE "
                                                    + juce::String (JUCE_MAJOR_VERSION) + "." + juce::String (JUCE_MINOR_VERSION) + "."
                                                    + juce::String (JUCE_BUILDNUMBER) + ".\n"
                                                    "Implemented and missing features: see docs/ROADMAP.md.\n\n"
                                                    "Projects folder: " + host.paths().projects.getFullPathName() + "\n"
                                                    "Plug-in folder: " + host.paths().plugins.getFullPathName() + "\n"
                                                    "Extra VST3 folders: " + (host.paths().extraVst3Folders.isEmpty()
                                                        ? juce::String ("none") : host.paths().extraVst3Folders.joinIntoString ("; "))
                                                    + " (VST3 scanning arrives in phase 7)")
                                      .withButton ("OK"),
                                  nullptr);
}
