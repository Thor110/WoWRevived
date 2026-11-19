// Shade tables (.SHH / .SHM / .SHL in Dat.wow), which is how the game actually colours its graphics.
//
// Every graphics entry in OBJ.ojd has a shade slot. Loading a palette group (LoadShadeTables, vanilla 0x40B6C0,
// called with the group's palette GType 0x2ED-0x2F9) fills the slots with that group's tables, see ShadeGroups.
// A table holds a number of brightness levels; each level maps the 256 sprite colour indices to screen colours.
// Objects pick a level (RType +0x34, 0 by default); menu text uses the [Colours] settings ("Menu Heading" 7,
// "Menu Items" 4). The game picks the table level as data + 1 + (level << 9) in 16-bit mode (0x40B500).
//
// File layout: uint8 level count, then for .SHH (16-bit) count x 256 uint16 RGB565 colours.
// (.SHM is the 15-bit version, .SHL the 8-bit one with a palette index per entry.)
#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace wow
{
    class ShadeTable
    {
    public:
        // Reads a .SHH file. Throws FormatError if it's cut short.
        static ShadeTable LoadShh(std::span<const uint8_t> data);

        int Levels() const { return levels; }
        // 256 XRGB8888 colours for a level, clamped to the levels the table has.
        const uint32_t* Level(int level) const;

    private:
        int levels = 0;
        std::vector<uint32_t> colours;
    };
}
