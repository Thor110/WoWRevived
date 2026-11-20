// The battle AI (the computer player's battle part, 0x445010-0x445E30) and the groups' periodic stance work
// (0x483B70). See docs/BATTLEMAP.md "The battle AI".
#include "battlemap/BattleMap.h"
#include "core/Random.h"
#include <algorithm>
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
    }

    // 0x441E70: an object's strength, its battle units (0x4216D0) times its type's weight (word_518B10)
    int BattleMap::AiValue(int group) const
    {
        int n = 0;
        for (const Unit& u : units) if (u.group == group) n++;
        return n * setup.groups[size_t(group)].aiWeight;
    }

    // 0x8075 to the AI player (0x445210): the counts and lists cleared; +0x450 whether it holds the sector (0x43CA30
    // against its side). Holding it, every construction in the sector, either side's (the sector's lists 2 and 3,
    // 0x446370), becomes a threat, active, up to 16. Then each of its own units (list 1) goes in (0x445500): one with
    // no battle units is passed over; one of AI class 2 has its group set to stance 0 (Avoid, 0x80B2) and isn't
    // added; the rest are added to its list.
    void BattleMap::AiStart()
    {
        aiThreats.clear();
        aiOwn.clear();
        aiTasks.clear();
        aiOn = battleOn;
        if (!aiOn) return;
        const int aiFaction = setup.human ? 1 : 0;
        aiHolder = setup.ownerFaction == aiFaction;
        if (aiHolder)
            for (size_t g = 0; g < setup.groups.size() && aiThreats.size() < 16; g++)
                if (setup.groups[g].list / 2 == 1) aiThreats.push_back({ int(g), true });
        for (size_t g = 0; g < setup.groups.size(); g++)
        {
            if (setup.groups[g].list != 1) continue;
            if (AiValue(int(g)) == 0 && std::none_of(units.begin(), units.end(), [&](const Unit& u) { return u.group == int(g); })) continue;
            if (setup.groups[g].aiClass == 2) { SetStance(int(g), 0); continue; }
            aiOwn.push_back(int(g));
        }
    }

    // 0x4455D0, each update: +0x45C counts up; with no tasks the planner runs (0x4456B0); otherwise the cursor
    // (+0x470) moves on, round the 32 slots, to the next task (the tasks act when they're made, 0x445010)
    void BattleMap::AiUpdate()
    {
        if (!aiOn) return;
        if (aiTasks.empty()) AiPlan();
    }

    // 0x445010, a task made: its group the own object's first battle unit's (0x47D600 -> 0x47EB00). With no threat,
    // the group's stance is Roam and Attack (0x80B2 with 3). A threat of the AI's own side (0x4406E0): the group goes
    // to its first battle unit's point (0x482510). Another side's: the group is told 0x80C0 with its first battle unit
    // (0x4807F0's case: 0x80B9 to the members, then 0x482880, the target).
    void BattleMap::AiRunTask(const AiTask& t)
    {
        const int g = t.own;
        if (g < 0) return;
        if (t.threat < 0) { SetStance(g, 3); return; }
        const Unit* target = nullptr;
        for (const Unit& u : units) if (u.group == t.threat) { target = &u; break; }
        if (!target) return;
        if (target->players == setup.groups[size_t(g)].Players())
        {
            std::vector<size_t> members;
            for (size_t i = 0; i < units.size(); i++)
                if (units[i].group == g && units[i].vehicle) members.push_back(i);
            OrderGroupMove(g, members, int(target->x), int(target->y));
            return;
        }
        GroupTarget& gt = groupTargets[size_t(g)];
        gt = GroupTarget{};
        gt.target = target->id;
        groupMode[size_t(g)] = 0;
    }

    // 0x4456B0, with its own list not empty:
    //  - holding the sector (+0x450): the first active threat that's the battle's key building (the Martian base 0x33E
    //    in sector 3, else 0x78; Parliament 0x116 in London, else 0x13A) is taken (its flag cleared), and its
    //    strongest own object (0x441E70) put on it (taken from the list, 0x445E30); with none to send, the threat's
    //    flag is set again;
    //  - r, its own count over the threats' (+0x474; 1.0 with none): over 5.0, a third of its own go to the next
    //    active threat and a quarter roam; over 3.0, two thirds go and a third roam; over 0.0, none go and all roam;
    //  - the next active threat (its flag cleared) gets that many, each the first in its list, while there are
    //    slots (32); then that many more from the list's front each get a task with no threat (Roam and Attack).
    // *Provisional*: the strongest is taken as the first of the greatest.
    void BattleMap::AiPlan()
    {
        if (aiOwn.empty()) return;
        auto addTask = [&](int threat, int own) {
            if (aiTasks.size() >= 32) return;
            AiTask t{ threat, own };
            aiTasks.push_back(t);
            AiRunTask(t);
        };
        auto takeOwn = [&](int g) { aiOwn.erase(std::remove(aiOwn.begin(), aiOwn.end(), g), aiOwn.end()); };
        if (aiHolder)
        {
            int key = -1;
            for (size_t i = 0; i < aiThreats.size(); i++)
            {
                if (!aiThreats[i].active) continue;
                const int type = setup.groups[size_t(aiThreats[i].group)].type;
                if (type == (setup.sector == 3 ? 0x33E : 0x78) || type == (setup.sector == 30 ? 0x116 : 0x13A)) { key = int(i); break; }
            }
            if (key >= 0)
            {
                aiThreats[size_t(key)].active = false;
                int best = -1, bestValue = 0;
                for (int g : aiOwn)
                    if (const int v = AiValue(g); best < 0 || v > bestValue) { best = g; bestValue = v; }
                if (best >= 0)
                {
                    takeOwn(best);
                    addTask(aiThreats[size_t(key)].group, best);
                }
                else aiThreats[size_t(key)].active = true;
            }
        }
        const int own = int(aiOwn.size());
        const float r = aiThreats.empty() ? 1.0f : float(own) / float(aiThreats.size());
        int send = 0, rest = 0;
        if (r > 5.0f) { send = int(unsigned(own) / 3u); rest = own >> 2; }
        else if (r > 3.0f) { rest = int(unsigned(own) / 3u); send = own - rest; }
        else if (r != 0.0f) rest = own;
        int threat = -1;
        for (AiThreat& t : aiThreats)
            if (t.active) { t.active = false; threat = t.group; break; }
        if (threat >= 0)
            for (int k = 0; k < send; k++)
            {
                if (aiTasks.size() >= 32 || aiOwn.empty()) continue;
                const int g = aiOwn.front();
                takeOwn(g);
                addTask(threat, g);
            }
        for (int k = 0; k < rest; k++)
        {
            if (aiTasks.size() >= 32 || aiOwn.empty()) continue;
            const int g = aiOwn.front();
            takeOwn(g);
            addTask(-1, g);
        }
    }

    // 0x807A to the AI player (0x445B90), an object gone, with a battle on: its own (its side the AI's): every task
    // whose own object it was goes, its list loses it, and a task's second object that was it is cleared; another
    // side's: every task whose threat it was goes, and every threat that was it (+0x474 down).
    void BattleMap::AiObjectGone(int group)
    {
        if (!aiOn || group < 0 || size_t(group) >= setup.groups.size()) return;
        const bool own = !setup.groups[size_t(group)].Players();
        if (own)
        {
            aiTasks.erase(std::remove_if(aiTasks.begin(), aiTasks.end(), [&](const AiTask& t) { return t.own == group; }), aiTasks.end());
            aiOwn.erase(std::remove(aiOwn.begin(), aiOwn.end(), group), aiOwn.end());
            return;
        }
        aiTasks.erase(std::remove_if(aiTasks.begin(), aiTasks.end(), [&](const AiTask& t) { return t.threat == group; }), aiTasks.end());
        aiThreats.erase(std::remove_if(aiThreats.begin(), aiThreats.end(), [&](const AiThreat& t) { return t.group == group; }), aiThreats.end());
    }

    // 0x483B70's periodic part, every group: +0x58 counts up past +0x5C (90), then (vt+0x38 first):
    //  - Roam and Attack (3), not attacking (+0x1D8 bit 0) and with members: to x = rand() % (the width - 200) + 100,
    //    y = rand() % (the height - 200) + 100 (0x482510);
    //  - Attack or Retaliate (2, 1), not attacking and with members: its centre (each member's whole position summed,
    //    over the count); with no home yet (+0x60 / +0x64 both 0) the centre is home; more than 400 from home
    //    (0x43FF00), bit 0 set and 0x804F home;
    // then bit 2 set; and +0x58 back to 0.
    // *Provisional*: vt+0x38 isn't in; "attacking" is taken as having a target.
    void BattleMap::GroupTick(int g)
    {
        if (size_t(g) >= groupTicks.size()) return;
        const int count = groupTicks[size_t(g)]++;
        if (count <= 90) return;
        groupTicks[size_t(g)] = 0;
        const int stance = groupStance[size_t(g)];
        const bool attacking = groupTargets[size_t(g)].target != 0;
        std::vector<size_t> members;
        for (size_t i = 0; i < units.size(); i++)
            if (units[i].group == g && units[i].vehicle) members.push_back(i);
        if (attacking || members.empty()) return;
        if (stance == 3)
        {
            const int w = land.WorldWidth(), h = int((int64_t(land.Height()) * land.Cell()) >> 16);
            const int x = random::Rand() % (w - 200) + 100;
            const int y = random::Rand() % (h - 200) + 100;
            OrderGroupMove(g, members, x, y);
            return;
        }
        if (stance != 1 && stance != 2) return;
        int sx = 0, sy = 0;
        for (size_t i : members) { sx += int(units[i].x); sy += int(units[i].y); }
        const int cx = int(unsigned(sx) / unsigned(members.size())), cy = int(unsigned(sy) / unsigned(members.size()));
        int& hx = groupHomeX[size_t(g)];
        int& hy = groupHomeY[size_t(g)];
        if (hx == 0 && hy == 0) { hx = cx; hy = cy; }
        if (Octagonal(cx, cy, hx, hy) > 400) OrderGroupMove(g, members, hx, hy);
    }

    // A group's last unit gone (0x8057 from its war map object's last battle unit): the AI player told (0x807A)
    void BattleMap::GroupGone(int group)
    {
        AiObjectGone(group);
    }
}
