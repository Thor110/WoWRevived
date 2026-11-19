#include "installer/Ui.h"
#include "installer/UiFont.h"

#include <algorithm>

namespace wow::ui
{
    namespace
    {
        const BakedGlyph* Find(uint8_t c)
        {
            for (const BakedGlyph& g : FontGlyphs)
                if (g.code == c) return &g;
            return nullptr;
        }
    }

    void Painter::Fill(const Box& b, uint32_t colour)
    {
        int x0 = std::max(0, b.x), y0 = std::max(0, b.y);
        int x1 = std::min(width, b.x + b.w), y1 = std::min(height, b.y + b.h);
        for (int y = y0; y < y1; y++) std::fill(pixels + size_t(y) * width + x0, pixels + size_t(y) * width + x1, colour);
    }

    void Painter::Frame(const Box& b, uint32_t colour)
    {
        Fill({ b.x, b.y, b.w, 1 }, colour);
        Fill({ b.x, b.y + b.h - 1, b.w, 1 }, colour);
        Fill({ b.x, b.y, 1, b.h }, colour);
        Fill({ b.x + b.w - 1, b.y, 1, b.h }, colour);
    }

    void Painter::Image(const uint32_t* source, int w, int h, int x, int y)
    {
        for (int row = 0; row < h; row++)
        {
            int py = y + row;
            if (py < 0 || py >= height) continue;
            for (int col = 0; col < w; col++)
            {
                int px = x + col;
                if (px >= 0 && px < width) pixels[size_t(py) * width + px] = source[size_t(row) * w + col];
            }
        }
    }

    void Painter::Blend(int x, int y, uint32_t colour, uint8_t alpha)
    {
        if (x < 0 || y < 0 || x >= width || y >= height || alpha == 0) return;
        uint32_t& d = pixels[size_t(y) * width + x];
        auto mix = [&](int shift) {
            uint32_t a = (d >> shift) & 0xFF, b = (colour >> shift) & 0xFF;
            return ((a * (255 - alpha) + b * alpha) / 255) << shift;
        };
        d = mix(16) | mix(8) | mix(0);
    }

    int Painter::Text(std::string_view text, int x, int y, uint32_t colour)
    {
        int start = x;
        for (char ch : text)
        {
            const BakedGlyph* g = Find(uint8_t(ch));
            if (!g) g = Find('?');
            if (!g) continue;
            const uint8_t* src = FontPixels + g->offset;
            for (int row = 0; row < FontHeight; row++)
                for (int col = 0; col < g->width; col++) Blend(x + col, y + row, colour, src[row * g->width + col]);
            x += g->width;
        }
        return x - start;
    }

    int Painter::Measure(std::string_view text)
    {
        int w = 0;
        for (char ch : text)
        {
            const BakedGlyph* g = Find(uint8_t(ch));
            if (!g) g = Find('?');
            if (g) w += g->width;
        }
        return w;
    }

    void Painter::TextCentred(std::string_view text, const Box& b, uint32_t colour)
    {
        Text(text, b.x + (b.w - Measure(text)) / 2, b.y + (b.h - FontHeight) / 2, colour);
    }

    std::string Painter::Fit(std::string_view text, int w)
    {
        if (Measure(text) <= w) return std::string(text);
        std::string s(text);
        while (!s.empty() && Measure(s + "...") > w) s.pop_back();
        return s + "...";
    }

    void Painter::Button(const Box& b, std::string_view label, State state, bool isDefault)
    {
        uint32_t face = Colour::ButtonFace, border = Colour::ButtonBorder, text = Colour::Text;
        switch (state)
        {
        case State::Hot: face = Colour::HotFace; border = Colour::HotBorder; break;
        case State::Pressed: face = Colour::PressedFace; border = Colour::PressedBorder; break;
        case State::Disabled: face = Colour::DisabledFace; border = Colour::DisabledBorder; text = Colour::DisabledText; break;
        default: if (isDefault) border = Colour::HotBorder; break;
        }
        Fill(b, face);
        Frame(b, border);
        if (isDefault && state != State::Disabled) Frame({ b.x + 1, b.y + 1, b.w - 2, b.h - 2 }, border);
        TextCentred(label, b, text);
    }

    void Painter::CheckBox(const Box& b, std::string_view label, bool checked, State state)
    {
        Box square{ b.x, b.y + (b.h - 13) / 2, 13, 13 };
        bool disabled = state == State::Disabled;
        Fill(square, disabled ? Colour::DisabledFace : state == State::Hot ? Colour::HotFace : 0xFFFFFF);
        Frame(square, disabled ? Colour::DisabledBorder : state == State::Hot ? Colour::HotBorder : 0x333333);
        if (checked)
        {
            uint32_t tick = disabled ? Colour::DisabledText : 0x333333;
            // a two-stroke tick, like the Windows checkbox
            for (int i = 0; i < 3; i++) { Fill({ square.x + 3 + i, square.y + 6 + i, 1, 2 }, tick); }
            for (int i = 0; i < 5; i++) { Fill({ square.x + 6 + i, square.y + 7 - i, 1, 2 }, tick); }
        }
        Text(label, square.x + 18, b.y + (b.h - FontHeight) / 2, disabled ? Colour::DisabledText : Colour::Text);
    }

    void Painter::Progress(const Box& b, double fraction)
    {
        Fill(b, Colour::ProgressBack);
        Frame(b, Colour::ProgressBorder);
        int filled = int(double(b.w - 2) * std::clamp(fraction, 0.0, 1.0));
        Fill({ b.x + 1, b.y + 1, filled, b.h - 2 }, Colour::ProgressFill);
    }

    std::string ToLatin1(std::string_view utf8)
    {
        std::string out;
        for (size_t i = 0; i < utf8.size(); i++)
        {
            unsigned char c = static_cast<unsigned char>(utf8[i]);
            if (c < 0x80) out += char(c);
            else if ((c & 0xE0) == 0xC0 && i + 1 < utf8.size())
            {
                unsigned code = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu);
                out += code < 256 ? char(code) : '?';
                i++;
            }
            else
            {
                out += '?';
                while (i + 1 < utf8.size() && (static_cast<unsigned char>(utf8[i + 1]) & 0xC0) == 0x80) i++;
            }
        }
        return out;
    }
}
