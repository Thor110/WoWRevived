#include "frontend/Canvas.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace wow
{
    void Canvas::Clear(uint32_t colour) { std::fill(pixels, pixels + size_t(width) * height, colour); }

    void Canvas::DrawTexturedTriangle(const uint8_t* texture, int texW, int texH, const uint32_t* colours,
                                      const TexVertex& a, const TexVertex& b, const TexVertex& c, Blend blend)
    {
        float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
        if (std::abs(area) < 0.5f) return;
        int minX = std::max(0, int(std::floor(std::min({ a.x, b.x, c.x }))));
        int maxX = std::min(width - 1, int(std::ceil(std::max({ a.x, b.x, c.x }))));
        int minY = std::max(0, int(std::floor(std::min({ a.y, b.y, c.y }))));
        int maxY = std::min(height - 1, int(std::ceil(std::max({ a.y, b.y, c.y }))));
        for (int y = minY; y <= maxY; y++)
            for (int x = minX; x <= maxX; x++)
            {
                float px = float(x) + 0.5f, py = float(y) + 0.5f;
                float w0 = ((b.x - px) * (c.y - py) - (c.x - px) * (b.y - py)) / area;
                float w1 = ((c.x - px) * (a.y - py) - (a.x - px) * (c.y - py)) / area;
                float w2 = 1.0f - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                int u = int(w0 * a.u + w1 * b.u + w2 * c.u), v = int(w0 * a.v + w1 * b.v + w2 * c.v);
                u = ((u % texW) + texW) % texW;
                v = ((v % texH) + texH) % texH;
                uint8_t index = texture[size_t(v) * size_t(texW) + size_t(u)];
                if (!index) continue;
                uint32_t& p = pixels[size_t(y) * size_t(width) + size_t(x)];
                p = blend == Blend::Opaque ? colours[index] : ((colours[index] >> 1) & 0x7F7F7F) + ((p >> 1) & 0x7F7F7F);
            }
    }

    void Canvas::DrawImage(const IndexedImage& image, const uint32_t* colours, int x, int y, Blend blend)
    {
        int x0 = std::max(0, -x), y0 = std::max(0, -y);
        int x1 = std::min(image.width, width - x), y1 = std::min(image.height, height - y);
        for (int iy = y0; iy < y1; iy++)
        {
            const uint8_t* src = image.pixels.data() + size_t(iy) * image.width;
            uint32_t* dst = pixels + size_t(y + iy) * width + x;
            if (blend == Blend::Opaque)
            {
                for (int ix = x0; ix < x1; ix++)
                    if (src[ix]) dst[ix] = colours[src[ix]];
            }
            else
            {
                for (int ix = x0; ix < x1; ix++)
                    if (src[ix]) dst[ix] = ((colours[src[ix]] >> 1) & 0x7F7F7F) + ((dst[ix] >> 1) & 0x7F7F7F);
            }
        }
    }

    void Canvas::DrawSprite(int gtype, int frame, int x, int y, int level, Blend blend)
    {
        const IndexedImage* image = res.GetFrame(gtype, frame);
        const ShadeTable* shade = res.ShadeFor(gtype);
        const GType* g = res.FindGType(gtype);
        if (!image || !shade || !g) return;
        DrawImage(*image, shade->Level(level), x - g->hotX, y - g->hotY, blend);
    }

    void Canvas::DrawSpriteRows(int gtype, int frame, int x, int y, int firstRow, int rows, int level)
    {
        const IndexedImage* image = res.GetFrame(gtype, frame);
        const ShadeTable* shade = res.ShadeFor(gtype);
        const GType* g = res.FindGType(gtype);
        if (!image || !shade || !g || rows <= 0) return;
        firstRow = std::clamp(firstRow, 0, image->height);
        rows = std::min(rows, image->height - firstRow);
        const uint32_t* colours = shade->Level(level);
        x -= g->hotX;
        for (int r = 0; r < rows; r++)
        {
            const int sy = y + r;
            if (sy < 0 || sy >= height) continue;
            const uint8_t* src = image->pixels.data() + size_t(firstRow + r) * image->width;
            uint32_t* dst = pixels + size_t(sy) * width;
            for (int ix = std::max(0, -x); ix < image->width && x + ix < width; ix++)
                if (src[ix]) dst[x + ix] = colours[src[ix]];
        }
    }

    void Canvas::DrawString(int fontGType, std::string_view text, const Rect& box, int flags, int level, int lineSpacing)
    {
        const Font* font = res.GetFont(fontGType);
        const ShadeTable* shade = res.ShadeFor(fontGType);
        if (!font || !shade) return;
        const uint32_t* colours = shade->Level(level);

        std::vector<std::string_view> lines;
        size_t start = 0;
        while (lines.size() < 16) // the game's text objects hold up to 16 lines
        {
            size_t end = text.find('\n', start);
            lines.push_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
            if (end == std::string_view::npos) break;
            start = end + 1;
        }

        int lineHeight = font->Height() + lineSpacing;
        int totalHeight = font->Height() * int(lines.size()) + lineSpacing * (int(lines.size()) - 1); // as 0x40F770
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
                    for (int gx = 0; gx < glyph->width; gx++)
                    {
                        int px = x + gx, ax = glyph->atlasX + gx;
                        if (px < 0 || px >= width || ax >= font->AtlasWidth()) continue;
                        uint8_t index = font->AtlasPixel(ax, gy);
                        if (index) pixels[size_t(py) * width + px] = colours[index];
                    }
                }
                x += glyph->width + font->Spacing();
            }
            y += lineHeight;
        }
    }

    // 0x416250: each character's width (0x40FB60: its glyph's plus the spacing, 0 below 0x20) is added as it goes;
    // a word starts at a character over ' ' after one that isn't. At a character over ' ' with the sum at the width or
    // more, or after a '\n', the line ends: the text from the line's start to just before the word, less the spaces
    // (and line breaks) before it, then a '\n'; the next line starts at the word, and the sum goes back to 0 as the
    // scan goes on from there. Runs of spaces inside a line are kept.
    std::string WrapText(const Font& font, std::string_view text, int width)
    {
        auto charWidth = [&](unsigned char c) { return c <= 0x1F ? 0 : font.Measure(std::string_view(reinterpret_cast<const char*>(&c), 1)); };
        std::string out;
        size_t p = 0, lineStart = 0, wordStart = 0;
        unsigned char prev = ' ';
        int sum = 0;
        for (;;)
        {
            const unsigned char c = p < text.size() ? static_cast<unsigned char>(text[p]) : 0;
            sum += charWidth(c);
            if (c > ' ' && prev <= ' ') wordStart = p;
            p++;
            if (c == 0)
            {
                if (lineStart < text.size()) out.append(text.substr(lineStart));
                break;
            }
            // (a word as wide as the line on its own would start the line again for ever, in the game too: the engine
            // lets it run over instead)
            if (((c > ' ' && sum >= width) || prev == '\n') && wordStart > lineStart)
            {
                // back over the spaces before the word (0x4162EB-0x416301)
                size_t end = wordStart;     // one past the line's last character
                while (end > lineStart && static_cast<unsigned char>(text[end - 1]) <= ' ') end--;
                if (end > lineStart) out.append(text.substr(lineStart, end - lineStart));
                out += '\n';
                lineStart = wordStart;
                p = wordStart;
                sum = 0;
            }
            prev = c;
        }
        return out;
    }

    std::string Canvas::Wrap(int fontGType, std::string_view text, int width) const
    {
        const Font* font = res.GetFont(fontGType);
        if (!font) return std::string(text);
        return WrapText(*font, text, width);
    }
}
