#include "frontend/Canvas.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace wow
{
    void Canvas::Clear(uint32_t colour) { std::fill(pixels, pixels + size_t(width) * height, colour); }

    void Canvas::DrawTexturedTriangle(const uint8_t* texture, int texW, int texH, const uint32_t* colours,
                                      const TexVertex& a, const TexVertex& b, const TexVertex& c)
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
                if (index) pixels[size_t(y) * size_t(width) + size_t(x)] = colours[index];
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

    std::string WrapText(const Font& font, std::string_view text, int width)
    {
        std::string out, line;
        auto flushWord = [&](std::string_view word) {
            std::string candidate = line.empty() ? std::string(word) : line + " " + std::string(word);
            if (!line.empty() && font.Measure(candidate) > width) { out += line + "\n"; line = std::string(word); }
            else line = candidate;
        };
        size_t start = 0;
        while (start <= text.size())
        {
            size_t end = text.find_first_of(" \n", start);
            if (end == std::string_view::npos) end = text.size();
            if (end > start) flushWord(text.substr(start, end - start));
            if (end < text.size() && text[end] == '\n') { out += line + "\n"; line.clear(); }
            start = end + 1;
        }
        return out + line;
    }

    std::string Canvas::Wrap(int fontGType, std::string_view text, int width) const
    {
        const Font* font = res.GetFont(fontGType);
        if (!font) return std::string(text);
        return WrapText(*font, text, width);
    }
}
