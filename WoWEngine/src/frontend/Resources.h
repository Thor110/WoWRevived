// Everything the front end draws with: the graphics table from OBJ.ojd, the strings from TEXT.ojd, and caches of
// the sprites, fonts and palettes the table points at. Missing files are reported once and then drawn as nothing,
// so one bad file doesn't stop the menus from working.
#pragma once
#include "core/Font.h"
#include "core/ObjOjd.h"
#include "core/Palette.h"
#include "core/Sprite.h"
#include "core/TextOjd.h"
#include "core/Vfs.h"

#include <map>
#include <memory>
#include <set>
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
        const Palette* GetPalette(int slot);
        const Palette* PaletteFor(int gtype);

    private:
        void Missing(const std::string& what);

        Vfs& vfs;
        bool human;
        TextTable text;
        std::map<int, GType> gtypes;
        std::map<int, std::unique_ptr<Sprite>> sprites;
        std::map<std::pair<int, int>, IndexedImage> frames;
        std::map<int, std::unique_ptr<Font>> fonts;
        std::map<int, std::unique_ptr<Palette>> palettes;
        std::set<std::string> reported;
    };
}
