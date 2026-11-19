#include "core/Font.h"
#include "core/Bytes.h"
#include "core/Charset.h"

namespace wow
{
    Font::Font(std::vector<uint8_t> data)
    {
        ByteReader r(data);
        atlasWidth = r.U16();
        height = r.U16();
        int count = r.U16();
        spacing = int16_t(r.U16());
        cellWidth = r.U16();
        glyphs.resize(size_t(count));
        for (Glyph& g : glyphs)
        {
            g.width = r.U16();
            g.atlasX = r.U16();
        }
        auto pixels = r.Bytes(size_t(atlasWidth) * size_t(height));
        atlas.assign(pixels.begin(), pixels.end());
    }

    const Font::Glyph* Font::Find(uint8_t latin1) const
    {
        int index = int(Latin1ToFont[latin1]) - 32;
        if (index < 0 || size_t(index) >= glyphs.size()) return nullptr;
        return &glyphs[size_t(index)];
    }

    int Font::Measure(std::string_view text) const
    {
        int width = 0;
        for (char c : text)
            if (const Glyph* g = Find(uint8_t(c))) width += g->width + spacing;
        return width;
    }
}
