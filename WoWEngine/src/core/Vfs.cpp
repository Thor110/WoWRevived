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
                // a loose copy wins, as in the game
                if (auto loose = LoadLoose(gamePath)) return loose;
                if (auto index = archive->Find(gamePath.substr(skip))) return archive->Read(*index);
            }
        }
        return LoadLoose(gamePath);
    }

    bool Vfs::Exists(std::string_view gamePath)
    {
        if (files.Resolve(gamePath)) return true;
        if (gamePath.size() > 5)
        {
            char first = char(gamePath[0] | 0x20);
            if (first == 'd' && dat->Find(gamePath.substr(4))) return true;
            if (first == 'm' && maps && maps->Find(gamePath.substr(5))) return true;
        }
        return false;
    }
}
