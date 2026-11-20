// The war map screen (WWarMapWorld 0x417CD0, its view set up at 0x418980): the map of Britain filling the screen,
// lit sector by sector through the sector map, scrolled about, with the mini-map in the bottom right corner.
//
// Worked out so far (see docs/WARMAP.md): the map and sector map sprites, palette group hw, the sector table and
// borders, the starting view (the WMAP block of the save: view centre and sector), and that the map is drawn
// with a shade level for every sector map value. Not yet: the exact scroll rules, the HUD's own controls, the
// units, the clock running, and everything the war map does.
#pragma once
#include "core/SaveGame.h"
#include "frontend/Menu.h"
#include "warmap/WarMapData.h"

#include <array>
#include <map>
#include <vector>

namespace wow
{
    class WarMap : public Screen
    {
    public:
        static constexpr int Id = 0x8A; // the command that starts it

        // start: the save it begins from (its WMAP block gives the view and sector)
        // counters: each unit type's frame in the counter sprites (OBJ.ojd record 23)
        WarMap(Resources& res, MenuColours colours, const SaveInfo& start, std::vector<SaveInfo::WarMapObject> objects,
               std::map<int, int> counters, int scrollSpeed);

        void Update() override;
        void Draw(Canvas& canvas) const override;
        std::optional<Command> OnKey(int sdlKey) override;
        std::optional<Command> OnMouseButton(int x, int y, bool down) override;
        void OnMouseMove(int x, int y) override;
        void OnKeyUp(int sdlKey) override;

        int SectorAt(int screenX, int screenY) const; // 0 off the land
        int HoveredSector() const { return hovered; }
        const SaveInfo& Game() const { return game; }

    private:
        void CentreOn(int mapX, int mapY);
        void Clamp();
        Rect MiniMapRect(const Canvas* canvas = nullptr) const;
        int ViewW() const;
        int ViewH() const;

        MenuColours colours;
        SaveInfo game;
        const IndexedImage* map = nullptr;
        const IndexedImage* sectorMap = nullptr;
        std::array<int, 256> levels{};   // shade level for each sector map value (0x49BE70: 11 for all)
        int cameraX = 0, cameraY = 0;    // the view's top-left on the map
        int mouseX = -1, mouseY = -1;
        int hovered = 0, selected = 0;
        int scrollStep = 8;
        bool keys[4] = {};               // left, right, up, down held
        bool dragging = false;           // the mini-map
        int speed = 1;                   // index into warmap::Speeds (the calendar's +0x24)

        struct Counter { int x, y, frame, count; bool players; };
        std::vector<Counter> counters;   // what's drawn: one per kind of object, with how many there are
    };
}
