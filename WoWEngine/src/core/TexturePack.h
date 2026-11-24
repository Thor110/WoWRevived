// MODERN (docs/TEXTURES.md): texture packs. PNG files in a folder beside the program, laid out as the texture
// export (wowdump --textures) writes them, read in place of the units' WOF atlases and the buildings' IOB pictures:
//   human/units, human/buildings, martian/units, martian/buildings, scenery: each faction's (both sides of a civil
//   war), and civilwar/<faction>/units|buildings: only the other side's in that faction's civil war.
// A picture is turned back into the game's colour indices against the palette the export used (the battle map's
// midday table for the model's colour group at level ExportLevel): an 8-bit PNG with that palette is read by its
// indices exactly, anything else by the nearest colour; see-through (alpha under 128, pure magenta, or palette
// entry 0 of an 8-bit PNG) is index 0.
#pragma once
#include "core/ObjOjd.h"
#include "core/Png.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace wow
{
    struct IndexedImage;

    namespace textures
    {
        constexpr int ExportLevel = 44;
        // the shade table (in dat\) the export coloured a palette slot's models with: 1 Human buildings, 2 Human
        // vehicles, 3 Martian vehicles, 4 Martian buildings, 10 nature; empty for any other
        std::string PaletteTable(int slot);
        // Each WOF / IOB graphics entry's folder: the unit types' models (+0x28) and their parts' (record 28, unit
        // types of their own) by the type's faction and category (2 and up buildings); the rest by its slot (2 Human
        // units, 3 Martian units, 4 Martian buildings, 1 and 10 scenery); the bridges scenery.
        std::map<int, std::string> Folders(const std::map<int, GType>& gtypes, const std::map<int, ObjOjd::UnitData>& units);
        // "dat\\AIRSHIP.WOF" -> "AIRSHIP"
        std::string Stem(const std::string& gameFile);
        // The picture as colour indices for a 256-colour palette (0x00RRGGBB, entry 0 the see-through one).
        std::vector<uint8_t> Indices(const PngImage& png, const uint32_t* palette);
        // a WOF file's atlas (its 256 x height rows at header 0x28) replaced; false if the sizes don't agree
        bool ReplaceWofAtlas(std::vector<uint8_t>& file, const std::vector<uint8_t>& indices, int width, int height);
        // a WOF file's atlas size: 256 x the rows from header 0x28 to header 0x2C (Models.cpp); 0 if none
        int WofAtlasHeight(const std::vector<uint8_t>& file);
    }

    class TexturePack
    {
    public:
        // Every PNG under root, by its path in lower case without ".png" ("human/units/airship"). False if there
        // are none.
        bool Open(const std::filesystem::path& root);
        bool Empty() const { return files.empty(); }
        const std::filesystem::path& Root() const { return root; }
        // the file for a model (its folder and stem), in the civil war's folder for the other side if asked
        std::optional<std::filesystem::path> Find(const std::string& folder, const std::string& stem, bool civilOther) const;
        size_t Count() const { return files.size(); }

    private:
        std::filesystem::path root;
        std::map<std::string, std::filesystem::path> files;
    };
}
