// The battle map's ground as the game draws it: WLandscapeView (0x49F240-0x4A0370) with the ISOLib terrain patch
// routines (0x4579A4, 16-bit span 0x45702D). A CLS map carries, after its heights, a pre-drawn patch of texels for
// each half cell (two triangles a cell); each patch is shaded between three of its cell's corner lights through the
// battle map's terrain shade table (slot 20, dat\LANDR<d><letter>.SHH), with a random dither darkening it in
// places (dat\dith.dat, 0x410F80). See docs/BATTLEMAP.md.
#pragma once
#include "core/Landscape.h"
#include "core/Models.h"
#include "core/ShadeTable.h"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace wow
{
    // The CLS tile tables (header 0x34 to 0x40, offsets from 0x44)
    struct TerrainTiles
    {
        uint32_t heightMask = 0xFF;         // header 0x34: the heights the patches line up on (0xF8)
        std::vector<uint16_t> map;          // header 0x38: a patch for each half cell, 0xFFFF none
        std::vector<uint32_t> offsets;      // header 0x3C: each patch's offset into the patch data
        std::vector<uint8_t> patches;       // header 0x40 to the end

        static TerrainTiles Load(const Terrain& cls);
    };

    // ISOLib's dither tables ("DithDat", 0x410F80): 16 sets of four 64-entry tables of level offsets, each a
    // negative multiple of a level (0x200 bytes in 16-bit) or 0. A patch picks a set by its corner's ground (water
    // types 0-3 their own set, land set 8) and a table by its height (smaller patches dither more).
    class DitherTables
    {
    public:
        // dith.dat: 16 descriptors of five ints {set, high, low, cut, unused}; the game then overrides sets 0-3.
        // The values come from the game's random table (0x4025F0), so they differ from run to run as in the game.
        static DitherTables Build(std::span<const uint8_t> dithDat);
        // The level offset at a byte offset into the tables (the game reads them as dwords)
        int LevelAt(int byteOffset) const;

    private:
        std::vector<int32_t> values; // 0x1000 dwords, in levels
    };

    class TerrainView
    {
    public:
        struct Target
        {
            uint32_t* pixels = nullptr;
            int pitch = 0;              // in pixels
            int left = 0, top = 0, right = 0, bottom = 0; // the clip rectangle, right and bottom outside it
        };

        // The fog of war the view darkens by (0x4A0045): a byte for each vertex, the threshold under which it
        // dims and how much of the dimming it keeps (WFogOfWar +0x18, +0x84, +0x9C, +0x90)
        struct Fog
        {
            const uint8_t* data = nullptr;
            int width = 0;
            int threshold = 1;
            float keep = 0;
        };

        // Draws the ground with world (0, 0) at originX, originY (the game's +0x4E / +0x52, the scroll negated).
        // levels is the 256-byte LANDR .INT table (byte_4DA660) that turns a vertex's light into a shade level.
        static void Render(Landscape& land, const TerrainTiles& tiles, const ShadeTable& table, std::span<const uint8_t> levels,
            const DitherTables& dither, const Target& target, int originX, int originY, const Fog* fog = nullptr);
    };
}
