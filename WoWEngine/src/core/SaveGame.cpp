#include "core/SaveGame.h"

#include <cstdio>
#include <fstream>

namespace wow
{
    std::string SaveSlot::GamePath(bool human, int slot)
    {
        char path[48];
        std::snprintf(path, sizeof(path), "SaveGame\\%s.%03d", human ? "Human" : "Martian", slot);
        return path;
    }

    bool SaveSlot::Exists(const GameFiles& files, bool human, int slot) { return files.Resolve(GamePath(human, slot)).has_value(); }

    std::optional<std::string> SaveSlot::Name(const GameFiles& files, bool human, int slot)
    {
        auto path = files.Resolve(GamePath(human, slot));
        if (!path) return std::nullopt;
        std::ifstream in(*path, std::ios::binary);
        char raw[NameLength] = {};
        in.seekg(std::streamoff(NameOffset));
        if (!in.read(raw, std::streamsize(NameLength))) return std::nullopt;
        size_t length = 0;
        while (length < NameLength && raw[length] != 0) length++;
        return std::string(raw, length);
    }
}
