#include "core/SaveGame.h"
#include "core/Bytes.h"
#include "core/Campaign.h"

#include <algorithm>
#include <cstring>

#include <cstdio>
#include <fstream>

namespace wow
{
    std::string SaveSlot::GamePath(bool human, int slot, bool civilWar)
    {
        char path[48];
        std::snprintf(path, sizeof(path), "SaveGame\\%s%s.%03d", human ? "Human" : "Martian", civilWar ? "CW" : "", slot);
        return path;
    }

    bool SaveSlot::Exists(const GameFiles& files, bool human, int slot, bool civilWar) { return files.Resolve(GamePath(human, slot, civilWar)).has_value(); }

    std::string SaveSlot::CustomPath(int slot)
    {
        char path[48];
        std::snprintf(path, sizeof(path), "SaveGame\\Custom.%03d", slot);
        return path;
    }

    std::optional<uint32_t> SaveSlot::CampaignWord(const GameFiles& files, const std::string& gamePath)
    {
        auto path = files.Resolve(gamePath);
        if (!path) return std::nullopt;
        std::ifstream in(*path, std::ios::binary);
        unsigned char head[EngineSaveHeader] = {};
        if (!in.read(reinterpret_cast<char*>(head), std::streamsize(sizeof(head))) || std::memcmp(head, EngineSaveTag, 4) != 0) return std::nullopt;
        return uint32_t(head[8]) | uint32_t(head[9]) << 8 | uint32_t(head[10]) << 16 | uint32_t(head[11]) << 24;
    }

    std::optional<std::string> SaveSlot::Name(const GameFiles& files, bool human, int slot, bool civilWar)
    {
        return NameAt(files, GamePath(human, slot, civilWar));
    }

    std::optional<std::string> SaveSlot::NameAt(const GameFiles& files, const std::string& gamePath)
    {
        auto path = files.Resolve(gamePath);
        if (!path) return std::nullopt;
        std::ifstream in(*path, std::ios::binary);
        char raw[NameLength] = {};
        // (a civil war's save: the engine's tag, then the original's layout)
        char head[4] = {};
        const bool tagged = in.read(head, 4) && std::memcmp(head, EngineSaveTag, 4) == 0;
        in.clear();
        in.seekg(std::streamoff(NameOffset + (tagged ? EngineSaveHeader : 0)));
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
        if (d.size() >= EngineSaveHeader && std::memcmp(d.data(), EngineSaveTag, 4) == 0) pos = EngineSaveHeader;
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

    std::vector<SaveInfo::WarMapObject> SaveInfo::WarMapObjects(std::span<const uint8_t> d)
    {
        static const char* const Lists[6] = { "HUNI", "AUNI", "HCON", "ACON", "HRES", "ARES" };
        auto u32 = [&](size_t o) { return uint32_t(d[o]) | (uint32_t(d[o + 1]) << 8) | (uint32_t(d[o + 2]) << 16) | (uint32_t(d[o + 3]) << 24); };
        std::vector<WarMapObject> objects;
        int list = -1;
        for (size_t i = 0; i + 24 <= d.size(); i++)
        {
            for (int l = 0; l < 6; l++)
                if (std::memcmp(&d[i], Lists[l], 4) == 0 && i + 8 <= d.size() && std::memcmp(&d[i + 4], "WMOB", 4) == 0) list = l;
            if (list < 0 || std::memcmp(&d[i], "SELO", 4) != 0 || std::memcmp(&d[i + 16], "WMOB", 4) != 0) continue;
            size_t typeAt = list >= 2 ? i - 12 : i - 4; // constructions and resources have two floats after the type
            if (typeAt >= d.size()) continue;
            WarMapObject o;
            o.list = list;
            o.unitType = int(u32(typeAt));
            o.id = int(u32(i + 4));
            o.x = int(int32_t(u32(i + 8)));
            o.y = int(int32_t(u32(i + 12)));
            o.sector = int(u32(i + 20));
            if (list >= 2)
                for (int f = 0; f < 2; f++) { uint32_t b = u32(typeAt + 4 + size_t(f) * 4); std::memcpy(&o.value[f], &b, 4); }
            objects.push_back(o);
        }
        return objects;
    }

    std::vector<int> SaveInfo::SectorOwners(std::span<const uint8_t> d)
    {
        std::vector<int> owners(31, 2);
        int sector = 0;
        for (size_t i = 0; i + 4 <= d.size() && sector < 30; i++)
        {
            if (std::memcmp(&d[i], "SECT", 4) != 0) continue;
            sector++;
            for (size_t j = i + 4; j + 4 <= d.size(); j++)
                if (std::memcmp(&d[j], "DMGL", 4) == 0)
                {
                    if (j >= 4) owners[size_t(sector)] = int(d[j - 4] | (d[j - 3] << 8) | (d[j - 2] << 16) | (d[j - 1] << 24));
                    break;
                }
        }
        return owners;
    }
}
