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
    }

    // 0x80A0 (0x489FA0): the object posts itself 0x8057 (gone) and makes its type's destruction effect (+0x98,
    // 0x488070) at its point, ftol(+0x84 + its +0x41 byte x 1.414) up, with its parent as the firer; the battle's
    // key building (the Martian Base 0x33E where the kind is 3, else the Communications Centre 0x78; Parliament
    // 0x116 where it's 0x1E, else 0x13A) going, with a battle on, speaks 0x112 if it was the player's, else 0x13B.
    // *Provisional*: gone at once rather than on the next update; the kind is taken as the sector.
    void BattleMap::Die(size_t index)
    {
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
        if (t == setup.units->end()) return;
        const int z = int(double(u.drive.lift) - double(t->second.size41) * -1.414);
        Explode(t->second.field98, int(u.x), int(u.y), z);
        const bool key = u.type == (setup.sector == 3 ? 0x33E : 0x78) || u.type == (setup.sector == 30 ? 0x116 : 0x13A);
        if (key && setup.sound) setup.sound->Play(u.players ? 0x112 : 0x13B, 1.0f);
    }

    // 0x488070 with a WExplosion (0x488180): +0xC8 the type's +0x44 (its radius), +0xCA its +0x4A (its power).
    // As it's added (0x488350): its sound (+0xA2) at its point; its parts started (0x489000 -> 0x47EF30: each a
    // child at the part's offset, explosions among them going off first); its radius set; then Detonate (0x488200):
    // everything within reach (0x47FC50: the octagonal distance under its radius + the other's, kind 4 left out),
    // but the firer and, unless they say otherwise (vt+0xF8), the firer's side, is hit (vt+0xC0 with 1.0: 0x48A5A0,
    // when its health is over 0 and +0xB8 bit 7 is set: ftol((1 - the distance / both radii) x the power x 1.0) to
    // vt+0xE0); and under 64 up, not on water (0x47FFE0: terrain 1-3), the ground is scorched (0x804B -> 0x47C4C0
    // with the radius and the power). With its sprite it then plays as a WAnimEffect.
    // *Provisional*: only units are hit (the scenery's damage isn't in); a death explosion's firer (the dying
    // object's parent) is always none, so both sides are hit; a gun's spares its unit's group (0x47EB00); light parts (334-346) and emitters (smoke) aren't
    // made.
    // The area blasts (0x487B00 on WExplosion: the factory's 718-720, 788-790, 812-823)
    bool BattleMap::IsAreaBlast(int t) { return (t >= 718 && t <= 720) || (t >= 788 && t <= 790) || (t >= 812 && t <= 823); }

    void BattleMap::Explode(int type, int x, int y, int z, int firer)
    {
        auto t = setup.units->find(type);
        const bool area = IsAreaBlast(type);
        if (t == setup.units->end() || (!IsExplosion(type) && !area)) return;
        const int radius = int16_t(t->second.hitPoints);
        const int power = MaxHealth(type);
        const int ground = land.HeightAt(x, y);
        if (t->second.fieldA2 && setup.sound)
            setup.sound->Play(t->second.fieldA2, 1.0f, 0, x, land.ProjectY(y, ground + std::max(z, 0)));
        for (const auto& part : t->second.parts)
        {
            const int px = x + int16_t(part.b), py = y + int16_t(part.c), pz = z + int16_t(part.d);
            if (IsExplosion(part.a) || IsAreaBlast(part.a)) Explode(part.a, px, py, pz, firer);
            else if (part.a >= 334 && part.a <= 346) AddLight(part.a, px, py, land.HeightAt(px, py) + pz);
            else SpawnEffect(part.a, float(px), float(py), float(pz));
        }
        if (area)
        {
            AreaDetonate(type, x, y, radius, power, firer);
            SpawnEffect(type, float(x), float(y), float(std::max(z, 0)));
            return;
        }
        // Detonate
        for (size_t i = 0; i < units.size(); i++)
        {
            Unit& u = units[i];
            auto ut = setup.units->find(u.type);
            if (ut == setup.units->end() || ut->second.category == 4) continue;
            if (firer >= 0 && u.group == firer) continue;   // the firer's own (0x47EB00), unless vt+0xF8 says not
            const int reach = radius + u.drive.radius;
            const int d = Octagonal(int(u.x), int(u.y), x, y);
            if (d > reach || reach == 0) continue;
            if (u.health <= 0) continue;
            const float hit = float((1.0 - double(float(d)) / double(float(reach))) * power);
            Damage(i, int(double(hit) * 1.0));
        }
        if (z < 64)
        {
            const int vx = std::clamp((int(uint32_t(x) << 16) + land.HalfCell()) / land.Cell(), 0, land.Width() - 1);
            const int vy = std::clamp((int(uint32_t(y) << 16) + land.HalfCell()) / land.Cell(), 0, land.Height() - 1);
            const int terrain = land.At(vx, vy).atm & 15;
            if (terrain < 1 || terrain > 3) land.Scorch(x, y, radius, power);
        }
        SpawnEffect(type, float(x), float(y), float(std::max(z, 0)));
    }

    // An area blast's detonation (vt+0xE0 = 0x487B50, in place of WExplosion's 0x488200: no splash for all, no
    // scorch): every object within reach (0x47FC50, its radius +0xAC) but the firer's side (0x47EB00) gets, by the
    // blast's type: 718-720 a hit (vt+0xC0 with 1.0: 0x48A5A0) if it's under the ground (+0x84 below 0.0);
    // 788-790 a hit if it's Martian (0x4406E0 = 1); 812-814 0x80F4 (the firer, 1-3, 120 / 240 / 360); 815-817
    // 0x80F5 (frozen) at 1-3; 818-820 0x80F6 at 1-3 and a hit; 821-823 0x80F7 at 1-3. (The switch also has 0x80F5
    // for 0x1F8-0x1FA, 0x21E-0x220 and 0x244-0x246, which the factory doesn't make as area blasts.)
    // *Provisional*: 0x80F4 (taking the key buildings over, 0x495CB0) and 0x80F7's 0x496100 aren't in; the
    // statuses' other effects (but frozen guns and 0x80F6's stance) aren't traced.
    void BattleMap::AreaDetonate(int type, int x, int y, int radius, int power, int firer)
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
                Damage(i, int(double(h) * 1.0));
            };
            if (type >= 718 && type <= 720) { if (u.drive.lift < 0.0f) hit(); }
            else if (type >= 788 && type <= 790) { if (ut->second.faction == 1) hit(); }
            else if (type >= 815 && type <= 817) SetStatus(i, 0x80F5, type - 814);
            else if (type >= 818 && type <= 820) { SetStatus(i, 0x80F6, type - 817); hit(); }
            else if (type >= 821 && type <= 823) SetStatus(i, 0x80F7, type - 820);
        }
    }

    // 0x80F5-0x80F7 (0x495CB0): the timer (vt+0x88: +0xDC) (level + 2) x 150 updates and the status (vt+0x90: +0xE0,
    // 0x496760); 0x80F6 first has the group's guns stood down (0x805A) and its stance 0 (0x80B2). 0x496760, the
    // status changing from 0x80F6 to another, gives the group the type's +0x43 byte as its stance again (from 0x80F7,
    // its own case). The unit's update (0x4967E0) counts the timer down (vt+0x8C) and at 0 sets the status 0.
    // Frozen (vt+0x108 = 0x489860: the timer running and the status 0x80F5) stops its guns (0x496920).
    // *Provisional*: the stance given back is 1 (the +0x43 byte isn't read).
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
            groupStance[size_t(u.group)] = 0;
        }
        u.statusTime = (level + 2) * 150;
        ChangeStatus(u, status);
    }

    void BattleMap::ChangeStatus(Unit& u, int status)
    {
        if (u.status == 0x80F6 && status != 0x80F6 && u.group >= 0 && size_t(u.group) < groupStance.size())
            groupStance[size_t(u.group)] = 1;
        u.status = status;
    }

    void BattleMap::StatusTick(Unit& u)
    {
        if (u.statusTime == 0) return;
        if (--u.statusTime == 0) ChangeStatus(u, 0);
    }
}
