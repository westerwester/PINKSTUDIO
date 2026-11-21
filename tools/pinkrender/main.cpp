// pinkrender - renders a PINKSTUDIO session offline to WAV from the command line.
// Usage: pinkrender [--demo] <output.wav> [--sr 48000] [--block 512] [--format 16|24|f32] [--tail 2]
#include "pink/demo/DemoProject.h"
#include "pink/engine/OfflineRenderer.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main (int argc, char** argv)
{
    std::string out;
    pink::OfflineRenderSettings s;

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? std::string (argv[++i]) : std::string(); };
        if (a == "--demo") continue;
        else if (a == "--sr") s.sampleRate = std::atof (next().c_str());
        else if (a == "--block") s.blockSize = std::atoi (next().c_str());
        else if (a == "--tail") s.tailSeconds = std::atof (next().c_str());
        else if (a == "--format")
        {
            const auto f = next();
            s.format = f == "16" ? pink::WavFormat::Pcm16 : (f == "f32" ? pink::WavFormat::Float32 : pink::WavFormat::Pcm24);
        }
        else if (a == "--help" || a == "-h")
        {
            std::printf ("pinkrender [--demo] <output.wav> [--sr 48000] [--block 512] [--format 16|24|f32] [--tail 2]\n");
            return 0;
        }
        else out = a;
    }

    if (out.empty())
    {
        std::fprintf (stderr, "error: missing output path (see --help)\n");
        return 2;
    }

    pink::AudioEngine engine;
    pink::buildDemoProject (engine.getProject());
    engine.prepare (s.sampleRate, s.blockSize);

    const auto r = pink::renderToWav (engine, s, out);
    if (! r.ok)
    {
        std::fprintf (stderr, "render failed: %s\n", r.error.c_str());
        return 1;
    }

    std::printf ("PINKSTUDIO render OK\n");
    std::printf ("  file            : %s\n", out.c_str());
    std::printf ("  length          : %.3f s (%lld frames @ %.0f Hz)\n", r.seconds, (long long) r.frames, s.sampleRate);
    std::printf ("  render time     : %.3f s (%.1fx real time)\n", r.renderSeconds, r.seconds / std::max (1.0e-9, r.renderSeconds));
    std::printf ("  sample peak     : %.2f dBFS\n", r.samplePeakDb);
    std::printf ("  true peak       : %.2f dBTP\n", r.truePeakDb);
    std::printf ("  integrated      : %.2f LUFS\n", r.integratedLufs);
    return 0;
}
