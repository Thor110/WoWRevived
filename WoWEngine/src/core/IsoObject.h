// The battle map's pre-drawn objects (RTypeISO 0x49DDA0, ISOLib 0x4113F0 / 0x457C60): an IOB model is a list of
// patches, each drawn like a ground half cell (0x4579A4) at its own offset, shaded between three light levels with
// texel 0 left out (kind 1). The levels are worked out once, when the renderer is made (0x49DF40), from the sun
// and the patch's normals; point lights (fires, the vehicles' lights) would add to them as it's drawn (0x4111B0,
// not in yet). See docs/BATTLEMAP.md.
#pragma once
#include "core/Landscape.h"
#include "core/Models.h"
#include "core/TerrainView.h"
#include <cstdint>
#include <span>
#include <vector>

namespace wow
{
    struct IsoObject
    {
        // 0x49E1E0: where the model sits against its object's screen point
        static int HalfWidth(const IobModel& m) { return m.width >> 1; }         // +0x2A (and -it, +0x28)
        static int Depth(const IobModel& m);                                     // +0x26: header 0x16 / sqrt 2
        static int OffsetY(const IobModel& m) { return int16_t(Depth(m) >> 1) - m.field12; } // +0x2C

        // 0x49DF40 with brightness 0xFF: three levels for each patch, the sun on its normals (one each, or one for
        // each corner when the model has a normal per point) times ftol(255 x the shade level), through the
        // LANDR .INT table, at least 8
        static std::vector<uint8_t> BaseLights(const IobModel& m, const Landscape::Sun& sun, std::span<const uint8_t> levels);

        // 0x457C60: every patch at (x, y) plus its offset, through the object's shade table, dither set 13
        // (0x49DEA1: DithDat + 0x3400)
        static void Draw(const IobModel& m, std::span<const uint8_t> lights, int x, int y, const ShadeTable& table,
                         const DitherTables& dither, const TerrainView::Target& target);
    };
}
