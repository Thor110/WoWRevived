// AI.ojd (loader 0x441580). The same record stream as SFX.ojd and OBJ.ojd: 0xFF, a u16 line number, a u16
// opcode, then its arguments.
//   0  u16 id         starts entry id (the table at 0x518B10, 8 bytes an entry)
//   1  s16            the entry's value (+4)
//   2  s16            the entry's weight; the game scales it by a per-side factor (+0x14 of 0x5187F8 in Human
//                     games, 0x51A0D0 in Martian ones) and keeps a running total
//   3  u16 side       0 Human, 1 Martian: whose the rules that follow are
//   4  u16 list       which list the rules that follow go in
//   5  u16 a, b, c    adds a rule {a, b, c}
//   6  u16 a, b, c, d, e   adds a rule with two more words
// Rules for the side being played go in the lists at 0x518994, the other side's in those at 0x518814 (12 bytes
// a list). What the lists and words mean is for the AI to work out; this only reads them.
#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <vector>

namespace wow
{
    struct AiRule
    {
        uint16_t words[5] = {};
        int count = 3;   // 3 (opcode 5) or 5 (opcode 6)
    };

    struct AiEntry
    {
        int value = 0;
        int weight = 0;  // as stored, before the game's per-side scaling
    };

    struct AiTable
    {
        std::map<int, AiEntry> entries;
        // [side][list]: side 0 Human, 1 Martian
        std::map<int, std::vector<AiRule>> rules[2];

        // Throws FormatError on a malformed file.
        static AiTable Load(std::span<const uint8_t> data);
    };
}
