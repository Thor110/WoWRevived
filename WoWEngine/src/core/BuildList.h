// A war map object's factory (WBuildList 0x45F770-0x462EE0): the queue of what it's making, as an object that can
// build (its type among some type's builders, OBJ.ojd records 49-51) has one (0x429DB0).
//
// Saved at the end of the object's WMOB part (0x42A220): after its battle map units (the last one's BATP, or for a
// vehicle VEHI), u32 has-a-factory, then the factory (0x460900): u32 the queue's length and, if it isn't 0, u8 busy
// and each entry (0x461F00): u32 type, u32 target object (0xDEADDEAD for none), u32 x, y, x2, y2 (where it goes,
// for a construction), u32 repeats (1-3, 4 forever), f32 hours left; then u32 the object's +0x3C. A vehicle's VEHU
// comes after that.
#pragma once
#include "core/SaveFile.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace wow
{
    struct BuildEntry
    {
        int type = 0;                      // +0x18
        uint32_t target = 0xDEADDEAD;      // +0x14
        std::array<int32_t, 4> place{};    // +0x1C-0x28
        int repeat = 1;                    // +0x30: 1-3, 4 for ever
        float hours = 0;                   // +0x34
    };

    struct BuildQueue
    {
        bool busy = false;                 // +0x12C
        std::vector<BuildEntry> entries;   // +0xA8, at most 32
    };

    struct ObjectTail
    {
        size_t at = 0, end = 0;            // where it is in the object's bytes
        bool hasFactory = false;
        BuildQueue queue;
        uint32_t last = 0;                 // the object's +0x3C
    };

    // Found and read, or nothing if the object's bytes don't end the way that's expected.
    std::optional<ObjectTail> ReadTail(const SaveObject& object);
    // Written back over the old one.
    void WriteTail(SaveObject& object, const ObjectTail& tail);
}
