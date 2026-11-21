// The battle map's damage, deaths and explosions: TakeDamage (vt+0xE0, 0x48AB20), HitByExplosion (vt+0xC0,
// 0x48A5A0), the death message 0x80A0 (0x489FA0) and WExplosion (0x488180, Init 0x488350, Detonate 0x488200).
// See docs/BATTLEMAP.md "Damage and death".
#include "battlemap/BattleMap.h"
#include "core/Random.h"
#include "platform/Sound.h"
#include <SDL3/SDL.h>
#include <cmath>

namespace wow
{
    namespace
    {
        // 0x43FF00
        int Octagonal(int x0, int y0, int x1, int y1)
        {
            const int a = std::abs(x1 - x0), b = std::abs(y1 - y0);
            return b - (std::min(a, b) >> 1) + a;
        }
    }

    // The types the factory (0x446B30) makes as WExplosion (0x488180, and 0x48CD80 on it): 0x17D-0x18A, 0x1E4-0x1F7,
    // 0x1FB-0x21D, 0x221-0x243, 0x247-0x255,
    // and its switch's cases 715-717, 946-948, 986, 997-999 and 1006-1008
    bool BattleMap::IsExplosion(int t)
    {
        return (t >= 0x17D && t <= 0x18A) || (t >= 0x1E4 && t <= 0x1F7) || (t >= 0x1FB && t <= 0x21D) || (t >= 0x221 && t <= 0x243) || (t >= 0x247 && t <= 0x255) || (t >= 715 && t <= 717) ||
               (t >= 946 && t <= 948) || t == 986 || (t >= 997 && t <= 999) || (t >= 1006 && t <= 1008);
    }

    // 0x47DA20: the type table's +0x4A, the type's record 34 times the armour modifier for its faction and the side
    // played, then record 35's (OBJ.ojd loader 0x43823F / 0x4382BA)
    int BattleMap::MaxHealth(int type) const
    {
        auto t = setup.units->find(type);
        if (t == setup.units->end()) return 0;
        return ObjOjd::Armour(t->second, setup.human, setup.armour);
    }

    // 0x48AB20: hit this update (+0xC0 bit 3) and the health less the amount; at 0 or under, 0x80A0 to itself
    // 10 + rand() % 20 updates on (0x4355A0). (It also refreshes the status bar and, the first time it's under 40%,
    // puts the object on the world's damaged list.)
    void BattleMap::Damage(size_t index, int amount)
    {
        Unit& u = units[index];
        if (u.health <= 0) return;      // vt+0xB4, the health, must be over 0
        u.health -= amount;
        if (u.health <= 0)
        {
            const int at = ticks + random::Rand() % 20 + 10;
            if (u.dieAt < 0 || at < u.dieAt) u.dieAt = at;
        }
        if (IsBuilding(u)) LoseParts(index);
    }

    // A building's vt+0xE0 (0x48EF80), after 0x48AB20: with +0xB8 bit 8 (set on every object, 0x47D7D4) and more
    // than one child, 0x48A7D0 with its part count less 1 gives how many to lose: the children less 1, less
    // ftol(the part count x the health / its most, 0x47DA20), at most the part count less 1. For each, while it
    // has more than one child, the last child (+0x70's tail), if its health is over 0, is sent 0x80A0, which takes
    // it off the list at once: a big hit takes several, the last made first (the Factory's chimneys).
    // A part's 0x80A0 (0x489FA0) makes its type's destruction effect (+0x98) at its point, its parent the firer.
    void BattleMap::LoseParts(size_t index)
    {
        Unit& u = units[index];
        auto t = setup.units->find(u.type);
        if (t == setup.units->end() || u.partsLeft <= 1) return;
        const int parts = int(t->second.parts.size());
        const int most = MaxHealth(u.type);
        if (parts <= 0 || most <= 0) return;
        const float ratio = float(std::max(0, u.health)) / float(most);
        int lose = (u.partsLeft - 1) - int(float(parts) * ratio);
        if (lose >= parts) lose = parts - 1;
        if (lose <= 0) return;
        // each pass takes the last child: its 0x80A0 runs at once (vt+0x1C = 0x47DF20) and its 0x8057 takes it off the
        // parent's list there and then (vt+0x78 -> the parent's vt+0x50, 0x47F220), so one big hit can take several
        for (int pass = 0; pass < lose && u.partsLeft > 1; pass++)
        {
            const int last = u.partsLeft - 1;
            u.partsLeft--;
            int type = -1, ox = 0, oy = 0, oz = 0;
            if (size_t(last) < t->second.parts.size())
            {
                const auto& part = t->second.parts[size_t(last)];
                type = part.a; ox = int16_t(part.b); oy = int16_t(part.c); oz = int16_t(part.d);
            }
            else
            {
                // one of the corner objects (AddCorners)
                type = 0x17B;
                for (const Attached& a : u.attached)
                    if (a.part == last) { ox = a.ox; oy = a.oy; }
            }
            // its children go with it (0x47EAA0): a light part's light out
            for (const Attached& a : u.attached)
                if (a.part == last && a.light >= 0 && size_t(a.light) < lights.size())
                {
                    lights[size_t(a.light)].alive = false;
                    const int li = a.light;
                    objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const ViewObject& o) { return o.light == li; }), objects.end());
                }
            objects.erase(std::remove_if(objects.begin(), objects.end(),
                                         [&](const ViewObject& o) { return o.unit == int(index) && o.part == last; }), objects.end());
            u.attached.erase(std::remove_if(u.attached.begin(), u.attached.end(), [&](const Attached& a) { return a.part == last; }),
                             u.attached.end());
            auto pt = setup.units->find(type);
            if (pt == setup.units->end()) continue;
            const int z = int(double(oz) - double(pt->second.size41) * -1.414);
            // 0x489FA0: the part's parent is its explosion's firer (+0xCC its id, the group spared by 0x488200)
            Explode(pt->second.field98, int(u.x) + ox, int(u.y) + oy, z, u.group, u.id);
        }
    }

    // 0x80A0 (0x489FA0): the object posts itself 0x8057 (gone) and makes its type's destruction effect (+0x98,
    // 0x488070) at its point, ftol(+0x84 + its +0x41 byte x 1.414) up, with its parent as the firer; the battle's
    // key building (the Martian Base 0x33E where the kind is 3, else the Communications Centre 0x78; Parliament
    // 0x116 where it's 0x1E, else the Command Post 0x3A) going, with a battle on, speaks 0x112 if it was the player's, else 0x13B.
    // *Provisional*: gone at once rather than on the next update; the kind is taken as the sector.
    void BattleMap::Die(size_t index)
    {
        ClearFootprint(units[index]);   // 0x490F10's vt+0x114
        StopUnitSounds(units[index]);
        const Unit u = units[index];
        auto t = setup.units->find(u.type);
        if (u.engineSound && setup.sound) setup.sound->Stop(u.engineSound);
        for (const Turret& tr : u.turrets)
            if (tr.sound && setup.sound) setup.sound->Stop(tr.sound);
        // the object gone: its view object and its place in the list
        objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const ViewObject& o) { return o.unit == int(index); }), objects.end());
        for (ViewObject& o : objects)
            if (o.unit > int(index)) o.unit--;
        units.erase(units.begin() + std::ptrdiff_t(index));
        // its war map object's last (0x47EB00 -> 0x4216D0 = 1), and the battle group's
        const bool last = std::none_of(units.begin(), units.end(), [&](const Unit& o) { return o.ref == u.ref; });
        const bool groupEmpty = std::none_of(units.begin(), units.end(), [&](const Unit& o) { return o.group == u.group; });
        // 0x8058 to the groups whose target it was (0x4807F0 -> 0x4821F0)
        for (size_t g = 0; g < groupTargets.size(); g++)
            if (u.id != 0 && groupTargets[g].target == u.id) TargetGone(int(g), u, last);
        if (groupEmpty) GroupGone(u.group);
        if (t == setup.units->end()) return;
        // vt+0xE8 (0x48A9A0): a category 0, 2 or 3 object, not a tree, frees its footprint (0x47C3A0 with 2)
        if (const int c = t->second.category; (c == 0 || c == 2 || c == 3) && !(u.type >= 0xCF && u.type <= 0xE5))
            land.ClearOccupied(int(u.x), int(u.y), int16_t(t->second.currentHitPoints), 2);
        const int z = int(double(u.drive.lift) - double(t->second.size41) * -1.414);
        Explode(t->second.field98, int(u.x), int(u.y), z);
        const bool key = u.type == (setup.sector == 3 ? 0x33E : 0x78) || u.type == (setup.sector == 30 ? 0x116 : 0x3A);
        if (key && setup.sound) setup.sound->Play(u.players ? 0x112 : 0x13B, 1.0f);
        if (key && t->second.category != 1 && u.group >= 0) KeyBuildingLost(u.players);
    }

    // 0x4821F0 (0x8058 to a group, its target, +0x30, gone or changed class): a player's group (+0x1D8 bit 4) says
    // 0x10E (0x464EE0 at the listener, in its voice 0x449120) when it was a building (0x440700: 2 or 3), and 0x10D
    // when it was a vehicle (+0xB8 bit 0) that was its war map object's last (0x47EB00 -> 0x4216D0 = 1), its leader's
    // portrait told each time (0x4842D0, *not in*). Then the members 0x80B9 and 0x805A (0x4821A0), the target cleared
    // (+0x2C / +0x30 0, +0x34..+0x3C -1), each member 0x805A again, 0x8086 to both players with its id, +0x1D8 bit 0
    // cleared, +0x56 and the mode (+0x1DC) 0.
    // *Provisional*: 0x80B9 and 0x8086 aren't sent; +0x56 isn't kept.
    void BattleMap::TargetGone(int g, const Unit& target, bool last)
    {
        if (g < 0 || size_t(g) >= groupTargets.size()) return;
        if (size_t(g) < groupPlayers.size() && groupPlayers[size_t(g)])
        {
            auto t = setup.units->find(target.type);
            const int category = t == setup.units->end() ? 0 : t->second.category;
            if (category == 2 || category == 3) Speak(0x10E, g);
            if (target.vehicle && last) Speak(0x10D, g);
        }
        StopAttacking(g);
        if (size_t(g) < groupBusy.size()) groupBusy[size_t(g)] = false;
        if (size_t(g) < groupMode.size()) groupMode[size_t(g)] = 0;
    }

    // vt+0x78 (0x48AC50) on the battle's key building (not a vehicle, with a war map object): its sector is told 0x80AA
    // with whether it was the player's (0x43B99C: to that side's construction and resource lists, +0x54 / +0x5C or
    // +0x58 / +0x60); and where the kind is 3 or 0x1E (the home sectors), with no battle on (the report's +0x54)
    // byte_4E9B94 is set (the game forfeited), and the sector is told 0x8136 "DESTROYING VEHICLES" (0x43B95F: that
    // side's unit list, +0x4C / +0x50). Each war map object in those lists (0x428D70, cases 0x80AA and 0x8136) posts
    // 0x80A0 to its battle units one update on (0x429410), and the list staggers them (StaggerDeaths).
    // *Provisional*: the kind is taken as the sector.
    void BattleMap::KeyBuildingLost(bool players)
    {
        auto wipe = [&](int kind) {
            for (size_t g = 0; g < setup.groups.size(); g++)
                if (setup.groups[g].Players() == players && setup.groups[g].list / 2 == kind) StaggerDeaths(int(g), ticks + 1);
        };
        wipe(1);    // 0x80AA: constructions,
        wipe(2);    // then resources
        if (setup.sector == 3 || setup.sector == 30)
        {
            if (!battleOn) forfeit = true;
            wipe(0);    // 0x8136: units
        }
    }

    // 0x47CEE0: a list told 0x80A0 posts it to each member (0x4355A0), the first 30 updates on and each next one
    // 30 + rand() % 30 after the one before
    void BattleMap::StaggerDeaths(int group, int at)
    {
        int delay = 30;
        for (Unit& m : units)
        {
            if (m.ref != group || m.health <= 0) continue;     // a war map object's units (its list's, 0x47CEE0)
            const int when = at + delay;
            if (m.dieAt < 0 || when < m.dieAt) m.dieAt = when;
            delay += random::Rand() % 30 + 30;
        }
    }

    // 0x488070 with a WExplosion (0x488180): +0xC8 the type's +0x44 (its radius), +0xCA its +0x4A (its power).
    // As it's added (0x488350): its sound (+0xA2) at its point; its parts started (0x489000 -> 0x47EF30: each a
    // child at the part's offset, explosions among them going off first); its radius set; then Detonate (0x488200):
    // everything within reach (0x47FC50: the octagonal distance under its radius + the other's, kind 4 left out),
    // but the firer and, unless they say otherwise (vt+0xF8), the firer's side, is hit (vt+0xC0 with 1.0: 0x48A5A0,
    // when its health is over 0 and +0xB8 bit 7 is set: ftol((1 - the distance / both radii) x the power x 1.0) to
    // vt+0xE0); and under 64 up, not on water (0x47FFE0: terrain 1-3), the ground is scorched (0x804B -> 0x47C4C0
    // with the radius and the power). With its sprite it then plays as a WAnimEffect.
    // A death explosion's firer is the dying object's parent: none for a unit, so both sides are hit; a building's
    // part's is its building (LoseParts); a gun's spares its unit's group (0x47EB00).
    // The area blasts (0x487B00 on WExplosion: the factory's 718-720, 788-790, 812-823)
    bool BattleMap::IsAreaBlast(int t) { return (t >= 718 && t <= 720) || (t >= 788 && t <= 790) || (t >= 812 && t <= 823); }

    void BattleMap::Explode(int type, int x, int y, int z, int firer, int attacker, int weapon, int radiusAs, int powerAs)
    {
        auto t = setup.units->find(type);
        const bool area = IsAreaBlast(type);
        if (t == setup.units->end() || (!IsExplosion(type) && !area)) return;
        const int radius = radiusAs ? radiusAs : int16_t(t->second.hitPoints);
        const int power = powerAs ? powerAs : MaxHealth(type);
        const int ground = land.HeightAt(x, y);
        if (t->second.fieldA2 && setup.sound)
        {
            const int sy = land.ProjectY(y, ground + std::max(z, 0));
            if (begun) setup.sound->Play(t->second.fieldA2, 1.0f, 0, x, sy);
            else heldSounds.push_back({ t->second.fieldA2, x, sy });   // built behind the Briefing: heard at Begin
        }
        // the WExplosion itself is made first (0x488070); its start (0x488350) then makes its parts and detonates
        const int self = SpawnEffect(type, float(x), float(y), float(std::max(z, 0)));
        if (self >= 0 && !area)
        {
            effects[size_t(self)].group = firer;        // +0xCC / +0xC4, which the cascade passes on (0x48CDE0)
            effects[size_t(self)].attacker = attacker;
        }
        for (const auto& part : t->second.parts)
        {
            const int px = x + int16_t(part.b), py = y + int16_t(part.c), pz = z + int16_t(part.d);
            if (IsExplosion(part.a) || IsAreaBlast(part.a)) Explode(part.a, px, py, pz, firer, attacker, weapon);
            else if (part.a >= 334 && part.a <= 346) AddLight(part.a, px, py, land.HeightAt(px, py) + pz);
            else SpawnEffect(part.a, float(px), float(py), float(pz));
        }
        if (area)
        {
            AreaDetonate(type, x, y, radius, power, firer, attacker, weapon);
            return;
        }
        // Detonate
        for (size_t i = 0; i < units.size(); i++)
        {
            Unit& u = units[i];
            auto ut = setup.units->find(u.type);
            if (ut == setup.units->end() || ut->second.category == 4) continue;
            if (firer >= 0 && u.group == firer && !u.Turned()) continue;   // the firer's own (0x47EB00), unless turned (vt+0xF8)
            const int reach = radius + u.drive.radius;
            const int d = Octagonal(int(u.x), int(u.y), x, y);
            if (d > reach || reach == 0) continue;
            if (u.health <= 0) continue;
            const float hit = float((1.0 - double(float(d)) / double(float(reach))) * power);
            Hit(i, int(double(hit) * 1.0), attacker, type, weapon);
        }
        // the scenery within reach too (it has no side to spare)
        for (size_t i = 0; i < props.size(); i++)
        {
            Prop& p = props[i];
            if (!p.alive || p.health <= 0) continue;
            auto pt = setup.units->find(p.type);
            if (pt == setup.units->end() || pt->second.category == 4) continue;
            const int reach = radius + int16_t(pt->second.hitPoints);
            const int d = Octagonal(int(p.x), int(p.y), x, y);
            if (d > reach || reach == 0) continue;
            const float hit = float((1.0 - double(float(d)) / double(float(reach))) * power);
            DamageProp(i, int(double(hit) * 1.0));
        }
        if (z < 64)
        {
            const int vx = std::clamp((int(uint32_t(x) << 16) + land.HalfCell()) / land.Cell(), 0, land.Width() - 1);
            const int vy = std::clamp((int(uint32_t(y) << 16) + land.HalfCell()) / land.Cell(), 0, land.Height() - 1);
            const int terrain = land.At(vx, vy).atm & 15;
            if (terrain < 1 || terrain > 3) land.Scorch(x, y, radius, power);
        }
    }

    // An area blast's detonation (vt+0xE0 = 0x487B50, in place of WExplosion's 0x488200: no splash for all, no
    // scorch): every object within reach (0x47FC50, its radius +0xAC) but the firer's side (0x47EB00) gets, by the
    // blast's type: 718-720 a hit (vt+0xC0 with 1.0: 0x48A5A0) if it's under the ground (+0x84 below 0.0);
    // 788-790 a hit if it's Martian (0x4406E0 = 1); 812-814 0x80F4 (the firer, 1-3, 120 / 240 / 360); 815-817
    // 0x80F5 (frozen) at 1-3; 818-820 0x80F6 at 1-3 and a hit; 821-823 0x80F7 at 1-3. (The switch also has 0x80F5
    // for 0x1F8-0x1FA, 0x21E-0x220 and 0x244-0x246, which the factory doesn't make as area blasts.)
    // (An area blast spares the firer's side outright; WExplosion's own detonation spares it unless the object is
    // turned, vt+0xF8.)
    // 812-814 post 0x80F4 (0x4355A0) with the firer (+0xCC) and the level 1-3, arriving 120 / 240 / 360 updates on
    // (ControlArrives).
    void BattleMap::AreaDetonate(int type, int x, int y, int radius, int power, int firer, int attacker, int weapon)
    {
        for (size_t i = 0; i < units.size(); i++)
        {
            Unit& u = units[i];
            auto ut = setup.units->find(u.type);
            if (ut == setup.units->end() || ut->second.category == 4) continue;
            if (firer >= 0 && u.group == firer) continue;
            const int reach = radius + u.drive.radius;
            const int d = Octagonal(int(u.x), int(u.y), x, y);
            if (d > reach || reach == 0) continue;
            auto hit = [&]() {
                if (units[i].health <= 0) return;
                const float h = float((1.0 - double(float(d)) / double(float(reach))) * power);
                Hit(i, int(double(h) * 1.0), attacker, type, weapon);
            };
            if (type >= 718 && type <= 720) { if (u.drive.lift < 0.0f) hit(); }
            else if (type >= 788 && type <= 790) { if (ut->second.faction == 1) hit(); }
            else if (type >= 812 && type <= 814) controlPosts.push_back({ ticks + 120 * (type - 811), u.id, attacker, type - 811 });
            else if (type >= 815 && type <= 817) SetStatus(i, 0x80F5, type - 814);
            else if (type >= 818 && type <= 820) { SetStatus(i, 0x80F6, type - 817); hit(); }
            else if (type >= 821 && type <= 823) SetStatus(i, 0x80F7, type - 820);
        }
        // the scenery: of the cases only 818-820's hit reaches it (it's never under the ground or Martian, and the
        // statuses' messages aren't a WScenery's)
        if (type < 818 || type > 820) return;
        for (size_t i = 0; i < props.size(); i++)
        {
            Prop& p = props[i];
            if (!p.alive || p.health <= 0) continue;
            auto pt = setup.units->find(p.type);
            if (pt == setup.units->end() || pt->second.category == 4) continue;
            const int reach = radius + int16_t(pt->second.hitPoints);
            const int d = Octagonal(int(p.x), int(p.y), x, y);
            if (d > reach || reach == 0) continue;
            const float h = float((1.0 - double(float(d)) / double(float(reach))) * power);
            DamageProp(i, int(double(h) * 1.0));
        }
    }

    // A scenery object hit (0x48AB20, as a unit): its health less the amount; at or under 0 a 0x80A0 is posted to it
    // 10 + rand() % 20 updates on
    void BattleMap::DamageProp(size_t index, int amount)
    {
        Prop& p = props[index];
        if (p.health <= 0) return;
        p.health -= amount;
        if (p.health <= 0)
        {
            const int at = ticks + random::Rand() % 20 + 10;
            if (p.dieAt < 0 || at < p.dieAt) p.dieAt = at;
        }
    }

    // 0x80A0 for a scenery object (0x489FA0, as a unit's): gone (0x8057: its view objects and its light; vt+0xE8,
    // 0x48A9A0, frees its footprint, 0x47C3A0), its type's destruction effect (+0x98) at its point,
    // ftol(+0x84 + its +0x41 byte x 1.414) up; the key buildings speak as a unit's do
    void BattleMap::PropDies(size_t index)
    {
        Prop& p = props[index];
        p.alive = false;
        objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const ViewObject& o) {
            return o.prop == int(index) || (p.light >= 0 && o.light == p.light); }), objects.end());
        if (p.light >= 0 && size_t(p.light) < lights.size()) lights[size_t(p.light)].alive = false;
        auto t = setup.units->find(p.type);
        if (t == setup.units->end()) return;
        const int c = t->second.category;
        if ((c == 0 || c == 2 || c == 3) && !(p.type >= 0xCF && p.type <= 0xE5))
            land.ClearOccupied(int(p.x), int(p.y), int16_t(t->second.currentHitPoints), 2);
        const int z = int(double(p.z) - double(t->second.size41) * -1.414);
        Explode(t->second.field98, int(p.x), int(p.y), z);
        const bool key = p.type == (setup.sector == 3 ? 0x33E : 0x78) || p.type == (setup.sector == 30 ? 0x116 : 0x3A);
        if (key && setup.sound) setup.sound->Play(0x13B, 1.0f);
    }

    // 0x80F5-0x80F7 (0x495CB0): the timer (vt+0x88: +0xDC) (level + 2) x 150 updates and the status (vt+0x90: +0xE0,
    // 0x496760); 0x80F6 first has the group's guns stood down (0x805A) and its stance 0 (0x80B2). 0x496760, the
    // status changing from 0x80F6 to another, gives the group the type's +0x43 byte as its stance again (from 0x80F7,
    // its own case). The unit's update (0x4967E0) counts the timer down (vt+0x8C) and at 0 sets the status 0.
    // Frozen (vt+0x108 = 0x489860: the timer running and the status 0x80F5) stops its guns (0x496920) and its driving
    // (0x4909D0). Turned (vt+0xF8 = 0x489880: running and 0x80F7): set (0x496100, TurnOnOwn) it aims at its own side's
    // vehicles; while it lasts it takes no other aim (0x496300) and its own side's explosions hit it (0x488200).
    // *Provisional*: frozen holds the vehicle still (Steer and Advance not run) rather than what 0x4909D0's callers do.
    void BattleMap::SetStatus(size_t index, int status, int level)
    {
        Unit& u = units[index];
        if (status == 0x80F6 && u.group >= 0 && size_t(u.group) < groupStance.size())
        {
            for (Unit& m : units)
                if (m.group == u.group)
                    for (Gun& g : m.guns)
                    {
                        g.armed = false;
                        if (g.state != 0) g.state = 4;
                        g.flags |= 1;
                    }
            SetStance(u.group, 0);
        }
        if (status == 0x80F7) TurnOnOwn(u);
        u.statusTime = (level + 2) * 150;
        ChangeStatus(u, status);
    }

    // vt+0x90 (0x496760): leaving 0x80F6, the group gets the type's +0x43 byte as its stance (0x80B2); leaving 0x80F7,
    // the group's target goes (0x482880(0))
    void BattleMap::ChangeStatus(Unit& u, int status)
    {
        if (u.status == 0x80F6 && status != 0x80F6 && u.group >= 0 && size_t(u.group) < groupStance.size())
            if (auto t = setup.units->find(u.type); t != setup.units->end()) SetStance(u.group, t->second.field43);
        if (u.status == 0x80F7 && status != 0x80F7 && u.group >= 0) StopAttacking(u.group);
        u.status = status;
    }

    // 0x80B2 to a group (0x4817C0): it keeps the stance (+0x54) and each member takes it (+0xD8)
    void BattleMap::SetStance(int group, int stance)
    {
        if (group < 0 || size_t(group) >= groupStance.size()) return;
        groupStance[size_t(group)] = stance;
        for (Unit& m : units)
            if (m.group == group) m.stance = stance;
    }

    // 0x496100, as 0x80F7 arrives (before its status is set): every vehicle (+0xB8 bit 0) of its own side (0x4406E0)
    // within its sight (+0x48; 0x43FF00 from whole positions, under it), neither under the ground (+0x84 below 0.0),
    // is approached (vt+0xC4, 0x496300) in turn, the last one's aim standing; itself is passed over by 0x496300
    void BattleMap::TurnOnOwn(Unit& u)
    {
        auto t = setup.units->find(u.type);
        if (t == setup.units->end()) return;
        const int sight = ObjOjd::Vision(t->second, setup.armour);
        for (const Unit& o : units)
        {
            if (o.players != u.players || !o.vehicle) continue;
            if (Octagonal(int(u.x), int(u.y), int(o.x), int(o.y)) >= sight) continue;
            if (o.drive.lift < 0.0f || u.drive.lift < 0.0f) continue;
            Approach(u, o);
        }
    }

    void BattleMap::StatusTick(Unit& u)
    {
        if (u.statusTime == 0) return;
        if (--u.statusTime == 0) ChangeStatus(u, 0);
    }
}

namespace wow
{
    // 0x495CB0's case 0x80F4 (firer, level): with the firer still there (0x440110) and the object not the player's
    // (+0x24 not 0) and the battle's key building (the Martian base 0x33E where the kind is 3, else the
    // Communications Centre 0x78; Parliament 0x116 where it's 0x1E, else the Command Post 0x3A), the sector (vt+0xDC -> +0x48 ->
    // +0x48) is told 0x80F4 (0x43B920: its +0x128 the level, dword_4E9B60 the firer's side, 0x4406E0) and 0x115 is
    // spoken with the building's voice (byte_51A190). Any other object does nothing with it.
    // *Provisional*: the kind is taken as the sector, as for the battle's end.
    void BattleMap::ControlArrives(const ControlPost& p)
    {
        const Unit* u = UnitById(p.unit);
        const Unit* firer = UnitById(p.attacker);
        if (!u || !firer || p.attacker == 0 || u->players) return;
        const bool key = u->type == (setup.sector == 3 ? 0x33E : 0x78) || u->type == (setup.sector == 30 ? 0x116 : 0x3A);
        if (!key) return;
        controlLevel = p.level;
        controlByPlayer = firer->players;
        Speak(0x115, u->group);
    }
}
