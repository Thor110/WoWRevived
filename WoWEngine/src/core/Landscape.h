// The battle map's terrain: the game's WLandscape (0x47AF10-0x47CCF0), built from a map's CLS heights and ATM
// cell types. Every query works on the same fixed-point numbers as the game (16.16 world units, the heights as
// stored in each vertex), so a position gives the same height, normal or building answer the game would.
#pragma once
#include "core/Models.h"
#include <cstdint>
#include <span>
#include <vector>

namespace wow
{
    class Landscape
    {
    public:
        // vertInf: ten bytes for each vertex (0x47BB5C)
        struct Vertex
        {
            uint8_t light = 0;          // +0 the lit level LightTheMap leaves
            uint8_t dynamicLight = 0;   // +1 lights that move: SetPointLight and the vehicles' headlight masks (the
                                        // terrain view takes it off the lit level, 0x4A00BA)
            uint8_t shown = 0;          // +2 the level the terrain view draws with, worked out each frame (0x49FDA0)
            int8_t normal[3] = {};      // +3 to +5: x 127, from CalcNormals
            int16_t height = 0;         // +6 world height: ((byte x scale >> 16) + header 0x30) x 92682 >> 16
            uint8_t atm = 0;            // +8 the cell's ATM byte (its top left vertex): low nibble the type, high the slope
            uint8_t flags = 0;          // +9 1 from the load, 2 blocked, 4 occupied
        };

        // Cell types (the ATM low nibble)
        static bool IsWater(int type) { return type > 0 && type < 4; }
        static constexpr int Impassable = 8, NoBuild = 10;
        static constexpr uint8_t Blocked = 2, Occupied = 4;

        // [Tweak] values read at 0x47B4A0
        struct Tweaks
        {
            int maxSlope = 6;   // "Max Slope For Building On" (dword_59D95C)
            int border = 16;    // "No-build border width (vertices)" (dword_59D93C)
        };

        // The sun for one hour (0x40B030 from an OBJ.ojd record 9 entry), what the landscape keeps of it, and the
        // level the shade table gives (0x40B570)
        struct Sun
        {
            float strength = 0;     // +0x50, the first set's first float (flt_4DA778)
            float ambient = 0;      // +0x54, its second (flt_4DAE28)
            float up = 0;           // sin of the angle (flt_4DAEC4): how much flat ground the sun lights
            float across = 0;       // cos of the angle (flt_4DA770): how much slopes facing it (x) gain
            float cos16 = 0;        // cos x 65536 (flt_4DAEBC)
            float sin16 = 0;        // sin x 65536 (flt_4DAEC0)
            float pointLight = 0;   // 64 x the third float x (0.2 + 0.5 sin) (flt_4DA76C), SetPointLight's level
            float shade = 0;        // flt_4DA764: the shade table's level / 256 (0x40B570)
            float third = 0;        // the set's third float (dword_4DA774), which the WOF faces' light is times
        };
        static Sun SunFor(int angleDegrees, const float set[4]);
        // 0x40CA90: the light a surface gets from the sun, for a normal (across, any, up): sqrt of the sun's part
        // of it, times the strength, plus the ambient (only the ambient when it faces away)
        static double SlopeLight(int across, int other, int up, float strength, float ambient, const Sun& sun);
        // 0x40B570: the last of the shade table's 256 entries at or under ftol(third float x 64) gives the level;
        // with none, the level stays as it was
        static float ShadeLevel(std::span<const uint8_t> table, float third, float previous);
        // The digit in the LANDR file names (dword_4DAED0, 0x40B570): 0 for up to 3 enemy units and buildings in
        // the sector, 1 for 4 to 9, 2 for 10 or more. The battle map counts them (dword_4F7430, 0x43983E) only
        // after loading the tables, so a battle uses the count from the battle before (0 for the first).
        static int ShadeClass(int enemyCount) { return enemyCount <= 3 ? 0 : enemyCount < 10 ? 1 : 2; }

        static Landscape Create(const Terrain& cls, const Tweaks& tweaks);
        static Landscape Create(const Terrain& cls) { return Create(cls, Tweaks{}); }

        int Width() const { return width; }
        int Height() const { return height; }
        int32_t Cell() const { return cell; }
        int32_t HalfCell() const { return halfCell; }   // +0x34
        const Vertex& At(int x, int y) const { return vertices[size_t(y) * size_t(width) + size_t(x)]; }
        Vertex& At(int x, int y) { return vertices[size_t(y) * size_t(width) + size_t(x)]; }
        const std::vector<Vertex>& Vertices() const { return vertices; }
        int32_t WorldWidth() const { return worldWidth; }
        int32_t TopY() const { return topY; }
        int32_t WorldHeight() const { return worldHeight; }
        // The iso projection (0x47C547, 0x439C05): a world y and height to a screen y, about y / sqrt 2 less h x 0.69
        // (fixed-point multiplies by 0xB4B10D5D and 0x2C3E63FB), from the map's top edge
        int ProjectY(int y, int h) const
        {
            int32_t a = int32_t(uint32_t(y) << 16);
            int32_t u = int32_t((int64_t(a) * int32_t(0xB4B10D5D)) >> 32) + a;
            u >>= 16;
            u += int32_t(uint32_t(u) >> 31);
            int32_t b = int32_t(uint32_t(h) << 16);
            int32_t v = int32_t((int64_t(b) * 0x2C3E63FB) >> 32) >> 14;
            v += int32_t(uint32_t(v) >> 31);
            return u - v + topY;
        }
        int32_t RowStep() const { return rowStep; }
        int32_t HeightScale() const { return heightScale; }
        int32_t HeightOffset() const { return heightOffset; }
        const std::vector<uint8_t>& Heights() const { return heights; }
        std::vector<Vertex>& Vertices() { return vertices; }

        // 0x47B120: the height of the vertex a world position falls in
        int HeightAtNearest(int x, int y) const;
        // 0x47B170: the height between the four vertices round a world position
        int HeightAt(int x, int y) const;
        // 0x47B240: the normal there, each part x 256 / 8 with the low five bits cleared
        void NormalAt(int x, int y, int out[3]) const;
        // 0x47B3F0: the world y a screen row at world x looks at, from the far edge in; 0 when none does
        int ScreenYToWorldY(int x, int screenY) const;
        // 0x47B520: a vehicle may stand on every vertex of the area (x0 <= x < x1, y0 <= y < y1)
        bool IsAreaPassable(int x0, int y0, int x1, int y1) const;
        // 0x47B610: a building may go there; one that stands in water (a bridge end, a dock) needs water and land
        bool CanBuildAt(int x0, int y0, int x1, int y1, bool waterSide) const;
        // 0x47B770: a wall or a pipe may run from one vertex to the other (2 to 25 vertices long)
        bool CanBuildLine(int x0, int y0, int x1, int y1, bool overWater) const;
        // 0x47C100: recalculates the normals over a vertex area
        void CalcNormals(int x0, int y0, int x1, int y1);
        // 0x47C280 / 0x47C3A0: sets or clears flag bits over a circle (world position and radius)
        void MarkOccupied(int x, int y, int radius, uint8_t bits);
        void ClearOccupied(int x, int y, int radius, uint8_t bits);
        // 0x47C4C0: darkens the ground round a blast. The game keeps the darkness in the battle world's layer
        // (+0x8C, +0x20); here it is the landscape's own, a byte for each vertex.
        void Scorch(int x, int y, int radius, int intensity);
        const std::vector<uint8_t>& Scorched() const { return scorch; }
        void SetScorched(std::vector<uint8_t> s) { if (s.size() == scorch.size()) scorch = std::move(s); }
        // 0x47C7E0: keeps a world position on the map; true when it moved it
        bool ClampWorld(int& x, int& y) const;
        // 0x47C830 / 0x47C8A0: a light at a world position
        void SetPointLight(int x, int y, const Sun& sun);
        void ClearPointLight(int x, int y);
        // 0x47C900 / 0x47CAB0: a 32 x 32 light mask (a vehicle's headlights) laid over a world rectangle, mirrored
        // left to right when flip is set
        void ProjectLight(const uint8_t* mask, int x0, int y0, int x1, int y1, bool flip, const Sun& sun);
        void ClearLight(const uint8_t* mask, int x0, int y0, int x1, int y1, bool flip);
        // 0x47BD80: lights every vertex for the sun, with the shadows hills cast
        void LightTheMap(const Sun& sun);

    private:
        int width = 0, height = 0;      // +0x24, +0x28
        int count = 0;                  // +0x2C
        int32_t worldWidth = 0;         // +0x1C (header 0x10)
        int32_t worldHeight = 0;        // +0x20 (header 0x14)
        int32_t cell = 0;               // +0x30 (header 0x24), 16.16
        int32_t halfCell = 0;           // +0x34
        int32_t rowStep = 0;            // +0x38 (header 0x28)
        int32_t heightScale = 0;        // +0x3C (header 0x2C)
        int32_t topY = 0;               // +0x48
        int32_t heightOffset = 0;       // +0x4C (header 0x30)
        std::vector<uint8_t> heights;   // +0x58
        std::vector<Vertex> vertices;   // +0x64
        std::vector<uint8_t> scorch;
        Tweaks tweaks;
        int Vertex16(int v) const { return int(((int64_t(v) << 16) + halfCell) / cell); }
        int SmoothedHeight(int x, int y) const;
    };
}
