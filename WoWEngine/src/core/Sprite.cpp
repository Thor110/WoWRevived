#include "core/Sprite.h"
#include "core/Bytes.h"

#include <algorithm>

namespace wow
{
    Sprite::Sprite(std::vector<uint8_t> bytes) : data(std::move(bytes))
    {
        ByteReader r(data);
        info.width = r.U16();
        info.height = r.U16();
        info.frames = r.U16();
        if (info.frames == 0) throw FormatError("SPR: no frames");
        for (int f = 0; f < info.frames; f++)
        {
            uint32_t frame = U32(6 + size_t(f) * 4);
            if (uint64_t(frame) + uint64_t(info.height) * 4 > data.size()) throw FormatError("SPR: frame offset out of range");
        }
    }

    uint32_t Sprite::U32(size_t at) const
    {
        if (at + 4 > data.size()) throw FormatError("SPR: read out of range");
        return uint32_t(data[at]) | (uint32_t(data[at + 1]) << 8) | (uint32_t(data[at + 2]) << 16) | (uint32_t(data[at + 3]) << 24);
    }

    IndexedImage Sprite::Decode(int frame) const
    {
        IndexedImage image;
        image.width = info.width;
        image.height = info.height;
        image.pixels.assign(size_t(info.width) * info.height, 0);
        frame = std::clamp(frame, 0, int(info.frames) - 1);
        const size_t frameStart = U32(6 + size_t(frame) * 4);

        for (int row = 0; row < info.height; row++)
        {
            uint64_t pos = uint64_t(frameStart) + U32(frameStart + size_t(row) * 4);
            uint8_t* out = image.pixels.data() + size_t(row) * image.width;
            int x = 0;
            while (x < image.width && pos + 1 < data.size())
            {
                uint8_t index = data[size_t(pos)];
                int count = data[size_t(pos) + 1];
                pos += 2;
                if (count == 0) break; // a zero-length run would never finish the row
                int run = std::min(count, image.width - x); // the last run of a row may overshoot, clip it
                if (index) std::fill(out + x, out + x + run, index);
                x += run;
            }
        }
        return image;
    }
}
