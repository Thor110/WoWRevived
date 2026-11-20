// The battle map (EnterBattleMap 0x439770, WBattleMap 0x484A30, WLandscapeView 0x49F240): so far its ground. It
// builds the sector's landscape (WLandscape 0x47AF10: the map OBJ.ojd record 8 names, lit for the hour of the
// game's clock) and draws it with the terrain view, scrolled by the view's keys (0x485610) and its edges (0x484DF0).
// Escape opens the battle map menu (0x80B8); the right button held opens the popup with Return to Warmap (0x76).
// See docs/BATTLEMAP.md.
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
        static constexpr int MenuCommand = 0x80B8;      // the battle map menu (0x49BD50)
        static constexpr int ReturnCommand = 0x76;      // Return to Warmap: to the world, which ends the battle map

        struct Setup
        {
            int sector = 0;
            std::string mapName;        // OBJ.ojd record 8, "land03.cls"
            float hours = 0;            // the game clock (calendar +0x28)
            int shadeClass = 0;         // Landscape::ShadeClass
            const ObjOjd::BattleTables::DayTime* dayTime = nullptr;
            bool human = true;
            int quickCancelTimeout = 5; // [Tweak] "Quick Cancel Timeout": updates the right button is held for the popup
        };

        // Throws when the map's files can't be read.
        BattleMap(Resources& res, const Setup& setup);

        void Update() override;
        void Draw(Canvas& canvas) const override;
        std::optional<Command> OnKey(int sdlKey) override;
        void OnKeyUp(int sdlKey) override;
        void OnMouseMove(int x, int y) override;
        std::optional<Command> OnMouseButton(int x, int y, bool down) override;
        std::optional<Command> OnRightButton(int x, int y, bool down) override;
        int CursorAt(int, int) const override { return 0; }
        int TipOwner(int x, int y) const override;
        std::string TipText(int owner, int level) const override;

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
        int ticks = 0;
        int rightDownAt = -1;
        struct PopupItem { int command, icon, text; Rect rect; };
        std::vector<PopupItem> popup;       // "Battlemap menu" (WPopupMenu 0x432C60, 0x485DE0)
        int popupHover = -1;
        void OpenPopup(int x, int y);
        void ClosePopup(bool escape);
        void EdgeScroll();
    };
}
