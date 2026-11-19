#include "core/Palette.h"
#include "core/Bytes.h"

namespace wow
{
    Palette Palette::Load(std::span<const uint8_t> data)
    {
        if (data.size() < BlendOffset) throw FormatError("PAL: too small for a colour table");
        Palette p;
        for (size_t i = 0; i < 256; i++)
        {
            // 6-bit VGA DAC values, scaled the same way the viewer does (x4)
            p.colours[i] = { uint8_t((data[i * 3] & 63) * 4), uint8_t((data[i * 3 + 1] & 63) * 4), uint8_t((data[i * 3 + 2] & 63) * 4) };
        }
        if (data.size() >= FileSize) p.blend.assign(data.begin() + BlendOffset, data.begin() + FileSize);
        return p;
    }
}
