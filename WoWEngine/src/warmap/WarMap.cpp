#include "warmap/WarMap.h"

#include "core/Random.h"
#include "warmap/AStar.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace wow
{
    namespace
    {
        constexpr int ScreenW = 640, ScreenH = 480;
        constexpr int EdgeZone = 4;           // pixels from the edge that scroll the view
        // The map's shade table has 89 levels: ramps of tints either side of the normal 11, then solid colours at
        // 59-66. 0x41D9B0 colours each border by the sectors either side of it:
        constexpr int SelectedBorderLevel = 66; // white: the selected sector's ([+0x84]+0x60)
        constexpr int HoverBorderLevel = 61;    // cream: the other sector it tracks ([+0x84]+0x5C), the one under the mouse
        constexpr int OwnBorderLevel = 64;      // green: the side being played against no one
        constexpr int EnemyBorderLevel = 63;    // dark red: the other side against no one
        constexpr int FrontLevel = 62;          // orange: the two sides against each other
        constexpr int PlainBorderLevel = 65;    // tan: the same owner either side, or no one
        constexpr int MiniW = 100, MiniH = 170;
    }

    WarMap::WarMap(Resources& r, MenuColours c, const SaveInfo& start, std::vector<SaveInfo::WarMapObject> objects,
                   std::map<int, ObjOjd::UnitData> units, std::map<int, int> objectClasses, std::vector<int> owners, int scrollSpeed, float clunting)
        : Screen(Id, r, 0, warmap::PaletteGroup), colours(c), game(start), cluntingSpeed(clunting)
    {
        // the war map's numbers start from srand(1) each time (WWarMapWorld 0x417CD0)
        random::Seed(1);
        unitData = units;
        // A counter for each object (0x42B2F0, made by 0x4205F0), except that objects of class 7 (AI.ojd's class,
        // 0x518B14: wire fences, mines) share one a type and sector (0x4282A0 / 0x428270), with how many there are
        // in the counter font (GType 0x6E, right-aligned). Where they go is the counter placement's (LayoutAll):
        // the war map doesn't use the places objects are saved with.
        // a construction counts once it's finished: 0x42AA70 wants its +0x7C at least its +0x78, taken here as the
        // save's second float against its first (provisional: which float is which isn't traced; at the start
        // both are 1)
        for (const auto& o : objects)
            if ((o.list == 2 || o.list == 3) && o.sector >= 1 && o.sector <= warmap::SectorCount && o.value[1] >= o.value[0])
                finished[size_t(o.sector)].push_back(o.unitType);
        struct Group { int sector, list, type, x, y, count; std::vector<int> refs; };
        std::vector<Group> groups;
        for (const auto& o : objects)
        {
            if (o.sector < 1 || o.sector > warmap::SectorCount) continue;
            Group* g = nullptr;
            auto cls = objectClasses.find(o.unitType);
            if (cls != objectClasses.end() && cls->second == 7)
                for (Group& existing : groups)
                    if (existing.sector == o.sector && existing.list == o.list && existing.type == o.unitType) { g = &existing; break; }
            if (!g) { groups.push_back({ o.sector, o.list, o.unitType, 0, 0, 0, {} }); g = &groups.back(); }
            g->count++;
            if (o.ref >= 0) g->refs.push_back(o.ref);
            if (!g->x && !g->y) { g->x = o.x; g->y = o.y; }
        }
        for (const Group& g : groups)
        {
            auto unit = units.find(g.type);
            Counter k;
            k.x = g.x; k.y = g.y;
            k.frame = unit == units.end() ? 0 : unit->second.counterFrame;
            k.speed = unit == units.end() || g.list / 2 != 0 ? 0 : unit->second.warMapSpeed;
            k.moveClass = unit == units.end() ? 0 : unit->second.moveClass;
            k.count = g.count;
            k.type = g.type;
            k.refs = g.refs;
            k.players = g.list % 2 == 0;
            k.kind = g.list / 2;
            k.sector = g.sector;
            counters.push_back(k);
    }
        levels.fill(warmap::NormalLevel);
        // who holds each sector, from the save (0 Human, 1 Martian, 2 no one), as the side being played (0),
        // the other side (1) or no one (2)
        owner.fill(2);
        int side = res.Human() ? 0 : 1;
        for (int s = 1; s <= warmap::SectorCount && size_t(s) < owners.size(); s++)
            owner[size_t(s)] = owners[size_t(s)] == 2 ? 2 : owners[size_t(s)] == side ? 0 : 1;
        map = res.GetFrame(warmap::MapGType);
        sectorMap = res.GetFrame(warmap::SectorMapGType);
        // [Screen] "Scroll Speed" (default 40) in map pixels a tick, a fifth of it (provisional)
        scrollStep = std::max(2, scrollSpeed / 5);
        int sector = int(game.warMap[2]);
        selected = sector >= 1 && sector <= warmap::SectorCount ? sector : 0;
        // The WMAP block holds the view's top-left corner, already less half of the game's 640 x 480 screen
        // (0x418980 subtracts it from the centre): Martian.000's 159, 298 centres on sector 3's units at 468, 540,
        // Human.000's 803, 1805 on London's at 1116, 2016. So the centre is that plus 320, 240.
        CentreOn(int(game.warMap[0]) + 320, int(game.warMap[1]) + 240);
        LayoutAll();
    }

    int WarMap::ViewW() const { return ScreenW; }
    int WarMap::ViewH() const { return ScreenH; }

    void WarMap::CentreOn(int x, int y)
    {
        cameraX = x - ViewW() / 2;
        cameraY = y - ViewH() / 2;
        Clamp();
    }

    void WarMap::Clamp()
    {
        cameraX = std::clamp(cameraX, 0, warmap::MapWidth - ViewW());
        cameraY = std::clamp(cameraY, 0, warmap::MapHeight - ViewH());
    }

    // The mini-map panel, "WarMap miniMap frame" (0x423420) around "WarMap miniMap view" (0x422C70). The view sits
    // in the bottom right corner (where exactly is from WoWRevived's screenshot: 115 and 175 in from the right and
    // bottom edges); 0x418C14 then puts the frame (GType 0x80: hmframe.SPR 122 x 290, mmframe.SPR 121 x 130) above
    // it, 0x50 + 0x20 higher and 9 to the left in Human games, 0x44 + 0x28 and 10 in Martian ones. The three view
    // buttons (0x4333B0, command 0x811F) are placed in the frame by 0x423420.
    WarMap::PanelLayout WarMap::Panel() const
    {
        PanelLayout p;
        if (!panelShown) return p; // hidden with Tab: nothing to hit
        p.mini = { ScreenW - 115, ScreenH - 175, MiniW, MiniH };
        bool human = res.Human();
        Rect frameSize = SpriteRect(0x80, 0, 0);
        p.frame = { p.mini.x - (human ? 9 : 10), p.mini.y - (human ? 0x50 + 0x20 : 0x44 + 0x28), frameSize.w, frameSize.h };
        static constexpr int Gtypes[3] = { 0x33, 0x35, 0x34 };        // Unit View, Construction View, Resource View
        static constexpr int HumanX[3] = { 6, 0x2A, 0x4F }, MartianX[3] = { 0xA, 0x2D, 0x51 };
        for (int i = 0; i < 3; i++)
        {
            Rect size = SpriteRect(Gtypes[i], 0, 0);
            p.buttons[i] = { p.frame.x + (human ? HumanX[i] : MartianX[i]), p.frame.y + (human ? 0x29 : 0x24), size.w, size.h };
        }
        // the Research button (0x423420 -> 0x45BD70, GType 0x341 rese-but.spr, control 0x7D)
        Rect rb = SpriteRect(0x341, 0, 0);
        p.research = { p.frame.x + (human ? 0x48 : 0x3A), p.frame.y + (human ? 5 : 0xC), rb.w, rb.h };
        return p;
    }

    Rect WarMap::MiniMapRect() const { return Panel().mini; }

    int WarMap::SectorAt(int x, int y) const
    {
        if (!sectorMap) return 0;
        int mx = cameraX + x, my = cameraY + y;
        if (mx < 0 || my < 0 || mx >= sectorMap->width || my >= sectorMap->height) return 0;
        int value = sectorMap->pixels[size_t(my) * size_t(sectorMap->width) + size_t(mx)];
        if (value <= warmap::SectorCount) return warmap::SectorOfValue(value);
        // a border pixel (0x49BF50) belongs to the lowest-numbered sector whose border list has it
        int border = value - 31;
        for (int sector = 1; sector <= warmap::SectorCount; sector++)
            for (int b : warmap::SectorBorders[sector])
                if (border > 0 && b == border) return sector;
        return 0;
    }

    float WarMap::PixelsPerHour(const Counter& k) const
    {
        // 0x424160 gives a new route its arrival time: the route's length in map pixels (+0x68) over the slowest
        // unit's war map speed (unit +0x54, OBJ.ojd records 37/38) x 0.05, in game hours. A counter is one unit
        // type, so it's that type's speed: 100 goes 5 map pixels an hour.
        return k.speed > 0 ? float(k.speed) * 0.05f : cluntingSpeed;
    }

    void WarMap::Log(std::string text)
    {
        SDL_Log("War map message: %s", text.c_str());
        // 0x420DA0: at most 10 lines (0x4BA4FC), gone ones included, so the oldest goes; each lives 5 x 30 updates
        if (log.size() >= 10)
        {
            // a line still scrolling away goes with it
            if (log.front().state == 1 && --logPending <= 0) { logPending = 0; logScrolling = false; logScroll = 0; }
            log.erase(log.begin());
        }
        log.push_back({ std::move(text), LogLife, 0, uint32_t(game.ticks) });
    }

    // The message list's tick (0x420CD0): live lines age (0x428A00); one that runs out starts the list scrolling up a
    // pixel an update, and once it has gone up a line (16) the first run-out line is gone and the list drops back.
    void WarMap::AgeLog()
    {
        LogLine* first = nullptr;
        for (LogLine& line : log)
        {
            if (line.state == 0 && line.life > 0)
            {
                line.life--;
                if (line.life == 0) { line.state = 1; logScrolling = true; logPending++; }
            }
            if (!first && line.state == 1) first = &line;
        }
        if (!logScrolling) return;
        if (--logScroll != -16) return; // 0x4BA500
        logScroll = 0;
        if (first) first->state = 2;
        if (--logPending <= 0) { logPending = 0; logScrolling = false; }
    }

    // 0x428A00 / 0x4289D0: a line's life set outright: live while there's any, gone at 0
    void WarMap::SetLife(LogLine& line, int life)
    {
        line.life = std::max(life, 0);
        line.state = line.life > 0 ? 0 : 2;
    }

    // The message history (world +0x6C, 'M'; 0x421150 / 0x421040): opening it gives every line with less than
    // 5 seconds left another 5 seconds, so the last 10 show again; closing takes the 5 seconds off them all.
    void WarMap::ToggleHistory()
    {
        historyOpen = !historyOpen;
        sounds.push_back(historyOpen ? 0x193 : 0x196);
        for (LogLine& line : log)
        {
            if (historyOpen) { if (line.life < LogLife) SetLife(line, line.life + LogLife); }
            else SetLife(line, line.life - LogLife);
        }
        if (historyOpen) { logScroll = 0; logScrolling = false; logPending = 0; }
    }

    // 0x80CC: the last two lines (0x4BA504) shown again for 5 seconds
    void WarMap::ReshowLastMessages()
    {
        int n = 0;
        for (auto it = log.rbegin(); it != log.rend() && n < 2; ++it, ++n)
            if (it->life < LogLife) SetLife(*it, it->life + LogLife);
    }

    // The facility types of each resource kind (0x423920): 0-2 Human, 3-5 Martian.
    static constexpr int FacilityType[6] = { 47, 48, 49, 108, 109, 110 };

    // What one more finished facility of a kind would add to a sector's output (0x424000): with B the sector's
    // deposit, n its finished facilities of the kind and r the drop-off rate, output(n) = n r B / (n + r - 1), and
    // the value is output(n + 1) - output(n); B with none yet.
    float WarMap::ResourceGain(int sector, int kind) const
    {
        auto it = deposits.find(sector);
        float b = it == deposits.end() ? 0.0f : float(it->second[size_t(kind)]);
        int n = 0;
        for (int type : finished[size_t(sector)]) if (type == FacilityType[kind]) n++;
        if (n == 0) return b;
        const float r = resourceDropOff, fn = float(n);
        return (fn + 1.0f) * r * b / (r + fn) - fn * r * b / (r + fn - 1.0f);
    }

    // What a sector's facilities of a kind make together (0x421D00 over the owner's constructions, each told
    // 0x803C by phase 0): a finished facility makes int(hp / maxHP) x r B / (n + r - 1), r the drop-off rate,
    // B the sector's deposit and n its finished facilities of the kind, and nothing while the sector has no
    // command building. (Facilities aren't damaged on the war map yet, so hp / maxHP is 1.)
    float WarMap::FacilityOutput(int sector, int kind) const
    {
        if (!HasCommandPost(sector)) return 0.0f;
        bool human = (owner[size_t(sector)] == 0) == res.Human();
        if ((kind < 3) != human) return 0.0f; // only the owner's constructions are asked
        int n = 0;
        for (int type : finished[size_t(sector)]) if (type == FacilityType[kind]) n++;
        if (n == 0) return 0.0f;
        auto it = deposits.find(sector);
        float b = it == deposits.end() ? 0.0f : float(it->second[size_t(kind)]);
        const float r = resourceDropOff;
        return float(n) * (r * b / (float(n) + r - 1.0f));
    }

    // How many men a counter's objects carry (each object's battle map part, 0x4216D0: its BMOL list's count);
    // one each for objects not read from a save.
    int WarMap::CrewOf(const Counter& k) const
    {
        int crew = 0, counted = 0;
        for (int ref : k.refs)
        {
            if (ref < 0 || size_t(ref) >= table.size()) continue;
            const SaveObject& o = table[size_t(ref)];
            size_t at = o.SeloAt() + 24;
            if (o.bytes.size() >= at + 8 && std::equal(o.bytes.begin() + std::ptrdiff_t(at), o.bytes.begin() + std::ptrdiff_t(at + 4), "BMOL"))
                crew += int(uint32_t(o.bytes[at + 4]) | uint32_t(o.bytes[at + 5]) << 8 | uint32_t(o.bytes[at + 6]) << 16 | uint32_t(o.bytes[at + 7]) << 24);
            else crew++;
            counted++;
        }
        return crew + std::max(0, k.count - counted);
    }

    // The sectors' cached paths (WM_COMMAND 0xB7 to each sector, 0x43AFEF: new WSectorPath(owner, this, n) for
    // n = 1-30, 0x4A39E0). The A* (warmap::AStar) over land links only, with the cost
    // (0x4A3D10): impassable into a sector held by neither the path's side (the start's owner) nor no one, when
    // neither end has its command building, or into one without it that isn't the goal; otherwise the whole
    // pixels between the two waypoints. A path keeps its sector count (the start included), or 0 when there's
    // none or it's only the start. The game sends 0xB7 when a construction is finished or a sector changes
    // hands (0x418890); here they're rebuilt when either has changed.
    void WarMap::BuildSectorPaths()
    {
        std::array<int, warmap::SectorCount + 1> from{};
        for (int s = 1; s <= warmap::SectorCount; s++) from[size_t(s)] = owner[size_t(s)] * 2 + (HasCommandPost(s) ? 1 : 0);
        if (pathsBuilt && from == pathsBuiltFrom) return;
        pathsBuilt = true;
        pathsBuiltFrom = from;
        for (auto& row : pathLength) row.fill(0);
        for (int start = 1; start <= warmap::SectorCount; start++)
        {
            int side = owner[size_t(start)];
            for (int goal = 1; goal <= warmap::SectorCount; goal++)
            {
                if (goal == start) continue;
                auto cost = [&](int a, int b) {
                    if (a == b) return 0;
                    if (!(warmap::RouteLinkKind(a, b) & 1)) return -1;
                    if (owner[size_t(b)] != side && owner[size_t(b)] != 2) return -1;
                    bool ca = HasCommandPost(a), cb = HasCommandPost(b);
                    if (!ca && !cb) return -1;
                    if (!cb && b != goal) return -1;
                    auto pa = warmap::Waypoint(a), pb = warmap::Waypoint(b);
                    int dx = pa.x - pb.x, dy = pa.y - pb.y;
                    return int(std::sqrt(double(dx * dx + dy * dy)));
                };
                std::vector<int> path = warmap::AStar(start, goal, cost);
                int count = path.size() > 1 ? int(path.size()) : 0;
                pathLength[size_t(start)][size_t(goal)] = count;
            }
        }
    }

    // The economy (0x41A760), one phase a tick (the tick count % 5, from 0x41AB00), all five when the war map
    // starts (0x418BAF). Each sector's figures for the six resource kinds:
    //   0  supply: what the owner's facilities make (FacilityOutput); 0 in a sector no one holds
    //   1  spread supply: the sector's own, plus every other sector's over (its path's count x VX + 1), plus the
    //      Resource Buffer
    //   2  demand: each object adds its own (0x8143 to the sector's lists, 0x429640): a unit's upkeep (unit
    //      table records 45/46) for each of its men, halved while it stays put, plus its work (records 47/48)
    //      while it repairs or builds (neither is in yet); a travelling unit's goes to the sector it's in now
    //   3  spread demand: the sector's own, plus every other sector's over (its path's count x VY + 1)
    //   4  ratio: spread demand over spread supply, at least 1; and table 1, the Construction view, from the
    //      player's three kinds: 1 - 1 / (r0 r1 r2) (byte_58913C, a cheat, makes it 0), with its least and most
    void WarMap::EconomyPhase(int phase)
    {
        constexpr int N = warmap::SectorCount;
        switch (phase)
        {
        case 0:
            for (int s = 1; s <= N; s++)
                for (int k = 0; k < 6; k++)
                    economy[size_t(s)].supply[size_t(k)] = owner[size_t(s)] == 2 ? 0.0f : FacilityOutput(s, k);
            break;
        case 1:
        case 3:
        {
            BuildSectorPaths();
            const bool supply = phase == 1;
            const float cripple = supply ? crippleX : crippleY;
            for (int s = 1; s <= N; s++)
                for (int k = 0; k < 6; k++)
                {
                    SectorEconomy& e = economy[size_t(s)];
                    float sum = supply ? e.supply[size_t(k)] : e.demand[size_t(k)];
                    for (int j = 1; j <= N; j++)
                    {
                        int count = pathLength[size_t(s)][size_t(j)];
                        if (count == 0) continue;
                        const SectorEconomy& o = economy[size_t(j)];
                        sum += (supply ? o.supply[size_t(k)] : o.demand[size_t(k)]) / (float(count) * cripple + 1.0f);
                    }
                    if (supply) e.effSupply[size_t(k)] = sum + resourceBuffer;
                    else e.effDemand[size_t(k)] = sum;
                }
            break;
        }
        case 2:
            for (SectorEconomy& e : economy) e.demand.fill(0.0f);
            for (const Counter& k : counters)
            {
                if (k.kind != 0 || k.sector < 1 || k.sector > N) continue;
                // a travelling unit's goes to the sector it last reached (0x424E90)
                const int at = k.Travelling() ? k.path[size_t(k.segment)] : k.sector;
                auto unit = unitData.find(k.type);
                if (unit == unitData.end()) continue;
                const bool human = k.players == res.Human();
                const float crew = float(CrewOf(k)), still = k.Travelling() ? 1.0f : 0.5f;
                for (int i = 0; i < 3; i++)
                    economy[size_t(at)].demand[size_t((human ? 0 : 3) + i)] += unit->second.upkeep[i] * still * crew;
            }
            break;
        case 4:
        {
            const int base = res.Human() ? 0 : 3;
            float lo = 1e6f, hi = -1e6f;
            for (int s = 1; s <= N; s++)
            {
                SectorEconomy& e = economy[size_t(s)];
                for (int k = 0; k < 6; k++)
                    e.ratio[size_t(k)] = std::max(1.0f, (e.effDemand[size_t(k)] - e.effSupply[size_t(k)]) / e.effSupply[size_t(k)] + 1.0f);
                float product = e.ratio[size_t(base)] * e.ratio[size_t(base + 1)] * e.ratio[size_t(base + 2)];
                float v = 1.0f - 1.0f / product;
                if (v > hi) hi = v;
                if (v < lo) lo = v;
                efficiency[size_t(s)] = v;
            }
            efficiencyMin = lo;
            efficiencyMax = hi;
            break;
        }
        }
    }

    // The sectors' shading (0x4192F0 fills five tables, 0x41D9B0 turns them into levels of the map's shade table).
    // Table 0, the Unit view: the objects in each sector of ours (dword_4F7650, one more for each object put in
    // it), 0 for the rest, onto levels 17-35. Tables 2-4, the Resource view: what another facility of the side's
    // kind 0-2 would give in every sector, onto 78-88, the top fixed at 80. A sector of ours takes
    // lo + (v - min) / (max - min) x (hi - lo); in the Resource view so does every other; otherwise a sector no one
    // holds is at 11, and the other side's is 11 too except in the Unit view, where it's 0 (dark). Table 1, the
    // Construction view, is the economy's strain on each sector (EconomyPhase 4) onto 67-77, from its least to 1.0.
    void WarMap::ShadeSectors()
    {
        levels.fill(warmap::NormalLevel);
        std::array<float, warmap::SectorCount + 1> v{};
        if (viewMode == 2) v = efficiency;
        if (viewMode == 1)
            for (const Counter& k : counters)
                if (k.sector >= 1 && k.sector <= warmap::SectorCount && owner[size_t(k.sector)] == 0) v[size_t(k.sector)] += float(std::max(k.count, 1));
        if (viewMode == 3)
        {
            int kind = (res.Human() ? 0 : 3) + std::clamp(resourceKind, 0, 2);
            for (int s = 1; s <= warmap::SectorCount; s++) v[size_t(s)] = ResourceGain(s, kind);
        }
        float lo = 1e6f, hi = -1e6f;
        for (int s = 1; s <= warmap::SectorCount; s++) { lo = std::min(lo, v[size_t(s)]); hi = std::max(hi, v[size_t(s)]); }
        if (viewMode == 3) hi = 80.0f;
        if (viewMode == 2) { lo = efficiencyMin; hi = 1.0f; }
        float range = hi - lo;
        if (range == 0.0f) range = 1.0f;
        const int first = viewMode == 1 ? 17 : viewMode == 2 ? 67 : 78, last = viewMode == 1 ? 35 : viewMode == 2 ? 77 : 88;
        for (int s = 1; s <= warmap::SectorCount; s++)
        {
            int level;
            if (owner[size_t(s)] == 0 || viewMode == 3) level = int((v[size_t(s)] - lo) / range * float(last - first) + float(first));
            else if (owner[size_t(s)] == 2) level = warmap::NormalLevel;
            else level = 0;
            levels[size_t(s)] = level;
        }
        PulseBattles();
    }

    // A sector with a battle in it pulses (0x41DE70): the view's frame count 0-22 as a ramp up and down 0-11-0, on
    // 36 for ours and 47 for the rest.
    void WarMap::PulseBattles()
    {
        int frame = animation % 23, ramp = frame > 11 ? 22 - frame : frame;
        for (int s = 1; s <= warmap::SectorCount; s++)
            if (battle[size_t(s)]) levels[size_t(s)] = 36 + (owner[size_t(s)] != 0 ? 11 : 0) + ramp;
    }

    // The calendar's tick (0x41F6C0): an hour asked for by a click on the calendar ("Advance time", +0x30), or
    // else the speed's hours a second over the 30 updates of a second; whole hours move the date on (0x41B770).
    // Returns the game hours that went by.
    float WarMap::ClockTick()
    {
        float before = game.ticks;
        if (forcedHours > 0) { forcedHours--; game.ticks += 1.0f; }
        else game.ticks += warmap::Speeds[speed] / 30.0f;
        int hours = int(game.ticks) - int(before);
        if (hours > 0) warmap::AdvanceDate(game.dateTime, hours);
        hoursPassed = hours;
        return game.ticks - before;
    }

    // The world's 0x802D, every whole game hour (0x419E9D): the routes, the players, both sides' research, the
    // pods, each sector, then the resource warnings. (The routes, the pods and the warnings are in so far.)
    void WarMap::Hourly(int hours)
    {
        MoveRoutes(hours);
        DropPod(hours);
        ResourceWarnings();
    }

    // Resource warnings (0x419EFB): for each of the player's three kinds, the least spread supply over spread
    // demand among the player's sectors (starting from 1, so a sector only counts below that). Under 0.3 a
    // counter (0x4E9BA0) goes up one a call, and past 336 raises event 14, ResourceAlert, and clears both
    // counters; otherwise, under 0.8, another (0x4E9BB0) goes up, and past 504 raises event 13,
    // ResourceWarning, and is cleared. They count calls, not hours. The events (0x41F280 / 0x41F340): "Resource
    // Warning: <the resource>" (0x41B, 0x214 + kind) in the message list with the kind's sound (0x4BA1D8 /
    // 0x4BA1F0); with the [Event] option on, the game also opens its event box (not in yet).
    void WarMap::ResourceWarnings()
    {
        static constexpr int WarningSound[6] = { 0x124, 0x122, 0x123, 0x125, 0x126, 0x127 };
        static constexpr int AlertSound[6] = { 0x12A, 0x128, 0x129, 0x12B, 0x12C, 0x12D };
        const int base = res.Human() ? 0 : 3;
        float least[3] = { 1.0f, 1.0f, 1.0f };
        for (int s = 1; s <= warmap::SectorCount; s++)
        {
            if (owner[size_t(s)] != 0) continue;
            const SectorEconomy& e = economy[size_t(s)];
            for (int k = 0; k < 3; k++)
            {
                float r = e.effSupply[size_t(base + k)] / e.effDemand[size_t(base + k)];
                if (r < least[k]) least[k] = r;
            }
        }
        for (int k = 0; k < 3; k++)
        {
            int sound = 0;
            if (least[k] < 0.3f)
            {
                if (++alertCount[k] > 336) { sound = AlertSound[base + k]; alertCount[k] = 0; warningCount[k] = 0; }
            }
            else if (least[k] < 0.8f)
            {
                if (++warningCount[k] > 504) { sound = WarningSound[base + k]; warningCount[k] = 0; }
            }
            if (!sound) continue;
            Log(res.String(0x41B) + ": " + res.String(0x214 + base + k));
            sounds.push_back(sound);
        }
    }

    // Martian pods (0x41A330): while fewer than 8 have come, every [Tweak] "Pod Interval (hours)" (24) one lands in
    // sector 2, 4, 6, 7, 8, 9, 10 or 11 (byte_4B9DE8[rand() & 7]) carrying one of four loads (off_4B9E28[rand() % 4]:
    // a ScoutMachineMk1, 62, alone or with a HandlingMachineMk1, 89, or a DiggingMechanismMk1, 91), added to the
    // sector (0x43CEB0). In a Human sector it marks the sector (+0x64) for the battle check (0x41AB00); otherwise
    // it's event 12, PodLanded (0x41F010): in a Martian sector sound 0x1B2 and "Pod landed: <sector>" (0x41E); in
    // one no one holds a battle there (+0x124), sound 0x1B3 and "Pod landed in our territory: <sector>" (0x420) in
    // a Human game, "Pod landed in enemy sector: ..." (0x41F) in a Martian one, with the battle to follow (0x8072,
    // not in yet). A Martian game switches to the Unit view first (0x41A730).
    void WarMap::DropPod(int hours)
    {
        if (pods >= 8) return;
        podHours += hours;
        if (podHours < podInterval) return;
        podHours -= podInterval;
        static constexpr int Sectors[8] = { 2, 4, 6, 7, 8, 9, 10, 11 };
        static const std::vector<int> Loads[4] = { { 62 }, { 62, 89 }, { 62, 91 }, { 62, 89 } };
        int sector = Sectors[random::Rand() & 7];
        const std::vector<int>& load = Loads[random::Rand() % 4];
        const bool human = res.Human();
        for (int type : load) AddUnit(type, sector, !human);
        pods++;
        // who holds it, as Human 0 / Martian 1 / no one 2
        int who = owner[size_t(sector)] == 2 ? 2 : (owner[size_t(sector)] == 0) == human ? 0 : 1;
        if (who == 0) { podFlag[size_t(sector)] = true; return; }
        if (!human) SetView(1);
        std::string name = res.String(warmap::NameText(sector));
        if (who == 1)
        {
            sounds.push_back(0x1B2);
            Log(res.String(0x41E) + ": " + name);
            return;
        }
        battle[size_t(sector)] = true;
        sounds.push_back(0x1B3);
        Log(res.String(human ? 0x420 : 0x41F) + ": " + name);
    }

    // A unit put in a sector (0x43CEB0): its own counter in the sector's next free cell. With a save behind the map
    // it also gets a saved record: a copy of one of the same type (its battle map units with it), with new ids
    // (the lowest free ones, as the game hands them out).
    void WarMap::AddUnit(int type, int sector, bool players)
    {
        int ref = -1;
        if (save)
        {
            const SaveObject* model = nullptr;
            for (const SaveObject& o : table) if (o.list < 2 && o.Type() == type) { model = &o; break; }
            if (model)
            {
                SaveObject o = *model;
                o.list = players ? 0 : 1;
                // the ids in use: the sectors' own data as saved, and every object the map has now
                std::vector<bool> used = save->UsedIds();
                for (const SaveObject& t : table) SaveFile::MarkIds(t.bytes, used);
                for (size_t at : o.IdOffsets())
                {
                    int id = SaveFile::TakeId(used);
                    for (int b = 0; b < 4; b++) o.bytes[at + size_t(b)] = uint8_t(uint32_t(id) >> (8 * b));
                }
                o.SetSector(sector);
                table.push_back(std::move(o));
                ref = int(table.size()) - 1;
            }
            else SDL_Log("War map: no saved unit of type %d to copy, so the new one won't be saved", type);
        }
        AddCounter(type, sector, players, ref);
    }

    int WarMap::AddCounter(int type, int sector, bool players, int ref)
    {
        auto unit = unitData.find(type);
        Counter k;
        k.frame = unit == unitData.end() ? 0 : unit->second.counterFrame;
        k.speed = unit == unitData.end() ? 0 : unit->second.warMapSpeed;
        k.moveClass = unit == unitData.end() ? 0 : unit->second.moveClass;
        k.count = 1;
        k.type = type;
        k.players = players;
        k.kind = 0;
        k.sector = sector;
        if (ref >= 0) k.refs.push_back(ref);
        counters.push_back(k);
        int index = int(counters.size()) - 1;
        // added to the sector (0x43CEB0 -> 0x43CD88): placed when the view shows units
        if (viewMode == 1) PlaceCounter(index);
        return index;
    }

    std::vector<SaveInfo::WarMapObject> WarMap::ObjectsOf(const SaveFile& save)
    {
        std::vector<SaveInfo::WarMapObject> objects;
        int ref = 0;
        for (int sector = 1; sector <= warmap::SectorCount; sector++)
            for (const auto& list : save.Sector(sector).lists)
                for (const SaveObject& o : list)
                {
                    SaveInfo::WarMapObject w;
                    w.list = o.list;
                    w.unitType = o.Type();
                    w.id = o.Id();
                    w.x = o.X();
                    w.y = o.Y();
                    w.sector = o.Sector();
                    w.value[0] = o.Value(0);
                    w.value[1] = o.Value(1);
                    w.ref = ref++;
                    objects.push_back(w);
                }
        return objects;
    }

    void WarMap::UseSave(SaveFile from)
    {
        table.clear();
        for (int sector = 1; sector <= warmap::SectorCount; sector++)
            for (const auto& list : from.Sector(sector).lists)
                for (const SaveObject& o : list) table.push_back(o);
        // the message list as it was (0x420EE0)
        log.clear();
        for (const SaveMessage& m : from.messages) log.push_back({ m.text, int(m.life), int(m.state), m.hour });
        podHours = int(from.podHours);
        pods = int(from.pods);
        // units that were on their way: back on the map where they set out from, and sent on again
        std::vector<SaveRoute> routes = std::move(from.routes);
        from.routes.clear();
        save = std::move(from);
        for (const SaveRoute& route : routes)
            for (const SaveObject& o : route.objects)
            {
                table.push_back(o);
                int k = AddCounter(o.Type(), int(route.origin), o.list == 0, int(table.size()) - 1);
                if (k >= 0 && route.destination >= 1 && int(route.destination) <= warmap::SectorCount) SendTo(k, int(route.destination));
            }
    }

    // The war map written back into the save it came from (0x41AD80): the view, the clock and date, the message
    // list, the pods, who holds each sector, and every object in the sector it's in now (the first of a counter
    // that stands for several at the counter's place, the rest at 0, 0, as the game keeps them); units on their way
    // go in a route each (origin, destination, the sectors between, and the attack flag), as 0x425A60 writes them.
    std::optional<std::vector<uint8_t>> WarMap::SaveData() const
    {
        if (!save) return std::nullopt;
        SaveFile s = *save;
        s.warMap[0] = uint32_t(cameraX);
        s.warMap[1] = uint32_t(cameraY);
        s.warMap[2] = uint32_t(selected);
        s.warMap[3] = uint32_t(viewMode);
        s.hours = game.ticks;
        for (int i = 0; i < 6; i++) s.dateTime[i] = game.dateTime[i];
        s.messages.clear();
        for (const LogLine& line : log) s.messages.push_back({ line.text, uint32_t(line.life), uint32_t(line.state), line.hour });
        s.podHours = uint32_t(podHours);
        s.pods = uint32_t(pods);
        const bool human = res.Human();
        for (int sector = 1; sector <= warmap::SectorCount; sector++)
        {
            int rel = owner[size_t(sector)];
            s.Sector(sector).SetOwner(rel == 2 ? 2 : human ? rel : 1 - rel);
            // the sector's economy (0x43C5E0): for each kind its spread supply, spread demand, ratio, demand, 0
            std::vector<uint8_t>& rest = s.Sector(sector).rest;
            if (rest.size() >= 120)
            {
                const SectorEconomy& e = economy[size_t(sector)];
                size_t at = rest.size() - 120;
                for (int k = 0; k < 6; k++)
                    for (float f : { e.effSupply[size_t(k)], e.effDemand[size_t(k)], e.ratio[size_t(k)], e.demand[size_t(k)], 0.0f })
                    {
                        uint32_t b;
                        std::memcpy(&b, &f, 4);
                        for (int i = 0; i < 4; i++) rest[at++] = uint8_t(b >> (8 * i));
                    }
            }
            for (auto& list : s.Sector(sector).lists) list.clear();
        }
        s.routes.clear();
        std::vector<bool> written(table.size(), false);
        for (const Counter& k : counters)
        {
            if (k.sector < 1 || k.sector > warmap::SectorCount) continue;
            SaveRoute* route = nullptr;
            if (k.Travelling())
            {
                SaveRoute r;
                r.origin = uint32_t(k.path.front());
                r.destination = uint32_t(k.path.back());
                r.flag5F = 1;
                r.flag5E = k.attack ? 1 : 0;
                for (int sector : k.path) r.path.push_back(uint32_t(sector));
                r.field50 = k.naval ? 1 : 0;
                s.routes.push_back(std::move(r));
                route = &s.routes.back();
            }
            for (size_t i = 0; i < k.refs.size(); i++)
            {
                int ref = k.refs[i];
                if (ref < 0 || size_t(ref) >= table.size() || written[size_t(ref)]) continue;
                written[size_t(ref)] = true;
                SaveObject o = table[size_t(ref)];
                // an object keeps the place it was saved with (0, 0 for most: the game finds them a cell) unless
                // its counter has moved; then the first takes the counter's place and the rest 0, 0
                if (k.moved || route)
                {
                    o.SetSector(k.sector);
                    if (k.refs.size() > 1 && i > 0) o.SetPosition(0, 0);
                    else o.SetPosition(k.x, k.y);
                }
                if (route) route->objects.push_back(std::move(o));
                else s.Sector(k.sector).lists[size_t(o.list)].push_back(std::move(o));
            }
        }
        // anything with no counter (a sector too full to show it) stays where it was
        for (size_t i = 0; i < table.size(); i++)
        {
            if (written[i]) continue;
            int sector = table[i].Sector();
            if (sector >= 1 && sector <= warmap::SectorCount) s.Sector(sector).lists[size_t(table[i].list)].push_back(table[i]);
        }
        return s.Write();
    }

    // The routes' hourly step (WWarMapRoute's 0x802D, 0x425080), for each counter on its way:
    //   - its first hour puts it at the curve's start, plus its place in the formation (0x4406A0)
    //   - unless it's interrupted already or an attack, every sector from the one it last reached to the end is
    //     checked: one with a battle (+0x124), or held by the other side, other than where the route started,
    //     interrupts it (+0x60) with event 11, TravelInterrupted (0x41EF80: "Travel interrupted" (0x419), sound
    //     0x147; with the [Event] option on the game also opens its event box, not in yet)
    //   - it goes the way still to go over the hours still to go each hour, waypoint to waypoint (0x437380); on
    //     reaching a sector's waypoint it arrives there if that's the last, or the route was interrupted
    //   - on the map it's that far along the arrow's curve (32 points a sector), and the arrow is drawn from there
    //     on (+0x5C)
    //   - when every counter on it has arrived, a route that isn't an attack raises event 3, ArrivedInSector
    void WarMap::MoveRoutes(int hours)
    {
        constexpr int Steps = 32; // the curve's points a sector (+0x40: 2 to the power +0x44, 5)
        const float h = float(hours);
        for (size_t i = 0; i < counters.size(); i++)
        {
            Counter& k = counters[i];
            if (!k.Travelling() || k.arrow.size() < 2) continue;
            const int origin = k.path.front(), last = int(k.path.size()) - 1;
            if (++k.hoursOn == 1)
            {
                FreeCounter(int(i)); // 0x4208B0: its cell given back as it sets off
                k.x = int(k.arrow[0].x) + k.offsetX - 16;
                k.y = int(k.arrow[0].y) + k.offsetY - 16;
            }
            if (!k.interrupted && !k.attack)
                for (int n = k.segment; n <= last; n++)
                {
                    int s = k.path[size_t(n)];
                    if ((battle[size_t(s)] || owner[size_t(s)] == 1) && s != origin)
                    {
                        k.interrupted = true;
                        Log(res.String(0x419));
                        sounds.push_back(0x147);
                        break;
                    }
                }
            // (with no hours left the game divides by 0; here it just gets there)
            float speed = k.hoursLeft > 0.0f ? k.remaining / k.hoursLeft : k.segmentLeft + k.remaining + 1.0f;
            k.hoursLeft -= h;
            float d = speed * h;
            k.segmentLeft -= d;
            k.remaining -= d;
            if (k.segmentLeft <= 0.0f)
            {
                k.segment++;
                k.segmentStarted = true;
                if (k.segment == last || k.interrupted)
                {
                    Arrive(int(i), k.path[size_t(k.segment)], k.path[size_t(k.segment - 1)]);
                    continue;
                }
                auto a = warmap::Waypoint(k.path[size_t(k.segment)]), b = warmap::Waypoint(k.path[size_t(k.segment + 1)]);
                int dx = a.x - b.x, dy = a.y - b.y;
                k.segmentLength = float(std::sqrt(double(dx * dx + dy * dy)));
                k.segmentLeft += k.segmentLength;
            }
            float frac = 1.0f - k.segmentLeft / k.segmentLength;
            if (frac >= 0.5f && k.segmentStarted) k.segmentStarted = false;
            float f = frac * float(Steps);
            int whole = int(f);
            // (the game reads past the curve when an hour goes beyond the next waypoint; kept on it here)
            int at = std::clamp(k.segment * Steps + whole, 0, int(k.arrow.size()) - 2);
            k.trim = at;
            const auto& p0 = k.arrow[size_t(at)];
            const auto& p1 = k.arrow[size_t(at + 1)];
            float part = f - float(whole);
            k.x = int(p0.x + (p1.x - p0.x) * part) + k.offsetX - 16;
            k.y = int(p0.y + (p1.y - p0.y) * part) + k.offsetY - 16;
        }
    }

    // A counter arriving (0x425471: 0xAD to the group with the sector, +0x58 the one before, then the entry goes),
    // and the route ending with it (0x4253F9). Going into a sector the other side holds is a battle, which needs
    // the battle map: "Battle in" and the sector with the attack sound (0x139, or 0x13A for sector 3 or London),
    // and the counter stays in the sector it came from (provisional). Arriving in a sector no one holds takes it
    // (provisional).
    void WarMap::Arrive(int index, int sector, int previous)
    {
        Counter& k = counters[size_t(index)];
        const bool attack = k.attack;
        int at = sector;
        if (owner[size_t(sector)] == 1)
        {
            Log(res.String(0x210) + " " + res.String(warmap::NameText(sector)));
            sounds.push_back(sector == 3 || sector == 30 ? 0x13A : 0x139);
            at = previous;
        }
        else if (owner[size_t(sector)] == 2) owner[size_t(sector)] = 0;
        k.path.clear();
        k.arrow.clear();
        k.sector = at;
        // the sector laid out again (0x43CB60), this one with the rest
        RefreshSector(at);
        // ArrivedInSector (event 3, 0x41EAE0): "Vehicle/s arrived in" and the sector, sound 0x120
        if (!attack && at == sector)
        {
            Log(res.String(0x211) + " " + res.String(warmap::NameText(sector)));
            sounds.push_back(0x120);
        }
    }

    // Attacks on the same sector arrive together (the route manager's 0x41FCF0, after a route is added): the
    // longest hours still to go of every route heading there go to each attack heading there (0x4255F0, its label
    // too).
    void WarMap::ArriveTogether()
    {
        std::array<float, warmap::SectorCount + 1> longest{};
        for (const Counter& k : counters)
            if (k.Travelling()) longest[size_t(k.path.back())] = std::max(longest[size_t(k.path.back())], k.hoursLeft);
        for (Counter& k : counters)
            if (k.Travelling() && k.attack)
            {
                k.hoursLeft = longest[size_t(k.path.back())];
                k.labelHours = k.hoursLeft;
                for (int i = 0; i < 6; i++) k.labelFrom[i] = game.dateTime[i];
            }
    }

    void WarMap::Update()
    {
        AgeLog();
        if (!map) map = res.GetFrame(warmap::MapGType);
        if (!sectorMap) sectorMap = res.GetFrame(warmap::SectorMapGType);

        if (!economyStarted)
        {
            economyStarted = true;
            for (int phase = 0; phase < 5; phase++) EconomyPhase(phase);
        }
        animation++;
        SlideCounters();
        ClockTick();
        if (hoursPassed > 0) Hourly(hoursPassed);
        EconomyPhase(animation % 5);
        ShadeSectors();
        TurnCalendar();

        int dx = 0, dy = 0;
        if (mouseX >= 0 && !dragging)
        {
            if (mouseX < EdgeZone) dx -= scrollStep;
            if (mouseX >= ScreenW - EdgeZone) dx += scrollStep;
            if (mouseY < EdgeZone) dy -= scrollStep;
            if (mouseY >= ScreenH - EdgeZone) dy += scrollStep;
        }
        if (dx || dy) { cameraX += dx; cameraY += dy; Clamp(); }
        PanelLayout panel = Panel();
        hotButton = -1;
        for (int i = 0; i < 3; i++) if (panel.buttons[i].Contains(mouseX, mouseY)) hotButton = i;
        if (panel.research.Contains(mouseX, mouseY)) hotButton = 3;
        bool overPanel = panel.mini.Contains(mouseX, mouseY) || hotButton >= 0 || (panel.frame.Contains(mouseX, mouseY) && res.Human());
        hovered = mouseX >= 0 && !overPanel ? SectorAt(mouseX, mouseY) : 0;
    }

    void WarMap::Draw(Canvas& canvas) const
    {
        const IndexedImage* m = map;
        const IndexedImage* s = sectorMap;
        const ShadeTable* shade = res.ShadeFor(warmap::MapGType);
        if (m && s && shade)
        {
            // every pixel takes its colour from the level its sector map value has
            std::array<int, 256> lv = levels;
            // every border, from the owners either side (a coastline has the sea on its other side)
            std::array<int, 128> sideA{}, sideB{};
            for (int sector = 1; sector <= warmap::SectorCount; sector++)
                for (int border : warmap::SectorBorders[sector])
                    if (border > 0 && border < 128) (sideA[size_t(border)] ? sideB : sideA)[size_t(border)] = sector;
            for (int border = 1; border < 128 && border + 31 < 256; border++)
            {
                int a = sideA[size_t(border)], b = sideB[size_t(border)];
                if (!a) continue;
                int level = PlainBorderLevel;
                if (selected && (a == selected || b == selected)) level = SelectedBorderLevel;
                else if (hovered && (a == hovered || b == hovered)) level = HoverBorderLevel;
                else
                {
                    int oa = owner[size_t(a)], ob = b ? owner[size_t(b)] : 2;
                    if (oa != ob)
                    {
                        if (oa == 2 || ob == 2) level = (oa == 0 || ob == 0) ? OwnBorderLevel : EnemyBorderLevel;
                        else level = FrontLevel;
                    }
                }
                lv[size_t(border + 31)] = level;
            }
            const uint32_t* palettes[256];
            for (int v = 0; v < 256; v++) palettes[v] = shade->Level(lv[size_t(v)]);
            uint32_t* out = canvas.Pixels();
            int w = std::min(canvas.Width(), m->width - cameraX), h = std::min(canvas.Height(), m->height - cameraY);
            for (int y = 0; y < h; y++)
            {
                const uint8_t* src = &m->pixels[size_t(cameraY + y) * size_t(m->width) + size_t(cameraX)];
                const uint8_t* sec = &s->pixels[size_t(cameraY + y) * size_t(s->width) + size_t(cameraX)];
                uint32_t* dst = out + size_t(y) * size_t(canvas.Width());
                for (int x = 0; x < w; x++) dst[x] = palettes[sec[x]][src[x]];
            }
        }

        // Routes: an arrow from each travelling counter through the rest of its points
        for (const Counter& k : counters)
            if (k.Travelling() && k.kind == viewMode - 1) DrawRouteArrow(canvas, k);

        // the counters (GType 0x77, 32 pixels) of the kind the view shows, with how many when there's more than one
        for (const Counter& k : counters)
        {
            if (k.kind != viewMode - 1) continue; // each view shows its own kind
            int x = k.x - cameraX, y = k.y - cameraY;
            if (x < -40 || y < -40 || x > canvas.Width() || y > canvas.Height()) continue;
            canvas.DrawSprite(0x77, k.frame, x, y);
            if (&k - counters.data() == chosen) canvas.DrawSprite(0x54, 0, x, y); // cselecth.spr
            // the count, in the counter font (GType 0x6E, hcntfnt.fnt; where the game puts it isn't traced yet)
            if (k.count > 1) // where the game's text object sits on the counter isn't traced
                canvas.DrawString(0x6E, std::to_string(k.count), { x, y + 22, 30, 10 }, TextRight);
        }

        // the panel: its frame, the three view buttons (frame 2 for the one that's on, 1 under the mouse; the
        // buttons' frames aren't confirmed), then the mini-map: the whole map at a fifteenth, the sector under the
        // mouse lit, and the view's outline
        PanelLayout panel = Panel();
        if (panelShown)
        {
            canvas.DrawSprite(0x80, 0, panel.frame.x, panel.frame.y);
            static constexpr int ButtonGtypes[3] = { 0x33, 0x35, 0x34 };
            for (int i = 0; i < 3; i++)
                canvas.DrawSprite(ButtonGtypes[i], viewMode == i + 1 ? 2 : hotButton == i ? 1 : 0, panel.buttons[i].x, panel.buttons[i].y);
            // a push button's frames (0x433DB0): 0, 1 under the mouse, 2 held under the mouse, 3 held off it
            int researchFrame = researchHeld ? (hotButton == 3 ? 2 : 3) : hotButton == 3 ? 1 : 0;
            canvas.DrawSprite(0x341, researchFrame, panel.research.x, panel.research.y);
            Rect mini = panel.mini;
            canvas.DrawSprite(warmap::MiniMapGType, 0, mini.x, mini.y, warmap::NormalLevel); // slot 6, like the map
            int lit = hovered ? hovered : selected;
            if (lit) canvas.DrawSprite(warmap::MiniSectorGType, lit, mini.x, mini.y);
            {
                uint32_t* out = canvas.Pixels();
                int x0 = mini.x + cameraX * MiniW / warmap::MapWidth, y0 = mini.y + cameraY * MiniH / warmap::MapHeight;
                int x1 = mini.x + (cameraX + ViewW()) * MiniW / warmap::MapWidth, y1 = mini.y + (cameraY + ViewH()) * MiniH / warmap::MapHeight;
                const uint32_t colour = 0xFFFFFF;
                auto plot = [&](int x, int y) { if (x >= 0 && y >= 0 && x < canvas.Width() && y < canvas.Height()) out[size_t(y) * size_t(canvas.Width()) + size_t(x)] = colour; };
                for (int x = x0; x <= x1; x++) { plot(x, y0); plot(x, y1); }
                for (int y = y0; y <= y1; y++) { plot(x0, y); plot(x1, y); }
            }
        }

        // the message list
        {
            // 0x420C10: from (0x28, 4) plus the scroll, 16 apart, the live lines and the one scrolling away
            int y = 4 + logScroll;
            for (const LogLine& line : log)
                if (line.life > 0 || line.state == 1) { canvas.DrawString(0x72, line.text, { 0x28, y, ScreenW - 0x28, 14 }, 0); y += 16; }
        }

        // the sector under the mouse or the panel button's name, and the engine's own notes (provisional: the game
        // shows these its own way, not traced)
        std::string status;
        static constexpr int ButtonText[4] = { 0x47B, 0x47C, 0x47E, 0x47D }; // "Unit View", "Construction view", "Resource view", "View Research"
        if (!message.empty()) status = message;
        if (hotButton >= 0) status = res.String(ButtonText[hotButton]);
        else if (hovered) status = res.String(warmap::NameText(hovered));
        DrawCalendar(canvas);
        {
            // a darkened strip behind the text so it reads on either map
            uint32_t* out = canvas.Pixels();
            int rows = status.empty() ? 0 : 16;
            for (int y = ScreenH - 20; y < ScreenH - 20 + rows; y++)
                for (int x = 4; x < 240; x++)
                {
                    uint32_t& p = out[size_t(y) * size_t(canvas.Width()) + size_t(x)];
                    p = (p >> 2) & 0x3F3F3F;
                }
        }
        if (!status.empty()) canvas.DrawString(0x72, status, { 8, ScreenH - 18, 300, 14 }, 0, colours.items);
    }

    // The keys, as the war map view (0x41C5E0 and 0x41D640) and the world (0x41A450) take them. Each press (and
    // each repeat while held) is one step, as with WM_KEYDOWN.
    // The right button on the map (0x41C5E0): let go within [Tweak] "Quick Cancel Timeout" (5) updates of pressing
    // it, without Ctrl, it's command 0x73, Cancel: the counter picked to move is let go. (Held longer it opens the
    // "Warmap menu" popup, 0x41D120, which isn't in yet.)
    std::optional<Command> WarMap::OnRightButton(int, int, bool down)
    {
        if (down) { rightDownAt = animation; return std::nullopt; }
        bool quick = rightDownAt >= 0 && animation - rightDownAt <= quickCancelTimeout;
        rightDownAt = -1;
        if (quick && !(SDL_GetModState() & SDL_KMOD_CTRL)) chosen = -1;
        return std::nullopt;
    }

    int WarMap::TipOwner(int x, int y) const
    {
        PanelLayout panel = Panel();
        for (int i = 0; i < 3; i++) if (panel.buttons[i].Contains(x, y)) return i;
        if (panel.research.Contains(x, y)) return 3;
        if (CalendarRect().Contains(x, y) && !SpeedRect().Contains(x, y)) return 4;
        return -1;
    }

    std::string WarMap::TipText(int owner, int level) const
    {
        static constexpr int ButtonText[3] = { 0x47B, 0x47C, 0x47E }; // the view buttons' labels (0x433480)
        if (owner >= 0 && owner < 3) return res.String(ButtonText[owner]);
        // the Research button (0x45BEB0): "View Research" and what's being researched, 0x461 with nothing
        if (owner == 3) return res.String(0x47D) + "\n" + res.String(0x461);
        // the calendar (0x4278A0) answers only at the third level: "Advance time"
        if (owner == 4 && level == 3) return res.String(0x481);
        return {};
    }

    std::optional<Command> WarMap::OnKey(int key)
    {
        const bool ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
        // the view's scroll keys send 0x7A with the view's new top-left: steps of the map's size, not the view's
        auto scrollTo = [&](int x, int y) { cameraX = x; cameraY = y; Clamp(); };
        const int w = warmap::MapWidth, h = warmap::MapHeight;
        switch (key)
        {
        // the view (0x41C5E0): F1 Unit view, F2 Construction view, F3-F5 Resource view with each kind
        case SDLK_F1: SetView(1); break;
        case SDLK_F2: SetView(2); break;
        case SDLK_F3: case SDLK_F4: case SDLK_F5: SetView(3); resourceKind = int(key - SDLK_F3); break;
        // 0x41D640
        case SDLK_ESCAPE: return Command{ Commands::Back, 0 };  // 0x80B5, the war map menu
        case SDLK_HOME:                                         // centre on the selected sector (0x41E000, not animated)
            if (selected)
            {
                const warmap::SectorInfo& r = warmap::Sectors[selected];
                CentreOn((r.left + r.right) / 2, (r.top + r.bottom) / 2);
            }
            break;
        case SDLK_PAGEUP: scrollTo(cameraX, 0); break;
        case SDLK_PAGEDOWN: scrollTo(cameraX, h); break;
        case SDLK_UP: scrollTo(cameraX, cameraY - (ctrl ? h / 8 : h / 16)); break;
        case SDLK_DOWN: scrollTo(cameraX, cameraY + (ctrl ? h / 8 : h / 16)); break;
        case SDLK_LEFT: scrollTo(cameraX - (ctrl ? w / 3 : w / 7), cameraY); break;
        case SDLK_RIGHT: scrollTo(cameraX + (ctrl ? w / 3 : w / 7), cameraY); break;
        case SDLK_R: return Command{ ResearchCommand, 0 };     // the research screen
        case SDLK_M: ToggleHistory(); break;                     // the message history (0x6C / 0x6D)
        // the world (0x41A450)
        case SDLK_TAB: panelShown = !panelShown; break;         // the mini-map and its frame (0x42F310)
        case SDLK_SPACE:                                        // Unit -> Construction -> Resource 0, 1, 2 -> Unit
            if (viewMode == 1) SetView(2);
            else if (viewMode == 2) { SetView(3); resourceKind = 0; }
            else if (resourceKind < 2) resourceKind++;
            else SetView(1);
            break;
        default:
            // '0'-'9' set the clock's speed, '0' being 10 (0x41F640)
            if (key >= int(SDLK_0) && key <= int(SDLK_9)) speed = key == SDLK_0 ? 10 : int(key - SDLK_0);
            break;
        }
        return std::nullopt;
    }

    void WarMap::OnKeyUp(int) {}

    void WarMap::OnMouseMove(int x, int y)
    {
        mouseX = x;
        mouseY = y;
        if (draggingSpeed) SetSpeedFromX(x);
        if (dragging)
        {
            Rect mini = MiniMapRect();
            CentreOn((x - mini.x) * warmap::MapWidth / MiniW, (y - mini.y) * warmap::MapHeight / MiniH);
        }
    }

    std::optional<Command> WarMap::OnMouseButton(int x, int y, bool down)
    {
        Rect mini = MiniMapRect();
        PanelLayout panel = Panel();
        for (int i = 0; i < 3; i++)
            if (down && panel.buttons[i].Contains(x, y)) { SetView(i + 1); return std::nullopt; } // 0x811F -> 0x419810
        // the Research button acts when the button comes up over it
        if (down && panel.research.Contains(x, y)) { researchHeld = true; return std::nullopt; }
        if (!down && researchHeld)
        {
            researchHeld = false;
            if (panel.research.Contains(x, y)) return Command{ ResearchCommand, 0 };
            return std::nullopt;
        }
        // the speed slider (a Scrollbar, 0x431E30): pressing it sets the position under the mouse, and moving
        // with the button held drags it until the button comes up, wherever the mouse is by then
        Rect sp = SpeedRect();
        if (!down && draggingSpeed) { draggingSpeed = false; return std::nullopt; }
        if (down && sp.Contains(x, y))
        {
            draggingSpeed = true;
            SetSpeedFromX(x);
            return std::nullopt;
        }
        // a click on the rest of the calendar moves time on an hour at once (0x4278A0: +0x30 and the tick)
        if (down && CalendarRect().Contains(x, y))
        {
            forcedHours++;
            ClockTick();
            if (hoursPassed > 0) Hourly(hoursPassed);
            return std::nullopt;
        }
        if (down && mini.Contains(x, y))
        {
            dragging = true;
            CentreOn((x - mini.x) * warmap::MapWidth / MiniW, (y - mini.y) * warmap::MapHeight / MiniH);
            return std::nullopt;
        }
        if (!down) { dragging = false; return std::nullopt; }
        // a counter of the side being played picks it; with one picked, a sector sends it there
        int counter = CounterAt(x, y);
        if (counter >= 0 && counters[size_t(counter)].players && counters[size_t(counter)].kind == 0)
        {
            chosen = chosen == counter ? -1 : counter;
            return std::nullopt;
        }
        int sector = SectorAt(x, y);
        if (chosen >= 0 && sector) { SendTo(chosen, sector); chosen = -1; return std::nullopt; }
        if (sector) selected = sector;
        return std::nullopt;
    }

    // The calendar ("Calendar View", 0x426C10, drawn by 0x4271E0, frames from 0x427500): GType 0x93 (HU_CALEN.SPR
    // 121 x 89, MA_CALEN.SPR 123 x 83) in the top right corner, 4 pixels in (0x4BA05C), with its parts on top.
    Rect WarMap::CalendarRect() const
    {
        Rect size = SpriteRect(0x93, 0, 0);
        return { ScreenW - size.w - 4, 4, size.w, size.h };
    }

    // The speed slider (0x431E30 with GType 0x97: hu_speed.spr 120 x 58, ma_speed.spr 65 x 26, 22 frames: 11
    // positions, then the same lit), at 0, 0x1F in the Human calendar and 0x30, 0x16 in the Martian one. Its
    // position is the speed (0-10, see warmap::Speeds).
    void WarMap::SetSpeedFromX(int x)
    {
        // the Scrollbar's position (0x431EF0): (max + 1) x (x - left margin) / (length - both margins), no rounding;
        // the margins are 18 and 31 on the Human slider, 8 and 11 on the Martian one
        Rect sp = SpeedRect();
        int left = res.Human() ? 18 : 8, right = res.Human() ? 31 : 11;
        int positions = int(std::size(warmap::Speeds));
        int span = std::max(sp.w - left - right, 1);
        speed = std::clamp(positions * (x - sp.x - left) / span, 0, positions - 1);
    }

    Rect WarMap::SpeedRect() const
    {
        Rect c = CalendarRect();
        Rect size = SpriteRect(0x97, 0, 0);
        bool human = res.Human();
        return { c.x + (human ? 0 : 0x30), c.y + (human ? 0x1F : 0x16), size.w, size.h };
    }

    void WarMap::TurnCalendar()
    {
        CalendarWheels& w = wheels;
        const bool human = res.Human();
        const float hours = game.ticks;
        // the targets (0x427500)
        if (human)
        {
            w.cycle = (w.cycle + 1) % 30;                                       // a frame an update
            w.faceTarget = int((hours - float(int(hours / 12.0f) * 12)) * 8.0f); // 0x4A9E24 = 1/12, 0x4A9E28 = 8
            w.ampmTarget = (int(hours) / 12 & 1) * 5;
            int day = game.Day(), year = game.Year();
            w.digitTarget[0] = (day / 10 % 10) * 5;
            w.digitTarget[1] = (day % 10) * 5;
            w.monthTarget = (game.Month() - 1) * 5;
            w.digitTarget[2] = (year / 1000 % 10) * 5;
            w.digitTarget[3] = (year / 100 % 10) * 5;
            w.digitTarget[4] = (year / 10 % 10) * 5;
            w.digitTarget[5] = (year % 10) * 5;
        }
        else
        {
            w.cycle = (w.cycle + (speed ? 4 : 1)) % 100;
            w.globe = int(hours) % 24;
            // the days gone by to two places: the hours over 24000, 2400 ... 0.24 (0x4BA910)
            static constexpr float Powers[6] = { 24000, 2400, 240, 24, 2.4f, 0.24f };
            for (int i = 0; i < 6; i++) w.digitTarget[i] = (int(hours / Powers[i]) % 10) * 5;
        }
        if (!w.started)
        {
            // a new calendar starts on its targets
            for (int i = 0; i < 6; i++) w.digit[i] = w.digitTarget[i];
            w.ampm = w.ampmTarget; w.month = w.monthTarget; w.face = w.faceTarget;
            w.started = true;
            return;
        }
        // the wheels (0x427710): a digit goes on by its step for the speed (frames mod 50) and stops on its target
        // when it would pass it (going round to 0, once it's within the first 10 frames)
        static constexpr int MartianStep[11][6] = { // 0x4BA6A0
            { 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 2 }, { 1, 1, 1, 1, 1, 4 }, { 1, 1, 1, 1, 1, 5 },
            { 1, 1, 1, 1, 2, 5 }, { 1, 1, 1, 1, 2, 5 }, { 1, 1, 1, 1, 4, 5 }, { 1, 1, 1, 2, 4, 5 },
            { 1, 1, 1, 2, 5, 5 }, { 1, 1, 1, 2, 5, 5 }, { 1, 1, 1, 2, 5, 5 } };
        static constexpr int HumanStep[11][6] = { // 0x4BA7A8
            { 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1 },
            { 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1 },
            { 1, 1, 1, 1, 1, 1 }, { 2, 1, 1, 1, 1, 1 }, { 4, 1, 1, 1, 1, 1 } };
        static constexpr int AmPmStep[11] = { 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3 };        // 0x4BA8B0
        static constexpr int FaceStep[11] = { 2, 3, 3, 5, 6, 9, 13, 17, 21, 25, 35 };  // 0x4BA8E0
        auto roll = [](int& frame, int target, int step) {
            if (frame == target) return;
            frame = (frame + step) % 50;
            if (target == 0 ? frame < 10 : frame > target) frame = target;
        };
        // one frame at a time, up to n, stopping on the target
        auto creep = [](int& frame, int target, int n, int frames) {
            for (; n > 0 && frame != target; n--) frame = (frame + 1) % frames;
        };
        const int s = std::clamp(speed, 0, 10);
        if (!human)
        {
            for (int i = 0; i < 6; i++) roll(w.digit[i], w.digitTarget[i], MartianStep[s][i]);
            return;
        }
        creep(w.digit[0], w.digitTarget[0], HumanStep[s][0], 20); // hu_num03: 0-3
        for (int i = 1; i < 6; i++) roll(w.digit[i], w.digitTarget[i], HumanStep[s][i]);
        creep(w.ampm, w.ampmTarget, AmPmStep[s], 10);
        creep(w.month, w.monthTarget, 1, 60);
        creep(w.face, w.faceTarget, FaceStep[s], 96);
    }

    // The calendar's drawing (0x4271E0).
    void WarMap::DrawCalendar(Canvas& canvas) const
    {
        Rect c = CalendarRect();
        canvas.DrawSprite(0x93, 0, c.x, c.y);
        const CalendarWheels& w = wheels;
        const int s = std::clamp(speed, 0, 10);
        if (res.Human())
        {
            // the clock face: 96 frames over 12 hours, and at the top speeds (an hour step of 21 frames or more)
            // the 48 blurred ones after them
            static constexpr int FaceStep[11] = { 2, 3, 3, 5, 6, 9, 13, 17, 21, 25, 35 };
            canvas.DrawSprite(0x96, FaceStep[s] >= 21 ? 0x60 + w.face / 2 : w.face, c.x + 0x52, c.y + 4);
            canvas.DrawSprite(0x94, w.ampm, c.x + 0x5A, c.y + 0x2B);
            canvas.DrawSprite(0x95, w.month, c.x + 0x15, c.y + 0xC);
            // the pendulum: frames 0-29 while stopped, 30-40 swinging while the clock runs (the game also sets a
            // flag on the sprite then, +0x14 bit 0, not worked out)
            canvas.DrawSprite(0x9B, s ? 30 + w.cycle * 11 / 30 : w.cycle, c.x + 0x54, c.y + 0x41);
            Rect twirl = SpriteRect(0x9A, 0, 0);
            canvas.DrawSprite(0x9A, 0, c.x - twirl.w, c.y + 6);
            // the day and year (the first digit is hu_num03, 0-3)
            static constexpr int X[6] = { 7, 14, 48, 55, 62, 69 }; // 0x4BA688
            for (int i = 0; i < 6; i++) canvas.DrawSprite(i == 0 ? 0x99 : 0x98, w.digit[i], c.x + X[i], c.y + 0xC);
        }
        else
        {
            // the globe (the hour of the day), the days gone by in six digits, and the goo
            canvas.DrawSprite(0x96, w.globe, c.x + 0xB, c.y + 8);
            static constexpr int X[6] = { 32, 41, 50, 59, 74, 83 }; // 0x4BA670
            for (int i = 0; i < 6; i++) canvas.DrawSprite(0x98, w.digit[i], c.x + X[i], c.y + 0x35);
            canvas.DrawSprite(0x9B, w.cycle, c.x + 0x31, c.y + 7);
        }
        Rect sp = SpeedRect();
        canvas.DrawSprite(0x97, speed + (sp.Contains(mouseX, mouseY) ? 11 : 0), sp.x, sp.y);
    }

    int WarMap::CounterAt(int x, int y) const
    {
        for (size_t i = counters.size(); i-- > 0;)
        {
            const Counter& k = counters[i];
            if (k.kind != viewMode - 1) continue;
            int cx = k.x - cameraX, cy = k.y - cameraY;
            if (x >= cx && y >= cy && x < cx + 32 && y < cy + 32) return int(i);
        }
        return -1;
    }

    namespace
    {
        constexpr double TurnRadians = 6.283185307179586 / 4096.0; // headings are 4096ths of a turn
        int Heading(float x0, float y0, float x1, float y1) // 0x43FDA0
        {
            double a = std::atan2(double(y1 - y0), double(x1 - x0)) / TurnRadians;
            return (int(std::lround(a)) % 4096 + 4096) % 4096;
        }
        std::pair<float, float> Polar(int heading, float distance) // 0x43FE90
        {
            double a = heading * TurnRadians;
            return { float(std::cos(a) * distance), float(std::sin(a) * distance) };
        }
        int AngleDiff(int a, int b) // 0x43FF30: a - b in -2048..2047
        {
            int d = ((a - b) % 4096 + 4096) % 4096;
            return d >= 2048 ? d - 4096 : d;
        }
    }

    std::vector<WarMap::Counter::ArrowPoint> WarMap::ArrowCurve(const std::vector<std::pair<float, float>>& control)
    {
        // RArrow's curve (0x4244E0 -> 0x424A40): the control points 2^5 apart, then five passes of subdivision,
        // each new midpoint bent towards the headings at its two ends by 5/11 of the average offset
        constexpr int Steps = 32;
        using P = Counter::ArrowPoint;
        if (control.size() < 2) return {};
        int count = int(control.size() - 1) * Steps + 1;
        std::vector<P> p(size_t(count), P{ 0, 0, 0 });
        for (size_t i = 0; i < control.size(); i++) p[i * Steps] = { control[i].first, control[i].second, 0 };
        for (int step = Steps; step >= 2; step /= 2)
        {
            for (int i = 0; i + step < count; i += step)
                p[size_t(i)].heading = Heading(p[size_t(i)].x, p[size_t(i)].y, p[size_t(i + step)].x, p[size_t(i + step)].y);
            for (int i = 0; i + step < count; i += step)
            {
                const P& a = p[size_t(i)];
                const P& b = p[size_t(i + step)];
                float mx = (a.x + b.x) / 2, my = (a.y + b.y) / 2;
                float length = std::hypot(b.x - a.x, b.y - a.y);
                int h = Heading(a.x, a.y, b.x, b.y);
                float vx = 0, vy = 0;
                int terms = 0;
                auto offset = [&](int diff) {
                    float o = float(std::tan(diff * TurnRadians)) * length * 0.5f;
                    return std::clamp(o, -length / 2, length / 2);
                };
                if (i > 0)
                {
                    auto [x, y] = Polar(h - 0x400, offset(AngleDiff(h, a.heading)));
                    vx += x; vy += y; terms++;
                }
                if (i < count - 2 * step)
                {
                    auto [x, y] = Polar(h - 0x400, offset(AngleDiff(b.heading + 0x800, h + 0x800)));
                    vx -= x; vy -= y; terms++;
                }
                if (terms) { vx /= float(terms); vy /= float(terms); }
                p[size_t(i + step / 2)] = { mx + vx * 5.0f / 11.0f, my + vy * 5.0f / 11.0f, 0 };
            }
        }
        for (int i = 1; i + 1 < count; i++)
            p[size_t(i)].heading = Heading(p[size_t(i - 1)].x, p[size_t(i - 1)].y, p[size_t(i + 1)].x, p[size_t(i + 1)].y);
        p[0].heading = p[1].heading;
        p[size_t(count - 1)].heading = p[size_t(count - 2)].heading;
        return p;
    }

    void WarMap::DrawRouteArrow(Canvas& canvas, const Counter& k) const
    {
        // RTypeArrow (0x49CAC0, drawn by 0x49CDB0) along the route's curve. Style (OBJ.ojd record 14): attacks 0
        // (Human) / 2 (Martian), other moves 1 / 3. Its texture is 256 x 64: the body strip in the first 64 columns,
        // the head's triangle from 64 to 191 with its point at (128, 0).
        if (k.arrow.size() < 2) return;
        bool human = res.Human();
        auto style = arrowTypes.find((human ? 0 : 2) + (k.attack ? 0 : 1));
        if (style == arrowTypes.end()) return;
        const ObjOjd::ArrowType& t = style->second;
        const std::vector<uint8_t>* texture = res.GetRaw(t.gtype);
        const ShadeTable* shade = res.ShadeFor(t.gtype);
        constexpr int TexW = 256, TexH = 64, Steps = 32;
        if (!texture || texture->size() < size_t(TexW * TexH) || !shade) return;
        const uint32_t* colours = shade->Level(0);
        const int n = int(k.arrow.size());
        auto at = [&](int i) { return std::make_pair(k.arrow[size_t(i)].x - float(cameraX), k.arrow[size_t(i)].y - float(cameraY)); };
        // the edges: at right angles to each point's heading, the half width going from the first number to the
        // second by (second - first) / points a point
        auto edges = [&](int i, float& lx, float& ly, float& rx, float& ry) {
            float w = float(t.a) + float(t.b - t.a) * float(i) / float(n);
            auto [dx, dy] = Polar(k.arrow[size_t(i)].heading + 0x400, w);
            auto [x, y] = at(i);
            lx = x + dx; ly = y + dy; rx = x - dx; ry = y - dy;
        };
        // the body from the units on (+0x5C: the quads past it)
        int bodyEnd = std::max(0, n - Steps / 3 - 1);
        int from = std::clamp(k.trim, 0, bodyEnd);
        float plx, ply, prx, pry;
        edges(from, plx, ply, prx, pry);
        for (int i = from + 1; i <= bodyEnd; i++)
        {
            float lx, ly, rx, ry;
            edges(i, lx, ly, rx, ry);
            // each quad takes the whole 64 x 64 strip
            Canvas::TexVertex l0{ plx, ply, 0, 0 }, l1{ lx, ly, 0, 63 }, r1{ rx, ry, 63, 63 }, r0{ prx, pry, 63, 0 };
            canvas.DrawTexturedTriangle(texture->data(), TexW, TexH, colours, l0, l1, r1);
            canvas.DrawTexturedTriangle(texture->data(), TexW, TexH, colours, l0, r1, r0);
            plx = lx; ply = ly; prx = rx; pry = ry;
        }
        // the head at the last body point B: wings W1, W2 a head half width either side (moved k = the style's
        // float times D1 back), the point the head length on, and the body's last edge points L and R between
        auto [bx, by] = at(bodyEnd);
        int h = k.arrow[size_t(bodyEnd)].heading;
        auto [d1x, d1y] = Polar(h, float(t.c));
        auto [d2x, d2y] = Polar(h, float(t.d));
        float kf = t.f;
        Canvas::TexVertex w1{ bx + d1y - kf * d1x, by - d1x - kf * d1y, 64, 63 };
        Canvas::TexVertex w2{ bx - d1y - kf * d1x, by + d1x - kf * d1y, 191, 63 };
        Canvas::TexVertex tip{ bx + d2x, by + d2y, 127, 0 };
        Canvas::TexVertex right{ prx, pry, 112, 63 }, left{ plx, ply, 144, 63 };
        canvas.DrawTexturedTriangle(texture->data(), TexW, TexH, colours, w1, right, tip);
        Canvas::TexVertex tip2 = tip;
        tip2.u = 128;
        canvas.DrawTexturedTriangle(texture->data(), TexW, TexH, colours, right, left, tip2);
        canvas.DrawTexturedTriangle(texture->data(), TexW, TexH, colours, left, w2, tip2);

        // an attack's arrival label (0x424776): boxed text (0x49D260, font 0x71) at the route's end, the box black
        // (dword_4DAFC8) with a 1 pixel outline (dword_4DAF88 black for the Humans, the Martians' side colour
        // dword_4DAF9C), the text 1 pixel in: the arrival date worked out when the route was made, or when its
        // hours were last set (0x424431 / 0x4255F0)
        if (!k.attack) return;
        std::string label = warmap::ArrivalText(k.labelFrom, k.labelHours, human,
            [&](int month) { return res.String(warmap::MonthText(month)); });
        auto [ex, ey] = at(n - 1);
        const Font* font = res.GetFont(0x71);
        int tw = font ? font->Measure(label) : 0;
        int th = font ? font->Height() : 10;
        Rect box{ int(ex) - tw / 2 - 1, int(ey) - th / 2 - 1, tw + 2, th + 2 };
        auto six = [](int v) { return uint32_t((v * 255 + 31) / 63); };
        uint32_t outline = human ? 0 : (six(12) << 16 | six(42) << 8 | six(4));
        uint32_t* out = canvas.Pixels();
        for (int y = box.y; y < box.y + box.h; y++)
            for (int x = box.x; x < box.x + box.w; x++)
            {
                if (x < 0 || y < 0 || x >= canvas.Width() || y >= canvas.Height()) continue;
                bool edge = x == box.x || y == box.y || x == box.x + box.w - 1 || y == box.y + box.h - 1;
                out[size_t(y) * size_t(canvas.Width()) + size_t(x)] = edge ? outline : 0;
            }
        canvas.DrawString(0x71, label, Rect{ box.x + 1, box.y + 1, tw, th }, 0);
    }

    bool WarMap::HasCommandPost(int sector) const
    {
        // 0x43C990: the sector's owner has its command building there, finished. A Human sector needs a Command
        // Post (OTYPE_CommandPost, 58; in London, sector 30, 278), a Martian one a Communications Centre (120; in
        // sector 3, where the Martians land, 830). The Human type is worked out as 0x116 + (sector == 30 ? 0 :
        // 0xFFFFFF24, from "and al, 24h" on -1), which wraps to 0x3A. "No further advance or construction can be co-ordinated in
        // the vicinity until a command post has been built." A sector no one holds never has one.
        if (sector < 1 || sector > warmap::SectorCount) return false;
        int who = owner[size_t(sector)];
        if (who == 2) return false;
        bool human = (who == 0) == res.Human();
        int type = human ? (sector == 30 ? 278 : 58) : (sector == 3 ? 830 : 120);
        const auto& here = finished[size_t(sector)];
        return std::find(here.begin(), here.end(), type) != here.end();
    }

    std::vector<int> WarMap::FindRoute(int from, int to, const Counter& k, std::string* why) const
    {
        // The game's route finder: the A* (warmap::AStar; set up by 0x424160) with 0x42ABD0 giving each step's
        // cost, or refusing it, the reason left at 0x4EA198 (by whichever call refused last, the A*'s estimates
        // included). The cost is 0 from a sector to itself, else the straight line between the two sectors'
        // waypoints (0x437380), whole pixels. A step is refused:
        //   0    a unit that can't move (0x42A410; not in yet)
        //   1/2  the link isn't one the unit can take: land units need a land link (bits 0-2), boats a sea one
        //        (bit 4); fliers take any
        //   3    neither end has its owner's command post
        //   4/5  into a sector other than the goal that has no command post, or isn't the player's
        //   6    the route's start has no command post and the step goes into a sector that isn't the player's
        //   7/8  into the goal when it would hold more than "Max units in sector" of the player's units, or "Max
        //        boats in sector" boats: those there, those on their way there (0x420060) and these
        // The route stands when the A* gets there with more than the start in it.
        int reason = -1;
        auto cost = [&](int a, int b) -> int {
            if (a == b) return 0;
            int kind = warmap::RouteLinkKind(a, b);
            if (k.moveClass <= 2 && !(kind & warmap::RouteKind::Land)) { reason = 1; return -1; }
            if (k.moveClass == 3 && !(kind & warmap::RouteKind::Sea)) { reason = 2; return -1; }
            if (!HasCommandPost(a) && !HasCommandPost(b)) { reason = 3; return -1; }
            if (b != to)
            {
                if (!HasCommandPost(b)) { reason = 4; return -1; }
                if (owner[size_t(b)] != 0) { reason = 5; return -1; }
            }
            if (!HasCommandPost(from) && owner[size_t(b)] != 0) { reason = 6; return -1; }
            if (b == to)
            {
                int units = k.moveClass != 3 ? k.count : 0, boats = k.moveClass == 3 ? k.count : 0;
                for (const Counter& c : counters)
                {
                    if (c.kind != 0 || &c == &k) continue;
                    if (!c.Travelling())
                    {
                        // 0x43D090 / 0x43D0F0: the player's units in the sector, boats (class 3) apart
                        if (c.players && c.sector == to) (c.moveClass == 3 ? boats : units) += c.count;
                    }
                    else if (c.path.back() == to) (c.naval ? boats : units) += c.count;
                }
                if (k.moveClass != 3 && units > maxUnits) { reason = 7; return -1; }
                if (k.moveClass == 3 && boats > maxBoats) { reason = 8; return -1; }
            }
            auto pa = warmap::Waypoint(a), pb = warmap::Waypoint(b);
            int dx = pa.x - pb.x, dy = pa.y - pb.y;
            return int(std::sqrt(double(dx * dx + dy * dy)));
        };
        std::vector<int> path = warmap::AStar(from, to, cost);
        if (path.size() <= 1)
        {
            if (why)
            {
                static const char* const Why[] = {
                    "That unit can't move", "Land units can't cross there", "Boats can't sail there",
                    "No command post to co-ordinate the move", "A command post is needed on the way",
                    "The way there isn't through our own sectors", "No command post where the move starts",
                    "Too many units in that sector", "Too many boats in that sector" };
                *why = reason >= 0 ? Why[reason] : "No way there";
            }
            return {};
        }
        path.erase(path.begin());
        return path;
    }

    namespace
    {
        // The spiral the searches follow (0x4282C0, built by 0x428110 into 0x800 bytes): the middle, then rings
        // 1-15, each clockwise from the cell above the middle; 961 steps of the 1024 room (the rest is never set).
        const std::vector<std::pair<int, int>>& Spiral()
        {
            static const std::vector<std::pair<int, int>> spiral = [] {
                std::vector<std::pair<int, int>> v{ { 0, 0 } };
                for (int r = 1; r < 16; r++)
                {
                    int dx = 0, dy = -r;
                    for (int i = 0; i < r; i++) v.push_back({ dx++, dy });
                    for (int i = 0; i < 2 * r; i++) v.push_back({ dx, dy++ });
                    for (int i = 0; i < 2 * r; i++) v.push_back({ dx--, dy });
                    for (int i = 0; i < 2 * r; i++) v.push_back({ dx, dy-- });
                    for (int i = 0; i < r; i++) v.push_back({ dx++, dy });
                }
                return v;
            }();
            return spiral;
        }
        constexpr int SpiralRoom = 0x400;
    }

    // A sector's grid (0x428110): `size` pixel cells over its bounds (0x4BAE78), the left and top rounded down and
    // the right and bottom up to whole cells; a cell is usable when CHI.SPR has the sector's number at its corner.
    void WarMap::BuildGrid(int sector, int size)
    {
        CellGrid& g = grids[size_t(sector)];
        size = std::max(1, size);
        const auto& b = warmap::Sectors[sector];
        g.size = size;
        g.left = b.left / size * size;
        g.top = b.top / size * size;
        int right = (b.right + size - 1) / size * size, bottom = (b.bottom + size - 1) / size * size;
        g.w = (right - g.left) / size;
        g.h = (bottom - g.top) / size;
        g.cells.assign(size_t(std::max(0, g.w * g.h)), 0);
        g.index = 0;
        const IndexedImage* chi = res.GetFrame(0x84);
        size_t n = 0;
        for (int y = g.top; y < bottom; y += size)
            for (int x = g.left; x < right; x += size, n++)
                if (chi && x >= 0 && y >= 0 && x < chi->width && y < chi->height && chi->pixels[size_t(y) * size_t(chi->width) + size_t(x)] == sector)
                    g.cells[n] = 1;
    }

    // The search for a free cell (0x4283F0): mode 3 (the Unit and Construction views) and mode 2 (the Resource
    // view) both go round the spiral from the grid's middle cell (w / 2, h / 2), from where the last search
    // stopped: the place moves on past every step that's off the grid or not free, and is kept at the one taken,
    // so a cell given back behind it isn't used again until the grid is laid out afresh. Mode 2 tests the step's
    // own row offset against the height instead of the row (so the rows above the middle are never used, and rows
    // below the grid would be read past it; those count as not free here). The cell taken is marked 0x81. Gives
    // false (the game's 0, 0) when the spiral runs out.
    bool WarMap::FindCell(int sector, int& x, int& y, int& cell)
    {
        CellGrid& g = grids[size_t(sector)];
        const bool resources = viewMode == 3;
        const auto& spiral = Spiral();
        const int cx = g.w / 2, cy = g.h / 2;
        for (; g.index < SpiralRoom; g.index++)
        {
            auto [dx, dy] = size_t(g.index) < spiral.size() ? spiral[size_t(g.index)] : std::pair<int, int>{ 0, 0 };
            int col = cx + dx, row = cy + dy;
            if (unsigned(col) >= unsigned(g.w)) continue;
            if (resources ? (unsigned(dy) >= unsigned(g.h) || row >= g.h) : unsigned(row) >= unsigned(g.h)) continue;
            int n = row * g.w + col;
            if (g.cells[size_t(n)] != 1) continue;
            g.cells[size_t(n)] |= 0x80;
            x = col * g.size + g.left;
            y = row * g.size + g.top;
            cell = n;
            return true;
        }
        x = y = 0;
        cell = -1;
        return false;
    }

    // Placing a counter (0x4205F0): the next free cell. When there's none the sector's grid is made again with
    // cells [Tweak] "WM clunting adj" (4) smaller, every counter already in the sector takes a new cell and slides
    // there at 5 pixels a tick (0x4286C0), and then this one is placed. (Class 7 objects sharing a counter are
    // grouped when the counters are made.)
    void WarMap::PlaceCounter(int index)
    {
        Counter& k = counters[size_t(index)];
        const int s = k.sector;
        if (s < 1 || s > warmap::SectorCount) return;
        int x, y, cell;
        if (!FindCell(s, x, y, cell))
        {
            SDL_Log("War map: sector %d is full, its cells go from %d to %d pixels", s, grids[size_t(s)].size, grids[size_t(s)].size - clunAdj);
            BuildGrid(s, grids[size_t(s)].size - clunAdj);
            for (int p : placed)
            {
                Counter& o = counters[size_t(p)];
                if (o.sector != s || o.Travelling()) continue;
                int ox, oy, ocell;
                FindCell(s, ox, oy, ocell);
                o.cell = ocell;
                o.targetX = ox; o.targetY = oy;
                o.slideX = float(o.x); o.slideY = float(o.y);
                float dx = float(ox - o.x), dy = float(oy - o.y), d = std::sqrt(dx * dx + dy * dy);
                o.slideLeft = d;
                o.slideStep = 5.0f;
                o.stepX = d > 0 ? dx / d * 5.0f : 0;
                o.stepY = d > 0 ? dy / d * 5.0f : 0;
            }
            FindCell(s, x, y, cell);
        }
        placed.push_back(index);
        k.cell = cell;
        k.x = x; k.y = y;
        k.targetX = x; k.targetY = y;
        k.slideLeft = 0;
    }

    // A counter leaving its cell: the cell is usable again (&= 0x7F) and it's out of the placement.
    void WarMap::FreeCounter(int index)
    {
        Counter& k = counters[size_t(index)];
        if (k.cell >= 0 && k.sector >= 1 && k.sector <= warmap::SectorCount)
        {
            CellGrid& g = grids[size_t(k.sector)];
            if (size_t(k.cell) < g.cells.size()) g.cells[size_t(k.cell)] &= 0x7F;
        }
        k.cell = -1;
        k.slideLeft = 0;
        placed.erase(std::remove(placed.begin(), placed.end(), index), placed.end());
    }

    // A sector laid out again (0x43CB60, when units arrive): its grid cleared, back to the full cell size if it
    // had shrunk (0x4203C0), then the counters of the view in it taken out and placed again, the player's first.
    void WarMap::RefreshSector(int sector)
    {
        if (sector < 1 || sector > warmap::SectorCount) return;
        CellGrid& g = grids[size_t(sector)];
        if (g.size != clunRes) BuildGrid(sector, clunRes);
        else
        {
            for (uint8_t& c : g.cells) c &= 0x7F;
            g.index = 0;
        }
        std::vector<int> here;
        for (bool players : { true, false })
            for (size_t i = 0; i < counters.size(); i++)
            {
                const Counter& k = counters[i];
                if (k.sector == sector && k.players == players && k.kind == viewMode - 1 && !k.Travelling()) here.push_back(int(i));
            }
        for (int i : here) { counters[size_t(i)].cell = -1; placed.erase(std::remove(placed.begin(), placed.end(), i), placed.end()); }
        for (int i : here) PlaceCounter(i);
    }

    // A new view (0x419810): every grid cleared (0x4204A0, keeping a shrunk grid's size), then each sector's
    // counters of the view placed (0x41C2E0 -> 0x43CB20), the player's first. Counters on their way stay where
    // they are.
    void WarMap::LayoutAll()
    {
        for (int s = 1; s <= warmap::SectorCount; s++)
        {
            CellGrid& g = grids[size_t(s)];
            if (g.size == 0) BuildGrid(s, clunRes);
            for (uint8_t& c : g.cells) c &= 0x7F;
            g.index = 0;
        }
        placed.clear();
        for (Counter& k : counters) { k.cell = -1; k.slideLeft = 0; }
        for (int s = 1; s <= warmap::SectorCount; s++)
            for (bool players : { true, false })
                for (size_t i = 0; i < counters.size(); i++)
                {
                    const Counter& k = counters[i];
                    if (k.sector == s && k.players == players && k.kind == viewMode - 1 && !k.Travelling()) PlaceCounter(int(i));
                }
    }

    void WarMap::SetView(int mode)
    {
        if (mode == viewMode) return;
        viewMode = mode;
        LayoutAll();
    }

    // Each tick (0x4209E0): a sliding counter steps on (0x428730) until what's left is less than a step, then
    // takes its cell; it's done when under 0.0005 is left (0x4287A0).
    void WarMap::SlideCounters()
    {
        for (int i : placed)
        {
            Counter& k = counters[size_t(i)];
            if (k.slideLeft < 0.0005f) continue;
            if (k.slideStep < k.slideLeft)
            {
                k.slideX += k.stepX;
                k.slideY += k.stepY;
                k.slideLeft -= k.slideStep;
            }
            else
            {
                k.slideX = float(k.targetX);
                k.slideY = float(k.targetY);
                k.slideLeft = 0;
            }
            k.x = int(k.slideX);
            k.y = int(k.slideY);
        }
    }

    void WarMap::SendTo(int index, int sector)
    {
        Counter& k = counters[size_t(index)];
        if (sector == k.sector || k.Travelling()) return;
        std::string why;
        std::vector<int> path = FindRoute(k.sector, sector, k, &why);
        std::string through;
        for (int s : path) through += " " + std::to_string(s);
        SDL_Log("War map: route from sector %d to %d for unit class %d: %s", k.sector, sector, k.moveClass,
                path.empty() ? why.c_str() : ("through" + through).c_str());
        // an attack (into a sector the other side holds) has to be to a neighbouring sector (0x424160)
        if (!path.empty() && owner[size_t(sector)] == 1 && path.size() != 1) { path.clear(); why = "An attack has to be on a neighbouring sector"; }
        if (path.empty()) { message = why; return; }
        k.moved = true;
        path.insert(path.begin(), k.sector);
        k.path = path;
        k.attack = owner[size_t(sector)] == 1; // +0x5E: not when the goal is the side's own or no one's
        auto unit = unitData.find(k.type);
        k.naval = unit != unitData.end() && (unit->second.field52 == 5 || unit->second.field52 == 6);
        // the arrow's control points (0x4244E0): for the start, the point of its link to the next sector where a
        // route over land leaves it (0x4BCEB0); then every sector's waypoint (0x437380). A naval route, or a point
        // that's 0, 0, takes the sector's sea waypoint instead (0x4373A0).
        std::vector<std::pair<float, float>> control;
        for (size_t n = 0; n < path.size(); n++)
        {
            warmap::Point at = warmap::Waypoint(path[n]);
            if (n == 0)
            {
                const warmap::SectorLink* link = warmap::FindLink(path[0], path[1]);
                at = link ? warmap::Point{ link->land[0], link->land[1] } : warmap::Point{ 0, 0 };
            }
            if (k.naval || (at.x == 0 && at.y == 0)) at = warmap::SeaWaypoint(path[n]);
            control.push_back({ float(at.x), float(at.y) });
        }
        k.arrow = ArrowCurve(control);
        // the route's length (+0x68): waypoint to waypoint, whole pixels each; a RouteEntry (0x42AFE0) goes it at
        // its unit's speed (+0x54 x 0.05), so in length / speed hours, starting on the first sector's way, and
        // takes its place in a formation [Tweak] "Route clunting width" (4) wide, "Route clunting spacing" (4)
        // apart: the n-th at ((n % w) - w / 2, (n / w) - w / 2) x spacing (a counter is the route's only one)
        float length = 0;
        for (size_t n = 1; n < path.size(); n++)
        {
            auto a = warmap::Waypoint(path[n - 1]), b = warmap::Waypoint(path[n]);
            int dx = b.x - a.x, dy = b.y - a.y;
            length += float(std::sqrt(double(dx * dx + dy * dy)));
        }
        k.remaining = length;
        k.hoursLeft = length / PixelsPerHour(k);
        {
            auto a = warmap::Waypoint(path[0]), b = warmap::Waypoint(path[1]);
            int dx = a.x - b.x, dy = a.y - b.y;
            k.segmentLength = k.segmentLeft = float(std::sqrt(double(dx * dx + dy * dy)));
        }
        k.segment = 0;
        k.segmentStarted = true;
        k.hoursOn = 0;
        k.interrupted = false;
        k.trim = 0;
        const int n = 0;
        k.offsetX = (n % clunWidth - clunWidth / 2) * clunSpacing;
        k.offsetY = (n / clunWidth - clunWidth / 2) * clunSpacing;
        k.labelHours = k.hoursLeft;
        for (int i = 0; i < 6; i++) k.labelFrom[i] = game.dateTime[i];
        ArriveTogether();
        message.clear();
    }
}
