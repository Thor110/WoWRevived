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
            const std::map<int, ObjOjd::UnitData>* units = nullptr;
            const std::map<int, ObjOjd::BattleTables::Six>* six = nullptr;  // OBJ.ojd record 6 (the WOF GTypes)
            const std::map<int, ObjOjd::BattleTables::Thirteen>* lights = nullptr; // record 13 (the light parts)
            bool showLights = true;     // [Options] "Show lights" (byte_4B8808)
            bool showSmoke = true;      // [Options] "Show Smoke & particles" (byte_4B880C)
            std::vector<BattleGroup> groups; // the sector's war map objects (WarMap::BattleGroups)
            int ownerFaction = 2;       // who holds the sector (0x43CA30): 0 Human, 1 Martian, 2 no one
            // The sector's neighbours as reinforcement points' sources (0x43D150): each sector, whether the player
            // holds it (0x4EAE1C against the player's side) and whether its holder's command building is there
            // finished (0x43C990)
            struct Source { int sector = 0; bool players = false, hq = false; };
            std::vector<Source> sources;
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
            } vehicles;
            const std::map<int, std::array<int, 17>>* moveCosts = nullptr;      // OBJ.ojd record 10 (0x4F6E38)
            const std::map<int, std::pair<int, float>>* moveSlopes = nullptr;   // record 11 (0x4F04C0 / 0x4F0510)
            int zMapRes = 400;
            Sound* sound = nullptr;
            ObjOjd::ArmourModifiers armour;     // [Tweak] the armour modifiers (0x4373D0-0x437510)
            // a research item's state is 2 (0x45B890), by side and its place in that side's tree (the globals from
            // 0x588C40 Human, 0x588F20 Martian, four bytes each)
            std::function<bool(bool human, int index)> researchDone;
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
                            int unit = -1, depth = 0, effect = -1, prop = -1; };
        // A scenery object (WScenery 0x4895E0 / 0x489380 on the damageable 0x489E20): its type, point and height
        // (+0x84), its health (+0xC4, the type's +0x4A to start with, bit 7 set), a 0x80A0 on its way, its light
        struct Prop { int type = 0; float x = 0, y = 0, z = 0; int health = 0, dieAt = -1, light = -1; bool alive = true; };
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
            EffectKind kind = EffectKind::Animated;
            float x = 0, y = 0, lift = 0;           // +0x78 / +0x7C, +0x84 (its height over the ground)
            float vx = 0, vy = 0, vz = 0, g = 0;    // +0x88..+0x90, +0x94
            std::vector<float> acc;                 // its emitter list (+0xB4): each entry's fraction (+8) and count (+4)
            std::vector<int> emitted;
            bool emitDone = false;                  // the list freed
            int count = 0, size = 0, group = -1;    // a shell's updates left (+0xC2), impact size (+0xC0), firer (+0xC4)
            int depth = 0;                          // its view object's key over its y
            int born = -1;                          // the update it was made on
        };
        std::vector<Effect> effects;
        int SpawnEffect(int type, float x, float y, float lift = 0, float vx = 0, float vy = 0, float vz = 0, float g = 0);
        static EffectKind KindOf(int type);
        void UpdateEffects();                                   // each flying object's update
        void Emit(size_t effect);                               // 0x47F050
        void Smoulder(size_t unit);                             // 0x48AEF0 (from the unit's update 0x4967E0)
        void Puff(int type, int ownType, float x, float y, float lift, int radius);   // 0x48ADD0
        void AddLight(int type, int x, int y, int z);           // a light part made (0x48DDC0, 0x48E050, 0x48DE80)
        void UpdateLights();                                    // 0x48E100 for the fading ones
        void Speak(int id, int group);                          // 0x481770 with the group's voice (0x449120)
        std::vector<ViewObject> objects;    // sorted by key
        std::map<int, ShadeTable> slotTables;
        std::map<std::string, std::vector<uint8_t>> masks;   // the shadows' .raw masks
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
                     int turret = -1; bool hardpoint = false; int middle = 0, half = 0; };
        // A turret (0x489820 on the vehicle 0x490820: 0x2E6-0x313, 0x389-0x38C): its type, its part's offset, its turn
        // from its unit (+0x190), the point it's aiming at (+0xC8 / +0xCC, +0xE4 bit 1), its hardpoint and its turning
        // sound (+0xE8)
        struct Turret { int type = 0, ox = 0, oy = 0, oz = 0, turn = 0; bool aiming = false; int aimX = 0, aimY = 0;
                        bool hardpoint = false; int middle = 0, half = 0, sound = 0; };
        // A part drawn with its unit (a WOF of its own: a turret's, or another like the lorries' 324)
        struct Attached { WofInstance* wof = nullptr; int ox = 0, oy = 0, oz = 0, turret = -1; };
        struct Unit { int type; float x, y; int heading; bool players; int group = 0; bool vehicle = false; Drive drive;
                      WofInstance* wof = nullptr; int engineSound = 0;
                      int health = 0;       // +0xC4: the type's +0x4A (0x47DA20) to start with
                      std::vector<Gun> guns;         // its weapon children (and its turrets')
                      std::vector<Turret> turrets;
                      std::vector<Attached> attached;
                      int dieAt = -1;       // a 0x80A0 posted to itself (the update it arrives on)
                      int id = 0;           // +0x08, the object's id (kept by a group's target, +0x30)
                      int scanCount = 0;    // +0xDA, updates since its last look round (0x4967E0)
                      int statusTime = 0, status = 0; // +0xDC / +0xE0: a timed status (0x80F5 frozen, 0x80F6, 0x80F7)
                      bool Frozen() const { return statusTime != 0 && status == 0x80F5; }  // vt+0x108, 0x489860
                      bool Turned() const { return statusTime != 0 && status == 0x80F7; }  // vt+0xF8, 0x489880
                      int stance = 1;       // +0xD8: its type's +0x43 byte (record 56) to start with (0x495740)
                    };
        int nextId = 1;
        // The reinforcement points (0x43D150: dword_4F7458, 0x18 bytes each, dword_4F76E8 of them): the point, the
        // sector it leads to, the link kind (7, 5, 1), whether the player holds that sector; each with its marker
        struct ReinforcePoint { int x = 0, y = 0, sector = 0, kind = 0; bool owned = false; };
        std::vector<ReinforcePoint> rpoints;
        void SetUpReinforcements();                             // 0x43D150
        void Retreat(int group);                                // 0x480EF0
        int NearestPoint(int x, int y, int moveClass) const;    // 0x43D3F0
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
        bool forceFireNext = false;                             // the popup's Force Fire waiting for the click
        std::vector<int> groupMode, groupRange;
        std::array<int, 8> WeaponData(int type) const;          // OBJ.ojd record 12 (0x4F2F10)
        void SetUpGuns(Unit& u);
        void ForceFire(int x, int y);                           // packet 0x13 / mode 1 (0x4812B0, 0x481CA0)
        void StopAttacking(int group);                          // the target cleared, the guns 0x805A
        void AttackObject(int id);                              // packet 0x10 (0x4812B0 -> 0x481F70 -> 0x482880)
        void Approach(Unit& u, const Unit& target);             // vt+0xC4: 0x491100 -> 0x496300
        void LookRound(size_t unit);                            // 0x4967E0's count -> vt+0x104 (0x48F8A0, 0x496890)
        void AutoTarget(const Unit& u);                         // 0x496570 -> the group's 0x80B1 (0x482960)
        void ChaseTargets();                                    // 0x483B70 -> 0x4826D0
        void Aim(Unit& u, int x, int y, int z);                 // vt+0xC8: 0x491110 -> 0x496200 -> guns 0x494D20
        void GunTick(size_t unit, Gun& g);                      // 0x494DB0 and the class's update
        void TurretTick(Unit& u, Turret& t);                    // 0x489B40
        int GunHeading(const Unit& u, const Gun& g) const;      // its +0x40: its turret's or its unit's
        void AddPartRenderers(size_t unit);
        void PoseParts(Unit& u);
        void Impact(size_t unit, const Gun& g);                 // 0x4886A0's landing: 0x487F50
        void ImpactAt(int size, int x, int y, int z, int group); // 0x487F50: the effect by size and terrain
        void Launch(size_t unit, Gun& g);                       // 0x4889D0
        bool ShellLands(size_t effect);                         // 0x488F00 after the step: gone?
        struct Landing { int size, x, y, z, group; };
        std::vector<Landing> landings;                          // made after the flying objects' updates
        // Damage and death (0x48A5A0, 0x48AB20, 0x80A0 in 0x489FA0) and the explosions (WExplosion 0x488180)
        int MaxHealth(int type) const;                          // 0x47DA20: +0x4A with the armour modifiers
        void Damage(size_t unit, int amount);                   // vt+0xE0, 0x48AB20
        void Die(size_t unit);                                  // 0x80A0
        void Explode(int type, int x, int y, int z, int firer = -1);  // 0x488070 + 0x488350 / 0x488200 (firer: a unit's group)
        static bool IsExplosion(int type);
        static bool IsAreaBlast(int type);
        void AreaDetonate(int type, int x, int y, int radius, int power, int firer);   // 0x487B50
        void SetStatus(size_t unit, int status, int level);     // 0x80F5-0x80F7 (0x495CB0)
        void ChangeStatus(Unit& u, int status);                 // vt+0x90, 0x496760
        void StatusTick(Unit& u);                               // 0x4967E0's timer
        void SetStance(int group, int stance);                  // 0x80B2 to the group: it and each member (+0xD8)
        void TurnOnOwn(Unit& u);                                // 0x496100
        std::vector<Unit> units;
        int startX = 0, startY = 0;                          // dword_4F742C / 4F7434: the player's deployment centre
        void Deploy();
        void Place(std::vector<size_t> members, int cx, int cy);
        void AddRenderer(int type, float x, float y, int heading, int unit = -1);
        // Driving (Driving.cpp): the move order (0x482E60 straight there, 0x80C2), each update's steering and step
        // (0x4913B0, 0x492F40, 0x492DE0, 0x491870), halt (0x8031)
        struct MoveOrder { int members = 0, arrived = 0, nearest = 0, farthest = 0; };  // +0x56, +0x68 / +0x6C
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
        void OrderGroupMove(int group, const std::vector<size_t>& members, int x, int y, int radius = 4);
        int NearestPassable(int group, int vx, int vy, int& ox, int& oy) const;   // 0x482B00
        void Halt();
        void Drive(size_t index);
        void Steer(Unit& u);
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
        struct PopupItem { int command, icon, text; Rect rect; int param = 0; };
        std::vector<PopupItem> popup;       // "Battlemap menu" (WPopupMenu 0x432C60, 0x485DE0)
        int popupHover = -1;
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
    public:
        // The war map objects whose battle units are all gone (each group stands for one), for the war map
        std::vector<int> LostRefs() const;
        // The war map objects that left by a reinforcement point, and the sector each went to
        std::vector<std::pair<int, int>> RetreatedRefs() const;
        bool Fought() const { return battleOn; }
    };
}
