// The war map's factories (WBuildList 0x45F770-0x462EE0): their hourly progress and what comes of it. See
// docs/WARMAP.md "Production" and core/BuildList.h for the save format.
#include <SDL3/SDL_log.h>
#include <sstream>
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
    // whose an object is: its counter's side, or its list's (the H lists the player's)
    bool WarMap::RefPlayers(int ref) const
    {
        const int at = CounterOf(ref);
        if (at >= 0) return counters[size_t(at)].players;
        return ref >= 0 && size_t(ref) < table.size() ? table[size_t(ref)].list % 2 == 0 : true;
    }

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
            // (MODERN: in a civil war both sides' trees have every type, so the builder side's own tree decides;
            // in the game a type is in one tree only, which is the same thing)
            const bool players = at >= 0 ? counters[size_t(at)].players : table[size_t(ref)].list % 2 == 0;
            for (size_t t = 0; t < research.size(); t++)
                if (const auto& r = research[t]; r && (!IsCivilWar(campaign) || t == (players ? 0u : 1u)))
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
        return HasRoom(!(unit != unitData.end() && unit->second.moveClass == 3), sector, 1);
    }

    // 0x43AC40 (land, count): the same, for `count` more (0x43AC10 asks for 1; the battle map's reinforcement
    // points and Retreat ask too, 0x43D150 / 0x43D3F0)
    bool WarMap::HasRoom(bool land, int sector, int count) const
    {
        const bool boat = !land;
        int n = 0;
        for (const Counter& c : counters)
        {
            if (c.kind != 0 || !c.players) continue;
            const bool isBoat = c.Travelling() ? c.naval : c.moveClass == 3;
            if (isBoat != boat) continue;
            if (c.Travelling() ? c.path.back() == sector : c.sector == sector) n += c.count;
        }
        return n + count < (boat ? maxBoats : maxUnits);
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
            head.hours = u == unitData.end() ? 0.0f : float(ObjOjd::BuildHours(u->second, ModifierHuman(players), manufacture));
            if (q.size() < 32) q.push_back(head);
            return !(isVehicle(q.front().type) && !(players && unlimitedBuilding) && !HasRoomFor(q.front().type, sector));
        }
        if (players && !unlimitedBuilding && !q.empty() && isVehicle(q.front().type) && !HasRoomFor(q.front().type, sector)) return false;
        return !q.empty();
    }

    // The factory's 0x802D (0x45FC90), only while it's busy (+0x12C). The hours go further the better the
    // sector's supplied: they're divided by the product of its three ratios for the factory's side (0-2 Human,
    // 3-5 Martian; 1 with the cheat byte_58913C, youlikeit). Then, while there's time and a queue, the entry at the
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
        float m = 1.0f;
        if (sector >= 1 && sector <= warmap::SectorCount)
        {
            const auto& r = economy[size_t(sector)].ratio;
            const size_t k = size_t(SlotBase(players));     // (MODERN: the side's slots; its faction's in the game)
            m = r[k] * r[k + 1] * r[k + 2];
            if (resourceCheat) m = 1.0f;    // byte_58913C (0x4606D0)
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
                e.hours = u == unitData.end() ? 0.0f : float(ObjOjd::BuildHours(u->second, ModifierHuman(players), manufacture));
            const int target = vehicle ? -1 : RefOfId(e.target);
            const bool done = dt >= e.hours;
            if (target >= 0) // 0x80AD with the time, or 0x80AC once it's done
            {
                if (done) FinishConstruction(target);
                else ConstructionProgress(target, dt);
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
                if (players) RaiseEvent(ConstructionCompleted, target >= 0 ? CounterOf(target) : -1);
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

    // The construction's side (WMOB 0x42A4B0): 0x80AE, started, sets +0x78 to the whole time and +0x7C to 0; 0x80AD
    // adds the hours to +0x7C; the counter shows +0x7C / +0x78 done.
    void WarMap::StartConstruction(int ref, float hours)
    {
        if (ref < 0 || size_t(ref) >= table.size()) return;
        table[size_t(ref)].SetValue(0, hours);
        table[size_t(ref)].SetValue(1, 0.0f);
        if (int k = CounterOf(ref); k >= 0)
        {
            Counter& c = counters[size_t(k)];
            // (a shared counter, update 253: one more of its count unfinished; a counter of its own, unfinished)
            if (c.shared) c.finishedCount = std::max(0, c.finishedCount - 1);
            else { c.unfinished = true; c.progress = 0.0f; c.finishedCount = 0; }
        }
    }

    void WarMap::ConstructionProgress(int ref, float hours)
    {
        if (ref < 0 || size_t(ref) >= table.size()) return;
        SaveObject& o = table[size_t(ref)];
        o.SetValue(1, o.Value(1) + hours);
        if (int k = CounterOf(ref); k >= 0 && o.Value(0) != 0.0f) counters[size_t(k)].progress = o.Value(1) / o.Value(0);
    }

    // 0x80AC (0x42A980): finished, its first battle unit's health (+0xC4) its type's whole (0x47DA20), +0x78 and
    // +0x7C both 1.0. (A construction goes onto a battle map with its progress's share of its health, at least 1,
    // 0x48E9D0, which the battle hands back; without this a facility placed on the battle map finished at that
    // and, short of its whole health, made nothing: update 255, a tester's report that new resource buildings
    // didn't add to the resources.) A command building (58, 278 in sector 30; 120, 830 in
    // sector 3) has the world think again about the sectors (0x418890: here the paths are built again) and, if
    // it's the local player's, raises CommandCentreBuilt (event 15) for the sector; anything else is laid out
    // again in its sector (0x4297C0, and 0x8145 to the view when it shows its kind).
    void WarMap::FinishConstruction(int ref)
    {
        if (ref < 0 || size_t(ref) >= table.size()) return;
        SaveObject& o = table[size_t(ref)];
        const int k = CounterOf(ref);
        const int sector = k >= 0 ? counters[size_t(k)].sector : o.Sector();
        const bool players = k >= 0 ? counters[size_t(k)].players : o.list % 2 == 0;
        if (!o.BattleHealths().empty())
            if (const int whole = UnitMaxHealth(o.Type(), players); whole > 0) o.SetBattleHealth(0, whole);
        o.SetValue(0, 1.0f);
        o.SetValue(1, 1.0f);
        if (k >= 0)
        {
            Counter& c = counters[size_t(k)];
            if (c.shared) c.finishedCount = std::min(c.count, c.finishedCount + 1);
            else if (c.unfinished) { c.unfinished = false; c.finishedCount = std::min(c.count, c.finishedCount + 1); }
            c.progress = 1.0f;
        }
        const int type = o.Type();
        const bool post = type == (sector == 3 ? 830 : 120) || type == (sector == 30 ? 278 : 58);
        if (post)
        {
            pathsBuilt = false;
            if (players) RaiseEvent(CommandCentreBuilt, sector);
        }
        else if (sector >= 1 && sector <= warmap::SectorCount) RefreshSector(sector);
    }

    // 0x4603B0, a type added (the Build List window's units, command 0x17): one a sector hasn't room for isn't (a
    // boat for the other side's factories, any vehicle for the local player's); a vehicle the same as the last
    // queued just has its repeats counted up (0x461EE0: 1, 2, 3, then 4 for ever); otherwise a new entry at the
    // end (0x4604B0, 0x461B60: one repeat, its whole time, nothing it's working on), not past 32, and the factory
    // made busy if it wasn't (the world's 0x4192F0).
    bool WarMap::AddType(int ref, int type)
    {
        auto it = factories.find(ref);
        if (it == factories.end()) return false;
        ObjectTail& f = it->second;
        const int at = CounterOf(ref);
        const int sector = at >= 0 ? counters[size_t(at)].sector : table[size_t(ref)].Sector();
        const bool players = at >= 0 ? counters[size_t(at)].players : table[size_t(ref)].list % 2 == 0;
        auto u = unitData.find(type);
        if (u == unitData.end()) return false;
        if (!players && u->second.moveClass == 3 && !HasRoomFor(type, sector)) return false;
        auto& q = f.queue.entries;
        if (u->second.category == 1)
        {
            if (players && !unlimitedBuilding && !HasRoomFor(type, sector)) return false;
            if (!q.empty() && q.back().type == type)
            {
                BuildEntry& e = q.back();
                if (e.repeat < 3) e.repeat++;
                else if (e.repeat == 3) e.repeat = 4;
                SaveFactory(ref);
                return true;
            }
        }
        if (q.size() >= 32) return false;
        BuildEntry e;
        e.type = type;
        e.repeat = 1;
        e.hours = float(ObjOjd::BuildHours(u->second, ModifierHuman(RefPlayers(ref)), manufacture));
        q.push_back(e);
        if (!f.queue.busy) { f.queue.busy = true; ShadeSectors(); }
        SaveFactory(ref);
        return true;
    }

    // 0x460240, a construction ordered from the war map (command 0x16, its placement all 0: the war map has no
    // battle map to place it on): added as above, then (0x461C80) the entry is set up again from the order and,
    // for anything but a vehicle, the construction made in the sector at once (0x4620B0 -> 0x43CD10) and told
    // 0x80AE with its whole time. Where its command building is to go and the order has no place, the game finds
    // one on the battle map (0x4411B0, not in); then 0x440EA0 for the sector (not traced).
    bool WarMap::OrderConstruction(int ref, int type, int* made)
    {
        if (made) *made = -1;
        if (!AddType(ref, type)) return false;
        auto u = unitData.find(type);
        if (u == unitData.end() || u->second.category == 1) return true;
        ObjectTail& f = factories[ref];
        BuildEntry& e = f.queue.entries.back();
        const int at = CounterOf(ref);
        const int sector = at >= 0 ? counters[size_t(at)].sector : table[size_t(ref)].Sector();
        const bool players = at >= 0 ? counters[size_t(at)].players : table[size_t(ref)].list % 2 == 0;
        const int k = AddObject(type, sector, players, u->second.category == 3 ? 4 : 2);
        const int target = k >= 0 && !counters[size_t(k)].refs.empty() ? counters[size_t(k)].refs.front() : -1;
        if (target >= 0)
        {
            e.target = uint32_t(table[size_t(target)].Id());
            StartConstruction(target, e.hours);
            if (made) *made = target;
            if (!players) AiPlaceAtSite(target); // 0x43CD10 -> 0x441420
        }
        else if (k >= 0) { counters[size_t(k)].unfinished = true; counters[size_t(k)].progress = 0.0f; }
        SaveFactory(ref);
        return true;
    }

    // 0x45FE60: on (with something queued, and room for a vehicle at the head) makes it busy; off stops it
    void WarMap::SetBusy(int ref, bool on)
    {
        auto it = factories.find(ref);
        if (it == factories.end()) return;
        ObjectTail& f = it->second;
        if (!on)
        {
            if (f.queue.busy) { f.queue.busy = false; ShadeSectors(); }
        }
        else if (!f.queue.entries.empty() && !f.queue.busy)
        {
            const int type = f.queue.entries.front().type;
            auto u = unitData.find(type);
            const int at = CounterOf(ref);
            const int sector = at >= 0 ? counters[size_t(at)].sector : table[size_t(ref)].Sector();
            const bool players = at >= 0 ? counters[size_t(at)].players : table[size_t(ref)].list % 2 == 0;
            if (u != unitData.end() && u->second.category == 1 && !(players && unlimitedBuilding) && !HasRoomFor(type, sector)) return;
            f.queue.busy = true;
            ShadeSectors();
        }
        SaveFactory(ref);
    }

    // 0x460080: an entry taken out; with nothing left, the factory isn't busy any more
    void WarMap::RemoveEntry(int ref, int index)
    {
        auto it = factories.find(ref);
        if (it == factories.end()) return;
        auto& q = it->second.queue.entries;
        if (index < 0 || size_t(index) >= q.size()) return;
        q.erase(q.begin() + index);
        if (q.empty() && it->second.queue.busy) { it->second.queue.busy = false; ShadeSectors(); }
        SaveFactory(ref);
    }

    // 0x460140 (0x813D to a war map object, 0x428F75): for a vehicle (its type's +0x10, 0x440700, 1) each entry of
    // a copy of its queue cancelled (0x462120: a construction being worked on goes from the sector, then the entry),
    // then the window refreshed
    void WarMap::CancelAll(int ref)
    {
        auto it = factories.find(ref);
        if (it == factories.end()) return;
        const int k = CounterOf(ref);
        if (k < 0) return;
        auto unit = unitData.find(counters[size_t(k)].type);
        if (unit == unitData.end() || unit->second.category != 1) return;
        while (!factories[ref].queue.entries.empty())
        {
            const uint32_t target = factories[ref].queue.entries.front().target;
            if (target != 0xDEADDEAD)
            {
                const int t = RefOfId(target);
                if (t >= 0)
                    if (int c = CounterOf(t); c >= 0) RemoveObject(c, t);
            }
            if (factories.find(ref) == factories.end()) break;
            RemoveEntry(ref, 0);
        }
    }

    // 0x4605A0, "promote" (a queue slot's 0x2FC button): the entry to the front, the factory made busy
    void WarMap::PromoteEntry(int ref, int index)
    {
        auto it = factories.find(ref);
        if (it == factories.end()) return;
        auto& q = it->second.queue.entries;
        if (index <= 0 || size_t(index) >= q.size()) return;
        BuildEntry e = q[size_t(index)];
        q.erase(q.begin() + index);
        q.insert(q.begin(), e);
        if (!it->second.queue.busy) { it->second.queue.busy = true; ShadeSectors(); }
        SaveFactory(ref);
    }

    // A queue slot clicked (0x4625B0, either button): a repeat less (0x461EB0: for ever goes to 3); at none, an
    // entry working on a construction is cancelled (0x462120: on the war map the construction goes from the sector
    // and the entry with it; on the battle map the object hears 0x80A0), any other just goes.
    int WarMap::SlotClicked(int ref, int index)
    {
        auto it = factories.find(ref);
        if (it == factories.end()) return -1;
        auto& q = it->second.queue.entries;
        if (index < 0 || size_t(index) >= q.size()) return -1;
        BuildEntry& e = q[size_t(index)];
        if (e.repeat == 4) e.repeat = 3;
        else if (e.repeat >= 1) e.repeat--;
        if (e.repeat != 0) { SaveFactory(ref); return -1; }
        int kill = -1;
        if (e.target != 0xDEADDEAD)
        {
            // 0x462120 (+0x20 at 0x462159): on the war map the sector takes it out (its vt+0x2C); on the battle map
            // the construction is sent 0x80A0, so it dies there and goes with the battle's losses
            const int target = RefOfId(e.target);
            if (target >= 0)
            {
                if (buildWindow && buildWindow->battle) kill = target;
                else if (int k = CounterOf(target); k >= 0) RemoveObject(k, target);
            }
        }
        RemoveEntry(ref, index);
        return kill;
    }

    // ---- Building on the battle map (battlemap/BuildHost.h) ----

    BuildHost::Factory WarMap::FactoryOf(int ref) const
    {
        Factory f;
        auto it = factories.find(ref);
        if (it == factories.end()) return f;
        f.exists = true;
        f.queued = int(it->second.queue.entries.size());
        f.busy = it->second.queue.busy;
        return f;
    }

    // 0x47DF74: 0x45FE60 with the opposite of 0x45FEF0 (busy)
    void WarMap::ToggleSuspend(int ref)
    {
        auto it = factories.find(ref);
        if (it != factories.end()) SetBusy(ref, !it->second.queue.busy);
    }

    void WarMap::OpenBattleWindow(int ref, int height)
    {
        if (factories.find(ref) == factories.end()) return;
        buildWindow = BuildWindowState{ ref };
        buildWindow->battle = true;
        buildWindow->height = std::max(480, height);
    }

    // Packet 0x16 from the placement (0x4A1460 / 0x4A0490 -> 0x80A4 -> 0x460EC1 -> 0x4601C0) to 0x460240: the entry
    // (0x4603B0), its place's four values (0x461C80: +0x1C-+0x28; a building's third and fourth are whatever was on
    // the stack in the original, kept 0 here), the construction made in the sector at once (0x4620B0 -> 0x43CD10)
    // and put where it was placed (0x80A7, its second point 0x80A6): the object's battle position, and a fence's
    // two ends in its four values after BATP (as it saves them, 0x498890), which the battle lays its posts from.
    std::optional<BattleGroup> WarMap::PlaceConstruction(int type, int x0, int y0, int x1, int y1)
    {
        if (!buildWindow) return std::nullopt;
        const int ref = buildWindow->ref;
        if (!ItemEnabled(ref, type)) return std::nullopt;
        int made = -1;
        if (!OrderConstruction(ref, type, &made) || made < 0) return std::nullopt;
        auto& q = factories[ref].queue.entries;
        if (!q.empty()) q.back().place = { x0, y0, x1, y1 };
        SaveFactory(ref);
        table[size_t(made)].SetBattlePosition(float(x0), float(y0));
        const int line[4] = { x0, y0, x1, y1 };
        table[size_t(made)].SetLinePoints(line);       // (a fence's ends; nothing for anything else)
        const int at = CounterOf(made);
        return GroupOf(made, at >= 0 ? counters[size_t(at)].previous : 0);
    }

    float WarMap::Progress(int ref) const
    {
        if (ref < 0 || size_t(ref) >= table.size()) return 1.0f;
        const SaveObject& o = table[size_t(ref)];
        if (o.list >= 2 && o.Value(0) != 0.0f) return o.Value(1) / o.Value(0);
        return 1.0f;
    }

    // 0x462DC0: an item in the window can be ordered when the sector allows another of it (0x43C7F0), it isn't
    // one only the battle map can make (a constructor vehicle's, on the war map), its prerequisite building is
    // there finished (0x442420) and the queue isn't full. 0x43C7F0: the type's faction (+0x0C) must be the
    // sector's owner's, and the side's objects of its kind there (for a vehicle, those there and those queued
    // in the sector's factories, 0x421C60) fewer than its "max per sector" (record 52, default 20).
    bool WarMap::ItemEnabled(int ref, int type, bool* battleMapOnly) const
    {
        auto it = factories.find(ref);
        auto u = unitData.find(type);
        if (it == factories.end() || u == unitData.end()) return false;
        const int at = CounterOf(ref);
        const int sector = at >= 0 ? counters[size_t(at)].sector : table[size_t(ref)].Sector();
        // (0x462770 at 0x46289C: only with the factory's +0x20 set, the war map's window; from the battle map the
        // constructor's items can be ordered)
        const bool mapOnly = at >= 0 && counters[size_t(at)].kind == 0 && !(buildWindow && buildWindow->battle && buildWindow->ref == ref);
        if (battleMapOnly) *battleMapOnly = mapOnly;
        if (mapOnly) return false;
        const int who = owner[size_t(sector)];
        if (who == 2) return false;
        const int side = (who == 0 ? res.Human() : OtherIsHuman()) ? 0 : 1;   // the holder's faction (MODERN)
        if (u->second.faction != side) return false;
        const int kind = u->second.category - 1;
        int n = 0;
        if (kind >= 0 && kind <= 2)
            for (const Counter& c : counters)
                if (c.players && !c.Travelling() && c.sector == sector && c.kind == kind && c.type == type) n += c.count;
        if (kind == 0)
            for (const auto& [r, f] : factories)
            {
                const int fk = CounterOf(r);
                if (fk < 0 || counters[size_t(fk)].sector != sector || !counters[size_t(fk)].players) continue;
                for (const BuildEntry& e : f.queue.entries) if (e.type == type) n++;
            }
        // (MODERN, "Unlimited building": no limit but the command buildings' one, 58 and 120, whose OBJ.ojd limit
        // is 1: the war map takes a sector's command building as its only one, 0x80AC)
        if (n >= u->second.maxPerSector && !(unlimitedBuilding && u->second.maxPerSector > 1)) return false;
        if (u->second.prerequisite)
        {
            const std::vector<int> here = Finished(sector);
            if (std::find(here.begin(), here.end(), u->second.prerequisite) == here.end()) return false;
        }
        return it->second.queue.entries.size() < 32;
    }

    void WarMap::SaveFactory(int ref)
    {
        auto it = factories.find(ref);
        if (it == factories.end()) return;
        WriteTail(table[size_t(ref)], it->second);
        if (auto t = ReadTail(table[size_t(ref)])) it->second = *t;
    }
}

namespace wow
{
    const std::vector<WarMap::BrokenBridge>& WarMap::BrokenBridges(int sector) const
    {
        static const std::vector<BrokenBridge> none;
        auto it = brokenBridges.find(sector);
        return it == brokenBridges.end() || !bridgesStayBroken ? none : it->second;
    }

    void WarMap::SetBrokenBridges(int sector, const std::vector<std::pair<int, int>>& places)
    {
        if (!bridgesStayBroken) return;
        std::vector<BrokenBridge> now;
        const std::vector<BrokenBridge>& before = brokenBridges[sector];
        for (const auto& [x, y] : places)
        {
            BrokenBridge b{ x, y, 0 };
            for (const BrokenBridge& o : before) if (o.x == x && o.y == y) b.hours = o.hours;
            now.push_back(b);
        }
        if (now.empty()) brokenBridges.erase(sector);
        else brokenBridges[sector] = std::move(now);
    }

    // Each hour of a sector: every construction vehicle there (a unit with a factory, either side's) with nothing
    // queued puts its hours into the sector's first broken bridge; at BridgeRebuildHours it's whole again (the next
    // battle builds it). After everything a vehicle can build: a queue with anything in it does that instead.
    void WarMap::RebuildBridges(int sector, int hours)
    {
        if (!bridgesStayBroken || hours <= 0) return;
        auto it = brokenBridges.find(sector);
        if (it == brokenBridges.end() || it->second.empty()) return;
        int workers = 0;
        for (const auto& [ref, f] : factories)
        {
            const int at = CounterOf(ref);
            if (at < 0 || counters[size_t(at)].kind != 0 || counters[size_t(at)].sector != sector || counters[size_t(at)].Travelling()) continue;
            if (f.queue.entries.empty()) workers++;
        }
        if (!workers) return;
        BrokenBridge& b = it->second.front();
        b.hours += hours * workers;
        if (b.hours >= BridgeRebuildHours)
        {
            SDL_Log("Bridge at %d,%d in sector %d rebuilt", b.x, b.y, sector);
            it->second.erase(it->second.begin());
            if (it->second.empty()) brokenBridges.erase(it);
        }
    }

    std::string WarMap::BrokenBridgesText() const
    {
        std::string out;
        for (const auto& [sector, list] : brokenBridges)
            for (const BrokenBridge& b : list)
                out += std::to_string(sector) + " " + std::to_string(b.x) + " " + std::to_string(b.y) + " " + std::to_string(b.hours) + "\n";
        return out;
    }

    void WarMap::ReadBrokenBridges(const std::string& text)
    {
        brokenBridges.clear();
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream words(line);
            int sector = 0;
            BrokenBridge b;
            if (words >> sector >> b.x >> b.y >> b.hours && sector >= 1 && sector <= warmap::SectorCount) brokenBridges[sector].push_back(b);
        }
    }
}
