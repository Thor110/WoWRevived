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
        static constexpr size_t NameOffset = 0x0C, NameLength = 36;

        static std::string GamePath(bool human, int slot); // "SaveGame\Human.001"
        // The save's name, or nothing if the slot is empty or the file can't be read.
        static std::optional<std::string> Name(const GameFiles& files, bool human, int slot);
        static bool Exists(const GameFiles& files, bool human, int slot);
    };

    // The start of a save, which the game writes as a stream of objects (each object's writer puts its tag, then
    // u32s, u16s and length-prefixed byte blocks, then its children's): a u32 version (5), then
    //   GAME  u32 0x28 and 40 bytes: the name (36 bytes of Latin-1) and 4 more            writer 0x44E8A0
    //   WMAP  4 u32s (803, 1805, 30, 1 in the new campaign saves), then its children:      writer 0x41AD80
    //   CALN  f32 game time in ticks, 20.055 an hour                                       writer 0x41F760
    //   DtTm  6 u16s: 3 not worked out, then day and month (both from 0) and year          writer 0x41B930
    // and then the research (HRSH 0x45A190, MRSH 0x45B2F0), the message pad (MPAD 0x420FD0), the damage map (DMGL
    // 0x405100) and the 30 sectors (SECT 0x4465B0, each with HUNI / AUNI / HCON / ACON / HRES / ARES lists of WMOB
    // war map objects 0x421B80, and battle map objects BMOL 0x47D360, BAMO 0x489F30, VEHI 0x490A40, BATP 0x495710,
    // VEHU 0x4A3C80, SELO 0x440550). Those aren't read yet; Tags() finds them by searching for their names, which
    // is fine for looking at a save but not a substitute for reading it object by object.
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
        int Hour() const { return int(ticks / 20.055f) % 24; }
        int Minute() const { float h = ticks / 20.055f; return int((h - float(int(h))) * 60.0f); }

        // Throws FormatError if the start isn't a save's.
        static SaveInfo Read(std::span<const uint8_t> data);

        struct Tag { size_t offset; std::string name; };
        static std::vector<Tag> Tags(std::span<const uint8_t> data);
    };
}
