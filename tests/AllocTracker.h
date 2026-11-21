// Counts heap allocations made on the current thread while a ScopedRtCheck is alive.
#pragma once

#include <atomic>

namespace pinktest
{
extern thread_local bool trackAllocations;
extern std::atomic<int> rtAllocations;

struct ScopedRtCheck
{
    ScopedRtCheck() { trackAllocations = true; }
    ~ScopedRtCheck() { trackAllocations = false; }
};
} // namespace pinktest
