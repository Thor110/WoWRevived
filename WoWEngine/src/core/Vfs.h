// Game file access with the same rules as the game's FILE_Load (vanilla 0x415200, lookup 0x415030):
//   - a name starting with "d" (dat\...) is looked up, without the first 4 characters, in DAT\Dat.wow
//   - a name starting with "m" (maps\...) is looked up, without the first 5 characters, in MAPS\MAPS.WoW
//   - archive names are compared without regard to case
//   - a loose file with the same name next to the archive (DAT\name) replaces the archive copy; the game marks
//     those entries when it opens Dat.wow (0x414DE0), which is how the Enhanced Assets swap works
//   - MODERN FIX: for MAPS.WoW the game looks for the loose file in a folder "Map" (0x414FAB), not "MAPS", and
//     then reads the one in MAPS, so its loose map files never work. The engine looks in MAPS (docs/MODERN.md)
//   - anything else, or anything not in an archive, is read as a loose file from the game folder
// FFUH compressed data is decompressed either way.
#pragma once
#include "core/Archive.h"
#include "core/GameFiles.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wow
{
    class Vfs
    {
    public:
        // Throws if Dat.wow can't be opened; MAPS.WoW is optional until the battle map needs it.
        explicit Vfs(GameFiles files);

        std::optional<std::vector<uint8_t>> Load(std::string_view gamePath);
        bool Exists(std::string_view gamePath);

        const GameFiles& Files() const { return files; }

    private:
        std::optional<std::vector<uint8_t>> LoadLoose(std::string_view gamePath) const;

        GameFiles files;
        std::unique_ptr<Archive> dat;
        std::unique_ptr<Archive> maps;
        std::vector<bool> datLoose, mapsLoose;   // each archive entry's mark (0x414DE0): read as a loose file
    };
}
