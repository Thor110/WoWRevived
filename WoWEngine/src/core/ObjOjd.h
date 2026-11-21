// OBJ.ojd: the game's object database, read by the vanilla exe at 0x437750.
//
// The file is a stream of records: 3 header bytes (not used by the loader), an int16 record type, then a payload
// whose size depends on the type (65 types, see RecordPayload in ObjOjd.cpp). Most types fill in unit, weapon and
// effect tables for the battle map; the ones read here build the graphics table ("GTypes") every sprite, font and
// palette is looked up in:
//   type 0  int16 id             current GType id
//   type 1  uint16 class         current GType class
//   type 2  uint16 len, name[len], uint16 palette slot   register a file for both sides
//   type 3  same as 2, Human games only;  type 4  same as 2, Martian games only
//   type 5  int16 x, int16 y     hotspot for the next registration
//   type 7  uint16 value         non-zero = load the next file straight away, 0 = when first used
// The hotspot and load flag reset after each registration.
#pragma once
#include <cstdint>
#include <array>
#include <map>
#include <vector>
#include <span>
#include <string>

namespace wow
{
    struct GType
    {
        uint16_t gclass = 0;
        uint16_t paletteSlot = 0;
        std::string file;         // game path, e.g. "dat\\bigmnbut.spr"
        bool loadOnUse = true;
        int16_t hotX = 0, hotY = 0;
    };

    class ObjOjd
    {
    public:
        // Throws FormatError if a record runs past the end of the data.
        static std::map<int, GType> LoadGTypes(std::span<const uint8_t> data, bool human);
        // Each unit type's frame in the war map counter sprites (HU-CNT24/32/48, GTypes 0x76-0x78): record type 15
        // starts a unit type, type 23 gives the frame.
        static std::map<int, int> LoadUnitCounters(std::span<const uint8_t> data);

        // The unit table (0x51A0F0, 0xB0 bytes a unit type) as far as it's used so far.
        struct UnitData
        {
            int counterFrame = 0; // +0x2C, record 23
            int moveClass = 0;    // +0x4E, record 36: which war map links it may take (0x42ABD0): 0-2 land,
                                  // 3 sea, 4 anything
            int field52 = 0;      // +0x52, record 37
            int warMapSpeed = 0;  // +0x54, record 37 (scaled by record 38): a route covers speed x 0.01 map pixels
                                  // a game hour (0x4257C0: hours = distance / (speed x 0.01), the slowest unit's)
            int field56 = 0;      // +0x56, record 37 (scaled by record 38)
            // what it takes of the side's three resources each game hour (0x51A150, record 45, scaled by 46): for
            // each of its crew, half of it while standing (0x429640)
            float upkeep[3] = { 0, 0, 0 };
            // more while it works (repairing, building; 0x51A15C, record 47, scaled by 48)
            float activity[3] = { 0, 0, 0 };
            int nameText = 0;     // +0x14, record 21: the TEXT.ojd string of its name (0x440720)
            int category = 0;     // +0x10, the class record 17 set before the type's first record 15: 1 vehicles,
                                  // 2 and up buildings (also the battle map's placement kind)
            int battleUnits = 1;  // +0x58, record 55: the battle map units each war map object stands for (1 until set)
            int faction = 2;      // +0x0C, record 26: 0 Human, 1 Martian, 2 neither (scenery)
            int researchDays = 0; // record 39: its research time in days; the table keeps it in hours (+0x08,
                                  // ResearchHours)
            int researchFaction = 2; // the faction as it was when record 39 came, which is what the hours use
            int baseType = 0;      // +0x00: the type itself (record 15), or the template's (record 18): boats are 19 / 22
            int upgrade = 0;       // +0x88, records 19 / 20: the type this one gives way to (0x45F900)
            int maxPerSector = 20; // +0x90, record 52: how many a sector can hold (0x43C7F0; 20 from record 15)
            int prerequisite = 0;  // +0x94, record 54: a building that must be finished in the sector first (0x442420)
            int builders[3] = { 0, 0, 0 }; // +0x78-0x80, records 49 / 50 / 51 (one, two or three): what builds it
            float buildDays = 0;     // record 43: its manufacture time in days (+0x5C after the modifier)
            int buildFaction = 2;    // the faction as it was when record 43 came
            // The rest of the 0xB0-byte entry (0x437750), for the battle map; defaults from 0x4377A3
            std::string code;        // +0x30, record 16: the last four characters of its data (the WSelectableObject name)
            int infoPage = 0;        // +0x18, record 22: its information page (0x4403A0)
            int field28 = 0;         // +0x28, record 24
            float scale = 1.0f;      // +0xA8, record 25 (which also sets the scale the records after it use, type or not)
            struct Part { int a = 0; float f = 0; int b = 0, c = 0, d = 0; int box[4] = { -1, 0, 0, 0 }; };
            std::vector<Part> parts; // +0x38, record 28 ("Part____", each put in front; the box from a record 27 just
                                     // before, else -1, 0, 0, 0; f times the scale); +0x42 their count
            struct Emitter { int a = 0, b = 0, c = 0; float f[4] = { 0, 0, 0, 0 }; };
            std::vector<Emitter> emitters; // +0x3C, record 29 ("Emitter", each put in front)
            int hitPoints = 0;       // +0x44, record 30: hp x the scale (record 31 sets +0x46 alone)
            int currentHitPoints = 0;// +0x46
            int size40 = 0x10, size41 = 0; // +0x40 / +0x41 bytes, record 30's other two, each x the scale
            int field43 = 1;         // +0x43, record 56
            int visionRaw = 0;       // record 32; +0x48 is it x [Tweak] "VisRadMul" (1.5), see Vision
            int field50 = 0;         // +0x50, record 33
            int armourRaw = 100;     // record 34 (100 if none); +0x4A is it x the armour modifier, see Armour
            int armourFaction = -1;  // the faction when record 34 came (-1: none came, +0x4A stays 100)
            std::vector<float> armourMuls; // record 35, each multiplying +0x4A again
            int field4C = 0;         // +0x4C, record 57
            int field92 = 90;        // +0x92, record 53
            int field98 = 0x250;     // +0x98, record 60
            int fieldA0 = 0;         // +0xA0, record 61 (stored less 1): the selection voice class (0x439000)
            int fieldA1 = 0xC0;      // +0xA1, record 62
            int fieldA2 = 0, fieldA4 = 0; // +0xA2 / +0xA4, records 58 / 59
            int researchName = 0;    // +0x1C, record 40: its name on the research screen
            int researchingText = 0; // +0x20, record 41: "Researching ..." while it isn't done
            int researchedText = 0;  // +0x24, record 42: what it says once done
        };
        static std::map<int, UnitData> LoadUnits(std::span<const uint8_t> data);
        // The research pages' table (0x5888C0, 12 bytes a page, records 63 and 64: the page, then three words): the
        // page's name, the GType of its picture (the blueprint, 0x45F4B0) and the text its scientist speaks
        struct ResearchPageInfo { int name = 0, picture = 0, speech = 0; };
        static std::map<int, ResearchPageInfo> LoadResearchPages(std::span<const uint8_t> data);
        // The research time in hours (+0x08, 0x437750 case 39): days x 24 x the [Tweak] "<HumanFriend | HumanEnemy |
        // MartianFriend | MartianEnemy> Research Modifier" (0.75 each) by the type's faction and the side being played
        // (Friend when they're the same), 1 for scenery; truncated, kept as a 16-bit word
        struct ResearchModifiers { float humanFriend = 0.75f, humanEnemy = 0.75f, martianFriend = 0.75f, martianEnemy = 0.75f; };
        static int ResearchHours(const UnitData& unit, bool playingHuman, const ResearchModifiers& m);
        // The manufacture time in hours (0x461B60 / 0x461E60): +0x5C (record 43's days x [Tweak] "<HumanFriend |
        // HumanEnemy | MartianFriend | MartianEnemy> Manufacture Modifier", 1.0 / 0.85 / 1.0 / 0.85, by its faction
        // then and the side being played, 0x438529) x 24, times its battle units for a vehicle (class 1), truncated
        // [Tweak] "VisRadMul" (1.5) and the armour modifiers: "Scenery Armour Modifier" (1.2) for faction 2, else
        // by faction and the side played as the research ones (all 1.8) (0x4373D0-0x437510)
        struct ArmourModifiers { float visRadMul = 1.5f, scenery = 1.2f, humanFriend = 1.8f, humanEnemy = 1.8f, martianFriend = 1.8f, martianEnemy = 1.8f; };
        static int Armour(const UnitData& unit, bool playingHuman, const ArmourModifiers& m); // +0x4A
        static int Vision(const UnitData& unit, const ArmourModifiers& m);                    // +0x48
        // The loader's tables kept by graphics id (the current record 0) and by unit type, for the battle map
        struct BattleTables
        {
            struct Six { float a = 0, b = 0; int c = 0, d = 0; };
            std::map<int, Six> six;                          // record 6, 0x4F0530 + id x 12 (read by 0x4802E0)
            std::map<int, std::array<int, 17>> ten;          // record 10, 0x4F6E38 + id x 0x44
            std::map<int, std::pair<int, float>> eleven;     // record 11, 0x4F04C0 / 0x4F0510 by id
            std::map<int, std::array<int, 8>> twelve;        // record 12, 0x4F2F10 + type x 16 (read by 0x494CB0)
            struct Thirteen { float f = 0; int a = 0, b = 0, c = 0; };
            std::map<int, Thirteen> thirteen;                // record 13, 0x4EC598 + type x 16 (read by 0x48DDC0)
            // Record 9 (0x437866): the lighting for each hour of the day, 64 bytes each at 0x4DA828. The sun's angle
            // in degrees, then three named sets of four floats; the first set lights the landscape (0x40B030) and
            // the first letter of its name picks the shade table file (0x40B570).
            struct DayTime
            {
                int angle = 0;
                struct Set { std::string name; float f[4] = {}; };
                Set sets[3];
            };
            std::map<int, DayTime> dayTimes;
        };
        static BattleTables LoadBattleTables(std::span<const uint8_t> data);
        struct ManufactureModifiers { float humanFriend = 1.0f, humanEnemy = 0.85f, martianFriend = 1.0f, martianEnemy = 0.85f; };
        static float BuildTable(const UnitData& unit, bool playingHuman, const ManufactureModifiers& m); // +0x5C
        static int BuildHours(const UnitData& unit, bool playingHuman, const ManufactureModifiers& m);

        // The war map's route arrow styles (RTypeArrow, a table of 24 bytes each at 0x59EDB8), from record 14
        // (0x437C10): the index, then the sprite's GType and four more words, and a float
        struct ArrowType { int gtype = 0; int a = 0, b = 0, c = 0, d = 0; float f = 0; };
        static std::map<int, ArrowType> LoadArrowTypes(std::span<const uint8_t> data);

        // Each sector's resource deposits, one for each of the six kinds (Human iron, coal, oil; Martian blood,
        // copper, heavy elements), from record 8 (0x437750 into the sector table at 0x4EAE1C, +0x04).
        static std::map<int, std::array<int, 6>> LoadSectorDeposits(std::span<const uint8_t> data);
        // Each sector's battle map, the name record 8 puts at the sector table's +0xAC (0x437B0A, 0x4EAEC8 + sector
        // x 0xC0): WLandscape (0x47B900) loads maps\<name>.cls (or .ils) and .atm
        static std::map<int, std::string> LoadSectorMaps(std::span<const uint8_t> data);
    };
}
