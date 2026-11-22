// Saved campaigns: SaveGame\Human.001-.005 or SaveGame\Martian.001-.005 (vanilla 0x404C60).
// The file is a list of chunks (GAME, WMAP, CALN, DtTm, HRSH, MRSH, MPAD... see WoWViewer/SaveEditorForm.cs); the
// load and save menus only need the name, 36 bytes of Latin-1 at 0x0C (read by 0x404CD0).
#pragma once
#include "core/GameFiles.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace wow
{
    struct SaveSlot
    {
        static constexpr int First = 1, Last = 5;
        // MODERN (docs/MODERN.md "More save slots"): the engine's menus turn pages of five, up to the last number
        // the file name's three digits hold
        static constexpr int Max = 999, PerPage = 5;
        static constexpr size_t NameOffset = 0x0C, NameLength = 36;

        static std::string GamePath(bool human, int slot); // "SaveGame\Human.001"
        // The save's name, or nothing if the slot is empty or the file can't be read.
        static std::optional<std::string> Name(const GameFiles& files, bool human, int slot);
        static bool Exists(const GameFiles& files, bool human, int slot);
    };

    // The start of a save, which the game writes as a stream of objects (each object's writer puts its tag, then
    // u32s, u16s and length-prefixed byte blocks, then its children's): a u32 version (5), then
    //   GAME  u32 0x28 and 40 bytes: the name (36 bytes of Latin-1) and 4 more            writer 0x44E8A0
    //   WMAP  4 u32s: the view's top-left at 640 x 480, its sector, 1 (Human.000: 803, 1805, 30, 1), then
    //         its children:                                                                writer 0x41AD80
    //   CALN  f32 game time in hours (the calendar's +0x28, 0x41F6C0)                      writer 0x41F760
    //   DtTm  6 u16s: hours elapsed, minute, hour, day and month (both from 0), year       writer 0x41B930
    // and then the research (HRSH 0x45A190, MRSH 0x45B2F0), the message pad (MPAD 0x420FD0), the damage map (DMGL
    // 0x405100) and the 30 sectors (SECT 0x4465B0, each with HUNI / AUNI / HCON / ACON / HRES / ARES lists of WMOB
    // war map objects 0x421B80, and battle map objects BMOL 0x47D360, BAMO 0x489F30, VEHI 0x490A40, BATP 0x495710,
    // VEHU 0x4A3C80, SELO 0x440550). core/SaveFile reads and writes the whole save exactly; Tags() and
    // WarMapObjects() here find things by searching for their names, the fallback for a save SaveFile can't read.
    // Human.000 and Martian.000 in the game folder are the new campaigns the game starts from (0x4043D0, slot 0).
    struct SaveInfo
    {
        uint32_t version = 0;
        std::string name;
        uint32_t warMap[4] = {};
        float ticks = 0;
        uint16_t dateTime[6] = {};
        int Day() const { return dateTime[3] + 1; }
        int Month() const { return dateTime[4] + 1; }
        int Year() const { return dateTime[5]; }
        int Hour() const { return dateTime[2]; }
        int Minute() const { return dateTime[1]; }

        // Throws FormatError if the start isn't a save's.
        static SaveInfo Read(std::span<const uint8_t> data);

        struct Tag { size_t offset; std::string name; };
        static std::vector<Tag> Tags(std::span<const uint8_t> data);

        // The war map's objects in the 30 sectors (SECT, 0x4465B0): each sector has six lists, each written as
        // its tag (HUNI, AUNI human / alien units; HCON, ACON constructions; HRES, ARES resources), WMOB and a
        // count, then the objects: a u32 unit type (constructions have two floats after it), then "SELO" with
        // an id and the object's x, y on the war map, then "WMOB" and its sector, then its battle map units
        // (BMOL...). Found by that SELO ... WMOB pattern, which the battle map's own objects don't have.
        struct WarMapObject
        {
            // 0 HUNI, 1 AUNI, 2 HCON, 3 ACON, 4 HRES, 5 ARES. The H lists are the side being played (in
            // Martian.000 the Martians are in HUNI), the A lists the other side's. Only the first object of a
            // kind in a list has a position; the rest are at 0, 0 and show as the count on its counter.
            // Resources are laid out like constructions and have no positions of their own.
            int list = 0;
            int unitType = 0;  // OBJ.ojd's unit type
            int id = 0;
            int x = 0, y = 0;  // on the war map
            int sector = 0;
            float value[2] = { 0, 0 }; // constructions and resources: the two floats after the type
            int ref = -1;      // the object in the war map's table of saved objects (-1: read without one)
            bool PlayersSide() const { return list % 2 == 0; }
        };
        static std::vector<WarMapObject> WarMapObjects(std::span<const uint8_t> data);

        // Who holds each sector, [1]-[30]: 0 Human, 1 Martian, 2 no one. The sector object's writer (0x43C5E0)
        // puts it straight after SECT's six lists, before the sector's damage map (DMGL); the game keeps it in
        // the sector table at 0x4EAE1C (0xC0 bytes a sector) and reads it with 0x43CA30.
        static std::vector<int> SectorOwners(std::span<const uint8_t> data);
    };
}
