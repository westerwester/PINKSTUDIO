#include "TestFramework.h"
#include "TestHelpers.h"

#include "pink/core/MidiEvent.h"
#include "pink/core/SnapshotExchange.h"
#include "pink/core/SpscQueue.h"
#include "pink/dsp/Processor.h"
#include "pink/transport/TempoMap.h"
#include "pink/undo/UndoManager.h"

#include <atomic>
#include <random>
#include <thread>

using namespace pink;

PINK_TEST (spsc_queue_basic_wrap_and_full)
{
    SpscQueue<int, 8> q;
    CHECK (q.capacity() == 7);
    for (int round = 0; round < 5; ++round)
    {
        for (int i = 0; i < 7; ++i) CHECK (q.push (i + round * 10));
        CHECK (! q.push (99)); // full
        int v = -1;
        for (int i = 0; i < 7; ++i) { CHECK (q.pop (v)); CHECK (v == i + round * 10); }
        CHECK (! q.pop (v));
    }
}

PINK_TEST (spsc_queue_two_threads_keeps_order)
{
    SpscQueue<int, 1024> q;
    constexpr int N = 500000;
    std::atomic<bool> ok { true };
    std::thread consumer ([&]
    {
        int expected = 0, v;
        while (expected < N)
            if (q.pop (v)) { if (v != expected) ok = false; ++expected; }
    });
    for (int i = 0; i < N;)
        if (q.push (i)) ++i;
    consumer.join();
    CHECK (ok.load());
}

namespace
{
std::atomic<int> liveSnapshots { 0 };
struct DummySnapshot
{
    explicit DummySnapshot (int v) : value (v) { ++liveSnapshots; }
    ~DummySnapshot() { magic = 0; --liveSnapshots; }
    int value;
    int magic = 0x5EED;
};
} // namespace

PINK_TEST (snapshot_exchange_concurrent_publish_never_frees_in_use)
{
    {
        SnapshotExchange<DummySnapshot> ex;
        ex.publish (std::make_unique<DummySnapshot> (0));
        std::atomic<bool> running { true }, corrupted { false };
        std::atomic<long> blocks { 0 };

        std::thread audio ([&]
        {
            while (running.load())
            {
                const DummySnapshot* s = ex.acquire();
                for (int k = 0; k < 50; ++k)
                    if (s == nullptr || s->magic != 0x5EED) corrupted = true;
                ex.blockFinished();
                ++blocks;
            }
        });

        for (int i = 1; i <= 20000; ++i)
        {
            ex.publish (std::make_unique<DummySnapshot> (i));
            if ((i & 63) == 0) std::this_thread::yield();
        }
        // let the audio thread complete a few more blocks, then everything retired can go
        const long target = blocks.load() + 3;
        while (blocks.load() < target) std::this_thread::yield();
        ex.collectGarbage();
        running = false;
        audio.join();

        CHECK (! corrupted.load());
        CHECK (ex.getGraveyardSize() == 0);
        CHECK (liveSnapshots.load() == 1);
        REPORT ("audio blocks run during 20000 publishes", blocks.load());
    }
    CHECK (liveSnapshots.load() == 0);
}

PINK_TEST (midi_event_list_is_sorted_and_stable)
{
    MidiEventList l;
    l.add (MidiEvent::noteOn (10, 0, 60, 100));
    l.add (MidiEvent::noteOn (5, 0, 61, 100));
    l.add (MidiEvent::noteOff (10, 0, 62));
    l.add (MidiEvent::noteOn (0, 0, 63, 100));
    CHECK (l.size() == 4);
    CHECK (l[0].data1 == 63);
    CHECK (l[1].data1 == 61);
    CHECK (l[2].data1 == 60); // same offset keeps insertion order
    CHECK (l[3].data1 == 62);
    for (int i = 0; i < MidiEventList::kCapacity + 10; ++i) l.add (MidiEvent::noteOn (1, 0, 1, 1));
    CHECK (l.size() == MidiEventList::kCapacity);
}

PINK_TEST (parameter_mapping_and_clamping)
{
    ParameterSet ps;
    auto& f = ps.add ("freq", "Freq", 20.0f, 20000.0f, 1000.0f, "Hz", Parameter::Scale::Logarithmic);
    CHECK_NEAR (f.fromNormalised (0.5f), std::sqrt (20.0 * 20000.0), 1.0);
    CHECK_NEAR (f.toNormalised (20000.0f), 1.0, 1e-6);
    f.set (1.0e9f);
    CHECK_NEAR (f.get(), 20000.0, 1e-3);
    f.set (std::nanf (""));
    CHECK_NEAR (f.get(), 1000.0, 1e-3); // NaN falls back to the default
    auto& c = ps.addChoice ("mode", "Mode", { "A", "B", "C" }, 1);
    c.set (1.6f);
    CHECK (c.getInt() == 2);
    CHECK (ps.find ("mode") == &c);
    CHECK (! ps.set ("missing", 1.0f));
}

PINK_TEST (undo_redo_transactions_and_budget)
{
    UndoManager um;
    int value = 0;
    um.perform ([&] { value += 1; }, [&] { value -= 1; }, "Add 1");
    um.perform ([&] { value += 10; }, [&] { value -= 10; }, "Add 10");
    CHECK (value == 11);
    um.undo();
    CHECK (value == 1);
    um.redo();
    CHECK (value == 11);

    // grouped transaction undoes as one unit
    um.beginNewTransaction ("Group");
    um.perform (std::make_unique<LambdaAction> ([&] { value *= 2; }, [&] { value /= 2; }));
    um.perform (std::make_unique<LambdaAction> ([&] { value += 3; }, [&] { value -= 3; }));
    CHECK (value == 25);
    um.undo();
    CHECK (value == 11);
    CHECK (um.getRedoDescription() == "Group");

    // a new action clears redo
    um.perform ([&] { value = 0; }, [&] { value = 11; }, "Reset");
    CHECK (! um.canRedo());
    um.undo();
    CHECK (value == 11);

    UndoManager small (64 * 3);
    for (int i = 0; i < 10; ++i) small.perform ([] {}, [] {}, "x" + std::to_string (i));
    CHECK (small.getNumUndoTransactions() == 3);
}

PINK_TEST (tempo_map_conversions)
{
    TempoMap m;
    m.setConstantTempo (120.0);
    CHECK_NEAR (m.ppqToSeconds (1.0), 0.5, 1e-12);
    CHECK_NEAR (m.secondsToPpq (3.0), 6.0, 1e-12);

    m.setTempoAt (4.0, 60.0);           // 4 beats @120 (2 s), then 60 BPM
    CHECK_NEAR (m.ppqToSeconds (6.0), 4.0, 1e-12);
    CHECK_NEAR (m.bpmAt (3.99), 120.0, 1e-12);
    CHECK_NEAR (m.bpmAt (4.0), 60.0, 1e-12);
    m.setTempoAt (8.0, 240.0);          // 4 beats @60 (4 s) -> 6 s at ppq 8
    CHECK_NEAR (m.ppqToSeconds (10.0), 6.5, 1e-12);

    std::mt19937 rng (7);
    std::uniform_real_distribution<double> d (0.0, 100.0);
    double worst = 0.0;
    for (int i = 0; i < 10000; ++i)
    {
        const double p = d (rng);
        worst = std::max (worst, std::abs (m.secondsToPpq (m.ppqToSeconds (p)) - p));
    }
    CHECK (worst < 1e-9);
    REPORT ("worst PPQ round-trip error", worst);
}

PINK_TEST (time_signature_bars_and_beats)
{
    TempoMap m;
    m.setTimeSignature (4, 4);
    m.setTimeSignatureAt (2, 7, 8);     // bars 0-1 in 4/4, then 7/8
    CHECK_NEAR (m.barToPpq (2), 8.0, 1e-12);
    CHECK_NEAR (m.barToPpq (3), 11.5, 1e-12);

    const auto bbt = m.ppqToBarBeatTick (9.25);
    CHECK (bbt.bar == 2);
    CHECK (bbt.beat == 2);              // eighth-note beats: (9.25 - 8) / 0.5 = 2.5
    CHECK (bbt.tick == 480);
    CHECK (bbt.numerator == 7 && bbt.denominator == 8);

    CHECK_NEAR (m.nextBeatAtOrAfter (8.1), 8.5, 1e-12);
    CHECK_NEAR (m.nextBeatAtOrAfter (7.2), 8.0, 1e-12);
    CHECK (m.isBarStart (11.5));
    CHECK (! m.isBarStart (11.0));
    CHECK (m.ppqToBarBeatTick (11.5).bar == 3);
}
