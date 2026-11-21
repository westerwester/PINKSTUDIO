// Test runner + global allocation tracker used to prove that the audio callback never allocates.
#include "TestFramework.h"
#include "AllocTracker.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <new>
#if defined(_WIN32)
 #include <malloc.h>
#endif

namespace pinktest
{
thread_local bool trackAllocations = false;
std::atomic<int> rtAllocations { 0 };
} // namespace pinktest

static void* pinkAlloc (std::size_t n)
{
    if (pinktest::trackAllocations)
        pinktest::rtAllocations.fetch_add (1);
    if (void* p = std::malloc (n ? n : 1))
        return p;
    throw std::bad_alloc();
}

static void* pinkAlignedAlloc (std::size_t n, std::size_t align)
{
    if (pinktest::trackAllocations)
        pinktest::rtAllocations.fetch_add (1);
   #if defined(_WIN32)
    if (void* p = _aligned_malloc (n ? n : 1, align))
        return p;
   #else
    void* p = nullptr;
    if (posix_memalign (&p, std::max (align, sizeof (void*)), n ? n : 1) == 0)
        return p;
   #endif
    throw std::bad_alloc();
}

static void pinkAlignedFree (void* p)
{
   #if defined(_WIN32)
    _aligned_free (p);
   #else
    std::free (p);
   #endif
}

void* operator new (std::size_t n) { return pinkAlloc (n); }
void* operator new[] (std::size_t n) { return pinkAlloc (n); }
void* operator new (std::size_t n, std::align_val_t a) { return pinkAlignedAlloc (n, (std::size_t) a); }
void* operator new[] (std::size_t n, std::align_val_t a) { return pinkAlignedAlloc (n, (std::size_t) a); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
void operator delete (void* p, std::align_val_t) noexcept { pinkAlignedFree (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { pinkAlignedFree (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { pinkAlignedFree (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { pinkAlignedFree (p); }

int main (int argc, char** argv)
{
    std::string filter, reportPath;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp (argv[i], "--report") == 0 && i + 1 < argc) reportPath = argv[++i];
        else filter = argv[i];
    }

    struct Outcome { std::string name; bool passed; };
    std::vector<Outcome> outcomes;
    int passed = 0, failed = 0;

    for (auto& t : pinktest::registry())
    {
        if (! filter.empty() && t.name.find (filter) == std::string::npos)
            continue;

        pinktest::currentTest() = t.name;
        const int before = pinktest::failures();
        std::printf ("[ RUN  ] %s\n", t.name.c_str());
        try
        {
            t.fn();
        }
        catch (const std::exception& e)
        {
            pinktest::fail (__FILE__, __LINE__, std::string ("exception: ") + e.what());
        }
        const bool ok = pinktest::failures() == before;
        std::printf ("[ %s ] %s\n", ok ? " OK " : "FAIL", t.name.c_str());
        outcomes.push_back ({ t.name, ok });
        (ok ? passed : failed)++;
    }

    std::printf ("\n%d passed, %d failed\n", passed, failed);

    if (! reportPath.empty())
    {
        std::ofstream r (reportPath);
        r << "| Test | Result |\n|---|---|\n";
        for (auto& o : outcomes)
            r << "| `" << o.name << "` | " << (o.passed ? "PASS" : "**FAIL**") << " |\n";
        r << "\n| Test | Measurement | Value |\n|---|---|---|\n";
        for (auto& m : pinktest::measurements())
            r << "| `" << m.test << "` | " << m.what << " | " << m.value << " |\n";
        r << "\nTotal: " << passed << " passed, " << failed << " failed\n";
    }

    return failed == 0 ? 0 : 1;
}
