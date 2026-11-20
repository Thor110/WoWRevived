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
//          f32 progress, n path sectors, u8, and its travelling objects as a WMOB list        0x41FEC0, 0x425A60
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
        void SetSector(int sector);
        bool Kind2() const { return list >= 2; } // a construction or resource (two floats before SELO)
        // every SELO id in it (its own first, then its battle map units')
        std::vector<size_t> IdOffsets() const;
        size_t SeloAt() const { return Kind2() ? 12 : 4; }
    };

    struct SaveRoute
    {
        uint32_t origin = 0, destination = 0, flag5F = 0, flag5E = 1, field44 = 5;
        float progress = 0;
        std::vector<uint32_t> path;
        uint8_t field50 = 0;
        std::vector<SaveObject> objects;
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
