// The war map screen (WWarMapWorld 0x417CD0, its view set up at 0x418980): the map of Britain filling the screen,
// lit sector by sector through the sector map, scrolled about, with the mini-map in the bottom right corner.
//
// Worked out so far (see docs/WARMAP.md): the map and sector map sprites, palette group hw, the sector table and
// borders, the starting view (the WMAP block of the save: view centre and sector), and that the map is drawn
// with a shade level for every sector map value. Not yet: the exact scroll rules, the HUD's own controls, the
// units, the clock running, and everything the war map does.
#pragma once
#include "core/SaveFile.h"
#include "core/SaveGame.h"
#include "core/ObjOjd.h"
#include "frontend/Menu.h"
#include "warmap/WarMapData.h"

#include <algorithm>
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
        std::optional<Command> OnRightButton(int x, int y, bool down) override;
        void SetQuickCancelTimeout(int ticks) { quickCancelTimeout = ticks; }
        void OnMouseMove(int x, int y) override;
        void OnKeyUp(int sdlKey) override;
        // tool tips: 0-2 the view buttons, 3 the Research button, 4 the calendar
        int TipOwner(int x, int y) const override;
        int CursorAt(int x, int y) const override;
        std::string CounterTip(int counter, int level) const;
        std::string TipText(int owner, int level) const override;

        int SectorAt(int screenX, int screenY) const; // 0 off the land
        int HoveredSector() const { return hovered; }
        const SaveInfo& Game() const { return game; }
        int Speed() const { return speed; }  // the clock's speed, 0-10
        void SetSectorLimits(int units, int boats) { maxUnits = units; maxBoats = boats; }
        void SetPodInterval(int hours) { podInterval = hours; }
        void SetClunting(int res, int adj) { clunRes = std::max(1, res); clunAdj = adj; for (auto& g : grids) g.size = 0; LayoutAll(); }
        // [Tweak] "Route clunting width" and "Route clunting spacing" (0x42AF20 / 0x42AF60, both 4)
        void SetRouteFormation(int width, int spacing) { clunWidth = std::max(1, width); clunSpacing = spacing; }
        // The war map's objects from a save read exactly (each with its ref into that save's objects, in order).
        static std::vector<SaveInfo::WarMapObject> ObjectsOf(const SaveFile& save);
        // Keeps the save the war map came from, so it can be written again: its objects (in ObjectsOf's order),
        // the travelling ones (sent on again from where they were), the message list and the pods.
        void UseSave(SaveFile save);
        bool CanSave() const { return save.has_value(); }
        void Message(std::string text) { Log(std::move(text)); }
        // The war map as a save (0x41AD80), or nothing if it didn't come from one read exactly.
        std::optional<std::vector<uint8_t>> SaveData() const;
        void SetArrowTypes(std::map<int, ObjOjd::ArrowType> types) { arrowTypes = std::move(types); }
        void SetDeposits(std::map<int, std::array<int, 6>> d, float dropOff) { deposits = std::move(d); resourceDropOff = dropOff; }
        // [Tweak] "Distance Crippler VX" and "VY" (0.5) and "Resource Buffer" (30)
        void SetEconomy(float vx, float vy, float buffer) { crippleX = vx; crippleY = vy; resourceBuffer = buffer; }

        // The economy (0x41A760; docs/WARMAP.md "The economy"): each sector's figures for the six resource kinds
        // (0-2 Human, 3-5 Martian), at the sector record's +0x38 demand, +0x50 spread demand, +0x68 supply,
        // +0x80 spread supply and +0x98 ratio
        struct SectorEconomy { std::array<float, 6> demand{}, effDemand{}, supply{}, effSupply{}, ratio{}; };
        const SectorEconomy& Economy(int sector) const { return economy[size_t(sector)]; }
        float Efficiency(int sector) const { return efficiency[size_t(sector)]; }
        // the length (sectors, the start included) of the cached land path from one sector to another, 0 for none
        int SectorPathLength(int from, int to) const { return pathLength[size_t(from)][size_t(to)]; }

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
        int speed = 0;                   // index into warmap::Speeds (the calendar's +0x24; 0 when it's made)
        // Time running (world +0x64, 0x41A6C0): on, it's the player's action 0xA and the opponent's 0xC; off, 0xB
        // and 0xD. Each on adds one to world +0x68 (0x8050 / 0x8054), each off takes one away and stops the
        // clock (0x8051 / 0x8055 -> speed 0); while it's exactly 1 (one side waiting for the other) the calendar
        // flashes (the clock document's +0x38)
        bool timeRunning = false;
        int timeVotes = 0;
        bool calendarWaiting = false;
        void SetTimeRunning(bool on);
        void SetSpeed(int index);        // 0x41F640
        bool SpeedChangeAllowed() const; // 0x41AEA0
        int forcedHours = 0;             // the calendar's +0x30: hours to go on by, one a tick
        int hoursPassed = 0;             // whole hours the last tick went over (0x802D's count)
        void Hourly(int hours);          // the world's 0x802D
        // Martian pods (0x41A330): world +0x48 hours since the last, +0x4C how many have come
        int podHours = 0, pods = 0;
        int podInterval = 24;            // [Tweak] "Pod Interval (hours)"
        void DropPod(int hours);
        int alertCount[3] = {}, warningCount[3] = {}; // 0x4E9BA0 / 0x4E9BB0
        void ResourceWarnings();
        std::array<bool, warmap::SectorCount + 1> podFlag{}; // a sector's +0x64: a pod landed among the Humans
        std::array<bool, warmap::SectorCount + 1> battle{};  // a sector's +0x124: a battle there
        void AddUnit(int type, int sector, bool players);
        int AddCounter(int type, int sector, bool players, int ref);
        // the save it came from, and its objects (counters hold indices into it)
        std::optional<SaveFile> save;
        std::vector<SaveObject> table;
        std::map<int, ObjOjd::UnitData> unitData;
        float ClockTick();
        void MoveRoutes(int hours);      // the routes' 0x802D (0x425080)
        void Arrive(int index, int sector, int previous);
        void ArriveTogether();           // 0x41FCF0
        // who holds each sector: 0 the side being played, 1 the other side, 2 no one
        std::array<int, warmap::SectorCount + 1> owner{};
        int viewMode = 1;                // 1 units, 2 constructions, 3 resources (0x419810, the WWarMapWorld's +0x70)
        int rightDownAt = -1;            // the update the right button went down on
        int quickCancelTimeout = 5;      // [Tweak] "Quick Cancel Timeout"
        int hotButton = -1;              // the panel button under the mouse (3: the Research button)
        bool researchHeld = false;       // the Research button, pressed
        int resourceKind = 0;            // the Resource view's kind, 0-2 (dword_4E9F40; F3-F5 or Space)
        bool panelShown = true;          // the mini-map and its frame (Tab, 0x41A450)

        struct PanelLayout { Rect frame, mini, buttons[3], research; };
        PanelLayout Panel() const;

        // The player's selection (the player object's +0x430, filled by its commands 4 clear, 5 select, 6 drop),
        // the cursor mode (0x4EA43C: 0 normal, 1 after a rubber band, 2 a move ready), the rubber band (tracker
        // 0x4E9EE0) and a counter pressed (it takes the mouse, 0x42E9B0), and the route preview for the sector
        // under the mouse (world +0x8C, 0x41D590 -> 0x419520): a route for each group of selected units
        std::vector<int> selection;
        bool Selected(int counter) const;
        int cursorMode = 0;
        bool leftHeld = false, rubberBand = false;
        int dragX = 0, dragY = 0;        // where the button went down (on the screen)
        int highlighted = 0;             // world +0x114: counters in the band
        int pressedCounter = -1;
        int previewSector = -1;
        void BuildPreview(int sector);
        void ClearSelection() { selection.clear(); previewSector = -1; }
        void UpdateBand();
        // The "Warmap menu" (WPopupMenu 0x432C60, opened by 0x41D120 when the right button is held): a column of
        // icon buttons (WHotButton 0x434300), one row a command
        struct PopupItem { int command, icon, text; Rect rect; };
        std::vector<PopupItem> popup;
        int popupHover = -1;
        void OpenWarmapMenu(int x, int y);
        void ClosePopup(bool escape);
        void PopupCommand(int command);
        int clunWidth = 4, clunSpacing = 4;
        float cluntingSpeed = 7.0f;      // [Tweak] "Clunting Speed" (0x428B70, default 7): only for units with no speed
        std::string message;             // the engine's own note (a refused route), shown in the status strip
        // The message list (PopupMessageController 0x420A30 at 0x28, 4; lines added by 0x4191A0 -> 0x420DA0):
        // at most 10 (0x4BA4FC, the oldest goes), each a "Message Item" (0x428810, font 0x72) shown for 5 x 30
        // updates (0x4BA4F8 x "Game Frequency"), drawn top down 16 pixels apart (0x4BA500, 0x420C10).
        // state: 0 live, 1 run out (scrolling away), 2 gone (kept for the history)
        struct LogLine { std::string text; int life = 0; int state = 0; uint32_t hour = 0; };
        static constexpr int LogLife = 5 * 30; // 0x4BA4F8 x "Game Frequency"
        std::vector<LogLine> log;
        int logScroll = 0;               // +0x1C: 0 to -16
        bool logScrolling = false;       // +0x20
        int logPending = 0;              // +0x38: lines run out and still to scroll away
        bool historyOpen = false;        // the world's +0x6C
        void Log(std::string text);
        void AgeLog();
        static void SetLife(LogLine& line, int life);
        void ToggleHistory();
        void ReshowLastMessages();
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
        std::map<int, std::array<int, 6>> deposits;  // each sector's deposit of each resource kind (OBJ.ojd record 8)
        float resourceDropOff = 3.0f;                 // [Tweak] "Resource Drop Off Rate" (0x423F40)
        float ResourceGain(int sector, int kind) const;
        float crippleX = 0.5f, crippleY = 0.5f, resourceBuffer = 30.0f;
        std::array<SectorEconomy, warmap::SectorCount + 1> economy{};
        std::array<float, warmap::SectorCount + 1> efficiency{}; // table 1 (0x4E9930): the Construction view
        float efficiencyMin = 0, efficiencyMax = 0;               // 0x4E99A8 / 0x4E99AC
        bool economyStarted = false;
        void EconomyPhase(int phase);
        float FacilityOutput(int sector, int kind) const;
        // the sectors' cached paths (each sector's +0xA0, WSectorPath 0x4A39E0, rebuilt on 0xB7), and what they
        // were built from: each sector's owner and whether it has its command building
        std::array<std::array<int, warmap::SectorCount + 1>, warmap::SectorCount + 1> pathLength{};
        std::array<int, warmap::SectorCount + 1> pathsBuiltFrom{};
        bool pathsBuilt = false;
        void BuildSectorPaths();
        void ShadeSectors();
        void PulseBattles();
        // the types of the finished constructions in each sector (both sides')
        std::array<std::vector<int>, warmap::SectorCount + 1> finished;
        // The counter placement ("clunting", 0x420160 at world +0x74): each sector's grid of cells (0x428110), cells
        // `size` pixels square over the sector's bounds rounded out, a cell usable when CHI.SPR (GType 0x84) has the
        // sector's number at its corner; 1 usable, 0x81 taken. A search keeps its place in the spiral (`index`).
        struct CellGrid { int size = 0, left = 0, top = 0, w = 0, h = 0, index = 0; std::vector<uint8_t> cells; };
        std::array<CellGrid, warmap::SectorCount + 1> grids;
        int clunRes = 36, clunAdj = 4;   // [Tweak] "WM clunting res" (0x4EA0D4) / "WM clunting adj" (0x4EA0EC)
        std::vector<int> placed;         // the counters placed, in order (+0x8)
        void BuildGrid(int sector, int size);
        bool FindCell(int sector, int& x, int& y, int& cell);
        void PlaceCounter(int index);    // 0x4205F0
        void FreeCounter(int index);     // its cell given back (0x4208B0 / 0x420930)
        void RefreshSector(int sector);  // 0x43CB60
        void LayoutAll();                // a view change: 0x4204A0, then 0x43CB20 for every sector
        void SetView(int mode);
        void SlideCounters();            // 0x4209E0
        void SendTo(int counter, int sector);

        struct Counter
        {
            int x = 0, y = 0, frame = 0, count = 0;
            bool players = false;
            int kind = 0;                // 0 units, 1 constructions, 2 resources
            int type = 0;                // the object type (unit type) it stands for
            bool shared = false;         // class 7: one counter for every object of the type in the sector (+0x8C)
            int finishedCount = 0;       // of those, the finished ones (0x42AA70)
            bool unfinished = false;     // a construction not finished yet (drawn at 50% when it's ours)
            float progress = 1.0f;       // a construction's +0x7C / +0x78
            int sector = 0;
            int speed = 0;               // the unit type's war map speed (unit +0x54), 0 for constructions and resources
            int moveClass = 0;           // the unit type's +0x4E: 0-2 land, 3 boats, 4 fliers
            // Travelling (WWarMapRoute 0x4240D0 and its RouteEntry 0x42AFE0; a counter is a route of one entry):
            // the route's sectors, the start first (+0x4C, count +0x48)
            std::vector<int> path;
            bool Travelling() const { return path.size() > 1; }
            float remaining = 0;         // entry +0x30: the way still to go, waypoint to waypoint
            float hoursLeft = 0;         // entry +0x34
            float segmentLeft = 0;       // entry +0x38: of the way to the next sector's waypoint
            float segmentLength = 0;     // entry +0x3C
            int segment = 0;             // entry +0x24: the path index it last reached
            bool segmentStarted = false; // entry +0x28: set on reaching a sector, cleared half way to the next
            int offsetX = 0, offsetY = 0; // entry +0x10 / +0x14: its place in the formation
            int hoursOn = 0;             // route +0x30: game hours it has been going
            bool interrupted = false;    // route +0x60
            bool naval = false;          // route +0x50: its first unit's +0x52 is 5 or 6
            int trim = 0;                // the arrow's +0x5C: drawn from this curve point on (behind the units)
            float labelHours = 0;        // route +0x6C: the hours to arrival the attack label shows
            uint16_t labelFrom[6] = {};  // the date the label counts from
            // the route's arrow (0x4244E0): its curve points (x, y and heading in 4096ths of a turn), and whether it's
            // an attack (into a sector the other side holds: the big arrow and the arrival label)
            struct ArrowPoint { float x, y; int heading; };
            std::vector<ArrowPoint> arrow;
            bool attack = false;
            std::vector<int> refs;       // its objects in the save's table
            bool moved = false;          // sent somewhere since it was read (its objects then take its place)
            // its place in the counter placement (0x428620): the cell (-1 for none), and a slide to a new cell
            // (0x4286C0: the target, the step a tick and the distance still to go)
            int cell = -1;
            bool inBand = false;         // inside the rubber band (0x440680)
            float slideX = 0, slideY = 0, stepX = 0, stepY = 0, slideStep = 0, slideLeft = 0;
            int targetX = 0, targetY = 0;
        };
        float PixelsPerHour(const Counter& k) const; // how far a counter goes in a game hour
        int CrewOf(const Counter& k) const;          // the men its objects carry
        std::vector<int> FindRoute(int from, int to, const std::vector<int>& group, std::string* why = nullptr) const;
        std::vector<int> PlanRoute(const std::vector<int>& group, int sector, std::string* why = nullptr) const;
        void StartRoute(int counter, const std::vector<int>& path, int n);
        std::vector<Counter::ArrowPoint> RouteCurve(const std::vector<int>& path, bool naval) const;
        void DrawRouteArrow(Canvas& canvas, const Counter& k) const;
        static std::vector<Counter::ArrowPoint> ArrowCurve(const std::vector<std::pair<float, float>>& control);
        std::vector<Counter> counters;   // what's drawn: one per kind of object, with how many there are
        struct Preview { std::vector<int> group; std::vector<int> path; Counter shape; };
        std::vector<Preview> preview;
    };
}
