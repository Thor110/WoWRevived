// The fog of war (WFogOfWar, 0x4344C0-0x434DF0) and what it does to the other side's objects (0x48B050 /
// 0x48B2D0). See docs/BATTLEMAP.md "Fog of war".
#include "battlemap/BattleMap.h"
#include <algorithm>
#include <cmath>

namespace wow
{
    // 0x4346C0: two width x height byte maps (FogMap +0x18, OldFog +0x1C; all 0xFF when the fog is off), and 19
    // round stamps (+0x20 + 4i, i 1-19) of radius 5 x i (+0x70 + i): 2r x 2r bytes, row by row from (-r, -r), each
    // 0 at r or further, 255 within r less "Fog vert dropoff" (+0xA0), and (r - d) x 255 / dropoff between (0x4349A0)
    void BattleMap::FogStart()
    {
        const int w = land.Width(), h = land.Height();
        fog.map.assign(size_t(w) * size_t(h), fog.enabled ? 0 : 0xFF);
        for (int i = 1; i < 20; i++)
        {
            const int r = 5 * i;
            std::vector<uint8_t>& s = fog.stamps[size_t(i)];
            s.assign(size_t(4 * r * r), 0);
            size_t at = 0;
            for (int y = -r; y < r; y++)
                for (int x = -r; x < r; x++)
                {
                    const float d = float(std::sqrt(double(x * x + y * y)));
                    float v = 0.0f;
                    if (d < float(r))
                        v = float(r - fog.dropoff) < d ? (float(r) - d) * 255.0f / float(fog.dropoff) : 255.0f;
                    s[at++] = uint8_t(int(v));
                }
        }
        FogDecay();   // 0x439D95 (0x434DF0 at EnterBattleMap +0x625)
    }

    // 0x434DF0: every byte that isn't 0 less "Fog return rate" (+0x94), at least 0
    void BattleMap::FogDecay()
    {
        if (!fog.enabled) return;
        for (uint8_t& b : fog.map)
            if (b) b = uint8_t(std::max(0, int(b) - fog.returnRate));
    }

    // 0x434A80 (vt+0x100: 0x4964B0 for a vehicle, 0x48C570 for anything else whose +0x48 isn't 0): at its vertex,
    // the range (its +0x48 in vertices) at most 100, x 0.2 the stamp (1-19; under 1 nothing), max-blended in
    void BattleMap::FogReveal(int vx, int vy, int range)
    {
        if (!fog.enabled) return;
        range = std::min(range, 100);
        int i = int(float(range) * 0.2f);
        if (i >= 20) i = 19;
        if (i < 1) return;
        const int r = 5 * i, w = land.Width(), h = land.Height();
        const std::vector<uint8_t>& s = fog.stamps[size_t(i)];
        for (int y = std::max(0, vy - r); y < std::min(h, vy + r); y++)
            for (int x = std::max(0, vx - r); x < std::min(w, vx + r); x++)
            {
                const uint8_t v = s[size_t(y - (vy - r)) * size_t(2 * r) + size_t(x - (vx - r))];
                uint8_t& b = fog.map[size_t(y) * size_t(w) + size_t(x)];
                if (v && v > b) b = v;
            }
    }

    // vt+0x58 (0x48B050): the fog at the object's four corners (its point plus and less +0xAC, in vertices), each
    // b / "Fog smooth index" (+0x9C) + (1 - that) x "Fog darkness" (+0x90) under the index, else 1.0; their mean
    float BattleMap::FogLevel(const Unit& u)
    {
        if (!fog.enabled || fog.map.empty()) return 1.0f;
        const int w = land.Width(), h = land.Height();
        const int64_t cell = land.Cell(), half = land.HalfCell();
        auto vertex = [&](int v) { return int((int64_t(int32_t(uint32_t(v) << 16)) + half) / cell); };
        const int r = vertex(int(Radius(u.type)));
        const int cx = vertex(int(u.x)), cy = vertex(int(u.y));
        float sum = 0;
        for (int dy : { -r, r })
            for (int dx : { -r, r })
            {
                const int x = std::clamp(cx + dx, 0, w - 1), y = std::clamp(cy + dy, 0, h - 1);
                const float b = float(fog.map[size_t(y) * size_t(w) + size_t(x)]);
                const float idx = float(fog.smooth);
                sum += b < idx ? b / idx + (1.0f - b / idx) * fog.darkness : 1.0f;
            }
        return sum * 0.25f;
    }

    // Each update (0x43A980 for the decay, the objects' updates 0x4967E0 / 0x48C840 for the rest), when the tick is
    // a multiple of "Fog update freq" (+0x98): the fog decays, then each of the player's objects (+0x24 0) reveals
    // round itself. Then the other side's (0x48B2D0, as each is drawn): at its fog level or under "Fog darkness" it's
    // hidden (vt+0xA0(1), +0xB8 bit 3; deselected, 0x4405C0(0), and 0x8126 to those targeting it), over it shown.
    // *Provisional*: the brightness (level x 255 to the renderer, 0x48B2A0) isn't applied, nor the 0x8126.
    void BattleMap::FogTick()
    {
        if (!fog.enabled) return;
        if (fog.updateFreq > 0 && ticks % fog.updateFreq == 0)
        {
            FogDecay();
            FogRevealOwn();
        }
        for (Unit& u : units)
            if (!u.players) u.fogged = !(FogLevel(u) > fog.darkness);
    }

    void BattleMap::FogRevealOwn()
    {
        const int64_t cell = land.Cell(), half = land.HalfCell();
        auto vertex = [&](int v) { return int((int64_t(int32_t(uint32_t(v) << 16)) + half) / cell); };
        for (const Unit& u : units)
        {
            if (!u.players) continue;
            auto t = setup.units->find(u.type);
            if (t == setup.units->end()) continue;
            const int sight = ObjOjd::Vision(t->second, setup.armour);
            if (!u.vehicle && sight == 0) continue;
            FogReveal(vertex(int(u.x)), vertex(int(u.y)), vertex(sight));
        }
    }
}
