#include "warmap/WarMap.h"

#include <SDL3/SDL_keycode.h>

#include <algorithm>
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
                   std::map<int, int> frames, int scrollSpeed)
        : Screen(Id, r, 0, warmap::PaletteGroup), colours(c), game(start)
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
            auto frame = frames.find(g.type);
            Counter k{ g.x, g.y, frame == frames.end() ? 0 : frame->second, g.count, g.list % 2 == 0, g.list / 2 };
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

    void WarMap::Update()
    {
        if (!map) map = res.GetFrame(warmap::MapGType);
        if (!sectorMap) sectorMap = res.GetFrame(warmap::SectorMapGType);

        // the clock
        {
            float before = game.ticks;
            game.ticks += warmap::Speeds[speed] / 30.0f;
            int hours = int(game.ticks) - int(before);
            if (hours > 0) warmap::AdvanceDate(game.dateTime, hours);
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

        // the counters (GType 0x77, 32 pixels) of the kind the view shows, with how many when there's more than one
        for (const Counter& k : counters)
        {
            if (k.kind != viewMode - 1) continue; // each view shows its own kind
            int x = k.x - cameraX, y = k.y - cameraY;
            if (x < -40 || y < -40 || x > canvas.Width() || y > canvas.Height()) continue;
            canvas.DrawSprite(0x77, k.frame, x, y);
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
        if (hotButton >= 0) status = res.String(ButtonText[hotButton]);
        else if (hovered) status = res.String(warmap::NameText(hovered));
        char date[64];
        std::snprintf(date, sizeof(date), "%d/%d/%d  %02d:%02d  x%g", game.Day(), game.Month(), game.Year(), game.Hour(), game.Minute(),
            double(warmap::Speeds[speed]));
        {
            // a darkened strip behind the text so it reads on either map
            uint32_t* out = canvas.Pixels();
            int rows = status.empty() ? 16 : 30;
            for (int y = 2; y < 2 + rows; y++)
                for (int x = 4; x < 240; x++)
                {
                    uint32_t& p = out[size_t(y) * size_t(canvas.Width()) + size_t(x)];
                    p = (p >> 2) & 0x3F3F3F;
                }
        }
        canvas.DrawString(0x72, date, { 8, 4, 300, 14 }, 0, colours.items);
        if (!status.empty()) canvas.DrawString(0x72, status, { 8, 18, 300, 14 }, 0, colours.items);
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
        if (down && mini.Contains(x, y))
        {
            dragging = true;
            CentreOn((x - mini.x) * warmap::MapWidth / MiniW, (y - mini.y) * warmap::MapHeight / MiniH);
            return std::nullopt;
        }
        if (!down) { dragging = false; return std::nullopt; }
        if (int sector = SectorAt(x, y)) selected = sector;
        return std::nullopt;
    }
}
