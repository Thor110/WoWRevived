#include "core/Vfs.h"
#include "core/Ffuh.h"

#include <fstream>
#include <iterator>

namespace wow
{
    Vfs::Vfs(GameFiles f) : files(std::move(f))
    {
        auto datPath = files.Resolve("DAT\\Dat.wow");
        if (!datPath) throw std::runtime_error("DAT\\Dat.wow is missing");
        dat = std::make_unique<Archive>(*datPath);
        if (auto mapsPath = files.Resolve("MAPS\\MAPS.WoW")) maps = std::make_unique<Archive>(*mapsPath);
        // 0x414DE0, as the archives are opened: each entry with a loose file of its name is marked (its first
        // dword set to 1), and a marked entry is then never found in the archive (0x4150F2), so the loose file is
        // read. Dat.wow's entries are looked for as "dat\<name>". MAPS.WoW's are looked for as "Map\<name>"
        // (0x414FAB; a folder the game doesn't have: the original's slip), and what is then read is the name that
        // was asked for, "maps\<name>": a loose map file only replaces the archive's when a file of its name is in
        // a "Map" folder as well. Kept as the game has it. The marks are made once, here.
        for (const ArchiveEntry& e : dat->Entries()) datLoose.push_back(files.Resolve("dat\\" + e.name).has_value());
        if (maps)
            for (const ArchiveEntry& e : maps->Entries()) mapsLoose.push_back(files.Resolve("Map\\" + e.name).has_value());
    }

    std::optional<std::vector<uint8_t>> Vfs::LoadLoose(std::string_view gamePath) const
    {
        auto path = files.Resolve(gamePath);
        if (!path) return std::nullopt;
        std::ifstream in(*path, std::ios::binary);
        if (!in) return std::nullopt;
        std::vector<uint8_t> data(std::istreambuf_iterator<char>(in), {});
        return ffuh::Decompress(data);
    }

    std::optional<std::vector<uint8_t>> Vfs::Load(std::string_view gamePath)
    {
        if (!gamePath.empty())
        {
            char first = char(gamePath[0] | 0x20);
            Archive* archive = first == 'd' ? dat.get() : first == 'm' ? maps.get() : nullptr;
            size_t skip = first == 'd' ? 4 : 5;
            if (archive && gamePath.size() > skip)
            {
                // 0x415030: found in the archive and not marked, it's read from there; marked, or not in the
                // archive, it's opened as a file (0x415250)
                const std::vector<bool>& loose = first == 'd' ? datLoose : mapsLoose;
                if (auto index = archive->Find(gamePath.substr(skip)); index && !(*index < loose.size() && loose[*index]))
                    return archive->Read(*index);
            }
        }
        return LoadLoose(gamePath);
    }

    bool Vfs::Exists(std::string_view gamePath)
    {
        if (files.Resolve(gamePath)) return true;   // (a marked entry whose file has gone since isn't caught here)
        if (gamePath.size() > 5)
        {
            char first = char(gamePath[0] | 0x20);
            if (first == 'd' && dat->Find(gamePath.substr(4))) return true;
            if (first == 'm' && maps && maps->Find(gamePath.substr(5))) return true;
        }
        return false;
    }
}
