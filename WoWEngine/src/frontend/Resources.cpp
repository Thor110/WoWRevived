#include "frontend/Resources.h"
#include "core/ShadeGroups.h"

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
        if (const GType* g = FindGType(id); g && (g->gclass == 1 || g->gclass == 2 || g->gclass == 14))
        {
            // an IOB or WOF model, or a shadow part's raw mask (class 14, dat\\b-shad01.raw), not a sprite: asked
            // of every type for its picture's size (the vehicles' "is it low enough to pass over"); nothing to
            // load, and nothing missing
        }
        else if (g)
        {
            try
            {
                if (auto data = vfs.Load(g->file)) sprite = std::make_unique<Sprite>(std::move(*data));
                else Missing(g->file);
            }
            catch (const std::exception& ex) { Missing(g->file + " (" + ex.what() + ")"); }
        }
        else Missing("graphics entry " + std::to_string(id));
        return (sprites[id] = std::move(sprite)).get();
    }

    const IndexedImage* Resources::GetFrame(int id, int frame)
    {
        auto key = std::make_pair(id, frame);
        auto it = frames.find(key);
        if (it != frames.end()) return &it->second;
        const Sprite* sprite = GetSprite(id);
        if (!sprite) return nullptr;
        return &(frames[key] = sprite->Decode(frame));
    }

    const IndexedImage* Resources::GetFileImage(const std::string& file)
    {
        auto it = fileImages.find(file);
        if (it == fileImages.end())
        {
            std::optional<IndexedImage> image;
            try
            {
                if (auto data = vfs.Load(file)) image = Sprite(std::move(*data)).Decode(0);
                else Missing(file);
            }
            catch (const std::exception& ex) { Missing(file + " (" + ex.what() + ")"); }
            it = fileImages.emplace(file, std::move(image)).first;
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
                data = vfs.Load(g->file);
                if (!data) Missing(g->file);
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
