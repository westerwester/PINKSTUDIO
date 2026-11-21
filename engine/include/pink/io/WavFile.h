// PINKSTUDIO - WAV reading and streaming writing (no external dependencies).
// Writer: 16-bit PCM (TPDF dither), 24-bit PCM, 32-bit float.
// Reader: 8/16/24/32-bit PCM, 32/64-bit float, WAVE_FORMAT_EXTENSIBLE.
// AIFF / FLAC / OGG / MP3 are handled by the app layer through JUCE's audio formats.
#pragma once

#include "pink/model/Project.h"

#include <cstdio>
#include <memory>
#include <string>

namespace pink
{
enum class WavFormat { Pcm16, Pcm24, Float32 };

class WavWriter
{
public:
    WavWriter() = default;
    ~WavWriter();
    WavWriter (const WavWriter&) = delete;
    WavWriter& operator= (const WavWriter&) = delete;

    bool open (const std::string& path, double sampleRate, int numChannels, WavFormat format);
    /** channels[c][i] for i in [0, numFrames). Values are clipped to [-1, 1] for PCM formats. */
    bool write (const float* const* channels, int numFrames);
    bool close();
    bool isOpen() const noexcept { return file != nullptr; }
    int64_t getFramesWritten() const noexcept { return framesWritten; }
    const std::string& getError() const noexcept { return error; }

private:
    bool writeHeader();
    std::FILE* file = nullptr;
    double rate = 48000.0;
    int channels = 2;
    WavFormat fmt = WavFormat::Pcm24;
    int64_t framesWritten = 0;
    std::string error;
    uint32_t ditherState = 0x12345678u;
};

/** Loads a WAV file completely into memory. Returns nullptr and fills error on failure. */
std::shared_ptr<SampleData> readWavFile (const std::string& path, std::string& error);

} // namespace pink
