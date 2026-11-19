#include "frontend/Canvas.h"

#include <algorithm>
#include <vector>

namespace wow
{
    namespace
    {
        uint32_t ToPixel(const Rgb& c) { return (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | c.b; }
    }

    void Canvas::Clear(uint32_t colour) { std::fill(pixels, pixels + size_t(width) * height, colour); }

    void Canvas::DrawImage(const IndexedImage& image, const Palette& palette, int x, int y)
    {
        int x0 = std::max(0, -x), y0 = std::max(0, -y);
        int x1 = std::min(image.width, width - x), y1 = std::min(image.height, height - y);
        for (int iy = y0; iy < y1; iy++)
        {
            const uint8_t* src = image.pixels.data() + size_t(iy) * image.width;
            uint32_t* dst = pixels + size_t(y + iy) * width + x;
            for (int ix = x0; ix < x1; ix++)
                if (src[ix]) dst[ix] = ToPixel(palette.colours[src[ix]]);
        }
    }

    void Canvas::DrawSprite(int gtype, int frame, int x, int y)
    {
        const IndexedImage* image = res.GetFrame(gtype, frame);
        const Palette* palette = res.PaletteFor(gtype);
        const GType* g = res.FindGType(gtype);
        if (!image || !palette || !g) return;
        DrawImage(*image, *palette, x - g->hotX, y - g->hotY);
    }

    void Canvas::DrawString(int fontGType, std::string_view text, const Rect& box, int flags, int paletteSlotOverride)
    {
        const Font* font = res.GetFont(fontGType);
        const Palette* palette = paletteSlotOverride >= 0 ? res.GetPalette(paletteSlotOverride) : res.PaletteFor(fontGType);
        if (!font || !palette) return;

        std::vector<std::string_view> lines;
        size_t start = 0;
        while (lines.size() < 16) // the game's text objects hold up to 16 lines
        {
            size_t end = text.find('\n', start);
            lines.push_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
            if (end == std::string_view::npos) break;
            start = end + 1;
        }

        int lineHeight = font->Height();
        int totalHeight = lineHeight * int(lines.size());
        int y = box.y;
        if (flags & TextCentreY) y += (box.h - totalHeight) / 2;

        for (std::string_view line : lines)
        {
            int lineWidth = font->Measure(line);
            int x = box.x;
            if (flags & TextRight) x = box.x + box.w - lineWidth;
            else if (flags & TextCentreX) x += (box.w - lineWidth) / 2;

            for (char c : line)
            {
                const Font::Glyph* glyph = font->Find(uint8_t(c));
                if (!glyph) continue;
                for (int gy = 0; gy < font->Height(); gy++)
                {
                    int py = y + gy;
                    if (py < 0 || py >= height) continue;
                    for (int gx = 0; gx < glyph->advance; gx++)
                    {
                        int px = x + gx, ax = glyph->atlasX + gx;
                        if (px < 0 || px >= width || ax >= font->AtlasWidth()) continue;
                        uint8_t index = font->AtlasPixel(ax, gy);
                        if (index) pixels[size_t(py) * width + px] = ToPixel(palette->colours[index]);
                    }
                }
                x += glyph->advance + font->Spacing();
            }
            y += lineHeight;
        }
    }
}
