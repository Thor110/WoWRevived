// The battle map (EnterBattleMap 0x439770, WLandscapeView 0x49F240): so far its ground. It builds the sector's
// landscape (WLandscape 0x47AF10: the map OBJ.ojd record 8 names, lit for the hour of the game's clock) and draws it
// with the terrain view, scrolled with the arrow keys or the mouse at the screen's edges. See docs/BATTLEMAP.md.
#pragma once
#include "core/Landscape.h"
#include "core/ObjOjd.h"
#include "core/TerrainView.h"
#include "frontend/Screen.h"
#include <memory>
#include <string>

namespace wow
{
    class BattleMap : public Screen
    {
    public:
        static constexpr int Id = 0x71;
        // The command that ends the battle map (0x97, the battle's end, 0x41A0DC)
        static constexpr int EndCommand = 0x97;

        struct Setup
        {
            int sector = 0;
            std::string mapName;        // OBJ.ojd record 8, "land03.cls"
            float hours = 0;            // the game clock (calendar +0x28)
            int shadeClass = 0;         // Landscape::ShadeClass
            const ObjOjd::BattleTables::DayTime* dayTime = nullptr;
            bool human = true;
            int scrollStep = 16;        // *provisional*: pixels a tick
        };

        // Throws when the map's files can't be read.
        BattleMap(Resources& res, const Setup& setup);

        void Update() override;
        void Draw(Canvas& canvas) const override;
        std::optional<Command> OnKey(int sdlKey) override;
        void OnKeyUp(int sdlKey) override;
        void OnMouseMove(int x, int y) override { mouseX = x; mouseY = y; }
        std::optional<Command> OnMouseButton(int, int, bool) override { return std::nullopt; }
        int CursorAt(int, int) const override { return 0; }

        int ScrollX() const { return scrollX; }
        int ScrollY() const { return scrollY; }
        void ScrollTo(int x, int y);

    private:
        Setup setup;
        mutable Landscape land;     // the view works out each vertex's shown level as it draws (0x49FDA0)
        TerrainTiles tiles;
        ShadeTable table;
        std::vector<uint8_t> levels;
        DitherTables dither;
        int scrollX = 0, scrollY = 0;
        int mouseX = -1, mouseY = -1;
        int keysX = 0, keysY = 0;
    };
}
