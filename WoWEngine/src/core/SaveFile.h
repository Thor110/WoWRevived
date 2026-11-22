// A whole campaign save, read and written exactly as the game does (WWarMapWorld's serialiser 0x41AD80 and its
// children), so a save the engine writes is the game's own and loads in either.
//
// The stream (WSaveFile, 0x44E7D0-0x44ED20) is u32s, u16s, u8s, length-prefixed blocks and tags; objects don't say
// how long they are, so what the war map owns is read field by field and everything else is carried along as it
// was: the research (HRSH, MRSH), each sector's economy and damage map, and each object's battle map part. The order:
//
//   u32 version (the [Debug] "Save File ID", 5)
//   GAME   a 0x28-byte block: the campaign's name (36 bytes of Latin-1) and 4 more            0x44E8A0
//   WMAP   u32 view x, view y (the top-left at 640 x 480), selected sector, view mode         0x41AD80
//   CALN   f32 game hours; DtTm 6 u16s: hours elapsed, minute, hour, day, month, year        0x41F760, 0x41B930
//   HRSH, MRSH   both sides' research (kept as it is)                                         0x45A190, 0x45B2F0
//   MPAD   u32 count, then each line: u32 length and the text with its NUL, u32 life,
//          u32 state, u32 the game hour it came                                               0x420FD0, 0x428B10
//   u32 routes, then each route: u32 origin, destination, two flags, 5, path length n,
//          f32 progress, n path sectors, u8, u32 units, and each unit: its object, then its
//          route entry's eight values (SaveRouteEntry)                              0x41FEC0, 0x425A60, 0x427F70
//   u32 pod hours, u32 pods                                                                   0x41A330
//   sector 0: u32 owner, DMGL and the rest (no SECT, no lists)
//   sectors 1-30: SECT, then six lists, HUNI AUNI HCON ACON HRES ARES (the H lists the side being played), each
//          "WMOB" u32 count and the objects; then u32 owner, DMGL and the rest                0x43C5E0, 0x421B80
//
// An object (0x421B80 writes its type, then its own writer): u32 type, two f32s for a construction or a resource,
// SELO u32 id, x, y, WMOB u32 sector, then its battle map part (BMOL: its units, each with SELO ids of their own).
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace wow
{
    struct SaveObject
    {
        int list = 0;                  // 0 HUNI, 1 AUNI, 2 HCON, 3 ACON, 4 HRES, 5 ARES
        std::vector<uint8_t> bytes;    // the object as written, from its type on

        int Type() const;
        int Id() const;
        int X() const;
        int Y() const;
        int Sector() const;
        float Value(int i) const;      // constructions and resources: the two floats after the type
        void SetPosition(int x, int y);
        void SetValue(int i, float v);  // constructions and resources: the two floats after the type
        // A construction's or resource's place on the battle map (its battle object's +0x78 / +0x7C, read by the
        // AI's site matching 0x440F80): the two floats 16 and 12 bytes before its last BATP tag. False if it has none.
        bool BattlePosition(float& x, float& y) const;
        // Every battle object's place (+0x78 / +0x7C: the second and third floats after each BAMO tag), in order
        std::vector<std::pair<float, float>> BattlePositions() const;
        // Every battle object's health (+0xC4), in order. A battle object writes "BAMO" and then its heading
        // (+0x40), its place (+0x78, +0x7C, +0x84) and its health (0x489F30): the health is the dword 20 bytes on
        // from the tag.
        std::vector<int> BattleHealths() const;
        void SetBattleHealth(size_t unit, int health);
        // The type changed where it's written: at the start, and before each of its battle objects' SELO tags
        // (the rest, its hit points among them, stay the copied object's)
        void SetType(int type);
        void SetBattlePosition(float x, float y);
        void SetSector(int sector);
        bool Kind2() const { return list >= 2; } // a construction or resource (two floats before SELO)
        // every SELO id in it (its own first, then its battle map units')
        std::vector<size_t> IdOffsets() const;
        size_t SeloAt() const { return Kind2() ? 12 : 4; }
    };

    // A route's entry (RouteEntry 0x42AFE0), as its save writes it after its object (0x42B250): eight values, 32
    // bytes. The entry's +0x10 / +0x14 (its place in the formation) are worked out again from the index (0x42B1F8).
    struct SaveRouteEntry
    {
        int32_t segment = 0;        // +0x24: the path index it last reached
        int32_t index = 0;          // +0x18: which of the route's units it is (its formation place)
        float speed = 0;            // +0x2C: map pixels a game hour (its type's +0x54 x 0.05)
        float remaining = 0;        // +0x30: the way still to go
        float hoursLeft = 0;        // +0x34
        float segmentLeft = 0;      // +0x38: of the way to the next sector's waypoint
        float segmentLength = 0;    // +0x3C
        uint32_t started = 1;       // +0x28 (a byte, written as a dword)
    };

    struct SaveRoute
    {
        uint32_t origin = 0, destination = 0, flag5F = 0, flag5E = 1, field44 = 5;
        float progress = 0;         // the route's +0x34 (*provisional*: read as an int by 0x42586A; 0 in every save seen)
        std::vector<uint32_t> path;
        uint8_t field50 = 0;
        std::vector<SaveObject> objects;
        // one for each object, or none: a save written before update 137 has the objects alone
        std::vector<SaveRouteEntry> entries;
    };

    struct SaveMessage
    {
        std::string text;              // without the NUL
        uint32_t life = 0, state = 0, hour = 0;
    };

    struct SaveSector
    {
        std::array<std::vector<SaveObject>, 6> lists;
        std::vector<uint8_t> rest;     // u32 owner, DMGL and the sector's own data
        int Owner() const;
        void SetOwner(int owner);
        // The sector's Damage Layer (DMGL after the owner, 0x405100 / 0x4051B0): "DMGL", u32 width, height, the
        // packed size; unless both are 0, u32 0x410 and the FFUH header (0x415720), u32 the packed size and the
        // packed bytes. False if it has none (an empty grid) or doesn't read.
        bool Damage(int& width, int& height, std::vector<uint8_t>& grid) const;
        void SetDamage(int width, int height, const std::vector<uint8_t>& grid);
    };

    struct SaveFile
    {
        uint32_t version = 5;
        std::vector<uint8_t> game;     // the GAME block, 0x28 bytes
        uint32_t warMap[4] = {};       // view x, view y, selected sector, view mode
        float hours = 0;
        uint16_t dateTime[6] = {};
        std::vector<uint8_t> research; // HRSH and MRSH as they were
        std::vector<SaveMessage> messages;
        std::vector<SaveRoute> routes;
        uint32_t podHours = 0, pods = 0;
        SaveSector sector0;            // its lists are always empty
        std::array<SaveSector, 30> sectors;

        // Throws FormatError when the data isn't a save this can read exactly.
        static SaveFile Parse(std::span<const uint8_t> data);
        std::vector<uint8_t> Write() const;

        std::string Name() const;
        void SetName(const std::string& latin1);
        SaveSector& Sector(int s) { return s == 0 ? sector0 : sectors[size_t(s - 1)]; }
        const SaveSector& Sector(int s) const { return s == 0 ? sector0 : sectors[size_t(s - 1)]; }

        // Object ids (0x4A....): 32768 of them, the lowest free one first.
        std::vector<bool> UsedIds() const;
        static void MarkIds(const std::vector<uint8_t>& bytes, std::vector<bool>& used);
        static int TakeId(std::vector<bool>& used);
    };
}
