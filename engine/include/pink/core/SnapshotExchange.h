// PINKSTUDIO - lock-free hand-over of immutable render snapshots.
//
// The message thread builds a new immutable object (e.g. the compiled render graph),
// publishes it, and the audio thread picks it up at the start of the next block.
// Old objects are never deleted on the audio thread: they go into a graveyard and are
// freed by collectGarbage() on the message thread once the audio thread has finished
// at least one complete block after the swap.
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace pink
{
template <typename T>
class SnapshotExchange
{
public:
    SnapshotExchange() = default;
    SnapshotExchange (const SnapshotExchange&) = delete;
    SnapshotExchange& operator= (const SnapshotExchange&) = delete;

    ~SnapshotExchange() { current.store (nullptr); }

    // ---- message thread -------------------------------------------------
    void publish (std::unique_ptr<const T> next)
    {
        current.store (next.get(), std::memory_order_seq_cst);
        const uint64_t completedAtSwap = completedBlocks.load (std::memory_order_seq_cst);

        if (owned != nullptr)
            graveyard.emplace_back (std::move (owned), completedAtSwap);

        owned = std::move (next);
        collectGarbage();
    }

    /** Frees retired snapshots the audio thread can no longer be using. */
    void collectGarbage()
    {
        const uint64_t done = completedBlocks.load (std::memory_order_seq_cst);
        std::erase_if (graveyard, [done] (const auto& entry) { return done > entry.second; });
    }

    /** Frees everything retired. Only call when the audio callback is guaranteed stopped. */
    void collectAllWhileStopped() { graveyard.clear(); }

    const T* getPublished() const noexcept { return owned.get(); }
    size_t getGraveyardSize() const noexcept { return graveyard.size(); }

    // ---- audio thread ---------------------------------------------------
    /** Call once at the start of an audio block. The pointer stays valid until blockFinished(). */
    const T* acquire() const noexcept { return current.load (std::memory_order_seq_cst); }

    /** Call once at the end of every audio block. */
    void blockFinished() noexcept { completedBlocks.fetch_add (1, std::memory_order_seq_cst); }

private:
    std::atomic<const T*> current { nullptr };
    std::atomic<uint64_t> completedBlocks { 0 };
    std::unique_ptr<const T> owned;
    std::vector<std::pair<std::unique_ptr<const T>, uint64_t>> graveyard;
};

} // namespace pink
