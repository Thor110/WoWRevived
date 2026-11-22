// A note of what the engine is doing, kept for the crash report in WoWEngine.log (main.cpp): a fault with no
// message otherwise leaves a log that just stops. Set at the start of the larger steps; costs a pointer write.
#pragma once

namespace wow
{
    inline const char* crumb = "starting";
    inline void Crumb(const char* what) { crumb = what; }
}
