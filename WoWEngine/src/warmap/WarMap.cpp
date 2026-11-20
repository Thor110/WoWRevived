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
        // 59-66 (red, bright green, cream, orange, dark red, green, tan, white), which the game gives border
        // values to colour sector borders by who holds them. Which the game uses for the selected and hovered
        // sector isn't traced yet; these are stand-ins.
        constexpr int SelectedBorderLevel = 66; // white
        constexpr int HoverBorderLevel = 61;    // cream
        constexpr int MiniW = 100, MiniH = 170;
    }

    WarMap::WarMap(Resources& r, MenuColours c, const SaveInfo& start, std::vector<SaveInfo::WarMapObject> objects,
                   std::map<int, int> frames, int scrollSpeed)
        : Screen(Id, r, 0, warmap::PaletteGroup), colours(c), game(start)
    {
        // One counter for each kind of unit or building in a list: the first of a kind carries the position,
        // the ones after it at 0, 0 add to its count. Where two kinds share a position (a sector's first unit and
        // first building do), the later one moves along a counter's width. That layout is a stand-in until the
        // game's own counter layout is traced.
        for (size_t i = 0; i < objects.size(); i++)
        {
            const auto& o = objects[i];
            if (o.list >= 4) continue; // resources
            auto frame = frames.find(o.unitType);
            if (o.x == 0 && o.y == 0)
            {
                if (!counters.empty()) counters.back().count++;
                continue;
            }
            Counter k{ o.x, o.y, frame == frames.end() ? 0 : frame->second, 1, o.PlayersSide() };
            for (bool moved = true; moved;)
            {
                moved = false;
                for (const Counter& other : counters)
                    if (other.x == k.x && other.y == k.y) { k.x += 36; moved = true; }
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

    Rect WarMap::MiniMapRect(const Canvas*) const { return { ScreenW - MiniW - 8, ScreenH - MiniH - 8, MiniW, MiniH }; }

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
        hovered = mouseX >= 0 && !MiniMapRect().Contains(mouseX, mouseY) ? SectorAt(mouseX, mouseY) : 0;
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
            auto outline = [&](int sector, int level) {
                for (int border : warmap::SectorBorders[sector])
                    if (border > 0 && border + 31 < 256) lv[size_t(border + 31)] = level;
            };
            if (selected) outline(selected, SelectedBorderLevel);
            if (hovered && hovered != selected) outline(hovered, HoverBorderLevel);
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

        // the counters (GType 0x77, 32 pixels), with how many when there's more than one
        for (const Counter& k : counters)
        {
            int x = k.x - cameraX, y = k.y - cameraY;
            if (x < -40 || y < -40 || x > canvas.Width() || y > canvas.Height()) continue;
            canvas.DrawSprite(0x77, k.frame, x, y);
            if (k.count > 1)
                canvas.DrawString(0x72, std::to_string(k.count), { x, y + 30, 32, 10 }, TextCentreX, colours.items);
        }

        // the mini-map: the whole map at a fifteenth, the sector under the mouse lit, and the view's outline
        Rect mini = MiniMapRect();
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
        if (hovered) status = res.String(warmap::NameText(hovered));
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
