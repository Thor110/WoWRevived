// The battle map's guns: a unit's weapon children (WProjectile base 0x4949A0: WShell 0x488410, WRocket 0x4887E0,
// WMortar 0x488930, 0x488460 and 0x48DBE0), their fire cycle (0x494DB0), aim (0x494FD0) and landing (0x4886A0 /
// 0x488830 -> 0x487F50), the mortars' shells (0x4889D0, 0x488F00), and the group's target (0x4826D0) with force fire (packet 0x13). See docs/BATTLEMAP.md
// "Weapons".
#include "battlemap/BattleMap.h"
#include "core/Bridge.h"
#include "core/Random.h"
#include "platform/Sound.h"
#include <SDL3/SDL.h>
#include <cmath>

namespace wow
{
    namespace
    {
        int Octagonal(int x0, int y0, int x1, int y1)
        {
            const int a = std::abs(x1 - x0), b = std::abs(y1 - y0);
            return b - (std::min(a, b) >> 1) + a;
        }

        // 0x43FE90: a heading (4096 a turn) and a length to x, y
        void Polar(int angle, float length, float& x, float& y)
        {
            const double a = double(angle) * 0.000244140625 * 6.283185307;
            x = float(std::cos(a) * length);
            y = float(std::sin(a) * length);
        }

        bool IsTurret(int t) { return (t >= 0x2E6 && t <= 0x313) || (t >= 0x389 && t <= 0x38C); }

        // The factory's weapon classes (0x446B30): -1 for none, 0 the shell's update (WShell 0x488410: 700-702,
        // 803-811; WProjectile 0x488460: 0x1AB-0x1B0, 0x1B5-0x1B7, 939; 0x48DBE0: 0x19A-0x19C), 1 the rocket's
        // (WRocket 0x4887E0: 0x19D-0x1A2, 0x1B8-0x1BB, 697-699, 800-802, 949-951), 2 the mortar's (WMortar 0x488930:
        // 0x197-0x199, 0x1A3-0x1AA, 0x1B1-0x1B4, 0x1C9-0x1CB, 703-708, 938, 1003-1005)
        int WeaponClass(int t)
        {
            if ((t >= 700 && t <= 702) || (t >= 803 && t <= 811) || (t >= 0x1AB && t <= 0x1B0) || (t >= 0x1B5 && t <= 0x1B7) ||
                t == 939 || (t >= 0x19A && t <= 0x19C))
                return 0;
            if ((t >= 0x19D && t <= 0x1A2) || (t >= 0x1B8 && t <= 0x1BB) || (t >= 697 && t <= 699) || (t >= 800 && t <= 802) ||
                (t >= 949 && t <= 951))
                return 1;
            if ((t >= 0x197 && t <= 0x199) || (t >= 0x1A3 && t <= 0x1AA) || (t >= 0x1B1 && t <= 0x1B4) || (t >= 0x1C9 && t <= 0x1CB) ||
                (t >= 703 && t <= 708) || t == 938 || (t >= 1003 && t <= 1005))
                return 2;
            return -1;
        }

        // 0x487F50's effect tables by size - 1: on land (0x4D0230), on water (terrain 1-3, 0x4D0340), and over 64
        // (0x4D0450)
        constexpr int kImpactLand[67] = { 381, 382, 383, 384, 385, 386, 387, 388, 389, 390, 484, 485, 486, 487, 488, 489,
            490, 491, 492, 493, 494, 495, 496, 497, 498, 499, 500, 501, 502, 503, 504, 505, 506, 507, 508, 509, 510, 511,
            512, 513, 715, 716, 717, 718, 719, 720, 788, 789, 790, 812, 813, 814, 815, 816, 817, 818, 819, 820, 821, 822,
            823, 946, 947, 948, 1006, 1007, 1008 };
        constexpr int kImpactWater[67] = { 391, 392, 514, 515, 516, 517, 518, 519, 520, 521, 522, 523, 524, 525, 526, 527,
            528, 529, 530, 531, 532, 533, 534, 535, 536, 537, 538, 539, 540, 541, 542, 543, 544, 545, 546, 547, 548, 549,
            550, 551, 715, 716, 717, 718, 719, 720, 788, 789, 790, 812, 813, 814, 815, 816, 817, 818, 819, 820, 821, 822,
            823, 946, 947, 948, 520, 521, 522 };
        constexpr int kImpactBig[67] = { 393, 394, 552, 553, 554, 555, 556, 557, 558, 559, 560, 561, 562, 563, 564, 565,
            566, 567, 568, 569, 570, 571, 572, 573, 574, 575, 576, 577, 578, 579, 580, 581, 582, 583, 584, 585, 586, 587,
            588, 589, 715, 716, 717, 718, 719, 720, 788, 789, 790, 812, 813, 814, 815, 816, 817, 818, 819, 820, 821, 822,
            823, 946, 947, 948, 1006, 1007, 1008 };

        // dword_4B84C8 (30) x the word / 20, as 0x494DB0 works it (0x66666667, >> 3)
        int Ticks(int word) { return int((int64_t(word * 30) * 0x66666667LL >> 32) >> 3) + ((word * 30) < 0 ? 1 : 0); }
    }

    std::array<int, 8> BattleMap::WeaponData(int type) const
    {
        if (setup.weapons)
            if (auto w = setup.weapons->find(type); w != setup.weapons->end()) return w->second;
        return {};
    }

    // The guns as the unit's children start (0x47EF30): its parts that are weapons, and those of its turrets
    // (0x489820: a vehicle of their own, aiming their own children)
    void BattleMap::SetUpGuns(Unit& u)
    {
        u.guns.clear();
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        auto addGun = [&](int type) {
            const int cls = WeaponClass(type);
            if (cls < 0) return;
            Gun g;
            g.type = type;
            g.cls = cls;
            u.guns.push_back(g);
        };
        for (const auto& part : t->second.parts)
        {
            if (IsTurret(part.a))
            {
                if (auto tt = setup.units->find(part.a); tt != setup.units->end())
                    for (const auto& sub : tt->second.parts) addGun(sub.a);
            }
            else addGun(part.a);
        }
    }

    // The selection's force fire at a point (packet 0x13, or the next click in mode 1 from the popup's Force Fire,
    // 0x481CA0): speech 0x10C, the target a point (+0x2C -1) at its ground height (0x47B170), the guns' aim and the
    // move into range follow from the group's update
    void BattleMap::ForceFire(int x, int y)
    {
        bool any = false;
        for (size_t g = 0; g < selected.size() && g < groupTargets.size(); g++)
        {
            if (!selected[g]) continue;
            bool members = false;
            for (const Unit& u : units) if (u.players && u.group == int(g) && !u.guns.empty()) members = true;
            if (!members) continue;
            GroupTarget& t = groupTargets[g];
            t.target = -1;
            t.x = x;
            t.y = y;
            t.z = land.HeightAt(x, y);
            t.lastX = t.lastY = t.lastZ = -1;
            groupMode[g] = 0;
            if (!any) Speak(0x10C, int(g));
            any = true;
        }
    }

    BattleMap::Unit* BattleMap::UnitById(int id)
    {
        for (Unit& u : units)
            if (u.id == id) return &u;
        return nullptr;
    }

    // Packet 0x10, an object attacked (0x4812B0 -> 0x481F70), for each selected group: an object of type 0x3DB is
    // its own case (0x481170, or speech 0x121); any other: each member told 0x80B9, then 0x482880 with it (each
    // member 0x80B9 again, the target +0x2C and its id +0x30, where it was last aimed (+0x34..+0x3C) -1), +0x1D8
    // bit 0 set (0x480EB0(1)), vt+0x38, and for a player's group (bit 4) whose first member is a vehicle speech
    // 0x10C; then bit 3. The move into range and the aim follow from the group's update (0x4826D0).
    // *Provisional*: type 0x3DB's case, 0x80B9 and vt+0x38 aren't in.
    void BattleMap::AttackObject(int id)
    {
        const Unit* target = UnitById(id);
        if (!target || target->type == 0x3DB) return;
        bool any = false;
        for (size_t g = 0; g < selected.size() && g < groupTargets.size(); g++)
        {
            if (!selected[g] || int(g) == target->group) continue;
            const Unit* first = nullptr;
            for (const Unit& u : units)
                if (u.group == int(g)) { first = &u; break; }
            if (!first || !first->players) continue;
            GroupTarget& t = groupTargets[g];
            t = GroupTarget{};
            t.target = id;
            groupMode[g] = 0;
            auto ft = setup.units->find(first->type);
            if (!any && ft != setup.units->end() && ft->second.category == 1) Speak(0x10C, int(g));
            any = true;
        }
    }

    // vt+0xC4 (0x491100 -> 0x496300): unless it says not (vt+0xF8), at another object: d = ftol(the distance - -1.0),
    // r its radius (+0xAC) / 3 for a category 0, 2 or 3 object (+0xB8 bit 1), else 0; the point aimed at is the
    // object's less (it - this) x r / d across and down, its height plus its +0x41 byte x 1.414 (vt+0xC8)
    void BattleMap::Approach(Unit& u, const Unit& target)
    {
        if (&u == &target) return;
        auto t = setup.units->find(target.type);
        if (t == setup.units->end()) return;
        const float dx = target.x - u.x, dy = target.y - u.y;
        const int d = int(double(std::sqrt(dy * dy + dx * dx)) - -1.0);
        const int cat = t->second.category;
        const int r = (cat == 0 || cat == 2 || cat == 3) ? target.drive.radius / 3 : 0;
        const float fr = float(r), fd = float(d);
        const int x = int(target.x - dx * fr / fd);
        const int y = int(target.y - dy * fr / fd);
        const float tz = float(land.HeightAt(int(target.x), int(target.y))) + target.drive.lift;
        const int z = int(double(tz) - double(t->second.size41) * -1.414);
        Aim(u, x, y, z);
    }

    // flt_59E44C, the sight's scale for looking round (0x48F8A0, 0x496570): nothing in the program writes it, so it
    // keeps the 0.0 it starts with. *Provisional*: if something unseen sets it, the scans below find targets.
    static constexpr float kLookScale = 0.0f;

    // The unit's update (0x4967E0): +0xDA counts up; past its type's +0x92 word it looks round (vt+0x104, 0x48F8A0:
    // with +0xE4 bit 1 and further than its sight x the scale from its post (+0xC8 / +0xCC), its group's guns are
    // stood down (0x805A); then 0x496890: on stance 2 or 3 (+0xD8) with its group not attacking (+0x1D8 bit 0
    // clear, or bit 2 set) it picks a target (0x496570), and the count goes back to 0).
    // *Provisional*: +0xE4 (posts) and 0x496890's vt+0xAC aren't in.
    void BattleMap::LookRound(size_t index)
    {
        Unit& u = units[index];
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        const int count = u.scanCount++;
        if (count <= int16_t(t->second.field92)) return;
        const int stance = u.group >= 0 && size_t(u.group) < groupStance.size() ? groupStance[size_t(u.group)] : 1;
        if (stance >= 2 && stance <= 3 && size_t(u.group) < groupTargets.size() && groupTargets[size_t(u.group)].target == 0)
            AutoTarget(u);
        u.scanCount = 0;
    }

    // 0x496570: within its sight (+0x48) x the scale (octagonal, whole positions), with it not under the ground,
    // every other side's object that can be targeted (vt+0x6C, +0xC0 bit 0): an armed one (+0x3C) is the target
    // at once; else the battle's key building (0x33E / 0x78, 0x116 / 0x13A by the kind) if seen, else the last seen.
    // The group gets 0x80B1 with it: for a player's group (+0x1D8 bit 4) and a vehicle target (+0xB8 bit 0), if
    // rand() is under 5000 speech 0x10F; then 0x482960: as 0x482880, the target set and bit 0 set.
    // *Provisional*: a player's unit seeing an unarmed one also tells the battle (0x80B1 to dword_4E9B78), not in.
    void BattleMap::AutoTarget(const Unit& u)
    {
        auto t = setup.units->find(u.type);
        if (t == setup.units->end() || u.drive.lift < 0.0f) return;
        const int sight = int(float(ObjOjd::Vision(t->second, setup.armour)) * kLookScale);
        const Unit* key = nullptr;
        const Unit* last = nullptr;
        const Unit* pick = nullptr;
        for (const Unit& o : units)
        {
            if (Octagonal(int(u.x), int(u.y), int(o.x), int(o.y)) >= sight) continue;
            if (o.health <= 0 || o.players == u.players) continue;
            if (!o.guns.empty()) { pick = &o; break; }
            last = &o;
            if (o.type == (setup.sector == 3 ? 0x33E : 0x78) || o.type == (setup.sector == 30 ? 0x116 : 0x13A)) key = &o;
        }
        if (!pick) pick = key ? key : last;
        if (!pick) return;
        const int g = u.group;
        if (u.players)
            if (auto pt = setup.units->find(pick->type); pt != setup.units->end() && pt->second.category == 1 && random::Rand() < 5000)
                Speak(0x10F, g);
        GroupTarget& gt = groupTargets[size_t(g)];
        gt = GroupTarget{};
        gt.target = pick->id;
    }

    // 0x482880(0) / the guns' 0x805A (0x494D80): not armed; a cycle under way goes to its reload; aimed set
    void BattleMap::StopAttacking(int group)
    {
        if (group < 0 || size_t(group) >= groupTargets.size()) return;
        groupTargets[size_t(group)] = GroupTarget{};
        for (Unit& u : units)
            if (u.group == group)
                for (Gun& g : u.guns)
                {
                    g.armed = false;
                    if (g.state != 0) g.state = 4;
                    g.flags |= 1;
                }
    }

    // 0x483B70 -> 0x4826D0, each update for a group with a target: a point target that's moved more than 80 (0x43FF00)
    // from where it was last aimed is aimed at again: the group moves (0x804F) to within its range (+0x4E, in
    // vertices, as the route's radius) and each member is aimed there (vt+0xC8, a point) or at it (vt+0xC4, an
    // object). An object's target follows it (+0x78..+0x80 each update); under the ground its guns are stood down.
    // *Provisional*: a target that's gone (0x8057's tidy-up) clears the group's target; the route's radius is the range snapped
    // to vertices whatever the goal's snap (the "CAN'T GET TO WITHIN ATTACK DISTANCE!" check isn't in).
    void BattleMap::ChaseTargets()
    {
        for (size_t g = 0; g < groupTargets.size(); g++)
        {
            GroupTarget& t = groupTargets[g];
            if (t.target == 0) continue;
            const Unit* object = nullptr;
            if (t.target > 0)
            {
                // an object: gone, the target's cleared; under the ground (+0x84 under 0), the guns are told 0x805A
                // and +0x1D8 bit 0 cleared; else the point is where it is (+0x78..+0x80)
                object = UnitById(t.target);
                if (!object) { StopAttacking(int(g)); continue; }
                if (object->drive.lift < 0.0f)
                {
                    for (Unit& u : units)
                        if (u.group == int(g))
                            for (Gun& gun : u.guns)
                            {
                                gun.armed = false;
                                if (gun.state != 0) gun.state = 4;
                                gun.flags |= 1;
                            }
                    continue;
                }
                t.x = int(object->x);
                t.y = int(object->y);
                t.z = int(float(land.HeightAt(t.x, t.y)) + object->drive.lift);
            }
            if (Octagonal(t.x, t.y, t.lastX, t.lastY) <= 80) continue;
            t.lastX = t.x;
            t.lastY = t.y;
            t.lastZ = t.z;
            std::vector<size_t> members;
            for (size_t i = 0; i < units.size(); i++)
                if (units[i].group == int(g) && units[i].vehicle) members.push_back(i);
            const int radius = int((int64_t(int32_t(uint32_t(groupRange[g]) << 16)) + land.HalfCell()) / land.Cell());
            OrderGroupMove(int(g), members, t.x, t.y, radius);
            const int id = t.target;
            for (Unit& u : units)
                if (u.group == int(g))
                {
                    if (id == -1) Aim(u, t.x, t.y, t.z);
                    else if (const Unit* o = UnitById(id)) Approach(u, *o);
                }
        }
    }

    // vt+0xC8 (0x491110): not under the ground (+0x84 under 0) or on a bridge link, the point kept on the map, then
    // 0x496200: the goal kept and the children all aimed at one point jittered by up to the distance / 16 + 1 each
    // way (rand() % twice that, less it, for x, y and height, drawn once); a turret (a vehicle) does the same for its guns, whose vt+0xC8
    // (0x494D20) keeps the point and arms them (a gun in flight, state 2, goes to 3).
    // *Provisional*: the turret's step is taken at the unit's position (its offset and turning aren't in).
    void BattleMap::Aim(Unit& u, int x, int y, int z)
    {
        if (u.drive.lift < 0.0f || u.drive.link) return;
        x = std::clamp(x, 0, land.WorldWidth() - 1);
        y = std::clamp(y, 0, int((int64_t(land.Cell()) * land.Height()) >> 16) - 1);
        auto jitter = [&](int gx, int gy, int gz, int& ox, int& oy, int& oz) {
            const double dx = double(gx) - u.x, dy = double(gy) - u.y;
            const int d = int(std::sqrt(dy * dy + dx * dx) - -1.0);
            const int s = (d + ((d >> 31) & 0xF)) / 16 + 1;
            ox = gx + random::Rand() % (2 * s) - s;
            oy = gy + random::Rand() % (2 * s) - s;
            oz = gz + random::Rand() % (2 * s) - s;
        };
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        // the unit's 0x496200: one jittered point for all its children
        int cx = 0, cy = 0, cz = 0;
        jitter(x, y, z, cx, cy, cz);
        size_t gun = 0;
        auto arm = [&](int ax, int ay, int az) {
            if (gun >= u.guns.size()) return;
            Gun& g = u.guns[gun++];
            g.tx = ax;
            g.ty = ay;
            g.tz = az;
            g.armed = true;
            if (g.state == 2) g.state = 3;
        };
        for (const auto& part : t->second.parts)
        {
            if (IsTurret(part.a))
            {
                // the turret's 0x491110 -> 0x496200: one more jittered point for its guns
                auto tt = setup.units->find(part.a);
                if (tt == setup.units->end()) continue;
                int gx = 0, gy = 0, gz = 0;
                jitter(cx, cy, cz, gx, gy, gz);
                for (const auto& sub : tt->second.parts)
                    if (WeaponClass(sub.a) >= 0) arm(gx, gy, gz);
            }
            else if (WeaponClass(part.a) >= 0) arm(cx, cy, cz);
        }
    }

    // 0x494DB0, each update: 0 waits out its count, then, armed and aimed (+0xF0 bit 0), goes to 1 if its owner lets
    // it (vt+0x84: 0x496920, the group's mode 0 for an ordinary gun; not frozen, vt+0x108), else drops aimed; 1 sets
    // the count to ftol(record 12's +2 x 30 / 20) + rand() % 15 and plays its sound (+0xA2) at its point, then 2;
    // 2 counts down to 3; 3 tells its owner it fired (vt+0xD0, 0x496BC0) and goes to 4; 4 sets the count to
    // ftol(+4 x 30 / 20) + rand() % 15 and goes back to 0. Then aimed (0x494FD0): the point within record 12's +0 x
    // +0xC8 (1.0) of it, and within its hardpoint's arc (+0xB0: +4 the middle, +6 the half width). The class's update
    // then lands the shot: the shell's (0x4886A0) once in state 2 when the count is a multiple of 5, the rocket's
    // (0x488830) in state 3.
    // The mortar's update (0x488CA0) launches its shells as the cycle goes to 1 (0x4889D0, Launch).
    // *Provisional*: the arcs (hardpoints) and
    // the research multipliers (+0xC8 / +0xCC, 0x494AF0) aren't in; the gun is at its unit's point.
    void BattleMap::GunTick(size_t index, Gun& g)
    {
        Unit& u = units[index];
        const auto w = WeaponData(g.type);
        switch (g.state)
        {
        case 0:
            if (g.counter > 0) g.counter--;
            else if (g.armed && (g.flags & 1))
            {
                const int mode = u.group >= 0 && size_t(u.group) < groupMode.size() ? groupMode[size_t(u.group)] : 0;
                if (mode == 0) g.state = 1;
                else g.flags &= ~1u;
            }
            break;
        case 1:
        {
            g.flags &= ~2u;
            g.state = 2;
            const int r = random::Rand() % 15;
            g.counter = Ticks(w[1]) + r;
            auto t = setup.units->find(g.type);
            if (t != setup.units->end() && t->second.fieldA2 && setup.sound)
                setup.sound->Play(t->second.fieldA2, 1.0f, 0, int(u.x), land.ProjectY(int(u.y), land.HeightAt(int(u.x), int(u.y))));
            break;
        }
        case 2:
            if (g.counter > 0) g.counter--;
            else g.state = 3;
            break;
        case 3:
            g.state = 4;
            break;
        case 4:
        {
            const int r = random::Rand() % 15;
            g.counter = Ticks(w[2]) + r;
            g.state = 0;
            break;
        }
        default: break;
        }
        // 0x494FD0
        {
            int heading = 0;
            float length = 0;
            HeadingAndLength(u.x, u.y, float(g.tx), float(g.ty), heading, length);
            const bool inRange = float(w[0]) * 1.0f > length;
            g.flags = (g.flags & ~1u) | (inRange ? 1u : 0u);
        }
        if (g.cls == 0 && g.state == 2 && !(g.flags & 2) && g.counter % 5 == 0)
        {
            Impact(index, g);
            g.flags |= 2;
        }
        else if (g.cls == 1 && g.state == 3) Impact(index, g);
        else if (g.cls == 2 && g.state == 1) Launch(index, g);
    }

    // 0x4889D0, the mortar's update (0x488CA0) with its cycle just gone to 1: with its list (+0xB4), h its height
    // over the ground (ftol(+0x80 less the ground's height, 0x47B170)), the heading and length to the aim point
    // (0x43FDA0), and the reach R = the length less (its height less the aim's), at least 0.1 (dbl_4AE1B8). For each
    // entry (the type's emitter: +4 the shell's type, +8 the spread, +0xA the limit, +0x14 the gravity), one shell:
    // s = ftol(+8 / +0xCC (1.0)); the heading rand() % 2s + heading - s, the elevation rand() % 2s - s + 0x200 (an
    // eighth of a turn), as radians (x 0.0015339808); the speed v = sqrt(-(R x gravity) / (2 sin a cos a)); its
    // horizontal and upward parts (0x43FE90), the updates ftol(the length / horizontal - -1.0); the shell (0x446B30)
    // gets the horizontal part along the heading and the upward as its velocity (vt+0x80), the mortar's point
    // (vt+0x40), h as its height (+0x84), the gravity (+0x94), record 12's +6 (+0xC0), the updates (vt+0xE0, +0xC2)
    // and its root's id as the firer (+0xC4); byte_51A19C of the mortar's type set. Then the entry's count goes up;
    // past a limit that isn't 0 the list is freed and the mortar fires no more.
    // *Provisional*: the shell starts at the unit's point (the mortar's place on it isn't in).
    void BattleMap::Launch(size_t index, Gun& g)
    {
        if (g.spent) return;
        const Unit& u = units[index];
        auto t = setup.units->find(g.type);
        if (t == setup.units->end() || t->second.emitters.empty()) return;
        const auto& list = t->second.emitters;
        if (g.fired.size() != list.size()) g.fired.assign(list.size(), 0);
        const int ground = land.HeightAt(int(u.x), int(u.y));
        const float z = float(ground) + u.drive.lift;
        const int h = int(z - float(land.HeightAt(int(u.x), int(u.y))));
        int heading = 0;
        float length = 0;
        HeadingAndLength(u.x, u.y, float(g.tx), float(g.ty), heading, length);
        float reach = length - (z - float(g.tz));
        if (reach < 0.1) reach = 0.1f;
        const int size = WeaponData(g.type)[3];
        for (size_t i = 0; i < list.size(); i++)
        {
            const auto& e = list[i];
            const int s = int(float(e.b) / 1.0f);
            if (s <= 0) continue;
            const int dir = random::Rand() % (2 * s) + heading - s;
            const int up = random::Rand() % (2 * s) - s + 0x200;
            const double a = double(up) * 0.001533980787841797;
            const float speed = float(std::sqrt(double(-(reach * e.f[2])) / (std::sin(a) * std::cos(a) * 2.0)));
            float across = 0, upward = 0;
            Polar(up, speed, across, upward);
            const int count = int(double(length / across) - -1.0);
            Shell sh;
            sh.type = e.a;
            sh.x = u.x;
            sh.y = u.y;
            Polar(dir, across, sh.vx, sh.vy);
            sh.vz = upward;
            sh.lift = float(h);
            sh.g = e.f[2];
            sh.size = size;
            sh.count = int16_t(count);
            sh.group = u.group;
            // drawn as an effect that doesn't animate (frames 0), moved each update
            if (auto st = setup.units->find(e.a); st != setup.units->end() && st->second.field28)
                if (const GType* gt = res.FindGType(st->second.field28))
                {
                    Effect ef;
                    ef.gtype = st->second.field28;
                    ef.alive = true;
                    effects.push_back(ef);
                    sh.effect = int(effects.size()) - 1;
                    objects.push_back({ int(sh.y), int(sh.x), land.ProjectY(int(sh.y), ground + h), nullptr, nullptr, gt->paletteSlot,
                                        ef.gtype, -1, -1, 0, sh.effect });
                }
            shells.push_back(sh);
            g.fired[i]++;
            if (e.c != 0 && g.fired[i] > int16_t(e.c)) { g.spent = true; break; }
        }
    }

    // A shell's update (0x488F00): first 0x487890, the flying object's (its emitters, 0x47F050; the step, vt+0x54
    // 0x47F770: the point moved by the velocity, the height kept over the ground's change (ftol(+0x80 - +0x84) less
    // the ground's height at the new point, plus the height and the upward speed), then the upward speed plus the
    // gravity; under 0 high it's gone, 0x8057). Then at or under 0 high, or with its updates (+0xC2) run out, it
    // lands: 0x487F50 with its size, its point, ftol(the height - -8.0) up, the firer and its type; with the
    // updates below 0 it's gone.
    // *Provisional*: its emitters (the smoke trail, 367) aren't in; it's drawn with frame 0.
    void BattleMap::ShellTick(Shell& s)
    {
        const int before = land.HeightAt(int(s.x), int(s.y));
        s.x += s.vx;
        s.y += s.vy;
        const int after = land.HeightAt(int(s.x), int(s.y));
        s.lift = float(before - after) + s.lift + s.vz;
        s.vz += s.g;
        bool gone = s.lift < 0.0f;
        bool lands = false;
        if (s.lift <= 0.0f) lands = true;
        else lands = s.count-- <= 0;
        if (lands)
        {
            ImpactAt(s.size, int(s.x), int(s.y), int(s.lift - -8.0f), s.group);
            if (s.count < 0) gone = true;
        }
        s.gone = gone;
    }

    // The landing (0x4886A0 / 0x488830): the spread s = ftol(the octagonal distance from the gun / 32 / +0xCC (1.0)
    // + 1); the point's x, y and height each moved by rand() % 2s - s, the height less the ground's at the vertex
    // there (0x47B120); then 0x487F50 with record 12's +6 as the size: by the terrain there (water, 1-3, or not) and
    // the size (over 64 its own table), the effect from the tables made at the point with the firer (the gun's root,
    // the unit) and the gun's type.
    void BattleMap::Impact(size_t index, const Gun& g)
    {
        const Unit& u = units[index];
        const auto w = WeaponData(g.type);
        const int d = Octagonal(int(u.x), int(u.y), g.tx, g.ty) >> 5;
        const int s = int(float(d) / 1.0f - -1.0f);
        const int span = 2 * s;
        const int x = random::Rand() % span - s + g.tx;
        const int y = random::Rand() % span - s + g.ty;
        const int z = random::Rand() % span + g.tz - s;
        const int h = z - land.HeightAtNearest(x, y);
        ImpactAt(w[3], x, y, h, u.group);
    }

    void BattleMap::ImpactAt(int size, int x, int y, int h, int group)
    {
        int effect = 0;
        if (size > 64) effect = size - 1 < 67 ? kImpactBig[size - 1] : 0;
        else if (size >= 1)
        {
            const int vx = std::clamp(int((int64_t(int32_t(uint32_t(x) << 16)) + land.HalfCell()) / land.Cell()), 0, land.Width() - 1);
            const int vy = std::clamp(int((int64_t(int32_t(uint32_t(y) << 16)) + land.HalfCell()) / land.Cell()), 0, land.Height() - 1);
            const int terrain = land.At(vx, vy).atm & 15;
            effect = terrain >= 1 && terrain < 4 ? kImpactWater[size - 1] : kImpactLand[size - 1];
        }
        if (effect) Explode(effect, x, y, std::max(h, 0), group);
    }
}
