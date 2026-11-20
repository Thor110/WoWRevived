// Which shade table file fills which slot for each palette group, from LoadShadeTables (vanilla 0x40B6C0).
// In the names, {S} is the side letter (H for Human games, M for Martian) and the extension is added by the
// caller (SHH for 16-bit). The slots are the game's table at 0x4DA780 (4 bytes a slot). The unit tables of bm
// build their names from unit data and aren't listed yet.
#pragma once
#include <string_view>

namespace wow
{
    struct ShadeGroup
    {
        int paletteGType;
        std::string_view files[12];
        int slots[12];
        int count;
    };

    inline constexpr ShadeGroup ShadeGroups[] = {
        // hw, the war map (0x40BCF2): the map in slot 6, its HUD in 5, fonts in 9
        { 0x2ED, { "{S}W{S}W", "{S}W{S}I", "{S}WGI" }, { 6, 5, 9 }, 3 },
        // hb, the Briefing and Debriefing (0x40BE05): the backdrop in slot 8, the interface in 5, fonts in 9 and the
        // mini battle maps' table (MINIT) in 21
        { 0x2EE, { "{S}B{S}B", "{S}B{S}I", "{S}BGI", "{S}MINIT" }, { 8, 5, 9, 21 }, 4 },
        // hr, the research screen (0x40BF79): backdrop in slot 7, interface in 5, fonts in 9
        { 0x2EF, { "{S}R{S}R", "{S}R{S}I", "{S}RGI" }, { 7, 5, 9 }, 3 },
        // bm, the battle map (0x40C08D): the interface in slot 5, fonts in 9, explosions in 11. *Not yet*: the tables
        // named by the hour's letter and the LANDR digit (BMHB 1, BMHV 2, BMMV 3, BMMB 4, BMR<d>NA 10, the ground's
        // LANDR 20, which the battle map loads itself) and {S}MINIB in 21
        { 0x2F0, { "BM{S}I", "BMGI", "BMEX", "{S}MINIB" }, { 5, 9, 11, 21 }, 4 },
        { 0x2F1, { "F1F1", "F1{S}I", "F1GI" }, { 12, 5, 9 }, 3 },
        { 0x2F2, { "F2F2", "F2{S}I" }, { 13, 5 }, 2 },
        { 0x2F3, { "F3F3", "F3{S}I" }, { 14, 5 }, 2 },
        { 0x2F4, { "F4F4", "F4{S}I" }, { 15, 5 }, 2 },
        { 0x2F5, { "F5F5", "F5{S}I" }, { 16, 5 }, 2 },
        { 0x2F6, { "F6F6", "F6{S}I", "F6GI" }, { 17, 5, 9 }, 3 },
        { 0x2F7, { "F7F7", "F7{S}I", "F7GI" }, { 18, 5, 9 }, 3 },
        { 0x2F8, { "SESE", "SE{S}I", "SEGI" }, { 19, 5, 9 }, 3 },
        { 0x2F9, { "cdsepia", "cd1", "cd2", "cd3", "cd4", "cd5", "cd6", "cd7", "cd8", "cd9", "cd10" },
                 { 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 }, 11 },
    };
}
