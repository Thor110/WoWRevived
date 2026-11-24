// Building on the battle map (docs/BATTLEMAP.md "Building on the battle map"): the Build List's construction
// items start a placement tool in the view (cursor modes 4-6, 0x42E790 -> 0x485F60), which draws the footprint
// green where it can go and red where it can't, and on a click orders it (0x80A4 -> the Build List 0x460EC1 ->
// packet 0x16), the construction made where it was put at once (0x43CD10) and growing with its factory's hours.
#include "battlemap/BattleMap.h"

#include <algorithm>
#include <cmath>

namespace wow
{
    namespace
    {
        // dword_4DAF80 / dword_4DAF7C (0x40B120): the palette's nearest to 6-bit (0, 0x30, 0) and (0x30, 0, 0)
        constexpr uint32_t PlaceGreen = 0x00C000, PlaceRed = 0xC00000;
    }

    void BattleMap::StartPlacing(int type)
    {
        PlaceTool t;
        t.type = type;
        // 0x460D70: the military bridge (0x1BC) mode 5, the fences and walls (0x1C3-0x1C8) mode 6, the rest 4
        t.mode = type == 0x1BC ? 5 : (type >= 0x1C3 && type <= 0x1C8) ? 6 : 4;
        auto u = setup.units->find(type);
        const int half = u == setup.units->end() ? 8 : u->second.currentHitPoints;   // the type's +0x46
        t.size = ((2 * half) + 14) & ~15;
        t.water = type >= 0x27 && type <= 0x29;     // the shipyards (+0x41)
        tool = t;
        dragging = boxActive = false;
    }

    // 0x4A14C0's test: the world point in vertices ((v << 16) / the cell), half the footprint ((size << 16) / the
    // cell + 1) / 2 either way, and 0x47B610 over that
    bool BattleMap::PostOk(int x, int y, int size, bool water) const
    {
        const int cell = std::max(1, int(land.Cell()));
        const int tx = int((int64_t(x) << 16) / cell), ty = int((int64_t(y) << 16) / cell);
        const int h = int(((int64_t(size) << 16) / cell + 1) / 2);
        return land.CanBuildAt(tx - h, ty - h, tx + h, ty + h, water);
    }

    // The tool's draw (0x4A14C0 / 0x4A0630) does its tracking, every frame: the mouse's world point (its x, the
    // ground's y under it, 0x47B3F0); the first time the centre goes there, after that half way from where it was
    // (so it eases after the mouse); then whether it can go there. A line's far post, and once anchored the run
    // from the anchor (0x47B770, over water for the bridge).
    void BattleMap::UpdatePlacing()
    {
        if (!tool || mouseX < 0 || mouseY < 0) return;
        PlaceTool& t = *tool;
        const int wx = mouseX + scrollX;
        const int wy = land.ScreenYToWorldY(wx, mouseY + scrollY);
        if (t.fresh) { t.cx = wx; t.cy = wy; t.fresh = false; }
        else { t.cx = (wx + t.cx) / 2; t.cy = (wy + t.cy) / 2; }
        if (t.mode == 4) { t.ok = PostOk(t.cx, t.cy, t.size, t.water); return; }
        t.endOk = PostOk(t.cx, t.cy, t.size, false);
        if (t.presses % 2 == 1)
        {
            const int cell = std::max(1, int(land.Cell()));
            auto tile = [cell](int v) { return int((int64_t(v) << 16) / cell); };
            t.lineOk = land.CanBuildLine(tile(t.ax), tile(t.ay), tile(t.cx), tile(t.cy), t.mode == 5);
            t.ok = t.anchorOk && t.lineOk && t.endOk;
        }
        else t.ok = t.endOk;
    }

    // The footprint's outline (0x4A14C0): its four edges in pieces 16 world units long, each end on the ground
    // (0x47B170), green or red. A line's tool draws the anchor's box, the far post's and the run between them.
    // *Provisional*: drawn over everything on the land, where the game puts each piece in the view's list at its
    // ground point's depth + 16.
    void BattleMap::DrawPlacing(Canvas& canvas) const
    {
        if (!tool || tool->fresh) return;
        const PlaceTool& t = *tool;
        const uint32_t colour = t.ok ? PlaceGreen : PlaceRed;
        uint32_t* px = canvas.Pixels();
        const int cw = canvas.Width(), ch = canvas.Height();
        auto plot = [&](int x, int y) { if (x >= 0 && y >= 0 && x < cw && y < ch) px[size_t(y) * size_t(cw) + size_t(x)] = colour; };
        auto screen = [&](int x, int y, int& sx, int& sy) {
            const int cx = std::clamp(x, 0, std::max(0, land.WorldWidth() - 1));
            sx = x - scrollX;
            sy = land.ProjectY(y, land.HeightAt(cx, std::max(0, y))) - scrollY;
        };
        auto line = [&](int x0, int y0, int x1, int y1) {
            const int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
            int err = dx - dy;
            for (int n = 0; n < 4096; n++)
            {
                plot(x0, y0);
                if (x0 == x1 && y0 == y1) break;
                const int e2 = 2 * err;
                if (e2 > -dy) { err -= dy; x0 += sx; }
                if (e2 < dx) { err += dx; y0 += sy; }
            }
        };
        auto ground = [&](int ax, int ay, int bx, int by) {     // a world segment in pieces of 16
            const int len = std::max(std::abs(bx - ax), std::abs(by - ay));
            const int pieces = std::max(1, (len + 15) / 16);
            int px0, py0;
            screen(ax, ay, px0, py0);
            for (int i = 1; i <= pieces; i++)
            {
                int px1, py1;
                screen(ax + (bx - ax) * i / pieces, ay + (by - ay) * i / pieces, px1, py1);
                line(px0, py0, px1, py1);
                px0 = px1; py0 = py1;
            }
        };
        auto box = [&](int x, int y, int size) {
            const int h = size / 2;
            ground(x - h, y - h, x + h, y - h);
            ground(x + h, y - h, x + h, y + h);
            ground(x + h, y + h, x - h, y + h);
            ground(x - h, y + h, x - h, y - h);
        };
        box(t.cx, t.cy, t.size);
        if (t.mode != 4 && t.presses % 2 == 1)
        {
            box(t.ax, t.ay, t.size);
            ground(t.ax, t.ay, t.cx, t.cy);
        }
    }

    // The order (0x4A1460 for a building, on the left button let go; 0x4A0490 / 0x4A05B0 for a line): the
    // placement's {type, x0, y0, x1, y1} broadcast as 0x80A4, which the Build List (0x460EC1) sends on as packet
    // 0x16. Every placement ends the tool (the view's 0x80A4 / 0x8137: cursor mode 7, 0x485453), whether the
    // factory took it or not.
    void BattleMap::PlaceNow(int x0, int y0, int x1, int y1)
    {
        const int type = tool ? tool->type : 0;
        tool.reset();
        if (!setup.build || !setup.build->WindowOpen() || !type) return;
        if (auto g = setup.build->PlaceConstruction(type, x0, y0, x1, y1)) AddGroup(*g);
    }

    // 0x43CD10 in a sector whose battle is on (+0x18): the new object's battle objects are made (0x42A030), put at
    // its place (0x80A7) and started (0x47CFA0, vt+0x14), unfinished (0x4865A0: health max(1, its full health x
    // +0x7C / +0x78), the build markers on the player's). The battle's group lists grow by one, as the deployment
    // makes them.
    void BattleMap::AddGroup(const BattleGroup& g)
    {
        setup.groups.push_back(g);
        const int gi = int(setup.groups.size()) - 1;
        selected.push_back(false);
        groupPlayers.push_back(g.Players());
        groupRetreat.push_back(0);
        groupLeft.push_back(false);
        groupStance.push_back(1);
        groupTargets.push_back(GroupTarget{});
        groupMode.push_back(0);
        groupRange.push_back(100000);
        groupTicks.push_back(0);
        groupInterval.push_back(90);
        groupHomeX.push_back(0);
        groupHomeY.push_back(0);
        groupBusy.push_back(false);
        groupHalted.push_back(false);
        groupOrdered.push_back(false);
        groupArrived.push_back(0);
        groupSelfMove.push_back(false);
        grounds.push_back(GroundGroup{});
        for (size_t slot = 0; slot < g.places.size(); slot++)
        {
            Unit u{};
            u.slot = int(slot);
            if (slot < g.healths.size()) u.savedHealth = g.healths[slot];
            u.type = g.type;
            u.x = g.places[slot].first;
            u.y = g.places[slot].second;
            u.heading = 0;
            u.players = g.Players();
            u.group = gi;
            u.ref = gi;
            u.id = nextId++;
            units.push_back(u);
            StartUnit(units.size() - 1);
        }
        RecomputeGroup(gi);
        RekeyViews();
    }

    void BattleMap::KillGroupOf(int ref)
    {
        if (ref < 0) return;
        for (size_t i = 0; i < units.size();)
        {
            const int g = units[i].group;
            if (g >= 0 && size_t(g) < setup.groups.size() && setup.groups[size_t(g)].ref == ref) Die(i);
            else i++;
        }
    }
}
