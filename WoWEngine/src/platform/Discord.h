// MODERN: Discord rich presence, as WoWRevived has it (WoWPatches/_inmm/winmm_driver9x.cpp): discord-rpc.dll is
// loaded if it is next to the program, and the presence says where the player is (the menus, a Human or a
// Martian campaign, the CD player and its track) in the game's language, with the time the session began.
// The same application id, texts and image keys as WoWRevived, so both show the same thing.
// Windows only (the DLL is a Windows one; it must be the 64-bit discord-rpc.dll, as the engine is 64-bit:
// WoWRevived's own copy is 32-bit and won't load). Anywhere else, and without the DLL, it does nothing.
#pragma once
#include <string>

namespace wow
{
    class Discord
    {
    public:
        enum class State { Menu, Human, Martian, Network, CdPlayer };

        ~Discord();
        // language: the first line of the install's language.txt ("English", "French", "German", "Italian", "Spanish")
        void Start(const std::string& language);
        // Once a game update. track: the CD track playing (2 to 5), for the CD player; human: the side's
        // soundtrack (and, in a network game, the side played).
        void Update(State state, int track, bool human);
        void Stop();
        bool Active() const { return started; }

    private:
        bool started = false;
        int language = 0;
        int lastState = -1, lastTrack = -1;
        unsigned long long lastSent = 0, lastCallbacks = 0;
        long long sessionStart = 0;
        void Send(State state, int track, bool human);
    };
}
