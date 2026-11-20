// Drawing into the frame buffer: sprites and text through their shade tables, at a brightness level.
#pragma once
#include "frontend/Resources.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace wow
{
    struct Rect
    {
        int x = 0, y = 0, w = 0, h = 0;
        bool Contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    };

    // Text alignment flags, as in the game's text objects (0x40FBF0)
    enum TextFlags : int
    {
        TextCentreX = 2,
        TextCentreY = 4,
        TextRight = 8,
    };

    class Canvas
    {
    public:
        Canvas(uint32_t* pixels, int width, int height, Resources& resources)
            : pixels(pixels), width(width), height(height), res(resources) {}

        int Width() const { return width; }
        // The frame buffer, for screens that draw their own way (the war map's per-pixel shade levels).
        uint32_t* Pixels() const { return pixels; }
        int Height() const { return height; }

        void Clear(uint32_t colour);
        // How a sprite is combined with what's already on screen. The game's blitter (0x451235) has these two for
        // sprites: Translucent is its mode 3 in 16-bit (each channel halved and added, a 50% blend); in 8-bit it
        // used a blend table instead, which is the checkered look of the menu panels.
        enum class Blend { Opaque, Translucent };

        // Draws a frame of a graphics entry with its top-left corner at x, y (less the entry's hotspot).
        void DrawSprite(int gtype, int frame, int x, int y, int level = 0, Blend blend = Blend::Opaque);
        void DrawImage(const IndexedImage& image, const uint32_t* colours, int x, int y, Blend blend = Blend::Opaque);
        // Draws Latin-1 text in a font from the graphics table, one line per '\n', aligned inside box. Lines are
        // the font's height plus lineSpacing apart: 2 for menu text objects (0x40F6D0), the [Tweak] setting
        // "Text Line Spacing" (default 4) for free-standing ones (0x40F620).
        void DrawString(int fontGType, std::string_view text, const Rect& box, int flags, int level = 0, int lineSpacing = 2);
        // Breaks text into lines no wider than width at spaces, keeping the line breaks it already has
        // (the game's word wrap, 0x4161E0).
        std::string Wrap(int fontGType, std::string_view text, int width) const;

    private:
        uint32_t* pixels;
        int width, height;
        Resources& res;
    };
}
