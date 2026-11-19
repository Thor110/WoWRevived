// .FNT fonts, read the way the game's text code uses them (measuring 0x40FBF0, drawing 0x40F770).
//   0x00 uint16 atlas width, 0x02 uint16 height, 0x04 uint16 glyph count, 0x06 int16 extra spacing
//   0x08 uint16 cell width, used instead of the glyph widths by monospaced text objects
//   0x0A glyph table: count x (uint16 width, uint16 atlas x)
//   then the atlas: height rows of atlas-width colour indices (0 = transparent)
// Characters advance by width + spacing. (WoWViewer reads the table from 0x08, which pairs each glyph's width with
// the previous glyph's x; the game's code reads it from 0x0A.)
// Glyph n is character n + 32 after the Latin1ToFont conversion (Charset.h).
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace wow
{
    class Font
    {
    public:
        // Throws FormatError if the header or atlas is cut short.
        explicit Font(std::vector<uint8_t> data);

        int Height() const { return height; }
        int Spacing() const { return spacing; }
        int CellWidth() const { return cellWidth; }

        // Pixel width of one line of Latin-1 text, as the game measures it (width + spacing per character).
        int Measure(std::string_view latin1) const;

        struct Glyph
        {
            int width = 0, atlasX = 0;
        };
        // nullptr for characters the font doesn't have.
        const Glyph* Find(uint8_t latin1) const;

        int AtlasWidth() const { return atlasWidth; }
        uint8_t AtlasPixel(int x, int y) const { return atlas[size_t(y) * atlasWidth + x]; }

    private:
        int atlasWidth = 0, height = 0, spacing = 0, cellWidth = 0;
        std::vector<Glyph> glyphs;
        std::vector<uint8_t> atlas;
    };
}
