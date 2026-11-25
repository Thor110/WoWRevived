#include "frontend/Resources.h"
#include "core/EngineText.h"
#include <cctype>
#include "core/ShadeGroups.h"
#include "core/Png.h"
#include "core/TexturePack.h"

#include <SDL3/SDL_log.h>

#include <exception>

namespace wow
{
    namespace
    {
        std::vector<uint8_t> Require(Vfs& vfs, const char* path)
        {
            auto data = vfs.Load(path);
            if (!data) throw std::runtime_error(std::string("can't load ") + path);
            return std::move(*data);
        }
    }

    Resources::Resources(Vfs& v, bool isHuman)
        : vfs(v), human(isHuman),
          text(TextTable::Load(Require(v, "Text.OJD"), isHuman)),
          gtypes(ObjOjd::LoadGTypes(Require(v, "obj.ojd"), isHuman))
    {
        SDL_Log("Loaded %zu strings and %zu graphics entries", text.Count(), gtypes.size());
    }

    void Resources::Missing(const std::string& what)
    {
        if (reported.insert(what).second) SDL_Log("Missing: %s", what.c_str());
    }

    const GType* Resources::FindGType(int id) const
    {
        auto it = gtypes.find(id);
        return it == gtypes.end() ? nullptr : &it->second;
    }

    const Sprite* Resources::GetSprite(int id)
    {
        auto it = sprites.find(id);
        if (it != sprites.end()) return it->second.get();
        std::unique_ptr<Sprite> sprite;
        // (a .spr always a sprite: OBJ.ojd's class record carries on to the entries after it, and the Martian research
        // pictures, dat\\rp_*.spr from 0x321, have none of their own after the models' class 1; a tester, update 253)
        const auto isSpr = [](const std::string& f) {
            return f.size() > 4 && (f[f.size() - 4] == '.') && std::tolower(uint8_t(f[f.size() - 3])) == 's' &&
                   std::tolower(uint8_t(f[f.size() - 2])) == 'p' && std::tolower(uint8_t(f[f.size() - 1])) == 'r';
        };
        if (const GType* g = FindGType(id); g && (g->gclass == 1 || g->gclass == 2 || g->gclass == 14) && !isSpr(g->file))
        {
            // an IOB or WOF model, or a shadow part's raw mask (class 14, dat\\b-shad01.raw), not a sprite: asked
            // of every type for its picture's size (the vehicles' "is it low enough to pass over"); nothing to
            // load, and nothing missing
        }
        else if (g)
        {
            try
            {
                const std::string file = FileFor(*g);
                readFrom[id] = file;
                if (auto data = vfs.Load(file)) sprite = std::make_unique<Sprite>(std::move(*data));
                else Missing(file);
            }
            catch (const std::exception& ex) { Missing(g->file + " (" + ex.what() + ")"); }
        }
        else Missing("graphics entry " + std::to_string(id));
        return (sprites[id] = std::move(sprite)).get();
    }

    // The file a graphics entry is read from: one named for it (SetFileOverrides), else the copy made for the view
    // size if there is one (the name after its folder), else the game's own
    std::string Resources::FileFor(const GType& g)
    {
        const size_t slash = g.file.find_last_of("\\/");
        std::string name = slash == std::string::npos ? g.file : g.file.substr(slash + 1);
        std::string lower = name;
        for (char& c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
        if (auto o = overrides.find(lower); o != overrides.end() && vfs.Exists(o->second)) return o->second;
        if (!sizeFolder.empty() && vfs.Exists(sizeFolder + name)) return sizeFolder + name;
        if (enhanced && vfs.Exists("dat\\extra\\HD\\" + name)) return "dat\\extra\\HD\\" + name;
        return g.file;
    }

    void Resources::Refresh()
    {
        std::vector<int> stale;
        for (const auto& [id, file] : readFrom)
            if (const GType* g = FindGType(id); g && FileFor(*g) != file) stale.push_back(id);
        for (int id : stale) Retire(id);
        if (!stale.empty()) generation++;
    }

    void Resources::Retire(int id)
    {
        if (auto s = sprites.find(id); s != sprites.end()) { if (s->second) retiredSprites.push_back(std::move(s->second)); sprites.erase(s); }
        if (auto r = raws.find(id); r != raws.end()) { retiredRaws.push_back(std::move(r->second)); raws.erase(r); }
        readFrom.erase(id);
        for (auto it = frames.begin(); it != frames.end();)
        {
            auto next = std::next(it);
            if (it->first.first == id) retiredFrames.push_back(frames.extract(it));
            it = next;
        }
        for (auto it = colourFrames.begin(); it != colourFrames.end();)
        {
            auto next = std::next(it);
            if (it->first.first == id) retiredColourFrames.push_back(colourFrames.extract(it));
            it = next;
        }
        for (auto it = pngChecked.begin(); it != pngChecked.end();) it = it->first == id ? pngChecked.erase(it) : std::next(it);
    }

    void Resources::RetireFrames()
    {
        while (!frames.empty()) retiredFrames.push_back(frames.extract(frames.begin()));
        while (!colourFrames.empty()) retiredColourFrames.push_back(colourFrames.extract(colourFrames.begin()));
        pngChecked.clear();
        generation++;
    }

    void Resources::SetViewSize(int width, int height)
    {
        const std::string folder = "dat\\extra\\" + std::to_string(width) + "x" + std::to_string(height) + "\\";
        if (folder == sizeFolder) return;
        sizeFolder = folder;
        // (for players' reports: which of the size's backgrounds were found)
        std::string found;
        for (const char* name : { "humanbd.spr", "martbd.spr", "legal1.spr", "legal2.spr" })
            if (vfs.Exists(sizeFolder + name)) found += std::string(found.empty() ? "" : ", ") + name;
        SDL_Log("View size %d x %d: backgrounds in %s: %s", width, height, sizeFolder.c_str(), found.empty() ? "none found" : found.c_str());
        Refresh();
    }

    void Resources::SetEnhancedAssets(bool on)
    {
        on = on && enhancedAllowed;
        if (on == enhanced) return;
        enhanced = on;
        Refresh();
        // (the PNG overrides come and go with it: every frame decoded again, looking for them or not; the old ones
        // kept, as something may be drawing with them)
        RetireFrames();
    }

    void Resources::SetFileOverrides(std::map<std::string, std::string> files)
    {
        if (files == overrides) return;
        overrides = std::move(files);
        Refresh();
    }

    const IndexedImage* Resources::GetFrame(int id, int frame)
    {
        auto key = std::make_pair(id, frame);
        auto it = frames.find(key);
        if (it != frames.end()) return &it->second;
        const Sprite* sprite = GetSprite(id);
        if (!sprite) return nullptr;
        IndexedImage image = sprite->Decode(frame);
        ApplyPngOverride(id, frame, image);
        return &(frames[key] = std::move(image));
    }

    const Resources::ColourImage* Resources::GetColourFrame(int id, int frame)
    {
        if (colourFrames.empty()) return nullptr;
        GetFrame(id, frame);
        auto it = colourFrames.find({ id, frame });
        return it == colourFrames.end() ? nullptr : &it->second;
    }

    // MODERN (docs/OVERRIDE.md): the frame's PNG, looked for beside the file the sprite is read from (its game path
    // with the extension taken off, through the same loose folders), named as WoWViewer exports a sprite's frames;
    // at the frame's size, or 2, 4 or 8 times it (made the frame's size, textures::FitTo). Kept in full colour, and
    // given to the indexed drawing matched to the sprite's colour table at level 0 (textures::Indices). Without the
    // table (a battle map's own slots), the indexed drawing keeps the game's frame.
    void Resources::ApplyPngOverride(int id, int frame, IndexedImage& image)
    {
        if (!enhanced) return;
        const GType* g = FindGType(id);
        const Sprite* sprite = GetSprite(id);
        if (!g || !sprite) return;
        std::string base = FileFor(*g);
        if (const size_t dot = base.find_last_of('.'); dot != std::string::npos && base.find_last_of("\\/") < dot) base.erase(dot);
        char suffix[32];
        std::snprintf(suffix, sizeof(suffix), "_frame_%02d.png", frame);
        const std::string path = sprite->FrameCount() > 1 ? base + suffix : base + ".png";
        auto data = vfs.Load(path);
        if (!data) return;
        const bool first = pngChecked.insert({ id, frame }).second;
        PngImage png;
        try { png = DecodePng(*data); }
        catch (const std::exception& ex) { if (first) SDL_Log("Override %s: %s; the game's is used", path.c_str(), ex.what()); return; }
        auto fitted = textures::FitTo(png, image.width, image.height);
        if (!fitted)
        {
            if (first) SDL_Log("Override %s is %dx%d, not %dx%d (or 2, 4 or 8 times it); the game's is used", path.c_str(), png.width, png.height, image.width, image.height);
            return;
        }
        if (first) SDL_Log("Override: %s", path.c_str());
        ColourImage c{ fitted->width, fitted->height, fitted->rgba };
        // (magenta, the game's see-through, as the PNGs WoWViewer writes have it)
        for (uint32_t& px : c.rgba) if ((px & 0xFFFFFFu) == 0xFF00FFu) px = 0;
        colourFrames[{ id, frame }] = std::move(c);
        if (const ShadeTable* shade = ShadeFor(id))
        {
            uint32_t palette[256];
            const uint32_t* level = shade->Level(0);
            for (int i = 0; i < 256; i++) palette[i] = level[i] & 0xFFFFFFu;
            image.pixels = textures::Indices(*fitted, palette, &image.pixels);
        }
    }

    const std::string& Resources::String(int id) const
    {
        if (id < enginetext::First || text.Has(id)) return text.Get(id);
        static std::map<int, std::string> english;
        auto it = english.find(id);
        if (it == english.end())
        {
            const char* found = enginetext::Find(id);
            it = english.emplace(id, found ? found : "").first;
        }
        return it->second;
    }

    const IndexedImage* Resources::GetFileImage(const std::string& file, int frame)
    {
        const std::string key = frame ? file + "#" + std::to_string(frame) : file;
        auto it = fileImages.find(key);
        if (it == fileImages.end())
        {
            std::optional<IndexedImage> image;
            try
            {
                if (auto data = vfs.Load(file)) image = Sprite(std::move(*data)).Decode(frame);
                else Missing(file);
            }
            catch (const std::exception& ex) { Missing(file + " (" + ex.what() + ")"); }
            it = fileImages.emplace(key, std::move(image)).first;
        }
        return it->second ? &*it->second : nullptr;
    }

    const std::vector<uint8_t>* Resources::GetRaw(int id)
    {
        auto it = raws.find(id);
        if (it == raws.end())
        {
            std::optional<std::vector<uint8_t>> data;
            if (const GType* g = FindGType(id))
            {
                const std::string file = FileFor(*g);
                readFrom[id] = file;
                data = vfs.Load(file);
                if (!data) Missing(file);
            }
            else Missing("graphics entry " + std::to_string(id));
            it = raws.emplace(id, std::move(data)).first;
        }
        return it->second ? &*it->second : nullptr;
    }

    const Font* Resources::GetFont(int id)
    {
        auto it = fonts.find(id);
        if (it != fonts.end()) return it->second.get();
        std::unique_ptr<Font> font;
        if (const GType* g = FindGType(id))
        {
            try
            {
                if (auto data = vfs.Load(g->file)) font = std::make_unique<Font>(std::move(*data));
                else Missing(g->file);
            }
            catch (const std::exception& ex) { Missing(g->file + " (" + ex.what() + ")"); }
        }
        else Missing("font entry " + std::to_string(id));
        return (fonts[id] = std::move(font)).get();
    }

    void Resources::LoadShadeGroup(int paletteGType)
    {
        for (const ShadeGroup& group : ShadeGroups)
        {
            if (group.paletteGType != paletteGType) continue;
            // 0x40B6C0 frees all 33 slots first, so a group's screen never draws with another group's tables
            shades.clear();
            // (an overridden frame is matched to its table again: another group can put another table in its slot)
            for (const auto& [key, c] : colourFrames) frames.erase(key);
            for (int i = 0; i < group.count; i++)
            {
                std::string name(group.files[i]);
                for (auto at = name.find("{S}"); at != std::string::npos; at = name.find("{S}")) name.replace(at, 3, human ? "H" : "M");
                std::string path = "dat\\" + name + ".SHH";
                std::unique_ptr<ShadeTable> table;
                try
                {
                    if (auto data = vfs.Load(path)) table = std::make_unique<ShadeTable>(ShadeTable::LoadShh(*data));
                    else Missing(path);
                }
                catch (const std::exception& ex) { Missing(path + " (" + ex.what() + ")"); }
                shades[group.slots[i]] = std::move(table);
            }
            return;
        }
        Missing("shade group " + std::to_string(paletteGType));
    }

    const ShadeTable* Resources::GetShade(int slot) const
    {
        auto it = shades.find(slot);
        return it == shades.end() ? nullptr : it->second.get();
    }

    const ShadeTable* Resources::ShadeFor(int id) const
    {
        const GType* g = FindGType(id);
        return g ? GetShade(g->paletteSlot) : nullptr;
    }
}
