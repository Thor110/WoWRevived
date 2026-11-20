#include "core/SaveGame.h"
#include "core/Bytes.h"

#include <algorithm>
#include <cstring>

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

    SaveInfo SaveInfo::Read(std::span<const uint8_t> d)
    {
        size_t pos = 0;
        auto need = [&](size_t n) { if (pos + n > d.size()) throw FormatError("save: ends early"); };
        auto u32 = [&]() { need(4); uint32_t v = uint32_t(d[pos]) | (uint32_t(d[pos + 1]) << 8) | (uint32_t(d[pos + 2]) << 16) | (uint32_t(d[pos + 3]) << 24); pos += 4; return v; };
        auto u16 = [&]() { need(2); uint16_t v = uint16_t(d[pos] | (d[pos + 1] << 8)); pos += 2; return v; };
        auto tag = [&](const char* t) {
            need(4);
            if (std::memcmp(&d[pos], t, 4) != 0) throw FormatError(std::string("save: expected ") + t);
            pos += 4;
        };
        SaveInfo info;
        info.version = u32();
        tag("GAME");
        uint32_t length = u32();
        need(length);
        size_t n = 0;
        while (n < std::min<size_t>(length, SaveSlot::NameLength) && d[pos + n]) n++;
        info.name.assign(reinterpret_cast<const char*>(&d[pos]), n);
        pos += length;
        tag("WMAP");
        for (uint32_t& w : info.warMap) w = u32();
        tag("CALN");
        uint32_t bits = u32();
        std::memcpy(&info.ticks, &bits, 4);
        tag("DtTm");
        for (uint16_t& v : info.dateTime) v = u16();
        return info;
    }

    std::vector<SaveInfo::Tag> SaveInfo::Tags(std::span<const uint8_t> d)
    {
        static const char* const Names[] = { "GAME", "WMAP", "CALN", "DtTm", "HRSH", "MRSH", "MPAD", "SECT", "SELO", "BMOL", "BAMO",
                                             "BATP", "WMOB", "VEHI", "VEHU", "AUNI", "HUNI", "HCON", "ACON", "HRES", "ARES", "DMGL" };
        std::vector<Tag> tags;
        for (size_t i = 0; i + 4 <= d.size(); i++)
            for (const char* n : Names)
                if (std::memcmp(&d[i], n, 4) == 0) { tags.push_back({ i, n }); i += 3; break; }
        return tags;
    }
}
