// .FNT fonts, read the way the game's text code uses them (vanilla 0x40FBF0).
//   0x00 uint16 atlas width, 0x02 uint16 height, 0x04 uint16 glyph count, 0x06 uint16 extra spacing
//   0x08 glyph table: count x (uint16 atlas x, uint16 advance), then 2 unused bytes
//   then the atlas: height rows of atlas-width palette indices (0 = transparent)
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

        // Pixel width of one line of Latin-1 text, as the game measures it (advance + spacing per character).
        int Measure(std::string_view latin1) const;

        struct Glyph
        {
            int atlasX = 0, advance = 0;
        };
        // nullptr for characters the font doesn't have.
        const Glyph* Find(uint8_t latin1) const;

        int AtlasWidth() const { return atlasWidth; }
        uint8_t AtlasPixel(int x, int y) const { return atlas[size_t(y) * atlasWidth + x]; }

    private:
        int atlasWidth = 0, height = 0, spacing = 0;
        std::vector<Glyph> glyphs;
        std::vector<uint8_t> atlas;
    };
}
