// MODERN (docs/TEXTURES.md): the texture pack's pictures read in place of the models' own as the battle map loads
// them: a unit's WOF atlas replaced in its file before any instance is made of it, a building's IOB picture painted
// back into its patches (IobModel::Repainted). In a civil war the other side's units take civilwar/<faction>/...
// where there's a picture there, as separate copies of the models (their keys marked OtherKey), drawn without the
// hue turn.
#include "battlemap/BattleMap.h"
#include "core/WofMesh.h"
#include "core/ShadeTable.h"

#include <SDL3/SDL_log.h>

#include <cctype>
#include <fstream>
#include <iterator>

namespace wow
{
    std::optional<std::vector<uint8_t>> BattleMap::PackPicture(int gtype, bool civilOther, int w, int h, const std::vector<uint8_t>& game) const
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
        // the game's size, or 2, 4 or 8 times it, made the game's (textures::FitTo)
        auto fitted = textures::FitTo(png, w, h);
        if (!fitted)
        {
            warn("is " + std::to_string(png.width) + "x" + std::to_string(png.height) + ", not " + std::to_string(w) + "x" + std::to_string(h) +
                 " (or 2, 4 or 8 times it)");
            return std::nullopt;
        }
        png = std::move(*fitted);
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
        return textures::Indices(png, pal->second.data(), &game);
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
        {
            const size_t at = size_t(file[0x28]) | size_t(file[0x29]) << 8 | size_t(file[0x2A]) << 16 | size_t(file[0x2B]) << 24;
            const std::vector<uint8_t> atlas(file.begin() + std::ptrdiff_t(at), file.begin() + std::ptrdiff_t(at + size_t(256) * size_t(h)));
            if (auto pic = PackPicture(gtype, civilOther, 256, h, atlas)) textures::ReplaceWofAtlas(file, *pic, 256, h);
        }
        PackModel(file, gtype, civilOther);
        return wofFiles.emplace(key, std::move(file)).first->second;
    }

    // MODERN (update 248, docs/MODELS.md): the pack's model for a unit, an OBJ, glTF or GLB beside its picture,
    // its pieces' shapes put into the game's WOF (WofMesh), the tree, pivots, animation and materials kept
    void BattleMap::PackModel(std::vector<uint8_t>& file, int gtype, bool civilOther)
    {
        if (!setup.textures || file.empty()) return;
        auto folder = setup.textureFolders.find(gtype);
        const GType* g = res.FindGType(gtype);
        if (folder == setup.textureFolders.end() || !g) return;
        const std::string stem = textures::Stem(g->file);
        auto path = setup.textures->FindModel(folder->second, stem, civilOther);
        if (!path) return;
        const std::string name = (civilOther ? "civilwar/" : "") + folder->second + "/" + path->filename().string();
        std::ifstream in(*path, std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
        std::vector<std::string> warnings;
        std::string ext = path->extension().string();
        for (char& c : ext) c = char(std::tolower(static_cast<unsigned char>(c)));
        const std::vector<MeshPiece> pieces = ext == ".obj" ? ParseObj(std::string(bytes.begin(), bytes.end()), warnings) : ParseGltf(bytes, warnings);
        const bool used = ApplyMeshes(file, pieces, warnings);
        if (packWarned.insert("model:" + name).second)
        {
            SDL_Log(used ? "Texture pack: model %s" : "Texture pack: model %s not used", name.c_str());
            for (const std::string& w : warnings) SDL_Log("  %s", w.c_str());
        }
    }

    void BattleMap::PackIob(IobModel& m, int gtype, bool civilOther) const
    {
        if (!setup.textures || m.patches.empty()) return;
        const IndexedImage old = m.Picture();
        if (auto pic = PackPicture(gtype, civilOther, old.width, old.height, old.pixels))
        {
            IndexedImage img;
            img.width = old.width;
            img.height = old.height;
            img.pixels = std::move(*pic);
            m = m.Repainted(img);
        }
    }
}
