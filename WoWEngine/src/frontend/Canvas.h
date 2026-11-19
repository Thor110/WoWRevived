// Drawing into the frame buffer: sprites and text through their shade tables, at a brightness level.
#pragma once
#include "frontend/Resources.h"

#include <cstdint>
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
        int Height() const { return height; }

        void Clear(uint32_t colour);
        // Draws a frame of a graphics entry with its top-left corner at x, y (less the entry's hotspot).
        void DrawSprite(int gtype, int frame, int x, int y, int level = 0);
        void DrawImage(const IndexedImage& image, const uint32_t* colours, int x, int y);
        // Draws Latin-1 text in a font from the graphics table, one line per '\n', aligned inside box.
        void DrawString(int fontGType, std::string_view text, const Rect& box, int flags, int level = 0);

    private:
        uint32_t* pixels;
        int width, height;
        Resources& res;
    };
}
