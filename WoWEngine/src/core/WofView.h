// The battle map's 3D models (RTypeWOF 0x49DA50, WOFLib 0x49E330-0x49F230): a WOF instance posed at a world point
// and heading, its pieces walked down from the root with 4.12 fixed-point matrices, the points projected like the
// rest of the battle map, the pieces drawn back to front with each face lit by the sun. See docs/BATTLEMAP.md.
#pragma once
#include "core/Bridge.h"
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

        // Animated models (header 0x1C, the frame count, not 0; 0x49E330 sets +0x62 from it): each piece has a key
        // per frame, its 4.12 3x3 turn as nine words, and the root's key (32 bytes, the others 20) also holds its
        // place as three ints, x 16384 / header 0x14 (+0x38). The frame (+0x5C) is the object's +0xA8 (0x47F630).
        int Frames() const { return frames; }

        // A piece's first world point from the last pose (WorldPts' entry, +8), false if it has none
        bool FirstPoint(int piece, int& x, int& y) const
        {
            if (piece < 0 || size_t(piece) >= world.size() || world[size_t(piece)].empty()) return false;
            x = world[size_t(piece)][0][0];
            y = world[size_t(piece)][0][1];
            return true;
        }
        // WorldPts' byte for a piece: hidden or not (the walkers' feet in water, 0x48FD50)
        void SetHidden(int piece, bool h) { if (piece >= 0 && size_t(piece) < hidden.size()) hidden[size_t(piece)] = h; }
        void SetFrame(int f) { frame = frames > 0 ? ((f % frames) + frames) % frames : 0; }

        // A bridge segment's ends (0x497E00 its start, 0x497AC0 its end): the nine pieces' four points each set from
        // the joint's six, world and screen alike, then the bounds widened to them (0x498110). The pieces' depth
        // order stays as Transform left it.
        void Stitch(const BridgeJoint& joint, bool end);

        // The projected bounds after Transform (+0x68..+0x72), relative to the anchor
        int MinX() const { return boundMin[0]; }
        int MaxX() const { return boundMax[0]; }
        int MaxY() const { return boundMax[1]; }     // +0x70

        // Drawn as a shadow (its instance's +0x48 0x20, 0x48E700): each pixel darkened through this table
        // (dat\b16.hsh, 64 rows of 256 words) by the texel, nothing of the texture's own colours
        void SetShadow(const uint16_t* table) { shadowTable = table; }
        // The x scale (+0x3C) times a factor, ftol'd (0x48E72F)
        void ScaleX(float f) { sx = int(float(sx) * f); }

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
        const uint16_t* shadowTable = nullptr;
        int frames = 0, frame = 0;           // header 0x1C (+0x62 set when it isn't 0), +0x5C
        uint32_t keys = 0;                   // header 0x20 (+0x30); its piece offsets 0x18 on (+0x34)
        float keyScale = 1.0f;               // header 0x14 (+0x38)
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
        int anchorX = 0, anchorY = 0;        // the screen point bounds are kept from (+0x1A, +0x1C)

        void Walk(int piece, int ox, int oy, int oz);
        size_t Key(int piece) const;         // the piece's key for this frame
        int FaceLight(const std::array<int32_t, 3>& a, const std::array<int32_t, 3>& b, const std::array<int32_t, 3>& c,
                      const Landscape::Sun& sun) const;
    };
}
