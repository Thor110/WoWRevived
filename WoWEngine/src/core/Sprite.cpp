#include "core/Sprite.h"
#include "core/Bytes.h"

#include <algorithm>

namespace wow
{
    namespace
    {
        // Offsets are stored as a carry byte plus the low 16 bits. The high part is worked out from the carry and
        // from the low part wrapping round between consecutive entries.
        struct OffsetDecoder
        {
            int high = 0, prevLow = 0;
            bool first = true;
            int Next(int carry, int low)
            {
                if (!first && (low < prevLow || (low == 0 && prevLow > 32768))) high++;
                if (carry > high) high = carry;
                prevLow = low;
                first = false;
                return high * 65536 + low;
            }
        };
    }

    Sprite::Sprite(std::vector<uint8_t> bytes) : data(std::move(bytes))
    {
        ByteReader r(data);
        info.width = r.U16();
        info.height = r.U16();
        info.frames = r.U16();
        info.rowHeaderSize = r.U16();
        if (info.frames == 0) throw FormatError("SPR: no frames");
        size_t tables = 8 + size_t(info.frames > 1 ? info.frames : 0) * 4 + size_t(info.height) * 4;
        if (tables > data.size()) throw FormatError("SPR: offset tables are cut short");
    }

    uint16_t Sprite::U16(size_t at) const
    {
        if (at + 2 > data.size()) throw FormatError("SPR: read out of range");
        return uint16_t(data[at] | (data[at + 1] << 8));
    }

    int32_t Sprite::I32(size_t at) const
    {
        if (at + 4 > data.size()) throw FormatError("SPR: read out of range");
        return int32_t(uint32_t(data[at]) | (uint32_t(data[at + 1]) << 8) | (uint32_t(data[at + 2]) << 16) | (uint32_t(data[at + 3]) << 24));
    }

    IndexedImage Sprite::Decode(int frame) const
    {
        IndexedImage image;
        image.width = info.width;
        image.height = info.height;
        image.pixels.assign(size_t(info.width) * info.height, 0);
        frame = std::clamp(frame, 0, int(info.frames) - 1);
        const int height = info.height;

        if (info.frames == 1)
        {
            OffsetDecoder offsets;
            for (int row = 0; row < height; row++)
            {
                size_t entry = 8 + size_t(row) * 4;
                int offset = offsets.Next(data[entry], U16(entry + 2));
                DecodeRow(image, row, size_t(offset) + info.rowHeaderSize);
            }
            return image;
        }

        const size_t outerTable = 8 + size_t(info.frames) * 4;
        if (frame < info.frames - 1)
        {
            // frames before the last: a table of height int32 offsets, relative to the frame's block
            OffsetDecoder offsets;
            int block = 0;
            for (int i = 0; i <= frame; i++)
            {
                size_t entry = 8 + size_t(i) * 4;
                block = offsets.Next(data[entry], U16(entry + 2));
            }
            for (int row = 0; row < height; row++)
            {
                size_t at = size_t(block) + size_t(row) * 4;
                if (at + 4 > data.size()) continue;
                int64_t pos = int64_t(block) + I32(at);
                if (pos >= 0) DecodeRow(image, row, size_t(pos));
            }
        }
        else
        {
            // the last frame: row 0 follows the outer row table, rows 1.. come from its entries
            for (int row = 0; row < height; row++)
            {
                size_t pos = row == 0 ? outerTable + size_t(height - 1) * 4
                                      : size_t(U16(outerTable + size_t(row - 1) * 4 + 2)) + info.rowHeaderSize;
                DecodeRow(image, row, pos);
            }
        }
        return image;
    }

    void Sprite::DecodeRow(IndexedImage& image, int row, size_t pos) const
    {
        uint8_t* out = image.pixels.data() + size_t(row) * image.width;
        int x = 0;
        while (x < image.width && pos + 1 < data.size())
        {
            uint8_t index = data[pos];
            int count = data[pos + 1];
            pos += 2;
            int run = std::min(count, image.width - x); // the last run of a row may overshoot, clip it
            std::fill(out + x, out + x + run, index);
            x += run;
        }
    }
}
