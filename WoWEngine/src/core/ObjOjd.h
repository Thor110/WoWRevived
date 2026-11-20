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
    };
}
