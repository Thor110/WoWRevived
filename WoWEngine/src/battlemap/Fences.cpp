// Fences and walls (types 451-456; WFence 0x48D780 on the line base 0x498750, its posts 0x48B590). See
// docs/BATTLEMAP.md "Fences". A fence is an object with no model of its own: as it's made (vt+0x24, 0x48D800) it
// lays a row of posts, its one part's type, from its first end to its second at most 64 apart; each post is an
// object with its own health and footprint, linked to the one before it by three or four lines ("wires").
#include "battlemap/BattleMap.h"
#include "core/Bridge.h"

#include <algorithm>
#include <cmath>

namespace wow
{
    namespace
    {
        // each post's lines back to the post before it (0x48BE30 / 0x48B700): pixels above this post's ground
        // point, then above the previous post's; and their colour (0x40B120's 6-bit colours)
        struct Wire { int from, to; };
        struct WireSet { int count; Wire wires[4]; uint32_t colour; };
        uint32_t Six(int r, int g, int b) { return uint32_t((r * 255 + 31) / 63) << 16 | uint32_t((g * 255 + 31) / 63) << 8 | uint32_t((b * 255 + 31) / 63); }
        WireSet WiresOf(int post)
        {
            switch (post)
            {
            case 464: case 695: return { 3, { { -16, -16 }, { -4, -4 }, { -16, -4 } }, 0 };             // dword_4DAF88
            case 694: return { 4, { { -16, -16 }, { -4, -4 }, { -16, -4 }, { -4, -16 } }, 0 };
            case 465: return { 3, { { -16, -16 }, { -10, -10 }, { -4, -4 } }, Six(16, 16, 45) };         // dword_4DAFBC
            case 466: return { 3, { { -16, -16 }, { -10, -10 }, { -4, -4 } }, Six(45, 16, 16) };         // dword_4DAFC0
            case 467: return { 3, { { -16, -16 }, { -10, -10 }, { -4, -4 } }, Six(32, 16, 45) };         // dword_4DAFC4
            default: return { 0, {}, 0 };
            }
        }
    }

    // vt+0x24 (0x48D800) and the start (0x4989F0): the ends put in order (the first the one with the lesser x) and
    // on the vertex grid (((v << 16) + half a cell) / the cell x the cell >> 16, 0x498900); the heading and length
    // between them (0x43FDA0) the fence's; N = ftol(length / 64 + 1) (0x4AF2D8, 0x4AF2DC), and N + 1 posts of its
    // part's type (record 28) at the ends and N - 1 between, each step (x1 - x0, y1 - y0) / N on from the last, each
    // linked to the one before (+0xEC). The fence's health is its type's; then (0x48A7D0) k = N - ftol((N + 1) x
    // health / full) posts from the far end aren't there: a finished fence (one loaded damaged) is shorter, an
    // unfinished one shows the build marker (0x17B) in their places. Each post finished marks the ground round it
    // (vt+0xE4 0x48B690: 0x47C280 radius 32, bit 8), so the posts, at most 64 apart, close the line to vehicles.
    // *Provisional*: the fence object is put at its first end (0x80A7 moves it there as it's placed; one loaded at
    // (0, 0) stays where the deployment puts it in the game, which nothing sees); the posts' heading isn't set.
    void BattleMap::LayFence(size_t index)
    {
        const BattleGroup* g = units[index].group >= 0 && size_t(units[index].group) < setup.groups.size() ? &setup.groups[size_t(units[index].group)] : nullptr;
        auto t = setup.units->find(units[index].type);
        if (!g || !g->hasLine || t == setup.units->end() || t->second.parts.empty()) return;
        const auto& part = t->second.parts.front();
        int x0 = g->line[0], y0 = g->line[1], x1 = g->line[2], y1 = g->line[3];
        if (x1 < x0) { std::swap(x0, x1); std::swap(y0, y1); }
        const int64_t cell = std::max<int64_t>(1, land.Cell()), half = land.HalfCell();
        auto snap = [&](int v) { return int(((((int64_t(v) << 16) + half) / cell) * cell) >> 16); };
        x0 = snap(x0); y0 = snap(y0); x1 = snap(x1); y1 = snap(y1);
        int heading = 0;
        float length = 0;
        HeadingAndLength(float(x0), float(y0), float(x1), float(y1), heading, length);
        const int len = int(length);
        const int n = std::max(1, int(double(len) * 0.015625 - -1.0));
        Unit& f = units[index];
        f.x = float(x0);
        f.y = float(y0);
        f.heading = heading;
        f.postsLaid = f.postsLeft = n;
        const int fenceId = f.id;
        const bool players = f.players;
        const int group = f.group, ref = f.ref;
        const bool finished = double(Progress(f)) > 0.9999;
        const int full = std::max(1, MaxHealth(f.type, f.players));
        const int missing = std::clamp(n - int(double(n + 1) * double(f.health) / double(full)), 0, n);
        const float sx = float(x1 - x0) / float(n), sy = float(y1 - y0) / float(n);
        float px = float(x0), py = float(y0);
        int prev = -1;
        std::vector<std::pair<float, float>> markers;
        for (int i = 0; i <= n; i++, px += sx, py += sy)
        {
            if (i > n - missing)
            {
                if (!finished) markers.push_back({ px, py });
                continue;
            }
            Unit p{};
            p.type = part.a;
            p.x = px;
            p.y = py;
            p.players = players;
            p.group = group;
            p.ref = ref;
            p.slot = -1;                // (a post isn't saved: the fence lays them again)
            p.id = nextId++;
            p.fenceOf = fenceId;
            p.prevPost = prev;
            prev = p.id;
            units.push_back(p);
            StartUnit(units.size() - 1);
        }
        f.postsLeft = n - missing;
        // the unbuilt part's build markers (0x47EB10 with 0x17B), with the fence (they go as it does)
        for (const auto& [mx, my] : markers)
        {
            for (size_t i = 0; i < units.size(); i++)
                if (units[i].id == fenceId) { AddRenderer(0x17B, mx, my, 0, int(i)); break; }
        }
        RekeyViews();
    }

    // the fence's vt+0x50 (0x48DAA0) as a post goes: the post after it (whose +0xEC it was) loses its lines back
    // (0x48B630), the fence one post fewer, its health its full x those left / N (0x48DB04); with none left the
    // fence goes too, quietly (0x48DA40: 0x807B, 0x42A0B0, 0x495BB0)
    void BattleMap::PostGone(const Unit& post)
    {
        for (Unit& u : units)
            if (u.prevPost == post.id) u.prevPost = -1;
        size_t fence = units.size();
        for (size_t i = 0; i < units.size(); i++)
            if (units[i].id == post.fenceOf) { fence = i; break; }
        if (fence == units.size()) return;
        Unit& f = units[fence];
        f.postsLeft = std::max(0, f.postsLeft - 1);
        f.health = f.postsLaid ? MaxHealth(f.type, f.players) * f.postsLeft / f.postsLaid : 0;
        const bool any = std::any_of(units.begin(), units.end(), [&](const Unit& u) { return u.fenceOf == f.id; });
        // (at the next update's deaths, so the caller's list of units stays as it was)
        if (!any) { f.dieAt = ticks; f.dieQuietly = true; }
    }

    // The posts' lines (0x48B700): from each finished post whose previous post stands, its three or four lines,
    // each end the given pixels above that post's ground point on the screen. *Provisional*: drawn over the land's
    // objects, where the game puts each line in the view's list just in front of the nearer post (the larger of
    // the two posts' depths less 1); the fourth line of type 467 isn't read.
    void BattleMap::DrawWires(Canvas& canvas) const
    {
        uint32_t* px = canvas.Pixels();
        const int cw = canvas.Width(), ch = canvas.Height();
        auto line = [&](int x0, int y0, int x1, int y1, uint32_t c) {
            const int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
            int err = dx - dy;
            for (int n = 0; n < 4096; n++)
            {
                if (x0 >= 0 && y0 >= 0 && x0 < cw && y0 < ch) px[size_t(y0) * size_t(cw) + size_t(x0)] = c;
                if (x0 == x1 && y0 == y1) break;
                const int e2 = 2 * err;
                if (e2 > -dy) { err -= dy; x0 += sx; }
                if (e2 < dx) { err += dx; y0 += sy; }
            }
        };
        for (const Unit& u : units)
        {
            if (u.fenceOf < 0 || u.prevPost < 0 || u.fogged) continue;
            const Unit* prev = nullptr;
            for (const Unit& o : units) if (o.id == u.prevPost) { prev = &o; break; }
            if (!prev) continue;
            // both finished (the fence's construction done)
            if (double(Progress(u)) <= 0.9999) continue;
            const WireSet w = WiresOf(u.type);
            const int ax = int(u.x), ay = int(u.y), bx = int(prev->x), by = int(prev->y);
            const int sa = land.ProjectY(ay, land.HeightAt(ax, ay)) - scrollY, sb = land.ProjectY(by, land.HeightAt(bx, by)) - scrollY;
            for (int i = 0; i < w.count; i++) line(ax - scrollX, sa + w.wires[i].from, bx - scrollX, sb + w.wires[i].to, w.colour);
        }
    }
}
