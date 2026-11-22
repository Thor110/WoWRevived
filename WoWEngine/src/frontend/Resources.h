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
        // The view size (docs/MODERN.md "Resolution"). A sprite that has a copy made for the size, in
        // dat\extra\<width>x<height>\ beside the game's files (WoWRevived's menu backgrounds: humanbd, martbd,
        // legal1, legal2), is read from there. Changing it lets go of the sprites that were, or now are, such copies.
        void SetViewSize(int width, int height);
        // Files read in place of the game's own, by name without its folder (lower case) to a game path: the war
        // map's wider pictures for a view wider than the original's (WarMap::SetViewSize). Replaces the last set.
        void SetFileOverrides(std::map<std::string, std::string> files);
        bool HasFile(const std::string& gamePath) { return vfs.Exists(gamePath); }
        const Font* GetFont(int gtype);
        // A graphics entry's file as it is (class 6 entries are raw 8-bit images, like the route arrows' 256 x 64)
        const std::vector<uint8_t>* GetRaw(int gtype);
        // The first frame of a sprite file named outright rather than through the graphics table (the mini battle
        // maps, maps\hmin%02d.spr / mmin%02d.spr, 0x4A1F90), nothing if it can't be read
        const IndexedImage* GetFileImage(const std::string& file);
        // A game file by its game path (dat\..., maps\...), loose or from the archives
        std::optional<std::vector<uint8_t>> LoadFile(const std::string& path) { return vfs.Load(path); }
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
        std::map<std::string, std::string> overrides;
        std::map<int, std::string> readFrom;   // each graphics entry's file as it was read
        std::string FileFor(const GType& g);
        void Refresh();                        // lets go of every entry whose file would now be another
        std::string sizeFolder;              // "dat\\extra\\800x600\\", empty until a size is set
        std::map<std::pair<int, int>, IndexedImage> frames;
        std::map<int, std::unique_ptr<Font>> fonts;
        std::map<int, std::optional<std::vector<uint8_t>>> raws;
        std::map<std::string, std::optional<IndexedImage>> fileImages;
        std::map<int, std::unique_ptr<ShadeTable>> shades; // by slot
        std::set<std::string> reported;
    };
}
