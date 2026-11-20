// A battle map's scenery (dat\<sector>.nsb): the trees, buildings, fences and walls the designers placed, loaded by
// EnterBattleMap (0x439770) when [Debug] "Load scenery" (default 1) is on, into the world's +0x11C list. The file
// is the object list's save (WObjectList 0x47D240 / 0x47D360): u32 0, the tag BMOL, a u32 count, then each object
// as its u32 type and its class's own Load (vtbl+8) without tags. For the classes the files use:
//   scenery (WScenery 0x4895E0): x, y (clamped to the map)
//   trees, types 0xCF-0xD4 (0x48F3B0): x, y, then a value at +0xC8
//   fences and walls, types 445-447 (0x48D0A0): six dwords, x, y and the two ends
// See docs/BATTLEMAP.md.
#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include "core/Random.h"
#include <cmath>

namespace wow
{
    struct SceneryObject
    {
        int type = 0;
        int x = 0, y = 0;
        int extra[4] = {};      // a tree's +0xC8; a wall's two ends (x1, y1, x2, y2)
    };

    // Throws FormatError when the data isn't a BMOL list or runs short.
    std::vector<SceneryObject> LoadScenery(std::span<const uint8_t> data);

    // An object put on the battle map: its type and world position (+0x78, +0x7C, +0x84 above the ground)
    struct PlacedObject
    {
        int type = 0;
        float x = 0, y = 0, z = 0;
    };

    // What the battle map makes of the list (0x47D240 then ActivateAll 0x47D030), with the game's rand():
    //  - every scenery object (WScenery 0x489380), as it's made: with more than 3 enemy units and buildings in the
    //    sector (dword_4F7430), a rand() and, under (0x8000 / 7) x (count - 3), a tree of types 0xD5-0xE5 becomes
    //    its burnt red-weed kind (0x399-0x3A9)
    //  - a tree circle (0xCF-0xD4, 0x48F410), as it starts: srand(its saved value), then 8, 4 or 2 trees (0xCF / 0xD2
    //    first) of the C (0xD5 + rand() % 7) or D (0xDC + rand() % 10) kinds round its centre, the first at angle
    //    rand() & 0xFFF of 4096, each a further 4096 / n on, at rand() % its radius (the unit table's +0x44)
    // radius(type) gives that word. Trees are put after the rest, as they start after everything's loaded.
    // *Provisional*: the order of the rand() calls of objects other than trees isn't traced.
    template <typename Radius>
    std::vector<PlacedObject> PlaceScenery(const std::vector<SceneryObject>& objects, int enemyCount, Radius radius);

    namespace scenery
    {
        // 0x489380: the burnt kind of a tree, or the type as it was
        inline int Burnt(int type, int enemyCount)
        {
            if (enemyCount <= 3) return type;
            int r = random::Rand();
            if (r >= (0x8000 / (10 - 3)) * (enemyCount - 3)) return type;
            if (type >= 0xD5 && type <= 0xE5) return type - 0xD5 + 0x399;
            return type;
        }
    }

    template <typename Radius>
    std::vector<PlacedObject> PlaceScenery(const std::vector<SceneryObject>& objects, int enemyCount, Radius radius)
    {
        std::vector<PlacedObject> out;
        std::vector<const SceneryObject*> circles;
        for (const SceneryObject& o : objects)
        {
            int type = scenery::Burnt(o.type, enemyCount);
            if (o.type >= 0xCF && o.type <= 0xD4)
            {
                random::Rand(); // the circle's own seed (0x48F343), replaced by the saved one
                circles.push_back(&o);
                continue;
            }
            out.push_back({ type, float(o.x), float(o.y), 0.0f });
        }
        for (const SceneryObject* c : circles)
        {
            random::Seed(uint32_t(c->extra[0]));
            static const int Counts[6] = { 8, 4, 2, 8, 4, 2 };
            const int n = Counts[c->type - 0xCF];
            const int first = c->type < 0xD2 ? 0xD5 : 0xDC;
            const int kinds = c->type < 0xD2 ? 7 : 10;
            int angle = random::Rand() & 0xFFF;
            const int step = 0x1000 / n;
            const int r = radius(c->type);
            for (int i = 0; i < n; i++)
            {
                int dist = r ? random::Rand() % r : random::Rand();
                // 0x43FE90: x4096-of-a-turn angle to radians, cos and sin times the distance, as floats
                double a = double(angle) * 0.000244140625 * 6.283185307;
                float dx = float(std::cos(a) * float(dist)), dy = float(std::sin(a) * float(dist));
                angle += step;
                int type = random::Rand() % kinds + first;
                type = scenery::Burnt(type, enemyCount);
                out.push_back({ type, float(c->x) + dx, float(c->y) + dy, 0.0f });
            }
        }
        return out;
    }
}
