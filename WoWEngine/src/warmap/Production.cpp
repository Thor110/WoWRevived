// The war map's factories (WBuildList 0x45F770-0x462EE0): their hourly progress and what comes of it. See
// docs/WARMAP.md "Production" and core/BuildList.h for the save format.
#include "warmap/WarMap.h"


namespace wow
{
    void WarMap::ReadFactories()
    {
        factories.clear();
        for (size_t i = 0; i < table.size(); i++)
            if (auto t = ReadTail(table[i]); t && t->hasFactory) factories[int(i)] = *t;
    }

    int WarMap::CounterOf(int ref) const
    {
        for (size_t i = 0; i < counters.size(); i++)
            for (int r : counters[i].refs) if (r == ref) return int(i);
        return -1;
    }

    bool WarMap::Working(int ref) const
    {
        auto it = factories.find(ref);
        return it != factories.end() && !it->second.queue.entries.empty();
    }

    // 0x45F970: what the object can be told to make, in type order (1-1008). A type is offered when the object's
    // type is one of its builders (OBJ.ojd records 49-51) and its research (the unit table's +0x04) is done or
    // obsolete, or it has none (0x442460). Where the sector has no command building finished (0x43C990) the only
    // thing offered is that building (58, or 278 in sector 30; 120, or 830 in sector 3; both are tried whoever's
    // factory it is), and not even that while the object is working on something already (0x42A410); where it
    // has one, neither 58 nor 120 is. Type 39 isn't offered in the sectors of byte_4BD2E0.
    std::vector<int> WarMap::BuildOptions(int ref) const
    {
        std::vector<int> out;
        if (ref < 0 || size_t(ref) >= table.size()) return out;
        const int self = table[size_t(ref)].Type();
        const int at = CounterOf(ref);
        const int sector = at >= 0 ? counters[size_t(at)].sector : table[size_t(ref)].Sector();
        const bool hasPost = HasCommandPost(sector);
        const bool working = Working(ref);
        for (const auto& [type, u] : unitData)
        {
            if (type < 1 || type > 1008) continue;
            if (u.builders[0] != self && u.builders[1] != self && u.builders[2] != self) continue;
            bool researched = true;
            for (const auto& r : research)
                if (r)
                    if (const Research::Item* item = r->ItemFor(type))
                        researched = item->state == Research::Done || item->state == Research::Obsolete;
            if (!researched) continue;
            bool ok = true;
            if (!hasPost)
            {
                if (working) ok = false;
                const int martianPost = sector == 3 ? 830 : 120, humanPost = sector == 30 ? 278 : 58;
                if (type != martianPost && type != humanPost) ok = false;
            }
            else if (type == 120 || type == 58) ok = false;
            if (type == 39 && warmap::Landlocked(sector)) ok = false;
            if (ok) out.push_back(type);
        }
        return out;
    }

    int WarMap::RefOfId(uint32_t id) const
    {
        if (id == 0xDEADDEAD) return -1;
        for (size_t i = 0; i < table.size(); i++)
            if (uint32_t(table[i].Id()) == id) return int(i);
        return -1;
    }

    // 0x43AC10 -> 0x43AC40: room in the sector for one more of the type. Whoever's factory it is, it counts the
    // local player's (0x446370(1, 1)): boats (move class 3) against "Max boats in sector", the rest against "Max
    // units in sector", those there and those on their way there (0x420060), plus this one, kept below the limit.
    // The game adds a count per sector from 0x4F86B8 / 0x4F76F0 too, which is still to be worked out (taken as 0).
    bool WarMap::HasRoomFor(int type, int sector) const
    {
        auto unit = unitData.find(type);
        const bool boat = unit != unitData.end() && unit->second.moveClass == 3;
        int n = 0;
        for (const Counter& c : counters)
        {
            if (c.kind != 0 || !c.players) continue;
            const bool isBoat = c.Travelling() ? c.naval : c.moveClass == 3;
            if (isBoat != boat) continue;
            if (c.Travelling() ? c.path.back() == sector : c.sector == sector) n += c.count;
        }
        return n + 1 < (boat ? maxBoats : maxUnits);
    }

    // 0x45FF00: the entry at the head is done with. One with repeats left (a count of 0 is none; 4, for ever, is
    // never counted down) starts again (0x461E60: its whole time) at the back, if the queue has room (32); one
    // without goes. True while there's something to go on with: not when the queue is empty, or a vehicle is next
    // and the sector hasn't room for it (an entry going round again always asks; otherwise only the local
    // player's factories do).
    bool WarMap::PopEntry(int ref, ObjectTail& f)
    {
        auto& q = f.queue.entries;
        if (q.empty()) return false;
        BuildEntry head = q.front();
        q.erase(q.begin());
        const int at = CounterOf(ref);
        const int sector = at >= 0 ? counters[size_t(at)].sector : table[size_t(ref)].Sector();
        const bool players = at >= 0 ? counters[size_t(at)].players : table[size_t(ref)].list % 2 == 0;
        auto isVehicle = [&](int type) { auto u = unitData.find(type); return u != unitData.end() && u->second.category == 1; };
        if (head.repeat != 0)
        {
            auto u = unitData.find(head.type);
            head.hours = u == unitData.end() ? 0.0f : float(ObjOjd::BuildHours(u->second, res.Human(), manufacture));
            if (q.size() < 32) q.push_back(head);
            return !(isVehicle(q.front().type) && !HasRoomFor(q.front().type, sector));
        }
        if (players && !q.empty() && isVehicle(q.front().type) && !HasRoomFor(q.front().type, sector)) return false;
        return !q.empty();
    }

    // The factory's 0x802D (0x45FC90), only while it's busy (+0x12C). The hours go further the better the
    // sector's supplied: they're divided by the product of its three ratios for the factory's side (0-2 Human,
    // 3-5 Martian; 1 with the cheat byte_58913C, not in). Then, while there's time and a queue, the entry at the
    // head takes it (0x461D60):
    //   - at 0 hours left it starts: its whole time (0x461E60), the type's build days (OBJ.ojd record 43, with
    //     the manufacture modifier) x 24, times its battle units for a vehicle
    //   - the object it's working on (a construction going up; not a vehicle) hears 0x80AD with the time, or
    //     0x80AC once it's done
    //   - done, its repeats count down (not 4, for ever) and what's left over goes to the next
    // An entry done: a vehicle is made in the sector (0x4620B0 -> 0x43CD10), UnitCompleted (event 2) is raised
    // for it if the factory is the local player's, and the entry forgets it; anything else raises
    // ConstructionCompleted (event 1) for the object it was working on, the same way. Then 0x45FF00; when that
    // says stop, the factory isn't busy any more (the world's 0x4192F0) and, for the local player's, raises
    // ConstructorIdle (event 9) if it's a unit or FacilityIdle (event 10) if not.
    void WarMap::Produce(int ref, int hours)
    {
        auto it = factories.find(ref);
        if (it == factories.end() || !it->second.queue.busy || hours <= 0) return;
        ObjectTail& f = it->second;
        int at = CounterOf(ref);
        const int sector = at >= 0 ? counters[size_t(at)].sector : table[size_t(ref)].Sector();
        const bool players = at >= 0 ? counters[size_t(at)].players : table[size_t(ref)].list % 2 == 0;
        const bool martian = players != res.Human();
        float m = 1.0f;
        if (sector >= 1 && sector <= warmap::SectorCount)
        {
            const auto& r = economy[size_t(sector)].ratio;
            const size_t k = martian ? 3 : 0;
            m = r[k] * r[k + 1] * r[k + 2];
        }
        float dt = (1.0f / m) * float(hours);
        bool changed = false;
        // (a type with no build time going round for ever would never use the time up; the game would hang)
        for (int guard = 0; dt != 0.0f && !f.queue.entries.empty() && guard < 1000; guard++)
        {
            changed = true;
            BuildEntry& e = f.queue.entries.front();
            auto u = unitData.find(e.type);
            const bool vehicle = u != unitData.end() && u->second.category == 1;
            if (e.hours == 0.0f)
                e.hours = u == unitData.end() ? 0.0f : float(ObjOjd::BuildHours(u->second, res.Human(), manufacture));
            const int target = vehicle ? -1 : RefOfId(e.target);
            const bool done = dt >= e.hours;
            if (target >= 0)
            {
                // 0x80AD / 0x80AC to the construction: how it takes them is still to be traced, so for now its
                // counter shows the share done and it's finished outright at the end (provisional)
                const int tk = CounterOf(target);
                const float whole = u == unitData.end() ? 0.0f : float(ObjOjd::BuildHours(u->second, res.Human(), manufacture));
                if (tk >= 0 && whole > 0) counters[size_t(tk)].progress = done ? 1.0f : 1.0f - (e.hours - dt) / whole;
            }
            if (done)
            {
                if (e.repeat <= 3) e.repeat--;
                dt -= e.hours;
                e.hours = 0;
            }
            else
            {
                e.hours -= dt;
                dt = 0;
            }
            if (e.hours != 0.0f) continue;
            if (vehicle)
            {
                const int k = AddUnit(e.type, sector, players);
                if (players && k >= 0) RaiseEvent(UnitCompleted, k);
                e.target = 0xDEADDEAD;
            }
            else
            {
                if (target >= 0)
                {
                    const int tk = CounterOf(target);
                    if (tk >= 0)
                    {
                        Counter& c = counters[size_t(tk)];
                        if (c.unfinished) { c.unfinished = false; c.finishedCount++; }
                        finished[size_t(c.sector)].push_back(c.type);
                    }
                    if (players) RaiseEvent(ConstructionCompleted, tk);
                }
                else if (players) RaiseEvent(ConstructionCompleted, -1);
            }
            at = CounterOf(ref); // AddUnit may have moved the counters about
            if (!PopEntry(ref, f))
            {
                if (f.queue.busy)
                {
                    f.queue.busy = false;
                    ShadeSectors();
                }
                if (players && at >= 0) RaiseEvent(counters[size_t(at)].kind == 0 ? ConstructorIdle : FacilityIdle, at);
            }
        }
        if (changed)
        {
            WriteTail(table[size_t(ref)], f);
            if (auto t = ReadTail(table[size_t(ref)])) f = *t;
        }
    }
}
