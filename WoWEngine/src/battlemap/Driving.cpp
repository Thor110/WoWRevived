// The battle map's vehicles driving (the ground vehicle 0x490820-0x4935C0 and its group's move order 0x482E60): see
// docs/BATTLEMAP.md "Driving". Each update a vehicle that isn't stopped (or still has speed) steers and steps
// (0x4913B0): every third update (vt+0x148) the steering behaviours add their accelerations within the maxAcc
// budget (0x492F40), then every update the speed is controlled (0x492DE0), the heading turned within its limit,
// the step taken and the arrival checked (0x491870).
#include "battlemap/BattleMap.h"
#include "core/Bridge.h"
#include "core/Random.h"
#include <SDL3/SDL.h>
#include <cmath>

namespace wow
{
    namespace
    {
        // 0x43FF30: b less a, both kept to a turn, in -2048..2047
        int AngleDiff(int a, int b)
        {
            int d = (b & 0xFFF) - (a & 0xFFF);
            while (d < -2048) d += 4096;
            while (d > 2047) d -= 4096;
            return d;
        }

        // 0x43FE90: a heading (4096 a turn) and a length to x, y
        void Polar(int angle, float length, float& x, float& y)
        {
            const double a = double(angle) * 0.000244140625 * 6.283185307;
            x = float(std::cos(a) * length);
            y = float(std::sin(a) * length);
        }
    }

    // 0x0E from the player (0x481CA0 -> 0x482510 -> 0x804F, 0x482E60): the point snapped (below), and with no
    // route needed, "GOING STRAIGHT THERE!": each member is sent 0x80C2 with it. 0x80C2 (0x490A70): the route goes, the
    // target is the point, the arrival radius the unit's radius x arrivalRadius + 20 and its growth (+0x11C) the
    // group's average war map speed (+0x1D0) x 0.01 x members / 2; a unit standing still (speed 0.01 or less) waits
    // ftol(5 x members x (its distance - the nearest's) / (the farthest's - the nearest's + 1) + rand() % 8) updates,
    // one already moving none, before 0x808B(0) sets it going.
    // The point goes to its vertex and then the nearest node the group can stand on (0x482B00).
    // *Provisional*: the route finder (0x494440) isn't in, so every order goes straight; the start isn't snapped (it
    // only feeds the route search and the attack range check), and the distances the stagger uses (+0xD4, the
    // group's +0x68 / +0x6C) are taken as each member's distance to the point; "ALREADY AT DESTINATION" (within 3
    // cells), the destination flag (type 0x17C) and the speech (0x10A) aren't.
    void BattleMap::OrderMove(int x, int y)
    {
        // each selected group gets the order (0x804F to the group)
        for (size_t g = 0; g < selected.size(); g++)
        {
            if (!selected[g]) continue;
            std::vector<size_t> members;
            for (size_t i = 0; i < units.size(); i++)
                if (units[i].players && units[i].vehicle && units[i].group == int(g)) members.push_back(i);
            OrderGroupMove(int(g), members, x, y);
        }
    }

    // 0x482B00 (vertex x, y): from the route finder's node at or before it (every 4 vertices from 4: ((v - 4) / 4)
    // x 4 + 4), squares 0, 4, 8 ... under 200 out, each tried at 8 points (right, right-down, down, left-down, left,
    // left-up, up, right-up), kept on the map, until one the group can cross, strictly (0x480680), is found; the
    // distance out is returned (0 when none is, leaving the last point tried)
    int BattleMap::NearestPassable(int group, int vx, int vy, int& ox, int& oy) const
    {
        const int bx = (vx - 4) / 4 * 4 + 4, by = (vy - 4) / 4 * 4 + 4;
        for (int d = 0; d < 200; d += 4)
        {
            const int pts[8][2] = { { bx + d, by }, { bx + d, by + d }, { bx, by + d }, { bx - d, by + d },
                                    { bx - d, by }, { bx - d, by - d }, { bx, by - d }, { bx + d, by - d } };
            for (auto& p : pts)
            {
                ox = std::clamp(p[0], 0, land.Width() - 1);
                oy = std::clamp(p[1], 0, land.Height() - 1);
                if (CellCost(group, ox, oy, true) != 0) return d;
            }
        }
        return 0;
    }

    void BattleMap::OrderGroupMove(int group, const std::vector<size_t>& members, int x, int y)
    {
        if (members.empty()) return;
        // the point to its vertex, kept on the map, then to the nearest node the group can stand on; back to the world
        const int32_t cell = land.Cell(), half = land.HalfCell();
        auto toVertex = [&](int v, int count) {
            return std::clamp(int(int32_t((int64_t(int32_t(uint32_t(v) << 16)) + half) / cell)), 0, count - 1);
        };
        int nx = 0, ny = 0;
        NearestPassable(group, toVertex(x, land.Width()), toVertex(y, land.Height()), nx, ny);
        const int dx = int(int32_t(int64_t(nx) * cell - half) >> 16), dy = int(int32_t(int64_t(ny) * cell - half) >> 16);
        const int n = int(members.size());
        int nearest = INT32_MAX, farthest = INT32_MIN;
        double speeds = 0;
        std::vector<int> dist;
        for (size_t i : members)
        {
            const Unit& u = units[i];
            const int d = int(std::sqrt(double(u.x - dx) * (u.x - dx) + double(u.y - dy) * (u.y - dy)));
            dist.push_back(d);
            nearest = std::min(nearest, d);
            farthest = std::max(farthest, d);
            if (auto t = setup.units->find(u.type); t != setup.units->end()) speeds += t->second.warMapSpeed;
        }
        const float groupSpeed = float(speeds / n);
        orders.push_back({ n, 0 });
        const int order = int(orders.size()) - 1;
        for (size_t k = 0; k < members.size(); k++)
        {
            Unit& u = units[members[k]];
            auto& d = u.drive;
            d.order = order;
            d.tx = dx;
            d.ty = dy;
            d.arrival = float(double(d.radius) * setup.vehicles.arrivalRadius - -20.0);
            d.grow = float(double(groupSpeed) * 0.01 * (double(n) * 0.5));
            int delay = 0;
            if (!(double(d.speed) > 0.01))
            {
                const float share = float(double(5 * n) * double(dist[k] - nearest) / (double(farthest - nearest) - -1.0));
                delay = int(double(random::Rand() % 8) + share);
            }
            Post808B(u, 0, delay);
        }
    }

    // Backspace (packet 0x11, 0x4812B0): the selection's members get 0x8031: stopped (bit 0), bits 5 and 13 cleared.
    // *Not yet*: the group's target set to its centre.
    void BattleMap::Halt()
    {
        for (Unit& u : units)
        {
            if (!u.players || !u.vehicle || !selected[size_t(u.group)]) continue;
            u.drive.flags |= 1;
            u.drive.flags &= ~0x2020u;
            u.drive.pendingAt = -1;
        }
    }

    // 0x808B, posted to arrive after delay updates (an earlier one dropped, 0x435620): bit 0 its value (stopped), and
    // when it goes the arrived bit cleared; bit 13 the opposite of bit 0
    void BattleMap::Post808B(Unit& u, int value, int delay)
    {
        u.drive.pendingAt = ticks + std::max(delay, 0);
        u.drive.pendingValue = value;
    }

    // 0x4913B0: one update of a unit
    void BattleMap::Drive(size_t index)
    {
        Unit& u = units[index];
        if (!u.vehicle) return;
        auto& d = u.drive;
        if (d.pendingAt >= 0 && ticks >= d.pendingAt)
        {
            d.pendingAt = -1;
            d.flags = (d.flags & ~1u) | (d.pendingValue ? 1u : 0u);
            if (!(d.flags & 1)) d.flags &= ~2u;
            d.flags = (d.flags & ~0x2000u) | (!(d.flags & 1) ? 0x2000u : 0u);
        }
        const bool active = !(d.flags & 1);    // vt+0x144 (0x4909D0): not stopped (bits 5 and 8 and links not in)
        if (!active && !(double(d.speed) > 0.01)) return;
        Steer(u);
        Advance(u);
        if (u.wof)
        {
            const int ix = int(u.x), iy = int(u.y);
            u.heading = d.heading & 0xFFF;  // vt+0x30 with +0x100
            u.wof->Transform(land, u.heading, ix, iy, land.HeightAt(ix, iy));
        }
    }

    // 0x492F40, with the behaviours a straight drive uses. Every third update (0x4898F0) the accelerations are
    // gathered (each one's force x weight x maxAcc, its length at most max x maxAcc: 0x491B20) and added in order until
    // their lengths come to maxAcc (the one that crosses it cut to what's left); their sum goes on the steering
    // velocity, and within (1 + 0.625 x sqrt(the group's size)) x the arrival radius of the target that radius grows
    // by +0x11C. Every update: the velocity's heading and length (0x43FDA0), the speed (0x492DE0), the velocity made
    // the speed long, the turn at most min(8 + 120 x maxAcc x speed, +0x140 x speed) toward its heading (a type of base
    // 0x13 turned more than 0x400 away drives backwards), and the step the speed long along the heading (0x43FE90),
    // taken by vt+0x54 (0x47F770).
    // The behaviours in the game's order: the lookahead's braking (vt+0x158, when 0x492350 finds something ahead), the
    // neighbours gathered (0x492C00), arrival (vt+0x168, not arrived and not bit 5), separation (vt+0x154, no link and
    // not bit 5), the target (vt+0x150), avoidance (vt+0x15C, no link), attraction (vt+0x160, not bit 5), velocity
    // matching (vt+0x16C, no link and not bit 5) and the attribute (vt+0x164, also needing +0xEC).
    // *Provisional*: the lookahead without its ground check, separation, the target, avoidance, braking and velocity
    // matching are in; arrival and attraction (both need a route), the attribute, bit 5 and the bridge links aren't.
    void BattleMap::Steer(Unit& u)
    {
        auto& d = u.drive;
        const auto& v = setup.vehicles;
        d.phase = int16_t(d.phase + 1);
        if (d.phase % 3 == 0)
        {
            d.flags &= ~4u; // arrival (0x491FA0): no route, so not arriving
            std::vector<std::pair<float, float>> acc;
            auto add = [&](float weight, float max, float fx, float fy) {
                float ax = fx * weight, ay = fy * weight;
                const double len = std::sqrt(double(ax) * ax + double(ay) * ay);
                if (len > max) { ax = float(ax * double(max) / len); ay = float(ay * double(max) / len); }
                acc.push_back({ ax, ay });
            };
            // vt+0x158 (0x491EA0): when the lookahead says so, away along +0x110, attributeAcc*
            if (LookAhead(u))
            {
                const double len = std::sqrt(double(d.ax) * d.ax + double(d.ay) * d.ay);
                if (len > 0.0)
                    add(float(double(v.attributeAccWeight) * v.maxAcc), float(double(v.attributeAccMax) * v.maxAcc),
                        float(-(d.ax / len)), float(-(d.ay / len)));
            }
            const Neighbours n = Gather(u);
            // separation (vt+0x154, 0x4920A0): the neighbours' push x how many there are
            if (n.count > 0)
                add(float(double(v.separationAccWeight) * v.maxAcc), float(double(v.separationAccMax) * v.maxAcc),
                    float(double(n.sepX) * n.count), float(double(n.sepY) * n.count));
            // the target (vt+0x150, 0x491BB0)
            {
                const float fx = float(double(d.tx) - u.x), fy = float(double(d.ty) - u.y);
                add(float(double(v.targAccWeight) * v.maxAcc), float(double(v.targAccMax) * v.maxAcc), fx, fy);
            }
            // avoidance (vt+0x15C, 0x491D90): with something ahead, away along the way to it
            if (d.flags & 8)
            {
                const double len = std::sqrt(double(d.ax) * d.ax + double(d.ay) * d.ay);
                if (len > 0.0)
                    add(float(double(v.avoidanceAccWeight) * v.maxAcc), float(double(v.avoidanceAccMax) * v.maxAcc),
                        float(-(d.ax / len)), float(-(d.ay / len)));
            }
            // braking (vt+0x160, 0x491C70): with something ahead, against the step, halved unless it's one of the
            // group's standing
            if (d.flags & 8)
            {
                const double sq = double(d.dx) * d.dx + double(d.dy) * d.dy;
                if (sq > 0.0)
                {
                    const double len = std::sqrt(sq);
                    float bx = float(-(d.dx / len)), by = float(-(d.dy / len));
                    if (!(d.flags & 0x10)) { bx = float(bx * 0.5); by = float(by * 0.5); }
                    add(float(double(v.brakingAccWeight) * v.maxAcc), float(double(v.brakingAccMax) * v.maxAcc), bx, by);
                }
            }
            // velocity matching (vt+0x16C, 0x492160): the moving group members' average step
            if (n.matched > 0)
                add(float(double(v.velMatchAccWeight) * v.maxAcc), float(double(v.velMatchAccMax) * v.maxAcc),
                    float(double(n.stepX) / n.matched), float(double(n.stepY) / n.matched));
            double used = 0, sx = 0, sy = 0;
            for (auto [ax, ay] : acc)
            {
                double len = std::sqrt(double(ax) * ax + double(ay) * ay);
                float step = float(len);
                if (len + used > v.maxAcc)
                {
                    if (step != 0.0f)
                    {
                        ax = float(ax * (v.maxAcc - used) / step);
                        ay = float(ay * (v.maxAcc - used) / step);
                    }
                    step = float(v.maxAcc - used);
                }
                sx += ax;
                sy += ay;
                used += step;
            }
            const float dist = float(d.dist);
            d.vx = float(sx + d.vx);
            d.vy = float(sy + d.vy);
            int members = 1;
            if (d.order >= 0) members = orders[size_t(d.order)].members;
            if (dist < (1.0 - -0.625 * std::sqrt(double(members))) * d.arrival) d.arrival = float(double(d.grow) + d.arrival);
        }
        int want = 0;
        float len = 0;
        HeadingAndLength(0.0f, 0.0f, d.vx, d.vy, want, len);
        SpeedControl(u, len);
        if (len > 0.0f)
        {
            d.vx = float(double(d.vx) * d.speed / len);
            d.vy = float(double(d.vy) * d.speed / len);
        }
        int limit = int(8.0 - -120.0 * double(v.maxAcc) * d.speed);
        const float byRate = float(double(d.turnRate) * d.speed);
        if (!(float(limit) <= byRate)) limit = int(byRate);
        int turn = AngleDiff(d.heading, want);
        float back = 0;
        if (auto t = setup.units->find(u.type); t != setup.units->end() && t->second.baseType == 0x13 && std::abs(turn) > 0x400)
        {
            turn = AngleDiff(d.heading + 0x800, want);
            back = 2048.0f;
        }
        if (turn < -limit) turn = -limit;
        if (turn > limit) turn = limit;
        d.heading += turn;
        Polar(int(float(d.heading) + back), d.speed, d.dx, d.dy);
        u.x = float(double(d.dx) + u.x);
        u.y = float(double(d.dy) + u.y);
    }

    // 0x492C00: the objects in the unit's bucket grid cell (ZMapRes square, built by 0x439770 over the map) and the
    // three cells beside it on the sides it's nearer (0x449920 q 1 to 3; one off the map's edge gives none for 1 and
    // 2, and 3 then gives the other side's neighbour again, so it's counted twice). Each one (0x492A90) that isn't
    // itself, is within 64 in height and isn't a type 0x273 to 0x276 adds to the push away from it, the difference over
    // the cube of the gap between their radii (at least 0.001), and is counted; a moving vehicle of the same group
    // also adds its step and position to the averages.
    // *Provisional*: only the units are in the grid (the game's holds every battle map object that places itself,
    // less category 4 and what vt+0xBC turns down), and the cells are worked out from where they are now.
    // 0x449920 for q 0 to 3: the point's cell, then those beside it on the sides it's nearer (none for q 1 or 2 when
    // that's clamped back onto its own cell; q 3 then gives the other one again)
    std::vector<std::pair<int, int>> BattleMap::BucketCells(int px, int py) const
    {
        const int size = setup.zMapRes;
        const int cols = (land.WorldWidth() + size - 1) / size;
        const int rows = (int((int64_t(land.Cell()) * land.Height()) >> 16) + size - 1) / size;
        auto clampTo = [](int v, int count) { return v < 0 ? 0 : v >= count ? count - 1 : v; };
        const int cx = clampTo(px / size, cols), cy = clampTo(py / size, rows);
        const int nx = clampTo(cx + ((px % size) >= size / 2 ? 1 : -1), cols);
        const int ny = clampTo(cy + ((py % size) >= size / 2 ? 1 : -1), rows);
        std::vector<std::pair<int, int>> cells{ { cx, cy } };
        if (nx != cx) cells.push_back({ nx, cy });
        if (ny != cy) cells.push_back({ cx, ny });
        if (nx != cx || ny != cy) cells.push_back({ nx, ny });
        return cells;
    }

    BattleMap::Neighbours BattleMap::Gather(const Unit& u) const
    {
        Neighbours n;
        const int size = setup.zMapRes;
        const int cols = (land.WorldWidth() + size - 1) / size;
        const int rows = (int((int64_t(land.Cell()) * land.Height()) >> 16) + size - 1) / size;
        auto clampTo = [](int v, int count) { return v < 0 ? 0 : v >= count ? count - 1 : v; };
        const int px = int(u.x), py = int(u.y);
        const auto cells = BucketCells(px, py);
        const int z = land.HeightAt(px, py);
        for (auto [gx, gy] : cells)
            for (const Unit& o : units)
            {
                if (&o == &u) continue;
                const int ox = int(o.x), oy = int(o.y);
                if (clampTo(ox / size, cols) != gx || clampTo(oy / size, rows) != gy) continue;
                if (std::abs(land.HeightAt(ox, oy) - z) > 64) continue;
                if (o.type >= 0x273 && o.type <= 0x276) continue;
                const float ddx = float(double(u.x) - o.x), ddy = float(double(u.y) - o.y);
                double gap = std::sqrt(double(ddx) * ddx + double(ddy) * ddy) - u.drive.radius - o.drive.radius;
                if (!(gap > 0.001)) gap = double(0.001f);
                n.sepX = float(double(ddx) / gap / (gap * gap) + n.sepX);
                n.sepY = float(double(ddy) / gap / (gap * gap) + n.sepY);
                if (o.group == u.group && o.players == u.players && o.vehicle && !(o.drive.flags & 1))
                {
                    n.stepX += o.drive.dx;
                    n.stepY += o.drive.dy;
                    n.posX += o.x;
                    n.posY += o.y;
                    n.matched++;
                }
                n.count++;
            }
        return n;
    }

    // 0x47FE10 (x, y, reach): the cells as 0x449920 gives them, in turn, until one has a hit (0x47FD40): of the objects
    // in it that aren't the unit and pass its vt+0xBC, the nearest by the octagonal distance (the larger difference
    // and half the smaller, from the object's position as whole numbers) that's under the object's radius + reach,
    // starting from 10000.
    // *Provisional*: only units are in the grid, placed by where they are now; vt+0xBC isn't known (all pass).
    const BattleMap::Unit* BattleMap::ObjectAt(const Unit& u, int px, int py, int reach) const
    {
        const int size = setup.zMapRes;
        const int cols = (land.WorldWidth() + size - 1) / size;
        const int rows = (int((int64_t(land.Cell()) * land.Height()) >> 16) + size - 1) / size;
        auto clampTo = [](int v, int count) { return v < 0 ? 0 : v >= count ? count - 1 : v; };
        for (auto [gx, gy] : BucketCells(px, py))
        {
            const Unit* found = nullptr;
            int best = 10000;
            for (const Unit& o : units)
            {
                if (&o == &u) continue;
                const int ox = int(o.x), oy = int(o.y);
                if (clampTo(ox / size, cols) != gx || clampTo(oy / size, rows) != gy) continue;
                const int a = std::abs(px - ox), b = std::abs(py - oy);
                const int d = b - (std::min(a, b) >> 1) + a;
                if (d < o.drive.radius + reach && d < best)
                {
                    best = d;
                    found = &o;
                }
            }
            if (found) return found;
        }
        return nullptr;
    }

    // 0x492350, before the behaviours: bit 3 cleared, then four points looked at, the first the unit's position and
    // each next a step on along the heading (the step: max(speed x 0.5, 1) x collisionCheckDist x the radius). At
    // each, with a ground group (+0x34) and not bit 7, three points a radius out (ahead, left and right) are checked
    // on the vertex grid for ground it can't cross (0x480680 giving 0, not on a bridge link): +0x110 is set toward
    // the vertex, bits 3 set and 4 cleared, and at the first point a type of move class 5 or 6 returns true (vt+0x158
    // then steers it away, `attributeAcc*`). Then the nearest object within the radius (0x47FE10) that's within 64
    // in height: +0x110 the way to it, bit 4 whether it's of the same group but not a vehicle going (vt+0x144), bit 3
    // set, and the look ends. Found at the unit's own position, an object that can't be crushed (vt+0xD8) pushes the
    // unit away: by (the octagonal distance less both radii) / 8 along the way to it, set straight onto the position
    // (vt+0x40); one that can be crushed is, 1 in 4 (rand() < 0x2000), with its type's effect (0x488070).
    // *Provisional*: bit 7 and the bridge links aren't in (the ground check always runs), nor the crushing.
    bool BattleMap::LookAhead(Unit& u)
    {
        auto& d = u.drive;
        const auto& v = setup.vehicles;
        float s = float(double(d.speed) * 0.5);
        if (s < 1.0f) s = 1.0f;
        const float along = float(double(float(double(s) * v.collisionCheckDist)) * d.radius);
        float sx = 0, sy = 0;
        Polar(d.heading, along, sx, sy);
        float fx = 0, fy = 0;
        Polar(d.heading, float(d.radius), fx, fy);
        float px = u.x, py = u.y;
        d.flags &= ~8u;
        const int z = land.HeightAt(int(u.x), int(u.y));
        auto t = setup.units->find(u.type);
        const int moveClass = t == setup.units->end() ? 0 : t->second.field52;
        // a probe point onto the nearest vertex, kept on the map (0x492950); ground it can't cross sets +0x110
        auto blocked = [&](float x, float y) {
            int vx = (int(uint32_t(int(x)) << 16) + land.HalfCell()) / land.Cell();
            int vy = (int(uint32_t(int(y)) << 16) + land.HalfCell()) / land.Cell();
            if (vx >= land.Width()) vx = land.Width() - 1;
            if (vy >= land.Height()) vy = land.Height() - 1;
            if (vx < 0) vx = 0;
            if (vy < 0) vy = 0;
            if (CellCost(u.group, vx, vy, false) != 0) return false;
            d.ax = float(double((int32_t(int64_t(land.Cell()) * vx) - land.HalfCell()) >> 16) - px);
            d.ay = float(double((int32_t(int64_t(land.Cell()) * vy) - land.HalfCell()) >> 16) - py);
            return true;    // and not on a bridge link (+0x18C), which isn't in
        };
        for (int i = 0; i < 4; i++)
        {
            // ahead, then left, then right, a radius out
            if (blocked(px + fx, py + fy) || blocked(px - fy, py + fx) || blocked(px + fy, py - fx))
            {
                d.flags = (d.flags & ~0x10u) | 8u;
                if (i == 0 && (moveClass == 5 || moveClass == 6)) return true;
            }
            const Unit* o = ObjectAt(u, int(px), int(py), d.radius);
            if (o && std::abs(land.HeightAt(int(o->x), int(o->y)) - z) > 64) o = nullptr;
            if (o)
            {
                d.ax = float(double(o->x) - px);
                d.ay = float(double(o->y) - py);
                d.flags &= ~0x10u;
                if (o->group == u.group && o->players == u.players && !(o->vehicle && !(o->drive.flags & 1))) d.flags |= 0x10;
                d.flags |= 8;
                if (i != 0) return false;
                const int dist = (std::abs(int(d.ay)) - (std::min(std::abs(int(d.ax)), std::abs(int(d.ay))) >> 1)) + std::abs(int(d.ax));
                const float fd = float(dist);
                const float gap = float(double(fd) - o->drive.radius - u.drive.radius);
                if (!(fd > 0.0f)) return false;
                const float pushY = float(double(d.ay) / fd * gap);
                const float pushX = float(double(d.ax) / fd * gap);
                u.x = float(double(pushX) / 8.0 + u.x);
                u.y = float(double(pushY) / 8.0 + u.y);
                return false;
            }
            px = float(double(sx) + px);
            py = float(double(sy) + py);
        }
        return false;
    }

    // 0x4802E0 (0x800F, when a group's members change): airborne (bit 5) unless a member isn't of move class 7; bit 6
    // when the members' side (0x4406E0) holds the sector (0x43CA30); over the members (+0xB8 bit 0) of move class c,
    // the terrain costs each the largest (as unsigned, so -1, can't cross, wins) of record 10's for c, the max slope
    // the smallest of record 11's (from 1000), and +0x52 the class whose record 11 slope base is the largest (over
    // -1000). The slope costs (+0xB4) are ftol(pow(base, s)) for s 1 to 8 for each of the 8 classes.
    // *Provisional*: every member counts as +0xB8 bit 0; the groups are set up once, at the start.
    void BattleMap::SetUpGrounds()
    {
        grounds.assign(setup.groups.size(), GroundGroup{});
        std::vector<float> best(setup.groups.size(), -1000.0f);
        std::vector<bool> any(setup.groups.size(), false);
        for (const Unit& u : units)
        {
            if (u.group < 0 || size_t(u.group) >= grounds.size()) continue;
            GroundGroup& g = grounds[size_t(u.group)];
            auto t = setup.units->find(u.type);
            if (t == setup.units->end()) continue;
            any[size_t(u.group)] = true;
            const int c = t->second.field52;
            if (c != 7) g.airborne = false;
            const int side = u.players == setup.human ? 0 : 1;
            g.ownSector = side == setup.ownerFaction;
            if (setup.moveCosts)
                if (auto m = setup.moveCosts->find(c); m != setup.moveCosts->end())
                    for (int k = 0; k < 16; k++)
                        if (uint32_t(m->second[size_t(k)]) > g.terrain[k]) g.terrain[k] = uint32_t(m->second[size_t(k)]);
            if (setup.moveSlopes)
                if (auto m = setup.moveSlopes->find(c); m != setup.moveSlopes->end())
                {
                    if (m->second.first < g.maxSlope) g.maxSlope = m->second.first;
                    if (m->second.second > best[size_t(u.group)])
                    {
                        best[size_t(u.group)] = m->second.second;
                        g.cls = c;
                    }
                }
        }
        for (size_t i = 0; i < grounds.size(); i++)
        {
            if (!any[i]) grounds[i].airborne = true;    // set first, only cleared by a member
            for (int c = 0; c < 8; c++)
            {
                float base = 0;
                if (setup.moveSlopes)
                    if (auto m = setup.moveSlopes->find(c); m != setup.moveSlopes->end()) base = m->second.second;
                for (int k = 1; k <= 8; k++) grounds[i].slopeCost[c * 8 + k - 1] = int(std::pow(double(base), double(k)));
            }
        }
    }

    // 0x480680 (vertex x, y, strict): the vertex's ATM byte gives the terrain (low nibble) and the slope (high, + 1).
    // 0 (can't cross) for a terrain cost of -1 or a slope over the max; and, unless airborne, for flags bit 1, bit 2
    // when strict, or bit 3 in the group's own sector. Otherwise the slope cost of the group's class x the terrain's.
    int BattleMap::CellCost(int group, int vx, int vy, bool strict) const
    {
        if (group < 0 || size_t(group) >= grounds.size()) return 0;
        if (vx < 0 || vy < 0 || vx >= land.Width() || vy >= land.Height()) return 0;
        const GroundGroup& g = grounds[size_t(group)];
        const auto& v = land.At(vx, vy);
        const int slope = (v.atm >> 4) + 1;
        const uint32_t terrain = g.terrain[v.atm & 15];
        if (terrain == 0xFFFFFFFFu) return 0;
        if (slope > g.maxSlope) return 0;
        if (v.flags && !g.airborne)
        {
            if (v.flags & 2) return 0;
            if (strict && (v.flags & 4)) return 0;
            if (g.ownSector && (v.flags & 8)) return 0;
        }
        const int index = g.cls * 8 + slope - 1;
        const int scale = index >= 0 && index < 64 ? g.slopeCost[index] : 0;
        return int(uint32_t(scale) * terrain);
    }

    // 0x492DE0: the speed it may go is the type's +0x54 word x 0.02 (vt+0x12C, 0x492DB0), over the ground's cost there
    // / speedCostDivisor when that's more than 1, and no more than the velocity's length. Going, it's at least 0.2,
    // and the speed rises by 0.05 x it toward it or falls by 0.1 x it above it; stopped, it falls by 0.1 x it to 0.
    // The ground's cost is its group's (0x480680, not strict) at the vertex nearest the unit.
    void BattleMap::SpeedControl(Unit& u, float wanted)
    {
        auto& d = u.drive;
        auto t = setup.units->find(u.type);
        float top = t == setup.units->end() ? 0.0f : float(double(int16_t(t->second.warMapSpeed)) * 0.02);
        {
            const int vx = (int(uint32_t(int(u.x)) << 16) + land.HalfCell()) / land.Cell();
            const int vy = (int(uint32_t(int(u.y)) << 16) + land.HalfCell()) / land.Cell();
            float over = float(double(CellCost(u.group, vx, vy, false)) / setup.vehicles.speedCostDivisor);
            if (over < 1.0f) over = 1.0f;
            top = float(double(top) / over);
        }
        if (!(top <= wanted)) top = wanted;
        if (!(d.flags & 1))
        {
            if (double(top) < 0.2) top = 0.2f;
            if (d.speed < top) d.speed = float(double(d.speed) - double(top) * -0.050000001);
            else if (d.speed > top) d.speed = float(double(d.speed) - double(top) * 0.1);
        }
        else if (d.speed > 0.0f) d.speed = float(double(d.speed) - double(top) * 0.1);
        else d.speed = 0.0f;
    }

    // 0x491870: the distance to the target (+0xFC); with no route it has arrived inside the arrival radius (plus
    // +0x13A, 0 without a route). On arriving (once: bit 1): its group counts it (0x482450) and 0x808B(1) stops it
    // 9 x (members not yet arrived) + 60 updates on.
    void BattleMap::Advance(Unit& u)
    {
        auto& d = u.drive;
        d.dist = int(std::sqrt(double(double(d.tx) - u.x) * (double(d.tx) - u.x) + double(double(d.ty) - u.y) * (double(d.ty) - u.y)));
        const bool arrived = !(0.0 + d.arrival <= double(d.dist));
        if (!arrived || (d.flags & 2)) return;
        d.flags |= 2;
        int left = 0;
        if (d.order >= 0)
        {
            MoveOrder& o = orders[size_t(d.order)];
            o.arrived++;
            left = o.members - o.arrived;
        }
        Post808B(u, 1, 9 * left + 60);
    }
}
