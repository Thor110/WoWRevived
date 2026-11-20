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
#include "core/BuildList.h"
#include "core/Research.h"
#include "frontend/Briefing.h"
#include "frontend/Menu.h"
#include "warmap/WarMapData.h"

#include <algorithm>
#include <array>
#include <functional>
#include <deque>
#include <string>
#include <utility>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace wow
{
    class WarMap : public Screen
    {
    public:
        static constexpr int Id = 0x8A; // the command that starts it
        static constexpr int ResearchCommand = 0x7D; // the research screen (0x41A450 'R', the Research button)

        // A battle (0x8072 -> 0x41B210 -> 0x8019 -> 0x71 / 0x98 -> 0x97 -> 0x99 / 0x8017; see WARMAP.md "Battles").
        // The war map sends 0x8133 (the briefing transition and disc check) with 0x8019 (the Briefing) to follow.
        static constexpr int BattleCommand = 0x8133;
        BriefingInfo Briefing() const;             // the Briefing's figures (0x4999E0) for the battle's sector
        void FightBattle();                        // 0x71 / 0x98: the battle (*provisional*: settled here)
        const DebriefingInfo& Debriefing() const { return debriefing; }
        void ReturnFromBattle();                   // 0x99 / 0x8017, then the view's 0x418980: 0x808D and 0x41E730

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
        // [Tweak] "HumanFriend / HumanEnemy / MartianFriend / MartianEnemy Manufacture Modifier" (0x437664-0x437724)
        void SetManufacture(const ObjOjd::ManufactureModifiers& m) { manufacture = m; }
        void SetBuildListPlace(int left, int top) { buildLeft = left; buildTop = top; }
        // The computer player's [Tweak] values (0x4424D0-0x442850) and AI.ojd's strength weights, scaled by
        // [Tweak] "AI strength table Human / Martian multiplier" (1.0 / 2.0) as the file is read (0x441625)
        struct AiTweaks
        {
            float frontlineDefences = 2.0f;   // "AI Frontline Attractor for Defences"
            float effThreshold = 0.6f;        // "AI Eff Threshold"
            float panicBuild = 500.0f;        // "AI Panic Build Threshold"
            float moveThreshold = 1.05f;      // "AI Move Threshold"
            float frontlineAttractor = 2.0f;  // "AI Frontline Attractor"
            float resfacEagerness = 50.0f;    // "AI Build Resfac Eagerness"
            float aggression = 0.5f;          // "AI Aggression Value" (read, used nowhere)
            float invasionThreshold = 150.0f; // "AI Invasion Threshold PC"
            float outnumberForce = 1.1f;      // "AI Outnumber Force"
            float retainForce = 0.2f;         // "AI Retain Force"
            float invaderNFriendMul = 3000.0f;// "AI Invader NFriend Mul"
            float safeRepeller = 2.0f;        // "AI safe sector repeller Mul"
            float minimumRr = 8.0f;           // "AI Minimum Rr"
            float temperature = 0.25f;        // "AI Movement Temperature"
        };
        void SetAi(const AiTweaks& tweaks, std::map<int, int> weights) { aiTweaks = tweaks; aiWeights = std::move(weights); }
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
        // the [Event] options (the Critical event options menu): the setting's name and its default
        void SetEventOptions(std::function<bool(const char*, bool)> get) { eventOption = std::move(get); }
        void SetDeposits(std::map<int, std::array<int, 6>> d, float dropOff) { deposits = std::move(d); resourceDropOff = dropOff; }
        // Both sides' research (0x417CD0: the local player's first, dword_4E9B88, then the other side's, 4E9B8C),
        // read from the save when there is one (HRSH, MRSH), and the war map view's trigger update (0x418D17)
        void StartResearch(const ObjOjd::ResearchModifiers& modifiers);
        Research* LocalResearch() { return research[0].get(); }
        Research* OtherResearch() { return research[1].get(); }
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
        int AddUnit(int type, int sector, bool players); // the new counter, or -1
        int AddCounter(int type, int sector, bool players, int ref, int kind = 0); // kind 0 units, 1 constructions, 2 resources
        int AddObject(int type, int sector, bool players, int kind); // kind as a save list: 0, 2 or 4
        // the save it came from, and its objects (counters hold indices into it)
        std::optional<SaveFile> save;
        std::vector<SaveObject> table;
        std::vector<int> destroyed;      // refs into the table of objects gone from the map (not written back)
        std::map<int, ObjOjd::UnitData> unitData;
        // The factories (WBuildList, docs/WARMAP.md "Production"): each object's that has one, by its place in the
        // table, as read from (and written back into) the end of its saved bytes
        std::map<int, ObjectTail> factories;
        ObjOjd::ManufactureModifiers manufacture;
        void ReadFactories();
        void Produce(int ref, int hours);        // 0x45FC90, the factory's 0x802D
        bool PopEntry(int ref, ObjectTail& f);    // 0x45FF00
        bool HasRoomFor(int type, int sector) const; // 0x43AC10 / 0x43AC40
        int CounterOf(int ref) const;
        void StartConstruction(int ref, float hours);   // 0x80AE
        void ConstructionProgress(int ref, float hours);// 0x80AD
        void FinishConstruction(int ref);               // 0x80AC (0x42A980)
        bool AddType(int ref, int type);                // 0x4603B0
        bool OrderConstruction(int ref, int type);      // 0x460240, war map mode
        void SetBusy(int ref, bool on);                 // 0x45FE60
        void RemoveEntry(int ref, int index);           // 0x460080
        void PromoteEntry(int ref, int index);          // 0x4605A0
        void SlotClicked(int ref, int index);           // 0x4625B0
        bool ItemEnabled(int ref, int type, bool* battleMapOnly = nullptr) const; // 0x462DC0
        void SaveFactory(int ref);
        void RemoveObject(int counter, int ref);        // one object taken off the map
        // The Build List window (warmap/BuildWindow.cpp): the object it's for, the list's and queue's scroll
        // positions (+0x234, +0xA4), what the mouse is on and what it went down on
        struct BuildWindowState { int ref = -1; int scroll = 0, queueScroll = 0, hover = -1, pressed = -1; };
        std::optional<BuildWindowState> buildWindow;
        int popupObject = -1;                           // the object whose items the popup has
        // The computer player (warmap/WarMapAi.cpp): its phase (+0x43C), its units in the order they move
        // (dword_546230, the cursor +0x440 and count +0x444), its clock (+0x448), its research (+0x454) and the
        // strength gathered (+0x460)
        struct AiState { int phase = 0; size_t cursor = 0; std::vector<int> groups; float clock = 0, total = 0; int researchItem = -1; } ai;
        AiTweaks aiTweaks;
        std::map<int, int> aiWeights;                   // word_518B10
        std::array<int, warmap::SectorCount + 1> aiCooldown{}; // each sector's +0x68: days before it's invaded again
        std::array<int, warmap::SectorCount + 1> sectorHours{}; // +0x6C: hours towards the next day
        float ObjectStrength(int counter) const;        // 0x441E70
        int AiClass(int type) const;                    // 0x441EB0
        std::vector<int> SectorObjects(int sector, bool players, int kind) const;
        std::vector<int> AiNeighbours(int sector, int which) const; // 0 all, 1 the AI's, 2 the other side's
        bool AiCanEnter(int sector, int type) const;    // 0x4419E0
        float AiForce(int sector, int into) const;      // 0x441F00
        float AiEnemyForce(int sector) const;           // 0x442080
        void AiMove(int counter, int sector);           // 0xAD
        void AiTurn(int hours);                         // 0x442EC0
        bool AiGather();                                // 0x443030
        bool AiMoveGroup(int counter);                  // 0x443340
        void AiInvade();                                // 0x444690
        void AiLaunch(int target, float need);          // 0x444990
        void AiProduce();                               // 0x444C10
        int buildLeft = 0, buildTop = 70;               // [BuildList] "Left" (0) and "Top" (70) (0x460A80 / 0x460AC0)
        struct BuildLayout
        {
            Rect list, listUp, listDown, queue, queueUp, queueDown;
            std::vector<std::pair<int, Rect>> items;    // type, place
            std::vector<std::pair<int, Rect>> slots;    // queue index (-1 empty), place
            std::pair<int, int> ttc{};
        };
        BuildLayout BuildWindowLayout() const;
        void OpenBuildWindow(int ref);
        void DrawBuildWindow(Canvas& canvas) const;
        Rect PromoteRect(const Rect& slot) const;
        int BuildWindowHit(int x, int y) const;
        bool BuildWindowClick(int x, int y, bool down);
        std::string BuildWindowTip(int hit) const;
        bool Working(int ref) const;              // 0x42A410: its factory has something queued
        std::vector<int> BuildOptions(int ref) const; // 0x45F970
        int RefOfId(uint32_t id) const;
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
        // an item: its command and parameter, icon and tool tip (text id, or label when set)
        struct PopupItem { int command, icon, text; Rect rect; int param = 0; std::string label; };
        std::vector<PopupItem> popup;
        int popupHover = -1;
        void OpenWarmapMenu(int x, int y);
        void ClosePopup(bool escape);
        void PopupCommand(int command, int param);
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

        // The war map's events (0x41E5B0, the table at 0x4BA0E8: handler, "is it critical" test, name). A
        // critical event while a box is up waits in a queue (0x4EA064) and comes when the box goes (0x41E730);
        // otherwise its handler runs, opening a box if its [Event] option is on.
        enum Event { BattleInSector = 0, ConstructionCompleted, UnitCompleted, ArrivedInSector, ResearchCompleted,
                     ResearchAvailable, ResearchAlert, ConstructorOnline, FacilityOnline, ConstructorIdle, FacilityIdle,
                     TravelInterrupted, PodLanded, ResourceWarning, ResourceAlert, CommandCentreBuilt,
                     EnteringNeutralSector, UndefendedSectorLost, EndGameWon, EndGameLost };
        // param: a sector, a counter (the object's), or as each event takes it; text: the research events' name
        void RaiseEvent(int event, int param, std::string text = {});
        // Battles (0x41AB00): the sector waiting for its battle (dword_4E9B98), 0 for none
        int pendingBattle = 0;
        void DetectBattles();
        bool Undefended(int sector) const;   // 0x43C8D0
        void ClearSide(int sector, bool players); // 0x43A250
        void RecomputeOwner(int sector);     // 0x43CA40
        // The battle report (0x421210 at 0x4E9B7C): the sector, the winning and losing sides (0 Human, 1 Martian, 2
        // none) and whether the game is over (+0x55)
        struct BattleReport { int sector = 0, winner = 2, loser = 2; bool gameOver = false; } report;
        int battleSector = 0;                // the sector the briefing is for (the world's +0x60)
        std::array<std::unique_ptr<Research>, 2> research; // the local player's, the other side's
        std::set<int> encountered;           // the unit table's +0xAC: types met in battle (set by the battle map)
        DebriefingInfo debriefing;
        void AnnounceBattleResult();         // 0x41AED0
        int EndCode() const;                 // 0x4212A0
        bool ending = false;                 // byte_4D2530: the game is over, the world stands still
        int endingDelay = 0, endingCode = 0;
        std::map<int, int> objectClasses;   // AI.ojd's class of each type (0x518B14)
        bool Critical(int event) const;
        void RunEvent(int event, int param, const std::string& text);
        void ObjectEvent(int counter, int suffix, bool box, int view); // 0x41ECD0 / 0x41EDC0's shared part
        void NextEvent();                // 0x41E730
        std::function<bool(const char*, bool)> eventOption;
        bool Option(const char* name, bool on) const { return eventOption ? eventOption(name, on) : on; }
        struct QueuedEvent { int event, param; std::string text; };
        std::deque<QueuedEvent> eventQueue;
        bool boxUp = false;              // byte_4EA0A0
        // The InfoBox (0x432520): the event's text, a second line, the command its button sends and whether Esc
        // cancels it
        struct InfoBoxState { std::string text, text2; int command = 0; bool cancellable = false; int sector = 0; };
        std::optional<InfoBoxState> infoBox;
        void OpenBox(std::string text, int sector, std::string text2, int command); // 0x41E4E0
        void BoxCommand(int command);    // 0x432B30
        Rect BoxRect() const;
        Rect BoxButtonRect() const;
        void DrawBox(Canvas& canvas) const;
        bool Visible(int counter) const; // 0x421630
        void RevealAfterBattle(int sector, int research);                          // 0x418E17
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
            int intel = 0;               // +0x3C: hours of intel left on the other side's (not saved)
            bool shown = false;          // activated by the last layout (0x421630)
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
