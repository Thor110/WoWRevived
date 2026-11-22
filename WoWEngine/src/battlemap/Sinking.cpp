// The units that go under (0x498D40's class on the vehicle: the Tunnelling Track Layers 7-9, 0x493B90; the
// Submersibles 0x16-0x18, 0x4935E0): Tunnel (0x80EE) and Submerge (0x80EF). See docs/BATTLEMAP.md "Going under".
#include "battlemap/BattleMap.h"
#include "core/Random.h"
#include "platform/Sound.h"
#include <algorithm>

namespace wow
{
    bool BattleMap::Sinker(int type)
    {
        return (type >= 7 && type <= 9) || (type >= 0x16 && type <= 0x18);
    }

    // 0x47FFE0: the terrain at its vertex is water (1-3)
    bool BattleMap::OnWater(const Unit& u) const
    {
        const int vx = std::clamp(int((int64_t(int32_t(uint32_t(int(u.x)) << 16)) + land.HalfCell()) / land.Cell()), 0, land.Width() - 1);
        const int vy = std::clamp(int((int64_t(int32_t(uint32_t(int(u.y)) << 16)) + land.HalfCell()) / land.Cell()), 0, land.Height() - 1);
        const int terrain = land.At(vx, vy).atm & 15;
        return terrain >= 1 && terrain <= 3;
    }

    // vt+0x184 (0x4990D0) with down: going down sets +0x190 bit 1 (hidden, vt+0x118 = 0x493600) at once; coming up,
    // with bit 0 (under) clear, vt+0x188(1) first. Then +0x194 is 64 and vt+0x170: the class's effect at its point
    // (the Tunnelling Track Layers' 0x3DE, 0x493C80; the Submersibles' 0x3DC, 0x493680) and, unless +0xB8 bit 3
    // (vt+0x9C), its sound there (0x4883E0: 0x1B8, 0x1B9).
    // *Provisional*: +0xB8 bit 3 is taken as clear.
    void BattleMap::StartSink(size_t index, bool down)
    {
        Unit& u = units[index];
        if (down) u.hidden = true;
        else if (!u.under) EndSink(index, true);
        u.sink = 64;
        const bool tunneller = u.type >= 7 && u.type <= 9;
        SpawnEffect(tunneller ? 0x3DE : 0x3DC, u.x, u.y, 0);
        if (setup.sound)
        {
            const int ix = int(u.x), iy = int(u.y);
            setup.sound->Play(tunneller ? 0x1B8 : 0x1B9, 1.0f, 0, ix, land.ProjectY(iy, land.HeightAt(ix, iy) + int(u.drive.lift)));
        }
    }

    // vt+0x188 (0x499150) with under: down, the player's gets its label (vt+0x17C, 0x4991D0: string 0x6F at its screen
    // point, +0x198) and its marker (vt+0x174, 0x4992D0: GType 0x359, +0x19C); up, both go (vt+0x180, vt+0x178), its
    // group's mode is 0 (0x4807E0) and bit 1 is cleared; bit 0 is set to under.
    // The label is drawn by DrawLabels while the unit is under.
    void BattleMap::EndSink(size_t index, bool under)
    {
        Unit& u = units[index];
        if (!under)
        {
            if (u.group >= 0 && size_t(u.group) < groupMode.size()) groupMode[size_t(u.group)] = 0;
            u.hidden = false;
        }
        u.under = under;
    }

    // The update (0x498F20, after the vehicle's 0x4913B0): with +0x194 running, it goes down by one; going down (bit 0
    // clear) the height (+0x84) is (+0x194 - 0x40) x 3, at 0 vt+0x188(1); coming up it's -(+0x194 x 3) (vt+0x40 to
    // its point and 0x47F320(1)), at 0 vt+0x188(0). Then the detected marker (as the mines'). By class:
    //  - the Tunnelling Track Layers (0x493C20): its renderer's +0x74 follows (-+0x194 x 16 going down, +0x194 x 16
    //    coming up, 0 at rest); coming up on water (0x47FFE0) it's gone (vt+0x78 = 0x495BB0: 0x807B to both players,
    //    out of the world);
    //  - the Submersibles (0x493640): off water, when rand() % 2 is 0, 10 damage (vt+0xE0).
    // *Provisional*: the renderer's +0x74 isn't in.
    bool BattleMap::SinkTick(size_t index)
    {
        Unit& u = units[index];
        if (!Sinker(u.type)) return false;
        if (u.sink)
        {
            u.sink--;
            if (!u.under)
            {
                u.drive.lift = float((u.sink - 0x40) * 3);
                if (u.sink == 0) EndSink(index, true);
            }
            else
            {
                u.drive.lift = float(-(u.sink * 3));
                if (u.sink == 0) EndSink(index, false);
            }
        }
        if (u.type >= 7 && u.type <= 9)
        {
            if (u.sink && u.under && OnWater(u))
            {
                Vanish(index);
                return true;
            }
            return false;
        }
        if (!OnWater(u) && random::Rand() % 2 == 0) Damage(index, 10);
        return false;
    }

    // vt+0x124 for the class (0x499120), as it stops (0x4913B0's edge): hidden (vt+0x118), it comes up (vt+0x184(0));
    // then the vehicle's (0x491590)
    void BattleMap::Surface(size_t index)
    {
        Unit& u = units[index];
        if (Sinker(u.type) && u.hidden) StartSink(index, false);
    }

    // 0x80EE / 0x80EF to a member (0x498E90): unless hidden (vt+0x118), it goes down (vt+0x184(1)); both are taken
    // the same way, whichever the class
    void BattleMap::GoUnder(size_t index)
    {
        Unit& u = units[index];
        if (Sinker(u.type) && !u.hidden) StartSink(index, true);
    }

    // vt+0x78 (0x495BB0): out of the world without a death (0x807B to both players, then gone)
    void BattleMap::Vanish(size_t index)
    {
        ClearFootprint(units[index]);
        StopUnitSounds(units[index]);
        const Unit u = units[index];
        if (u.engineSound && setup.sound) setup.sound->Stop(u.engineSound);
        for (const Turret& tr : u.turrets)
            if (tr.sound && setup.sound) setup.sound->Stop(tr.sound);
        objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const ViewObject& o) { return o.unit == int(index); }), objects.end());
        for (ViewObject& o : objects)
            if (o.unit > int(index)) o.unit--;
        units.erase(units.begin() + std::ptrdiff_t(index));
        if (std::none_of(units.begin(), units.end(), [&](const Unit& o) { return o.group == u.group; })) GroupGone(u.group);
    }

    // The cheats punyhumans (0x806C) and atchooo (0x806D) at the sector (0x43B15C): by the local player's side (its
    // +0x24), 0x806C the other side's and 0x806D its own units, constructions and resources (0x446370, kinds 1-3)
    // each have every battle object taken out (0x421F50 -> 0x47D4E0: vt+0x78 on each; a vehicle's 0x495BB0 goes
    // without a death, a key building's 0x48AC50 is lost).
    // *Provisional*: every object goes as a vehicle does, the key building also lost.
    void BattleMap::CheatRemoveSide(bool own)
    {
        for (size_t i = units.size(); i-- > 0;)
        {
            if (units[i].players != own) continue;
            const Unit u = units[i];
            Vanish(i);
            const bool key = u.type == (setup.sector == 3 ? 0x33E : 0x78) || u.type == (setup.sector == 30 ? 0x116 : 0x3A);
            if (key && !u.vehicle && u.group >= 0) KeyBuildingLost(u.players);
            if (i > units.size()) i = units.size();
        }
    }
}
