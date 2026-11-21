// PINKSTUDIO - application entry point.
//
// Command line (used by CI and docs; the normal user never needs it):
//   --screenshot <file.png>   render the UI after a short headless playback and exit
//   --theme <0..3>            theme for this run
#include "MainComponent.h"

class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow (const juce::String& name, MainComponent* content, juce::Colour background)
        : DocumentWindow (name, background, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (content, true);
        setResizable (true, true);
        setResizeLimits (1100, 700, 16384, 16384);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class PinkStudioApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "PINKSTUDIO"; }
    const juce::String getApplicationVersion() override { return PINKSTUDIO_VERSION; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName = "PINKSTUDIO";
        opts.folderName = "PINKSTUDIO";
        opts.filenameSuffix = ".settings";
        opts.osxLibrarySubFolder = "Application Support";
        properties.setStorageParameters (opts);

        const auto args = juce::StringArray::fromTokens (commandLine, true);
        const int shotIndex = args.indexOf ("--screenshot");
        const int themeIndex = args.indexOf ("--theme");
        screenshotPath = shotIndex >= 0 && shotIndex + 1 < args.size() ? args[shotIndex + 1].unquoted() : juce::String();

        lookAndFeel = std::make_unique<pinkui::PinkLookAndFeel>();
        int theme = properties.getUserSettings()->getIntValue ("theme", 0);
        if (themeIndex >= 0 && themeIndex + 1 < args.size())
            theme = args[themeIndex + 1].getIntValue();
        lookAndFeel->setTheme (theme);
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

        host = std::make_unique<AudioHost>();
        juce::String deviceError;
        if (screenshotPath.isEmpty())
            deviceError = host->initialiseDevices();

        auto* content = new MainComponent (*host, *lookAndFeel, properties);
        mainComponent = content;
        mainWindow = std::make_unique<MainWindow> ("PINKSTUDIO", content, lookAndFeel->palette().background);

        if (deviceError.isNotEmpty())
            content->setStatus ("Audio device problem: " + deviceError + "  -  open Options > Audio settings (F10)");
        else if (screenshotPath.isEmpty() && host->deviceManager().getCurrentAudioDevice() == nullptr)
            content->setStatus ("No audio output device is open  -  choose one in Options > Audio settings (F10)");

        if (screenshotPath.isNotEmpty())
            takeScreenshotSoon();
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        if (host != nullptr && screenshotPath.isEmpty())
            host->saveDeviceSettings();
        host = nullptr;
        properties.saveIfNeeded();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        lookAndFeel = nullptr;
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted (const juce::String&) override
    {
        if (mainWindow != nullptr)
            mainWindow->toFront (true);
    }

private:
    /** Headless mode: no audio device; the engine is driven directly so meters and playhead move. */
    void takeScreenshotSoon()
    {
        auto& engine = host->engine();
        engine.prepare (48000.0, 512);
        engine.play();
        std::vector<float> l (512), r (512);
        float* outs[2] = { l.data(), r.data() };
        for (int i = 0; i < (int) (5.6 * 48000 / 512); ++i)
            engine.processBlock (nullptr, 0, outs, 2, 512);

        juce::Timer::callAfterDelay (1500, [this]
        {
            if (mainComponent == nullptr)
                return;
            const auto image = mainComponent->createComponentSnapshot (mainComponent->getLocalBounds(), true, 1.0f);
            juce::File out (screenshotPath);
            out.deleteFile();
            juce::FileOutputStream stream (out);
            juce::PNGImageFormat png;
            const bool ok = stream.openedOk() && png.writeImageToStream (image, stream);
            stream.flush();
            setApplicationReturnValue (ok ? 0 : 1);
            quit();
        });
    }

    juce::ApplicationProperties properties;
    std::unique_ptr<pinkui::PinkLookAndFeel> lookAndFeel;
    std::unique_ptr<AudioHost> host;
    std::unique_ptr<MainWindow> mainWindow;
    juce::Component::SafePointer<MainComponent> mainComponent;
    juce::String screenshotPath;
};

START_JUCE_APPLICATION (PinkStudioApplication)
