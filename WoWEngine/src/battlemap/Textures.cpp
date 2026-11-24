// MODERN (docs/TEXTURES.md): the texture pack's pictures read in place of the models' own as the battle map loads
// them: a unit's WOF atlas replaced in its file before any instance is made of it, a building's IOB picture painted
// back into its patches (IobModel::Repainted). In a civil war the other side's units take civilwar/<faction>/...
// where there's a picture there, as separate copies of the models (their keys marked OtherKey), drawn without the
// hue turn.
#include "battlemap/BattleMap.h"
#include "core/ShadeTable.h"

#include <SDL3/SDL_log.h>

#include <fstream>
#include <iterator>

namespace wow
{
    std::optional<std::vector<uint8_t>> BattleMap::PackPicture(int gtype, bool civilOther, int w, int h) const
    {
        if (!setup.textures) return std::nullopt;
        auto folder = setup.textureFolders.find(gtype);
        const GType* g = res.FindGType(gtype);
        if (folder == setup.textureFolders.end() || !g) return std::nullopt;
        const std::string stem = textures::Stem(g->file);
        auto path = setup.textures->Find(folder->second, stem, civilOther);
        if (!path) return std::nullopt;
        const std::string name = (civilOther ? "civilwar/" : "") + folder->second + "/" + stem;
        auto warn = [&](const std::string& why) {
            if (packWarned.insert(name).second) SDL_Log("Texture pack: %s.png %s; the game's is used", name.c_str(), why.c_str());
        };
        std::ifstream in(*path, std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
        PngImage png;
        try { png = DecodePng(bytes); }
        catch (const std::exception& e) { warn(std::string("can't be read (") + e.what() + ")"); return std::nullopt; }
        if (png.width != w || png.height != h)
        {
            warn("is " + std::to_string(png.width) + "x" + std::to_string(png.height) + ", not " + std::to_string(w) + "x" + std::to_string(h));
            return std::nullopt;
        }
        // the export's palette: the slot's midday table at its level
        auto pal = packPalettes.find(g->paletteSlot);
        if (pal == packPalettes.end())
        {
            std::array<uint32_t, 256> colours{};
            const std::string table = textures::PaletteTable(g->paletteSlot);
            try
            {
                if (auto data = table.empty() ? std::nullopt : res.LoadFile("dat\\" + table + ".SHH"))
                {
                    const ShadeTable t = ShadeTable::LoadShh(*data);
                    const uint32_t* level = t.Level(textures::ExportLevel);
                    for (size_t i = 0; i < 256; i++) colours[i] = level[i] & 0xFFFFFFu;
                }
            }
            catch (const std::exception&) {}
            pal = packPalettes.emplace(g->paletteSlot, colours).first;
        }
        if (packWarned.insert("+" + name).second) SDL_Log("Texture pack: %s.png", name.c_str());
        return textures::Indices(png, pal->second.data());
    }

    bool BattleMap::CivilOwnLook(int gtype, int unit) const
    {
        if (!setup.civilWar || !setup.textures || unit < 0 || size_t(unit) >= units.size() || units[size_t(unit)].players) return false;
        auto folder = setup.textureFolders.find(gtype);
        const GType* g = res.FindGType(gtype);
        return folder != setup.textureFolders.end() && g && setup.textures->Find(folder->second, textures::Stem(g->file), true).has_value();
    }

    std::vector<uint8_t>& BattleMap::WofFile(int gtype, bool civilOther)
    {
        const int key = civilOther ? (gtype | OtherModelKey) : gtype;
        auto f = wofFiles.find(key);
        if (f != wofFiles.end()) return f->second;
        std::vector<uint8_t> file;
        if (const GType* g = res.FindGType(gtype))
            if (auto data = res.LoadFile(g->file)) file = std::move(*data);
        if (const int h = textures::WofAtlasHeight(file); h > 0)
            if (auto pic = PackPicture(gtype, civilOther, 256, h)) textures::ReplaceWofAtlas(file, *pic, 256, h);
        return wofFiles.emplace(key, std::move(file)).first->second;
    }

    void BattleMap::PackIob(IobModel& m, int gtype, bool civilOther) const
    {
        if (!setup.textures || m.patches.empty()) return;
        const IndexedImage old = m.Picture();
        if (auto pic = PackPicture(gtype, civilOther, old.width, old.height))
        {
            IndexedImage img;
            img.width = old.width;
            img.height = old.height;
            img.pixels = std::move(*pic);
            m = m.Repainted(img);
        }
    }
}
