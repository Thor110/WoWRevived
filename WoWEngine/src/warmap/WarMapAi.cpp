// The computer player's turn on the war map (the AI player 0x442890, its handler 0x442AC0; docs/WARMAP.md "The
// computer player"). Each game hour the world sends it 0x802D (0x419EC9, after the local player and before both
// sides' research), and it takes one step of a ten-phase cycle (0x442EC0):
//   0  gather its combat units, strongest first (0x443030)       -> 2 if it has none, else 1
//   1  one unit at a time, stay or step into a neighbour (0x443340) -> 2 when all have had their turn
//   2  its constructors and the sectors' economy (0x443610)        -> 4 with no constructor, else 3
//   3  a constructor sent to build where it's needed (0x443CA0)    -> 4 when done
//   4  the front line and its defence builders (0x4440B0)          -> 6 with none, else 5
//   5  a defence builder sent to the front (0x4443D0)             -> 6 when done
//   6  an invasion, if it's strong enough (0x444690)               -> 7
//   7  -> 8;  8  its factories told what to make (0x444C10) -> 9;  9 -> 0
#include "warmap/WarMap.h"

#include "core/Random.h"

#include <SDL3/SDL_log.h>

#include <cmath>

namespace wow
{
    // "AI.ojd" strength of one object (0x441E70): its battle map units (the group at +0x4C, its +0x24) times its
    // type's weight. A saved object's units are its SELO ids after its own; one with no saved bytes counts its type's
    // battle units.
    float WarMap::ObjectStrength(int counter) const
    {
        const Counter& c = counters[size_t(counter)];
        auto w = aiWeights.find(c.type);
        const int weight = w == aiWeights.end() ? 0 : w->second;
        int units = 0;
        for (int ref : c.refs) units += std::max(0, int(table[size_t(ref)].IdOffsets().size()) - 1);
        if (c.refs.empty())
        {
            auto u = unitData.find(c.type);
            units = (u == unitData.end() ? 1 : u->second.battleUnits) * std::max(1, c.count);
        }
        return float(units * weight);
    }

    int WarMap::AiClass(int type) const
    {
        auto it = objectClasses.find(type);
        return it == objectClasses.end() ? 0 : it->second;
    }

    // The objects in a sector's lists (the sector's +0x4C-0x58: units and constructions, each side's), in the
    // order the engine keeps them (*provisional*: the game's lists keep the order objects came in)
    std::vector<int> WarMap::SectorObjects(int sector, bool players, int kind) const
    {
        std::vector<int> out;
        for (size_t i = 0; i < counters.size(); i++)
        {
            const Counter& c = counters[i];
            if (c.sector == sector && c.players == players && c.kind == kind && !c.Travelling() && c.count > 0) out.push_back(int(i));
        }
        return out;
    }

    std::vector<BattleGroup> WarMap::BattleGroups(int sector) const
    {
        std::vector<BattleGroup> out;
        for (int list = 0; list < 6; list++)
            for (int c : SectorObjects(sector, (list & 1) == 0, list / 2))
            {
                const Counter& k = counters[size_t(c)];
                for (int ref : k.refs)
                {
                    if (ref < 0 || size_t(ref) >= table.size()) continue;
                    if (std::find(destroyed.begin(), destroyed.end(), ref) != destroyed.end()) continue;
                    const SaveObject& o = table[size_t(ref)];
                    BattleGroup g;
                    g.type = o.Type();
                    g.list = list;
                    g.previous = k.previous;
                    g.ref = ref;
                    if (auto w = aiWeights.find(g.type); w != aiWeights.end()) g.aiWeight = w->second;
                    g.aiClass = AiClass(g.type);
                    g.places = o.BattlePositions();
                    if (list >= 2 && o.Value(0) != 0.0f) g.progress = o.Value(1) / o.Value(0);
                    out.push_back(std::move(g));
                }
            }
        return out;
    }

    int WarMap::EnemyObjects(int sector) const
    {
        int n = 0;
        for (int kind = 0; kind < 2; kind++)
            for (int c : SectorObjects(sector, false, kind)) n += counters[size_t(c)].count;
        return n;
    }

    // 0x4356C0: b is among a's neighbours (0x4BD260) by land (the link table's bits 0-2)
    static bool Adjacent(int a, int b)
    {
        for (const int* n = warmap::Neighbours(a); *n != -1; n++)
            if (*n == b) return (warmap::RouteLinkKind(a, b) & 7) != 0;
        return false;
    }

    // 0x441AD0 / 0x441B80 / 0x441C50: a sector's neighbours, 1 to 30: all, the AI's, the other side's
    std::vector<int> WarMap::AiNeighbours(int sector, int which) const
    {
        std::vector<int> out;
        for (int n = 1; n <= warmap::SectorCount; n++)
        {
            if (n == sector || !Adjacent(sector, n)) continue;
            const int o = owner[size_t(n)];
            if (which == 0 || (which == 1 && o == 1) || (which == 2 && o == 0)) out.push_back(n);
        }
        return out;
    }

    // 0x4419E0: a boat (base type 19, Ironclad, or 22, Submersible) only into a sector that isn't landlocked
    // (byte_4BD2E0) with room (0x43AC10); anything else anywhere
    bool WarMap::AiCanEnter(int sector, int type) const
    {
        auto u = unitData.find(type);
        const int base = u == unitData.end() ? type : u->second.baseType;
        if (base != 0x13 && base != 0x16) return true;
        return !warmap::Landlocked(sector) && HasRoomFor(type, sector);
    }

    // 0x441F00: the AI's combat units (class 1) in a sector, those that could go into `into` if it's given
    float WarMap::AiForce(int sector, int into) const
    {
        float sum = 0;
        for (int c : SectorObjects(sector, false, 0))
        {
            if (AiClass(counters[size_t(c)].type) != 1) continue;
            if (into && !AiCanEnter(into, counters[size_t(c)].type)) continue;
            sum += ObjectStrength(c);
        }
        return sum;
    }

    // 0x442080: what the AI would meet in a sector: the other side's combat units (class 1) and defences (class 7)
    // there, and the combat units of every route from or to it (whoever's)
    float WarMap::AiEnemyForce(int sector) const
    {
        float sum = 0;
        for (int c : SectorObjects(sector, true, 0)) if (AiClass(counters[size_t(c)].type) == 1) sum += ObjectStrength(c);
        for (int c : SectorObjects(sector, true, 1)) if (AiClass(counters[size_t(c)].type) == 7) sum += ObjectStrength(c);
        for (size_t i = 0; i < counters.size(); i++)
        {
            const Counter& c = counters[i];
            if (!c.Travelling() || c.count <= 0 || AiClass(c.type) != 1) continue;
            if (c.path.back() == sector) sum += ObjectStrength(int(i));
            if (c.path.front() == sector) sum += ObjectStrength(int(i));
        }
        return sum;
    }

    // 0xAD to an object (0x429E80): out of its sector and straight into the other (0x429DB0), no route; both are
    // held by whoever's left in them (0x43CA40) and the map's shading is done again (0x4192F0)
    void WarMap::AiMove(int counter, int sector)
    {
        Counter& k = counters[size_t(counter)];
        const int from = k.sector;
        if (from == sector) return;
        SDL_Log("War map AI: type %d from sector %d to %d", k.type, from, sector);
        FreeCounter(counter);
        k.previous = from;  // 0x4219DC
        k.sector = sector;
        k.moved = true;
        RecomputeOwner(from);
        RecomputeOwner(sector);
        ShadeSectors();
        RefreshSector(from);
        RefreshSector(sector);
    }

    void WarMap::AiTurn(int hours)
    {
        ai.clock += float(hours);
        // its research (+0x454): an item done or made obsolete is let go; with none, one is picked (vtable +0x2C:
        // the side's plan, 0x45A400 / 0x45B4E0, falling back to 0x458A60)
        if (Research* r = research[1].get())
        {
            if (ai.researchItem >= 0)
            {
                const auto& item = r->Items()[size_t(ai.researchItem)];
                if (item.state == Research::Done || item.state == Research::Obsolete) ai.researchItem = -1;
            }
            else
            {
                ai.researchItem = r->ChooseFromPlan();
                if (ai.researchItem >= 0) SDL_Log("War map AI: researching %s", r->Items()[size_t(ai.researchItem)].name.c_str());
            }
        }
        switch (ai.phase)
        {
        case 0: ai.phase += AiGather() ? 2 : 1; break;
        case 1:
            if (ai.cursor < ai.groups.size()) AiMoveGroup(ai.groups[ai.cursor]);
            ai.cursor++;
            if (ai.cursor >= ai.groups.size()) ai.phase++;
            break;
        case 2: ai.phase += AiEconomy() ? 2 : 1; break;
        case 3: if (AiBuilder()) ai.phase++; break;
        case 4: ai.phase += AiFrontline() ? 2 : 1; break;
        case 5: if (AiDefender()) ai.phase++; break;
        case 6: AiInvade(); ai.phase++; break;
        case 7: ai.phase = 8; break;
        case 8: AiProduce(); ai.phase++; break;
        default: ai.phase = 0; break;
        }
    }

    // Phase 0 (0x443030): every combat unit (class 1) the AI has in sectors 1-30, with its strength, into a binary
    // tree rooted at the one nearest half-way between the weakest and the strongest (swapped to the front), each
    // other going to the stronger side if it's stronger than a node, else the other; read out stronger side first,
    // that's the order the units move in. True when there are none (phase 1 is skipped).
    bool WarMap::AiGather()
    {
        struct Node { float s; int counter; int stronger = -1, weaker = -1; };
        std::vector<Node> nodes;
        float most = 0.0f, least = 100000.0f;
        ai.total = 0;
        for (int s = 1; s <= warmap::SectorCount; s++)
            for (int c : SectorObjects(s, false, 0))
            {
                if (AiClass(counters[size_t(c)].type) != 1) continue;
                const float st = ObjectStrength(c);
                nodes.push_back({ st, c });
                ai.total += st;
                if (st > most) most = st;
                if (st < least) least = st;
            }
        if (nodes.empty()) return ai.cursor == ai.groups.size();
        const float pivot = float((double(most) - double(least)) * 0.5 + double(least));
        float best = 100000.0f;
        int root = -1;
        for (size_t i = 0; i < nodes.size(); i++)
        {
            const float d = std::fabs(nodes[i].s - pivot);
            if (d < best) { best = d; root = int(i); }
        }
        if (root > 0) std::swap(nodes[0], nodes[size_t(root)]);
        for (size_t i = 1; i < nodes.size(); i++)
        {
            int at = 0;
            for (;;)
            {
                int& next = nodes[i].s > nodes[size_t(at)].s ? nodes[size_t(at)].stronger : nodes[size_t(at)].weaker;
                if (next < 0) { next = int(i); break; }
                at = next;
            }
        }
        ai.groups.clear();
        std::vector<int> stack;
        int at = 0;
        while (at >= 0 || !stack.empty())
        {
            while (at >= 0) { stack.push_back(at); at = nodes[size_t(at)].stronger; }
            at = stack.back();
            stack.pop_back();
            ai.groups.push_back(nodes[size_t(at)].counter);
            at = nodes[size_t(at)].weaker;
        }
        ai.cursor = 0;
        return ai.groups.empty();
    }

    // Phase 1 (0x443340), one unit: staying is worth what its sector has without it, F(here) - S, times 1 + a
    // random [Tweak] "AI Movement Temperature" (0.25) share; twice that ("AI safe sector repeller Mul") when every
    // neighbour is the AI's, and 0.8 of it in sector 3 or 30. Each neighbour of its own it can go into scores that
    // less F(there) / (its enemy neighbours x "AI Frontline Attractor" (2.0) + 1) x "AI Move Threshold" (1.05);
    // the best above 0 is where it goes (+0x58 the sector it leaves).
    bool WarMap::AiMoveGroup(int counter)
    {
        if (counter < 0 || size_t(counter) >= counters.size()) return false;
        const Counter& k = counters[size_t(counter)];
        if (k.count <= 0 || k.Travelling() || k.players) return false;
        const int here = k.sector;
        float stay = AiForce(here, 0) - ObjectStrength(counter);
        const int r = random::Rand() & 0x7FFF;
        stay = float((double(r) * double(aiTweaks.temperature) * double(3.0517578e-5f) + 1.0) * double(stay));
        const std::vector<int> all = AiNeighbours(here, 0), mine = AiNeighbours(here, 1);
        if (all.size() == mine.size()) stay *= aiTweaks.safeRepeller;
        if (here == 3 || here == 30) stay = float(double(stay) * 0.8);
        float best = 0.0f;
        int pick = -1;
        for (size_t i = 0; i < mine.size(); i++)
        {
            float score = 0.0f;
            if (AiCanEnter(mine[i], k.type))
            {
                const double enemies = double(AiNeighbours(mine[i], 2).size());
                const double there = AiForce(mine[i], 0);
                score = float(double(stay) - there / (enemies * double(aiTweaks.frontlineAttractor) + 1.0) * double(aiTweaks.moveThreshold));
            }
            if (best < score) { best = score; pick = int(i); }
        }
        if (pick < 0) return false;
        AiMove(counter, mine[size_t(pick)]);
        return true;
    }

    // Phase 6 (0x444690): only when the AI's sectors average at least [Tweak] "AI Invasion Threshold PC" (150) of
    // combat strength. For each sector not its own: f, what its neighbours could send in; E, what's there
    // (0x442080). It's a candidate when f (1 - "AI Retain Force" 0.2) / (the sector's cool-down + 1) is more than
    // (rand() % 100 + 50) x 0.01 x E x "AI Outnumber Force" (1.1), which is then the force to send; it scores its
    // neighbours of the AI's x "AI Invader NFriend Mul" (3000) plus what its resources are worth to the AI (each
    // kind's (spread demand - spread supply + "AI Build Resfac Eagerness" 50) x its gain, at least 0). The best
    // is invaded (0x444990) and its cool-down set to rand() % 8 + 10 days.
    void WarMap::AiInvade()
    {
        double sum = 0;
        int held = 0;
        for (int s = 1; s <= warmap::SectorCount; s++)
            if (owner[size_t(s)] == 1) { sum = float(sum + AiForce(s, 0)); held++; }
        float average = float(sum);
        if (held) average = float(sum / double(held));
        if (average < aiTweaks.invasionThreshold) return;
        const int base = res.Human() ? 3 : 0; // the AI's resource kinds
        float bestScore = 0.0f, bestNeed = 0.0f;
        int bestSector = -1;
        for (int t = 1; t <= warmap::SectorCount; t++)
        {
            if (owner[size_t(t)] == 1) continue;
            const std::vector<int> mine = AiNeighbours(t, 1);
            float f = 0;
            for (int n : mine) f += AiForce(n, t);
            const double enemy = AiEnemyForce(t);
            const float need0 = float(enemy * double(aiTweaks.outnumberForce));
            const float spare = float(double(f) - double(f) * double(aiTweaks.retainForce));
            const int r = random::Rand() % 100 + 50;
            const int cool = aiCooldown[size_t(t)] + 1;
            const float need = float(double(r) * double(0.0099999998f) * double(need0));
            if (!(double(spare) / double(cool) > double(need))) continue;
            const SectorEconomy& e = economy[size_t(t)];
            double value = 0;
            for (int k : { 2, 0, 1 })
                value += (double(e.effDemand[size_t(base + k)]) - double(e.effSupply[size_t(base + k)]) + double(aiTweaks.resfacEagerness))
                         * double(ResourceGain(t, base + k));
            if (value < 0) value = 0;
            const float score = float(double(mine.size()) * double(aiTweaks.invaderNFriendMul) + value);
            if (bestScore < score) { bestScore = score; bestNeed = need; bestSector = t; }
        }
        if (bestSector < 0) return;
        SDL_Log("War map AI: invading sector %d (score %.1f, force wanted %.1f)", bestSector, double(bestScore), double(bestNeed));
        AiLaunch(bestSector, bestNeed);
        aiCooldown[size_t(bestSector)] = (random::Rand() & 7) + 10;
    }

    // 0x444990: the combat units it can send from its neighbouring sectors with a command building (0x441A80),
    // into a list; all of them if they fall short of the force wanted (plus 1), otherwise shuffled (4 x their
    // count random swaps) and taken from the front until they make it up
    void WarMap::AiLaunch(int target, float need)
    {
        need = float(double(need) + 1.0);
        std::vector<int> units;
        float total = 0;
        for (int n : AiNeighbours(target, 0))
        {
            if (owner[size_t(n)] != 1 || !HasCommandPost(n)) continue;
            for (int c : SectorObjects(n, false, 0))
                if (AiClass(counters[size_t(c)].type) == 1 && AiCanEnter(target, counters[size_t(c)].type))
                {
                    units.push_back(c);
                    total += ObjectStrength(c);
                }
        }
        if (total == 0.0f) return;
        if (total < need) need = total;
        else
            for (size_t i = 0; i < units.size() * 4; i++)
            {
                const int a = random::Rand() % int(units.size());
                const int b = random::Rand() % int(units.size());
                std::swap(units[size_t(a)], units[size_t(b)]);
            }
        for (size_t i = 0; need > 0.0f && i < units.size(); i++)
        {
            need -= ObjectStrength(units[i]);
            AiMove(units[i], target);
        }
    }

    // Phase 8 (0x444C10): each of its finished factories (class 4) with nothing queued gets one thing to make from
    // what it can (0x45F970, each swapped for the type it gives way to when that's offered too, 0x45F900): the
    // first constructor (class 2) none of its kind is idle (not working, 0x42A410) or queued for in the sectors
    // the factory's sector has a path to (or itself); failing that a combat unit (class 1) at random. Added with
    // 0x4603B0.
    void WarMap::AiProduce()
    {
        std::vector<int> refs;
        for (int s = 1; s <= warmap::SectorCount; s++)
            for (int c : SectorObjects(s, false, 1))
                if (AiClass(counters[size_t(c)].type) == 4)
                    for (int ref : counters[size_t(c)].refs) refs.push_back(ref);
        for (int ref : refs)
        {
            std::vector<int> options = BuildOptions(ref);
            for (size_t i = 0; i < options.size(); i++) // 0x45F900
            {
                int to = 0;
                for (size_t j = 0; j < options.size(); j++)
                {
                    if (j == i) continue;
                    auto u = unitData.find(options[i]);
                    if (u != unitData.end() && u->second.upgrade == options[j]) to = options[j];
                }
                if (to) options[i] = to;
            }
            const SaveObject& o = table[size_t(ref)];
            const bool finishedHere = !o.Kind2() || o.Value(1) >= o.Value(0);
            if (!finishedHere || Working(ref)) continue;
            const int at = CounterOf(ref);
            const int sector = at >= 0 ? counters[size_t(at)].sector : o.Sector();
            int choice = 0;
            for (int type : options)
            {
                if (AiClass(type) != 2) continue;
                auto u = unitData.find(type);
                const int base = u == unitData.end() ? type : u->second.baseType;
                bool idle = false, queued = false;
                for (int s = 1; s <= warmap::SectorCount; s++)
                {
                    if (!(pathLength[size_t(sector)][size_t(s)] > 1 || s == sector)) continue;
                    for (int c : SectorObjects(s, false, 0))
                        if (counters[size_t(c)].type == base)
                            for (int r : counters[size_t(c)].refs) if (!Working(r)) idle = true;
                    for (int c : SectorObjects(s, false, 1))
                        for (int r : counters[size_t(c)].refs)
                        {
                            auto f = factories.find(r);
                            if (f == factories.end()) continue;
                            for (const BuildEntry& e : f->second.queue.entries) if (e.type == type) queued = true;
                        }
                }
                if (!idle && !queued) { choice = type; break; }
            }
            if (!choice)
            {
                std::vector<int> combat;
                for (int type : options) if (AiClass(type) == 1) combat.push_back(type);
                if (!combat.empty()) choice = combat[size_t(random::Rand() % int(combat.size()))];
            }
            if (choice && AddType(ref, choice)) SDL_Log("War map AI: factory in sector %d makes type %d", sector, choice);
        }
    }
}

// The building phases (2-5) and AI.ojd's build sites
namespace wow
{
    // An AI.ojd site (0x441580, opcodes 5 / 6: {type, x, y} or a fence's {type, x1, y1, x2, y2}, 16 bytes with the
    // object built there at +0x0C), in the lists for the side not being played (0x518814, one per sector); the
    // computer player's are those. Linked to the object of exactly its type standing at its place (0x440F80: the
    // battle object's +0x78 / +0x7C truncated; a command building matches wherever it is). The game links them
    // when the war map starts (0x441110), as the AI's constructions come into a sector (0x441420) and again on some
    // sector events (0x440EA0); the engine works the link out when it's asked (*provisional*, the same while
    // every object stays where it was put).
    int WarMap::AiSiteObject(int sector, const AiSite& site, int except) const
    {
        const bool post = site.type == (sector == 3 ? 830 : 120) || site.type == (sector == 30 ? 278 : 58);
        for (int kind = 1; kind <= 2; kind++)
            for (int c : SectorObjects(sector, false, kind))
                for (int ref : counters[size_t(c)].refs)
                {
                    if (ref == except || table[size_t(ref)].Type() != site.type) continue;
                    if (post) return ref;
                    float x, y;
                    if (table[size_t(ref)].BattlePosition(x, y) && int16_t(int(x)) == int16_t(site.x) && int16_t(int(y)) == int16_t(site.y))
                        return ref;
                }
        return -1;
    }

    const std::vector<WarMap::AiSite>& WarMap::AiSites(int sector) const
    {
        static const std::vector<AiSite> none;
        auto it = aiSites.find(sector);
        return it == aiSites.end() ? none : it->second;
    }

    bool WarMap::AiFinished(int ref) const // 0x42AA70
    {
        const SaveObject& o = table[size_t(ref)];
        return !o.Kind2() || o.Value(1) >= o.Value(0);
    }

    // 0x4423D0: its research done (or none), and its prerequisite (record 54) the first of its type among the
    // AI's constructions in the sector (0x421C10) and finished
    bool WarMap::AiCanBuildHere(int type, int sector) const
    {
        for (const auto& r : research)
            if (r)
                if (const Research::Item* item = r->ItemFor(type))
                    if (item->state != Research::Done && item->state != Research::Obsolete) return false;
        auto u = unitData.find(type);
        if (u == unitData.end() || !u->second.prerequisite) return true;
        for (int c : SectorObjects(sector, false, 1))
            for (int ref : counters[size_t(c)].refs)
                if (table[size_t(ref)].Type() == u->second.prerequisite) return AiFinished(ref);
        return false;
    }

    // 0x442460: a constructor of type `type` can put up `building` (it's among the building's builders and the
    // building's research is done)
    bool WarMap::AiCanBuild(int type, int building) const
    {
        auto u = unitData.find(building);
        if (u == unitData.end()) return false;
        if (u->second.builders[0] != type && u->second.builders[1] != type && u->second.builders[2] != type) return false;
        for (const auto& r : research)
            if (r)
                if (const Research::Item* item = r->ItemFor(building))
                    return item->state == Research::Done || item->state == Research::Obsolete;
        return true;
    }

    // 0x441310: the first free site in the sector whose type is of the AI class and can be built there
    int WarMap::AiFirstBuildableSite(int sector, int cls) const
    {
        for (const AiSite& s : AiSites(sector))
            if (AiClass(s.type) == cls && AiSiteObject(sector, s) < 0 && AiCanBuildHere(s.type, sector)) return s.type;
        return 0;
    }

    // 0x4411B0 / 0x441290: the first free site for the type (by its base type, record 18), and its place
    const WarMap::AiSite* WarMap::AiFreeSite(int sector, int type) const
    {
        auto u = unitData.find(type);
        const int base = u == unitData.end() ? type : u->second.baseType;
        for (const AiSite& s : AiSites(sector))
            if (s.type == base && AiSiteObject(sector, s) < 0) return &s;
        return nullptr;
    }

    // 0x4607C0: how well a sector could run one more of the type for the side: 1 / the product over its three
    // kinds of (spread demand + the type's work use, record 47) / spread supply, each at least 1
    float WarMap::AiEfficiency(int type, int sector) const
    {
        if (sector < 1 || sector > warmap::SectorCount) return 0;
        const int base = res.Human() ? 3 : 0;
        auto u = unitData.find(type);
        const SectorEconomy& e = economy[size_t(sector)];
        double product = 1.0;
        for (int k = 0; k < 3; k++)
        {
            const double use = u == unitData.end() ? 0.0 : double(u->second.activity[size_t(k)]);
            double r = (double(e.effDemand[size_t(base + k)]) + use - double(e.effSupply[size_t(base + k)])) / double(e.effSupply[size_t(base + k)]) + 1.0;
            if (r < 1.0) r = 1.0;
            product *= r;
        }
        return float(1.0 / product);
    }

    bool WarMap::AiReachable(int from, int to) const // the sector's path (0x43C090) has more than its start, or it's itself
    {
        return to == from || pathLength[size_t(from)][size_t(to)] > 1;
    }

    // 0x444510: the constructor goes there if it isn't (each sector's count of constructors there or on their way,
    // record +4, kept), then is told to build the type at its first free site (0x4411B0 -> 0x460240); a
    // Communications Centre (120) brings its partner (119) at that one's site, if it has one.
    void WarMap::AiSendToBuild(int counter, int sector, int type)
    {
        Counter& k = counters[size_t(counter)];
        if (k.sector != sector)
        {
            aiSectors[size_t(k.sector)].constructors--;
            AiMove(counter, sector);
            aiSectors[size_t(sector)].constructors++;
        }
        const int ref = k.refs.empty() ? -1 : k.refs.front();
        if (ref < 0) return;
        if (OrderConstruction(ref, type))
            SDL_Log("War map AI: constructor in sector %d builds type %d", sector, type);
        if (type == 0x78)
            if (const AiSite* s = AiFreeSite(sector, 0x77); s && s->x && s->y) OrderConstruction(ref, 0x77);
    }

    // 0x441420, an AI construction coming into a sector: the first free site of its type takes it, and it's put
    // there (0x80D1 / 0x80D2 to its battle object)
    void WarMap::AiPlaceAtSite(int ref)
    {
        if (ref < 0 || size_t(ref) >= table.size()) return;
        const int at = CounterOf(ref);
        if (at < 0 || counters[size_t(at)].players) return;
        const int sector = counters[size_t(at)].sector;
        for (const AiSite& s : AiSites(sector))
            if (s.type == table[size_t(ref)].Type() && AiSiteObject(sector, s, ref) < 0)
            {
                table[size_t(ref)].SetBattlePosition(float(s.x), float(s.y));
                return;
            }
    }

    // Phase 2 (0x443610). Each sector's record (0x545BB0, 0x18 bytes) is cleared; the constructors (class 2 with a
    // factory) in each sector counted (+4) and, idle ones that can put up the side's command building (58 or 120,
    // 0x442460), gathered. Then for each of the AI's sectors not yet looked at (+8 still 1.0), the sectors it has a
    // path to (and itself) are one network (+8 set to 2.0): each site there with an object tells the record a
    // finished factory (class 4, +0; an idle one +0x14, its work use, record 47, added up for the network), a
    // factory going up (+0x16), a resource facility going up (class 6, +0x15), or that the command building stands
    // (any other class); one of the AI's with no command building finished nor a site holding one is marked to need
    // it (+0x17). The network's records take its work use (+8-+0x10). Then each of the network's sectors that's the
    // AI's, has its command building and no resource facility going up, gets for each kind k a priority (spread
    // demand + the network's use - spread supply + "AI Build Resfac Eagerness") x one more facility's gain, and
    // each above 0 a candidate (the gain, the sector, k); the candidates sorted by priority, highest first (a
    // bubble sort). True with no constructors gathered.
    bool WarMap::AiEconomy()
    {
        for (AiSector& r : aiSectors) r = AiSector{};
        for (int s = 1; s <= warmap::SectorCount; s++) aiSectors[size_t(s)].use[0] = 1.0f;
        for (auto& p : aiPriority) p = { 0.0f, 0.0f, 0.0f };
        ai.groups.clear();
        const int post = res.Human() ? 0x78 : 0x3A;
        for (int s = 1; s <= warmap::SectorCount; s++)
            for (int c : SectorObjects(s, false, 0))
            {
                const Counter& k = counters[size_t(c)];
                const int ref = k.refs.empty() ? -1 : k.refs.front();
                if (AiClass(k.type) != 2 || factories.find(ref) == factories.end()) continue;
                aiSectors[size_t(s)].constructors++;
                if (!Working(ref) && AiCanBuild(k.type, post)) ai.groups.push_back(c);
            }
        aiResource.clear();
        for (int v = 1; v <= warmap::SectorCount; v++)
        {
            if (owner[size_t(v)] != 1 || aiSectors[size_t(v)].use[0] != 1.0f) continue;
            float use[3] = { 0, 0, 0 };
            for (int s = 1; s <= warmap::SectorCount; s++)
            {
                if (!AiReachable(v, s)) continue;
                AiSector& r = aiSectors[size_t(s)];
                r.use[0] = 2.0f;
                bool noPost = true;
                for (const AiSite& site : AiSites(s))
                {
                    const int obj = AiSiteObject(s, site);
                    if (obj < 0) continue;
                    const int cls = AiClass(site.type);
                    if (cls == 4)
                    {
                        if (AiFinished(obj) && factories.count(obj))
                        {
                            if (!Working(obj))
                            {
                                r.idleFactory = true;
                                auto u = unitData.find(table[size_t(obj)].Type());
                                if (u != unitData.end())
                                    for (int k = 0; k < 3; k++) use[k] += u->second.activity[size_t(k)];
                            }
                            r.factories++;
                        }
                        if (!AiFinished(obj)) r.factoryBuilding = true;
                    }
                    else if (cls == 6) { if (!AiFinished(obj)) r.resfacBuilding = true; }
                    else
                    {
                        const int t = table[size_t(obj)].Type();
                        const int sec = table[size_t(obj)].Sector();
                        if (t == (sec == 3 ? 830 : 120) || t == (sec == 30 ? 278 : 58)) noPost = false;
                    }
                }
                if (owner[size_t(s)] == 1 && !HasCommandPost(s) && noPost) r.needsPost = true;
            }
            for (int s = 1; s <= warmap::SectorCount; s++)
                if (aiSectors[size_t(s)].use[0] == 2.0f)
                    for (int k = 0; k < 3; k++) aiSectors[size_t(s)].use[k] = use[k];
            const int base = res.Human() ? 3 : 0;
            for (int s = 1; s <= warmap::SectorCount; s++)
            {
                if (!AiReachable(v, s)) continue;
                const AiSector& r = aiSectors[size_t(s)];
                if (r.resfacBuilding || owner[size_t(s)] != 1 || !HasCommandPost(s)) continue;
                const SectorEconomy& e = economy[size_t(s)];
                for (int k = 0; k < 3; k++)
                {
                    const float gain = ResourceGain(s, base + k);
                    const float d = float(double(e.effDemand[size_t(base + k)]) + double(r.use[k]));
                    aiPriority[size_t(s)][size_t(k)] = float((double(d) - double(e.effSupply[size_t(base + k)]) + double(aiTweaks.resfacEagerness)) * double(gain));
                }
                for (int k = 0; k < 3; k++)
                    if (aiPriority[size_t(s)][size_t(k)] > 0.0f) aiResource.push_back({ ResourceGain(s, base + k), s, k });
            }
        }
        for (bool swapped = true; swapped && aiResource.size() > 1;)
        {
            swapped = false;
            for (size_t i = 0; i + 1 < aiResource.size(); i++)
                if (aiPriority[size_t(aiResource[i].sector)][size_t(aiResource[i].k)] < aiPriority[size_t(aiResource[i + 1].sector)][size_t(aiResource[i + 1].k)])
                {
                    std::swap(aiResource[i], aiResource[i + 1]);
                    swapped = true;
                }
        }
        ai.cursor = 0;
        return ai.groups.empty();
    }

    // Phase 3 (0x443CA0), the next constructor: unless a factory is going up in its sector (+0x16), the best
    // factory site (class 4) by efficiency (0x4607C0) in the sectors it has a path to that have their command
    // building; above [Tweak] "AI Eff Threshold" (0.6) it's built and the network marked (+0x16). (A "panic"
    // build follows, compared with "AI Panic Build Threshold" (500), but the game compares the candidate's index
    // with it, so it never happens.) Else a sector it has a path to that needs its command building (+0x17), if it's
    // its own or has no constructor there, while the constructor's own type runs better than 0.4 where it is. Else
    // the first resource candidate with a gain above "AI Minimum Rr" (8.0), no facility going up there, its own
    // sector or one with no constructor, and a free site for that kind's facility (47-49 Human, 108-110 Martian).
    // True when it was the last constructor.
    bool WarMap::AiBuilder()
    {
        if (ai.cursor >= ai.groups.size()) return true;
        const int g = ai.groups[ai.cursor++];
        const auto done = [&] { return ai.cursor == ai.groups.size(); };
        const Counter& k = counters[size_t(g)];
        const int here = k.sector;
        const int post = res.Human() ? 0x78 : 0x3A;
        const int base = res.Human() ? 3 : 0;
        if (!aiSectors[size_t(here)].factoryBuilding)
        {
            float best = 0.0f;
            int type = 0, target = 0;
            for (int s = 1; s <= warmap::SectorCount; s++)
            {
                if (!AiReachable(here, s) || !HasCommandPost(s)) continue;
                const int t = AiFirstBuildableSite(s, 4);
                const float e = AiEfficiency(t, s);
                if (t && e > best) { best = e; type = t; target = s; }
            }
            if (best > aiTweaks.effThreshold)
            {
                AiSendToBuild(g, target, type);
                for (int s = 1; s <= warmap::SectorCount; s++) if (AiReachable(here, s)) aiSectors[size_t(s)].factoryBuilding = true;
                return done();
            }
        }
        for (int s = 1; s <= warmap::SectorCount; s++)
        {
            if (!AiReachable(here, s)) continue;
            AiSector& r = aiSectors[size_t(s)];
            if (!r.needsPost || !(s == here || r.constructors == 0)) continue;
            if (AiEfficiency(k.type, here) > 0.4f)
            {
                r.needsPost = false;
                AiSendToBuild(g, s, post);
                return done();
            }
        }
        static constexpr int Facility[6] = { 0x2F, 0x30, 0x31, 0x6C, 0x6D, 0x6E }; // dword_4BEDC0
        for (const AiCandidate& c : aiResource)
        {
            AiSector& r = aiSectors[size_t(c.sector)];
            if (!(c.score > aiTweaks.minimumRr) || r.resfacBuilding || !(c.sector == here || r.constructors == 0)) continue;
            const int type = Facility[base + c.k];
            if (!AiFreeSite(c.sector, type)) continue;
            r.resfacBuilding = true;
            AiSendToBuild(g, c.sector, type);
            return done();
        }
        return done();
    }

    // Phase 4 (0x4440B0): each of the AI's sectors with its command building is a candidate scored x / (its
    // defences (class 7) / (its enemy neighbours x [Tweak] "AI Frontline Attractor for Defences" (2.0) + 1) / (1.4
    // in sector 3 or 30, else 1) + 1), x 2 on the front (an enemy neighbour) or in sector 3 or 30, 1.5 next to the
    // front, else 1; sorted highest first. Then the constructors are counted again and the idle ones that can't put
    // up a command building gathered. True with none.
    bool WarMap::AiFrontline()
    {
        ai.cursor = 0;
        aiFront.clear();
        std::array<bool, warmap::SectorCount + 1> front{};
        for (int s = 1; s <= warmap::SectorCount; s++)
            front[size_t(s)] = owner[size_t(s)] == 1 && !AiNeighbours(s, 2).empty();
        for (int s = 1; s <= warmap::SectorCount; s++)
        {
            if (owner[size_t(s)] != 1 || !HasCommandPost(s)) continue;
            float mult = 1.0f;
            if (front[size_t(s)] || s == 30 || s == 3) mult = 2.0f;
            else
                for (int n : AiNeighbours(s, 0)) if (front[size_t(n)]) { mult = 1.5f; break; }
            float defence = 0;
            for (int c : SectorObjects(s, false, 1)) if (AiClass(counters[size_t(c)].type) == 7) defence += ObjectStrength(c);
            const double enemies = double(AiNeighbours(s, 2).size());
            const double x = double(defence) / (enemies * double(aiTweaks.frontlineDefences) + 1.0);
            const double c = (s == 30 || s == 3) ? 1.4 : 1.0;
            const float score = float(1.0 / (x / c + 1.0));
            aiFront.push_back({ float(double(mult) * double(score)), s, 0 });
        }
        for (bool swapped = true; swapped && aiFront.size() > 1;)
        {
            swapped = false;
            for (size_t i = 0; i + 1 < aiFront.size(); i++)
                if (aiFront[i].score < aiFront[i + 1].score) { std::swap(aiFront[i], aiFront[i + 1]); swapped = true; }
        }
        ai.groups.clear();
        const int post = res.Human() ? 0x78 : 0x3A;
        for (int s = 1; s <= warmap::SectorCount; s++)
        {
            aiSectors[size_t(s)].constructors = 0;
            for (int c : SectorObjects(s, false, 0))
            {
                const Counter& k = counters[size_t(c)];
                const int ref = k.refs.empty() ? -1 : k.refs.front();
                if (AiClass(k.type) != 2 || factories.find(ref) == factories.end()) continue;
                aiSectors[size_t(s)].constructors++;
                if (!Working(ref) && !AiCanBuild(k.type, post)) ai.groups.push_back(c);
            }
        }
        return ai.cursor == ai.groups.size();
    }

    // Phase 5 (0x4443D0), the next defence builder: down the front candidates while their score is above 0, the
    // first it has a path to (not its own sector) with no constructor there and a buildable defence site (class 7,
    // 0x441310) gets that defence. True when it was the last.
    bool WarMap::AiDefender()
    {
        if (ai.cursor >= ai.groups.size()) return true;
        const int g = ai.groups[ai.cursor++];
        const auto done = [&] { return ai.cursor == ai.groups.size(); };
        const Counter& k = counters[size_t(g)];
        if (AiCanBuild(k.type, res.Human() ? 0x78 : 0x3A)) return done();
        for (const AiCandidate& c : aiFront)
        {
            if (!(c.score > 0.0f)) break;
            if (pathLength[size_t(k.sector)][size_t(c.sector)] <= 1 || aiSectors[size_t(c.sector)].constructors != 0) continue;
            if (const int t = AiFirstBuildableSite(c.sector, 7))
            {
                AiSendToBuild(g, c.sector, t);
                return done();
            }
        }
        return done();
    }

    // 0x442D80 (0x111 / 0xB9 to the AI player, 0x442AC0: its research finished a type): for a building (+0x10, 2),
    // in each sector 1-30 that's the AI's with its command building finished (0x441A80: 0x43CA30, 0x43C990), every
    // construction (the sector's +0x58 list) whose type's +0x84 or +0x88 (the type it gives way to, records 19 / 20)
    // is the new type takes it (+0x20). byte_56625C is set meanwhile. (+0x84 isn't written by any record.)
    void WarMap::AiApplyUpgrade(int type)
    {
        auto t = unitData.find(type);
        if (t == unitData.end() || t->second.category != 2) return;
        for (int s = 1; s <= warmap::SectorCount; s++)
        {
            if (owner[size_t(s)] != 1 || !HasCommandPost(s)) continue;
            for (size_t i = 0; i < counters.size(); i++)
            {
                Counter& k = counters[i];
                if (k.sector != s || k.Travelling() || k.kind != 1 || k.players) continue;
                auto u = unitData.find(k.type);
                if (u == unitData.end() || u->second.upgrade != type) continue;
                k.type = type;
                k.frame = t->second.counterFrame;
                for (int ref : k.refs)
                    if (ref >= 0 && size_t(ref) < table.size()) table[size_t(ref)].SetType(type);
            }
            RefreshSector(s);
        }
    }
}
