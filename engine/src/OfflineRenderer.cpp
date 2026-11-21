#include "pink/engine/OfflineRenderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace pink
{
OfflineRenderResult renderToWav (AudioEngine& engine, const OfflineRenderSettings& s, const std::string& path,
                                 const std::function<bool (float)>& progress)
{
    OfflineRenderResult result;
    const auto wallStart = std::chrono::steady_clock::now();

    const auto before = engine.getTransportInfo();
    const double previousRate = engine.getSampleRate();
    const int previousBlock = engine.getMaxBlockSize();

    engine.prepare (s.sampleRate, s.blockSize);
    for (auto& t : engine.getProject().tracks)
        if (t.instrument) t.instrument->reset();
    for (int i = 0; i < engine.getProject().mixer.getNumChannels(); ++i)
        for (auto& p : engine.getProject().mixer.channel (i).inserts)
            p->reset();

    const auto& tempo = engine.getProject().tempo;
    const double endPpq = s.endPpq >= 0.0 ? s.endPpq : engine.getProject().getSongEndPpq();
    const double startSec = tempo.ppqToSeconds (s.startPpq);
    const double endSec = tempo.ppqToSeconds (std::max (endPpq, s.startPpq));
    const int64_t musicFrames = (int64_t) std::ceil ((endSec - startSec) * s.sampleRate - 1.0e-6);
    const int64_t tailFrames = (int64_t) std::ceil (std::max (0.0, s.tailSeconds) * s.sampleRate - 1.0e-6);
    const int64_t totalFrames = musicFrames + tailFrames;

    WavWriter writer;
    if (! writer.open (path, s.sampleRate, 2, s.format))
    {
        result.error = writer.getError();
        return result;
    }

    // bring the transport into a known state (commands are consumed by the first block)
    engine.stop();
    engine.stop();
    engine.setLoopEnabled (false);
    engine.setMetronomeEnabled (s.includeMetronome);
    engine.resetLoudnessMeters();
    engine.locate (s.startPpq);
    engine.play();

    std::vector<float> left ((size_t) s.blockSize), right ((size_t) s.blockSize);
    float* outs[2] = { left.data(), right.data() };
    float peak = 0.0f;
    bool stopped = false;
    int64_t done = 0;

    while (done < totalFrames)
    {
        int n = (int) std::min<int64_t> (s.blockSize, totalFrames - done);
        if (! stopped && done + n > musicFrames)
            n = (int) std::max<int64_t> (1, musicFrames - done);
        if (! stopped && done >= musicFrames)
        {
            engine.stop();
            stopped = true;
            n = (int) std::min<int64_t> (s.blockSize, totalFrames - done);
        }

        engine.processBlock (nullptr, 0, outs, 2, n);
        for (int i = 0; i < n; ++i)
            peak = std::max (peak, std::max (std::abs (left[(size_t) i]), std::abs (right[(size_t) i])));

        if (! writer.write (outs, n))
        {
            result.error = writer.getError();
            return result;
        }
        done += n;

        if (progress && ! progress ((float) done / (float) std::max<int64_t> (1, totalFrames)))
        {
            result.error = "Cancelled";
            writer.close();
            return result;
        }
    }

    if (! stopped)
        engine.stop();

    const auto meters = engine.getMasterMeters();
    if (! writer.close())
    {
        result.error = writer.getError();
        return result;
    }

    // drain the stop command and restore the user's transport settings
    engine.processBlock (nullptr, 0, outs, 2, 1);
    engine.setLoop (before.loopStart, before.loopEnd);
    engine.setLoopEnabled (before.loopEnabled);
    engine.setMetronomeEnabled (before.metronome);
    engine.locate (before.ppq);
    engine.processBlock (nullptr, 0, outs, 2, 1);

    if (previousRate > 0.0 && (previousRate != s.sampleRate || previousBlock != s.blockSize))
        engine.prepare (previousRate, previousBlock);

    result.ok = true;
    result.frames = writer.getFramesWritten();
    result.seconds = (double) result.frames / s.sampleRate;
    result.samplePeakDb = gainToDb (peak);
    result.truePeakDb = gainToDb (meters.truePeakMax);
    result.integratedLufs = meters.integratedLufs;
    result.renderSeconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - wallStart).count();
    return result;
}

} // namespace pink
