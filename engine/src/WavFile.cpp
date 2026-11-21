#include "pink/io/WavFile.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <vector>

namespace pink
{
namespace
{
void putU16 (std::vector<uint8_t>& b, uint32_t v) { b.push_back ((uint8_t) v); b.push_back ((uint8_t) (v >> 8)); }
void putU32 (std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back ((uint8_t) (v >> (8 * i))); }
void putTag (std::vector<uint8_t>& b, const char* t) { b.insert (b.end(), t, t + 4); }

uint16_t getU16 (const uint8_t* p) { return (uint16_t) (p[0] | (p[1] << 8)); }
uint32_t getU32 (const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }

int bytesPerSample (WavFormat f) { return f == WavFormat::Pcm16 ? 2 : (f == WavFormat::Pcm24 ? 3 : 4); }

struct FileCloser { void operator() (std::FILE* f) const { if (f) std::fclose (f); } };

/** Opens a file from a UTF-8 path (Windows needs the wide API for non-ASCII names, e.g. accented user names). */
std::FILE* openUtf8 (const std::string& path, bool write)
{
   #if defined(_WIN32)
    const std::filesystem::path p (std::u8string (path.begin(), path.end()));
    return _wfopen (p.c_str(), write ? L"wb" : L"rb");
   #else
    return std::fopen (path.c_str(), write ? "wb" : "rb");
   #endif
}
} // namespace

WavWriter::~WavWriter() { close(); }

bool WavWriter::open (const std::string& path, double sampleRate, int numChannels, WavFormat format)
{
    close();
    error.clear();
    rate = sampleRate;
    channels = std::clamp (numChannels, 1, 32);
    fmt = format;
    framesWritten = 0;
    file = openUtf8 (path, true);
    if (file == nullptr)
    {
        error = "Cannot open " + path + " for writing";
        return false;
    }
    return writeHeader();
}

bool WavWriter::writeHeader()
{
    const int bps = bytesPerSample (fmt);
    const uint64_t dataBytes = (uint64_t) framesWritten * (uint64_t) channels * (uint64_t) bps;
    std::vector<uint8_t> h;
    putTag (h, "RIFF");
    putU32 (h, (uint32_t) std::min<uint64_t> (0xFFFFFFFFull, 36 + dataBytes));
    putTag (h, "WAVE");
    putTag (h, "fmt ");
    putU32 (h, 16);
    putU16 (h, fmt == WavFormat::Float32 ? 3 : 1);
    putU16 (h, (uint32_t) channels);
    putU32 (h, (uint32_t) std::lround (rate));
    putU32 (h, (uint32_t) (std::lround (rate) * channels * bps));
    putU16 (h, (uint32_t) (channels * bps));
    putU16 (h, (uint32_t) (bps * 8));
    putTag (h, "data");
    putU32 (h, (uint32_t) std::min<uint64_t> (0xFFFFFFFFull, dataBytes));

    if (std::fseek (file, 0, SEEK_SET) != 0 || std::fwrite (h.data(), 1, h.size(), file) != h.size())
    {
        error = "Failed to write WAV header";
        return false;
    }
    return true;
}

bool WavWriter::write (const float* const* data, int numFrames)
{
    if (file == nullptr)
        return false;

    const int bps = bytesPerSample (fmt);
    std::vector<uint8_t> out ((size_t) numFrames * (size_t) channels * (size_t) bps);
    size_t p = 0;

    for (int i = 0; i < numFrames; ++i)
    {
        for (int c = 0; c < channels; ++c)
        {
            const float x = data[c][i];
            if (fmt == WavFormat::Float32)
            {
                uint32_t bits;
                std::memcpy (&bits, &x, 4);
                for (int k = 0; k < 4; ++k) out[p++] = (uint8_t) (bits >> (8 * k));
            }
            else if (fmt == WavFormat::Pcm16)
            {
                // TPDF dither (+-1 LSB)
                auto rnd = [this] { ditherState ^= ditherState << 13; ditherState ^= ditherState >> 17; ditherState ^= ditherState << 5;
                                    return (double) ditherState / 4294967296.0; };
                const double d = (rnd() - rnd());
                const double v = std::clamp ((double) x * 32767.0 + d, -32768.0, 32767.0);
                const int s = (int) std::lround (v);
                out[p++] = (uint8_t) s;
                out[p++] = (uint8_t) (s >> 8);
            }
            else
            {
                const double v = std::clamp ((double) x * 8388607.0, -8388608.0, 8388607.0);
                const int s = (int) std::lround (v);
                out[p++] = (uint8_t) s;
                out[p++] = (uint8_t) (s >> 8);
                out[p++] = (uint8_t) (s >> 16);
            }
        }
    }

    if (std::fwrite (out.data(), 1, out.size(), file) != out.size())
    {
        error = "Disk write failed";
        return false;
    }
    framesWritten += numFrames;
    return true;
}

bool WavWriter::close()
{
    if (file == nullptr)
        return true;
    bool ok = writeHeader();
    ok = std::fclose (file) == 0 && ok;
    file = nullptr;
    return ok;
}

// ---------------------------------------------------------------------------
std::shared_ptr<SampleData> readWavFile (const std::string& path, std::string& error)
{
    std::unique_ptr<std::FILE, FileCloser> f (openUtf8 (path, false));
    if (! f)
    {
        error = "Cannot open " + path;
        return nullptr;
    }

    std::vector<uint8_t> bytes;
    {
        uint8_t chunk[65536];
        size_t got;
        while ((got = std::fread (chunk, 1, sizeof (chunk), f.get())) > 0)
            bytes.insert (bytes.end(), chunk, chunk + got);
    }

    if (bytes.size() < 12 || std::memcmp (bytes.data(), "RIFF", 4) != 0 || std::memcmp (bytes.data() + 8, "WAVE", 4) != 0)
    {
        error = "Not a RIFF/WAVE file";
        return nullptr;
    }

    int format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* data = nullptr;
    size_t dataSize = 0;

    size_t pos = 12;
    while (pos + 8 <= bytes.size())
    {
        const uint8_t* ck = bytes.data() + pos;
        const uint32_t size = getU32 (ck + 4);
        const size_t body = pos + 8;
        const size_t available = std::min<size_t> (size, bytes.size() - body);

        if (std::memcmp (ck, "fmt ", 4) == 0 && available >= 16)
        {
            format = getU16 (ck + 8);
            channels = getU16 (ck + 10);
            rate = getU32 (ck + 12);
            bits = getU16 (ck + 22);
            if (format == 0xFFFE && available >= 26)
                format = getU16 (ck + 8 + 24); // first two bytes of the SubFormat GUID
        }
        else if (std::memcmp (ck, "data", 4) == 0)
        {
            data = bytes.data() + body;
            dataSize = available;
        }

        pos = body + size + (size & 1u);
    }

    if (data == nullptr || channels <= 0 || rate == 0)
    {
        error = "Missing fmt or data chunk";
        return nullptr;
    }

    const bool isFloat = format == 3;
    if (! (format == 1 || isFloat) || ! (bits == 8 || bits == 16 || bits == 24 || bits == 32 || (isFloat && bits == 64)))
    {
        error = "Unsupported WAV encoding (format " + std::to_string (format) + ", " + std::to_string (bits) + " bit)";
        return nullptr;
    }

    const int bps = bits / 8;
    const int64_t frames = (int64_t) (dataSize / (size_t) (bps * channels));
    auto sd = std::make_shared<SampleData>();
    sd->sampleRate = rate;
    sd->numChannels = channels;
    sd->numFrames = frames;
    sd->sourcePath = path;
    sd->channels.assign ((size_t) channels, std::vector<float> ((size_t) frames));

    for (int64_t i = 0; i < frames; ++i)
    {
        for (int c = 0; c < channels; ++c)
        {
            const uint8_t* s = data + ((size_t) i * (size_t) channels + (size_t) c) * (size_t) bps;
            float v = 0.0f;
            if (isFloat && bits == 32) { float x; std::memcpy (&x, s, 4); v = x; }
            else if (isFloat && bits == 64) { double x; std::memcpy (&x, s, 8); v = (float) x; }
            else if (bits == 8)  v = ((int) s[0] - 128) / 128.0f;
            else if (bits == 16) v = (int16_t) getU16 (s) / 32768.0f;
            else if (bits == 24) { int32_t x = (int32_t) ((uint32_t) s[0] << 8 | (uint32_t) s[1] << 16 | (uint32_t) s[2] << 24) >> 8; v = (float) x / 8388608.0f; }
            else if (bits == 32) v = (float) ((double) (int32_t) getU32 (s) / 2147483648.0);
            sd->channels[(size_t) c][(size_t) i] = v;
        }
    }

    return sd;
}

} // namespace pink
