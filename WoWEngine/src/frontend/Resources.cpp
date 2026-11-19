#include "frontend/Resources.h"

#include <SDL3/SDL_log.h>

#include <exception>

namespace wow
{
    namespace
    {
        // Palette slots from OBJ.ojd, in the order the PAL files are listed there (see WoWViewer/SprDecoder.cs).
        // TODO: the game draws in 16-bit through the shade tables loaded at 0x40B6C0; check the slot to file
        // mapping against that code once the shade tables are ported.
        const char* const PaletteFiles[16] = {
            "dat\\hw.pal", "dat\\mw.pal", "dat\\hb.pal", "dat\\mb.pal", "dat\\hr.pal", "dat\\mr.pal", "dat\\bm.pal",
            "dat\\f1.pal", "dat\\f2.pal", "dat\\f3.pal", "dat\\f4.pal", "dat\\f5.pal", "dat\\f6.pal", "dat\\f7.pal",
            "dat\\se.pal", "dat\\cd.pal",
        };

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
        if (const GType* g = FindGType(id))
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

    const Palette* Resources::GetPalette(int slot)
    {
        if (slot < 0 || slot >= 16) slot = 15; // slots 16 and up are set up at run time by the game; not known yet
        auto it = palettes.find(slot);
        if (it != palettes.end()) return it->second.get();
        std::unique_ptr<Palette> palette;
        try
        {
            if (auto data = vfs.Load(PaletteFiles[slot])) palette = std::make_unique<Palette>(Palette::Load(*data));
            else Missing(PaletteFiles[slot]);
        }
        catch (const std::exception& ex) { Missing(std::string(PaletteFiles[slot]) + " (" + ex.what() + ")"); }
        return (palettes[slot] = std::move(palette)).get();
    }

    const Palette* Resources::PaletteFor(int id)
    {
        const GType* g = FindGType(id);
        return g ? GetPalette(g->paletteSlot) : nullptr;
    }
}
