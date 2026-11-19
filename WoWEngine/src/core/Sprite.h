// .SPR sprites (usually FFUH compressed; Archive and Vfs decompress them), in the layout the game's sprite drawing
// uses (0x40EE10 picks the frame, 0x451235 walks the rows):
//   0x00 uint16 width, 0x02 uint16 height, 0x04 uint16 frame count
//   0x06 uint32 frame offsets[frame count], from the start of the sprite
//   each frame: uint32 row offsets[height], from the start of the frame, then the rows
//   each row: pairs of (colour index, run length); index 0 is a transparent run
// (WoWViewer reads the offsets 2 bytes out of step as a "carry" byte plus 16 bits; that agrees for single frames
// but not for every animated sprite.)
#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace wow
{
    struct IndexedImage
    {
        int width = 0, height = 0;
        std::vector<uint8_t> pixels; // colour indices, 0 = transparent
    };

    struct SpriteInfo
    {
        uint16_t width = 0, height = 0, frames = 0;
    };

    class Sprite
    {
    public:
        // Throws FormatError if the header or offset tables are damaged.
        explicit Sprite(std::vector<uint8_t> data);

        const SpriteInfo& Info() const { return info; }
        int FrameCount() const { return info.frames; }

        // Frame is clamped to the valid range. Rows that point outside the data are left transparent.
        IndexedImage Decode(int frame = 0) const;

    private:
        uint32_t U32(size_t at) const;

        std::vector<uint8_t> data;
        SpriteInfo info;
    };
}
