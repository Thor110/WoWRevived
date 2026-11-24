#include "core/ShadeTable.h"

#include <algorithm>
#include <cmath>
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

    ShadeTable ShadeTable::HueRotated(float degrees) const
    {
        ShadeTable t = *this;
        const float turn = degrees / 60.0f;
        for (uint32_t& c : t.colours)
        {
            const float r = float((c >> 16) & 255) / 255.0f, g = float((c >> 8) & 255) / 255.0f, b = float(c & 255) / 255.0f;
            const float mx = std::max({ r, g, b }), mn = std::min({ r, g, b }), d = mx - mn;
            if (d <= 0.0f) continue;                          // a grey: no hue to turn
            float h = mx == r ? std::fmod((g - b) / d, 6.0f) : mx == g ? (b - r) / d + 2.0f : (r - g) / d + 4.0f;
            h = std::fmod(h + turn + 12.0f, 6.0f);
            const float x = d * (1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f));
            float rr = 0, gg = 0, bb = 0;
            switch (int(h))
            {
            case 0: rr = d; gg = x; break;
            case 1: rr = x; gg = d; break;
            case 2: gg = d; bb = x; break;
            case 3: gg = x; bb = d; break;
            case 4: rr = x; bb = d; break;
            default: rr = d; bb = x; break;
            }
            auto byte = [&](float v) { return uint32_t(std::clamp((v + mn) * 255.0f + 0.5f, 0.0f, 255.0f)); };
            c = byte(rr) << 16 | byte(gg) << 8 | byte(bb);
        }
        return t;
    }

    const uint32_t* ShadeTable::Level(int level) const
    {
        level = std::clamp(level, 0, levels - 1);
        return colours.data() + size_t(level) * 256;
    }
}
