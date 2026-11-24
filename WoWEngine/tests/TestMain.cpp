#include "Test.h"
#include <cstring>
#include <exception>

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    for (const wowtest::Case& c : wowtest::Cases())
    {
        if (only && !std::strstr(c.name, only)) continue;
        const int before = wowtest::Failures();
        try { c.run(); }
        catch (const wowtest::Stop&) {}
        catch (const std::exception& e) { wowtest::Fail(c.name, 0, std::string("exception: ") + e.what()); }
        catch (...) { wowtest::Fail(c.name, 0, "unknown exception"); }
        std::printf("%s %s\n", wowtest::Failures() == before ? "ok    " : "FAILED", c.name);
        run++;
    }
    std::printf("%d tests, %d failed checks\n", run, wowtest::Failures());
    return run == 0 || wowtest::Failures() ? 1 : 0;
}
