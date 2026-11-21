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

        // 0x487F50's effect tables by size - 1: on land (0x4D0230), on water (terrain 1-3, 0x4D0340), and in the air
        // (the impact's height over 64, 0x4D0450)
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

        // dword_4B84C8 ("Game Frequency", 30) x the word / 20 (0x66666667, >> 3)
        int Ticks(int word, int freq) { return int((int64_t(word * freq) * 0x66666667LL >> 32) >> 3) + ((word * freq) < 0 ? 1 : 0); }
    }

    std::array<int, 8> BattleMap::WeaponData(int type) const
    {
        if (setup.weapons)
            if (auto w = setup.weapons->find(type); w != setup.weapons->end()) return w->second;
        return {};
    }

    // 0x494AF0, as a gun starts (vt+0x14): by its owner's side (vt+0xDC, 0x4406E0), each enhancement done (0x45B890)
    // sets +0xC8 (its range's multiplier) or +0xCC (its spread's divisor) to that enhancement's value, the later
    // overriding the earlier. The Humans': Improved Powder Charge (item 78, 0x25A) then 79 (0x25B) the range, 82
    // (0x25E) the spread. The Martians': for the Heat-Rays (0x1AB-0x1B0) items 77 (0x27D) then 78 (0x27E) the range;
    // for all, 33 (0x289) the range and 80 (0x280) the spread.
    void BattleMap::Upgrade(Gun& g, int side) const
    {
        if (!setup.researchDone || !setup.researchMultiplier) return;
        auto set = [&](float& v, bool human, int item, int type) { if (setup.researchDone(human, item)) v = setup.researchMultiplier(human, type); };
        if (side == 0)
        {
            set(g.range, true, 78, 0x25A);
            set(g.range, true, 79, 0x25B);
            set(g.accuracy, true, 82, 0x25E);
        }
        else if (side == 1)
        {
            if (g.type >= 0x1AB && g.type <= 0x1B0)
            {
                set(g.range, false, 77, 0x27D);
                set(g.range, false, 78, 0x27E);
            }
            set(g.range, false, 33, 0x289);
            set(g.accuracy, false, 80, 0x280);
        }
    }

    // The guns as the unit's children start (0x47EF30): its parts that are weapons, and those of its turrets
    // (0x489820: a vehicle of their own, aiming their own children)
    void BattleMap::SetUpGuns(Unit& u)
    {
        u.guns.clear();
        u.turrets.clear();
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        auto addGun = [&](const ObjOjd::UnitData::Part& part, int turret) {
            const int cls = WeaponClass(part.a);
            if (cls < 0) return;
            Gun g;
            g.type = part.a;
            g.cls = cls;
            g.turret = turret;
            g.hardpoint = part.box[0] >= 0;
            g.piece = part.box[0];          // +0xB0's first two: held to that point of its parent's model
            g.vertex = part.box[1];
            g.middle = uint16_t(part.box[2]);
            g.half = uint16_t(part.box[3]);
            g.ox = int16_t(part.b);
            g.oy = int16_t(part.c);
            g.oz = int16_t(part.d);
            Upgrade(g, t->second.faction);
            u.guns.push_back(g);
        };
        for (const auto& part : t->second.parts)
        {
            if (IsTurret(part.a))
            {
                Turret tr;
                tr.type = part.a;
                tr.ox = int16_t(part.b);
                tr.oy = int16_t(part.c);
                tr.oz = int16_t(part.d);
                tr.hardpoint = part.box[0] >= 0;
                tr.middle = uint16_t(part.box[2]);
                tr.half = uint16_t(part.box[3]);
                u.turrets.push_back(tr);
                if (auto tt = setup.units->find(part.a); tt != setup.units->end())
                    for (const auto& sub : tt->second.parts) addGun(sub, int(u.turrets.size()) - 1);
            }
            else addGun(part, -1);
        }
    }

    int BattleMap::GunHeading(const Unit& u, const Gun& g) const
    {
        if (g.turret >= 0 && size_t(g.turret) < u.turrets.size()) return (u.heading + u.turrets[size_t(g.turret)].turn) & 0xFFF;
        return u.heading & 0xFFF;
    }

    // A turret's update (0x489B40): its unit's status copied (vt+0x8C -> vt+0x88, vt+0x94 -> vt+0x90); the heading
    // wanted: aiming (+0xE4 bit 1), the heading from it to the point (0x43FDA0); else its hardpoint's middle plus its
    // unit's heading. d = 0x43FF30(wanted, its heading) (its heading less the wanted, in a half turn each way): under
    // -50 it turns +50, over 50 -50, else not. Aiming, the turn is taken only if |0x43FF30(+0x190 + the step, the
    // middle)| stays under the half width; not aiming, always (+0x190 += the step). Its heading is then its unit's
    // plus +0x190 (vt+0x30). Turning, with no sound yet, a type +0xA2 that isn't 0 and vt+0x9C clear, its sound
    // starts at its point (0x4883C0 / 0x4883E0: +0xE8); not turning, a sound playing stops (vt+0x13C). Then the
    // vehicle's own update (0x4913B0).
    // With the Armoured Lorry's turret 742 (a whole turn's hardpoint) and gun 414 (+-256 of the turret's heading).
    // The turret's own point (PoseChildren) is where it turns from. *Provisional*: the vehicle update it then runs
    // isn't.
    void BattleMap::TurretTick(Unit& u, Turret& t)
    {
        const int heading = (u.heading + t.turn) & 0xFFF;
        int want = 0;
        if (t.aiming)
        {
            float len = 0;
            HeadingAndLength(float(t.x), float(t.y), float(t.aimX), float(t.aimY), want, len);
        }
        else want = (t.middle + u.heading) & 0xFFFF;
        auto diff = [](int a, int b) {
            int d = (b & 0xFFF) - (a & 0xFFF);
            while (d < -2048) d += 4096;
            while (d > 2047) d -= 4096;
            return d;
        };
        const int d = diff(want, heading);
        int step = 0;
        if (d < -50) step = 50;
        if (d > 50) step = -50;
        if (!t.aiming || std::abs(diff(t.turn + step, t.middle)) < t.half) t.turn += step;
        t.turn = int16_t(t.turn);
        if (step != 0)
        {
            auto tt = setup.units->find(t.type);
            // 0x489B40: the turning sound only out of the fog (vt+0x9C clear)
            if (!t.sound && !u.fogged && tt != setup.units->end() && tt->second.fieldA2 && setup.sound)
                t.sound = setup.sound->Play(tt->second.fieldA2, 1.0f, 0, t.x, land.ProjectY(t.y, t.z));
        }
        else if (t.sound && setup.sound)
        {
            setup.sound->Stop(t.sound);
            t.sound = 0;
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
            ForceFireGroup(int(g), x, y);
            groupOrdered[g] = true;     // packet 0x13: bit 3 (0x48167F)
            if (!any) Speak(0x10C, int(g));
            any = true;
        }
    }

    // A group's target a point (packet 0x13's case, 0x4815B1: the members 0x80B9, the target cleared, the guns stood
    // down, then +0x2C -1 and +0x40..+0x48 the point at its ground height, 0x47B170); its mode (+0x1DC) is left as it
    // is, so a special chosen from the popup fires there
    void BattleMap::ForceFireGroup(int g, int x, int y)
    {
        if (g < 0 || size_t(g) >= groupTargets.size()) return;
        GroupTarget& t = groupTargets[size_t(g)];
        t.target = -1;
        t.x = x;
        t.y = y;
        t.z = land.HeightAt(x, y);
        t.lastX = t.lastY = t.lastZ = -1;
        // the group's home becomes the point (0x4812B0's packet cases set +0x60 / +0x64; *provisional*: the
        // values they store aren't traced)
        if (size_t(g) < groupHomeX.size()) { groupHomeX[size_t(g)] = x; groupHomeY[size_t(g)] = y; }
    }

    // 0x496920 (vt+0x84 of a gun's owner): with no group it may fire; otherwise its group's mode (+0x1DC) must be the
    // one its type wants: 407-409 modes 4-6, 706-708 7-9, 697-699 10-12, 800-802 13-15, 803 / 938 / 939 16, 804-811
    // 17-24; any other gun 0
    int BattleMap::FireMode(int t)
    {
        if (t >= 0x197 && t <= 0x199) return t - 0x197 + 4;
        if (t >= 706 && t <= 708) return t - 706 + 7;
        if (t >= 697 && t <= 699) return t - 697 + 10;
        if (t >= 800 && t <= 802) return t - 800 + 13;
        if (t == 803 || t == 938 || t == 939) return 16;
        if (t >= 804 && t <= 811) return t - 804 + 17;
        return 0;
    }

    // 0x4817C0's other cases, a special from the popup (0x80F3 with its command), for a group: Force Fire (0x80ED)
    // mode 1; 0x80EE: halted (0x8031), the members 0x80B9, the target cleared, the guns stood down (0x805A), bit 0
    // cleared, mode 2; 0x80EF mode 3; 0x80D5-0x80DD modes 4-12; 0x80DE-0x80E0 13-15; 0x80E4-0x80EC 16-24; 0x80E1-0x80E3
    // each member 0x80E1-0x80E3 with 1-3, and 0x80F0 each member 0x80F0, at once.
    // 0x80EE / 0x80EF to the members are taken by one class only (0x498E90: unless vt+0x118, vt+0x184(1)).
    // (The members get 0x80EE / 0x80EF on the next click, OrderMove; the class is in Sinking.cpp.)
    void BattleMap::GroupOrder(int g, int command)
    {
        if (g < 0 || size_t(g) >= groupMode.size()) return;
        int& mode = groupMode[size_t(g)];
        if (command == 0x80ED) mode = 1;
        else if (command == 0x80EE) { StopAttacking(g); mode = 2; }
        else if (command == 0x80EF) mode = 3;
        else if (command >= 0x80D5 && command <= 0x80DD) mode = command - 0x80D5 + 4;
        else if (command >= 0x80DE && command <= 0x80E0) mode = command - 0x80DE + 13;
        else if (command >= 0x80E4 && command <= 0x80EC) mode = command - 0x80E4 + 16;
        else if ((command >= 0x80E1 && command <= 0x80E3) || command == 0x80F0)
        {
            // each member (0x4817C0's loop): 0x80E1-0x80E3 with 1-3, 0x80F0; each sends its message round (below)
            std::vector<int> ids;
            for (const Unit& u : units) if (u.group == g) ids.push_back(u.id);
            for (int id : ids)
                for (size_t i = 0; i < units.size(); i++)
                    if (units[i].id == id) { MemberSpecial(i, command == 0x80F0 ? 0x80F9 : 0x80F8, command == 0x80F0 ? 0 : command - 0x80E0); break; }
        }
    }

    // A member's 0x80E1-0x80E3 (0x80F8 with the level) or 0x80F0 (0x80F9) (0x495CB0): vt+0xFC (0x495C10) sends it to
    // every object (the world's list) within its sight (+0x48; 0x43FF00 from whole positions, under it), itself too;
    // then its group's mode is 0 (0x4807E0)
    void BattleMap::MemberSpecial(size_t index, int message, int level)
    {
        const Unit u = units[index];
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        const int sight = ObjOjd::Vision(t->second, setup.armour);
        std::vector<int> ids;
        for (const Unit& o : units)
            if (Octagonal(int(u.x), int(u.y), int(o.x), int(o.y)) < sight) ids.push_back(o.id);
        for (int id : ids)
            for (size_t i = 0; i < units.size(); i++)
                if (units[i].id == id) { ReceiveSpecial(i, message, level); break; }
        if (u.group >= 0 && size_t(u.group) < groupMode.size()) groupMode[size_t(u.group)] = 0;
    }

    // 0x80F8 is taken only by the class of 0x273-0x276 (0x48C8E0, its handler 0x48C9B0): damage of the level x 50
    // (vt+0xE0) and smoke 0x16F made a quarter of its radius + 1 about it. 0x80F9 (0x495CB0): types 7-9 and 0x16-0x18
    // unless byte_589134 is set, and 0x273-0x276 unless byte_589130, get the status 0x80F9 for 240 updates (vt+0x88
    // 0xF0, vt+0x90); whatever else, nothing.
    // *Provisional*: byte_589130 / byte_589134 are taken as the Martian research items 132 / 133 done; what the
    // status 0x80F9 does (vt+0x10C: 0x48CC00's renderer +0xE8 for 0x273-0x276, the bridges' 0x498110) isn't in; the
    // smoke is made at the unit's point by Puff.
    void BattleMap::ReceiveSpecial(size_t index, int message, int level)
    {
        Unit& u = units[index];
        const bool special = u.type >= 0x273 && u.type <= 0x276;
        auto done = [&](int item) { return setup.researchDone && setup.researchDone(false, item); };
        if (message == 0x80F8)
        {
            if (!special) return;
            Damage(index, level * 50);
            Puff(0x16F, u.type, u.x, u.y, u.drive.lift, u.drive.radius * 3 / 4);
            return;
        }
        if (message != 0x80F9) return;
        const bool machine = (u.type >= 7 && u.type <= 9) || (u.type >= 0x16 && u.type <= 0x18);
        if ((machine && !done(133)) || (special && !done(132)))
        {
            u.statusTime = 240;
            ChangeStatus(u, 0x80F9);
        }
    }

    // 0x482570, from the group's target chase (0x4826D0, so only while it has a target) for a computer's group
    // (+0x1D8 bit 7: not a player's, the computer player played by the machine, 0x4802E0) when +0x58 is a multiple
    // of 8: rand() % 27 picks 0x80F0 (0), 0x80DB-0x80DD (3-5), 0x80D5-0x80DA (6-11), 0x80E1-0x80EC (15-26) or
    // nothing (1, 2, 12-14); if its first member can (0x47DA80) the group's given it (0x4817C0)
    void BattleMap::ComputerSpecials(int g)
    {
        static constexpr int table[27] = { 0x80F0, 0, 0, 0x80DB, 0x80DC, 0x80DD, 0x80D5, 0x80D6, 0x80D7, 0x80D8, 0x80D9, 0x80DA,
            0, 0, 0, 0x80E1, 0x80E2, 0x80E3, 0x80E4, 0x80E5, 0x80E6, 0x80E7, 0x80E8, 0x80E9, 0x80EA, 0x80EB, 0x80EC };
        const int command = table[random::Rand() % 27];
        if (!command) return;
        const Unit* first = nullptr;
        for (const Unit& u : units) if (u.group == g) { first = &u; break; }
        if (first && CanDo(first->type, command)) GroupOrder(g, command);
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
    // (Type 0x3DB's case, the reinforcement markers: RetreatToMarker.) *Provisional*: 0x80B9 isn't sent.
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
            auto ft = setup.units->find(first->type);
            // in mode 1 (Force Fire from the popup) the object's point is the target, and the mode 0 (0x481423);
            // otherwise the mode stays (a special chosen fires at it)
            if (groupMode[g] == 1)
            {
                ForceFireGroup(int(g), int(target->x), int(target->y));
                groupMode[g] = 0;
            }
            else
            {
                GroupTarget& t = groupTargets[g];
                t = GroupTarget{};
                t.target = id;
                if (g < groupHomeX.size()) { groupHomeX[g] = int(target->x); groupHomeY[g] = int(target->y); }
                Engage(int(g));     // vt+0x38
            }
            if (!any && ft != setup.units->end() && ft->second.category == 1) Speak(0x10C, int(g));
            any = true;
            if (g < groupOrdered.size()) groupOrdered[g] = true;    // then bit 3 (0x4814D8)
        }
    }

    // vt+0x38 (0x483760), the group's engage: with a target (+0x2C), an object (+0x30) that's gone (0x440110) has the
    // members told 0x80B9 and 0x805A, the target cleared (+0x34..+0x3C -1) and bit 0 cleared (0x480EB0(0)); each
    // member then aims at the point (vt+0xC8 with +0x40..+0x48) or at the object (vt+0xC4). Then +0x1D8 bit 1 is set
    // when more than 10, or more than half the members (+0x24), say vt+0xA8.
    // *Provisional*: 0x80B9 isn't sent.
    void BattleMap::Engage(int g)
    {
        if (g < 0 || size_t(g) >= groupTargets.size()) return;
        GroupTarget& t = groupTargets[size_t(g)];
        if (t.target == 0) return;
        if (t.target != -1 && !UnitById(t.target))
        {
            StopAttacking(g);
            if (size_t(g) < groupBusy.size()) groupBusy[size_t(g)] = false;
        }
        const int id = t.target, x = t.x, y = t.y, z = t.z;
        for (Unit& u : units)
            if (u.group == g)
            {
                if (id == -1) Aim(u, x, y, z);
                else if (const Unit* o = UnitById(id)) Approach(u, *o);
            }
        // bit 1: more than 10, or more than half the members (+0x24, halved unsigned), halted (vt+0xA8, +0xE4 bit 0)
        int halted = 0, members = 0;
        for (const Unit& u : units)
            if (u.group == g) { members++; if (u.halted) halted++; }
        if (size_t(g) < groupHalted.size()) groupHalted[size_t(g)] = halted > 10 || halted > (members >> 1);
    }

    // vt+0xC4 (0x491100 -> 0x496300): unless it says not (vt+0xF8), at another object: d = ftol(the distance - -1.0),
    // r its radius (+0xAC) / 3 for a category 0, 2 or 3 object (+0xB8 bit 1), else 0; the point aimed at is the
    // object's less (it - this) x r / d across and down, its height plus its +0x41 byte x 1.414 (vt+0xC8)
    // The drones (0x5D-0x5F; class 0x48F9D0: 0x490E70 and +0x3C 1): each update (0x48FAC0), with its group's target a
    // point (+0x2C -1), within 64.0 of it (0x43FF00 from whole positions) it posts itself 0x80A0 an update on, then
    // the vehicle's update (0x4913B0). Its look ahead's object test (vt+0xBC, 0x48F9F0): the object found being its
    // target (+0x190, set by vt+0xC4) within 64.0, the same; then the vehicle's test (0x4929A0).
    // *Provisional*: the target's test is made each update from the distance alone, not only when the look ahead
    // (0x492350) finds it.
    void BattleMap::DroneTick(size_t index)
    {
        Unit& u = units[index];
        if (u.type < 0x5D || u.type > 0x5F || u.dieAt >= 0) return;
        auto octagonal = [](int x0, int y0, int x1, int y1) {
            const int a = std::abs(x1 - x0), b = std::abs(y1 - y0);
            return b - (std::min(a, b) >> 1) + a;
        };
        bool hit = false;
        if (u.group >= 0 && size_t(u.group) < groupTargets.size() && groupTargets[size_t(u.group)].target == -1)
        {
            const GroupTarget& g = groupTargets[size_t(u.group)];
            hit = float(octagonal(int(u.x), int(u.y), g.x, g.y)) < 64.0f;
        }
        if (!hit && u.droneTarget)
            if (const Unit* t = UnitById(u.droneTarget))
                hit = float(octagonal(int(u.x), int(u.y), int(t->x), int(t->y))) < 64.0f;
        if (hit) u.dieAt = ticks + 1;
    }

    void BattleMap::Approach(Unit& u, const Unit& target)
    {
        if (u.type >= 0x5D && u.type <= 0x5F) u.droneTarget = target.id;   // 0x48FA80: +0x190, then 0x491100
        if (u.Turned() || &u == &target) return;
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

    // flt_59E44C, the sight's scale for looking round (0x48F8A0, 0x496570): the value of the [Tweak] "Audible radius
    // multiplier" object at 0x59E438 (0x495620 makes it, 1.0; 0x416500 keeps the value at its +0x14)

    // The unit's update (0x4967E0): +0xDA counts up; past its type's +0x92 word it looks round (vt+0x104, 0x48F8A0:
    // with +0xE4 bit 1 and further than its sight x the scale from its post (+0xC8 / +0xCC), its group's guns are
    // stood down (0x805A); then 0x496890: on stance 2 or 3 (+0xD8) with its group not attacking (+0x1D8 bit 0
    // clear, or bit 2 set) it picks a target (0x496570), and the count goes back to 0).
    // 0x496890's first part: vt+0xAC(0) (not halted); aiming (+0xE4 bit 1) with a group, it posts itself 0x808A
    // when the group isn't mostly halted (+0x1D8 bit 1 clear; vt+0x128 = 0x4910B0: +0x188 bit 5, held: it steers on without arrival, separation,
    // braking or matching), else 0x8031 (0x4910C0: stopped, bits 5 and 13 cleared). An emplacement (0x48F8A0) first,
    // aiming and further from its aim point (+0xC8 / +0xCC) than its sight x the scale (1.0), stands its group's
    // guns down (0x805A to the group), which clears its aiming.
    // *Provisional*: the group's 0x805A is taken as each member's (no target cleared).
    bool BattleMap::LookRound(size_t index)
    {
        Unit& u = units[index];
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return false;
        const int count = u.scanCount++;
        if (count <= int16_t(t->second.field92)) return false;
        const bool hasGroup = u.group >= 0 && size_t(u.group) < groupTargets.size();
        if (IsEmplacement(u.type) && u.aiming && hasGroup &&
            Octagonal(int(u.x), int(u.y), u.aimX, u.aimY) > int(float(ObjOjd::Vision(t->second, setup.armour)) * setup.lookScale))
        {
            for (Unit& m : units)
                if (m.group == u.group)
                {
                    StandDown(m);
                    for (Turret& tr : m.turrets) tr.aiming = false;
                    for (Gun& gn : m.guns)
                    {
                        gn.armed = false;
                        if (gn.state != 0) gn.state = 4;
                        gn.flags |= 1;
                    }
                }
        }
        // 0x48F8A0: an emplacement that isn't aiming (+0xE4 bit 1 clear) frees its group (0x480EB0(0): bit 0)
        else if (IsEmplacement(u.type) && !u.aiming && hasGroup && size_t(u.group) < groupBusy.size())
            groupBusy[size_t(u.group)] = false;
        u.halted = false;   // vt+0xAC(0)
        if (u.aiming && hasGroup && u.vehicle)
        {
            if (!groupHalted[size_t(u.group)]) u.drive.flags |= 0x20;
            else Stop8031(u);
        }
        const int stance = u.stance;
        if (stance >= 2 && stance <= 3 && size_t(u.group) < groupTargets.size() && groupTargets[size_t(u.group)].target == 0 &&
            (!groupBusy[size_t(u.group)] || groupSelfMove[size_t(u.group)]))
            AutoTarget(u);
        u.scanCount = 0;
        return true;
    }

    // 0x496570: within its sight (+0x48) x the scale (octagonal, whole positions), with it not under the ground,
    // every other side's object that can be targeted (vt+0x6C) and is finished (+0xC0 bit 0): an armed one (+0x3C) is the target
    // at once; else the battle's key building (0x33E / 0x78, 0x116 / 0x3A by the kind) if seen, else the last seen.
    // The group gets 0x80B1 with it: for a player's group (+0x1D8 bit 4) and a vehicle target (+0xB8 bit 0), if
    // rand() is under 5000 speech 0x10F; then 0x482960: as 0x482880, the target set and bit 0 set.
    // *Provisional*: a player's unit seeing an unarmed one also tells the battle (0x80B1 to dword_4E9B78), not in.
    void BattleMap::AutoTarget(const Unit& u)
    {
        auto t = setup.units->find(u.type);
        if (t == setup.units->end() || u.drive.lift < 0.0f) return;
        const int sight = int(float(ObjOjd::Vision(t->second, setup.armour)) * setup.lookScale);
        const Unit* key = nullptr;
        const Unit* last = nullptr;
        const Unit* pick = nullptr;
        for (const Unit& o : units)
        {
            if (Octagonal(int(u.x), int(u.y), int(o.x), int(o.y)) >= sight) continue;
            // +0xC0 bit 0 (0x496616): set as an object is made (0x489E20), cleared by a building's start while its
            // construction isn't done (0x48E9D0), so a building site isn't picked; its health isn't asked
            if (IsBuilding(o) && double(Progress(o)) <= 0.9999) continue;
            if (o.players == u.players || !Targetable(o)) continue;
            if (!o.guns.empty()) { pick = &o; break; }
            last = &o;
            if (o.type == (setup.sector == 3 ? 0x33E : 0x78) || o.type == (setup.sector == 30 ? 0x116 : 0x3A)) key = &o;
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

    // vt+0x6C, whether an object can be targeted (0x496570's look round, 0x483880's retaliation, the cursor's
    // 0x44B800-0x44E5C0): the unit base's is yes (0x496C30: mov al, 1); the mines' (0x273-0x276, 0x48CCB0) only with
    // the status 0x80F9 (vt+0x10C); 0x4993F0's class (0x498E90's) not while +0x190 bit 1 is set unless with it.
    bool BattleMap::Targetable(const Unit& u) const
    {
        if (u.type >= 0x273 && u.type <= 0x276) return u.Detected();
        if (Sinker(u.type)) return !u.hidden || u.Detected();
        return true;
    }

    // The mines' update (0x48CC00, in place of the unit's timer): the status time (+0xDC) down, at 0 the status
    // cleared (vt+0x90 with 0); then with the status 0x80F9 (vt+0x10C) on one that isn't the player's (+0x24 not 0):
    // at its last update (vt+0x8C = 1) the second renderer (+0xE8) goes; else, with none, 0x48CCD0 makes one,
    // GType 0x359 (0x40EC50, its +0x26 set to 0x20).
    // The sinkers (0x498F20) keep theirs at +0x19C the same way, and the player's has one while it's under
    // (vt+0x188, Sinking.cpp).
    // Its renderer's +0x26 is 0x20 and its +0x1E never set (0x4650), so it draws over everything (RekeyViews).
    void BattleMap::MarkerTick(size_t index)
    {
        Unit& u = units[index];
        const bool mine = u.type >= 0x273 && u.type <= 0x276;
        if (!mine && !Sinker(u.type)) return;
        const bool detected = u.Detected() && !u.players && u.statusTime != 1;
        const bool want = detected || (!mine && u.players && u.under);
        if (want == u.marked) return;
        u.marked = want;
        if (!want)
        {
            objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const ViewObject& o) {
                return o.unit == int(index) && (o.sprite == 0x359 || o.sprite == 0x35A); }), objects.end());
            return;
        }
        // the Tunnelling Track Layers' is their mound (vt+0x174 = 0x493D70: GType 0x35A)
        const int marker = u.type >= 7 && u.type <= 9 ? 0x35A : 0x359;
        const GType* g = res.FindGType(marker);
        if (!g || !res.GetFrame(marker, 0)) return;
        int depth = 1;
        for (const ViewObject& o : objects)
            if (o.unit == int(index) && o.light < 0) { depth = o.depth + 1; break; }
        const int ix = int(u.x), iy = int(u.y);
        ViewObject v{ iy + depth, ix, land.ProjectY(iy, land.HeightAt(ix, iy) + std::max(0, int(u.drive.lift))), nullptr, nullptr, g->paletteSlot, marker };
        v.unit = int(index);
        v.depth = depth;
        objects.push_back(v);
        std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
    }

    // vt+0xC0 on a vehicle (0x491020; the damage itself 0x4958B0 -> 0x48A5A0): the player's (+0x24 0), when rand()
    // is under 2000 and the hit has a firer (+0xCC, the object that fired it), says 0x110 with its type's voice
    // (byte_51A190); then its group is told 0x8085 with it and the hit (0x483880). Other objects just take the damage.
    // The sinkers' vt+0xC0 (0x498EE0) first: under (+0x190 bit 0), only a hit from 0x2CE-0x2D0 (718-720) reaches it.
    void BattleMap::Hit(size_t index, double raw, int attacker, int by, int weapon)
    {
        Unit& u = units[index];
        if (Sinker(u.type) && u.under && (by < 718 || by > 720)) return;
        if (u.vehicle)
        {
            if (u.players && random::Rand() < 2000 && attacker != 0) Speak(0x110, u.group);
            Retaliate(u.group, index, attacker);
        }
        else if (IsBuilding(u) && u.players)
        {
            // a building's vt+0xC0 (0x48F030): the player's (+0x24 0), when rand() is under 500 or it's still at full
            // health (0x47DA20), hit by an object of another side (0x440110, 0x4406E0): its last 0x111 cut off if it's
            // still playing, then 0x111 at the listener (+0xEC)
            if (random::Rand() < 500 || u.health == MaxHealth(u.type))
                if (const Unit* a = attacker ? UnitById(attacker) : nullptr; a && a->players != u.players && setup.sound)
                {
                    if (u.alertSound && setup.sound->Active(u.alertSound)) setup.sound->Stop(u.alertSound);
                    u.alertSound = setup.sound->Play(0x111, 1.0f, 0);
                }
        }
        // 0x4958B0, the damage by research: by the target's side (0x4406E0) and the type that made the hit (the hit's
        // +0xC4, the gun's or shell's), each multiplier 1.0 until its enhancement is done (0x45A1C0 / 0x45B320)
        // the multipliers scale the hit's scale (arg_4, 1.0), and 0x48A5A0 truncates (1 - d / reach) x power x scale
        // once (ftol, x87); gas never hurts the Martians (0x495A97), whatever the research
        auto ut = setup.units->find(u.type);
        const int side = ut == setup.units->end() ? 2 : ut->second.faction;
        if (side == 1 && weapon >= 0x393 && weapon <= 0x395) return;
        int amount = int(raw);
        if (setup.researchMultiplier)
        {
            double f = raw;
            auto mulf = [&](bool human, int type) { f *= setup.researchMultiplier(human, type); };
            const bool a = weapon >= 0x1AB && weapon <= 0x1B0, b = weapon >= 0x1B5 && weapon <= 0x1B7;
            if (side == 0 && u.vehicle)
            {
                if (a) mulf(true, 0x256);                       // HeatResistantArmour
                if (a || b) mulf(true, 0x257);                  // InsulatedHighEnergyResistantArmour
                if (weapon >= 915 && weapon <= 917) { mulf(true, 0x25C); mulf(true, 0x25D); }   // FineAirFilters, ChemicalFilters
                if (weapon >= 712 && weapon <= 714) mulf(true, 0x266);                          // TunnelReinforcementApparatus
                if (a) mulf(false, 0x27F);                      // the Martians' SecondaryBeamAmplifiers
            }
            else if (side == 1)
            {
                if (u.vehicle) { mulf(false, 0x27B); mulf(false, 0x27C); }   // ShockResistant, ImpactReflecting Armour
                mulf(true, 0x259);                              // the Humans' HighExplosiveRounds
            }
            amount = int(f);
        }
        Damage(index, amount);
    }

    // 0x8085 to a group (0x4807F0 -> 0x483880) with the member hit and the hit (its firer +0xCC):
    //  - with no object targeted (+0x2C 0): nothing if +0x1D8 bit 0 is set and bit 2 clear; on Avoid (stance 0) the
    //    group runs: 0x43FE90 with rand() & 0xFFF and 400.0 from the member's point, bit 0 set (bit 2 cleared), 0x804F
    //    there; on stances 1-3, with a firer (0x440110) of another side (0x4406E0) that can be targeted (vt+0x6C), each
    //    member 0x80B9, the firer the target (0x482880), bit 0 set (0x480EB0) and vt+0x38;
    //  - with one targeted: a computer's group (bit 7) whose target is unarmed (+0x3C 0), hit by an armed firer, stands
    //    its guns down (0x805A to each member) and clears bit 0 (and bit 2), so it's free to pick again.
    // *Provisional*: a point target (force fire) is taken as bit 0 set; the second case drops the target outright
    // (StopAttacking) where the game leaves +0x2C; 0x80B9 isn't sent.
    void BattleMap::Retaliate(int g, size_t member, int attacker)
    {
        if (g < 0 || size_t(g) >= groupTargets.size()) return;
        GroupTarget& gt = groupTargets[size_t(g)];
        const Unit& m = units[member];
        if (gt.target <= 0)
        {
            // a group moving under an order (bit 0 set, bit 2 clear: 0x48389E-0x4838AD) takes no notice
            if (gt.target < 0 || (groupBusy[size_t(g)] && !groupSelfMove[size_t(g)])) return;
            const int stance = groupStance[size_t(g)];
            if (stance == 0)
            {
                float dx = 0, dy = 0;
                Polar(random::Rand() & 0xFFF, 400.0f, dx, dy);
                const int y = int(m.y + dy), x = int(m.x + dx);
                groupBusy[size_t(g)] = true;
                std::vector<size_t> members;
                for (size_t i = 0; i < units.size(); i++)
                    if (units[i].group == g && units[i].vehicle) members.push_back(i);
                OrderGroupMove(g, members, int(int16_t(x)), int(int16_t(y)));
                return;
            }
            if (stance < 1 || stance > 3 || attacker == 0) return;
            const Unit* a = UnitById(attacker);
            if (!a || a->players == m.players || !Targetable(*a)) return;
            gt = GroupTarget{};
            gt.target = attacker;
            Engage(g);              // vt+0x38
            return;
        }
        if (g >= int(setup.groups.size()) || setup.groups[size_t(g)].Players() || attacker == 0) return;
        const Unit* current = UnitById(gt.target);
        const Unit* a = UnitById(attacker);
        if (!current || !current->guns.empty() || !a || a->guns.empty()) return;
        StopAttacking(g);
        groupBusy[size_t(g)] = false;
    }

    // 0x482880(0) / the guns' 0x805A (0x494D80): not armed; a cycle under way goes to its reload; aimed set
    void BattleMap::StopAttacking(int group)
    {
        if (group < 0 || size_t(group) >= groupTargets.size()) return;
        groupTargets[size_t(group)] = GroupTarget{};
        for (Unit& u : units)
            if (u.group == group)
            {
                // each member's 0x805A (0x495CB0's case): vt+0xCC, for a vehicle 0x491180: steering bit 5 cleared
                // and bit 0 (stopped) set, then 0x496440: aiming (+0xE4 bit 1) cleared; then vt+0xAC(0): not halted
                StandDown(u);
                // 0x80B9 (0x490CD5) with 0 goes with the stand-down in each sender (0x4821A0, 0x482880, 0x481CA0,
                // the retreat's first): +0x188 bit 9 cleared
                u.drive.flags &= ~0x200u;
                for (Turret& t : u.turrets) t.aiming = false;
            }
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
    // *Provisional*: a target that's gone (0x8057's tidy-up) clears the group's target.
    void BattleMap::ChaseTarget(size_t g)
    {
        if (g >= groupTargets.size()) return;
        {
            GroupTarget& t = groupTargets[g];
            if (t.target == 0) return;
            // a computer's group (+0x1D8 bit 7) every 8th of its counts tries a special (0x482570)
            if (g < groupPlayers.size() && !groupPlayers[g] && g < groupTicks.size() && groupTicks[g] % 8 == 0)
                ComputerSpecials(int(g));
            const Unit* object = nullptr;
            if (t.target > 0)
            {
                // an object: gone, the target's cleared; under the ground (+0x84 under 0), the guns are told 0x805A
                // and +0x1D8 bit 0 cleared; else the point is where it is (+0x78..+0x80)
                object = UnitById(t.target);
                if (!object) { StopAttacking(int(g)); return; }
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
                    // and +0x1D8 bit 0 cleared and +0x56 0 (0x4826D0); the target and its point are kept
                    if (g < groupBusy.size()) groupBusy[g] = false;
                    if (g < groupArrived.size()) groupArrived[g] = 0;
                    return;
                }
                t.x = int(object->x);
                t.y = int(object->y);
                t.z = int(float(land.HeightAt(t.x, t.y)) + object->drive.lift);
            }
            if (Octagonal(t.x, t.y, t.lastX, t.lastY) <= 80) return;
            t.lastX = t.x;
            t.lastY = t.y;
            t.lastZ = t.z;
            std::vector<size_t> members;
            for (size_t i = 0; i < units.size(); i++)
                if (units[i].group == int(g) && units[i].vehicle) members.push_back(i);
            OrderGroupMove(int(g), members, t.x, t.y);  // the range as the route's radius (0x4830FD)
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
    void BattleMap::Aim(Unit& u, int x, int y, int z)
    {
        if (u.drive.lift < 0.0f || u.drive.link) return;
        x = std::clamp(x, 0, land.WorldWidth() - 1);
        y = std::clamp(y, 0, int((int64_t(land.Cell()) * land.Height()) >> 16) - 1);
        // each 0x496200 measures from its own object's point (the unit's, then the turret's for its guns)
        auto jitter = [&](float fromX, float fromY, int gx, int gy, int gz, int& ox, int& oy, int& oz) {
            const double dx = double(gx) - fromX, dy = double(gy) - fromY;
            const int d = int(std::sqrt(dy * dy + dx * dx) - -1.0);
            const int s = (d + ((d >> 31) & 0xF)) / 16 + 1;
            ox = gx + random::Rand() % (2 * s) - s;
            oy = gy + random::Rand() % (2 * s) - s;
            oz = gz + random::Rand() % (2 * s) - s;
        };
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        // the unit's 0x496200: the goal kept (+0xC8..+0xD0), aiming (+0xE4 bit 1), one jittered point for all its
        // children
        u.aimX = x;
        u.aimY = y;
        u.aiming = true;
        int cx = 0, cy = 0, cz = 0;
        jitter(u.x, u.y, x, y, z, cx, cy, cz);
        size_t gun = 0, turret = 0;
        auto arm = [&](int ax, int ay, int az) {
            if (gun >= u.guns.size()) return;
            Gun& g = u.guns[gun++];
            g.tx = ax;
            g.ty = ay;
            g.tz = az;
            g.armed = true;
            if (g.state == 2) g.state = 3;
            AimFlag(u, g);  // 0x494D20 (vt+0xC8) runs 0x494FD0 at once, so state 0 can go to 1 on the next cycle
        };
        for (const auto& part : t->second.parts)
        {
            if (IsTurret(part.a))
            {
                // the turret's 0x491110 -> 0x496200: one more jittered point for its guns
                auto tt = setup.units->find(part.a);
                if (tt == setup.units->end()) continue;
                int gx = 0, gy = 0, gz = 0;
                const float fx = turret < u.turrets.size() ? float(u.turrets[turret].x) : u.x;
                const float fy = turret < u.turrets.size() ? float(u.turrets[turret].y) : u.y;
                jitter(fx, fy, cx, cy, cz, gx, gy, gz);
                if (turret < u.turrets.size())
                {
                    Turret& tr = u.turrets[turret++];
                    tr.aiming = true;
                    tr.aimX = gx;
                    tr.aimY = gy;
                }
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
    // The arc (0x494FD0, with a hardpoint): |0x43FF30(ftol(vt+0x2C (its own +0x40) + the middle), the heading to the
    // point)| under the half width (vt+0x2C leaves its argument on the stack: that's 0x43FF30's second).
    // 0x494FD0: +0xF0 bit 0, the aim point in range (record 12's +0 x the range scale over the length from the gun's
    // point) and, on a hardpoint, within its arc
    void BattleMap::AimFlag(const Unit& u, Gun& g) const
    {
        const auto w = WeaponData(g.type);
        int heading = 0;
        float length = 0;
        HeadingAndLength(float(g.x), float(g.y), float(g.tx), float(g.ty), heading, length);    // from the gun's point
        bool inArc = true;
        if (g.hardpoint)
        {
            int d = (heading & 0xFFF) - ((GunHeading(u, g) + g.middle) & 0xFFF);
            while (d < -2048) d += 4096;
            while (d > 2047) d -= 4096;
            inArc = std::abs(d) < g.half;
        }
        const bool inRange = float(w[0]) * g.range > length;
        g.flags = (g.flags & ~1u) | (inRange && inArc ? 1u : 0u);
    }

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
                if (mode == FireMode(g.type) && !u.Frozen()) g.state = 1;
                else g.flags &= ~1u;
            }
            break;
        case 1:
        {
            g.flags &= ~2u;
            g.state = 2;
            const int r = random::Rand() % 15;
            g.counter = Ticks(w[1], setup.gameFrequency) + r;
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
            g.counter = Ticks(w[2], setup.gameFrequency) + r;
            g.state = 0;
            break;
        }
        default: break;
        }
        AimFlag(u, g);
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
    void BattleMap::Launch(size_t index, Gun& g)
    {
        if (g.spent) return;
        const Unit& u = units[index];
        auto t = setup.units->find(g.type);
        if (t == setup.units->end() || t->second.emitters.empty()) return;
        const auto& list = t->second.emitters;
        if (g.fired.size() != list.size()) g.fired.assign(list.size(), 0);
        // from the mortar's own point (+0x78..+0x80, PoseChildren), its height over the ground there
        const float z = float(g.z);
        const int h = int(z - float(land.HeightAt(g.x, g.y)));
        int heading = 0;
        float length = 0;
        HeadingAndLength(float(g.x), float(g.y), float(g.tx), float(g.ty), heading, length);
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
            float vx = 0, vy = 0;
            Polar(dir, across, vx, vy);
            const int k = SpawnEffect(e.a, float(g.x), float(g.y), float(h), vx, vy, upward, e.f[2]);
            if (k >= 0)
            {
                Effect& sh = effects[size_t(k)];
                sh.size = size;
                sh.count = int16_t(count);
                sh.group = u.group;
                sh.attacker = u.id;
                if (IsGas(e.a)) sh.count = int16_t(e.a == 0x393 ? 240 : e.a == 0x394 ? 480 : 720);   // 0x488D20
            }
            encountered.insert(g.type);     // 0x488C00: the gun's type met, for each shell
            g.fired[i]++;
            if (e.c != 0 && g.fired[i] > int16_t(e.c)) { g.spent = true; break; }
        }
    }

    // The rest of a shell's update (0x488F00), after the flying object's (0x487890, UpdateEffects): at or under 0.0
    // high, or with its updates (+0xC2) counted out (taken down only while above the ground), it lands: 0x487F50
    // with its size (+0xC0), its point, ftol(the height - -8.0) up, the firer (+0xC4) and its type; with the updates
    // below 0 it's gone.
    bool BattleMap::ShellLands(size_t index)
    {
        Effect& s = effects[index];
        bool lands = false;
        if (s.lift <= 0.0f) lands = true;
        else lands = s.count-- <= 0;
        if (!lands) return false;
        landings.push_back({ s.size, int(s.x), int(s.y), int(s.lift - -8.0f), s.group, s.attacker, s.type });
        return s.count < 0;
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
        const int d = Octagonal(g.x, g.y, g.tx, g.ty) >> 5;     // from the gun's own point
        const int s = int(float(d) / g.accuracy - -1.0f);
        const int span = 2 * s;
        const int x = random::Rand() % span - s + g.tx;
        const int y = random::Rand() % span - s + g.ty;
        // the shell (0x4886A0) jitters its height with a third rand(); the rocket (0x488830) takes two and lands at
        // the target's height (+0xC4)
        const int z = g.cls == 1 ? g.tz : random::Rand() % span + g.tz - s;
        const int h = z - land.HeightAtNearest(x, y);
        ImpactAt(w[3], x, y, h, u.group, u.id, g.type);
    }

    void BattleMap::ImpactAt(int size, int x, int y, int h, int group, int attacker, int weapon)
    {
        // 0x487F50: the height decides the table (over 64: the air bursts, cmp arg_C, 40h), the terrain under the
        // point land or water; the size only indexes it
        if (size < 1 || size > 67) return;
        int effect;
        if (h > 64) effect = kImpactBig[size - 1];
        else
        {
            const int vx = std::clamp(int((int64_t(int32_t(uint32_t(x) << 16)) + land.HalfCell()) / land.Cell()), 0, land.Width() - 1);
            const int vy = std::clamp(int((int64_t(int32_t(uint32_t(y) << 16)) + land.HalfCell()) / land.Cell()), 0, land.Height() - 1);
            const int terrain = land.At(vx, vy).atm & 15;
            effect = terrain >= 1 && terrain < 4 ? kImpactWater[size - 1] : kImpactLand[size - 1];
        }
        if (effect) Explode(effect, x, y, std::max(h, 0), group, attacker, weapon);   // (+0xC4 the weapon's type)
    }
}
