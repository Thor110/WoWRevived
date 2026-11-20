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
            int researchName = 0;    // +0x1C, record 40: its name on the research screen
            int researchingText = 0; // +0x20, record 41: "Researching ..." while it isn't done
            int researchedText = 0;  // +0x24, record 42: what it says once done
        };
        static std::map<int, UnitData> LoadUnits(std::span<const uint8_t> data);
        // The research time in hours (+0x08, 0x437750 case 39): days x 24 x the [Tweak] "<HumanFriend | HumanEnemy |
        // MartianFriend | MartianEnemy> Research Modifier" (0.75 each) by the type's faction and the side being played
        // (Friend when they're the same), 1 for scenery; truncated, kept as a 16-bit word
        struct ResearchModifiers { float humanFriend = 0.75f, humanEnemy = 0.75f, martianFriend = 0.75f, martianEnemy = 0.75f; };
        static int ResearchHours(const UnitData& unit, bool playingHuman, const ResearchModifiers& m);

        // The war map's route arrow styles (RTypeArrow, a table of 24 bytes each at 0x59EDB8), from record 14
        // (0x437C10): the index, then the sprite's GType and four more words, and a float
        struct ArrowType { int gtype = 0; int a = 0, b = 0, c = 0, d = 0; float f = 0; };
        static std::map<int, ArrowType> LoadArrowTypes(std::span<const uint8_t> data);

        // Each sector's resource deposits, one for each of the six kinds (Human iron, coal, oil; Martian blood,
        // copper, heavy elements), from record 8 (0x437750 into the sector table at 0x4EAE1C, +0x04).
        static std::map<int, std::array<int, 6>> LoadSectorDeposits(std::span<const uint8_t> data);
    };
}
