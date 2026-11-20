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
// Phases 2-5 (building, which goes by AI.ojd's build sites) aren't in yet: they draw no random numbers, so the
// rest draws them in the game's order; for now they pass on as if there were nothing to build.
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
        // its research (+0x454): an item done or made obsolete is let go; with none, one is picked (0x458A60)
        if (Research* r = research[1].get())
        {
            if (ai.researchItem >= 0)
            {
                const auto& item = r->Items()[size_t(ai.researchItem)];
                if (item.state == Research::Done || item.state == Research::Obsolete) ai.researchItem = -1;
            }
            else
            {
                ai.researchItem = r->ChooseForAi();
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
        case 2: ai.phase += 2; break; // 0x443610: no constructors gathered yet (see the top)
        case 3: ai.phase++; break;
        case 4: ai.phase += 2; break; // 0x4440B0: likewise
        case 5: ai.phase++; break;
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
