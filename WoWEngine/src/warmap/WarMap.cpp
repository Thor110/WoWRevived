#include "warmap/WarMap.h"

#include "core/Random.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
        // in the counter font (GType 0x6E, right-aligned). An object saved at 0, 0 goes in the sector's next free
        // cell, in the game's spiral order (SectorCells).
        // a construction counts once it's finished: 0x42AA70 wants its +0x7C at least its +0x78, taken here as the
        // save's second float against its first (provisional: which float is which isn't traced; at the start
        // both are 1)
        for (const auto& o : objects)
            if ((o.list == 2 || o.list == 3) && o.sector >= 1 && o.sector <= warmap::SectorCount && o.value[1] >= o.value[0])
                finished[size_t(o.sector)].push_back(o.unitType);
        struct Group { int sector, list, type, x, y, count; };
        std::vector<Group> groups;
        for (const auto& o : objects)
        {
            if (o.sector < 1 || o.sector > warmap::SectorCount) continue;
            Group* g = nullptr;
            auto cls = objectClasses.find(o.unitType);
            if (cls != objectClasses.end() && cls->second == 7)
                for (Group& existing : groups)
                    if (existing.sector == o.sector && existing.list == o.list && existing.type == o.unitType) { g = &existing; break; }
            if (!g) { groups.push_back({ o.sector, o.list, o.unitType, 0, 0, 0 }); g = &groups.back(); }
            g->count++;
            if (!g->x && !g->y) { g->x = o.x; g->y = o.y; }
        }
        auto taken = [&](int x, int y, int kind) {
            for (const Counter& c : counters) if (c.kind == kind && c.x == x && c.y == y) return true;
            return false;
        };
        for (const Group& g : groups)
        {
            auto unit = units.find(g.type);
            Counter k;
            k.x = g.x; k.y = g.y;
            k.frame = unit == units.end() ? 0 : unit->second.counterFrame;
            k.speed = unit == units.end() || g.list / 2 != 0 ? 0 : unit->second.warMapSpeed;
            k.moveClass = unit == units.end() ? 0 : unit->second.moveClass;
            k.count = g.count;
            k.players = g.list % 2 == 0;
            k.kind = g.list / 2;
            k.sector = g.sector;
            if ((!k.x && !k.y) || taken(k.x, k.y, k.kind))
            {
                bool placed = false;
                for (const auto& [x, y] : SectorCells(g.sector))
                    if (!taken(x, y, k.kind)) { k.x = x; k.y = y; placed = true; break; }
                if (!placed) continue;
            }
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
        log.push_back({ std::move(text), LogLife, 0 });
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

    // What one more finished facility of a kind would add to a sector's output (0x424000): with B the sector's
    // deposit, n its finished facilities of the kind and r the drop-off rate, output(n) = n r B / (n + r - 1), and
    // the value is output(n + 1) - output(n); B with none yet.
    float WarMap::ResourceGain(int sector, int kind) const
    {
        auto it = deposits.find(sector);
        float b = it == deposits.end() ? 0.0f : float(it->second[size_t(kind)]);
        // the facility types of each kind (0x423920)
        static constexpr int Facility[6] = { 47, 48, 49, 108, 109, 110 };
        int n = 0;
        for (int type : finished[size_t(sector)]) if (type == Facility[kind]) n++;
        if (n == 0) return b;
        const float r = resourceDropOff, fn = float(n);
        return (fn + 1.0f) * r * b / (r + fn) - fn * r * b / (r + fn - 1.0f);
    }

    // The sectors' shading (0x4192F0 fills five tables, 0x41D9B0 turns them into levels of the map's shade table).
    // Table 0, the Unit view: the objects in each sector of ours (dword_4F7650, one more for each object put in
    // it), 0 for the rest, onto levels 17-35. Tables 2-4, the Resource view: what another facility of the side's
    // kind 0-2 would give in every sector, onto 78-88, the top fixed at 80. A sector of ours takes
    // lo + (v - min) / (max - min) x (hi - lo); in the Resource view so does every other; otherwise a sector no one
    // holds is at 11, and the other side's is 11 too except in the Unit view, where it's 0 (dark). Table 1, the
    // Construction view (67-77, top 1.0), is the sectors' production efficiency from the economy, which isn't in
    // yet, so that view leaves the sectors at 11.
    void WarMap::ShadeSectors()
    {
        levels.fill(warmap::NormalLevel);
        if (viewMode == 2) { PulseBattles(); return; }
        std::array<float, warmap::SectorCount + 1> v{};
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
        float range = hi - lo;
        if (range == 0.0f) range = 1.0f;
        const int first = viewMode == 1 ? 17 : 78, last = viewMode == 1 ? 35 : 88;
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

    // The world's 0x802D, every whole game hour (0x419E9D): the routes, the players, both sides' research, then
    // the pods. (Only the pods are in so far; the routes move every update in MoveCounters.)
    void WarMap::Hourly(int hours)
    {
        DropPod(hours);
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
        if (!human) viewMode = 1;
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

    // A unit put in a sector (0x43CEB0): its own counter in the sector's next free cell
    void WarMap::AddUnit(int type, int sector, bool players)
    {
        auto unit = unitData.find(type);
        Counter k;
        k.frame = unit == unitData.end() ? 0 : unit->second.counterFrame;
        k.speed = unit == unitData.end() ? 0 : unit->second.warMapSpeed;
        k.moveClass = unit == unitData.end() ? 0 : unit->second.moveClass;
        k.count = 1;
        k.players = players;
        k.kind = 0;
        k.sector = sector;
        for (const auto& [x, y] : SectorCells(sector))
        {
            bool taken = false;
            for (const Counter& c : counters) if (c.kind == 0 && c.x == x && c.y == y) { taken = true; break; }
            if (taken) continue;
            k.x = x; k.y = y; k.fx = float(x); k.fy = float(y);
            counters.push_back(k);
            return;
        }
    }

    void WarMap::MoveCounters(float hours)
    {
        // Counters on their way: routes (WWarMapRoute 0x4240D0) move on every game hour (message 0x802D, 0x425080)
        // from sector waypoint to sector waypoint, at the unit's own speed (see PixelsPerHour). Going into a sector
        // the other side holds stops them at its border: that's a battle, which needs the battle map. Arriving in a
        // sector no one holds takes it (provisional).
        for (size_t i = 0; i < counters.size(); i++)
        {
            Counter& k = counters[i];
            if (k.route.empty()) continue;
            float step = PixelsPerHour(k) * hours;
            float tx = float(k.route.front().first), ty = float(k.route.front().second);
            float dx = tx - k.fx, dy = ty - k.fy, dist = std::sqrt(dx * dx + dy * dy);
            if (dist > step) { k.fx += dx / dist * step; k.fy += dy / dist * step; }
            else
            {
                k.fx = tx; k.fy = ty;
                int next = k.routeSectors.front();
                k.route.erase(k.route.begin());
                k.routeSectors.erase(k.routeSectors.begin());
                k.routeWaypoint.erase(k.routeWaypoint.begin());
                if (next != k.sector && owner[size_t(next)] == 1)
                {
                    // BattleInSector (event 0, 0x41E850): "Battle in" and the sector, and the attack sound (0x139,
                    // or 0x13A for sector 3 or London); defending a sector of ours would be 0x138. The battle itself
                    // needs the battle map, so the counter stops at the border.
                    Log(res.String(0x210) + " " + res.String(warmap::NameText(next)));
                    sounds.push_back(next == 3 || next == 30 ? 0x13A : 0x139);
                    k.route.clear(); k.routeSectors.clear(); k.routeWaypoint.clear();
                    auto cell = FreeCell(k.sector, k.kind, int(i));
                    k.fx = float(cell.first); k.fy = float(cell.second);
                }
                else
                {
                    k.sector = next;
                    if (k.route.empty())
                    {
                        if (owner[size_t(next)] == 2) owner[size_t(next)] = 0;
                        // ArrivedInSector (event 3, 0x41EAE0): "Vehicle/s arrived in" and the sector, sound 0x120
                        Log(res.String(0x211) + " " + res.String(warmap::NameText(next)));
                        sounds.push_back(0x120);
                    }
                }
            }
            k.x = int(k.fx); k.y = int(k.fy);
        }

    }

    void WarMap::Update()
    {
        AgeLog();
        if (!map) map = res.GetFrame(warmap::MapGType);
        if (!sectorMap) sectorMap = res.GetFrame(warmap::SectorMapGType);

        animation++;
        MoveCounters(ClockTick());
        if (hoursPassed > 0) Hourly(hoursPassed);
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
            if (!k.route.empty() && k.kind == viewMode - 1) DrawRouteArrow(canvas, k);

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
        case SDLK_F1: viewMode = 1; break;
        case SDLK_F2: viewMode = 2; break;
        case SDLK_F3: case SDLK_F4: case SDLK_F5: viewMode = 3; resourceKind = int(key - SDLK_F3); break;
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
            if (viewMode == 1) viewMode = 2;
            else if (viewMode == 2) { viewMode = 3; resourceKind = 0; }
            else if (resourceKind < 2) resourceKind++;
            else viewMode = 1;
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
            if (down && panel.buttons[i].Contains(x, y)) { viewMode = i + 1; return std::nullopt; } // 0x811F -> 0x419810
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
            MoveCounters(ClockTick());
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
        int bodyEnd = std::max(0, n - Steps / 3 - 1);
        float plx, ply, prx, pry;
        edges(0, plx, ply, prx, pry);
        for (int i = 1; i <= bodyEnd; i++)
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
        // dword_4DAF9C), the text 1 pixel in; the hours still to go are what's left of the way at the counter's speed
        if (!k.attack) return;
        float left_ = 0, px = float(k.x), py = float(k.y);
        for (const auto& p : k.route)
        {
            left_ += std::hypot(float(p.first) - px, float(p.second) - py);
            px = float(p.first); py = float(p.second);
        }
        float perHour = PixelsPerHour(k);
        if (perHour <= 0) return;
        std::string label = warmap::ArrivalText(game.dateTime, left_ / perHour, human,
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
        // The game's route finder: an A* (0x44EFA0, set up by 0x44EE70 from 0x424160) over the route links, with
        // 0x42ABD0 giving each step's cost, or refusing it. The cost is the straight line between the two
        // sectors' waypoints (0x437380), whole pixels. A step is refused (the reason left at 0x4EA198):
        //   1/2  the link isn't one the unit can take (land units over land, boats by sea, fliers anywhere)
        //   3    neither end has its owner's command post
        //   4/5  a sector passed through (not the goal) has no command post, or isn't the player's
        //   6    the route's start has no command post and the step goes into a sector that isn't the player's
        //   7/8  the goal would hold more than "Max units in sector" units, or "Max boats in sector" boats
        // There's no estimate to the goal, so it finds the shortest way, as Dijkstra does.
        int reason = -1;
        auto allowed = [&](int a, int b) {
            int kind = warmap::RouteLinkKind(a, b);
            if (!kind) return false;
            if (k.moveClass <= 2 && !(kind & warmap::RouteKind::Land)) { reason = 1; return false; }
            if (k.moveClass == 3 && !(kind & warmap::RouteKind::Sea)) { reason = 2; return false; }
            if (!HasCommandPost(a) && !HasCommandPost(b)) { reason = 3; return false; }
            if (b != to)
            {
                if (!HasCommandPost(b)) { reason = 4; return false; }
                if (owner[size_t(b)] != 0) { reason = 5; return false; }
            }
            if (!HasCommandPost(from) && owner[size_t(b)] != 0) { reason = 6; return false; }
            if (b == to)
            {
                int units = k.count, boats = k.moveClass == 3 ? k.count : 0;
                for (const Counter& c : counters)
                    if (c.players && c.kind == 0 && c.sector == to && c.route.empty())
                        (c.moveClass == 3 ? boats : units) += c.count;
                if (k.moveClass != 3 && units > maxUnits) { reason = 7; return false; }
                if (k.moveClass == 3 && boats > maxBoats) { reason = 8; return false; }
            }
            return true;
        };
        std::array<int, warmap::SectorCount + 1> best;
        std::array<int, warmap::SectorCount + 1> previous;
        std::array<bool, warmap::SectorCount + 1> done{};
        best.fill(INT32_MAX);
        previous.fill(-1);
        best[size_t(from)] = 0;
        for (;;)
        {
            int s = -1;
            for (int n = 1; n <= warmap::SectorCount; n++)
                if (!done[size_t(n)] && best[size_t(n)] != INT32_MAX && (s < 0 || best[size_t(n)] < best[size_t(s)])) s = n;
            if (s < 0 || s == to) break;
            done[size_t(s)] = true;
            auto a = warmap::Waypoint(s);
            for (int n = 1; n <= warmap::SectorCount; n++)
            {
                if (done[size_t(n)] || !allowed(s, n)) continue;
                auto b = warmap::Waypoint(n);
                double dx = b.x - a.x, dy = b.y - a.y;
                int d = best[size_t(s)] + int(std::sqrt(dx * dx + dy * dy));
                if (d < best[size_t(n)]) { best[size_t(n)] = d; previous[size_t(n)] = s; }
            }
        }
        if (to == from || previous[size_t(to)] < 0)
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
        std::vector<int> path;
        for (int s = to; s != from; s = previous[size_t(s)]) path.insert(path.begin(), s);
        return path;
    }

    std::vector<std::pair<int, int>> WarMap::SectorCells(int sector) const
    {
        // Each sector has a grid of 36 pixel cells over its bounds (0x428110, rounded out to whole cells); a cell
        // is the sector's when CHI.SPR (GType 0x84) has the sector's number at its corner (0x49BF50). Counters
        // take cells in a square spiral from the grid's middle cell (0x4283F0 modes 2 and 3, which the war map
        // uses): the middle, then rings 1-15 (0x4282C0), each clockwise from the cell above the middle.
        std::vector<std::pair<int, int>> cells;
        if (sector < 1 || sector > warmap::SectorCount) return cells;
        const IndexedImage* map = res.GetFrame(0x84);
        const auto& b = warmap::Sectors[sector];
        constexpr int Cell = 36;
        int left = b.left / Cell * Cell, top = b.top / Cell * Cell;
        int w = ((b.right + Cell - 1) / Cell * Cell - left) / Cell, h = ((b.bottom + Cell - 1) / Cell * Cell - top) / Cell;
        int cx = w / 2, cy = h / 2;
        auto consider = [&](int dx, int dy) {
            int gx = cx + dx, gy = cy + dy;
            if (gx < 0 || gy < 0 || gx >= w || gy >= h) return;
            int x = left + gx * Cell, y = top + gy * Cell;
            if (map && x >= 0 && y >= 0 && x < map->width && y < map->height &&
                map->pixels[size_t(y) * size_t(map->width) + size_t(x)] == sector)
                cells.push_back({ x, y });
        };
        consider(0, 0);
        for (int r = 1; r < 16; r++)
        {
            int dx = 0, dy = -r;
            for (int i = 0; i < r; i++) consider(dx++, dy);
            for (int i = 0; i < 2 * r; i++) consider(dx, dy++);
            for (int i = 0; i < 2 * r; i++) consider(dx--, dy);
            for (int i = 0; i < 2 * r; i++) consider(dx, dy--);
            for (int i = 0; i < r; i++) consider(dx++, dy);
        }
        return cells;
    }

    std::pair<int, int> WarMap::FreeCell(int sector, int kind, int except) const
    {
        for (const auto& [x, y] : SectorCells(sector))
        {
            bool taken = false;
            for (size_t i = 0; i < counters.size(); i++)
                if (int(i) != except && counters[i].kind == kind && counters[i].x == x && counters[i].y == y) taken = true;
            if (!taken) return { x, y };
        }
        const auto& b = warmap::Sectors[sector];
        return { (b.left + b.right) / 2, (b.top + b.bottom) / 2 };
    }

    void WarMap::SendTo(int index, int sector)
    {
        Counter& k = counters[size_t(index)];
        if (sector == k.sector) return;
        std::string why;
        std::vector<int> path = FindRoute(k.sector, sector, k, &why);
        std::string through;
        for (int s : path) through += " " + std::to_string(s);
        SDL_Log("War map: route from sector %d to %d for unit class %d: %s", k.sector, sector, k.moveClass,
                path.empty() ? why.c_str() : ("through" + through).c_str());
        // an attack (into a sector the other side holds) has to be to a neighbouring sector (0x424160)
        if (!path.empty() && owner[size_t(sector)] == 1 && path.size() != 1) { path.clear(); why = "An attack has to be on a neighbouring sector"; }
        if (path.empty()) { message = why; return; }
        k.route.clear();
        k.routeSectors.clear();
        k.routeWaypoint.clear();
        // out of each sector across the link's two border points, through the next sector's waypoint (0x437380),
        // ending in a free cell of the last; the counter's corner follows the points less half its size
        int at = k.sector;
        for (size_t i = 0; i < path.size(); i++)
        {
            // land units cross at a link's land points, boats at its sea points, fliers whichever it has
            int kind = warmap::RouteLinkKind(at, path[i]);
            bool bySea = k.moveClass == 3 || (k.moveClass == 4 && !(kind & warmap::RouteKind::Land));
            const warmap::SectorLink* link = bySea ? warmap::SeaLink(at, path[i]) : warmap::LandLink(at, path[i]);
            if (link)
            {
                const int* p = bySea ? link->sea : link->land;
                k.route.push_back({ p[0] - 16, p[1] - 16 });
                k.routeSectors.push_back(at);
                k.routeWaypoint.push_back(false);
                k.route.push_back({ p[2] - 16, p[3] - 16 });
                k.routeSectors.push_back(path[i]);
                k.routeWaypoint.push_back(false);
            }
            auto w = warmap::Waypoint(path[i]);
            if (i + 1 < path.size()) k.route.push_back({ w.x - 16, w.y - 16 });
            else k.route.push_back(FreeCell(path[i], k.kind, index));
            k.routeSectors.push_back(path[i]);
            k.routeWaypoint.push_back(true);
            at = path[i];
        }
        // the arrow's control points (0x4244E0): where the route leaves the first sector (the link's land point, or
        // its sea point for a naval route), the waypoints of the sectors on the way, and the destination's waypoint
        {
            std::vector<std::pair<float, float>> control;
            int first = path.front();
            int kind = warmap::RouteLinkKind(k.sector, first);
            bool bySea = k.moveClass == 3 || (k.moveClass == 4 && !(kind & warmap::RouteKind::Land));
            const warmap::SectorLink* link = bySea ? warmap::SeaLink(k.sector, first) : warmap::LandLink(k.sector, first);
            if (link) control.push_back({ float(bySea ? link->sea[0] : link->land[0]), float(bySea ? link->sea[1] : link->land[1]) });
            else { auto w = warmap::Waypoint(k.sector); control.push_back({ float(w.x), float(w.y) }); }
            for (int s : path) { auto w = warmap::Waypoint(s); control.push_back({ float(w.x), float(w.y) }); }
            k.arrow = ArrowCurve(control);
            k.attack = owner[size_t(sector)] == 1;
        }
        k.fx = float(k.x);
        k.fy = float(k.y);
        message.clear();
    }
}
