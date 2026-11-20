// The game's random numbers, as its C runtime made them: MSVC's rand() (x = x * 214013 + 2531011, 15 bits out),
// seeded with srand(timeGetTime()) at start-up (WinMain 0x401AE8) and srand(1) whenever the war map is built
// (0x417DA3), so a campaign's war map draws the same numbers each time. At start-up the game also fills a table
// of 1024 rand() values (0x402780 into dword_4D14A8) that Random(n) (0x4025F0) goes through in turn, giving
// table[i++ & 1023] % (n + 1): 0 to n, both included. Vehicles, puppets and the ISO library use the table; the
// war map (pods 0x41A360, music 0x41B17C, 0x422738, 0x42B34B) and the battle map set-up call rand() directly.
#pragma once
#include <array>
#include <cstdint>

namespace wow::random
{
    inline uint32_t& State() { static uint32_t state = 1; return state; }
    inline void Seed(uint32_t seed) { State() = seed; }                 // srand
    inline int Rand()                                                    // rand: 0-32767
    {
        State() = State() * 214013u + 2531011u;
        return int((State() >> 16) & 0x7FFF);
    }

    struct Table
    {
        std::array<int, 1024> values{};
        uint32_t next = 0;                // dword_4D2580
    };
    inline Table& GameTable() { static Table t; return t; }
    // Fills the table from rand() (WGame 0x402630).
    inline void FillTable()
    {
        Table& t = GameTable();
        for (int& v : t.values) v = Rand();
        t.next = 0;
    }
    // 0x4025F0: the next table value, 0 to n.
    inline int Random(int n)
    {
        Table& t = GameTable();
        int v = t.values[t.next++ & 1023];
        return n >= 0 ? v % (n + 1) : 0;
    }
}
