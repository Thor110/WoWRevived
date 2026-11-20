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

    // 0x0E from the player (0x481CA0 -> 0x482510 -> 0x804F, 0x482E60): the point snapped to a vertex, and with no
    // route needed, "GOING STRAIGHT THERE!": each member is sent 0x80C2 with it. 0x80C2 (0x490A70): the route goes, the
    // target is the point, the arrival radius the unit's radius x arrivalRadius + 20 and its growth (+0x11C) the
    // group's average war map speed (+0x1D0) x 0.01 x members / 2; a unit standing still (speed 0.01 or less) waits
    // ftol(5 x members x (its distance - the nearest's) / (the farthest's - the nearest's + 1) + rand() % 8) updates,
    // one already moving none, before 0x808B(0) sets it going.
    // *Provisional*: the route finder (0x494440) isn't in, so every order goes straight; the snap (+0x30 / +0x34) and
    // the distances the stagger uses (+0xD4, the group's +0x68 / +0x6C) are taken as the vertex nearest the point and
    // each member's distance to it; "ALREADY AT DESTINATION" (within 3 cells), the destination flag (type 0x17C) and
    // the speech (0x10A) aren't.
    void BattleMap::OrderMove(int x, int y)
    {
        std::vector<size_t> members;
        for (size_t i = 0; i < units.size(); i++)
            if (units[i].players && units[i].vehicle && selected[size_t(units[i].group)]) members.push_back(i);
        if (members.empty()) return;
        const int32_t cell = land.Cell(), half = cell / 2;
        auto snap = [&](int v) {
            const int32_t n = int32_t((int64_t(int32_t(uint32_t(v) << 16)) + half) / cell);
            return int(int32_t(int64_t(n) * cell - half) >> 16);
        };
        const int dx = snap(x), dy = snap(y);
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
    // *Provisional*: only the target behaviour; the collision lookahead (0x492350) and the neighbours (0x492C00:
    // separation, avoidance, velocity matching), braking (bit 3), arrival and attraction (both need a route) aren't
    // in, nor the bridge links.
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
            // the target (0x491BB0)
            {
                const float fx = float(double(d.tx) - u.x), fy = float(double(d.ty) - u.y);
                add(float(double(v.targAccWeight) * v.maxAcc), float(double(v.targAccMax) * v.maxAcc), fx, fy);
            }
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

    // 0x492DE0: the speed it may go is the type's +0x54 word x 0.02 (vt+0x12C, 0x492DB0), over the ground's cost there
    // / speedCostDivisor when that's more than 1, and no more than the velocity's length. Going, it's at least 0.2,
    // and the speed rises by 0.05 x it toward it or falls by 0.1 x it above it; stopped, it falls by 0.1 x it to 0.
    // *Provisional*: the ground's cost (the group's 0x480680) is taken as speedCostDivisor (no slowing).
    void BattleMap::SpeedControl(Unit& u, float wanted)
    {
        auto& d = u.drive;
        auto t = setup.units->find(u.type);
        float top = t == setup.units->end() ? 0.0f : float(double(int16_t(t->second.warMapSpeed)) * 0.02);
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
