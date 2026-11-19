#include "core/ShadeTable.h"
#include "core/Bytes.h"

#include <algorithm>

namespace wow
{
    ShadeTable ShadeTable::LoadShh(std::span<const uint8_t> data)
    {
        ByteReader r(data);
        ShadeTable t;
        t.levels = r.U8();
        if (t.levels == 0) throw FormatError("shade table has no levels");
        t.colours.resize(size_t(t.levels) * 256);
        for (uint32_t& c : t.colours)
        {
            uint16_t v = r.U16();
            uint32_t red = (v >> 11) & 31, green = (v >> 5) & 63, blue = v & 31;
            red = (red << 3) | (red >> 2);
            green = (green << 2) | (green >> 4);
            blue = (blue << 3) | (blue >> 2);
            c = (red << 16) | (green << 8) | blue;
        }
        return t;
    }

    const uint32_t* ShadeTable::Level(int level) const
    {
        level = std::clamp(level, 0, levels - 1);
        return colours.data() + size_t(level) * 256;
    }
}
