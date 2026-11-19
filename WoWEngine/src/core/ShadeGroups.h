// Which shade table file fills which slot for each palette group, from LoadShadeTables (vanilla 0x40B6C0).
// In the names, {S} is the side letter (H for Human games, M for Martian) and the extension is added by the
// caller (SHH for 16-bit). Groups hw, hb, hr and the unit tables of bm build their names from unit data and
// aren't listed yet; the front end only needs f1-f7, se and cd.
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
