#include "AudioHost.h"

#include "pink/demo/DemoProject.h"
#include "pink/engine/OfflineRenderer.h"

AudioHost::AudioHost()
{
    loadPaths();
    pink::buildDemoProject (audioEngine.getProject());
    audioEngine.prepare (48000.0, 512); // valid snapshot even before a device is open
    audioEngine.setLoop (0.0, audioEngine.getProject().getSongEndPpq());
    audioEngine.setLoopEnabled (true);
    midiBuffer.ensureSize (8192);
}

AudioHost::~AudioHost()
{
    devices.removeMidiInputDeviceCallback ({}, &midiCollector);
    if (callbackAttached)
        devices.removeAudioCallback (this);
    devices.closeAudioDevice();
}

void AudioHost::loadPaths()
{
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("PINKSTUDIO");
    appPaths.projects = docs.getChildFile ("Projects");
    appPaths.plugins = docs.getChildFile ("Plugins");
    appPaths.configFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                              .getParentDirectory().getChildFile ("config").getChildFile ("paths.xml");

    if (auto xml = juce::XmlDocument::parse (appPaths.configFile))
    {
        if (xml->hasTagName ("PINKSTUDIO_PATHS"))
        {
            appPaths.fromInstaller = true;
            if (xml->getStringAttribute ("projects").isNotEmpty())
                appPaths.projects = juce::File (xml->getStringAttribute ("projects"));
            if (xml->getStringAttribute ("plugins").isNotEmpty())
                appPaths.plugins = juce::File (xml->getStringAttribute ("plugins"));
            for (auto* e : xml->getChildWithTagNameIterator ("VST3_FOLDER"))
                if (e->getStringAttribute ("path").isNotEmpty())
                    appPaths.extraVst3Folders.add (e->getStringAttribute ("path"));
        }
    }
}

juce::File AudioHost::getSettingsDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("PINKSTUDIO");
    dir.createDirectory();
    return dir;
}

juce::String AudioHost::initialiseDevices()
{
    const auto settingsFile = getSettingsDirectory().getChildFile ("audio-device.xml");
    std::unique_ptr<juce::XmlElement> saved;
    if (settingsFile.existsAsFile())
        saved = juce::XmlDocument::parse (settingsFile);

    // up to 16 inputs / 16 outputs; the engine plays on outputs 1-2
    const auto error = devices.initialise (16, 16, saved.get(), true);

    if (saved == nullptr)
        for (const auto& input : juce::MidiInput::getAvailableDevices())
            devices.setMidiInputDeviceEnabled (input.identifier, true);

    devices.addMidiInputDeviceCallback ({}, &midiCollector);
    devices.addAudioCallback (this);
    callbackAttached = true;
    return error;
}

void AudioHost::saveDeviceSettings()
{
    if (auto xml = devices.createStateXml())
        xml->writeTo (getSettingsDirectory().getChildFile ("audio-device.xml"));
}

juce::String AudioHost::describeCurrentDevice() const
{
    auto* d = devices.getCurrentAudioDevice();
    if (d == nullptr)
        return "No audio device";

    const double sr = d->getCurrentSampleRate();
    const int block = d->getCurrentBufferSizeSamples();
    const int outLatency = d->getOutputLatencyInSamples() + block;
    return d->getTypeName() + " | " + d->getName() + " | " + juce::String (sr / 1000.0, 1) + " kHz | "
         + juce::String (block) + " smp | out " + juce::String (1000.0 * outLatency / juce::jmax (1.0, sr), 1) + " ms";
}

int AudioHost::getXrunCount() const
{
    auto* d = devices.getCurrentAudioDevice();
    return d != nullptr ? d->getXRunCount() : 0;
}

bool AudioHost::exportMixdown (const juce::File& file, juce::String& message)
{
    const bool wasAttached = callbackAttached;
    if (wasAttached)
    {
        devices.removeAudioCallback (this); // waits for the running callback to finish
        callbackAttached = false;
    }

    pink::OfflineRenderSettings settings;
    if (auto* d = devices.getCurrentAudioDevice())
        settings.sampleRate = d->getCurrentSampleRate() > 0 ? d->getCurrentSampleRate() : 48000.0;
    settings.tailSeconds = 2.5;
    settings.format = pink::WavFormat::Pcm24;

    const auto result = pink::renderToWav (audioEngine, settings, file.getFullPathName().toStdString());

    if (wasAttached)
    {
        devices.addAudioCallback (this); // re-prepares the engine for the live device
        callbackAttached = true;
    }

    if (! result.ok)
    {
        message = "Export failed: " + juce::String (result.error);
        return false;
    }

    message = "Exported " + file.getFileName() + "  |  " + juce::String (result.seconds, 1) + " s  |  "
            + juce::String (result.integratedLufs, 1) + " LUFS  |  true peak " + juce::String (result.truePeakDb, 1)
            + " dBTP  |  rendered " + juce::String (result.seconds / juce::jmax (1.0e-6, result.renderSeconds), 0) + "x real time";
    return true;
}

// ---- audio thread -------------------------------------------------------------------
void AudioHost::audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs, float* const* outputs,
                                                  int numOutputs, int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    midiBuffer.clear();
    midiCollector.removeNextBlockOfMessages (midiBuffer, numSamples);

    int count = 0;
    for (const auto metadata : midiBuffer)
    {
        if (count >= (int) liveEvents.size())
            break;
        const auto msg = metadata.getMessage();
        if (msg.getRawDataSize() < 1 || msg.getRawDataSize() > 3 || msg.isSysEx())
            continue;
        const auto* raw = msg.getRawData();
        pink::MidiEvent e;
        e.sampleOffset = metadata.samplePosition;
        e.status = raw[0];
        e.data1 = msg.getRawDataSize() > 1 ? raw[1] : 0;
        e.data2 = msg.getRawDataSize() > 2 ? raw[2] : 0;
        liveEvents[(size_t) count++] = e;
    }

    audioEngine.processBlock (inputs, numInputs, outputs, numOutputs, numSamples, liveEvents.data(), count);
}

void AudioHost::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const double sr = device->getCurrentSampleRate();
    const int block = device->getCurrentBufferSizeSamples();
    audioEngine.prepare (sr > 0 ? sr : 48000.0, block > 0 ? block : 512);
    midiCollector.reset (sr > 0 ? sr : 48000.0);
}

void AudioHost::audioDeviceStopped() {}
