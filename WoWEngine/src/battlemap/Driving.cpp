// The battle map's vehicles driving (the ground vehicle 0x490820-0x4935C0 and its group's move order 0x482E60): see
// docs/BATTLEMAP.md "Driving". Each update a vehicle that isn't stopped (or still has speed) steers and steps
// (0x4913B0): every third update (vt+0x148) the steering behaviours add their accelerations within the maxAcc
// budget (0x492F40), then every update the speed is controlled (0x492DE0), the heading turned within its limit,
// the step taken and the arrival checked (0x491870).
#include "battlemap/BattleMap.h"
#include "platform/Sound.h"
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

    // The buildings (0x48E8A0's class: the non-vehicles of categories 2 and 3)
    bool BattleMap::IsBuilding(const Unit& u) const
    {
        if (u.vehicle || IsEmplacement(u.type)) return false;
        auto t = setup.units->find(u.type);
        return t != setup.units->end() && (t->second.category == 2 || t->second.category == 3);
    }

    // its own sound (the type's +0xA2, 0x4883C0) at its screen point (0x4883E0), kept at +0xE8
    void BattleMap::BuildingSound(Unit& u)
    {
        auto t = setup.units->find(u.type);
        if (!setup.sound || t == setup.units->end() || !t->second.fieldA2) return;
        const int ix = int(u.x), iy = int(u.y);
        u.ambientSound = setup.sound->Play(t->second.fieldA2, 1.0f, 0, ix, land.ProjectY(iy, land.HeightAt(ix, iy) + int(u.drive.lift)));
    }

    // 0x48F130, a building's update: when rand() is under 100 its sound (+0xE8) stops and, unless it's hidden
    // (vt+0x9C), starts again at its point; then the puppet's update (0x4967E0)
    void BattleMap::BuildingTick(Unit& u)
    {
        if (!IsBuilding(u) || random::Rand() >= 100) return;
        if (u.ambientSound && setup.sound) setup.sound->Stop(u.ambientSound);
        u.ambientSound = 0;
        if (!u.fogged) BuildingSound(u);
    }

    // 0x48EEE0 (a building going): both its sounds stopped (+0xE8, +0xEC); and a vehicle's engine and turrets'
    void BattleMap::StopUnitSounds(Unit& u)
    {
        if (!setup.sound) return;
        for (int* h : { &u.engineSound, &u.ambientSound, &u.alertSound })
            if (*h) { setup.sound->Stop(*h); *h = 0; }
        for (Turret& tr : u.turrets)
            if (tr.sound) { setup.sound->Stop(tr.sound); tr.sound = 0; }
    }

    // vt+0x110 (0x4915B0, a vehicle stopping: 0x491590): with none marked (+0x13C + +0x13E 0) the vertices within its
    // type's +0x46 word x 1.25 of its whole position get bit 4 (0x47C280), which the strict ground cost (0x480680)
    // refuses, and the position is kept; vt+0x114 (0x491630: going, 0x491570; each member of a group ordered to move,
    // 0x482F17; removed, 0x490F10) takes it off again (0x47C3A0) if it's there (the sum over 0)
    void BattleMap::MarkFootprint(Unit& u)
    {
        if (u.footX + u.footY != 0) return;
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        const int r = int(double(int16_t(t->second.currentHitPoints)) * 1.25);
        land.MarkOccupied(int(u.x), int(u.y), r, 4);
        u.footX = int16_t(int(u.x));
        u.footY = int16_t(int(u.y));
    }

    void BattleMap::ClearFootprint(Unit& u)
    {
        if (u.footX + u.footY <= 0) return;
        auto t = setup.units->find(u.type);
        if (t != setup.units->end())
            land.ClearOccupied(u.footX, u.footY, int(double(int16_t(t->second.currentHitPoints)) * 1.25), 4);
        u.footX = u.footY = 0;
    }

    // 0x80C3 (x, y) to a vehicle (0x490D06), sent by deployment (0x4222C0) with its spot, or the map's middle for one
    // that kept its place: its heading (+0x100) and the length from the point to it (0x43FDA0), its speed (+0x104)
    // 0.2 and its velocity (+0x108 / +0x10C) that way (0x43FE90), so it eases away from the point.
    // *Provisional*: what +0xB8 bit 9 (0x47F320) is for isn't traced; it isn't kept.
    void BattleMap::Muster(size_t index, int x, int y)
    {
        Unit& u = units[index];
        if (!u.vehicle) return;
        auto& d = u.drive;
        float length = 0;
        HeadingAndLength(float(x), float(y), u.x, u.y, d.heading, length);
        d.speed = 0.2f;
        Polar(d.heading, 0.2f, d.vx, d.vy);
        // 0x490D70: +0xB8 bit 9 set (0x47F320(1)), then vt+0x11C, one step of its driving there and then (0x492F40;
        // a walker's 0x490040 leaves its footprints after it)
        Steer(u);
        if (!d.link) Paddle(u);
        Hover(u);
        Advance(u);
    }

    // 0x0E from the player (0x481CA0 -> 0x482510 -> 0x804F, 0x482E60): the point snapped (below), and with no
    // route needed, "GOING STRAIGHT THERE!": each member is sent 0x80C2 with it. 0x80C2 (0x490A70): the route goes, the
    // target is the point, the arrival radius the unit's radius x arrivalRadius + 20 and its growth (+0x11C) the
    // group's average war map speed (+0x1D0) x 0.01 x members / 2; a unit standing still (speed 0.01 or less) waits
    // ftol(5 x members x (its distance - the nearest's) / (the farthest's - the nearest's + 1) + rand() % 8) updates,
    // one already moving none, before 0x808B(0) sets it going.
    // Both ends go to their vertices and then the nearest node the group can stand on (0x482B00), and the route
    // finder is asked (0x494440, Routes.cpp): going straight, the members get 0x80C2 to where it says; with a
    // route, 0x802E. The group's members are its vehicles (+0xB8 bit 0) not on a bridge link or with bit 8.
    // *Provisional*: the re-post while another group's search runs isn't in (the destination flag, the speech and
    // the attack range's check are, below).
    void BattleMap::OrderMove(int x, int y)
    {
        // each selected group gets the order (0x804F to the group)
        for (size_t g = 0; g < selected.size(); g++)
        {
            if (!selected[g]) continue;
            // 0x481CA0 by the group's mode: 1 (Force Fire from the popup) fires at the point instead (0x481E78);
            // 2 and 3 tell the members 0x80EE / 0x80EF first (2 only if none is on a bridge link; only the sinkers take
            // them, 0x498E90: Sinking.cpp); then the mode 0 and the move
            if (g < groupMode.size() && groupMode[g] == 1)
            {
                ForceFireGroup(int(g), x, y);
                groupMode[g] = 0;
                continue;
            }
            if (g < groupMode.size() && (groupMode[g] == 2 || groupMode[g] == 3))
            {
                bool onLink = false;
                for (const Unit& u : units)
                    if (u.group == int(g) && u.drive.link) onLink = true;
                if (groupMode[g] == 3 || !onLink)
                    for (size_t i = 0; i < units.size(); i++)
                        if (units[i].group == int(g)) GoUnder(i);
            }
            if (g < groupMode.size()) groupMode[g] = 0;
            StopAttacking(int(g));      // a move clears the target (0x481CA0)
            // and the point is the group's home (0x4812B0's case 0x0E: +0x60 / +0x64)
            if (g < groupHomeX.size()) { groupHomeX[g] = x; groupHomeY[g] = y; }
            std::vector<size_t> members;
            for (size_t i = 0; i < units.size(); i++)
                if (units[i].players && units[i].vehicle && units[i].group == int(g)) members.push_back(i);
            if (g < groupOrdered.size()) groupOrdered[g] = true;    // packet 0x0E: bit 3 (0x481E4E)
            OrderGroupMove(int(g), members, x, y);
        }
    }

    int BattleMap::GroupCount(int group) const
    {
        return int(std::count_if(units.begin(), units.end(), [&](const Unit& u) { return u.group == group; }));
    }

    bool BattleMap::PlayersOrder(int group) const
    {
        if (group < 0 || size_t(group) >= groupOrdered.size() || !groupOrdered[size_t(group)]) return false;
        if (size_t(group) >= groupPlayers.size() || !groupPlayers[size_t(group)]) return false;
        for (const Unit& u : units)
            if (u.group == group) return u.players;     // 0x4831B3 / 0x483370: the first member's +0x24 0
        return false;
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
        // 0x482E60's first test: the route finder busy with another group's search, this move is put off: 0x804F
        // posted to the group again, with the point, 10 updates on
        if (finder.group >= 0 && finder.group != group)
        {
            HeldMove h{ ticks + 10, group, x, y, {} };
            for (size_t i : members) h.ids.push_back(units[i].id);
            heldMoves.push_back(std::move(h));
            return;
        }
        // a move order (0x482510 / 0x480EB0) sets +0x1D8 bit 0 and clears bit 2 (the group's own periodic moves set
        // bit 2 again after, GroupTick); bit 0 goes when they've all arrived (0x8081)
        if (group >= 0 && size_t(group) < groupBusy.size()) { groupBusy[size_t(group)] = true; groupSelfMove[size_t(group)] = false; }
        // each member's distance to the point (0x43FF00, +0xD4), the nearest (+0x68) and the farthest (+0x6C),
        // whose position is the route's start
        auto octagonal = [](int x0, int y0, int x1, int y1) {
            const int a = std::abs(x1 - x0), b = std::abs(y1 - y0);
            return b - (std::min(a, b) >> 1) + a;
        };
        int nearest = 1000000, farthest = -1000000, sx = 0, sy = 0;
        for (size_t i : members)
        {
            Unit& u = units[i];
            // 0x482F17: vt+0x114 for every vehicle member (its own footprint off the ground it routes over), before
            // the link test
            if (u.vehicle) ClearFootprint(u);
            if (u.drive.link) continue;
            const int d = octagonal(int(u.x), int(u.y), x, y);
            u.drive.toPoint = d;
            if (d < nearest) nearest = d;
            if (d > farthest)
            {
                farthest = d;
                sx = int(u.x);
                sy = int(u.y);
            }
        }
        // 0x482F98: no member off a bridge link counted, nothing more is done
        if (farthest == -1000000) return;
        // both ends to vertices, kept on the map; "ALREADY AT DESTINATION" within 3; then each to the nearest node
        // the group can stand on (0x482B00)
        const int32_t cell = land.Cell(), half = land.HalfCell();
        auto toVertex = [&](int v, int count) {
            return std::clamp(int(int32_t((int64_t(int32_t(uint32_t(v) << 16)) + half) / cell)), 0, count - 1);
        };
        const int svx = toVertex(sx, land.Width()), svy = toVertex(sy, land.Height());
        const int dvx = toVertex(x, land.Width()), dvy = toVertex(y, land.Height());
        // (0x483062: only when the group's first member is a vehicle, +0xB8 bit 0, without steering bit 9, the
        // retreat: a group retreating from beside its point still goes, to leave there)
        const Unit* first = nullptr;
        for (const Unit& o : units)
            if (o.group == group) { first = &o; break; }
        if (octagonal(svx, svy, dvx, dvy) <= 3 && first && first->vehicle && !(first->drive.flags & 0x200))
        {
            SDL_Log("ALREADY AT DESTINATION");
            return;
        }
        int ssx = 0, ssy = 0, gx = 0, gy = 0;
        NearestPassable(group, svx, svy, ssx, ssy);
        const int goalOut = NearestPassable(group, dvx, dvy, gx, gy);
        const bool attacking = group >= 0 && size_t(group) < groupTargets.size() && groupTargets[size_t(group)].target != 0;
        // 0x4830FD: the route's radius is 4, or with a target (+0x2C) the group's range (+0x4E) in vertices; when the
        // goal had to be moved further out than that, "CAN'T GET TO WITHIN ATTACK DISTANCE!" and route not found
        // (0x4833F0)
        auto notFound = [&]() { RouteNotFound(group, members); };
        int radius = 4;
        if (attacking)
        {
            const int range = int((int64_t(int32_t(uint32_t(groupRange[size_t(group)]) << 16)) + land.HalfCell()) / land.Cell());
            if (range < goalOut)
            {
                SDL_Log("CAN'T GET TO WITHIN ATTACK DISTANCE!");
                notFound();
                return;
            }
            radius = range;
        }
        // the counts the move uses are the group list's (0x4216D0), not just the members ordered
        const int n = std::max(1, GroupCount(group));
        orders.push_back({ n, 0, nearest, farthest, group });
        const int order = int(orders.size()) - 1;
        for (size_t i : members) units[i].drive.order = order;
        // 0x482E60: each vehicle member is asked vt+0x118 (0x493600 for the sinker classes: under); when the first
        // member's vt+0x70 says so (only the Tunnelling Track Layers' class, 0x493B90), all of them under go straight
        // there without a route, and some but not all under drops the order. Every other group asks for a route.
        bool anyUnder = false, allUnder = true;
        for (size_t i : members)
        {
            if (!units[i].vehicle) continue;
            if (Sinker(units[i].type) && units[i].under) anyUnder = true;
            else allUnder = false;
        }
        const Unit* firstMember = nullptr;
        for (const Unit& o : units)
            if (o.group == group) { firstMember = &o; break; }
        const bool tunnelling = firstMember && ClassOf(firstMember->type) == VehicleClass::Tunneller;
        if (tunnelling && anyUnder && !allUnder) return;
        if (!(tunnelling && anyUnder && allUnder) && BeginRoute(group, ssx, ssy, gx, gy, radius))
        {
            // the search is the route finder's now; the group's update carries it on and ends the move (PollRoute)
            finder.order = order;
            finder.n = n;
            finder.sx = ssx; finder.sy = ssy; finder.gx = gx; finder.gy = gy;
            finder.attacking = attacking;
            return;
        }
        SDL_Log("GOING STRAIGHT THERE! (from vertex %d,%d to %d,%d)", ssx, ssy, gx, gy);
        // 0x483590 at (the goal + 2): a player's move by a player's group (+0x1D8 bits 3 and 4) puts the "_ACK" flag
        // (type 0x17C, a WAnimEffect) there and, with no target (+0x2C), the group speaks 0x10A; bit 3 then cleared
        if (PlayersOrder(group))
        {
            MoveFlag(int32_t(int64_t(gx + 2) * cell - half) >> 16, int32_t(int64_t(gy + 2) * cell - half) >> 16);
            if (!attacking) Speak(0x10A, group);
            groupOrdered[size_t(group)] = false;
        }
        const int dx = int(int32_t(int64_t(gx) * cell - half) >> 16), dy = int(int32_t(int64_t(gy) * cell - half) >> 16);
        // 0x80C2 to each member (0x490A70)
        double speeds = 0;
        int count = 0;
        for (const Unit& o : units)
            if (o.group == group)
            {
                count++;
                if (auto t = setup.units->find(o.type); t != setup.units->end()) speeds += int16_t(t->second.warMapSpeed);
            }
        const float groupSpeed = count ? float(speeds / count) : 0.0f;
        for (size_t i : members)
        {
            Unit& u = units[i];
            auto& d = u.drive;
            d.route.reset();
            d.waypoint = 0;
            d.tx = dx;
            d.ty = dy;
            d.arrival = float(double(float(double(d.radius) * setup.vehicles.arrivalRadius)) - -20.0);
            d.grow = float(double(groupSpeed) * 0.01 * (double(count) * 0.5));
            int delay = 0;
            if (!(double(d.speed) > 0.01))
            {
                const float share = float(double(5 * n) * double(d.toPoint - nearest) / (double(farthest - nearest) - -1.0));
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
            Stop8031(u);
        }
    }

    // The move flag (0x483590): type 0x17C (hove-ack / move-ack.spr, palette slot 5) made there and started, then its
    // renderer's level set to 0 (vt+0x18(0)) and its +0x14 bit 0 cleared (drawn solid). Its type's +0xA1 >> 2, 48, is
    // past the 30 levels of slot 5's table, all black there: the flag was a black disc.
    void BattleMap::MoveFlag(int x, int y)
    {
        const int e = SpawnEffect(0x17C, float(x), float(y));
        if (e < 0) return;
        for (ViewObject& o : objects)
            if (o.effect == e && o.light < 0) o.level = 0;
    }

    // 0x805A to a unit (0x495CB0's case): vt+0xCC, a vehicle's 0x491180 (steering bit 5 cleared, bit 0 set: it stops)
    // then 0x496440 (its goal cleared, +0xE4 bit 1: not aiming); then vt+0xAC(0), +0xE4 bit 0: not halted
    void BattleMap::StandDown(Unit& u)
    {
        if (u.vehicle)
        {
            u.drive.flags &= ~0x20u;
            u.drive.flags |= 1;
        }
        u.aiming = false;
        u.aimX = u.aimY = 0;    // 0x49648B: +0xC8 / +0xCC cleared with the goal
        u.halted = false;
    }

    // 0x8031 to a vehicle (0x4910C0): bit 5 cleared, bit 0 (stopped) set, bit 13 cleared (an 0x808B still to come
    // dropped); then 0x496BC0: +0xDA (the look round's count) 0 and vt+0xAC(1), +0xE4 bit 0: halted
    void BattleMap::Stop8031(Unit& u)
    {
        u.drive.flags |= 1;
        u.drive.flags &= ~0x2020u;
        u.drive.pendingAt = -1;
        u.scanCount = 0;
        u.halted = true;
    }

    // 0x808B, posted to arrive after delay updates (an earlier one dropped, 0x435620): bit 0 its value (stopped), and
    // when it goes the arrived bit cleared; bit 13 the opposite of bit 0
    void BattleMap::Post808B(Unit& u, int value, int delay)
    {
        u.drive.pendingAt = ticks + std::max(delay, 0);
        u.drive.pendingValue = value;
    }

    // 0x491450 (vt+0x138): the engine sound (+0xE8) stopped (vt+0x13C), then, unless it's dead (vt+0x9C), its type's
    // +0xA2 while it's going (vt+0x144) or +0xA4 while not, at its screen point (+0x2C / +0x30)
    // and each step moves it with the vehicle (0x49330A, Sound::Move).
    void BattleMap::EngineSound(Unit& u)
    {
        if (!setup.sound) return;
        if (u.engineSound) setup.sound->Stop(u.engineSound);
        u.engineSound = 0;
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        const bool going = !(u.drive.flags & 1) || (u.drive.flags & 0x20) || u.drive.link;
        const int id = going ? t->second.fieldA2 : t->second.fieldA4;
        if (id == 0) return;
        // at the posed object's screen point (+0x2C / +0x30, 0x491450): the lift in
        const int x = int(u.x), y = land.ProjectY(int(u.y), land.HeightAt(int(u.x), int(u.y)) + int(u.drive.lift));
        u.engineSound = setup.sound->Play(id, 1.0f, 0, x, y);
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
        // vt+0x144 (0x4909D0): not stopped, or held (bit 5, 0x808A; bit 8 isn't set by anything ported), or on a link
        const bool active = !(d.flags & 1) || (d.flags & 0x20) || d.link;
        // the edges (0x4913B0): stopping sets bit 11 and, from bit 10, clears it and calls vt+0x124 (0x491590: vt+0x110,
        // then the engine sound); going sets bit 10 and, from bit 11, clears it and calls vt+0x120 (0x491570: vt+0x114,
        // then the engine sound)
        if (!active && !(double(d.speed) > 0.01))
        {
            d.flags |= 0x800;
            if (d.flags & 0x400)
            {
                d.flags &= ~0x400u;
                Surface(index);         // the sinkers' vt+0x124 (0x499120)
                MarkFootprint(units[index]);
                EngineSound(units[index]);
            }
            return;
        }
        d.flags |= 0x400;
        if (d.flags & 0x800)
        {
            d.flags &= ~0x800u;
            ClearFootprint(u);
            EngineSound(u);
        }
        // frozen (vt+0x108, 0x4909D0's check), it doesn't move
        if (!u.Frozen())
        {
            Steer(u);
            if (!d.link) Paddle(u);     // the walkers' step (0x490040)
            Hover(u);                   // the fliers' (0x490170)
            Advance(u);
            // 0x49330A: with a sound in +0xE8, 0x489290 moves it to the object's screen point, so it follows
            if (u.engineSound && setup.sound)
                setup.sound->Move(u.engineSound, int(u.x), land.ProjectY(int(u.y), land.HeightAt(int(u.x), int(u.y)) + int(d.lift)));
        }
        if (u.wof)
        {
            const int ix = int(u.x), iy = int(u.y);
            u.heading = d.heading & 0xFFF;  // vt+0x30 with +0x100
            u.wof->SetFrame(int(d.frame));  // 0x47F677
            u.wof->Transform(land, u.heading, ix, iy, int(float(land.HeightAt(ix, iy)) + d.lift));  // + +0x84
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
    // The behaviours in the game's order: away from ground it can't cross (vt+0x158, when 0x492350 says so), the
    // neighbours gathered (0x492C00), arrival (vt+0x168, not arrived and not bit 5), separation (vt+0x154, no link and
    // not bit 5), the target (vt+0x150), avoidance (vt+0x15C, no link), braking (vt+0x160, not bit 5), velocity
    // matching (vt+0x16C, no link and not bit 5) and cohesion (vt+0x164, the same and on a route).
    // After the step (0x492F40 + 0x408): on a route whose waypoint has a link byte the bridge's vt+0x100 (OnBridge)
    // is asked and +0x18C becomes the link; else, on a link, it's asked and +0x18C is cleared when it says no (a
    // broken bridge's link (+0xEC 0) over water kills it: 0x80A0, its group 0x8031).
    // *Provisional*: broken bridges aren't in.
    void BattleMap::Steer(Unit& u)
    {
        auto& d = u.drive;
        const auto& v = setup.vehicles;
        d.phase = int16_t(d.phase + 1);
        if (d.phase % SteerPeriod(u.type) == 0)
        {
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
            Neighbours n = Gather(u);
            // arrival (vt+0x168, 0x491FA0; not arrived and not bit 5): on a route inside the arrival radius, bit 2 and
            // against the step, arrivalAcc*; else bit 2 cleared
            const bool held = (d.flags & 0x20) != 0;
            if (!(d.flags & 2) && !held)
            {
                if (d.route && float(d.dist) < d.arrival)
                {
                    d.flags |= 4;
                    add(float(double(v.arrivalAccWeight) * v.maxAcc), float(double(v.arrivalAccMax) * v.maxAcc), -d.dx, -d.dy);
                }
                else d.flags &= ~4u;
            }
            // separation (vt+0x154, 0x4920A0): the neighbours' push x how many there are
            // (the water classes' 0x493AD0: the push doubled, the weight x the count)
            const bool boat = Boat(u.type);
            if (n.count > 0 && !d.link && !held)
            {
                if (boat)
                    add(float(double(v.separationAccWeight) * v.maxAcc * n.count), float(double(v.separationAccMax) * v.maxAcc),
                        float(double(n.sepX) + n.sepX), float(double(n.sepY) + n.sepY));
                else
                    add(float(double(v.separationAccWeight) * v.maxAcc), float(double(v.separationAccMax) * v.maxAcc),
                        float(double(n.sepX) * n.count), float(double(n.sepY) * n.count));
            }
            // the target (vt+0x150, 0x491BB0)
            {
                const float fx = float(double(d.tx) - u.x), fy = float(double(d.ty) - u.y);
                add(float(double(v.targAccWeight) * v.maxAcc), float(double(v.targAccMax) * v.maxAcc), fx, fy);
            }
            // avoidance (vt+0x15C, 0x491D90): with something ahead, away along the way to it
            if ((d.flags & 8) && !d.link)
            {
                const double len = std::sqrt(double(d.ax) * d.ax + double(d.ay) * d.ay);
                // (the water classes' 0x4939A0 / 0x4937C0: a quarter of it)
                if (len > 0.0)
                    add(float(double(v.avoidanceAccWeight) * v.maxAcc), float(double(v.avoidanceAccMax) * v.maxAcc),
                        float(-(d.ax / len) * (boat ? 0.25 : 1.0)), float(-(d.ay / len) * (boat ? 0.25 : 1.0)));
            }
            // braking (vt+0x160, 0x491C70): with something ahead, against the step, halved unless it's one of the
            // group's standing
            if ((d.flags & 8) && !held)
            {
                const double sq = double(d.dx) * d.dx + double(d.dy) * d.dy;
                if (sq > 0.0)
                {
                    const double len = std::sqrt(sq);
                    float bx = float(-(d.dx / len)), by = float(-(d.dy / len));
                    if (!(d.flags & 0x10)) { bx = float(bx * 0.5); by = float(by * 0.5); }
                    if (boat) { bx = bx + bx; by = by + by; }   // the water classes' 0x4938E0 / 0x493700: doubled
                    add(float(double(v.brakingAccWeight) * v.maxAcc), float(double(v.brakingAccMax) * v.maxAcc), bx, by);
                }
            }
            // velocity matching (vt+0x16C, 0x492160): the moving group members' average step
            if (n.matched > 0 && !d.link && !held)
                add(float(double(v.velMatchAccWeight) * v.maxAcc), float(double(v.velMatchAccMax) * v.maxAcc),
                    float(double(n.stepX) / n.matched), float(double(n.stepY) / n.matched));
            // cohesion (vt+0x164, 0x492240; with a route): toward the moving group members' average position,
            // attractAcc*
            // (the water classes' vt+0x164 is a nullsub: no cohesion)
            if (d.route && n.matched > 0 && !d.link && !held && !Boat(u.type))
            {
                n.posX = float(double(n.posX) / n.matched);
                n.posY = float(double(n.posY) / n.matched);
                add(float(double(v.attractAccWeight) * v.maxAcc), float(double(v.attractAccMax) * v.maxAcc),
                    float(double(n.posX) - u.x), float(double(n.posY) - u.y));
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
        const float fromX = u.x, fromY = u.y;
        u.x = float(double(d.dx) + u.x);
        u.y = float(double(d.dy) + u.y);
        const int waypointLink = d.route && d.waypoint < int(d.route->nodes.size()) ? d.route->nodes[size_t(d.waypoint)].link : 0;
        // (0x493374 / 0x49339F: a link whose bridge has gone from the table is left as it is, the height with it)
        if (waypointLink)
        {
            if (!BridgeGone(waypointLink))
            {
                OnBridge(u, waypointLink);
                d.link = waypointLink;
            }
        }
        else if (d.link && !BridgeGone(d.link) && !OnBridge(u, d.link)) d.link = 0;
        // 0x49342C: the length of the step (0x43FDA0) and the turn animate the model
        const double mx = double(u.x) - fromX, my = double(u.y) - fromY;
        Animate(u, float(std::sqrt(mx * mx + my * my)), turn);
        Wake(u);
    }

    // The fliers' step (0x490170, class 0x490120: the factory's types 0x19-0x1B and 0x4D-0x4F), after the steering:
    // they keep 256 over the height of the vertex they're in (0x47B120). With the gap from their height (+0x80, the
    // ground's plus +0x84) under 4 either way the upward speed (+0x90) is x 0.8; else it gains (gap - 4) x the
    // type's +0x54 x 0.00005, kept within +0x54 x 0.01 either way. The height over the ground (+0x84) then gains it.
    void BattleMap::Hover(Unit& u)
    {
        if (!Flier(u.type)) return;
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        auto& d = u.drive;
        const int ix = int(u.x), iy = int(u.y);
        const float height = float(land.HeightAt(ix, iy)) + d.lift;   // +0x80
        const float gap = float(land.HeightAtNearest(ix, iy) + 0x100) - height;
        if (std::abs(int(gap)) < 4) d.climb = float(double(d.climb) * 0.8);
        else
        {
            const float rate = float(int16_t(t->second.warMapSpeed));
            const float most = rate * 0.0099999998f;
            d.climb = (gap - 4.0f) * rate * 0.000049999999f + d.climb;
            if (d.climb > most) d.climb = most;
            if (d.climb < -most) d.climb = -most;
        }
        d.lift += d.climb;
    }

    // vt+0x134 after each step (0x4934F0, every vehicle class but the walkers'): every fourth update of the look-round
    // count (+0xDA), with vt+0x9C false, off a link, over water (0x47FFE0: the cell of its point is of types 1-3), of a
    // type whose +0x52 isn't 5 or 6, and under 8 over the water (|ftol(+0x84)|), it leaves a wake 0x3C8 at its point at
    // height 0 (its renderer's +0x26 8).
    // *Provisional*: vt+0x9C is taken as false, and the wake's vt+0x14 value (dword_4D2550's +0x90) isn't in.
    void BattleMap::Wake(Unit& u)
    {
        auto& d = u.drive;
        if (int16_t(u.scanCount) % 4 != 0 || d.link || Walker(u.type)) return;
        auto t = setup.units->find(u.type);
        if (t == setup.units->end() || t->second.field52 == 5 || t->second.field52 == 6) return;
        if (std::abs(int(d.lift)) >= 8) return;
        const int x = int(u.x), y = int(u.y);
        const int vx = std::clamp(int((int64_t(int32_t(uint32_t(x) << 16)) + land.HalfCell()) / land.Cell()), 0, land.Width() - 1);
        const int vy = std::clamp(int((int64_t(int32_t(uint32_t(y) << 16)) + land.HalfCell()) / land.Cell()), 0, land.Height() - 1);
        const int terrain = land.At(vx, vy).atm & 15;
        if (terrain < 1 || terrain > 3) return;
        LowEffect(0x3C8, float(u.x), float(u.y));
    }

    // An effect at ground height with its renderer's +0x26 (the depth over its y) 8: the walkers' splashes and the
    // wakes
    void BattleMap::LowEffect(int type, float x, float y)
    {
        if (const int k = SpawnEffect(type, x, y); k >= 0)
        {
            Effect& e = effects[size_t(k)];
            for (ViewObject& o : objects)
                if (o.effect == k) o.key += 8 - e.depth;
            e.depth = 8;
            std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
        }
    }

    // The Ironclads' update (0x493B50, after the vehicle's 0x4913B0): off water (0x47FFE0), when rand() % 2 is 0, it
    // posts itself 0x80C1 with 10 for the next update: 10 damage (0x48A273, vt+0xE0).
    void BattleMap::ShipTick(size_t index)
    {
        Unit& u = units[index];
        if (u.type < 0x13 || u.type > 0x15) return;
        if (u.shoreAt >= 0 && ticks >= u.shoreAt)
        {
            u.shoreAt = -1;
            Damage(index, 10);
        }
        if (!OnWater(u) && random::Rand() % 2 == 0) u.shoreAt = ticks + 1;
    }

    // vt+0x130 after each step. Most vehicles (0x493490, the fliers' class 0x490120 too): the frame (+0xA8, 2.0 as it starts) goes on by the length
    // of the step x its model GType's OBJ.ojd record 6 second float (0x4F0538, read by 0x437A52). The walkers'
    // class (0x48FC20, vtable 0x4AFEA8, its vt+0x130 0x490070: the factory's types 0x3E-0x4C and 0x50-0x5C) first smooths the turn, +0x190 = (turn + 2 x
    // +0x190) / 3, and while it's at least [Vehicles] startTurnRate either way the frame goes on by |it| x
    // turnAnimRate + 0.2 instead. Either way a frame at or past the model's count has count - 1 taken off, so the
    // loop comes back round to frame 1 (frame 0 is the model at rest). The renderer (0x47F630) poses the model at
    // ftol(the frame).
    // *Provisional*: the frame isn't kept in saves, and every vehicle class but the walkers' is taken to be 0x493490's.
    // The walkers' step (0x490040): after the steering, off a link (and vt+0x9C false), 0x48FDD0 checks the feet, the
    // pieces listed by type. A foot (0x48FCB0) whose first point (from the last pose) is on a water cell (types 1-3) is
    // hidden (WorldPts' byte) and, every third update of the look-round count (+0xDA), makes a splash 0x3C7 there
    // at height 0 (vt+0x14 with dword_4D2550's +0x90, its renderer's +0x26 8); one on land is shown again.
    // *Provisional*: vt+0x9C isn't known (taken as false) and the splash's vt+0x14 value isn't in.
    void BattleMap::Paddle(Unit& u)
    {
        if (!u.wof || !Walker(u.type)) return;
        // each walker type's feet (0x48FDD0's switch on the type)
        auto feet = [](int t) -> std::vector<int> {
            switch (t)
            {
            case 0x3E: case 0x3F: case 0x40: return { 1, 2, 3 };
            case 0x41: case 0x42: case 0x43: return { 2, 3, 4 };
            case 0x44: case 0x45: case 0x46: case 0x51: case 0x52: return { 4, 5, 6, 7 };
            case 0x47: case 0x48: case 0x49: return { 9, 6 };
            case 0x4A: case 0x4B: case 0x4C: return { 9, 10, 11, 12, 13, 14 };
            case 0x50: case 0x5C: return { 2, 3, 4, 5 };
            case 0x53: return { 8, 9, 10, 11 };
            case 0x54: case 0x55: return { 9, 10, 11, 12 };
            case 0x56: return { 1, 2, 3, 4, 5 };
            case 0x57: return { 2, 3, 4, 15, 16 };
            case 0x58: return { 3, 4, 5, 6, 7 };
            case 0x59: case 0x5A: return { 13, 14, 15, 16, 17, 18 };
            case 0x5B: return { 1, 2, 3, 4 };
            default: return {};
            }
        };
        for (int piece : feet(u.type))
        {
            int x = 0, y = 0;
            if (!u.wof->FirstPoint(piece, x, y)) continue;
            const int vx = std::clamp(int((int64_t(int32_t(uint32_t(x) << 16)) + land.HalfCell()) / land.Cell()), 0, land.Width() - 1);
            const int vy = std::clamp(int((int64_t(int32_t(uint32_t(y) << 16)) + land.HalfCell()) / land.Cell()), 0, land.Height() - 1);
            const int terrain = land.At(vx, vy).atm & 15;
            const bool wet = terrain > 0 && terrain < 4;
            u.wof->SetHidden(piece, wet);
            if (wet && int16_t(u.scanCount) % 3 == 0) LowEffect(0x3C7, float(x), float(y));
        }
    }

    void BattleMap::Animate(Unit& u, float moved, int turn)
    {
        auto& d = u.drive;
        if (!u.wof || u.wof->Frames() <= 0) return;
        float rate = 0;
        if (auto t = setup.units->find(u.type); t != setup.units->end() && setup.six)
            if (auto six = setup.six->find(t->second.field28); six != setup.six->end()) rate = six->second.b;
        const bool walker = Walker(u.type);
        bool turning = false;
        if (walker)
        {
            d.smoothTurn = float((float(turn) - d.smoothTurn * -2.0f) * 0.33333334f);
            const int t = std::abs(int(d.smoothTurn));
            if (t >= setup.vehicles.startTurnRate)
            {
                d.frame = float(double(float(t) * setup.vehicles.turnAnimRate + d.frame) - -0.2);
                turning = true;
            }
        }
        if (!turning) d.frame = rate * moved + d.frame;
        const int frames = u.wof->Frames();
        if (!(float(frames) > d.frame)) d.frame -= float(frames - 1);
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
        // 0x492A90: the heights compared are the objects' own (+0x80, the ground plus the lift), so a deck and what's
        // under it don't steer round each other
        const int z = land.HeightAt(px, py) + int(u.drive.lift);
        for (auto [gx, gy] : cells)
            for (const Unit& o : units)
            {
                if (&o == &u || !Collides(u, o)) continue;
                const int ox = int(o.x), oy = int(o.y);
                if (clampTo(ox / size, cols) != gx || clampTo(oy / size, rows) != gy) continue;
                if (std::abs(land.HeightAt(ox, oy) + int(o.drive.lift) - z) > 64) continue;
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
    // The grid's objects are the units and the scenery (update 142), placed by where they are now.
    // Crushable (vt+0xD8, 0x486340: +0xC0 bit 2): the mines (0x48C940 sets it on them as they're placed) and the
    // line-linked objects 0x1D0-0x1D3, 0x2B6, 0x2B7 (0x48A8B0), which aren't in the engine yet (as scenery they'd
    // be crushed the same way)
    bool BattleMap::Crushable(const Unit& o) const
    {
        return o.type >= 0x273 && o.type <= 0x276;
    }

    // The vehicles' vt+0xBC (0x4929A0): whether an object gets in the way. Not one with +0xB8 bit 2; for a type whose
    // +0x52 is 3 (4), not one under 0x30 (0x20) tall (+0x2E less +0x2C) that isn't crushable; then one with
    // +0xB8 bit 0 does; else, on a bridge link, not the bridges' pieces (0x1BC-0x1BF, 0x1CC-0x1CE) nor 0x1C0; else
    // whether it has +0xB8 bit 1.
    // *Provisional*: every unit counts as +0xB8 bit 0 (so the last two never come into it), and the renderer's height
    // is its GType's first frame's.
    bool BattleMap::Collides(const Unit& u, const Unit& o) const
    {
        auto t = setup.units->find(u.type);
        const int moveClass = t == setup.units->end() ? 0 : t->second.field52;
        // a low object (0x492A04) is passed over unless it's crushable (0x492A12: vt+0xD8 true goes on to the
        // test), so the mines are hit and then crushed or set off (0x492350)
        if (moveClass == 3 || moveClass == 4)
            if (auto ot = setup.units->find(o.type); ot != setup.units->end() && ot->second.field28)
                if (const IndexedImage* image = res.GetFrame(ot->second.field28, 0))
                    if (image->height < (moveClass == 3 ? 0x30 : 0x20) && !Crushable(o)) return false;
        return true;
    }

    // A renderer's height as 0x4929A0 takes it (+0x2E less +0x2C): an IOB model's is its header's 0x12 (0x49E1E0:
    // +0x2E half its depth, +0x2C that less 0x12), a sprite's its first frame's.
    // *Provisional*: a WOF model's (its posed bounds) is taken as tall.
    int BattleMap::RendererHeight(int type) const
    {
        auto t = setup.units->find(type);
        if (t == setup.units->end() || !t->second.field28) return 10000;
        const GType* g = res.FindGType(t->second.field28);
        if (g && g->gclass == 1)
        {
            auto m = models.find(t->second.field28);
            return m != models.end() && !m->second.model.patches.empty() ? m->second.model.field12 : 10000;
        }
        if (const IndexedImage* image = res.GetFrame(t->second.field28, 0)) return image->height;
        return 10000;
    }

    // 0x4929A0 for a scenery object: not a tree (+0xB8 bit 2); a walker (+0x52 3 or 4) passes over one under 0x30
    // (0x20) tall that can't be crushed; then (it isn't a vehicle, bit 0) on a bridge link not the bridges' pieces
    // (0x1BC-0x1BF, 0x1CC-0x1CE) nor 0x1C0; else whether it has +0xB8 bit 1 (category 0, 2 or 3).
    bool BattleMap::CollidesProp(const Unit& u, const Prop& p) const
    {
        if (p.tree) return false;     // +0xB8 bit 2 (0x47D828: made as 0xCF-0xE5, burnt since or not)
        auto t = setup.units->find(u.type);
        const int moveClass = t == setup.units->end() ? 0 : t->second.field52;
        if ((moveClass == 3 || moveClass == 4) && RendererHeight(p.type) < (moveClass == 3 ? 0x30 : 0x20)) return false;
        if (u.drive.link && ((p.type >= 0x1BC && p.type <= 0x1BF) || (p.type >= 0x1CC && p.type <= 0x1CE) || p.type == 0x1C0)) return false;
        auto pt = setup.units->find(p.type);
        if (pt == setup.units->end()) return false;
        const int c = pt->second.category;
        return c == 0 || c == 2 || c == 3;
    }

    // 0x47FE10 -> 0x47FD40: in each of the four buckets round the point in turn, the nearest object that gets in the
    // unit's way (vt+0xBC) and is nearer (octagonal) than its radius (+0xAC) + the reach; the first bucket with one
    // answers. The grid holds the units and the scenery (not a tree circle's trees nor a building's parts, which
    // are children, 0x48AAA0).
    bool BattleMap::ObjectAt(const Unit& u, int px, int py, int reach, Obstacle& out) const
    {
        const int size = setup.zMapRes;
        const int cols = (land.WorldWidth() + size - 1) / size;
        const int rows = (int((int64_t(land.Cell()) * land.Height()) >> 16) + size - 1) / size;
        auto clampTo = [](int v, int count) { return v < 0 ? 0 : v >= count ? count - 1 : v; };
        for (auto [gx, gy] : BucketCells(px, py))
        {
            bool any = false;
            int best = 10000;
            auto consider = [&](int ox, int oy, int radius) {
                if (clampTo(ox / size, cols) != gx || clampTo(oy / size, rows) != gy) return false;
                const int a = std::abs(px - ox), b = std::abs(py - oy);
                const int d = b - (std::min(a, b) >> 1) + a;
                if (!(d < radius + reach && d < best)) return false;
                best = d;
                return true;
            };
            for (const Unit& o : units)
            {
                if (&o == &u || !Collides(u, o)) continue;
                if (!consider(int(o.x), int(o.y), o.drive.radius)) continue;
                any = true;
                out = { &o, o.x, o.y, o.drive.radius, land.HeightAt(int(o.x), int(o.y)) + int(o.drive.lift), o.type };
            }
            for (const Prop& p : props)
            {
                if (!p.alive || p.circle >= 0 || !CollidesProp(u, p)) continue;
                const int radius = Radius44(p.type);
                if (!consider(int(p.x), int(p.y), radius)) continue;
                any = true;
                out = { nullptr, p.x, p.y, radius, land.HeightAt(int(p.x), int(p.y)) + int(p.z), p.type };
            }
            if (any) return true;
        }
        return false;
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
    // *Provisional*: bit 7 and the bridge links aren't in (the ground check always runs).
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
        const int z = land.HeightAt(int(u.x), int(u.y)) + int(d.lift);    // +0x80 (0x492350)
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
            return !d.link;     // not on a bridge link (+0x18C)
        };
        for (int i = 0; i < 4; i++)
        {
            // ahead, then left, then right, a radius out
            if (blocked(px + fx, py + fy) || blocked(px - fy, py + fx) || blocked(px + fy, py - fx))
            {
                d.flags = (d.flags & ~0x10u) | 8u;
                if (i == 0 && (moveClass == 5 || moveClass == 6)) return true;
            }
            Obstacle o;
            bool hit = ObjectAt(u, int(px), int(py), d.radius, o);
            if (hit && std::abs(o.z - z) > 64) hit = false;
            if (hit)
            {
                d.ax = float(double(o.x) - px);
                d.ay = float(double(o.y) - py);
                d.flags &= ~0x10u;
                if (o.unit && o.unit->group == u.group && o.unit->players == u.players && !(o.unit->vehicle && !(o.unit->drive.flags & 1)))
                    d.flags |= 0x10;
                d.flags |= 8;
                if (i != 0) return false;
                // one that can be crushed (vt+0xD8), at the unit's own position: 1 in 4 (rand() < 0x2000) its type's
                // +0x98 goes off at the unit's point, ftol(+0x84 + its +0x41 byte x 1.414) up, with no firer, and the
                // type is met (byte_51A19C)
                if (o.unit && Crushable(*o.unit))
                {
                    if (random::Rand() < 0x2000)
                        if (auto ot = setup.units->find(o.type); ot != setup.units->end())
                        {
                            const int crushedType = o.type;
                            Explode(ot->second.field98, int(u.x), int(u.y), int(double(d.lift) - double(ot->second.size41) * -1.414));
                            encountered.insert(crushedType);
                        }
                    return false;
                }
                const int dist = (std::abs(int(d.ay)) - (std::min(std::abs(int(d.ax)), std::abs(int(d.ay))) >> 1)) + std::abs(int(d.ax));
                const float fd = float(dist);
                const float gap = float(double(fd) - o.radius - u.drive.radius);
                if (!(fd > 0.0f)) return false;
                const float pushY = float(double(d.ay) / fd * gap);
                const float pushX = float(double(d.ax) / fd * gap);
                // vt+0x14C: 8.0 (0x489900), the water classes' and drones' 1.0
                const double give = Give(u.type);
                u.x = float(double(pushX) / give + u.x);
                u.y = float(double(pushY) / give + u.y);
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
    // (Only the vehicle members count, update 134.)
    void BattleMap::SetUpGrounds()
    {
        const size_t count = std::max(setup.groups.size(), groupTargets.size());
        grounds.assign(count, GroundGroup{});
        std::vector<float> best(count, -1000.0f);
        std::vector<int> members(count, 0);
        std::vector<bool> any(count, false);
        for (const Unit& u : units)
        {
            if (u.group < 0 || size_t(u.group) >= grounds.size()) continue;
            GroundGroup& g = grounds[size_t(u.group)];
            auto t = setup.units->find(u.type);
            // the vehicles (+0xB8 bit 0: category 1, set as the object's made, so before it starts)
            if (t == setup.units->end() || t->second.category != 1) continue;
            any[size_t(u.group)] = true;
            members[size_t(u.group)]++;
            const int c = t->second.field52;
            // +0x5A: record 10's 17th value (dword_4F6E78) added up, -1 for good once one is
            if (g.bridgeFactor != -1 && setup.moveCosts)
            {
                auto m = setup.moveCosts->find(c);
                const int v = m == setup.moveCosts->end() ? 0 : m->second[16];
                g.bridgeFactor = v == -1 ? -1 : int16_t(g.bridgeFactor + v);
            }
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
            if (grounds[i].bridgeFactor != -1 && members[i]) grounds[i].bridgeFactor /= members[i];
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
    // Going is vt+0x144's test (a stopped vehicle on a bridge link still goes, at 0.2 or more).
    // The ground's cost is its group's (0x480680, not strict) at the vertex nearest the unit.
    void BattleMap::SpeedControl(Unit& u, float wanted)
    {
        auto& d = u.drive;
        auto t = setup.units->find(u.type);
        float top = t == setup.units->end() ? 0.0f : float(double(int16_t(t->second.warMapSpeed)) * 0.02);
        // the Tunnelling Track Layers' vt+0x12C (0x493D00): under (+0x190 bit 0) the speed / 65, or / (65 / the
        // Enhanced Drill multiplier, 0x265: dword_4CA1A4) once Human research 34 (dword_588CC8) is done
        if (t != setup.units->end() && u.type >= 7 && u.type <= 9 && u.under)
        {
            float by = 65.0f;
            if (setup.researchDone && setup.researchDone(true, 34))
                by = 65.0f / (setup.researchMultiplier ? setup.researchMultiplier(true, 0x265) : 1.0f);
            top = float(int16_t(t->second.warMapSpeed)) / by;
        }
        {
            const int vx = (int(uint32_t(int(u.x)) << 16) + land.HalfCell()) / land.Cell();
            const int vy = (int(uint32_t(int(u.y)) << 16) + land.HalfCell()) / land.Cell();
            float over = float(double(CellCost(u.group, vx, vy, false)) / setup.vehicles.speedCostDivisor);
            if (over < 1.0f) over = 1.0f;
            top = float(double(top) / over);
        }
        if (d.link) top = 1.0f;     // on a bridge
        if (!(top <= wanted)) top = wanted;
        // going is vt+0x144 (0x4909D0, at 0x492E8E): not stopped, or held (bit 5), or on a bridge link, and not
        // frozen
        if ((!(d.flags & 1) || (d.flags & 0x20) || d.link) && !u.Frozen())
        {
            if (double(top) < 0.2) top = 0.2f;
            if (d.speed < top) d.speed = float(double(d.speed) - double(top) * -0.050000001);
            else if (d.speed > top) d.speed = float(double(d.speed) - double(top) * 0.1);
        }
        else if (d.speed > 0.0f) d.speed = float(double(d.speed) - double(top) * 0.1);
        else d.speed = 0.0f;
    }

    // 0x491870: the distance to the target (+0xFC, whole). On a route, short of its last node: the next waypoint
    // once inside the arrival radius, or once the node after is nearer than this one (off a bridge link); at the last
    // node, or with no route, it has arrived inside the arrival radius + the offset (+0x13A), and the route is let
    // go. On arriving (once: bit 1): its group counts it (0x482450) and 0x808B(1) stops it 9 x (members not yet
    // arrived) + 60 updates on.
    // *Provisional*: the sector stats and the selection refresh aren't in.
    void BattleMap::Advance(Unit& u)
    {
        auto& d = u.drive;
        auto distanceTo = [&](int x, int y) {
            const float fx = float(double(x) - u.x), fy = float(double(y) - u.y);
            return int(std::sqrt(double(fx) * fx + double(fy) * fy));
        };
        d.dist = distanceTo(d.tx, d.ty);
        bool reached = false;
        if (d.route && d.waypoint < int(d.route->nodes.size()) - 1)
        {
            const RouteNode& nd = d.route->nodes[size_t(d.waypoint + 1)];
            const int32_t cell = land.Cell(), half = land.HalfCell();
            const int next = distanceTo(int32_t(int64_t(nd.x) * cell - half) >> 16, int32_t(int64_t(nd.y) * cell - half) >> 16);
            if (next < d.dist && !d.link) reached = true;
            if (float(d.dist) < d.arrival) reached = true;
        }
        else if (float(d.offset) + d.arrival > float(d.dist)) reached = true;
        if (!reached) return;
        if (d.route)
        {
            if (d.waypoint < int(d.route->nodes.size()) - 1)
            {
                d.waypoint++;
                SetWaypoint(u, d.waypoint);
                return;
            }
            d.waypoint = 0;
            d.route.reset();
        }
        // (0x491A96, after the 0x808B below) with bit 9 (retreating, 0x80B9 with 1): its war map object (+0x48) told
        // 0x80B9 (the sector's room count), bit 9 cleared, and it's taken off the map (vt+0x18): LeaveMap
        if (d.flags & 2) return;
        // 0x491A0C: with a point aimed at (+0xC8 and +0xCC both set, 0x496200) that isn't its target, that point
        // becomes the target and it hasn't arrived: it drives on toward what it's aiming at
        if (u.aimX && u.aimY && (d.tx != u.aimX || d.ty != u.aimY))
        {
            d.tx = u.aimX;
            d.ty = u.aimY;
            return;
        }
        d.flags |= 2;
        if (d.flags & 0x200)
        {
            d.flags &= ~0x200u;
            u.leaving = true;
        }
        // 0x491A4F: its group counts it (0x482450: +0x56 up; at the group's count it goes back to 0 and bit 0 is
        // cleared, 0x480EB0), then 0x808B(1) 9 x (the count - +0x56) + 60 updates on. The count isn't reset by a new
        // order (0x482510), so arrivals carry over.
        int left = 0;
        if (const int g = u.group; g >= 0 && size_t(g) < groupArrived.size())
        {
            const int count = GroupCount(g);
            if (++groupArrived[size_t(g)] >= count)
            {
                groupArrived[size_t(g)] = 0;
                if (size_t(g) < groupBusy.size()) groupBusy[size_t(g)] = false;
            }
            left = count - groupArrived[size_t(g)];
        }
        Post808B(u, 1, 9 * left + 60);
        // 0x491A9C: the bridge link dropped (+0x18C = 0), so vt+0x144 no longer counts it as going and it stops; its
        // deck height (+0x84) stays, so it stands on the deck
        d.link = 0;
    }
}
