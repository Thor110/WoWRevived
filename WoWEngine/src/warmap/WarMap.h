// The war map screen (WWarMapWorld 0x417CD0, its view set up at 0x418980): the map of Britain filling the screen,
// lit sector by sector through the sector map, scrolled about, with the mini-map in the bottom right corner.
//
// Worked out so far (see docs/WARMAP.md): the map and sector map sprites, palette group hw, the sector table and
// borders, the starting view (the WMAP block of the save: view centre and sector), and that the map is drawn
// with a shade level for every sector map value. Not yet: the exact scroll rules, the HUD's own controls, the
// units, the clock running, and everything the war map does.
#pragma once
#include "core/SaveGame.h"
#include "core/ObjOjd.h"
#include "frontend/Menu.h"
#include "warmap/WarMapData.h"

#include <array>
#include <string>
#include <utility>
#include <map>
#include <vector>

namespace wow
{
    class WarMap : public Screen
    {
    public:
        static constexpr int Id = 0x8A; // the command that starts it

        // start: the save it begins from (its WMAP block gives the view and sector)
        // units: each unit type's counter frame (OBJ.ojd record 23) and war map speed (records 37/38)
        WarMap(Resources& res, MenuColours colours, const SaveInfo& start, std::vector<SaveInfo::WarMapObject> objects,
               std::map<int, ObjOjd::UnitData> units, std::map<int, int> objectClasses, std::vector<int> owners, int scrollSpeed, float cluntingSpeed = 7.0f);

        void Update() override;
        void Draw(Canvas& canvas) const override;
        std::optional<Command> OnKey(int sdlKey) override;
        std::optional<Command> OnMouseButton(int x, int y, bool down) override;
        void OnMouseMove(int x, int y) override;
        void OnKeyUp(int sdlKey) override;

        int SectorAt(int screenX, int screenY) const; // 0 off the land
        int HoveredSector() const { return hovered; }
        const SaveInfo& Game() const { return game; }
        void SetSectorLimits(int units, int boats) { maxUnits = units; maxBoats = boats; }
        void SetArrowTypes(std::map<int, ObjOjd::ArrowType> types) { arrowTypes = std::move(types); }

    private:
        void CentreOn(int mapX, int mapY);
        void Clamp();
        Rect MiniMapRect() const;
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
        // who holds each sector: 0 the side being played, 1 the other side, 2 no one
        std::array<int, warmap::SectorCount + 1> owner{};
        int viewMode = 1;                // 1 units, 2 constructions, 3 resources (0x419810, the WWarMapWorld's +0x70)
        int hotButton = -1;              // the panel button under the mouse

        struct PanelLayout { Rect frame, mini, buttons[3]; };
        PanelLayout Panel() const;

        int chosen = -1;                 // the counter picked to move (drawn with cselecth.spr, GType 0x54)
        float cluntingSpeed = 7.0f;      // [Tweak] "Clunting Speed" (0x428B70, default 7): only for units with no speed
        std::string message;             // the latest war map message
        int animation = 0;               // ticks, for the calendar's pendulum / goo and spinning digits
        Rect CalendarRect() const;
        Rect SpeedRect() const;
        void DrawCalendar(Canvas& canvas) const;
        int CounterAt(int screenX, int screenY) const;
        bool HasCommandPost(int sector) const;
        // [Tweak] "Max units in sector" (0x42AAF0, default 15) and "Max boats in sector" (0x42AB30, default 5)
        int maxUnits = 15, maxBoats = 5;
        std::map<int, ObjOjd::ArrowType> arrowTypes; // OBJ.ojd record 14
        // the types of the finished constructions in each sector (both sides')
        std::array<std::vector<int>, warmap::SectorCount + 1> finished;
        std::pair<int, int> FreeCell(int sector, int kind, int except) const;
        std::vector<std::pair<int, int>> SectorCells(int sector) const;
        void SendTo(int counter, int sector);

        struct Counter
        {
            int x = 0, y = 0, frame = 0, count = 0;
            bool players = false;
            int kind = 0;                // 0 units, 1 constructions, 2 resources
            int sector = 0;
            int speed = 0;               // the unit type's war map speed (unit +0x54), 0 for constructions and resources
            int moveClass = 0;           // the unit type's +0x4E: 0-2 land, 3 boats, 4 fliers
            // travelling: where it's heading, through these points (sector by sector)
            std::vector<std::pair<int, int>> route;
            std::vector<int> routeSectors;
            std::vector<bool> routeWaypoint; // the points the arrow goes through: a waypoint or the last cell
            float fx = 0, fy = 0;        // exact position while travelling
        };
        float PixelsPerHour(const Counter& k) const; // how far a counter goes in a game hour
        std::vector<int> FindRoute(int from, int to, const Counter& k, std::string* why = nullptr) const;
        void DrawRouteArrow(Canvas& canvas, const Counter& k) const;
        std::vector<Counter> counters;   // what's drawn: one per kind of object, with how many there are
    };
}
