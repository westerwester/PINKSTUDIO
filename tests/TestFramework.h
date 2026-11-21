// Minimal self-contained test framework for the PINKSTUDIO engine (no dependencies).
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace pinktest
{
struct TestCase
{
    std::string name;
    std::function<void()> fn;
};

struct Measurement
{
    std::string test, what, value;
};

inline std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
inline std::vector<Measurement>& measurements() { static std::vector<Measurement> m; return m; }
inline std::string& currentTest() { static std::string s; return s; }
inline int& failures() { static int f = 0; return f; }

struct Registrar
{
    Registrar (const char* name, std::function<void()> fn) { registry().push_back ({ name, std::move (fn) }); }
};

inline void fail (const char* file, int line, const std::string& msg)
{
    ++failures();
    std::printf ("    FAIL %s:%d: %s\n", file, line, msg.c_str());
}

template <typename T>
inline std::string str (const T& v) { std::ostringstream o; o.precision (6); o << v; return o.str(); }

inline void report (const std::string& what, const std::string& value)
{
    measurements().push_back ({ currentTest(), what, value });
    std::printf ("    %-52s %s\n", what.c_str(), value.c_str());
}
} // namespace pinktest

#define PINK_CONCAT_(a, b) a##b
#define PINK_CONCAT(a, b) PINK_CONCAT_ (a, b)
#define PINK_TEST(name)                                                                     \
    static void name();                                                                     \
    static pinktest::Registrar PINK_CONCAT (registrar_, name) (#name, name);                \
    static void name()

#define CHECK(cond)                                                                         \
    do { if (! (cond)) pinktest::fail (__FILE__, __LINE__, "CHECK(" #cond ")"); } while (0)

#define CHECK_NEAR(a, b, tol)                                                               \
    do {                                                                                    \
        const double va_ = (double) (a), vb_ = (double) (b);                                \
        if (! (std::abs (va_ - vb_) <= (double) (tol)))                                     \
            pinktest::fail (__FILE__, __LINE__, std::string (#a " = ") + pinktest::str (va_) \
                            + ", expected " + pinktest::str (vb_) + " +/- " + pinktest::str (tol)); \
    } while (0)

#define REPORT(what, value) pinktest::report (what, pinktest::str (value))
