#include "warmap/WarMap.h"

#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <cmath>
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
        // position (all resources) go in the sector's first free cell: 0x428110 lays a 36 pixel grid over the
        // sector's bounds and counts a cell as the sector's when CHI.SPR (GType 0x84, the sectors drawn a little
        // smaller, with the sea areas numbered from 192) has the sector's number at its corner. The order the
        // game fills the cells in (0x4282C0) isn't traced yet; row by row is a stand-in.
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
        const IndexedImage* cells = res.GetFrame(0x84);
        constexpr int Cell = 36;
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
            k.count = g.count;
            k.players = g.list % 2 == 0;
            k.kind = g.list / 2;
            k.sector = g.sector;
            if ((!k.x && !k.y) || taken(k.x, k.y, k.kind))
            {
                const auto& b = warmap::Sectors[g.sector];
                bool placed = false;
                for (int y = b.top / Cell * Cell; y < b.bottom && !placed; y += Cell)
                    for (int x = b.left / Cell * Cell; x < b.right && !placed; x += Cell)
                    {
                        bool inside = cells && x >= 0 && y >= 0 && x < cells->width && y < cells->height &&
                                      cells->pixels[size_t(y) * size_t(cells->width) + size_t(x)] == g.sector;
                        if (inside && !taken(x, y, k.kind)) { k.x = x; k.y = y; placed = true; }
                    }
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
                if (next != k.sector && owner[size_t(next)] == 1)
                {
                    message = "Battle in " + res.String(warmap::NameText(next)) + ": the battle map isn't rebuilt yet";
                    k.route.clear(); k.routeSectors.clear();
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

        // Routes: a line from each travelling counter through the rest of its points. The game draws arrows
        // (RArrow 0x4244E0: a curve through the points, 2^5 steps a stretch, drawn by RTypeArrow), which aren't
        // rebuilt yet; the line is a stand-in.
        {
            uint32_t* out = canvas.Pixels();
            const uint32_t colour = 0xFFFFFF;
            auto plot = [&](int x, int y) {
                if (x >= 0 && y >= 0 && x < canvas.Width() && y < canvas.Height()) out[size_t(y) * size_t(canvas.Width()) + size_t(x)] = colour;
            };
            for (const Counter& k : counters)
            {
                if (k.route.empty() || k.kind != viewMode - 1) continue;
                int px = k.x + 16 - cameraX, py = k.y + 16 - cameraY;
                for (const auto& p : k.route)
                {
                    int qx = p.first + 16 - cameraX, qy = p.second + 16 - cameraY;
                    int steps = std::max(std::abs(qx - px), std::abs(qy - py));
                    for (int t = 0; t <= steps; t += 2) // dotted
                        plot(px + (qx - px) * t / std::max(steps, 1), py + (qy - py) * t / std::max(steps, 1));
                    px = qx; py = qy;
                }
            }
        }

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
    // positions, then the same lit), at 0, 0x1F in the Human calendar and 0x30, 0x16 in the Martian one. Which
    // of its positions gives which game speed isn't traced; the engine uses position = speed (0-7).
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

    std::vector<int> WarMap::FindRoute(int from, int to) const
    {
        // The shortest way over land, through the sector links (0x4BCEB0). The game's own route finder (FindRoute
        // 0x44EFA0, WRouteFinder) isn't traced; this is Dijkstra over the links' lengths.
        std::array<double, warmap::SectorCount + 1> best;
        std::array<int, warmap::SectorCount + 1> previous;
        std::array<bool, warmap::SectorCount + 1> done{};
        best.fill(1e18);
        previous.fill(-1);
        best[size_t(from)] = 0;
        for (;;)
        {
            int s = -1;
            for (int n = 1; n <= warmap::SectorCount; n++)
                if (!done[size_t(n)] && best[size_t(n)] < 1e17 && (s < 0 || best[size_t(n)] < best[size_t(s)])) s = n;
            if (s < 0 || s == to) break;
            done[size_t(s)] = true;
            auto a = warmap::Waypoint(s);
            for (int n = 1; n <= warmap::SectorCount; n++)
            {
                const warmap::SectorLink* link = done[size_t(n)] ? nullptr : warmap::LandLink(s, n);
                if (!link) continue;
                auto b = warmap::Waypoint(n);
                // waypoint, out across the border at the link's two points, on to the next waypoint
                double d = best[size_t(s)] + std::hypot(double(link->land[0] - a.x), double(link->land[1] - a.y)) +
                           std::hypot(double(link->land[2] - link->land[0]), double(link->land[3] - link->land[1])) +
                           std::hypot(double(b.x - link->land[2]), double(b.y - link->land[3]));
                if (d < best[size_t(n)]) { best[size_t(n)] = d; previous[size_t(n)] = s; }
            }
        }
        if (to != from && previous[size_t(to)] < 0) return {};
        std::vector<int> path;
        for (int s = to; s != from; s = previous[size_t(s)]) path.insert(path.begin(), s);
        return path;
    }

    std::pair<int, int> WarMap::FreeCell(int sector, int kind, int except) const
    {
        const IndexedImage* cells = res.GetFrame(0x84);
        const auto& b = warmap::Sectors[sector];
        constexpr int Cell = 36;
        for (int y = b.top / Cell * Cell; y < b.bottom; y += Cell)
            for (int x = b.left / Cell * Cell; x < b.right; x += Cell)
            {
                if (!cells || x < 0 || y < 0 || x >= cells->width || y >= cells->height ||
                    cells->pixels[size_t(y) * size_t(cells->width) + size_t(x)] != sector) continue;
                bool taken = false;
                for (size_t i = 0; i < counters.size(); i++)
                    if (int(i) != except && counters[i].kind == kind && counters[i].x == x && counters[i].y == y) taken = true;
                if (!taken) return { x, y };
            }
        return { (b.left + b.right) / 2, (b.top + b.bottom) / 2 };
    }

    void WarMap::SendTo(int index, int sector)
    {
        Counter& k = counters[size_t(index)];
        if (sector == k.sector) return;
        std::vector<int> path = FindRoute(k.sector, sector);
        if (path.empty()) { message = "No way there over land"; return; }
        k.route.clear();
        k.routeSectors.clear();
        // out of each sector across the link's two border points, through the next sector's waypoint (0x437380),
        // ending in a free cell of the last; the counter's corner follows the points less half its size
        int at = k.sector;
        for (size_t i = 0; i < path.size(); i++)
        {
            const warmap::SectorLink* link = warmap::LandLink(at, path[i]);
            if (link)
            {
                k.route.push_back({ link->land[0] - 16, link->land[1] - 16 });
                k.routeSectors.push_back(at);
                k.route.push_back({ link->land[2] - 16, link->land[3] - 16 });
                k.routeSectors.push_back(path[i]);
            }
            auto w = warmap::Waypoint(path[i]);
            if (i + 1 < path.size()) k.route.push_back({ w.x - 16, w.y - 16 });
            else k.route.push_back(FreeCell(path[i], k.kind, index));
            k.routeSectors.push_back(path[i]);
            at = path[i];
        }
        k.fx = float(k.x);
        k.fy = float(k.y);
        message = "On the way to " + res.String(warmap::NameText(sector));
    }
}
