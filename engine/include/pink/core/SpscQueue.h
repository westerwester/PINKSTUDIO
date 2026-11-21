// PINKSTUDIO - wait-free single-producer / single-consumer ring buffer.
// Used to send commands from the message thread to the audio thread and
// analysis data back. Capacity must be a power of two.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace pink
{
template <typename T, size_t Capacity>
class SpscQueue
{
    static_assert ((Capacity & (Capacity - 1)) == 0 && Capacity >= 2, "Capacity must be a power of two");
    static_assert (std::is_trivially_copyable_v<T>, "SpscQueue only stores trivially copyable types");

public:
    /** Producer side. Returns false if the queue is full (the item is dropped). */
    bool push (const T& item) noexcept
    {
        const size_t head = writeIndex.load (std::memory_order_relaxed);
        const size_t next = (head + 1) & mask;
        if (next == readIndex.load (std::memory_order_acquire))
            return false;

        items[head] = item;
        writeIndex.store (next, std::memory_order_release);
        return true;
    }

    /** Consumer side. Returns false if empty. */
    bool pop (T& out) noexcept
    {
        const size_t tail = readIndex.load (std::memory_order_relaxed);
        if (tail == writeIndex.load (std::memory_order_acquire))
            return false;

        out = items[tail];
        readIndex.store ((tail + 1) & mask, std::memory_order_release);
        return true;
    }

    size_t sizeApprox() const noexcept
    {
        return (writeIndex.load (std::memory_order_acquire) - readIndex.load (std::memory_order_acquire)) & mask;
    }

    static constexpr size_t capacity() noexcept { return Capacity - 1; }

private:
    static constexpr size_t mask = Capacity - 1;
    std::array<T, Capacity> items {};
    alignas (64) std::atomic<size_t> writeIndex { 0 };
    alignas (64) std::atomic<size_t> readIndex { 0 };
};

} // namespace pink
