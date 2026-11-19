// Saved campaigns: SaveGame\Human.001-.005 or SaveGame\Martian.001-.005 (vanilla 0x404C60).
// The file is a list of chunks (GAME, WMAP, CALN, DtTm, HRSH, MRSH, MPAD... see WoWViewer/SaveEditorForm.cs); the
// load and save menus only need the name, 36 bytes of Latin-1 at 0x0C (read by 0x404CD0).
#pragma once
#include "core/GameFiles.h"

#include <optional>
#include <string>

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
}
