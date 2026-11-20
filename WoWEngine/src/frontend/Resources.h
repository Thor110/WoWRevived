// Everything the front end draws with: the graphics table from OBJ.ojd, the strings from TEXT.ojd, the shade
// tables of the current palette group, and caches of the sprites and fonts the table points at.
// Missing files are reported once and then drawn as nothing, so one bad file doesn't stop the menus from working.
#pragma once
#include "core/Font.h"
#include "core/ObjOjd.h"
#include "core/ShadeTable.h"
#include "core/Sprite.h"
#include "core/TextOjd.h"
#include "core/Vfs.h"

#include <map>
#include <memory>
#include <set>
#include <optional>
#include <vector>
#include <string>
#include <utility>

namespace wow
{
    class Resources
    {
    public:
        // Throws if obj.ojd or Text.OJD can't be read.
        Resources(Vfs& vfs, bool human);

        bool Human() const { return human; }
        const TextTable& Text() const { return text; }
        const std::string& String(int id) const { return text.Get(id); }

        const GType* FindGType(int id) const;
        const Sprite* GetSprite(int gtype);
        const IndexedImage* GetFrame(int gtype, int frame = 0);
        const Font* GetFont(int gtype);
        // A graphics entry's file as it is (class 6 entries are raw 8-bit images, like the route arrows' 256 x 64)
        const std::vector<uint8_t>* GetRaw(int gtype);
        // The first frame of a sprite file named outright rather than through the graphics table (the mini battle
        // maps, maps\hmin%02d.spr / mmin%02d.spr, 0x4A1F90), nothing if it can't be read
        const IndexedImage* GetFileImage(const std::string& file);
        // Loads a palette group's shade tables into their slots, as LoadShadeTables does (see ShadeGroups.h).
        void LoadShadeGroup(int paletteGType);
        const ShadeTable* GetShade(int slot) const;
        const ShadeTable* ShadeFor(int gtype) const;

    private:
        void Missing(const std::string& what);

        Vfs& vfs;
        bool human;
        TextTable text;
        std::map<int, GType> gtypes;
        std::map<int, std::unique_ptr<Sprite>> sprites;
        std::map<std::pair<int, int>, IndexedImage> frames;
        std::map<int, std::unique_ptr<Font>> fonts;
        std::map<int, std::optional<std::vector<uint8_t>>> raws;
        std::map<std::string, std::optional<IndexedImage>> fileImages;
        std::map<int, std::unique_ptr<ShadeTable>> shades; // by slot
        std::set<std::string> reported;
    };
}
