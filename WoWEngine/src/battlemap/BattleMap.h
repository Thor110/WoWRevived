// The battle map (EnterBattleMap 0x439770, WBattleMap 0x484A30, WLandscapeView 0x49F240): so far its ground. It
// builds the sector's landscape (WLandscape 0x47AF10: the map OBJ.ojd record 8 names, lit for the hour of the
// game's clock) and draws it with the terrain view, scrolled by the view's keys (0x485610) and its edges (0x484DF0).
// Escape opens the battle map menu (0x80B8); the right button held opens the popup with Return to Warmap (0x76).
// See docs/BATTLEMAP.md.
#pragma once
#include "core/Landscape.h"
#include "core/ObjOjd.h"
#include "core/TerrainView.h"
#include "core/IsoObject.h"
#include "core/Scenery.h"
#include "core/WofView.h"
#include "core/Deployment.h"
#include <algorithm>
#include <map>
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
            int enemyCount = 0;         // dword_4F7430 for this battle (the scenery's burnt trees)
            bool loadScenery = true;    // [Debug] "Load scenery" (0x4F73F1)
            const std::map<int, ObjOjd::UnitData>* units = nullptr;
            const std::map<int, ObjOjd::BattleTables::Six>* six = nullptr;  // OBJ.ojd record 6 (the WOF GTypes)
            const std::map<int, ObjOjd::BattleTables::Thirteen>* lights = nullptr; // record 13 (the light parts)
            bool showLights = true;     // [Options] "Show lights" (byte_4B8808)
            std::vector<BattleGroup> groups; // the sector's war map objects (WarMap::BattleGroups)
            int ownerFaction = 2;       // who holds the sector (0x43CA30): 0 Human, 1 Martian, 2 no one
            int noBuildBorder = 16;     // [Tweak] "No-build border width (vertices)" (dword_59D93C)
            // [Vehicles] (each setting's value at its object's +0x14)
            struct Vehicles
            {
                float maxAcc = 1.0f;                // flt_59E33C
                float targAccWeight = 0.003f;       // flt_59E30C
                float targAccMax = 0.15f;           // flt_59E234
                float arrivalRadius = 1.5f;         // flt_59E294
                float speedCostDivisor = 25.0f;     // flt_59E2DC
                float separationAccWeight = 0.2f;   // flt_59E3B4
                float separationAccMax = 0.1f;      // flt_59E324
                float velMatchAccWeight = 0.08f;    // flt_59E24C
                float velMatchAccMax = 0.03f;       // flt_59E204
            } vehicles;
            int zMapRes = 400;          // [System] "ZMapRes" (dword_4F7424): the bucket grid's cell size
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
        int CursorAt(int x, int y) const override;
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
        // The scenery (dat\<sector>.nsb) as RTypeISO objects: the IOB models and their lights by GType, each
        // object's screen point and depth, and the time-of-day shade tables of the slots they use
        struct IsoModel { IobModel model; std::vector<uint8_t> lights; int slot = 0; };
        std::map<int, IsoModel> models;
        struct ViewObject { int key; int x, y; const IsoModel* model; const WofInstance* wof = nullptr; int slot = 0; int sprite = 0; int light = -1;
                            int unit = -1, depth = 0; };
        std::vector<ViewObject> objects;    // sorted by key
        std::map<int, ShadeTable> slotTables;
        std::map<std::string, std::vector<uint8_t>> masks;   // the shadows' .raw masks
        std::vector<std::unique_ptr<WofInstance>> wofs;      // the bridges' segments, posed once
        struct Light { int x, y, z, range; float intensity; };
        std::vector<Light> lights;                           // the light parts that stayed (RTypeLight 0x4A27A0)
        Landscape::Sun sun;
        void LoadScenery(const Landscape::Sun& sun, char letter);
        void PlaceBridge(const PlacedObject& o);
        // The battle units (each war map object's +0x4C list): type, place, heading (4096 a turn), whose
        // A vehicle's driving state (the ground vehicle 0x490820: its fields by their offsets; docs/BATTLEMAP.md
        // "Driving")
        struct Drive
        {
            uint32_t flags = 0x801;     // +0x188 (0x490E70): bit 0 stopped, 1 arrived, 2 arriving, 13 set when it starts
            int heading = 0;            // +0x100, 4096 a turn (0 when it's added to the world)
            float speed = 0;            // +0x104
            float vx = 0, vy = 0;       // +0x108 / +0x10C, the steering velocity
            float dx = 0, dy = 0;       // +0x88 / +0x8C, the step it takes
            int tx = 0, ty = 0;         // +0xF4 / +0xF8, the target
            int dist = 0;               // +0xFC, to the target
            float arrival = 0, grow = 0;// +0x118 / +0x11C
            int phase = 0;              // +0x146 (rand() at first)
            int turnRate = 64;          // +0x140 by move class: 64, 8 for 5 / 6, 16 for 7
            int radius = 0;             // +0xAC, the type's +0x44 word
            int pendingAt = -1, pendingValue = 0; // a 0x808B on its way (0x4355A0)
            int order = -1;             // the move order it's on (its group's arrival count)
        };
        struct Unit { int type; float x, y; int heading; bool players; int group = 0; bool vehicle = false; Drive drive;
                      WofInstance* wof = nullptr; };
        std::vector<Unit> units;
        int startX = 0, startY = 0;                          // dword_4F742C / 4F7434: the player's deployment centre
        void Deploy();
        void Place(std::vector<size_t> members, int cx, int cy);
        void AddRenderer(int type, float x, float y, int heading, int unit = -1);
        // Driving (Driving.cpp): the move order (0x482E60 straight there, 0x80C2), each update's steering and step
        // (0x4913B0, 0x492F40, 0x492DE0, 0x491870), halt (0x8031)
        struct MoveOrder { int members = 0, arrived = 0; };
        std::vector<MoveOrder> orders;
        void OrderMove(int x, int y);
        void Halt();
        void Drive(size_t index);
        void Steer(Unit& u);
        void SpeedControl(Unit& u, float wanted);
        void Advance(Unit& u);
        // 0x492C00 / 0x492A90: what a unit's neighbours add up to (+0x120..+0x144)
        struct Neighbours { float sepX = 0, sepY = 0, stepX = 0, stepY = 0, posX = 0, posY = 0; int matched = 0, count = 0; };
        Neighbours Gather(const Unit& u) const;
        void Post808B(Unit& u, int value, int delay);
        double Radius(int type);
        std::map<int, std::vector<uint8_t>> wofFiles;        // each WOF GType's file, loaded once
        // The minimap ("BattleMap miniMap view" 0x4868B0, RTypeMiniBattleMap 0x4A1F90): the sector's picture at the
        // bottom right, the objects' dots (0x4A1CD0, made again every 16 frames), the view's rectangle
        struct Dot { int x, y; uint32_t colour; };
        const IndexedImage* miniPicture = nullptr;
        int miniX = 0, miniY = 0, miniW = 0, miniH = 0;
        int rectW = 0, rectH = 0, rectX = 0, rectY = 0;   // +0x98's size, +0x90 / +0x94
        std::vector<Dot> dots;
        int miniFrames = 0;                                 // +0x88
        bool miniShown = true, miniDrag = false;
        uint32_t rectColour = 0, vehicleColour = 0, otherColour = 0, enemyColour = 0;
        void SetUpMinimap();
        void MakeDots();
        bool InMinimap(int x, int y) const { return miniShown && miniPicture && x >= miniX && y >= miniY && x < miniX + miniW && y < miniY + miniH; }
        void MinimapScroll(int x, int y);
        // Selection (docs/BATTLEMAP.md "Selecting"): the selected groups (each war map object's units are one group,
        // WBMGroup), the drag rectangle (0x49C080) and the hotspots (0x496D60)
        std::vector<bool> selected;                         // by group (setup.groups' index)
        bool dragging = false, boxActive = false;
        int dragX0 = 0, dragY0 = 0, dragX1 = 0, dragY1 = 0;
        uint32_t boxInner = 0;                              // dword_4DAFB8
        bool UnitScreen(const Unit& u, int& sx, int& sy) const;
        int Selectable(const Unit& u) const;                // its +0x40 byte (half the hotspot), or 0
        void ReleaseLeft(int x, int y);
        // dword_4EA43C, the cursor mode: 1 (orders) with something selected, else 0 (0x800F, 0x4853A0)
        int Mode() const { return std::find(selected.begin(), selected.end(), true) != selected.end() ? 1 : 0; }
        void ClearSelection() { selected.assign(selected.size(), false); }
        const Unit* HotspotAt(int x, int y) const;
        void CentreOnSelection();
        void DrawSelection(Canvas& canvas) const;
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
