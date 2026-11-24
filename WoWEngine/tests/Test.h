// The unit tests' own small framework (no library to fetch): TEST(name) { ... } registers a test, CHECK(expr)
// and CHECK_EQ(a, b) record a failure and go on, REQUIRE(expr) stops the test. tests/TestMain.cpp runs them all,
// or the ones whose names contain the argument: wowtests [part of a name].
#pragma once
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace wowtest
{
    struct Case { const char* name; std::function<void()> run; };
    inline std::vector<Case>& Cases() { static std::vector<Case> cases; return cases; }
    inline int& Failures() { static int failures = 0; return failures; }
    struct Register { Register(const char* name, std::function<void()> run) { Cases().push_back({ name, std::move(run) }); } };
    struct Stop {};

    inline void Fail(const char* file, int line, const std::string& what)
    {
        Failures()++;
        std::printf("    FAILED %s:%d: %s\n", file, line, what.c_str());
    }
    template <typename T>
    std::string Show(const T& v)
    {
        if constexpr (requires(std::ostream& o, const T& x) { o << x; }) { std::ostringstream s; s << v; return s.str(); }
        else return "(not printable)";
    }
    template <typename A, typename B>
    std::string Pair(const A& a, const B& b) { return Show(a) + " != " + Show(b); }
}

#define WOWTEST_CAT2(a, b) a##b
#define WOWTEST_CAT(a, b) WOWTEST_CAT2(a, b)
#define TEST(name)                                                                                              \
    static void WOWTEST_CAT(test_, __LINE__)();                                                                 \
    static wowtest::Register WOWTEST_CAT(reg_, __LINE__)(name, WOWTEST_CAT(test_, __LINE__));                   \
    static void WOWTEST_CAT(test_, __LINE__)()
#define CHECK(expr) do { if (!(expr)) wowtest::Fail(__FILE__, __LINE__, #expr); } while (0)
#define CHECK_EQ(a, b) do { const auto& wa_ = (a); const auto& wb_ = (b); if (!(wa_ == wb_)) wowtest::Fail(__FILE__, __LINE__, std::string(#a " == " #b ": ") + wowtest::Pair(wa_, wb_)); } while (0)
#define REQUIRE(expr) do { if (!(expr)) { wowtest::Fail(__FILE__, __LINE__, #expr); throw wowtest::Stop{}; } } while (0)
#define CHECK_THROWS(expr) do { bool thrown_ = false; try { (void)(expr); } catch (...) { thrown_ = true; } if (!thrown_) wowtest::Fail(__FILE__, __LINE__, "no exception from " #expr); } while (0)
