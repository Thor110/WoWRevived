// The battle map's pre-drawn objects (RTypeISO 0x49DDA0, ISOLib 0x4113F0 / 0x457C60): an IOB model is a list of
// patches, each drawn like a ground half cell (0x4579A4) at its own offset, shaded between three light levels with
// texel 0 left out (kind 1). The levels are worked out once, when the renderer is made (0x49DF40), from the sun
// and the patch's normals; the two nearest point lights (fires, lamps, the vehicles' lights) add to a copy of them
// as it's drawn (0x4113F0 / 0x4111B0). See docs/BATTLEMAP.md.
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
        static std::vector<uint8_t> BaseLights(const IobModel& m, const Landscape::Sun& sun, std::span<const uint8_t> levels, int level = 255);

        // 0x4111B0: one point light added to a copy of the levels: for each patch, the light (0x40C930 from the light
        // to the object, dx / dy / dz the object less the light, with its normal's bytes 0 / 2 / 1, out to the light's
        // range from `inner`) x the intensity, added to each corner's level (with a normal for each point, each
        // corner its own; else the patch's one for all three), at most the cap
        static void AddPointLight(const IobModel& m, std::vector<uint8_t>& lights, int dx, int dy, int dz, int intensity,
                                  int range, int inner, int cap);

        // 0x457C60: every patch at (x, y) plus its offset, through the object's shade table, dither set 13
        // (0x49DEA1: DithDat + 0x3400)
        static void Draw(const IobModel& m, std::span<const uint8_t> lights, int x, int y, const ShadeTable& table,
                         const DitherTables& dither, const TerrainView::Target& target);
    };
}
