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
        static constexpr int ResearchCommand = 0x7D; // the research screen (0x41A450 'R', the Research button)

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
        int Speed() const { return speed; }  // the clock's speed, 0-10
        void SetSectorLimits(int units, int boats) { maxUnits = units; maxBoats = boats; }
        // sounds (SFX.ojd ids) the war map wants played, taken by the front end every update
        std::vector<int> TakeSounds() { return std::exchange(sounds, {}); }
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
        bool dragging = false;           // the mini-map
        bool draggingSpeed = false;      // the speed slider, held
        void SetSpeedFromX(int x);
        int speed = 1;                   // index into warmap::Speeds (the calendar's +0x24)
        int forcedHours = 0;             // the calendar's +0x30: hours to go on by, one a tick
        float ClockTick();
        void MoveCounters(float hours);
        // who holds each sector: 0 the side being played, 1 the other side, 2 no one
        std::array<int, warmap::SectorCount + 1> owner{};
        int viewMode = 1;                // 1 units, 2 constructions, 3 resources (0x419810, the WWarMapWorld's +0x70)
        int hotButton = -1;              // the panel button under the mouse (3: the Research button)
        bool researchHeld = false;       // the Research button, pressed
        int resourceKind = 0;            // the Resource view's kind, 0-2 (dword_4E9F40; F3-F5 or Space)
        bool panelShown = true;          // the mini-map and its frame (Tab, 0x41A450)

        struct PanelLayout { Rect frame, mini, buttons[3], research; };
        PanelLayout Panel() const;

        int chosen = -1;                 // the counter picked to move (drawn with cselecth.spr, GType 0x54)
        float cluntingSpeed = 7.0f;      // [Tweak] "Clunting Speed" (0x428B70, default 7): only for units with no speed
        std::string message;             // the engine's own note (a refused route), shown in the status strip
        // The message list (PopupMessageController 0x420A30 at 0x28, 4; lines added by 0x4191A0 -> 0x420DA0):
        // at most 10 (0x4BA4FC, the oldest goes), each a "Message Item" (0x428810, font 0x72) shown for 5 x 30
        // updates (0x4BA4F8 x "Game Frequency"), drawn top down 16 pixels apart (0x4BA500, 0x420C10).
        struct LogLine { std::string text; int life = 0; };
        std::vector<LogLine> log;
        void Log(std::string text);
        std::vector<int> sounds;
        int animation = 0;               // ticks
        // The calendar's moving parts (Calendar View 0x426C10): each wheel's frame and the frame it's heading for
        // (0x427500 sets the targets, 0x427710 turns the wheels, both every update)
        struct CalendarWheels
        {
            int digit[6] = {}, digitTarget[6] = {};  // +0xD4 / +0xEC: frames, 5 a digit
            int ampm = 0, ampmTarget = 0;             // +0x9C / +0xA0 (Human)
            int month = 0, monthTarget = 0;           // +0xA8 / +0xAC (Human)
            int face = 0, faceTarget = 0;             // +0x90 / +0x94 (Human: 8 frames an hour over 12 hours)
            int globe = 0;                            // the Martian globe: the hour of the day
            int cycle = 0;                            // +0xB8: the pendulum (Human, 0-29) or the goo (Martian, 0-99)
            bool started = false;
        } wheels;
        void TurnCalendar();
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
            std::vector<bool> routeWaypoint; // a sector's waypoint (or the last cell) rather than a border point
            // the route's arrow (0x4244E0): its curve points (x, y and heading in 4096ths of a turn), and whether it's
            // an attack (into a sector the other side holds: the big arrow and the arrival label)
            struct ArrowPoint { float x, y; int heading; };
            std::vector<ArrowPoint> arrow;
            bool attack = false;
            float fx = 0, fy = 0;        // exact position while travelling
        };
        float PixelsPerHour(const Counter& k) const; // how far a counter goes in a game hour
        std::vector<int> FindRoute(int from, int to, const Counter& k, std::string* why = nullptr) const;
        void DrawRouteArrow(Canvas& canvas, const Counter& k) const;
        static std::vector<Counter::ArrowPoint> ArrowCurve(const std::vector<std::pair<float, float>>& control);
        std::vector<Counter> counters;   // what's drawn: one per kind of object, with how many there are
    };
}
