// .SPR sprites (FFUH compressed inside the archives; Archive::Read decompresses them).
// Ported from WoWViewer/SprDecoder.cs, which documents the format in full.
//
//   0x00 uint16 width, 0x02 uint16 height, 0x04 uint16 frame count, 0x06 uint16 row header size (6 + frames * 4)
//   then row / frame offset tables and RLE rows of [palette index, run length] pairs. Index 0 is transparent.
//
// TODO: check the multi-frame offset rules against the game's own sprite blitter. The viewer's rules were worked
// out from the files and a few sprites (R_FOLD.SPR) have rows that resolve out of range; those rows are left blank.
#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace wow
{
    struct IndexedImage
    {
        int width = 0, height = 0;
        std::vector<uint8_t> pixels; // palette indices, 0 = transparent
    };

    struct SpriteInfo
    {
        uint16_t width = 0, height = 0, frames = 0, rowHeaderSize = 0;
    };

    class Sprite
    {
    public:
        // Throws FormatError if the header is damaged.
        explicit Sprite(std::vector<uint8_t> data);

        const SpriteInfo& Info() const { return info; }
        int FrameCount() const { return info.frames; }

        // Frame is clamped to the valid range. Rows whose data is out of range are left transparent.
        IndexedImage Decode(int frame = 0) const;

    private:
        void DecodeRow(IndexedImage& image, int row, size_t pos) const;
        uint16_t U16(size_t at) const;
        int32_t I32(size_t at) const;

        std::vector<uint8_t> data;
        SpriteInfo info;
    };
}
