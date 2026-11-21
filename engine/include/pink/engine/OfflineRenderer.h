// PINKSTUDIO - offline (faster than real time) mixdown through the very same
// AudioEngine::processBlock() used for live playback, so exports match what you hear.
// The audio device callback must be detached while rendering.
#pragma once

#include "pink/engine/AudioEngine.h"
#include "pink/io/WavFile.h"

#include <functional>
#include <string>

namespace pink
{
struct OfflineRenderSettings
{
    double sampleRate = 48000.0;
    int blockSize = 512;
    double startPpq = 0.0;
    double endPpq = -1.0;          // < 0 : end of the song
    double tailSeconds = 2.0;      // rendered after the end with the transport stopped
    WavFormat format = WavFormat::Pcm24;
    bool includeMetronome = false;
};

struct OfflineRenderResult
{
    bool ok = false;
    std::string error;
    int64_t frames = 0;
    double seconds = 0.0;
    double renderSeconds = 0.0;    // wall-clock time spent
    float samplePeakDb = -144.0f;
    float truePeakDb = -144.0f;
    float integratedLufs = -144.0f;
};

/** progress receives 0..1; return false from it to cancel. */
OfflineRenderResult renderToWav (AudioEngine& engine, const OfflineRenderSettings& settings, const std::string& path,
                                 const std::function<bool (float)>& progress = {});

} // namespace pink
