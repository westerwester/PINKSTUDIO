// PINKSTUDIO - bridges JUCE's audio/MIDI device layer (ASIO, WASAPI, DirectSound, ...)
// to the device-independent pink::AudioEngine.
#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "pink/engine/AudioEngine.h"
#include "pink/undo/UndoManager.h"

#include <array>

/** Folders chosen in the installer (config/paths.xml next to the exe) or sensible defaults. */
struct AppPaths
{
    juce::File projects, plugins, configFile;
    juce::StringArray extraVst3Folders; // read now, used by the VST3 scanner (Phase 7)
    bool fromInstaller = false;
};

class AudioHost final : private juce::AudioIODeviceCallback
{
public:
    AudioHost();
    ~AudioHost() override;

    /** Opens the audio device (restoring the last saved setup). Returns an error message or empty. */
    juce::String initialiseDevices();
    void saveDeviceSettings();

    pink::AudioEngine& engine() noexcept { return audioEngine; }
    juce::AudioDeviceManager& deviceManager() noexcept { return devices; }
    pink::UndoManager& undo() noexcept { return undoManager; }

    /** Renders the whole song offline to a WAV file (the live callback is paused meanwhile). */
    bool exportMixdown (const juce::File& file, juce::String& resultMessage);

    const AppPaths& paths() const noexcept { return appPaths; }
    juce::String describeCurrentDevice() const;
    int getXrunCount() const;

    static juce::File getSettingsDirectory();

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels, int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void loadPaths();

    AppPaths appPaths;
    juce::AudioDeviceManager devices;
    pink::AudioEngine audioEngine;
    pink::UndoManager undoManager;
    juce::MidiMessageCollector midiCollector;
    juce::MidiBuffer midiBuffer;
    std::array<pink::MidiEvent, 1024> liveEvents {};
    bool callbackAttached = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioHost)
};
