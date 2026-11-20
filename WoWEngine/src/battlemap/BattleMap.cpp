#include "battlemap/BattleMap.h"
#include "frontend/Canvas.h"
#include "frontend/Resources.h"
#include "core/Bytes.h"
#include "core/Random.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <stdexcept>

namespace wow
{
    namespace
    {
        std::vector<uint8_t> Need(Resources& res, const std::string& path)
        {
            auto data = res.LoadFile(path);
            if (!data) throw std::runtime_error("battle map: can't load " + path);
            return std::move(*data);
        }
    }

    BattleMap::BattleMap(Resources& res, const Setup& s) : Screen(Id, res, 0, 0x2F0), setup(s)
    {
        // WLandscape::CreateTerrain (0x47B900): maps\<name>, its extension swapped for .atm
        std::string base = s.mapName;
        if (auto dot = base.rfind('.'); dot != std::string::npos) base.resize(dot);
        Terrain t = Terrain::Load(Need(res, "maps\\" + base + ".cls"), Need(res, "maps\\" + base + ".atm"));
        land = Landscape::Create(t);
        tiles = TerrainTiles::Load(t);

        // The hour's light (0x47AF10 -> 0x40B030) and the shade tables (0x40B570, 0x40B6C0): the first set's letter
        // names them, L and N becoming X and Y in a Martian game
        static const float noLight[4] = { 0.9f, 0.1f, 0.75f, 0.0f };
        int angle = 90;
        const float* set = noLight;
        char letter = 'M';
        if (s.dayTime)
        {
            angle = s.dayTime->angle;
            set = s.dayTime->sets[0].f;
            if (!s.dayTime->sets[0].name.empty()) letter = s.dayTime->sets[0].name[0];
        }
        if (!s.human && letter == 'L') letter = 'X';
        if (!s.human && letter == 'N') letter = 'Y';
        std::string tail = std::to_string(s.shadeClass) + letter;
        levels = Need(res, "dat\\LANDR" + tail + ".int");
        table = ShadeTable::LoadShh(Need(res, "dat\\LANDR" + tail + ".SHH"));
        Landscape::Sun sun = Landscape::SunFor(angle, set);
        sun.shade = Landscape::ShadeLevel(levels, set[2], 0.0f);
        land.LightTheMap(sun);
        dither = DitherTables::Build(Need(res, "dat\\dith.dat"));

        // *Provisional*: the view starts on the map's centre (the game centres it on the player's HQ when there is
        // one, 0x439770)
        int cx = land.WorldWidth() / 2, cy = (land.Height() * (land.Cell() >> 16)) / 2;
        ScrollTo(cx - 320, land.ProjectY(cy, land.HeightAt(cx, cy)) - 240);
    }

    void BattleMap::ScrollTo(int x, int y)
    {
        scrollX = std::clamp(x, 0, std::max(0, land.WorldWidth() - 640));
        scrollY = std::clamp(y, 0, std::max(0, land.WorldHeight() - 480));
    }

    std::optional<Command> BattleMap::OnKey(int key)
    {
        switch (key)
        {
        case SDLK_LEFT: keysX = -1; break;
        case SDLK_RIGHT: keysX = 1; break;
        case SDLK_UP: keysY = -1; break;
        case SDLK_DOWN: keysY = 1; break;
        case SDLK_ESCAPE:
            // *Provisional*: the game's Escape opens the battle map menu (0x80B8); until it's in, Escape ends the
            // battle map
            return Command{ EndCommand, setup.sector };
        default: break;
        }
        return std::nullopt;
    }

    void BattleMap::OnKeyUp(int key)
    {
        if ((key == SDLK_LEFT && keysX < 0) || (key == SDLK_RIGHT && keysX > 0)) keysX = 0;
        if ((key == SDLK_UP && keysY < 0) || (key == SDLK_DOWN && keysY > 0)) keysY = 0;
    }

    void BattleMap::Update()
    {
        int dx = keysX, dy = keysY;
        if (mouseX >= 0)
        {
            if (mouseX < 4) dx = -1;
            else if (mouseX >= 636) dx = 1;
            if (mouseY < 4) dy = -1;
            else if (mouseY >= 476) dy = 1;
        }
        if (dx || dy) ScrollTo(scrollX + dx * setup.scrollStep, scrollY + dy * setup.scrollStep);
    }

    void BattleMap::Draw(Canvas& canvas) const
    {
        canvas.Clear(0);
        TerrainView::Target t{ canvas.Pixels(), canvas.Width(), 0, 0, canvas.Width(), canvas.Height() };
        TerrainView::Render(land, tiles, table, levels, dither, t, -scrollX, -scrollY);
    }
}
