// The battle map's 3D models (RTypeWOF 0x49DA50, WOFLib 0x49E330-0x49F230): a WOF instance posed at a world point
// and heading, its pieces walked down from the root with 4.12 fixed-point matrices, the points projected like the
// rest of the battle map, the pieces drawn back to front with each face lit by the sun. See docs/BATTLEMAP.md.
#pragma once
#include "core/Landscape.h"
#include "core/ShadeTable.h"
#include "core/TerrainView.h"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace wow
{
    class WofInstance
    {
    public:
        // data is the whole WOF file; scale and groundFollow are the GType's OBJ.ojd record 6 float and first word
        // (0x49DCA0: the three scales x the float; +0x60 the word)
        WofInstance(std::vector<uint8_t> data, float scale, int groundFollow);

        // 0x49E690: poses the model at world (x, y, z), turned to heading (4096 a turn), tilted to the ground's
        // normal there unless upright or the GType says not (0x4A3020)
        void Transform(const Landscape& land, int heading, int x, int y, int z, bool upright = false);

        // 0x49EC00 / 0x49EEF0: the pieces in depth order, each face lit and drawn at the view offset
        void Draw(const Landscape::Sun& sun, const ShadeTable& table, const TerrainView::Target& target, int ox, int oy) const;

        // Damage variants: a piece whose flags' low two bits are neither 0 nor this is hidden (+0x5E, 1 at first)
        void SetVariant(int v);

        // The projected bounds after Transform (+0x68..+0x72), relative to the anchor
        int MinX() const { return boundMin[0]; }
        int MaxX() const { return boundMax[0]; }

    private:
        struct Piece
        {
            uint8_t flags = 0;
            int vertexCount = 0, faceCount = 0;
            int16_t pivot[3] = {};
            uint32_t vertexOffset = 0, faceOffset = 0;
            std::vector<int> children;
        };
        std::vector<uint8_t> file;
        std::vector<Piece> pieces;
        int root = 0;
        int sx = 1, sy = 1, sz = 1;          // +0x3C..+0x44
        int groundFollow = 0;                // +0x60
        uint32_t vertices = 0, materials = 0, texture = 0, smooth = 0;
        std::vector<bool> hidden;            // WorldPts' byte for each piece
        // per piece: its world points (x, y, z) and screen points (x, y, depth), and its nearest depth
        std::vector<std::vector<std::array<int32_t, 3>>> world;
        std::vector<std::vector<std::array<int16_t, 3>>> screen;
        std::vector<int> nearest;
        int boundMin[3] = {}, boundMax[3] = {};
        int32_t m[9] = {};                   // unk_59FD70
        int32_t t[3] = {};                   // dword_59FD94..9C
        int topY = 0;

        void Walk(int piece, int ox, int oy, int oz);
        int FaceLight(const std::array<int32_t, 3>& a, const std::array<int32_t, 3>& b, const std::array<int32_t, 3>& c,
                      const Landscape::Sun& sun) const;
    };
}
