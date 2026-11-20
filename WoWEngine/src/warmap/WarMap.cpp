#include "warmap/WarMap.h"

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
                   std::map<int, ObjOjd::UnitData> units, std::vector<int> owners, int scrollSpeed, float clunting)
        : Screen(Id, r, 0, warmap::PaletteGroup), colours(c), game(start), cluntingSpeed(clunting)
    {
        // One counter for each kind of object in a sector's list, with how many there are. The first object of a
        // kind carries the counter's position (cells of 36 pixels); the game places the rest itself. Kinds with no
        // position (all resources) go in the sector's first free cell, in the game's spiral order (SectorCells).
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
        return p;
    }

    Rect WarMap::MiniMapRect() const { return Panel().mini; }

    int WarMap::SectorAt(int x, int y) const
    {
        if (!sectorMap) return 0;
        int mx = cameraX + x, my = cameraY + y;
        if (mx < 0 || my < 0 || mx >= sectorMap->width || my >= sectorMap->height) return 0;
        return warmap::SectorOfValue(sectorMap->pixels[size_t(my) * size_t(sectorMap->width) + size_t(mx)]);
    }

    float WarMap::PixelsPerHour(const Counter& k) const
    {
        // 0x424160 gives a new route its arrival time: the route's length in map pixels (+0x68) over the slowest
        // unit's war map speed (unit +0x54, OBJ.ojd records 37/38) x 0.05, in game hours. A counter is one unit
        // type, so it's that type's speed: 100 goes 5 map pixels an hour.
        return k.speed > 0 ? float(k.speed) * 0.05f : cluntingSpeed;
    }

    void WarMap::Update()
    {
        if (!map) map = res.GetFrame(warmap::MapGType);
        if (!sectorMap) sectorMap = res.GetFrame(warmap::SectorMapGType);

        animation++;
        // the clock
        {
            float before = game.ticks;
            game.ticks += warmap::Speeds[speed] / 30.0f;
            int hours = int(game.ticks) - int(before);
            if (hours > 0) warmap::AdvanceDate(game.dateTime, hours);
        }

        // Counters on their way: routes (WWarMapRoute 0x4240D0) move on every game hour (message 0x802D, 0x425080)
        // from sector waypoint to sector waypoint, at the unit's own speed (see PixelsPerHour). Going into a sector
        // the other side holds stops them at its border: that's a battle, which needs the battle map. Arriving in a
        // sector no one holds takes it (provisional).
        for (size_t i = 0; i < counters.size(); i++)
        {
            Counter& k = counters[i];
            if (k.route.empty()) continue;
            float step = PixelsPerHour(k) * warmap::Speeds[speed] / 30.0f;
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
                    message = "Battle in " + res.String(warmap::NameText(next)) + ": the battle map isn't rebuilt yet";
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
                        message = res.String(warmap::NameText(next)) + ": arrived";
                    }
                }
            }
            k.x = int(k.fx); k.y = int(k.fy);
        }

        int dx = 0, dy = 0;
        if (keys[0]) dx -= scrollStep;
        if (keys[1]) dx += scrollStep;
        if (keys[2]) dy -= scrollStep;
        if (keys[3]) dy += scrollStep;
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
            if (k.count > 1)
                canvas.DrawString(0x6E, std::to_string(k.count), { x, y + 26, 32, 10 }, TextCentreX);
        }

        // the panel: its frame, the three view buttons (frame 2 for the one that's on, 1 under the mouse; the
        // buttons' frames aren't confirmed), then the mini-map: the whole map at a fifteenth, the sector under the
        // mouse lit, and the view's outline
        PanelLayout panel = Panel();
        canvas.DrawSprite(0x80, 0, panel.frame.x, panel.frame.y);
        static constexpr int ButtonGtypes[3] = { 0x33, 0x35, 0x34 };
        for (int i = 0; i < 3; i++)
            canvas.DrawSprite(ButtonGtypes[i], viewMode == i + 1 ? 2 : hotButton == i ? 1 : 0, panel.buttons[i].x, panel.buttons[i].y);
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

        // the sector under the mouse, and the date (provisional placement: the game's own text objects aren't
        // traced yet)
        std::string status;
        static constexpr int ButtonText[3] = { 0x47B, 0x47C, 0x47E }; // "Unit View", "Construction view", "Resource view"
        if (!message.empty()) status = message;
        if (hotButton >= 0) status = res.String(ButtonText[hotButton]);
        else if (hovered) status = res.String(warmap::NameText(hovered));
        DrawCalendar(canvas);
        {
            // a darkened strip behind the text so it reads on either map
            uint32_t* out = canvas.Pixels();
            int rows = status.empty() ? 0 : 16;
            for (int y = 2; y < 2 + rows; y++)
                for (int x = 4; x < 240; x++)
                {
                    uint32_t& p = out[size_t(y) * size_t(canvas.Width()) + size_t(x)];
                    p = (p >> 2) & 0x3F3F3F;
                }
        }
        if (!status.empty()) canvas.DrawString(0x72, status, { 8, 4, 300, 14 }, 0, colours.items);
    }

    std::optional<Command> WarMap::OnKey(int key)
    {
        switch (key)
        {
        case SDLK_LEFT: keys[0] = true; break;
        case SDLK_RIGHT: keys[1] = true; break;
        case SDLK_UP: keys[2] = true; break;
        case SDLK_DOWN: keys[3] = true; break;
        case SDLK_ESCAPE: return Command{ Commands::Back, 0 };
        // the game's speed (provisional keys: the war map's own speed controls aren't traced yet)
        case SDLK_EQUALS: case SDLK_KP_PLUS: speed = std::min(speed + 1, int(std::size(warmap::Speeds)) - 1); break;
        case SDLK_MINUS: case SDLK_KP_MINUS: speed = std::max(speed - 1, 0); break;
        default: break;
        }
        return std::nullopt;
    }

    void WarMap::OnKeyUp(int key)
    {
        switch (key)
        {
        case SDLK_LEFT: keys[0] = false; break;
        case SDLK_RIGHT: keys[1] = false; break;
        case SDLK_UP: keys[2] = false; break;
        case SDLK_DOWN: keys[3] = false; break;
        default: break;
        }
    }

    void WarMap::OnMouseMove(int x, int y)
    {
        mouseX = x;
        mouseY = y;
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
        Rect sp = SpeedRect();
        if (down && sp.Contains(x, y))
        {
            speed = std::clamp((x - sp.x) * int(std::size(warmap::Speeds)) / std::max(sp.w, 1), 0, int(std::size(warmap::Speeds)) - 1);
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
    Rect WarMap::SpeedRect() const
    {
        Rect c = CalendarRect();
        Rect size = SpriteRect(0x97, 0, 0);
        bool human = res.Human();
        return { c.x + (human ? 0 : 0x30), c.y + (human ? 0x1F : 0x16), size.w, size.h };
    }

    void WarMap::DrawCalendar(Canvas& canvas) const
    {
        Rect c = CalendarRect();
        canvas.DrawSprite(0x93, 0, c.x, c.y);
        int hour = game.Hour(), minute = game.Minute();
        if (res.Human())
        {
            // the clock face (144 frames: twelve an hour), month (5 frames each), am / pm, the day and year in
            // digits (5 frames each; the first is hu_num03, 0-3), the pendulum, the twirl to the left
            canvas.DrawSprite(0x96, (hour % 12) * 12 + minute / 5, c.x + 0x52, c.y + 4);
            canvas.DrawSprite(0x95, (game.Month() - 1) * 5, c.x + 0x15, c.y + 0xC);
            canvas.DrawSprite(0x94, hour >= 12 ? 5 : 0, c.x + 0x5A, c.y + 0x2B);
            int day = game.Day(), year = game.Year();
            int digits[6] = { day / 10 % 10, day % 10, year / 1000 % 10, year / 100 % 10, year / 10 % 10, year % 10 };
            static constexpr int X[6] = { 7, 14, 48, 55, 62, 69 }; // 0x4BA688
            for (int i = 0; i < 6; i++) canvas.DrawSprite(i == 0 ? 0x99 : 0x98, digits[i] * 5, c.x + X[i], c.y + 0xC);
            canvas.DrawSprite(0x9B, (animation / 2) % 41, c.x + 0x54, c.y + 0x41);
            Rect twirl = SpriteRect(0x9A, 0, 0);
            canvas.DrawSprite(0x9A, 0, c.x - twirl.w, c.y + 6);
        }
        else
        {
            // the globe, the days gone by in six digits (the game time over 24000, 2400 ... 0.24 hours: days to
            // two decimal places, 0x427500 with the table at 0x4BA910), and the goo
            canvas.DrawSprite(0x96, (animation / 4) % 24, c.x + 0xB, c.y + 8);
            static constexpr float Powers[6] = { 24000, 2400, 240, 24, 2.4f, 0.24f };
            static constexpr int X[6] = { 32, 41, 50, 59, 74, 83 }; // 0x4BA670
            for (int i = 0; i < 6; i++)
                canvas.DrawSprite(0x98, (int(game.ticks / Powers[i]) % 10) * 5, c.x + X[i], c.y + 0x35);
            canvas.DrawSprite(0x9B, animation % 100, c.x + 0x31, c.y + 7);
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

    void WarMap::DrawRouteArrow(Canvas& canvas, const Counter& k) const
    {
        // RArrow (0x4244E0) makes an RTypeArrow (0x49CAC0, drawn by 0x49CDB0) through the route's points: a curve
        // with 2^5 steps a stretch (made by 0x424A40), a textured strip along it and a head at its end. The style is
        // OBJ.ojd record 14's: 0 / 1 for the Humans, 2 / 3 for the Martians (one more where the route's +0x5E is
        // clear; the engine always uses the first). Its texture (GType 0x19 harrow2.raw / marrow2.raw, or 0x18
        // ...arrow1.raw) is 256 x 64: the body strip in its first 64 columns, the head's triangle in the next 128.
        // The body's half width runs from the style's first number to its second, and stops a third of a stretch
        // short of the end (the style's step count / 3), where the head starts, its half width the third number.
        // Provisional: 0x424A40 bends the curve by headings (0x43FDA0, 0x43FF30, 0x43FE90) where the engine uses a
        // Catmull-Rom spline; the head is one triangle (the game draws three, 0x410190) its fourth number long; the
        // body's texture runs along the curve a pixel a row; the Martian styles' -0.4 isn't used.
        bool human = res.Human();
        auto style = arrowTypes.find(human ? 0 : 2);
        if (style == arrowTypes.end()) return;
        const ObjOjd::ArrowType& t = style->second;
        const std::vector<uint8_t>* texture = res.GetRaw(t.gtype);
        const ShadeTable* shade = res.ShadeFor(t.gtype);
        constexpr int TexW = 256, TexH = 64, Steps = 32; // 2^5 (0x4244E0's pow(2.0, 5))
        if (!texture || texture->size() < size_t(TexW * TexH) || !shade) return;
        const uint32_t* colours = shade->Level(0);

        std::vector<std::pair<float, float>> points{ { float(k.x + 16 - cameraX), float(k.y + 16 - cameraY) } };
        // one point a sector, as 0x4244E0 takes them (the waypoints, 0x437380), and the cell it ends in
        for (size_t i = 0; i < k.route.size(); i++)
            if (k.routeWaypoint[i]) points.push_back({ float(k.route[i].first + 16 - cameraX), float(k.route[i].second + 16 - cameraY) });
        std::vector<std::pair<float, float>> curve;
        for (size_t i = 0; i + 1 < points.size(); i++)
        {
            auto p0 = points[i > 0 ? i - 1 : i], p1 = points[i], p2 = points[i + 1], p3 = points[i + 2 < points.size() ? i + 2 : i + 1];
            for (int s = 0; s < Steps; s++)
            {
                float u = float(s) / Steps, u2 = u * u, u3 = u2 * u;
                auto at = [&](float a, float b, float c, float d) {
                    return 0.5f * (2 * b + (c - a) * u + (2 * a - 5 * b + 4 * c - d) * u2 + (3 * b - a - 3 * c + d) * u3);
                };
                curve.push_back({ at(p0.first, p1.first, p2.first, p3.first), at(p0.second, p1.second, p2.second, p3.second) });
            }
        }
        curve.push_back(points.back());
        int n = int(curve.size());
        int headAt = std::max(0, n - 1 - Steps / 3);
        auto direction = [&](int i) {
            int a = std::max(0, i - 1), b = std::min(n - 1, i + 1);
            float dx = curve[size_t(b)].first - curve[size_t(a)].first, dy = curve[size_t(b)].second - curve[size_t(a)].second;
            float len = std::sqrt(dx * dx + dy * dy);
            return len > 0 ? std::make_pair(dx / len, dy / len) : std::make_pair(1.0f, 0.0f);
        };
        // the body
        float along = 0;
        auto edge = [&](int i, float& lx, float& ly, float& rx, float& ry) {
            auto [dx, dy] = direction(i);
            float w = float(t.a) + float(t.b - t.a) * float(i) / float(std::max(n - 1, 1));
            lx = curve[size_t(i)].first - dy * w; ly = curve[size_t(i)].second + dx * w;
            rx = curve[size_t(i)].first + dy * w; ry = curve[size_t(i)].second - dx * w;
        };
        float lx0, ly0, rx0, ry0;
        edge(0, lx0, ly0, rx0, ry0);
        for (int i = 1; i <= headAt; i++)
        {
            float lx1, ly1, rx1, ry1;
            edge(i, lx1, ly1, rx1, ry1);
            float step = std::hypot(curve[size_t(i)].first - curve[size_t(i - 1)].first, curve[size_t(i)].second - curve[size_t(i - 1)].second);
            float v0 = along, v1 = along + step;
            along = v1;
            Canvas::TexVertex l0{ lx0, ly0, 0, v0 }, r0{ rx0, ry0, 63, v0 }, l1{ lx1, ly1, 0, v1 }, r1{ rx1, ry1, 63, v1 };
            canvas.DrawTexturedTriangle(texture->data(), TexW, TexH, colours, l0, r0, r1);
            canvas.DrawTexturedTriangle(texture->data(), TexW, TexH, colours, l0, r1, l1);
            lx0 = lx1; ly0 = ly1; rx0 = rx1; ry0 = ry1;
        }
        // the head: its base across the curve where the body stops, its point the style's length on
        auto [hx, hy] = curve[size_t(headAt)];
        auto [dx, dy] = direction(headAt);
        float c = float(t.c), d = float(t.d);
        Canvas::TexVertex left{ hx - dy * c, hy + dx * c, 64, 63 }, right{ hx + dy * c, hy - dx * c, 191, 63 },
            tip{ hx + dx * d, hy + dy * d, 128, 0 };
        canvas.DrawTexturedTriangle(texture->data(), TexW, TexH, colours, left, right, tip);
    }

    bool WarMap::HasCommandPost(int sector) const
    {
        // 0x43C990: the sector's owner has its command building there, finished. A Human sector needs a Command
        // Post (type 314; in London, sector 30, the war room, 278), a Martian one a Communications Centre (120;
        // in sector 3, where the Martians land, 830). "No further advance or construction can be co-ordinated in
        // the vicinity until a command post has been built." A sector no one holds never has one.
        if (sector < 1 || sector > warmap::SectorCount) return false;
        int who = owner[size_t(sector)];
        if (who == 2) return false;
        bool human = (who == 0) == res.Human();
        int type = human ? (sector == 30 ? 278 : 314) : (sector == 3 ? 830 : 120);
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
        k.fx = float(k.x);
        k.fy = float(k.y);
        message = "On the way to " + res.String(warmap::NameText(sector));
    }
}
