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
#include <cstdlib>
#include <cstring>
#include <map>
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
        this->objectClasses = objectClasses;
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
        struct Group { int sector, list, type, x, y, count; std::vector<int> refs; bool shared = false; int finished = 0; bool firstUnfinished = false; float progress = 1.0f; };
        std::vector<Group> groups;
        for (const auto& o : objects)
        {
            if (o.sector < 1 || o.sector > warmap::SectorCount) continue;
            Group* g = nullptr;
            auto cls = objectClasses.find(o.unitType);
            if (cls != objectClasses.end() && cls->second == 7)
                for (Group& existing : groups)
                    if (existing.sector == o.sector && existing.list == o.list && existing.type == o.unitType) { g = &existing; break; }
            const bool done = !(o.list == 2 || o.list == 3) || o.value[1] >= o.value[0];
            if (!g)
            {
                groups.push_back({ o.sector, o.list, o.unitType, 0, 0, 0, {} });
                g = &groups.back();
                g->shared = cls != objectClasses.end() && cls->second == 7;
                g->firstUnfinished = !done;
                if (o.list == 2 || o.list == 3) g->progress = o.value[0] != 0.0f ? o.value[1] / o.value[0] : 1.0f;
            }
            g->count++;
            if (done) g->finished++;
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
            k.shared = g.shared;
            k.finishedCount = g.finished;
            k.unfinished = g.firstUnfinished;
            k.progress = g.progress;
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
    // The world's 0x802D (0x419E9D), in its order: the routes, the players (the computer player's turn), both
    // sides' research (0x458BD0: the local player's, then the other's), the pods (0x41A330), then each sector
    // (0x43B6C9): its objects (0x446310: their intel 0x429B80, the factories 0x45FB20; by its lists, units then
    // constructions, home side first) and its day count (+0x6C; each whole day takes one off an invasion
    // cool-down, +0x68); the resource warnings come in the same loop.
    void WarMap::Hourly(int hours)
    {
        MoveRoutes(hours);
        AiTurn(hours);
        for (auto& r : research) if (r) r->Advance(hours);
        DropPod(hours);
        for (Counter& k : counters) k.intel = std::max(0, k.intel - hours);
        for (int s = 1; s <= warmap::SectorCount; s++)
        {
            std::vector<int> refs;
            for (int list = 0; list < 6; list++)
                for (const auto& [ref, f] : factories)
                {
                    const int at = CounterOf(ref);
                    const int sector = at >= 0 ? counters[size_t(at)].sector : table[size_t(ref)].Sector();
                    if (sector == s && table[size_t(ref)].list == list && !(at >= 0 && counters[size_t(at)].Travelling())) refs.push_back(ref);
                }
            for (int ref : refs) Produce(ref, hours);
            sectorHours[size_t(s)] += hours;
            while (sectorHours[size_t(s)] >= 24)
            {
                sectorHours[size_t(s)] -= 24;
                if (aiCooldown[size_t(s)]) aiCooldown[size_t(s)]--;
            }
        }
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
        int leastSector[3] = { 0, 0, 0 };
        for (int s = 1; s <= warmap::SectorCount; s++)
        {
            if (owner[size_t(s)] != 0) continue;
            const SectorEconomy& e = economy[size_t(s)];
            for (int k = 0; k < 3; k++)
            {
                float r = e.effSupply[size_t(base + k)] / e.effDemand[size_t(base + k)];
                if (r < least[k]) { least[k] = r; leastSector[k] = s; }
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
            RaiseEvent(sound == AlertSound[base + k] ? ResourceAlert : ResourceWarning, leastSector[k] << 16 | k);
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
        // who holds it now the pod's units are in (each one added worked it out again, 0x43CEB0 -> 0x43CA40), as
        // Human 0 / Martian 1 / no one 2: one no one held is the Martians' now, and PodLanded only notes the landing
        int who = owner[size_t(sector)] == 2 ? 2 : (owner[size_t(sector)] == 0) == human ? 0 : 1;
        if (who == 0) { podFlag[size_t(sector)] = true; return; }
        RaiseEvent(PodLanded, sector);
    }

    // A unit put in a sector (0x43CEB0): its own counter in the sector's next free cell. With a save behind the map
    // it also gets a saved record: a copy of one of the same type (its battle map units with it), with new ids
    // (the lowest free ones, as the game hands them out).
    int WarMap::AddUnit(int type, int sector, bool players) { return AddObject(type, sector, players, 0); }

    // An object made in a sector (0x43CD10 / 0x43CEB0), copied from a saved one of the type in the same kind of
    // list (0 units, 2 constructions, 4 resources) so it can be saved, with ids of its own
    int WarMap::AddObject(int type, int sector, bool players, int kind)
    {
        int ref = -1;
        if (save)
        {
            const SaveObject* model = nullptr;
            for (const SaveObject& o : table) if (o.list / 2 == kind / 2 && o.Type() == type) { model = &o; break; }
            // none of the type saved: another of the same kind and category, its type changed (*provisional*: what
            // the game writes for a new object of the type isn't known; its battle data stays the copy's)
            bool retype = false;
            if (!model)
            {
                auto want = unitData.find(type);
                for (const SaveObject& o : table)
                {
                    auto u = unitData.find(o.Type());
                    if (o.list / 2 != kind / 2 || u == unitData.end() || want == unitData.end()) continue;
                    if (u->second.category != want->second.category) continue;
                    if (!model || (o.list % 2 == (players ? 0 : 1) && model->list % 2 != (players ? 0 : 1))) model = &o;
                }
                retype = model != nullptr;
            }
            if (model)
            {
                SaveObject o = *model;
                if (retype) o.SetType(type);
                o.list = kind + (players ? 0 : 1);
                // the ids in use: the sectors' own data as saved, and every object the map has now
                std::vector<bool> used = save->UsedIds();
                for (const SaveObject& t : table) SaveFile::MarkIds(t.bytes, used);
                for (size_t at : o.IdOffsets())
                {
                    int id = SaveFile::TakeId(used);
                    for (int b = 0; b < 4; b++) o.bytes[at + size_t(b)] = uint8_t(uint32_t(id) >> (8 * b));
                }
                o.SetSector(sector);
                // a new object's factory starts empty, whatever the one it's copied from was making
                std::optional<ObjectTail> tail = ReadTail(o);
                if (tail && tail->hasFactory)
                {
                    tail->queue = {};
                    WriteTail(o, *tail);
                    tail = ReadTail(o);
                }
                table.push_back(std::move(o));
                ref = int(table.size()) - 1;
                if (tail && tail->hasFactory) factories[ref] = *tail;
            }
            else SDL_Log("War map: no saved object of type %d to copy, so the new one won't be saved", type);
        }
        int k = AddCounter(type, sector, players, ref, kind / 2);
        RecomputeOwner(sector); // 0x43CEB0 -> 0x43CA40
        return k;
    }

    int WarMap::AddCounter(int type, int sector, bool players, int ref, int kind)
    {
        auto unit = unitData.find(type);
        Counter k;
        k.frame = unit == unitData.end() ? 0 : unit->second.counterFrame;
        k.speed = unit == unitData.end() ? 0 : unit->second.warMapSpeed;
        k.moveClass = unit == unitData.end() ? 0 : unit->second.moveClass;
        k.count = 1;
        k.type = type;
        k.players = players;
        k.kind = kind;
        k.sector = sector;
        if (kind != 0) k.speed = 0;
        if (ref >= 0) k.refs.push_back(ref);
        counters.push_back(k);
        int index = int(counters.size()) - 1;
        // added to the sector (0x43CEB0 -> 0x43CD88): placed when the view shows its kind
        if (viewMode == kind + 1 && Visible(index)) PlaceCounter(index);
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

    void WarMap::StartResearch(const ObjOjd::ResearchModifiers& modifiers)
    {
        const bool human = res.Human();
        Research::Listener local;
        local.event = [this](int event, const std::string& name) { RaiseEvent(event, event == ResearchAlert ? 1 : 0, name); };
        // the other side's finished items go to its player (0xB9 to dword_4E9B78): the AI, not in yet
        research[0] = std::make_unique<Research>(human, true, unitData, res.Text(), modifiers, human, local);
        research[1] = std::make_unique<Research>(!human, false, unitData, res.Text(), modifiers, human, Research::Listener{});
        if (save && !save->research.empty())
        {
            // 0x41AC40: HRSH then MRSH (0x45A160 reads the tag, then 0x4584C0)
            const std::vector<uint8_t>& blob = save->research;
            size_t pos = 0;
            for (bool side : { true, false })
            {
                Research& r = *research[side == human ? 0 : 1];
                const char* tag = Research::Tag(side);
                if (pos + 4 > blob.size() || std::memcmp(&blob[pos], tag, 4) != 0 || !(pos += 4, r.Load(blob, pos)))
                {
                    SDL_Log("War map: the save's %s research can't be read", tag);
                    break;
                }
            }
        }
        // the view's set-up (0x418D17): both sides' triggers and multipliers
        for (auto& r : research) r->UpdateTriggers(encountered);
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
        ReadFactories();
    }

    // The war map written back into the save it came from (0x41AD80): the view, the clock and date, the message
    // list, the pods, who holds each sector, and every object in the sector it's in now (the first of a counter
    // that stands for several at the counter's place, the rest at 0, 0, as the game keeps them); units on their way
    // go in a route each (origin, destination, the sectors between, and the attack flag), as 0x425A60 writes them.
    std::optional<std::vector<uint8_t>> WarMap::SaveData() const
    {
        if (!save) return std::nullopt;
        SaveFile s = *save;
        // the research, Human first whichever side is played (0x41AE0D)
        if (research[0] && research[1])
        {
            s.research.clear();
            (research[0]->Human() ? research[0] : research[1])->Save(s.research);
            (research[0]->Human() ? research[1] : research[0])->Save(s.research);
        }
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
            if (written[i] || std::find(destroyed.begin(), destroyed.end(), int(i)) != destroyed.end()) continue;
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
                        RaiseEvent(TravelInterrupted, int(i));
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
    // and the route ending with it (0x4253F9), ArrivedInSector unless it was an attack.
    void WarMap::Arrive(int index, int sector, int previous)
    {
        Counter& k = counters[size_t(index)];
        k.previous = previous;
        const bool attack = k.attack;
        k.path.clear();
        k.arrow.clear();
        k.sector = sector;
        // the unit goes into the sector (0x429E80 -> 0x429DB0), which then belongs to whoever alone is in it
        // (0x43CA40), and is laid out again (0x43CB60); a battle, if the other side is there, is found by the
        // next tick's check (0x41AB00)
        RecomputeOwner(sector);
        RefreshSector(sector);
        if (!attack) RaiseEvent(ArrivedInSector, sector);
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

        // the right button held for [Tweak] "Quick Cancel Timeout" updates opens the Warmap menu (0x805B -> 0x41D120)
        if (rightDownAt >= 0 && popup.empty() && animation - rightDownAt == quickCancelTimeout && mouseX >= 0)
            OpenWarmapMenu(mouseX, mouseY);
        // the game over (byte_4D2530): nothing moves, and the game is told when the time comes
        if (ending)
        {
            if (endingDelay > 0 && --endingDelay == 0) sent = Command{ 0x80FE, endingCode };
            return;
        }
        if (!economyStarted)
        {
            economyStarted = true;
            for (int phase = 0; phase < 5; phase++) EconomyPhase(phase);
        }
        animation++;
        SlideCounters();
        ClockTick();
        if (hoursPassed > 0) Hourly(hoursPassed);
        if (!pendingBattle) DetectBattles();
        EconomyPhase(animation % 5);
        ShadeSectors();
        // a waiting calendar is hidden every other pair of updates (0x4271E0: the tick count's bit 1), its wheels
        // standing still then
        if (!calendarWaiting || !(animation & 2)) TurnCalendar();

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
        // the selection drops what's gone off on its way or out of the view's kind
        selection.erase(std::remove_if(selection.begin(), selection.end(), [&](int i) {
            return size_t(i) >= counters.size() || counters[size_t(i)].Travelling() || counters[size_t(i)].kind != viewMode - 1; }), selection.end());
        if (hovered != previewSector) BuildPreview(hovered);
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
        for (const Preview& p : preview)
            if (p.path.size() > 1) DrawRouteArrow(canvas, p.shape);

        // The counters (GType 0x77) of the kind the view shows (WWarMapObject's draw, 0x42B850): at depth 4, or 7
        // for the one picked, so it's drawn over the rest; the player's unfinished constructions at 50% (the
        // sprite's +0x14 bit 0; the other side's would go by how old the intel on them is, not in yet); the pick
        // ring (cselecth.spr, GType 0x54) at depth + 2; and a class 7 counter's count (0x42B790: "%d" finished,
        // or "%d/%d" finished / all when some aren't) in the counter font (GType 0x6E) at depth + 1, its bottom
        // right on the counter's.
        const Font* countFont = res.GetFont(0x6E);
        auto drawCounter = [&](const Counter& k) {
            int x = k.x - cameraX, y = k.y - cameraY;
            if (x < -40 || y < -40 || x > canvas.Width() || y > canvas.Height()) return;
            if (!k.shown && !(k.players && k.Travelling())) return;
            const bool dim = (k.players && k.kind == 1 && k.unfinished) || (!k.players && k.intel < 0xA8);
            canvas.DrawSprite(0x77, k.frame, x, y, 0, dim ? Canvas::Blend::Translucent : Canvas::Blend::Opaque);
            if (Selected(int(&k - counters.data())) || k.inBand) canvas.DrawSprite(0x54, 0, x, y); // cselecth.spr
            if (k.shared)
            {
                std::string text = k.finishedCount < k.count ? std::to_string(k.finishedCount) + "/" + std::to_string(k.count)
                                                              : std::to_string(k.finishedCount);
                const IndexedImage* face = res.GetFrame(0x77, k.frame);
                int w = face ? face->width : 32, h = face ? face->height : 32, th = countFont ? countFont->Height() : 8;
                canvas.DrawString(0x6E, text, { x, y + h - th, w, th }, TextRight);
            }
        };
        auto onTop = [&](const Counter& k) { return Selected(int(&k - counters.data())) || k.inBand; };
        for (const Counter& k : counters)
            if (k.kind == viewMode - 1 && !onTop(k)) drawCounter(k); // each view shows its own kind
        for (const Counter& k : counters)
            if (k.kind == viewMode - 1 && onTop(k)) drawCounter(k);
        // the rubber band (tracker 0x4E9EE0; its look isn't traced: a 1 pixel white box here)
        if (rubberBand)
        {
            int l = std::min(dragX, mouseX), r = std::max(dragX, mouseX), t = std::min(dragY, mouseY), b = std::max(dragY, mouseY);
            uint32_t* out = canvas.Pixels();
            auto plot = [&](int px, int py) { if (px >= 0 && py >= 0 && px < canvas.Width() && py < canvas.Height()) out[size_t(py) * size_t(canvas.Width()) + size_t(px)] = 0xFFFFFFu; };
            for (int px = l; px <= r; px++) { plot(px, t); plot(px, b); }
            for (int py = t; py <= b; py++) { plot(l, py); plot(r, py); }
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
        if (!calendarWaiting || !(animation & 2)) DrawCalendar(canvas);
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
        DrawBuildWindow(canvas);
        // the popup menu over everything (z 0x61B2): each item's icon, the next frame while the mouse is on it
        // (*provisional*: the hot button's frames aren't traced)
        for (size_t i = 0; i < popup.size(); i++)
        {
            const PopupItem& item = popup[i];
            const IndexedImage* lit = int(i) == popupHover ? res.GetFrame(item.icon, 1) : nullptr;
            canvas.DrawSprite(item.icon, lit ? 1 : 0, item.rect.x, item.rect.y);
        }
        DrawBox(canvas); // z 0x65FE, over everything
    }

    // The keys, as the war map view (0x41C5E0 and 0x41D640) and the world (0x41A450) take them. Each press (and
    // each repeat while held) is one step, as with WM_KEYDOWN.
    // The right button on the map (0x41C5E0): let go within [Tweak] "Quick Cancel Timeout" (5) updates of pressing
    // it, without Ctrl, it's command 0x73, Cancel. (Held longer it opens the
    // "Warmap menu" popup, 0x41D120, which isn't in yet.) Cancel drops the selection (the player's action 2).
    std::optional<Command> WarMap::OnRightButton(int, int, bool down)
    {
        if (!popup.empty() || infoBox) { rightDownAt = -1; return std::nullopt; }
        if (down) { SetTimeRunning(false); rightDownAt = animation; return std::nullopt; } // 0x204: 0x41A6C0(0)
        bool quick = rightDownAt >= 0 && animation - rightDownAt <= quickCancelTimeout;
        rightDownAt = -1;
        if (quick && !(SDL_GetModState() & SDL_KMOD_CTRL)) ClearSelection(); // 0x73 -> the player's action 2
        return std::nullopt;
    }

    int WarMap::TipOwner(int x, int y) const
    {
        if (infoBox) return BoxButtonRect().Contains(x, y) ? 99 : -1;
        for (size_t i = 0; i < popup.size(); i++) if (popup[i].rect.Contains(x, y)) return 100 + int(i);
        if (int hit = BuildWindowHit(x, y); hit >= 0) return hit >= 200 && hit < 400 ? 10000 + hit : -1;
        PanelLayout panel = Panel();
        for (int i = 0; i < 3; i++) if (panel.buttons[i].Contains(x, y)) return i;
        if (panel.research.Contains(x, y)) return 3;
        if (CalendarRect().Contains(x, y) && !SpeedRect().Contains(x, y)) return 4;
        if (panel.frame.Contains(x, y) || panel.mini.Contains(x, y)) return -1;
        // a counter of the player's answers 0x807C (the other side's don't)
        int counter = CounterAt(x, y);
        if (counter >= 0 && counters[size_t(counter)].players) return 1000 + counter;
        return -1;
    }

    // A counter's tool tip (0x807C, level 1-3 by how long the mouse has stayed):
    //   units (0x428D70): the name (the type's +0x14, OBJ.ojd record 21); "In transit" (0x51D) on its way (a
    //   unit with a task would show it: Currently building / Days to completion / Efficiency, Idle or
    //   Construction suspended, 0x48E-0x493, not in yet); from level 2, if it uses resources: "Resource use"
    //   (0x519) and the side's three kinds, "Working:" (0x51B) its work figures when it has any, then "Idle:"
    //   (0x51A) and "Moving:" (0x51C) its upkeep for all its men, halved when idle
    //   constructions (0x42A4B0): the name, " X n" for a class 7 counter (how many, 0x43A160); unfinished,
    //   "n% complete" (0x4B5, +0x7C / +0x78), and nothing more; finished, from level 2, "Resource use" and
    //   "Working:" when it has work figures
    std::string WarMap::CounterTip(int index, int level) const
    {
        const Counter& k = counters[size_t(index)];
        auto unit = unitData.find(k.type);
        if (unit == unitData.end()) return {};
        const ObjOjd::UnitData& u = unit->second;
        const int base = res.Human() ? 0 : 3;
        std::string tip = u.nameText ? res.String(u.nameText) : std::string();
        auto figures = [&](const char* format, float a, float b, float c) {
            char line[96];
            std::snprintf(line, sizeof(line), format, a, b, c);
            return std::string(line);
        };
        auto kinds = [&]() {
            return "\n" + res.String(0x519) + " (" + res.String(0x214 + base) + ", " + res.String(0x215 + base) + ", " +
                   res.String(0x216 + base) + ")";
        };
        const float work = u.activity[0] + u.activity[1] + u.activity[2];
        const float keep = u.upkeep[0] + u.upkeep[1] + u.upkeep[2];
        if (k.kind == 0)
        {
            if (k.Travelling()) tip += "\n" + res.String(0x51D);
            if (level < 2 || work + keep == 0.0f) return tip;
            tip += kinds();
            if (work != 0.0f) tip += "\n" + res.String(0x51B) + figures(" %0.1f, %0.1f, %0.1f", u.activity[0], u.activity[1], u.activity[2]);
            if (keep != 0.0f)
            {
                const float crew = float(CrewOf(k));
                tip += "\n" + res.String(0x51A) + figures(" %0.1f, %0.1f, %0.1f", u.upkeep[0] * crew * 0.5f, u.upkeep[1] * crew * 0.5f, u.upkeep[2] * crew * 0.5f);
                tip += "\n" + res.String(0x51C) + figures(" %0.1f, %0.1f, %0.1f", u.upkeep[0] * crew, u.upkeep[1] * crew, u.upkeep[2] * crew);
            }
            return tip;
        }
        if (k.shared) tip += " X " + std::to_string(k.count);
        if (k.unfinished)
        {
            if (k.shared) return tip;
            char line[48];
            std::snprintf(line, sizeof(line), "\n%2.1f%% ", double(k.progress) * 100.0);
            return tip + line + res.String(0x4B5);
        }
        if (level >= 2 && work != 0.0f)
            tip += kinds() + "\n" + res.String(0x51B) + figures(" %0.1f, %0.1f, %0.1f", u.activity[0], u.activity[1], u.activity[2]);
        return tip;
    }

    std::string WarMap::TipText(int owner, int level) const
    {
        static constexpr int ButtonText[3] = { 0x47B, 0x47C, 0x47E }; // the view buttons' labels (0x433480)
        // a popup item's text (its +0x0C; 0x475 "View map" would be the hovered sector's name)
        if (owner == 99 && infoBox) return res.String(infoBox->cancellable ? 0x480 : 0x47F);
        if (owner >= 100 && size_t(owner - 100) < popup.size())
        {
            const PopupItem& item = popup[size_t(owner - 100)];
            return item.label.empty() ? res.String(item.text) : item.label;
        }
        if (owner >= 10000) return BuildWindowTip(owner - 10000);
        if (owner >= 1000 && size_t(owner - 1000) < counters.size()) return CounterTip(owner - 1000, level);
        if (owner >= 0 && owner < 3) return res.String(ButtonText[owner]);
        // the Research button (0x45BEB0): "View Research" and what's being researched, 0x461 with nothing
        if (owner == 3)
        {
            const Research* r = research[0].get();
            const Research::Item* item = r ? r->Current() : nullptr;
            return res.String(0x47D) + "\n" + (item ? r->StatusText(*item, res.Human()) : res.String(0x461));
        }
        // the calendar (0x4278A0) answers only at the third level: "Advance time"
        if (owner == 4 && level == 3) return res.String(0x481);
        return {};
    }

    std::optional<Command> WarMap::OnKey(int key)
    {
        // the box has the keyboard (0x432C0E): Enter or Space presses its button, Esc cancels if it can
        if (infoBox)
        {
            if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_KP_ENTER) BoxCommand(infoBox->command);
            else if (key == SDLK_ESCAPE && infoBox->cancellable) BoxCommand(0x8123);
            return std::nullopt;
        }
        if (!popup.empty())
        {
            if (key == SDLK_ESCAPE) ClosePopup(true);
            return std::nullopt;
        }
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
        case SDLK_R: SetTimeRunning(false); return Command{ ResearchCommand, 0 }; // the research screen (0x7D)
        case SDLK_BACKSPACE: SetTimeRunning(false); break;     // stops the clock (0x41A6C0(0))
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
            // (0x41A4A1, unless a sector holds both sides, 0x41AEA0), and time runs (0x41A6C0(1))
            if (key >= int(SDLK_0) && key <= int(SDLK_9) && SpeedChangeAllowed())
            {
                SetSpeed(key == SDLK_0 ? 10 : int(key - SDLK_0));
                SetTimeRunning(true);
            }
            break;
        }
        return std::nullopt;
    }

    void WarMap::OnKeyUp(int) {}

    // A battle (0x8072, the box's Defend / Attack): the world's selected sector becomes the battle's (+0x60 =
    // sectors[dword_4E9B98]) and it sends itself 0x8133 with 0x8019: the briefing transition (0x41B210) and disc
    // check, then the Briefing (0x499930). Its 0x71 / 0x98 go to the battle map (0x41A082) and the battle's end
    // to the Debriefing (0x97), whose 0x99 / 0x8017 come back to the war map (0x419AA2).
    //
    // The Briefing's figures (0x4999E0): VIEWING when the sector hasn't both sides in it (0x43D010), else
    // ATTACKING when it isn't the player's (0x43CA30), else DEFENDING; the calendar; and the player's units
    // there (+0x4C: the war map objects and, 0x421F90, their battle map units) and constructions (+0x54).
    BriefingInfo WarMap::Briefing() const
    {
        BriefingInfo b;
        const int s = battleSector;
        b.sector = s;
        for (int i = 0; i < 6; i++) b.dateTime[i] = game.dateTime[i];
        b.hours = game.ticks;
        bool mine = false, theirs = false;
        for (const Counter& k : counters)
        {
            if (k.sector != s || k.Travelling() || k.kind > 1) continue;
            (k.players ? mine : theirs) = true;
            if (!k.players) continue;
            if (k.kind == 0) { b.units += k.count; b.vehicles += CrewOf(k); }
            else b.facilities += k.count;
        }
        b.heading = !(mine && theirs) ? 0x497 : s >= 1 && s <= warmap::SectorCount && owner[size_t(s)] != 0 ? 0x498 : 0x499;
        return b;
    }

    // The battle. The battle map isn't in yet, so the engine settles it here (*provisional*): the side with more
    // men in its units in the sector wins (the side holding it on a tie), and the loser's units, constructions and
    // resources there go (0x43A250). The battle report (0x421210 at dword_4E9B7C) gets what each side lost for the
    // Debriefing; the sector's battle flag goes and it and its neighbours are seen (0x418E17).
    void WarMap::FightBattle(const std::vector<int>* lost)
    {
        for (auto& r : research) if (r) r->UpdateTriggers(encountered); // 0x41A094, on the way to the battle map
        const int s = battleSector;
        debriefing = DebriefingInfo{};
        debriefing.sector = s;
        if (s < 1 || s > warmap::SectorCount) return;
        int men[2] = { 0, 0 };
        int units[2] = {}, vehicles[2] = {}, facilities[2] = {};
        for (size_t i = 0; i < counters.size(); i++)
        {
            const Counter& k = counters[i];
            if (k.sector != s || k.Travelling() || k.kind > 1) continue;
            const int side = k.players ? 0 : 1;
            if (k.kind == 0)
            {
                men[side] += CrewOf(k);
                units[side] += k.count;
                vehicles[side] += CrewOf(k);
            }
            else facilities[side] += k.count;
        }
        // a sector viewed with only one side in it has no battle: back with RETURNING TO WAR ROOMS
        if (!(units[0] + facilities[0]) || !(units[1] + facilities[1]))
        {
            if (pendingBattle == s) { pendingBattle = 0; battle[size_t(s)] = false; }
            return;
        }
        const int holder = owner[size_t(s)] == 0 ? 0 : 1;
        if (lost)
        {
            // fought on the battle map: each object whose battle units are all gone is taken out of its counter (the
            // battle map's 0x8057 on its last unit; 0x43A800 counts what's left for the report): a counter left
            // empty goes from the sector; the side with nothing left lost
            int gone[2] = { 0, 0 }, goneCrew[2] = { 0, 0 }, goneFacilities[2] = { 0, 0 };
            std::vector<int> emptied;
            for (int ref : *lost)
                for (size_t i = 0; i < counters.size(); i++)
                {
                    Counter& k = counters[i];
                    if (k.sector != s || k.Travelling()) continue;
                    auto it = std::find(k.refs.begin(), k.refs.end(), ref);
                    if (it == k.refs.end()) continue;
                    const int side = k.players ? 0 : 1;
                    Counter one = k;
                    one.refs = { ref };
                    one.count = 1;
                    if (k.kind == 0) { gone[side]++; goneCrew[side] += CrewOf(one); }
                    else goneFacilities[side]++;
                    k.refs.erase(it);
                    k.count = std::max(0, k.count - 1);
                    destroyed.push_back(ref);
                    if (k.count == 0) emptied.push_back(int(i));
                    break;
                }
            for (int i : emptied)
            {
                FreeCounter(i);
                counters[size_t(i)].sector = 0;
                counters[size_t(i)].refs.clear();
            }
            selection.erase(std::remove_if(selection.begin(), selection.end(), [&](int i) {
                return std::find(emptied.begin(), emptied.end(), i) != emptied.end(); }), selection.end());
            int left[2] = { 0, 0 };
            for (const Counter& k : counters)
                if (k.sector == s && !k.Travelling() && k.kind <= 1) left[k.players ? 0 : 1] += k.count;
            const int winner = left[0] && !left[1] ? 0 : left[1] && !left[0] ? 1 : holder;   // relative
            const bool human = res.Human();
            auto absolute = [&](int rel) { return human ? rel : 1 - rel; };
            report.sector = s;
            report.winner = absolute(winner);
            report.loser = absolute(1 - winner);
            report.gameOver = (s == 3 && report.winner == 0) || (s == 30 && report.winner == 1);
            debriefing.fought = true;
            debriefing.survivors = left[0] != 0;
            debriefing.lostUnits = gone[0];
            debriefing.lostVehicles = goneCrew[0];
            debriefing.lostFacilities = goneFacilities[0];
            debriefing.killedUnits = gone[1];
            debriefing.killedVehicles = goneCrew[1];
            debriefing.killedFacilities = goneFacilities[1];
            Log(res.String(0x210) + " " + res.String(warmap::NameText(s)) + (winner == 0 ? ": won" : ": lost")); // the engine's own
            RecomputeOwner(s);
            RefreshSector(s);
            battle[size_t(s)] = false;
            if (pendingBattle == s) pendingBattle = 0;
            RevealAfterBattle(s, 0);
            return;
        }
        const int winner = men[0] > men[1] ? 0 : men[1] > men[0] ? 1 : holder; // relative: 0 the player
        const bool human = res.Human();
        auto absolute = [&](int rel) { return human ? rel : 1 - rel; };
        report.sector = s;
        report.winner = absolute(winner);
        report.loser = absolute(1 - winner);
        report.gameOver = (s == 3 && report.winner == 0) || (s == 30 && report.winner == 1);
        debriefing.fought = true;
        debriefing.survivors = winner == 0;
        const int loser = 1 - winner;
        (loser == 0 ? debriefing.lostUnits : debriefing.killedUnits) = units[loser];
        (loser == 0 ? debriefing.lostVehicles : debriefing.killedVehicles) = vehicles[loser];
        (loser == 0 ? debriefing.lostFacilities : debriefing.killedFacilities) = facilities[loser];
        Log(res.String(0x210) + " " + res.String(warmap::NameText(s)) + (winner == 0 ? ": won" : ": lost")); // the engine's own
        ClearSide(s, winner != 0);
        battle[size_t(s)] = false;
        if (pendingBattle == s) pendingBattle = 0;
        RevealAfterBattle(s, 0);
    }

    // Back on the war map (0x419AA2, then the view's 0x418980): the result is announced (0x808D -> 0x41AED0) and
    // the queued events come again (0x41E730).
    void WarMap::ReturnFromBattle()
    {
        for (auto& r : research) if (r) r->UpdateTriggers(encountered); // the view's set-up, 0x418D17
        if (debriefing.fought) AnnounceBattleResult();
        battleSector = 0;
        NextEvent();
    }

    // The completed-object events' shared part (0x41ECD0 / 0x41EDC0): "<the object's name> <suffix>" (0x440720:
    // its type's name, record 21), with the option on the view (0x419810) and a box for the object's sector, and the
    // text in the messages.
    void WarMap::ObjectEvent(int counter, int suffix, bool box, int view)
    {
        const Counter* k = counter >= 0 && size_t(counter) < counters.size() ? &counters[size_t(counter)] : nullptr;
        std::string name;
        if (k)
            if (auto unit = unitData.find(k->type); unit != unitData.end()) name = res.String(unit->second.nameText);
        std::string text = name + " " + res.String(suffix);
        if (box) { SetView(view); OpenBox(text, k ? k->sector : 0, {}, 0x8122); }
        Log(text);
    }

    // 0x41AED0: a winner and a loser: the player won: EndGameWon (event 18, with the other side's home sector)
    // if the game is over, else sound 0x13C; lost: EndGameLost (19) if over, else 0x140. No winner: 0x13D or
    // 0x13F.
    void WarMap::AnnounceBattleResult()
    {
        const int player = res.Human() ? 0 : 1;
        if (report.winner != report.loser)
        {
            if (report.winner == player)
            {
                if (report.gameOver) RaiseEvent(EndGameWon, res.Human() ? 3 : 30);
                else sounds.push_back(0x13C);
            }
            else
            {
                if (report.gameOver) RaiseEvent(EndGameLost, res.Human() ? 30 : 3);
                else sounds.push_back(0x140);
            }
        }
        else sounds.push_back(report.winner == player ? 0x13D : 0x13F);
    }

    // 0x4212A0: the game's ending, sent with 0x80FE: the Humans winning at sector 3 is 0x80FA (Human victory) or
    // 0x80FD (Martian defeat); the Martians winning at London (30) 0x80FB (Human defeat) or 0x80FC (Martian
    // victory); 0 when it isn't over.
    int WarMap::EndCode() const
    {
        if (!report.gameOver) return 0;
        const bool human = res.Human();
        if (report.sector == 3 && report.winner == 0) return human ? 0x80FA : 0x80FD;
        if (report.sector == 30 && report.winner == 1) return human ? 0x80FB : 0x80FC;
        return 0;
    }

    // Who holds a sector (0x43CA40, whenever an object goes in or out): the side that alone has units or
    // constructions there (less units counting themselves out, 0x8116, none yet); with both, it stays as it was;
    // with neither, no one.
    void WarMap::RecomputeOwner(int sector)
    {
        if (sector < 1 || sector > warmap::SectorCount) return;
        int mine = 0, theirs = 0;
        for (const Counter& k : counters)
            if (k.sector == sector && k.kind <= 1 && !k.Travelling()) (k.players ? mine : theirs) += k.count;
        int& o = owner[size_t(sector)];
        if (mine && theirs) return;
        o = mine ? 0 : theirs ? 1 : 2;
    }

    // The tick's check (0x41AB00), while no battle is waiting: the first sector with both sides' units or
    // constructions in it and no battle flagged (+0x124): undefended, it's UndefendedSectorLost (event 17) and the
    // holder's objects go (0x43A250); otherwise PodLanded (the pod flag +0x64) or BattleInSector, and the battle
    // waits there.
    void WarMap::DetectBattles()
    {
        std::array<int, warmap::SectorCount + 1> mine{}, theirs{};
        for (const Counter& k : counters)
            if (k.kind <= 1 && !k.Travelling() && k.sector >= 1 && k.sector <= warmap::SectorCount)
                (k.players ? mine : theirs)[size_t(k.sector)] += k.count;
        for (int s = 1; s <= warmap::SectorCount; s++)
        {
            if (!mine[size_t(s)] || !theirs[size_t(s)] || battle[size_t(s)]) continue;
            if (Undefended(s))
            {
                RaiseEvent(UndefendedSectorLost, s);
                ClearSide(s, owner[size_t(s)] == 0);
                return;
            }
            RaiseEvent(podFlag[size_t(s)] ? PodLanded : BattleInSector, s);
            pendingBattle = s;
            return;
        }
    }

    // 0x43C8D0: the holder's side (the other side's for a sector no one holds) has no units there and no class 7
    // construction (guns, turrets, wire, mines: AI.ojd class 7).
    bool WarMap::Undefended(int sector) const
    {
        const bool players = owner[size_t(sector)] == 0;
        for (const Counter& k : counters)
        {
            if (k.sector != sector || k.players != players || k.Travelling()) continue;
            if (k.kind == 0) return false;
            if (k.kind == 1)
            {
                auto c = objectClasses.find(k.type);
                if (c != objectClasses.end() && c->second == 7) return false;
            }
        }
        return true;
    }

    // 0x43A250: every unit, construction and resource of the side in the sector goes (out of the placement,
    // 0x420930, and out of the sector, 0x43C7C0); then the sector's holder is worked out again and it's laid out.
    void WarMap::RemoveObject(int counter, int ref)
    {
        if (counter < 0 || size_t(counter) >= counters.size()) return;
        Counter& k = counters[size_t(counter)];
        auto at = std::find(k.refs.begin(), k.refs.end(), ref);
        if (at != k.refs.end()) k.refs.erase(at);
        if (ref >= 0) { destroyed.push_back(ref); factories.erase(ref); }
        if (buildWindow && buildWindow->ref == ref) buildWindow.reset();
        const int sector = k.sector;
        if (--k.count > 0) { RefreshSector(sector); return; }
        FreeCounter(counter);
        k.sector = 0; k.refs.clear(); k.count = 0;
        selection.erase(std::remove(selection.begin(), selection.end(), counter), selection.end());
        RecomputeOwner(sector);
        RefreshSector(sector);
    }

    void WarMap::ClearSide(int sector, bool players)
    {
        std::vector<int> gone;
        for (size_t i = 0; i < counters.size(); i++)
            if (counters[i].sector == sector && counters[i].players == players && !counters[i].Travelling()) gone.push_back(int(i));
        for (int i : gone) FreeCounter(i);
        // the counters are taken out of the sector (index order kept for the rest: they're marked gone)
        for (int i : gone)
        {
            Counter& k = counters[size_t(i)];
            destroyed.insert(destroyed.end(), k.refs.begin(), k.refs.end());
            k.sector = 0; k.refs.clear(); k.count = 0;
        }
        selection.erase(std::remove_if(selection.begin(), selection.end(), [&](int i) {
            return std::find(gone.begin(), gone.end(), i) != gone.end(); }), selection.end());
        RecomputeOwner(sector);
        RefreshSector(sector);
    }

    // 0x41E5B0: a critical event (its test, the table's second column) waits while a box is up, or runs and puts
    // one up; any other runs at once.
    void WarMap::RaiseEvent(int event, int param, std::string text)
    {
        if (Critical(event))
        {
            if (boxUp) { eventQueue.push_back({ event, param, std::move(text) }); return; }
            RunEvent(event, param, text);
            boxUp = true;
            return;
        }
        RunEvent(event, param, text);
    }

    // The tests (0x41E420-0x41E4C0): the [Event] options (0x41E160-0x41E3E0; "Entered Neutral Sector" isn't in
    // the menu); BattleInSector and the two endings always; ResearchAlert, PodLanded and UndefendedSectorLost
    // unless the cheat 0x4D1261 is on; ConstructorOnline / FacilityOnline by 0x48B580 (taken as off).
    bool WarMap::Critical(int event) const
    {
        switch (event)
        {
        case BattleInSector: case EndGameWon: case EndGameLost: return true;
        case ResearchAlert: case PodLanded: case UndefendedSectorLost: return true;
        case ConstructionCompleted: return Option("Construction Completed", false);
        case UnitCompleted: return Option("Unit Manufactured", false);
        case ArrivedInSector: return Option("Unit Arrived At Destination", false);
        case ResearchCompleted: case ResearchAvailable: return Option("Research Completed", true);
        case ConstructorIdle: return Option("Constructor Idle", true);
        case FacilityIdle: return Option("Facility Idle", true);
        case TravelInterrupted: return Option("Travel Interrupted", true);
        case ResourceWarning: return Option("Resource Warning", false);
        case ResourceAlert: return Option("Resource Alert", true);
        case CommandCentreBuilt: return Option("Command Centre Built", false);
        case EnteringNeutralSector: return Option("Entered Neutral Sector", false);
        default: return false;
        }
    }

    // The handlers (0x41E850-0x41F4B0) of the events the war map raises so far.
    void WarMap::RunEvent(int event, int param, const std::string& eventText)
    {
        const bool human = res.Human();
        auto name = [&](int sector) { return res.String(warmap::NameText(sector)); };
        switch (event)
        {
        case BattleInSector: // 0x41E850
        {
            const int sector = param;
            battle[size_t(sector)] = true;
            if (owner[size_t(sector)] == 0)
            {
                OpenBox(res.String(0x4AF), sector, res.String(0x4B2), 0x8072); // "Enemy have attacked" / "Defend"
                sounds.push_back(0x138);
            }
            else
            {
                OpenBox(res.String(0x4B0), sector, res.String(0x4B1), 0x8072); // "Invasion commencing" / "Attack"
                sounds.push_back(sector == 3 || sector == 30 ? 0x13A : 0x139);
            }
            Log(res.String(0x210) + " " + name(sector));
            break;
        }
        case ConstructionCompleted: // 0x41EDC0: "<name> completed."; a box (when the option is on) with the view on
        {                           // constructions for a building (class 2), resources otherwise; sound 0x12F
            const Counter* k = param >= 0 && size_t(param) < counters.size() ? &counters[size_t(param)] : nullptr;
            auto unit = k ? unitData.find(k->type) : unitData.end();
            const bool building = unit != unitData.end() && unit->second.category == 2;
            ObjectEvent(param, 0x4B4, Option("Construction Completed", false), building ? 2 : 3);
            auto cls = k ? objectClasses.find(k->type) : objectClasses.end();
            if (cls == objectClasses.end() || cls->second != 7) sounds.push_back(0x12F); // not for class 7
            break;
        }
        case UnitCompleted: // 0x41ECD0: "<name> unit completed."; sound 0x131 for a type of one battle unit, 0x1C0
        {
            ObjectEvent(param, 0x4B3, Option("Unit Manufactured", false), 1);
            const Counter* k = param >= 0 && size_t(param) < counters.size() ? &counters[size_t(param)] : nullptr;
            auto unit = k ? unitData.find(k->type) : unitData.end();
            sounds.push_back(unit != unitData.end() && unit->second.battleUnits == 1 ? 0x131 : 0x1C0);
            break;
        }
        case ResearchCompleted: // 0x41EB90 / 0x41EC30: "<heading>: <name>" in a box with no sector, "<heading> ..
        case ResearchAvailable: // <name>" in the messages, and the heading's speech (0x414700)
        {
            const int heading = event == ResearchCompleted ? 0x46B : 0x46C;
            if (Option("Research Completed", true)) OpenBox(res.String(heading) + ": " + eventText, 0, {}, 0x8122);
            Log(res.String(heading) + " .. " + eventText);
            if (int speech = res.Text().Extra(heading)) sounds.push_back(speech);
            break;
        }
        case ResearchAlert: // 0x41F220: for the local player's research (its +0x88, param 1), "No research is
            if (param)      // currently in progress" and sound 0x135; otherwise nothing (and the queue waits)
            {
                OpenBox(res.String(0x421), 0, {}, 0x8122);
                sounds.push_back(0x135);
            }
            break;
        case ConstructorIdle: // 0x41EEA0 / 0x41EF10: with the option on, the units / constructions view and "Vehicle
        case FacilityIdle:    // idle" (0x416) / "Facility idle" (0x417) for the object's sector; the message either way
        {
            const int text = event == ConstructorIdle ? 0x416 : 0x417;
            if (Option(event == ConstructorIdle ? "Constructor Idle" : "Facility Idle", true))
            {
                SetView(event == ConstructorIdle ? 1 : 2);
                const int sector = param >= 0 && size_t(param) < counters.size() ? counters[size_t(param)].sector : 0;
                OpenBox(res.String(text), sector, {}, 0x8122);
            }
            Log(res.String(text));
            break;
        }
        case CommandCentreBuilt: // 0x41F1C0: "Command centre built" (0x415) for the sector, on the constructions view
            if (Option("Command Centre Built", false)) { SetView(2); OpenBox(res.String(0x415), param, {}, 0x8122); }
            Log(res.String(0x415));
            break;
        case ArrivedInSector: // 0x41EAE0
        {
            std::string text = res.String(0x211) + " " + name(param);
            if (Option("Unit Arrived At Destination", false)) { SetView(1); OpenBox(text, param, {}, 0x8122); }
            Log(text);
            sounds.push_back(0x120);
            break;
        }
        case TravelInterrupted: // 0x41EF80
        {
            int sector = param >= 0 && size_t(param) < counters.size() ? counters[size_t(param)].sector : 0;
            if (Option("Travel Interrupted", true)) { SetView(1); OpenBox(res.String(0x419), sector, {}, 0x8122); }
            Log(res.String(0x419));
            sounds.push_back(0x147);
            break;
        }
        case PodLanded: // 0x41F010: in a Martian sector a note; in one no one holds a battle to come
        {
            const int sector = param;
            if (!human) SetView(1);
            const bool martians = owner[size_t(sector)] == (human ? 1 : 0);
            std::string text;
            if (martians)
            {
                sounds.push_back(0x1B2);
                text = res.String(0x41E) + ": " + name(sector);
                OpenBox(text, sector, {}, 0x8122);
            }
            else
            {
                battle[size_t(sector)] = true;
                sounds.push_back(0x1B3);
                text = res.String(human ? 0x420 : 0x41F) + ": " + name(sector);
                OpenBox(text, sector, res.String(human ? 0x4B2 : 0x4B1), 0x8072);
            }
            Log(text);
            break;
        }
        case UndefendedSectorLost: // 0x41E960: told before the sector goes over
        {
            const int sector = param;
            const int what = owner[size_t(sector)] == 0 ? 0x49E : 0x49F; // "Undefended Sector Lost" / "... captured"
            std::string box, line;
            if (podFlag[size_t(sector)])
            {
                box = res.String(0x41E) + "\n";
                line = res.String(0x41E) + "; ";
                sounds.push_back(0x1B2);
            }
            box += res.String(what) + ": " + name(sector);
            line += res.String(what) + ": " + name(sector);
            OpenBox(box, sector, {}, 0x8122);
            SetView(1);
            Log(line);
            podFlag[size_t(sector)] = false;
            break;
        }
        case EndGameWon: // 0x41F400: "Victory is ours!", sound 0x13E; the world stops and the game hears 0x80FE with
                         // the ending after the sound and 30 ticks more
            SetView(1);
            OpenBox(res.String(0x533), param, {}, 0x8122);
            Log(res.String(0x533));
            sounds.push_back(0x13E);
            ending = true;
            endingCode = EndCode();
            endingDelay = 30 + 150; // (the sound's length isn't known here: 5 seconds taken)
            break;
        case EndGameLost: // 0x41F4B0: the world stops and the game hears 0x80FE after 30 ticks
            SetView(1);
            ending = true;
            endingCode = EndCode();
            endingDelay = 30;
            break;
        case ResourceWarning: case ResourceAlert: // 0x41F280 / 0x41F340
        {
            static constexpr int WarningSound[6] = { 0x124, 0x122, 0x123, 0x125, 0x126, 0x127 };
            static constexpr int AlertSound[6] = { 0x12A, 0x128, 0x129, 0x12B, 0x12C, 0x12D };
            const int kind = (param & 0xFFFF) + (human ? 0 : 3), sector = param >> 16;
            std::string text = res.String(0x41B) + ": " + res.String(0x214 + kind);
            if (event == ResourceAlert ? Option("Resource Alert", true) : Option("Resource Warning", false))
                OpenBox(text, sector, {}, 0x8122);
            Log(text);
            sounds.push_back(event == ResourceAlert ? AlertSound[kind] : WarningSound[kind]);
            break;
        }
        default:
            break;
        }
    }

    // 0x41E730 (0x8122, a box gone): the next queued event comes (its handler puts its box up), or, with none
    // left, no box is up any more.
    void WarMap::NextEvent()
    {
        if (eventQueue.empty()) { boxUp = false; return; }
        QueuedEvent e = std::move(eventQueue.front());
        eventQueue.pop_front();
        RunEvent(e.event, e.param, e.text);
    }

    // 0x41E4E0: the InfoBox with the text, the second line and the command; time stops (0x41A6C0(0)), the view
    // goes to the sector (0x41E000, which the game glides; the engine jumps), sound 0x198.
    void WarMap::OpenBox(std::string text, int sector, std::string text2, int command)
    {
        infoBox = InfoBoxState{ std::move(text), std::move(text2), command, false, sector };
        SetTimeRunning(false);
        if (sector >= 1 && sector <= warmap::SectorCount)
        {
            const warmap::SectorInfo& r = warmap::Sectors[sector];
            CentreOn((r.left + r.right) / 2, (r.top + r.bottom) / 2);
        }
        sounds.push_back(0x198);
    }

    // The InfoBox's commands (0x432B30): 0x8122 closes it and tells the war map (the next event); 0x8123 (Esc,
    // when it can be cancelled) does the same; anything else goes to the war map and the box closes, the queue
    // waiting (for a battle, until the war map comes back, 0x418980 -> 0x41E730).
    void WarMap::BoxCommand(int command)
    {
        const int sector = infoBox ? infoBox->sector : 0;
        infoBox.reset();
        if (command == 0x8122 || command == 0x8123) { NextEvent(); return; }
        if (command == 0x8072)
        {
            // the battle: the one waiting, or the box's (a pod in a sector no one holds)
            if (!pendingBattle) pendingBattle = sector;
            battleSector = pendingBattle;
            sent = Command{ BattleCommand, 0x8019 };
        }
    }

    // Its layout (0x432520): the backdrop (GType 0x68, hu_event / ma_event) centred, 4 pixels above the bottom; in
    // Martian games ma_scrn (0x69) at +0x0B, -0x5E drawn at 50%; the text (font 0x73, wrapped to 320 pixels,
    // centred, level 21) 0x2B below the backdrop's top (Human) or 0x17 below ma_scrn's (Martian), the second line (level 22) 16
    // below it; the button (GType 0x6A, hu_buton / ma_buton) at +0x52, +0x76 (Human) or +0x87, +0x28 (Martian)
    // whose text, "Press button to continue" (0x47F) or "... ESC to cancel" (0x480) when it can be cancelled, is its
    // tool tip (the same button as the popup menu's items, 0x42FB40), not drawn on it. Both lines are centred, as
    // the original shows them.
    Rect WarMap::BoxRect() const
    {
        Rect b = SpriteRect(0x68, 0, 0);
        return { (ScreenW - b.w) / 2, ScreenH - b.h - 4, b.w, b.h };
    }

    Rect WarMap::BoxButtonRect() const
    {
        Rect b = BoxRect();
        Rect button = SpriteRect(0x6A, 0, 0);
        bool human = res.Human();
        return { b.x + (human ? 0x52 : 0x87), b.y + (human ? 0x76 : 0x28), button.w, button.h };
    }

    void WarMap::DrawBox(Canvas& canvas) const
    {
        if (!infoBox) return;
        const bool human = res.Human();
        Rect b = BoxRect();
        canvas.DrawSprite(0x68, 0, b.x, b.y);
        if (!human) canvas.DrawSprite(0x69, 0, b.x + 0x0B, b.y - 0x5E, 0, Canvas::Blend::Translucent);
        const Font* font = res.GetFont(0x73);
        const int lineH = font ? font->Height() : 12;
        // the text wrapped at 320 pixels (0x4161E0), centred on the backdrop
        std::vector<std::string> lines;
        {
            std::string line, word;
            auto width = [&](const std::string& t) { return font ? font->Measure(t) : int(t.size()) * 8; };
            auto flushWord = [&]() {
                if (word.empty()) return;
                std::string tryLine = line.empty() ? word : line + " " + word;
                if (!line.empty() && width(tryLine) > 320) { lines.push_back(line); line = word; }
                else line = tryLine;
                word.clear();
            };
            for (char c : infoBox->text)
            {
                if (c == ' ') flushWord();
                else if (c == '\n') { flushWord(); lines.push_back(line); line.clear(); }
                else word += c;
            }
            flushWord();
            if (!line.empty()) lines.push_back(line);
        }
        // (in Martian games the text goes from ma_scrn's corner, not the backdrop's)
        const int ox = human ? b.x : b.x + 0x0B, oy = human ? b.y : b.y - 0x5E;
        int y = oy + (human ? 0x2B : 0x17);
        for (const std::string& line : lines)
        {
            canvas.DrawString(0x73, line, { ox, y, b.w, lineH }, TextCentreX, 21);
            y += lineH;
        }
        if (!infoBox->text2.empty())
            canvas.DrawString(0x73, infoBox->text2, { ox, y + 16, b.w, lineH }, TextCentreX, 22);
        Rect bt = BoxButtonRect();
        bool over = bt.Contains(mouseX, mouseY);
        canvas.DrawSprite(0x6A, over && res.GetFrame(0x6A, 1) ? 1 : 0, bt.x, bt.y);
    }

    // 0x41A6C0: a change of the time-running flag goes to both players' command streams (actions 0xA / 0xB for the
    // player; the opponent, 0x442A80 for the computer, answers with 0xC / 0xD), and their commands reach the world
    // as 0x8050 / 0x8054 (on) and 0x8051 / 0x8055 (off). With the computer both come at once, so time stopping is
    // what shows: every off sets the speed to 0.
    void WarMap::SetTimeRunning(bool on)
    {
        if (on == timeRunning) return;
        timeRunning = on;
        for (int side = 0; side < 2; side++)
        {
            if (on) timeVotes++;
            else
            {
                timeVotes--;
                SetSpeed(0);
            }
            calendarWaiting = timeVotes == 1;
        }
    }

    // The clock's speed (0x41F640), and with it the hours a tick; a speed of 0 stops time (0x41A6C0(0)).
    void WarMap::SetSpeed(int index)
    {
        speed = std::clamp(index, 0, int(std::size(warmap::Speeds)) - 1);
        if (warmap::Speeds[speed] == 0.0f) SetTimeRunning(false);
    }

    // 0x41AEA0: every sector must allow it (0x43D010): one where one side has units or constructions (0x422860,
    // 0x4228B0) while the other does too won't (a battle to come).
    bool WarMap::SpeedChangeAllowed() const
    {
        std::array<int, warmap::SectorCount + 1> mine{}, theirs{};
        for (const Counter& k : counters)
        {
            if (k.kind > 1 || k.Travelling() || k.sector < 1 || k.sector > warmap::SectorCount) continue;
            (k.players ? mine : theirs)[size_t(k.sector)] += k.count;
        }
        for (int s = 1; s <= warmap::SectorCount; s++)
            if (mine[size_t(s)] && theirs[size_t(s)]) return false;
        return true;
    }

    // The Warmap menu (0x41D120): Cancel (command 0x73, icon GType 0x1C RBHcncl, text 0x476), then the items the
    // active tool, the player's selection or the sector under the mouse (if it's the player's: it's selected
    // and the selection cancelled first) adds through 0x805C (Build and Suspend / Resume building tasks; Cancel
    // Move and Information aren't in yet). Shown at the mouse
    // (0x432FB0): the first item centred on it (a quarter of its height above), kept 3 pixels inside the screen,
    // sound 0x193. Items with the same command would share a row (0x4331C0), only the hovered one's opened out
    // (0x433330); the rows go down the column (0x433220).
    void WarMap::OpenWarmapMenu(int x, int y)
    {
        popup.clear();
        popupHover = -1;
        int sector = SectorAt(x, y);
        if (sector && owner[size_t(sector)] == 0 && selection.empty()) selected = sector;
        auto add = [&](int command, int param, int icon, int text) {
            Rect r = SpriteRect(icon, 0, 0);
            if (r.w <= 0) r.w = r.h = 32;
            PopupItem item{ command, icon, text, r, param, {} };
            // 0x432D70 / 0x432E90: "View map" (0x475) takes the hovered sector's name (view +0x5C)
            if (text == 0x475 && sector) item.label = res.String(warmap::NameText(sector));
            popup.push_back(std::move(item));
        };
        add(0x73, 0, 0x1C, 0x476);
        // then the items of whoever answers 0x805C: the current object, else the own sector under the mouse,
        // selected, else the world's selected sector (+0x60). A sector adds "View map", 0x8133 with 0x8019
        // (0x43B9F0), while its battle map isn't open (+0x18). An object (0x429A70) with a factory, not on its
        // way anywhere, and a vehicle or a finished construction (0x42AA70), adds "Build" (0x813E with 0x8065,
        // icon 0x20, 0x470) and, with something queued, "Suspend building tasks" (0x813F, icon 0x22, 0x495) while
        // busy or "Resume building tasks" (icon 0x23, 0x496). The current object here is the first selected
        // counter's first object (*provisional*: the war map's own idea of it isn't traced).
        popupObject = -1;
        if (!selection.empty())
        {
            const Counter& c = counters[size_t(selection.front())];
            const int ref = c.refs.empty() ? -1 : c.refs.front();
            auto f = factories.find(ref);
            if (f != factories.end() && !c.Travelling() && c.players && (c.kind == 0 || !c.unfinished))
            {
                popupObject = ref;
                add(0x813E, 0x8065, 0x20, 0x470);
                if (!f->second.queue.entries.empty())
                {
                    if (f->second.queue.busy) add(0x813E, 0x813F, 0x22, 0x495);
                    else add(0x813E, 0x813F, 0x23, 0x496);
                }
            }
        }
        else if (selected >= 1 && selected <= warmap::SectorCount) add(0x8133, 0x8019, 0x2E, 0x475);
        int w = 0, h = 0;
        for (const PopupItem& item : popup) { w = std::max(w, item.rect.w); h += item.rect.h; }
        int left = x - popup[0].rect.w / 2, top = y - popup[0].rect.h / 4;
        left = std::clamp(left, 3, ScreenW - 3 - w);
        top = std::clamp(top, 3, ScreenH - 3 - h);
        int at = top;
        for (PopupItem& item : popup) { item.rect.x = left; item.rect.y = at; at += item.rect.h; }
        sounds.push_back(0x193);
    }

    void WarMap::ClosePopup(bool escape)
    {
        popup.clear();
        popupHover = -1;
        if (escape) sounds.push_back(0x196);
    }

    // A popup command reaching the war map (and the sector, 0x43AF47): 0x73, Cancel, is the player's action 2,
    // the selection dropped. "View map" (0x8133 with 0x8019) is the world's briefing transition for its selected
    // sector (0x41B210, then the Briefing for +0x60).
    void WarMap::PopupCommand(int command, int param)
    {
        if (command == 0x73) ClearSelection();
        // an object's (0x428DFD): Build opens its window, the other its building tasks suspended or resumed
        if (command == 0x813E && param == 0x8065) OpenBuildWindow(popupObject);
        if (command == 0x813E && param == 0x813F)
        {
            auto f = factories.find(popupObject);
            if (f != factories.end()) SetBusy(popupObject, !f->second.queue.busy);
        }
        if (command == BattleCommand && selected >= 1 && selected <= warmap::SectorCount)
        {
            battleSector = selected;
            sent = Command{ BattleCommand, param };
        }
    }

    void WarMap::OnMouseMove(int x, int y)
    {
        mouseX = x;
        mouseY = y;
        if (!popup.empty())
        {
            popupHover = -1;
            for (size_t i = 0; i < popup.size(); i++) if (popup[i].rect.Contains(x, y)) popupHover = int(i);
            return;
        }
        if (buildWindow) buildWindow->hover = BuildWindowHit(x, y);
        if (draggingSpeed) SetSpeedFromX(x);
        if (dragging)
        {
            Rect mini = MiniMapRect();
            CentreOn((x - mini.x) * warmap::MapWidth / MiniW, (y - mini.y) * warmap::MapHeight / MiniH);
        }
        if (leftHeld && pressedCounter < 0)
        {
            if (!rubberBand && (std::abs(x - dragX) > 2 || std::abs(y - dragY) > 2)) rubberBand = true;
            if (rubberBand) UpdateBand();
        }
    }

    bool WarMap::Selected(int counter) const
    {
        return std::find(selection.begin(), selection.end(), counter) != selection.end();
    }

    // While the rubber band is dragged the sectors are told 0x8010 with it (0x43C2B0): each counter of the view's
    // kind of the player's, its corner inside (PtInRect, so not on the right or bottom edge), is lit (0x440680)
    // and counted (world +0x114).
    void WarMap::UpdateBand()
    {
        int left = std::min(dragX, mouseX) + cameraX, right = std::max(dragX, mouseX) + cameraX;
        int top = std::min(dragY, mouseY) + cameraY, bottom = std::max(dragY, mouseY) + cameraY;
        highlighted = 0;
        for (Counter& k : counters)
        {
            k.inBand = k.players && k.kind == viewMode - 1 && !k.Travelling() &&
                       k.x >= left && k.x < right && k.y >= top && k.y < bottom;
            if (k.inBand) highlighted++;
        }
    }

    // The route preview (0x41D590), made again whenever the mouse goes onto another sector (0x41D470): in the Unit
    // view, with units selected and the rubber band not out, the selected units are put in groups by the sector
    // they're in and how they go (0x419520: naval, the unit's +0x52 5 or 6; 7; or the rest), and each group gets a
    // route to the sector, its arrow shown (+0x5F). Cursor mode 2 when any of them can go (0x41FF70 counts
    // them, +0x5D), back to 0 from 2 when none can.
    void WarMap::BuildPreview(int sector)
    {
        preview.clear();
        previewSector = sector;
        bool any = false;
        if (viewMode == 1 && !rubberBand && !selection.empty() && sector)
        {
            std::map<int, std::vector<int>> groups;
            for (int i : selection)
            {
                const Counter& k = counters[size_t(i)];
                if (k.Travelling() || k.sector < 1 || k.sector > warmap::SectorCount) continue;
                auto unit = unitData.find(k.type);
                int f52 = unit == unitData.end() ? 0 : unit->second.field52;
                int how = f52 == 5 || f52 == 6 ? 2 : f52 == 7 ? 1 : 0;
                groups[k.sector * 3 + how].push_back(i);
            }
            for (auto& [key, group] : groups)
            {
                Preview p;
                p.group = group;
                if (key / 3 != sector) p.path = PlanRoute(group, sector);
                if (p.path.size() > 1)
                {
                    any = true;
                    const Counter& first = counters[size_t(group.front())];
                    p.shape.arrow = RouteCurve(p.path, key % 3 == 2);
                    p.shape.attack = owner[size_t(sector)] == 1;
                    p.shape.path = p.path;
                    // the label's hours (0x4243E2): the route's length over the slowest unit's speed
                    float length = 0;
                    for (size_t n = 1; n < p.path.size(); n++)
                    {
                        auto a = warmap::Waypoint(p.path[n - 1]), b = warmap::Waypoint(p.path[n]);
                        int dx = b.x - a.x, dy = b.y - a.y;
                        length += float(std::sqrt(double(dx * dx + dy * dy)));
                    }
                    for (int g : group) p.shape.labelHours = std::max(p.shape.labelHours, length / PixelsPerHour(counters[size_t(g)]));
                    for (int i = 0; i < 6; i++) p.shape.labelFrom[i] = game.dateTime[i];
                    (void)first;
                }
                preview.push_back(std::move(p));
            }
        }
        if (any) cursorMode = 2;
        else if (cursorMode == 2) cursorMode = 0;
    }

    std::optional<Command> WarMap::OnMouseButton(int x, int y, bool down)
    {
        // the box is modal and has the mouse: only its button answers
        if (infoBox)
        {
            if (!down && BoxButtonRect().Contains(x, y)) BoxCommand(infoBox->command);
            return std::nullopt;
        }
        // the popup has the mouse (0x42E9B0): a click on an item sends its command to the war map and closes it
        // (0x4330B0), one anywhere else just closes it
        if (!popup.empty())
        {
            if (!down) return std::nullopt;
            for (const PopupItem& item : popup)
                if (item.rect.Contains(x, y)) { int command = item.command, param = item.param; ClosePopup(false); PopupCommand(command, param); return std::nullopt; }
            ClosePopup(false);
            return std::nullopt;
        }
        if (BuildWindowClick(x, y, down)) return std::nullopt;
        Rect mini = MiniMapRect();
        PanelLayout panel = Panel();
        for (int i = 0; i < 3; i++)
            if (down && panel.buttons[i].Contains(x, y)) { SetView(i + 1); return std::nullopt; } // 0x811F -> 0x419810
        // the Research button acts when the button comes up over it
        if (down && panel.research.Contains(x, y)) { researchHeld = true; return std::nullopt; }
        if (!down && researchHeld)
        {
            researchHeld = false;
            if (panel.research.Contains(x, y)) { SetTimeRunning(false); return Command{ ResearchCommand, 0 }; }
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
        if (!down && dragging) { dragging = false; return std::nullopt; }
        const bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
        if (down)
        {
            // a counter of the player's takes the mouse (0x42BEB0 0x201); otherwise the band starts (0x201); either
            // way time stops (0x41A6C0(0))
            SetTimeRunning(false);
            leftHeld = true;
            rubberBand = false;
            dragX = x; dragY = y;
            int counter = CounterAt(x, y);
            pressedCounter = counter >= 0 && counters[size_t(counter)].players ? counter : -1;
            return std::nullopt;
        }
        if (!leftHeld) return std::nullopt;
        leftHeld = false;
        if (pressedCounter >= 0)
        {
            // a click on a counter of the player's (0x42C160): in mode 1 with Shift it's dropped from the selection
            // if it's in it and added if not; in modes 0 and 1 otherwise the selection is just it; in modes 2 and
            // 3 nothing
            int counter = pressedCounter;
            pressedCounter = -1;
            if (cursorMode == 1 && shift)
            {
                if (Selected(counter)) selection.erase(std::remove(selection.begin(), selection.end(), counter), selection.end());
                else selection.push_back(counter);
            }
            else if (cursorMode <= 1) selection = { counter };
            previewSector = -1;
            return std::nullopt;
        }
        // 0x41D300: a rubber band with counters in it selects them (the player's action 4, then 5 for each, the
        // sectors told 0x8011, 0x43C340), cursor mode 1
        if (rubberBand)
        {
            rubberBand = false;
            if (highlighted > 0)
            {
                selection.clear();
                for (size_t i = 0; i < counters.size(); i++)
                    if (counters[i].inBand) selection.push_back(int(i));
                cursorMode = 1;
            }
            for (Counter& k : counters) k.inBand = false;
            highlighted = 0;
            previewSector = -1;
            return std::nullopt;
        }
        int sector = SectorAt(x, y);
        // in cursor modes 2 and 3 the click goes to the sector (0x43C3F0): in mode 2, with routes heading for it,
        // the selection is sent there (the player's command 8, then action 2), sound 0x11F
        if (cursorMode == 2 || cursorMode == 3)
        {
            if (cursorMode == 2 && sector && sector == previewSector)
            {
                bool sent = false;
                for (const Preview& p : preview)
                {
                    if (p.path.size() < 2) continue;
                    for (size_t n = 0; n < p.group.size(); n++) StartRoute(p.group[n], p.path, int(n));
                    sent = true;
                }
                if (sent)
                {
                    ArriveTogether();
                    sounds.push_back(0x11F);
                    ClearSelection();
                    preview.clear();
                    cursorMode = 0;
                    message.clear();
                }
            }
            return std::nullopt;
        }
        // otherwise the sector is selected if it's the player's (or with the cheats), and the player's action 2
        if (sector && sector != selected && owner[size_t(sector)] == 0)
        {
            selected = sector;
            ClearSelection();
        }
        return std::nullopt;
    }

    // The cursor (0x4EA438, set by whatever the mouse is over when told 0x20 with the mode): over a counter of
    // the player's (0x42BEB0) 1, or in mode 1 with Shift 3 if it's selected and 2 if not, and 12 in modes 2 and
    // 3; over a sector (0x43ADA0) in mode 0 the default if it's the player's and 12 if not, in mode 2 4 to move
    // into one of the player's or no one's and 5 into the other side's.
    int WarMap::CursorAt(int x, int y) const
    {
        PanelLayout panel = Panel();
        if (panel.frame.Contains(x, y) || panel.mini.Contains(x, y) || CalendarRect().Contains(x, y)) return 0;
        int counter = CounterAt(x, y);
        if (counter >= 0 && counters[size_t(counter)].players)
        {
            if (cursorMode == 0) return 1;
            if (cursorMode == 1) return (SDL_GetModState() & SDL_KMOD_SHIFT) ? (Selected(counter) ? 3 : 2) : 1;
            return 12;
        }
        int sector = SectorAt(x, y);
        if (!sector) return 0;
        if (cursorMode == 0) return owner[size_t(sector)] == 0 ? 0 : 12;
        if (cursorMode == 2) return owner[size_t(sector)] == 1 ? 5 : 4;
        return 0;
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
        // a scroll of the slider (0x4278A0 0x114 / 0x115): the speed, or 0 if a sector won't have it changed
        SetSpeed(SpeedChangeAllowed() ? std::clamp(positions * (x - sp.x - left) / span, 0, positions - 1) : 0);
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
            // the pendulum: frames 0-29 while stopped, 30-40 swinging while the clock runs, then with the sprite's
            // +0x14 bit 0 set: drawn at 50%
            canvas.DrawSprite(0x9B, s ? 30 + w.cycle * 11 / 30 : w.cycle, c.x + 0x54, c.y + 0x41, 0,
                              s ? Canvas::Blend::Translucent : Canvas::Blend::Opaque);
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
            if (k.kind != viewMode - 1 || (!k.shown && !(k.players && k.Travelling()))) continue;
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
        // RArrow's curve (0x4244E0 -> 0x424A40): the control points 2^5 apart, then five passes of subdivision.
        // Each pass gives the inner points the heading of the chord through their neighbours a step either side,
        // then puts each new midpoint between two points, bent by 5/11 (-(-0.4545)) of the average of two offsets
        // at right angles to the chord h (heading h - 0x400): tan(h - the start's heading) x half the chord, and
        // less tan(h - the end's heading) x half the chord (0x43FF30(a, b) is b - a), each held to half the
        // chord; the start's term is left out for the first stretch and the end's for the last.
        constexpr int Steps = 32;
        using P = Counter::ArrowPoint;
        if (control.size() < 2) return {};
        int count = int(control.size() - 1) * Steps + 1;
        std::vector<P> p(size_t(count), P{ 0, 0, 0 });
        for (size_t i = 0; i < control.size(); i++) p[i * Steps] = { control[i].first, control[i].second, 0 };
        for (int step = Steps; step >= 2; step /= 2)
        {
            // each inner point's heading: the chord from the point a step before to the one a step after
            // (0x424A63; the two ends keep theirs, which the bends below never use)
            for (int i = step; i < count - step; i += step)
                p[size_t(i)].heading = Heading(p[size_t(i - step)].x, p[size_t(i - step)].y, p[size_t(i + step)].x, p[size_t(i + step)].y);
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
                    auto [x, y] = Polar(h - 0x400, offset(AngleDiff(h + 0x800, b.heading + 0x800)));
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

    std::vector<int> WarMap::FindRoute(int from, int to, const std::vector<int>& group, std::string* why) const
    {
        // The game's route finder: the A* (warmap::AStar; set up by 0x424160) with 0x42ABD0 giving each step's
        // cost, or refusing it, the reason left at 0x4EA198 (by whichever call refused last, the A*'s estimates
        // included). The cost is 0 from a sector to itself, else the straight line between the two sectors'
        // waypoints (0x437380), whole pixels. A step is refused:
        //   0    a unit that can't move: its factory has something queued (0x42A410, a constructor at work)
        //   1/2  the link isn't one the unit can take: land units need a land link (bits 0-2), boats a sea one
        //        (bit 4); fliers take any
        //   3    neither end has its owner's command post
        //   4/5  into a sector other than the goal that has no command post, or isn't the player's
        //   6    the route's start has no command post and the step goes into a sector that isn't the player's
        //   7/8  into the goal when it would hold more than "Max units in sector" of the player's units, or "Max
        //        boats in sector" boats: those there, those on their way there (0x420060) and these
        // The route stands when the A* gets there with more than the start in it.
        int reason = -1;
        bool atWork = false;
        for (int g : group)
            for (int ref : counters[size_t(g)].refs) atWork = atWork || Working(ref);
        auto cost = [&](int a, int b) -> int {
            if (a == b) return 0;
            if (atWork) { reason = 0; return -1; }
            int kind = warmap::RouteLinkKind(a, b);
            // every unit of the group (0x42AC5E): land units need a land link, boats a sea one
            bool anyLand = false;
            for (int g : group)
            {
                const int mc = counters[size_t(g)].moveClass;
                if (mc <= 2 || mc == 4) anyLand = true;
                if (mc <= 2 && !(kind & warmap::RouteKind::Land)) { reason = 1; return -1; }
                if (mc == 3 && !(kind & warmap::RouteKind::Sea)) { reason = 2; return -1; }
            }
            if (!HasCommandPost(a) && !HasCommandPost(b)) { reason = 3; return -1; }
            if (b != to)
            {
                if (!HasCommandPost(b)) { reason = 4; return -1; }
                if (owner[size_t(b)] != 0) { reason = 5; return -1; }
            }
            if (!HasCommandPost(from) && owner[size_t(b)] != 0) { reason = 6; return -1; }
            if (b == to)
            {
                int mine = 0;
                for (int g : group) mine += counters[size_t(g)].count;
                int units = anyLand ? mine : 0, boats = anyLand ? 0 : mine;
                const bool boatLimit = counters[size_t(group.front())].moveClass == 3; // the first unit's class
                for (const Counter& c : counters)
                {
                    if (c.kind != 0 || std::find(group.begin(), group.end(), int(&c - counters.data())) != group.end()) continue;
                    if (!c.Travelling())
                    {
                        // 0x43D090 / 0x43D0F0: the player's units in the sector, boats (class 3) apart
                        if (c.players && c.sector == to) (c.moveClass == 3 ? boats : units) += c.count;
                    }
                    else if (c.path.back() == to) (c.naval ? boats : units) += c.count;
                }
                if (!boatLimit && units > maxUnits) { reason = 7; return -1; }
                if (boatLimit && boats > maxBoats) { reason = 8; return -1; }
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
        k.shown = true;
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
                if (k.sector == sector && k.players == players && k.kind == viewMode - 1 && !k.Travelling() && Visible(int(i))) here.push_back(int(i));
            }
        for (size_t i = 0; i < counters.size(); i++)
            if (counters[i].sector == sector && !counters[i].Travelling()) counters[i].shown = false;
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
        for (Counter& k : counters) { k.cell = -1; k.slideLeft = 0; k.shown = k.players && k.Travelling(); }
        for (int s = 1; s <= warmap::SectorCount; s++)
            for (bool players : { true, false })
                for (size_t i = 0; i < counters.size(); i++)
                {
                    const Counter& k = counters[i];
                    if (k.sector == s && k.players == players && k.kind == viewMode - 1 && !k.Travelling() && Visible(int(i))) PlaceCounter(int(i));
                }
    }

    // The war map's set-up after a battle (0x418E17, for each sector with +0x128 set): every object in the sector,
    // both sides', gets 336 hours of intel (0x422BA0), and so do those in up to `research` of its neighbours
    // (0x441AD0) that the other side holds, research being 3, 2 or 1 by which of three research items is
    // done (Human 0x588D94 / 0x588D90 / 0x588D8C, Martian 0x58906C / 0x589068 / 0x589064), 0 otherwise.
    // (Battles aren't in yet, so nothing calls it: the other side's counters stay hidden, as at the start of
    // the original's campaigns.)
    void WarMap::RevealAfterBattle(int sector, int research)
    {
        auto reveal = [&](int s) { for (Counter& k : counters) if (k.sector == s) k.intel = 0x150; };
        reveal(sector);
        for (const int* n = warmap::Neighbours(sector); *n >= 0 && research > 0; n++)
            if (owner[size_t(*n)] == 1) { reveal(*n); research--; }
        LayoutAll();
    }

    // An object is shown when it's the player's, or there's intel on it (+0x3C), or in a network game (0x421630).
    bool WarMap::Visible(int counter) const
    {
        const Counter& k = counters[size_t(counter)];
        return k.players || k.intel > 0;
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
        const Counter& k = counters[size_t(index)];
        if (sector == k.sector || k.Travelling()) return;
        std::string why;
        std::vector<int> path = PlanRoute({ index }, sector, &why);
        if (path.empty()) { message = why; return; }
        StartRoute(index, path, 0);
        ArriveTogether();
        message.clear();
    }

    // A route for a group of units from their sector (0x424160): the way (FindRoute, the start first), refused
    // unless it gets anywhere, and an attack (into a sector the other side holds) only to a neighbouring sector.
    std::vector<int> WarMap::PlanRoute(const std::vector<int>& group, int sector, std::string* why) const
    {
        const Counter& k = counters[size_t(group.front())];
        std::vector<int> path = FindRoute(k.sector, sector, group, why);
        std::string through;
        for (int s : path) through += " " + std::to_string(s);
        SDL_Log("War map: route from sector %d to %d for %d unit(s): %s", k.sector, sector, int(group.size()),
                path.empty() ? (why ? why->c_str() : "none") : ("through" + through).c_str());
        if (!path.empty() && owner[size_t(sector)] == 1 && path.size() != 1)
        {
            path.clear();
            if (why) *why = "An attack has to be on a neighbouring sector";
        }
        if (!path.empty()) path.insert(path.begin(), k.sector);
        return path;
    }

    std::vector<WarMap::Counter::ArrowPoint> WarMap::RouteCurve(const std::vector<int>& path, bool naval) const
    {
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
            if (naval || (at.x == 0 && at.y == 0)) at = warmap::SeaWaypoint(path[n]);
            control.push_back({ float(at.x), float(at.y) });
        }
        return ArrowCurve(control);
    }

    // A unit set off on a route (0x424160 and its RouteEntry 0x42AFE0), the n-th of its group.
    void WarMap::StartRoute(int index, const std::vector<int>& path, int n)
    {
        Counter& k = counters[size_t(index)];
        const int sector = path.back();
        k.moved = true;
        k.path = path;
        // out of its sector and into the route's list: the sector it leaves may change hands (0x43CA40)
        RecomputeOwner(path.front());
        k.attack = owner[size_t(sector)] == 1; // +0x5E: not when the goal is the side's own or no one's
        auto unit = unitData.find(k.type);
        k.naval = unit != unitData.end() && (unit->second.field52 == 5 || unit->second.field52 == 6);
        k.arrow = RouteCurve(path, k.naval);
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
        k.offsetX = (n % clunWidth - clunWidth / 2) * clunSpacing;
        k.offsetY = (n / clunWidth - clunWidth / 2) * clunSpacing;
        k.labelHours = k.hoursLeft;
        for (int i = 0; i < 6; i++) k.labelFrom[i] = game.dateTime[i];
    }
}
