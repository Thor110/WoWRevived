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
    // (0x43C990: Parliament 0x116 in London, else the Command Post 0x3A, for a Human holder; the Martian base 0x33E in sector 3, else
    // 0x78, for a Martian one; in either of its construction lists), for each link kind 7, 5 and 1: the point units
    // from it enter this sector by (0x436050), (0, 0) for none; a point nearest an edge is put 100 in from it (its x
    // nearest the left: 100; y the top: 100; the right: the width less 100; the bottom: the height less 100). With
    // the player holding that sector (or the debug flag), and its war map taking a unit of type 19 (kind 5) or 1
    // (else) by its move class (0x43AC40), the point goes in the table (x, y, the sector, the kind, held, 0) and its
    // marker, type 0x3DB, is made there (+0xCC the sector, +0xC8 the kind).
    // The room check (0x43AC40 on the sector, its land flag from the type's move class +0x4E not 3, the count 1) is
    // the war map's (WarMap::HasRoom through the setup's hasRoom).
    // *Provisional*: 0x436050 is the engine's EntryPoint, whose first point serves kinds 7 and 1 and its second
    // kind 5 (so 7 and 1 share a point).
    void BattleMap::SetUpReinforcements()
    {
        rpoints.clear();
        if (!battleOn && !setup.mat) return;       // 0x43D171: with no battle on, only with Mat
        const int playerSide = setup.human ? 0 : 1;
        (void)playerSide;
        const int w = land.WorldWidth();
        const int h = int((int64_t(land.Height()) * land.Cell()) >> 16);
        for (const Setup::Source& src : setup.sources)
        {
            if (!src.hq && !setup.mat) continue;       // 0x43D2D2: with Mat every neighbouring sector, qualifying or not
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
                if (!src.players && !setup.mat) continue;      // ("or the debug flag")
                if (setup.hasRoom)
                {
                    auto rt = setup.units->find(kind == 5 ? 19 : 1);
                    const bool land = rt == setup.units->end() || rt->second.moveClass != 3;
                    if (!setup.hasRoom(land, src.sector, 1)) continue;
                }
                rpoints.push_back({ x, y, src.sector, kind, src.players });
                // its marker (type 0x3DB, a sprite: GType 862)
                if (auto u = setup.units->find(0x3DB); u != setup.units->end())
                    if (const GType* g = res.FindGType(u->second.field28); g && g->gclass == 0 && res.GetFrame(u->second.field28, 0))
                    {
                        objects.push_back({ y + 1, x, land.ProjectY(y, land.HeightAt(x, y)), nullptr, nullptr, g->paletteSlot, u->second.field28 });
                        objects.back().level = 255 >> 2;    // WScenery's 0x4897D0: ftol(255 x the fog level) out of the fog
                    }
            }
        }
        std::stable_sort(objects.begin(), objects.end(), [](const ViewObject& a, const ViewObject& b) { return a.key < b.key; });
    }

    // 0x43D3F0 (x, y, move class, count): the kind wanted by the move class (2-4 and 7: 1; 5 and 6: 5, and the room
    // check's "not land" flag; 0 and 1 as they are); of the table's points of that kind the player holds, the nearest
    // (0x43FF00 from the point, under 1000000); with a count, the room check (0x43AC40) on its sector.
    // moveClass -1 takes any kind (the engine's own, for a unit's arrival).
    int BattleMap::NearestPoint(int x, int y, int moveClass, int count) const
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
        if (best >= 0 && count && !HasRoom(!(moveClass == 5 || moveClass == 6), rpoints[size_t(best)].sector, count))
            return -1;
        return best;
    }

    bool BattleMap::HasRoom(bool land, int sector, int count) const
    {
        if (!setup.hasRoom) return true;
        const auto& left = land ? leftLand : leftBoats;
        const auto it = left.find(sector);
        return setup.hasRoom(land, sector, count + (it == left.end() ? 0 : it->second));
    }

    uint32_t BattleMap::GroupBits(int group) const
    {
        uint32_t bits = 0;
        for (const Unit& u : units)
        {
            if (u.group != group) continue;
            auto t = setup.units->find(u.type);
            const int mc = t == setup.units->end() ? 0 : t->second.field52;
            if (mc == 5 || mc == 6) bits |= 0x100;
            if (mc >= 1 && mc <= 4) bits |= 0x200;
        }
        return bits;
    }

    // Retreat (0x480EF0) for a group: the nearest point (0x43D3F0) to its first member's whole position for its type's
    // move class (+0x52) and its count of objects (0x4216D0: one war map object a group here). With +0x1D8 bits 8 and 9 both set, or no point, speech
    // 0x121 ("can't", 0x464EE0 with its voice) and the group's leader is told (0x4842D0). Otherwise: the members told
    // 0x80B9 (0x4821A0), the target cleared (+0x2C / +0x30 0, +0x34..+0x3C -1), the guns stood down (0x805A), +0x1D8
    // bit 0 cleared (0x480EB0), each member 0x80B9 again, bit 3 set, bit 0 set, the group sent to the point (0x804F
    // with its x and y), and each member's war map object (+0x48) told 0x80BB with the point's sector.
    // Each member's second 0x80B9 has 1: its steering's bit 9, so at its route's end it leaves the map (0x491A96).
    // *Provisional*: what 0x80BB does to the war map object isn't traced: its object goes to that sector when the
    // battle map closes.
    void BattleMap::Retreat(int group)
    {
        if (group < 0 || size_t(group) >= groupRetreat.size()) return;
        const Unit* first = nullptr;
        for (const Unit& u : units)
            if (u.group == group) { first = &u; break; }
        if (!first || !first->players) return;
        auto t = setup.units->find(first->type);
        if (t == setup.units->end()) return;
        // with both +0x1D8 bits 8 and 9 (boats and land vehicles together) it can't; the room for the group's battle
        // units (0x4216D0, 0x480F22)
        const int k = (GroupBits(group) & 0x300) == 0x300 ? -1 : NearestPoint(int(first->x), int(first->y), t->second.field52, GroupCount(group));
        if (k < 0)
        {
            Speak(0x121, group);
            return;
        }
        RetreatVia(group, size_t(k));
    }

    // The going (0x480EF0 / 0x481170 alike): the members told 0x80B9, the target cleared, the guns stood down
    // (0x805A), bit 0 cleared, each member 0x80B9 again, bit 3 set, bit 0 set, 0x804F to the point, and each member's
    // war map object 0x80BB with the point's sector.
    void BattleMap::RetreatVia(int group, size_t k)
    {
        StopAttacking(group);
        const ReinforcePoint p = rpoints[k];
        // each member's war map object told 0x80BB with the sector (kept by war map object)
        for (const Unit& u : units)
            if (u.group == group && u.ref >= 0 && size_t(u.ref) < groupRetreat.size()) groupRetreat[size_t(u.ref)] = p.sector;
        groupBusy[size_t(group)] = true;
        std::vector<size_t> members;
        for (size_t i = 0; i < units.size(); i++)
            if (units[i].group == group && units[i].vehicle) members.push_back(i);
        // each member's second 0x80B9, with 1 (0x4810B4 / 0x4811DA): +0x188 bit 9, retreating, before the move
        for (size_t i : members) units[i].drive.flags |= 0x200;
        OrderGroupMove(group, members, p.x, p.y);
    }

    // A unit leaving the map at its point: gone without its destruction (0x8057), its group marked as having left
    void BattleMap::LeaveMap(size_t index)
    {
        ClearFootprint(units[index]);
        StopUnitSounds(units[index]);
        const Unit u = units[index];
        if (u.ref >= 0 && size_t(u.ref) < groupLeft.size()) groupLeft[size_t(u.ref)] = true;
        if (u.engineSound && setup.sound) setup.sound->Stop(u.engineSound);
        for (const Turret& tr : u.turrets)
            if (tr.sound && setup.sound) setup.sound->Stop(tr.sound);
        objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const ViewObject& o) { return o.unit == int(index); }), objects.end());
        for (ViewObject& o : objects)
            if (o.unit > int(index)) o.unit--;
        units.erase(units.begin() + std::ptrdiff_t(index));
        if (std::none_of(units.begin(), units.end(), [&](const Unit& o) { return o.group == u.group; })) GroupGone(u.group);
        // its war map object's last gone (0x429453: +0x60 up to its count): one more left for the sector it went to,
        // as a boat when its type's +0x4E (word_51A13E) is 3 (the war map's move class)
        if (u.ref >= 0 && size_t(u.ref) < groupRetreat.size() && std::none_of(units.begin(), units.end(), [&](const Unit& o) { return o.ref == u.ref; }))
        {
            auto t = setup.units->find(u.type);
            const bool boat = t != setup.units->end() && t->second.moveClass == 3;
            (boat ? leftBoats : leftLand)[groupRetreat[size_t(u.ref)]]++;
        }
    }

    std::vector<std::pair<int, int>> BattleMap::RetreatedRefs() const
    {
        std::vector<std::pair<int, int>> out;
        for (size_t g = 0; g < setup.groups.size() && g < groupLeft.size(); g++)
        {
            if (!groupLeft[g] || setup.groups[g].ref < 0) continue;
            bool still = false;
            for (const Unit& u : units) if (u.ref == int(g)) { still = true; break; }
            if (!still) out.push_back({ setup.groups[g].ref, groupRetreat[g] });
        }
        return out;
    }

    // An attack (packet 0x10, 0x481F70) on a reinforcement point's marker (type 0x3DB: +0xCC its sector, +0xC8 its
    // kind): kinds 1 and 7, with +0x1D8 bit 8 clear and room for the group's objects (0x4216D0) as land (0x43AC40);
    // kind 5, with bit 9 clear and room as boats; then the group goes through it (0x481170: as Retreat, to the
    // marker's point, +0x78 / +0x7C). Otherwise speech 0x121 in the group's voice (0x449120) and its leader told.
    void BattleMap::RetreatToMarker(int group, size_t k)
    {
        const ReinforcePoint& p = rpoints[k];
        bool ok = false;
        const uint32_t bits = GroupBits(group);
        const int count = GroupCount(group);
        if (p.kind == 1 || p.kind == 7) ok = !(bits & 0x100) && HasRoom(true, p.sector, count);
        else if (p.kind == 5) ok = !(bits & 0x200) && HasRoom(false, p.sector, count);
        if (!ok) { Speak(0x121, group); return; }
        RetreatVia(group, k);
    }

    // The marker under a screen point: its sprite's box (GType 862, less its hot spot) holding it, the frontmost
    int BattleMap::MarkerAt(int x, int y) const
    {
        auto u = setup.units->find(0x3DB);
        if (u == setup.units->end()) return -1;
        const int gtype = u->second.field28;
        const GType* g = res.FindGType(gtype);
        const IndexedImage* image = res.GetFrame(gtype, 0);
        if (!g || !image) return -1;
        int best = -1, bestY = INT32_MIN;
        for (size_t i = 0; i < rpoints.size(); i++)
        {
            const ReinforcePoint& p = rpoints[i];
            const int sx = p.x - g->hotX - scrollX, sy = land.ProjectY(p.y, land.HeightAt(p.x, p.y)) - g->hotY - scrollY;
            if (x >= sx && x < sx + image->width && y >= sy && y < sy + image->height && sy > bestY) { best = int(i); bestY = sy; }
        }
        return best;
    }
}
