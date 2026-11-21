// The battle map (EnterBattleMap 0x439770, WBattleMap 0x484A30, WLandscapeView 0x49F240): so far its ground. It
// builds the sector's landscape (WLandscape 0x47AF10: the map OBJ.ojd record 8 names, lit for the hour of the
// game's clock) and draws it with the terrain view, scrolled by the view's keys (0x485610) and its edges (0x484DF0).
// Escape opens the battle map menu (0x80B8); the right button held opens the popup with Return to Warmap (0x76).
// See docs/BATTLEMAP.md.
#pragma once
#include <array>
#include <functional>
#include "core/Bridge.h"
#include "core/Landscape.h"
#include "core/ObjOjd.h"
#include "core/TerrainView.h"
#include "core/IsoObject.h"
#include "core/Scenery.h"
#include "core/WofView.h"
#include "core/Deployment.h"
#include <algorithm>
#include <map>
#include <set>
#include "frontend/Screen.h"
#include <memory>
#include <optional>
#include <string>

namespace wow
{
    class Sound;

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
            bool pod = false;           // the sector's +0x64: a pod landed here since the last battle (0x41A3BC)
            const std::map<int, ObjOjd::UnitData>* units = nullptr;
            const std::map<int, ObjOjd::BattleTables::Six>* six = nullptr;  // OBJ.ojd record 6 (the WOF GTypes)
            const std::map<int, ObjOjd::BattleTables::Thirteen>* lights = nullptr; // record 13 (the light parts)
            bool showLights = true;     // [Options] "Show lights" (byte_4B8808)
            bool showShadows = true;    // [Options] "Show vehicle shadows" (byte_4B8814)
            float wofBrightness = 2.5f; // [Game] "WOF Brightness" (flt_59F364): the point lights on the models
            float isoBrightness = 0.5f;
            float zoomerSpeed = 2.5f;
            float lookScale = 1.0f;     // [Tweak] "Audible radius multiplier" (flt_59E44C): the look round's sight x it
            int waterTwinkles = 20;     // [Tweak] "Water twinkles per frame" (dword_4F76E4, 0x439400)   // [Tweak] "Battlemap Zoomer Speed" (dword_4F744C, 0x43A440): Home's and '.''s glide // [Game] "ISO Brightness" (flt_4E8C04): the point lights on the IOB objects
            bool showSmoke = true;      // [Options] "Show Smoke & particles" (byte_4B880C)
            std::vector<BattleGroup> groups; // the sector's war map objects (WarMap::BattleGroups)
            int ownerFaction = 2;       // who holds the sector (0x43CA30): 0 Human, 1 Martian, 2 no one
            // The sector's neighbours as reinforcement points' sources (0x43D150): each sector, whether the player
            // holds it (0x4EAE1C against the player's side) and whether its holder's command building is there
            // finished (0x43C990)
            struct Source { int sector = 0; bool players = false, hq = false; };
            std::vector<Source> sources;
            bool stickyPopup = false;   // [Modern] "Sticky popup menus": the popup stays open once the right button's let go
            // 0x43AC40 on a sector: room there for `count` more of the player's land objects (else boats)
            std::function<bool(bool land, int sector, int count)> hasRoom;
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
                float avoidanceAccWeight = 0.15f;   // flt_59E2AC
                float avoidanceAccMax = 0.1f;       // flt_59E1EC
                float brakingAccWeight = 0.1f;      // flt_59E39C
                float brakingAccMax = 0.2f;         // flt_59E264
                float collisionCheckDist = 0.5f;    // flt_59E1D4
                float attributeAccWeight = 0.3f;    // flt_59E2C4
                float attributeAccMax = 0.2f;       // flt_59E27C
                float arrivalAccWeight = 0.33f;     // flt_59E414
                float arrivalAccMax = 0.2f;         // flt_59E3CC
                float attractAccWeight = 0.001f;    // flt_59E36C
                float attractAccMax = 0.1f;         // flt_59E2F4
                int startTurnRate = 12;             // dword_59E1BC: the walkers' turn that animates them
                float turnAnimRate = 0.02f;         // flt_59E1A4
            } vehicles;
            const std::map<int, std::array<int, 17>>* moveCosts = nullptr;      // OBJ.ojd record 10 (0x4F6E38)
            const std::map<int, std::pair<int, float>>* moveSlopes = nullptr;   // record 11 (0x4F04C0 / 0x4F0510)
            int zMapRes = 400;
            Sound* sound = nullptr;
            ObjOjd::ArmourModifiers armour;     // [Tweak] the armour modifiers (0x4373D0-0x437510)
            // a research item's state is 2 (0x45B890), by side and its place in that side's tree (the globals from
            // 0x588C40 Human, 0x588F20 Martian, four bytes each)
            std::function<bool(bool human, int index)> researchDone;
            // a side's improvement multiplier by its enhancement's type (Research::Multiplier: 1.0 until it's done)
            std::function<float(bool human, int type)> researchMultiplier;
            // the sector's Damage Layer (width, height, a byte a vertex; 0 x 0 for none yet), and [BattleMap]
            // "Damage reduction divisor" (dword_4D259C, 500)
            int damageWidth = 0, damageHeight = 0;
            std::vector<uint8_t> damage;
            int damageDivisor = 500;
            // the [BattleMap] fog keys (0x4344C0-0x434680)
            bool fogEnabled = true;
            bool clearDamage = false;   // the shym cheat (byte_4D25A0, 0x405039)
            float fogDarkness = 0.7f;
            int fogReturnRate = 5, fogUpdateFreq = 15, fogSmooth = 128, fogDropoff = 3, fogDamageDelta = 0, fogDamageRadius = 16;
            const std::map<int, std::array<int, 8>>* weapons = nullptr;     // OBJ.ojd record 12 (0x4F2F10)   // the front end's mixer: the battle's speech and engine sounds          // [System] "ZMapRes" (dword_4F7424): the bucket grid's cell size
        };

        // Throws when the map's files can't be read.
        BattleMap(Resources& res, const Setup& setup);
        ~BattleMap() override;

        void Update() override;
        void Draw(Canvas& canvas) const override;
        std::optional<Command> OnKey(int sdlKey) override;
        void OnKeyUp(int sdlKey) override;
        void OnMouseMove(int x, int y) override;
        std::optional<Command> OnMouseButton(int x, int y, bool down) override;
        std::optional<Command> ActivatePopupItem(size_t index);
        int PopupItemAt(int x, int y) const;
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
        mutable std::map<std::pair<const IsoModel*, int>, std::vector<uint8_t>> isoLights;   // base lights by level
        struct ViewObject { int key; int x, y; const IsoModel* model; const WofInstance* wof = nullptr; int slot = 0; int sprite = 0; int light = -1;
                            int unit = -1, depth = 0, effect = -1, prop = -1;
                            int part = -1;      // part: which of its unit's parts (the type's record 28s) it draws
                            // the renderer's shade level (+0x34, as a sprite takes it: the level vt+0x18 was given
                            // >> 2); -1: the default for its kind
                            int level = -1;
                            // the level vt+0x18 was given (+0x34 for a WOF or IOB renderer, 0-255): the fog's, set
                            // each update by its object's vt+0x5C (0x48B3D0)
                            int bright = 255; };
        // A scenery object (WScenery 0x4895E0 / 0x489380 on the damageable 0x489E20): its type, point and height
        // (+0x84), its health (+0xC4, the type's +0x4A to start with, bit 7 set), a 0x80A0 on its way, its light
        // updating: on the world's update list (dword_4D2550 +0x20), so its update (the smoulder, 0x48AEF0) runs
        struct Prop { int type = 0; float x = 0, y = 0, z = 0; int health = 0, dieAt = -1, light = -1; bool alive = true; bool updating = false; };
        std::vector<Prop> props;
        void DamageProp(size_t prop, int amount);               // vt+0xE0, 0x48AB20
        void PropDies(size_t prop);                             // 0x80A0
        void SmoulderProp(size_t prop);                         // WScenery's update is 0x48AEF0
        // The flying objects (0x487730 and its classes): WAnimEffect (0x488FB0: the factory's 0x15B-0x16E, 0x174-0x17A,
        // 918-920, 952-972, 988-993, 1000, 0x17C) and WExplosion on it (0x488180) animate, the frame counter (8.8, the
        // renderer's +0x38) going on by +0xC0 (0x80) an update (0x40EF20), gone past the last frame (vt+0x78); the
        // smoke (0x48DCB0: 0x16F-0x173) rises 1.5 an update as it does; a mortar's shell (0x488E80: 0x18B-0x196, 713,
        // 714, 1002) lands (0x488F00); 0x17B is just the flying object. All of them, not done animating, run their
        // emitters and take their step (0x487890: 0x47F050, vt+0x54 0x47F770), gone under the ground.
        enum class EffectKind { Animated, Smoke, Shell, Plain };
        struct Effect
        {
            int type = 0, gtype = 0, counter = 0, frames = 0; bool alive = false;
            int frameStep = 0x80;                   // +0xC0: the frame's step an update (8.8)
            EffectKind kind = EffectKind::Animated;
            float x = 0, y = 0, lift = 0;           // +0x78 / +0x7C, +0x84 (its height over the ground)
            float vx = 0, vy = 0, vz = 0, g = 0;    // +0x88..+0x90, +0x94
            std::vector<float> acc;                 // its emitter list (+0xB4): each entry's fraction (+8) and count (+4)
            std::vector<int> emitted;
            bool emitDone = false;                  // the list freed
            int count = 0, size = 0, group = -1;    // a shell's updates left (+0xC2), impact size (+0xC0), firer (+0xC4)
            int attacker = 0;                       // the firing unit's id (passed to its explosion's +0xCC)
            int depth = 0;                          // its view object's key over its y
            int born = -1;                          // the update it was made on
            int age = 0;                            // +0xD0: the cascading explosions' updates (0x48CDE0)
        };
        std::vector<Effect> effects;
        int SpawnEffect(int type, float x, float y, float lift = 0, float vx = 0, float vy = 0, float vz = 0, float g = 0);
        static EffectKind KindOf(int type);
        void UpdateEffects();                                   // each flying object's update
        void WaterTwinkles();                                   // 0x43A670
        void Emit(size_t effect);                               // 0x47F050
        void Smoulder(size_t unit);                             // 0x48AEF0 (from the unit's update 0x4967E0)
        void Puff(int type, int ownType, float x, float y, float lift, int radius);   // 0x48ADD0
        int AddLight(int type, int x, int y, int z);  // its index in lights, -1 for none           // a light part made (0x48DDC0, 0x48E050, 0x48DE80)
        void UpdateLights();                                    // 0x48E100 for the fading ones
        void Speak(int id, int group);                          // 0x481770 with the group's voice (0x449120)
        void SelectionSpeech(int group);                       // 0x439000's 0x109
        std::vector<ViewObject> objects;    // sorted by key
        std::map<int, ShadeTable> slotTables;
        std::map<std::string, std::vector<uint8_t>> masks;   // the shadows' .raw masks
        std::vector<uint16_t> shadowBlend;  // dat\b16.hsh (PolyLib's set-up, 0x40FEA0), for the shadows
        const std::vector<uint16_t>& ShadowBlend();
        std::vector<std::unique_ptr<WofInstance>> wofs;      // the bridges' segments, posed once
        // A light (0x48DD50 on 0x487A30: types 334-346): its point, range (its type's +0x44 word) and the intensity
        // its renderer (RTypeLight 0x4A27A0, +0x40) has. One whose record 13 has times (a, b, c) fades (0x48E100):
        // the phase (+0xCC), the updates left (+0xD0), the intensity it has (+0xC0) and its step (+0xC4).
        struct Light { int x, y, z, range; float intensity; bool fading = false, alive = true; int phase = 0, count = 0, type = 0;
                       float current = 0, step = 0; };
        std::vector<Light> lights;                           // the light parts that stayed (RTypeLight 0x4A27A0)
        Landscape::Sun sun;
        void LoadScenery(const Landscape::Sun& sun, char letter);
        void PlaceBridge(const PlacedObject& o);
        void Occupy(int type, float x, float y);                // vt+0xE4, 0x48A950
        // The battle units (each war map object's +0x4C list): type, place, heading (4096 a turn), whose
        // A route ("BatRoute" 0x499850): its nodes (16 bytes each in the game): x and y in vertices, the heading less
        // 0x400, and the link byte (a bridge)
        struct RouteNode { int x = 0, y = 0, heading = 0; uint8_t link = 0; };  // 16 bytes each, vertices
        struct Route { std::vector<RouteNode> nodes; };
        // A vehicle's driving state (the ground vehicle 0x490820: its fields by their offsets; docs/BATTLEMAP.md
        // "Driving")
        struct Drive
        {
            uint32_t flags = 0x801;     // +0x188 (0x490E70): bit 0 stopped, 1 arrived, 2 arriving, 3 something ahead,
                                        // 4 it's of the same group and not a vehicle going, 13 set when it starts
            int heading = 0;            // +0x100, 4096 a turn (0 when it's added to the world)
            float speed = 0;            // +0x104
            float vx = 0, vy = 0;       // +0x108 / +0x10C, the steering velocity
            float dx = 0, dy = 0;       // +0x88 / +0x8C, the step it takes
            int tx = 0, ty = 0;         // +0xF4 / +0xF8, the target
            int dist = 0;               // +0xFC, to the target
            float arrival = 0, grow = 0;// +0x118 / +0x11C
            float ax = 0, ay = 0;       // +0x110 / +0x114, from the lookahead to what it found
            int phase = 0;              // +0x146 (rand() at first)
            int turnRate = 64;          // +0x140 by move class: 64, 8 for 5 / 6, 16 for 7
            int radius = 0;             // +0xAC, the type's +0x44 word
            int pendingAt = -1, pendingValue = 0; // a 0x808B on its way (0x4355A0)
            int order = -1;             // the move order it's on (its group's arrival count)
            std::shared_ptr<const Route> route; // +0xEC, the route it's following (shared, as the game counts)
            int waypoint = 0;           // +0xF0
            int toPoint = 0;            // +0xD4, from the last order's point (0x43FF00)
            int link = 0;               // +0x18C, the bridge it's on (dword_59D9C0's index), 0 for none
            float lift = 0;             // +0x84, its height over the ground (a bridge's deck)
            int offset = 0;             // +0x13A, rand() % (0x802E's value + 1): more room at the route's end
            float frame = 2.0f;         // +0xA8, its model's animation frame (0x490E70)
            float smoothTurn = 0;       // +0x190, the walkers' turn, smoothed
            float climb = 0;            // +0x90, the fliers' upward speed
        };
        // A gun (WProjectile base 0x4949A0 and its classes): its type, its class's update (0 the shell's 0x4886A0,
        // 1 the rocket's 0x488830, 2 the mortar's 0x488CA0, launching shells), the fire cycle's state (+0xD8) and count (+0xD4),
        // armed (+0xD0), +0xF0 (bit 0 aimed, 1 landed), the point aimed at (+0xBC..+0xC4)
        // A mortar's (WMortar 0x488930) launch list (+0xB4, from its type's emitters, +0x3C): each entry's count of
        // shells fired; spent when an entry's passed its limit (the list freed)
        // Its hardpoint (+0xB0, from the part's record 27 box when box[0] is 0 or more): +4 the middle, +6 the half
        // width, against its own heading (+0x40: its parent's); its turret (-1 on the unit itself)
        struct Gun { int type = 0, cls = 0, state = 0, counter = 0; bool armed = false; uint32_t flags = 0; int tx = 0, ty = 0, tz = 0;
                     std::vector<int> fired; bool spent = false;
                     int turret = -1; bool hardpoint = false; int middle = 0, half = 0;
                     float range = 1.0f, accuracy = 1.0f;   // +0xC8 / +0xCC, by research (0x494AF0)
                     mutable float beamPhase = 0;           // +0xA8: the beam quads' frame step (0x495180), stepped as drawn
                     int piece = -1, vertex = 0;            // +0xB0: the point of its parent's model it's held to
                     mutable int flashFrame = 0;            // the shells' class's sprite's frame (+0xF4's +0x38)
                     int ox = 0, oy = 0, oz = 0;            // its part's offset from its parent (0x47EE10)
                     int x = 0, y = 0, z = 0; };            // its own point (+0x78 / +0x7C / +0x80), posed each update
        // A turret (0x489820 on the vehicle 0x490820: 0x2E6-0x313, 0x389-0x38C): its type, its part's offset, its turn
        // from its unit (+0x190), the point it's aiming at (+0xC8 / +0xCC, +0xE4 bit 1), its hardpoint and its turning
        // sound (+0xE8)
        struct Turret { int type = 0, ox = 0, oy = 0, oz = 0, turn = 0; bool aiming = false; int aimX = 0, aimY = 0;
                        bool hardpoint = false; int middle = 0, half = 0, sound = 0;
                        int x = 0, y = 0, z = 0; };         // its own point (+0x78 / +0x7C / +0x80), posed each update
        // A part drawn with its unit (a WOF of its own: a turret's, or another like the lorries' 324)
        // a part's model; piece / vertex: the point of its unit's model it's held to (the part's record 27 box, the
        // child's +0xB0), -1 for none
        struct Attached { WofInstance* wof = nullptr; int ox = 0, oy = 0, oz = 0, turret = -1; bool shadow = false;
                          int piece = -1, vertex = 0; int light = -1;       // light: a light part's index in lights
                          int part = -1; };                                 // which of the type's parts it is
        // group: its battle group (+0x34), which a selection takes it into (0x4480F0); ref: its war map object (+0x48,
        // setup.groups' index), for which side's list it's in, losses and retreats
        struct Unit { int type; float x, y; int heading; bool players; int group = 0; int ref = -1; bool vehicle = false; Drive drive;
                      WofInstance* wof = nullptr; int engineSound = 0;
                      int health = 0;       // +0xC4: the type's +0x4A (0x47DA20) to start with
                      std::vector<Gun> guns;         // its weapon children (and its turrets')
                      std::vector<Turret> turrets;
                      std::vector<Attached> attached;
                      int dieAt = -1;       // a 0x80A0 posted to itself (the update it arrives on)
                      int shoreAt = -1;     // an Ironclad's 0x80C1 (10 damage) posted to itself, off water
                      int aimX = 0, aimY = 0;   // +0xC8 / +0xCC: the point last aimed at (0x496200)
                      bool leaving = false;     // arrived with steering bit 9 (retreating): off the map (0x491A96)
                      bool aiming = false;      // +0xE4 bit 1: aimed (0x496200), till stood down (0x496440)
                      bool halted = false;      // +0xE4 bit 0 (vt+0xAC sets, vt+0xA8 reads): stopped by 0x8031 (0x496BC0)
                      int partsLeft = 0;    // a building's children (+0x50's count, 0x4216D0): its parts not yet lost
                      int id = 0;           // +0x08, the object's id (kept by a group's target, +0x30)
                      int scanCount = 0;    // +0xDA, updates since its last look round (0x4967E0)
                      int statusTime = 0, status = 0; // +0xDC / +0xE0: a timed status (0x80F5 frozen, 0x80F6, 0x80F7)
                      bool Frozen() const { return statusTime != 0 && status == 0x80F5; }  // vt+0x108, 0x489860
                      bool Turned() const { return statusTime != 0 && status == 0x80F7; }  // vt+0xF8, 0x489880
                      bool Detected() const { return statusTime != 0 && status == 0x80F9; }  // vt+0x10C, 0x4898A0
                      bool marked = false;  // its marker made (GType 0x359: a mine's +0xE8, a sinker's +0x19C)
                      int sink = 0;         // +0x194: the updates left going down or coming up
                      bool under = false, hidden = false;   // +0x190 bits 0 and 1
                      int ambientSound = 0, alertSound = 0; // a building's +0xE8 (its own loop) and +0xEC (0x111)
                      int footX = 0, footY = 0; // +0x13C / +0x13E: where its footprint is marked (both 0: none)
                      int droneTarget = 0;  // +0x190: a drone's object to run into (vt+0xC4 = 0x48FA80)
                      bool fogged = false;  // +0xB8 bit 3 (vt+0x9C / vt+0xA0): the other side's, hidden by the fog
                      int stance = 1;       // +0xD8: its type's +0x43 byte (record 56) to start with (0x495740)
                    };
        int nextId = 1;
        // The reinforcement points (0x43D150: dword_4F7458, 0x18 bytes each, dword_4F76E8 of them): the point, the
        // sector it leads to, the link kind (7, 5, 1), whether the player holds that sector; each with its marker
        struct ReinforcePoint { int x = 0, y = 0, sector = 0, kind = 0; bool owned = false; };
        std::vector<ReinforcePoint> rpoints;
        void SetUpReinforcements();                             // 0x43D150
        void Retreat(int group);                                // 0x480EF0
        void RetreatVia(int group, size_t point);               // 0x480EF0's / 0x481170's going
        void RetreatToMarker(int group, size_t point);          // 0x481F70's type 0x3DB case
        int MarkerAt(int x, int y) const;                       // a reinforcement point's marker under the screen point
        int NearestPoint(int x, int y, int moveClass, int count = 0) const; // 0x43D3F0
        std::vector<int> groupRetreat;                          // the sector a group's retreating to (0x80BB), 0 none
        std::vector<bool> groupLeft;                            // a group with a unit that left the map
        void LeaveMap(size_t unit);
        Unit* UnitById(int id);
        void EngineSound(Unit& u);                              // 0x491450
        // Each group's target (+0x2C, -1 a point: +0x40..+0x48) and where it was last aimed (+0x34..+0x3C), its mode
        // (+0x1DC) and engagement range (+0x4E)
        // (target: 0 none, -1 a point, else the object's id)
        struct GroupTarget { int target = 0; int x = 0, y = 0, z = 0, lastX = -1, lastY = -1, lastZ = -1; };
        std::vector<GroupTarget> groupTargets;
        std::vector<int> groupMode, groupRange;
        std::array<int, 8> WeaponData(int type) const;          // OBJ.ojd record 12 (0x4F2F10)
        void SetUpGuns(Unit& u);
        void ForceFire(int x, int y);                           // packet 0x13 (0x4812B0), each selected group
        void ForceFireGroup(int g, int x, int y);               // the group's target the point
        void GroupOrder(int g, int command);                    // 0x4817C0's special cases: the group's mode (+0x1DC)
        static int FireMode(int gunType);                       // 0x496920: the group mode a gun fires in
        void ComputerSpecials(int g);                           // 0x482570
        void MemberSpecial(size_t unit, int message, int level); // 0x495CB0's 0x80E1-0x80E3 / 0x80F0: 0x495C10
        void ReceiveSpecial(size_t unit, int message, int level); // 0x80F8 (0x48C9B0) / 0x80F9 (0x495CB0)
        void StopAttacking(int group);                          // the target cleared, the guns 0x805A
        void TargetGone(int group, const Unit& target, bool last); // 0x4821F0
        void Engage(int group);                                 // vt+0x38, 0x483760
        void AttackObject(int id);                              // packet 0x10 (0x4812B0 -> 0x481F70 -> 0x482880)
        void Approach(Unit& u, const Unit& target);             // vt+0xC4: 0x491100 -> 0x496300
        void DroneTick(size_t unit);                            // 0x48FAC0 / 0x48F9F0
        void MarkFootprint(Unit& u);                            // vt+0x110, 0x4915B0
        bool IsBuilding(const Unit& u) const;                   // the building class (0x48E8A0)
        // the gun emplacements (0x48F750 on the vehicles' 0x490820, +0xB8 bit 0 clear): the Medium and Heavy Guns, AA
        // Array, Rapid Heat Ray, Heat Ray Turret and Projectile Launcher, types 0x32-0x39 and 0x6F-0x76 (category 2,
        // but not the building class)
        static bool IsEmplacement(int type) { return (type >= 0x32 && type <= 0x39) || (type >= 0x6F && type <= 0x76); }
        float Progress(const Unit& u) const;                    // its war map object's +0x7C / +0x78 (1 for none)
        void BuildingSound(Unit& u);                            // its own sound (+0xA2) at its point, 0x4883E0
        void BuildingTick(Unit& u);                             // 0x48F130
        void LoseParts(size_t unit);
        void Stop8031(Unit& u);
        void MoveFlag(int x, int y);                            // 0x483590's flag, type 0x17C at level 0                                 // 0x8031 to a vehicle: 0x4910C0 -> 0x496BC0
        void StandDown(Unit& u);                                // 0x805A to a unit (its guns aside): vt+0xCC, vt+0xAC(0)                            // 0x48EF80's part after 0x48AB20
        const IsoModel* LoadIob(int gtype);                     // an IOB model and its lights, loaded once
        void StopUnitSounds(Unit& u);
        void ClearFootprint(Unit& u);                           // vt+0x114, 0x491630
        static bool IsGas(int type);                            // 0x393-0x395, WGasCloud
        bool GasTick(size_t effect);                            // 0x488D70
        bool LookRound(size_t unit);                            // 0x4967E0's count -> vt+0x104 (0x48F8A0, 0x496890); true when it looked
        void AutoTarget(const Unit& u);                         // 0x496570 -> the group's 0x80B1 (0x482960)
        void ChaseTarget(size_t group);                         // 0x483B70 -> 0x4826D0, first in GroupTick
        void Aim(Unit& u, int x, int y, int z);                 // vt+0xC8: 0x491110 -> 0x496200 -> guns 0x494D20
        void GunTick(size_t unit, Gun& g);                      // 0x494DB0 and the class's update
        void TurretTick(Unit& u, Turret& t);                    // 0x489B40
        int GunHeading(const Unit& u, const Gun& g) const;      // its +0x40: its turret's or its unit's
        void AddPartRenderers(size_t unit);
        void PoseChildren(Unit& u);                             // the turrets' and guns' own points
        // The view list's keys as the original's renderers have them (+0x1E y + +0x26 depth), worked out each update
        // for everything that goes with a unit, the parent's before the children's, then sorted (0x40D750)
        void RekeyViews();
        void AddCorners(size_t unit);   // 0x48E9D0: the four 0x17B objects round a Human building being built
        void PoseParts(Unit& u);
        void Impact(size_t unit, const Gun& g);                 // 0x4886A0's landing: 0x487F50
        void ImpactAt(int size, int x, int y, int z, int group, int attacker = 0, int weapon = 0); // 0x487F50: the effect by height, terrain and size
        void Launch(size_t unit, Gun& g);                       // 0x4889D0
        bool ShellLands(size_t effect);                         // 0x488F00 after the step: gone?
        struct Landing { int size, x, y, z, group, attacker, weapon; };
        struct Cascade { int type, x, y, z, firer, attacker, weapon = 0; };
        std::vector<Cascade> cascades;      // 0x48CDE0's next explosions, made once the effects are walked
        std::vector<Landing> landings;                          // made after the flying objects' updates
        // Damage and death (0x48A5A0, 0x48AB20, 0x80A0 in 0x489FA0) and the explosions (WExplosion 0x488180)
        int MaxHealth(int type) const;                          // 0x47DA20: +0x4A with the armour modifiers
        void Damage(size_t unit, int amount);                   // vt+0xE0, 0x48AB20
        void Die(size_t unit, bool quietly = false);            // 0x80A0; quietly: vt+0x78 0x495BB0, no explosion
        void KeyBuildingLost(bool players);                     // 0x48AC50's key building case
        void StaggerDeaths(int group, int at);                  // 0x47CEE0's 0x80A0
        bool forfeit = false;                                   // byte_4E9B94: the key building lost with no battle on
        int forfeitAt = -1;                                     // the update its 0x76 arrives (0x43AA18, + 150)
        // 0x488070 + 0x488350 / 0x488200 (firer: a unit's group); a radius and power given (0x4880F0's +0xC8 /
        // +0xCA) in place of the type's
        void Explode(int type, int x, int y, int z, int firer = -1, int attacker = 0, int weapon = 0, int radiusAs = 0, int powerAs = 0);
        void LandPod(const Landscape::Sun& sun);
        // the war map objects that have left by each sector this battle (dword_4F86B8 land, dword_4F76F0 boats: the
        // war map object's 0x80B9 case, 0x429453, once its last unit has gone), which the room check adds (0x43AC40)
        std::map<int, int> leftLand, leftBoats;
        bool HasRoom(bool land, int sector, int count) const;
        uint32_t GroupBits(int group) const;     // +0x1D8 bits 8 (a member of move class 5 / 6) and 9 (1-4), 0x4802E0
        bool begun = false;
        std::vector<std::array<int, 3>> heldSounds;             // sounds made before Begin (the pod's blasts): id, x, y
        int podX = -1, podY = -1;                               // where this battle's pod landed
    public:
        // As it's shown (the rest is built beforehand, behind the Briefing): the buildings' sounds started and the
        // pod landed, which make sounds
        void Begin();
    private:        // 0x439AE5 -> the pod's vt+0x14, 0x48F620
        static bool IsExplosion(int type);
        static bool IsAreaBlast(int type);
        void AreaDetonate(int type, int x, int y, int radius, int power, int firer, int attacker = 0, int weapon = 0);   // 0x487B50
        void SetStatus(size_t unit, int status, int level);     // 0x80F5-0x80F7 (0x495CB0)
        void ChangeStatus(Unit& u, int status);                 // vt+0x90, 0x496760
        void StatusTick(Unit& u);
        bool Targetable(const Unit& u) const;                   // vt+0x6C
        void MarkerTick(size_t unit);                           // 0x48CC00's / 0x498F20's marker (GType 0x359)
        static bool Sinker(int type);                           // 0x498D40's class: 7-9, 0x16-0x18
        bool OnWater(const Unit& u) const;                      // 0x47FFE0
        void StartSink(size_t unit, bool down);                 // vt+0x184, 0x4990D0
        void EndSink(size_t unit, bool under);                  // vt+0x188, 0x499150
        bool SinkTick(size_t unit);                             // 0x498F20 and its classes' updates
        void Surface(size_t unit);                              // vt+0x124, 0x499120
        void GoUnder(size_t unit);                              // 0x80EE / 0x80EF to a member, 0x498E90
        void Vanish(size_t unit);                               // vt+0x78, 0x495BB0
        void Upgrade(Gun& g, int side) const;          // 0x494AF0's research upgrades
        void Hit(size_t unit, double amount, int attacker, int by, int weapon = 0); // vt+0xC0 (0x498EE0, 0x491020, 0x4958B0), then the damage
        void Retaliate(int group, size_t member, int attacker); // 0x8085 to the group, 0x483880                               // 0x4967E0's timer
        void SetStance(int group, int stance);                  // 0x80B2 to the group: it and each member (+0xD8)
        void TurnOnOwn(Unit& u);                                // 0x496100
        std::vector<Unit> units;
        int startX = 0, startY = 0;                          // dword_4F742C / 4F7434: the player's deployment centre
        void Deploy();
        void Place(std::vector<size_t> members, int cx, int cy);
        void Muster(size_t index, int x, int y);        // 0x80C3 to a vehicle (0x490D06)
        void AddRenderer(int type, float x, float y, int heading, int unit = -1);
        // Driving (Driving.cpp): the move order (0x482E60 straight there, 0x80C2), each update's steering and step
        // (0x4913B0, 0x492F40, 0x492DE0, 0x491870), halt (0x8031)
        struct MoveOrder { int members = 0, arrived = 0, nearest = 0, farthest = 0, group = -1; };  // +0x56, +0x68 / +0x6C
        // The route finder (WRouteFinder 0x493DF0-0x494960, BatRoute 0x499420-0x4998D0; Routes.cpp)
        int rfCols = 0, rfRows = 0;                     // +0x30 / +0x34
        std::vector<std::vector<int>> rfLinks;          // each node's neighbours (connDat through connList)
        std::vector<int16_t> rfX, rfY;                  // nodeXCMem / nodeYCMem
        std::vector<uint8_t> rfFlags;                   // nodeFMem: the link byte (bridges)
        std::vector<std::vector<int>> rfExtra;          // links added later (0x494220's -2 chain), newest first
        std::vector<int> rfOwn;                         // a node's own list's link to its partner (0x494400), or -1
        // The bridges as links (dword_59D9C0[1..], counted from 1 by dword_59D9B8 as they're made, 0x486490)
        struct BridgeLink { int ax = 0, ay = 0, bx = 0, by = 0, length = 0; float segment = 0; };
        std::vector<BridgeLink> bridgeLinks;
        int InsertNode(int x, int y, int link);                                         // 0x494220
        void AddBridgeNodes(const Bridge& bridge);                                      // 0x4983D0
        bool OnBridge(Unit& u, int link);                                               // 0x4984E0
        const std::vector<int>& RouteNeighbours(int node) const;
        void SetUpRouteFinder();                                                        // 0x493DF0
        int LineCost(int group, int x0, int y0, int x1, int y1) const;                  // 0x4994B0
        int StepCost(int group, int goal, int a, int b) const;                          // 0x499660
        bool RequestRoute(int group, int sx, int sy, int& dx, int& dy, int radius, std::vector<int>& path); // 0x494440
        std::shared_ptr<const Route> BuildRoute(int group, const std::vector<int>& path) const;            // 0x494690
        void FollowRoute(Unit& u, int value, std::shared_ptr<const Route> route);       // 0x802E, 0x4911B0
        void SetWaypoint(Unit& u, int index);                                           // 0x491790
        std::vector<MoveOrder> orders;
        void OrderMove(int x, int y);
        void OrderGroupMove(int group, const std::vector<size_t>& members, int x, int y);
        int NearestPassable(int group, int vx, int vy, int& ox, int& oy) const;   // 0x482B00
        void Halt();
        void Drive(size_t index);
        void Steer(Unit& u);
        void Animate(Unit& u, float moved, int turn);   // vt+0x130
        void Paddle(Unit& u);                           // 0x48FDD0
        void Hover(Unit& u);                            // 0x490170
        void Wake(Unit& u);                             // vt+0x134, 0x4934F0
        void LowEffect(int type, float x, float y);
        // the guns' own visuals (0x495180 / 0x495280): an atlas quad, or the shells' class's sprite at the target
        struct BeamItem { int key = 0; const std::vector<uint8_t>* atlas = nullptr; const ShadeTable* shade = nullptr; size_t cell = 0;
                          int q[4][2] = {}; int level = 0; int sprite = 0, frame = 0, x = 0, y = 0; };
        void BuildBeams(std::vector<BeamItem>& out) const;
        void DrawBeam(const BeamItem& b, Canvas& canvas, const TerrainView::Target& t) const;
        void ShipTick(size_t index);
        bool Crushable(const Unit& o) const;            // vt+0xD8
        bool Collides(const Unit& u, const Unit& o) const;  // the vehicles' vt+0xBC, 0x4929A0                    // the Ironclads' update, 0x493B50
        // The water classes (the Ironclads' 0x4B0568, the Submersibles' 0x4B03C8)
        // The vehicle classes the factory (0x446B30) makes, each with its own vtable (0x490820's derived classes):
        // 7-9 the Tunnelling Track Layers (0x493B90), 0x13-0x15 the Ironclads (0x493870), 0x16-0x18 the water
        // sinkers (0x4935E0), 0x19-0x1B and 0x4D-0x4F the fliers (0x490120), 0x3E-0x4C and 0x50-0x5C the walkers
        // (0x48FC20), 0x5D-0x5F the drones (0x48F970), the rest plain (0x490820)
        enum class VehicleClass { Plain, Tunneller, Ironclad, WaterSinker, Flier, Walker, Drone };
        static VehicleClass ClassOf(int t)
        {
            if (t >= 7 && t <= 9) return VehicleClass::Tunneller;
            if (t >= 0x13 && t <= 0x15) return VehicleClass::Ironclad;
            if (t >= 0x16 && t <= 0x18) return VehicleClass::WaterSinker;
            if ((t >= 0x19 && t <= 0x1B) || (t >= 0x4D && t <= 0x4F)) return VehicleClass::Flier;
            if ((t >= 0x3E && t <= 0x4C) || (t >= 0x50 && t <= 0x5C)) return VehicleClass::Walker;
            if (t >= 0x5D && t <= 0x5F) return VehicleClass::Drone;
            return VehicleClass::Plain;
        }
        static bool Boat(int type) { const auto c = ClassOf(type); return c == VehicleClass::Ironclad || c == VehicleClass::WaterSinker; }
        static bool Walker(int type) { return ClassOf(type) == VehicleClass::Walker; }
        static bool Flier(int type) { return ClassOf(type) == VehicleClass::Flier; }
        // vt+0x148, the steering's period (0x492F7A): 1 (0x48FB30) for the water classes and the drones, else 3
        // (0x4898F0); vt+0x14C, the give in the lookahead's push (0x492350): 1.0 for the same (0x493B40 / 0x493860 /
        // 0x48FB40), else 8.0 (0x489900)
        static int SteerPeriod(int type) { const auto c = ClassOf(type); return c == VehicleClass::Ironclad || c == VehicleClass::WaterSinker || c == VehicleClass::Drone ? 1 : 3; }
        static double Give(int type) { return SteerPeriod(type) == 1 ? 1.0 : 8.0; }
        void SpeedControl(Unit& u, float wanted);
        void Advance(Unit& u);
        // 0x492C00 / 0x492A90: what a unit's neighbours add up to (+0x120..+0x144)
        struct Neighbours { float sepX = 0, sepY = 0, stepX = 0, stepY = 0, posX = 0, posY = 0; int matched = 0, count = 0; };
        Neighbours Gather(const Unit& u) const;
        std::vector<std::pair<int, int>> BucketCells(int px, int py) const;  // 0x449920 q 0 to 3
        const Unit* ObjectAt(const Unit& u, int px, int py, int reach) const; // 0x47FE10
        bool LookAhead(Unit& u);                                              // 0x492350
        // A group's ground (WBMGroup 0x4802E0): the flags, the max slope (+0x4C), the move class whose slope base is
        // largest (+0x52), the terrain costs (+0x70) and the slope costs (+0xB4)
        struct GroundGroup
        {
            bool airborne = true;       // +0x1D8 bit 5: every member of move class 7
            bool ownSector = false;     // bit 6: the members' side holds the sector
            int maxSlope = 1000;
            int cls = 0;
            int bridgeFactor = 0;       // +0x5A: the members' record 10 17th values averaged, -1 if any is
            uint32_t terrain[16] = {};
            int slopeCost[64] = {};
        };
        std::vector<GroundGroup> grounds;
        void SetUpGrounds();
        int CellCost(int group, int vx, int vy, bool strict) const;          // 0x480680
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
        std::vector<bool> selected;                         // by battle group
        // The battle groups (WBMGroup 0x480190): one for each war map object to start with (0x4216E0), then one made
        // for each selection that takes units from more than one (the world's vt+0x24, 0x43A600, from 0x439000)
        std::vector<bool> groupPlayers;                     // the group's side: the player's
        int AddGroup(bool players);
        // 0x439000 for each war map object selected (0x8088): its units into the player's selection group (0x4480F0,
        // out of their own), which is then the group the orders go to; that group stays theirs once deselected
        void SelectRefs(const std::vector<int>& refs);
        void RecomputeGroup(int group);                     // 0x800F -> 0x4802E0 when its members change
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
        // The Zoomer (0x446690 / 0x446850): the view glides from where it is to a point, gaining the speed each
        // update for the first half of the way and losing it for the rest, done once its step is 0 or less
        struct Zoomer { bool active = false; float x = 0, y = 0, dx = 0, dy = 0, half = 0, step = 0, speed = 0; };
        Zoomer zoomer;
        void ZoomTo(int x, int y);
        void ZoomTick();
        // ',' and '.' (0x43BE50 / 0x43BF30): the player's war map unit objects in the battle in turn by id
        int StepObject(bool up);
        int lastStepped = 0;            // dword_4F8740: the id last stepped to
        void ViewOnGroup(int group, bool glide);
        void DrawSelection(Canvas& canvas) const;
        int scrollX = 0, scrollY = 0;
        int mouseX = -1, mouseY = -1;
        int ticks = 0;
        // The fog of war (WFogOfWar at the world's +0x84): the [BattleMap] keys, the FogMap and the stamps
        struct FogOfWar
        {
            bool enabled = true;        // +0x8C: "EnableFogOfWar" (1), and byte_4BAD38 (the datsun cheat clears it)
            float darkness = 0.7f;      // +0x90: "Fog darkness"
            int returnRate = 5;         // +0x94: "Fog return rate"
            int updateFreq = 15;        // +0x98: "Fog update freq"
            int smooth = 128;           // +0x9C: "Fog smooth index"
            int dropoff = 3;            // +0xA0: "Fog vert dropoff"
            int damageDelta = 0;        // +0xA4: "Fog damage delta" (read, not used)
            int damageRadius = 16;      // +0xA8: "Fog vert damage radius" (the redraw rectangle's margin, 0x434BD0)
            std::vector<uint8_t> map;   // +0x18, a byte a vertex
            std::array<std::vector<uint8_t>, 20> stamps; // +0x20 + 4i
        } fog;
        void FogStart();                                // 0x4346C0, 0x434DF0
        void FogDecay();                                // 0x434DF0
        void FogReveal(int vx, int vy, int range);      // 0x434A80
        float FogLevel(const Unit& u);                  // 0x48B050
        float FogLevelAt(int type, float x, float y) const;   // 0x48B050 at a point, by its type's +0x44 (+0xAC)
        void FogTick();
        void FogRevealOwn();                            // each of the player's objects' vt+0x100 (0x4964B0 / 0x48C570)
        std::set<int> encountered;      // byte_51A19C by type: 0x47F630, 0x488C00
        int rightDownAt = -1;
        // an item's row and place in it (0x432D70 / 0x432E90: the row whose first item has its command, +0x28)
        struct PopupItem { int command, icon, text; Rect rect; int param = 0; int row = 0, col = 0; };
        std::vector<PopupItem> popup;       // "Battlemap menu" (WPopupMenu 0x432C60, 0x485DE0)
        int popupHover = -1;
        int popupOpenRow = -1;              // the row opened out (0x433330 with the hovered item's command)
        bool PopupShown(size_t i) const { return popup[i].col == 0 || popup[i].row == popupOpenRow; }
        void LayOutPopup(int x, int y);
        void OpenPopup(int x, int y);
        void ClosePopup(bool escape);
        // The selection's items (0x805C: the selection 0x4482D0 asks its first member, 0x47DF20 -> 0x47E1E0)
        void AddOrders(const Unit& first);
        bool CanDo(int type, int command) const;     // 0x47DA80
        static bool Armed(const ObjOjd::UnitData& unit); // +0x3C: children with +0xB8 bit 11
        std::vector<int> groupStance;                // each group's +0x54 (WBMGroup 0x480190: 1)
        void EdgeScroll();
        // The battle's end (0x43A980, the battle world's update): a side with nothing left puts up the InfoBox
        // (0x432520) with its text and the command its button sends (0x76 back to the war map, 0x8024 just closed)
        struct EndBox { std::string text; int command = 0; };
        std::optional<EndBox> endBox;
        bool battleOn = false, battleDecided = false;   // the report's +0x54; dword_4F7450 (a box was put up)
        void CheckBattleEnd();
        Rect EndBoxRect() const;
        Rect EndBoxButtonRect() const;
        std::optional<Command> EndBoxCommand();
        // The battle AI: the computer player's part in a battle (its object 0x442890: the battle begun 0x8075 ->
        // 0x445210, each update 0x4455D0 -> the planner 0x4456B0, its tasks 0x445010, objects gone 0x807A ->
        // 0x445B90). A threat (+0x47C, 16) is a war map object (here its group) with an active flag; the AI's own
        // objects (+0x540 list, +0x544 their count) wait for a task; a task (+0x4BC, 32; +0x478 their count) puts
        // one of them on a threat (none: Roam and Attack)
        struct AiThreat { int group = -1; bool active = true; };
        struct AiTask { int threat = -1, own = -1; };
        bool aiOn = false, aiHolder = false;                    // +0x450: the AI holds the sector
        std::vector<AiThreat> aiThreats;
        std::vector<int> aiOwn;
        std::vector<AiTask> aiTasks;
        void AiStart();                                         // 0x445210
        void AiUpdate();                                        // 0x4455D0
        void AiPlan();                                          // 0x4456B0
        void AiRunTask(const AiTask& t);                        // 0x445010
        void AiObjectGone(int group);                           // 0x445B90
        void AiRegister(int group);                            // 0x445500
        int AiValue(int group) const;                           // 0x441E70
        // Each group's periodic stance work (0x483B70: +0x58 counting to +0x5C, 90): Roam and Attack (3) wanders to a
        // random point; Attack and Retaliate (2, 1) go back home (+0x60 / +0x64) if more than 400 from it
        std::vector<int> groupTicks, groupHomeX, groupHomeY;
        std::vector<int> groupInterval;                         // +0x5C, the think interval (the members' +0x92 averaged)
        std::vector<bool> groupHalted;
        // +0x1D8 bit 3: an order of the player's (packets 0x0E, 0x10 and 0x13; 0x481E4E, 0x4814D8, 0x48167F), which
        // with bit 4 (a player's group) lets the move show its flag and speak (0x483590), cleared once it has
        std::vector<bool> groupOrdered;
        std::vector<int> groupArrived;                          // +0x56: members arrived (0x482450)
        int GroupCount(int group) const;                        // 0x4216D0: the group list's count (+0x24)
        bool PlayersOrder(int group) const;                     // bits 3 and 4, and the first member the player's                          // +0x1D8 bit 1: most of it halted (0x483760)
        std::vector<bool> groupBusy;                            // +0x1D8 bit 0 without a target: moving (an order, Avoid's run, Retreat)
        std::vector<bool> groupSelfMove;                        // +0x1D8 bit 2: that move is the group's own (0x483B70's roam / home)
        // 0x80F4 posted to an object (0x4355A0 with its delay): the update it arrives, the object, the firer, the level
        struct ControlPost { int at = 0, unit = 0, attacker = 0, level = 0; };
        std::vector<ControlPost> controlPosts;
        int controlLevel = 0;
        bool controlByPlayer = false;
        void ControlArrives(const ControlPost& p);              // 0x495CB0's case 0x80F4
        void GroupTick(int group);
        void GroupGone(int group);                              // its last unit gone: the AI told
    public:
        // The war map objects whose battle units are all gone (each group stands for one), for the war map
        std::vector<int> LostRefs() const;
        // The war map objects that left by a reinforcement point, and the sector each went to
        std::vector<std::pair<int, int>> RetreatedRefs() const;
        bool Fought() const { return battleOn; }
        bool Forfeited() const { return forfeit; }
        void CheatRemoveSide(bool own);                         // 0x806C / 0x806D (0x43B15C)
        // the unit table's +0xAC (byte_51A19C), the types met in this battle, for the research triggers
        const std::set<int>& Encountered() const { return encountered; }
        // the scorch as the battle map closes (0x439FE2 -> 0x4050B0: packed into the sector's Damage Layer)
        const std::vector<uint8_t>& Scorch() const { return land.Scorched(); }
        int ScorchWidth() const { return land.Width(); }
        int ScorchHeight() const { return land.Height(); }               // byte_4E9B94 (0x48ACEF)
        // 0x80F4 taken by the other side's key building (0x495CB0 -> the sector, 0x43B920): the level (+0x128, 1-3;
        // 0 for none) and whether the player's side fired it (dword_4E9B60)
        int ControlLevel() const { return controlLevel; }
        bool ControlByPlayer() const { return controlByPlayer; }
    };
}
