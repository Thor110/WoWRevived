// The battle map's reinforcement points (0x43D150) and the Retreat order (0x480EF0, 0x43D3F0). See docs/BATTLEMAP.md
// "Reinforcement points and Retreat".
#include "battlemap/BattleMap.h"
#include "core/Random.h"
#include "platform/Sound.h"
#include <algorithm>
#include <cmath>

namespace wow
{
    // 0x43D150, as the battle map is entered (0x439770), with the battle on (the report's +0x54) or the debug flag
    // byte_4D1279: the table cleared, then for each sector 1-30 that has its holder's command building finished
    // (0x43C990: Parliament 0x116 in London, else 0x13A, for a Human holder; the Martian base 0x33E in sector 3, else
    // 0x78, for a Martian one; in either of its construction lists), for each link kind 7, 5 and 1: the point units
    // from it enter this sector by (0x436050), (0, 0) for none; a point nearest an edge is put 100 in from it (its x
    // nearest the left: 100; y the top: 100; the right: the width less 100; the bottom: the height less 100). With
    // the player holding that sector (or the debug flag), and its war map taking a unit of type 19 (kind 5) or 1
    // (else) by its move class (0x43AC40), the point goes in the table (x, y, the sector, the kind, held, 0) and its
    // marker, type 0x3DB, is made there (+0xCC the sector, +0xC8 the kind).
    // *Provisional*: 0x43AC40's room check isn't in (taken as yes); 0x436050 is the engine's EntryPoint, whose
    // first point serves kinds 7 and 1 and its second kind 5 (so 7 and 1 share a point).
    void BattleMap::SetUpReinforcements()
    {
        rpoints.clear();
        if (!battleOn) return;
        const int playerSide = setup.human ? 0 : 1;
        (void)playerSide;
        const int w = land.WorldWidth();
        const int h = int((int64_t(land.Height()) * land.Cell()) >> 16);
        for (const Setup::Source& src : setup.sources)
        {
            if (!src.hq) continue;
            for (int kind : { 7, 5, 1 })
            {
                int x = 0, y = 0;
                deploy::EntryPoint(src.sector, setup.sector, kind == 5 ? 5 : 0, x, y);
                if (x + y == 0) continue;
                const int left = x, top = y, right = w - x, bottom = h - y;
                if (left < top && left < right && left < bottom) x = 100;
                if (top < left && top < right && top < bottom) y = 100;
                if (right < left && right < top && right < bottom) x = w - 100;
                if (bottom < left && bottom < top && bottom < right) y = h - 100;
                if (!src.players) continue;
                rpoints.push_back({ x, y, src.sector, kind, src.players });
                // its marker (type 0x3DB, a sprite: GType 862)
                if (auto u = setup.units->find(0x3DB); u != setup.units->end())
                    if (const GType* g = res.FindGType(u->second.field28); g && g->gclass == 0 && res.GetFrame(u->second.field28, 0))
                        objects.push_back({ y + 1, x, land.ProjectY(y, land.HeightAt(x, y)), nullptr, nullptr, g->paletteSlot, u->second.field28 });
            }
        }
        std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
    }

    // 0x43D3F0 (x, y, move class, count): the kind wanted by the move class (2-4 and 7: 1; 5 and 6: 5, and the room
    // check's "not land" flag; 0 and 1 as they are); of the table's points of that kind the player holds, the nearest
    // (0x43FF00 from the point, under 1000000); with a count, the room check (0x43AC40) on its sector.
    // moveClass -1 takes any kind (the engine's own, for a unit's arrival).
    // *Provisional*: the room check isn't in.
    int BattleMap::NearestPoint(int x, int y, int moveClass) const
    {
        int kind = moveClass;
        if (moveClass == 2 || moveClass == 3 || moveClass == 4 || moveClass == 7) kind = 1;
        if (moveClass == 5 || moveClass == 6) kind = 5;
        int best = -1, bestD = 1000000;
        for (size_t i = 0; i < rpoints.size(); i++)
        {
            const ReinforcePoint& p = rpoints[i];
            if ((moveClass >= 0 && p.kind != kind) || !p.owned) continue;
            const int a = std::abs(p.x - x), b = std::abs(p.y - y);
            const int d = b - (std::min(a, b) >> 1) + a;
            if (d < bestD) { bestD = d; best = int(i); }
        }
        return best;
    }

    // Retreat (0x480EF0) for a group: the nearest point (0x43D3F0) to its first member's whole position for its type's
    // move class (+0x52) and its count of objects (0x4216D0). With +0x1D8 bits 8 and 9 both set, or no point, speech
    // 0x121 ("can't", 0x464EE0 with its voice) and the group's leader is told (0x4842D0). Otherwise: the members told
    // 0x80B9 (0x4821A0), the target cleared (+0x2C / +0x30 0, +0x34..+0x3C -1), the guns stood down (0x805A), +0x1D8
    // bit 0 cleared (0x480EB0), each member 0x80B9 again, bit 3 set, bit 0 set, the group sent to the point (0x804F
    // with its x and y), and each member's war map object (+0x48) told 0x80BB with the point's sector.
    // *Provisional*: what 0x80BB does to the war map object isn't traced: here a member within 64 of the point
    // leaves the map, and its object goes to that sector when the battle map closes.
    void BattleMap::Retreat(int group)
    {
        if (group < 0 || size_t(group) >= groupRetreat.size()) return;
        const Unit* first = nullptr;
        for (const Unit& u : units)
            if (u.group == group) { first = &u; break; }
        if (!first || !first->players) return;
        auto t = setup.units->find(first->type);
        if (t == setup.units->end()) return;
        const int k = NearestPoint(int(first->x), int(first->y), t->second.field52);
        if (k < 0)
        {
            Speak(0x121, group);
            return;
        }
        StopAttacking(group);
        const ReinforcePoint p = rpoints[size_t(k)];
        groupRetreat[size_t(group)] = p.sector;
        groupBusy[size_t(group)] = true;
        std::vector<size_t> members;
        for (size_t i = 0; i < units.size(); i++)
            if (units[i].group == group && units[i].vehicle) members.push_back(i);
        OrderGroupMove(group, members, p.x, p.y);
    }

    // A unit leaving the map at its point: gone without its destruction (0x8057), its group marked as having left
    void BattleMap::LeaveMap(size_t index)
    {
        const Unit u = units[index];
        if (u.group >= 0 && size_t(u.group) < groupLeft.size()) groupLeft[size_t(u.group)] = true;
        if (u.engineSound && setup.sound) setup.sound->Stop(u.engineSound);
        for (const Turret& tr : u.turrets)
            if (tr.sound && setup.sound) setup.sound->Stop(tr.sound);
        objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const ViewObject& o) { return o.unit == int(index); }), objects.end());
        for (ViewObject& o : objects)
            if (o.unit > int(index)) o.unit--;
        units.erase(units.begin() + std::ptrdiff_t(index));
        if (std::none_of(units.begin(), units.end(), [&](const Unit& o) { return o.group == u.group; })) GroupGone(u.group);
    }

    std::vector<std::pair<int, int>> BattleMap::RetreatedRefs() const
    {
        std::vector<std::pair<int, int>> out;
        for (size_t g = 0; g < setup.groups.size() && g < groupLeft.size(); g++)
        {
            if (!groupLeft[g] || setup.groups[g].ref < 0) continue;
            bool still = false;
            for (const Unit& u : units) if (u.group == int(g)) { still = true; break; }
            if (!still) out.push_back({ setup.groups[g].ref, groupRetreat[g] });
        }
        return out;
    }
}
