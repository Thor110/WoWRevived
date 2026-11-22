// A note of what the engine is doing, kept for the crash report in WoWEngine.log (main.cpp): a fault with no
// message otherwise leaves a log that just stops. Set at the start of the larger steps; costs a pointer write.
// The heartbeat is counted once a pass of the main loop; main.cpp's watchdog writes a STALL line, with the crumb, when
// it stops for five seconds (a hang closes the window with nothing said either).
#pragma once
#include <atomic>

namespace wow
{
    inline std::atomic<const char*> crumb{ "starting" };
    inline void Crumb(const char* what) { crumb.store(what, std::memory_order_relaxed); }
    inline std::atomic<unsigned> heartbeat{ 0 };
    inline void Beat() { heartbeat.fetch_add(1, std::memory_order_relaxed); }
}
