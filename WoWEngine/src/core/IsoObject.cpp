#include "core/IsoObject.h"
#include <algorithm>

namespace wow
{
    int IsoObject::Depth(const IobModel& m)
    {
        // ((header 0x16 << 8) x 0x2D4279A3) >> (32 + 6), rounded towards 0, kept as a word
        int32_t a = int32_t(uint32_t(m.field16) << 8);
        int32_t v = int32_t((int64_t(a) * 0x2D4279A3) >> 32) >> 6;
        v += int32_t(uint32_t(v) >> 31);
        return int16_t(v);
    }

    // 0x49DF40(level): the brightness min(255, ftol(the renderer's level x the hour's shade level)); 255 as it's made
    // (0x49E1E0), and again whenever the fog changes the renderer's level (0x49E2C0)
    std::vector<uint8_t> IsoObject::BaseLights(const IobModel& m, const Landscape::Sun& sun, std::span<const uint8_t> levels, int rendererLevel)
    {
        int level = int(double(rendererLevel) * double(sun.shade));
        if (level > 255) level = 255;
        const double scale = double(float(level));
        auto light = [&](const std::array<int8_t, 3>& n) -> uint8_t
        {
            // 0x40CA90 with (-n0, -n2, -n1): the model's y is up
            int v = int(Landscape::SlopeLight(-n[0], -n[2], -n[1], sun.strength, sun.ambient, sun) * scale);
            uint8_t b = uint8_t(v);
            uint8_t l = levels.size() == 256 ? levels[b] : uint8_t(b >> 2);
            return std::max<uint8_t>(l, 8);
        };
        std::vector<uint8_t> out(m.patches.size() * 3, 8);
        for (size_t i = 0; i < m.patches.size(); i++)
        {
            if (m.smooth)
            {
                // a normal for each point: the patch names one for each corner
                for (int k = 0; k < 3; k++)
                {
                    size_t n = m.patches[i].normals[k];
                    if (n < m.normals.size()) out[i * 3 + size_t(k)] = light(m.normals[n]);
                }
            }
            else if (i < m.normals.size())
            {
                uint8_t l = light(m.normals[i]);
                out[i * 3] = out[i * 3 + 1] = out[i * 3 + 2] = l;
            }
        }
        return out;
    }

    void IsoObject::Draw(const IobModel& m, std::span<const uint8_t> lights, int x, int y, const ShadeTable& table,
                         const DitherTables& dither, const TerrainView::Target& target)
    {
        const int count = int(m.patches.size());
        for (int i = 0; i < count; i++)
        {
            const IobPatch& p = m.patches[size_t(i)];
            if (p.offset >= m.patchData.size() || size_t(i) * 3 + 2 >= lights.size()) continue;
            const uint8_t header[4] = { p.width, p.height, p.colour, p.mode };
            const int l[3] = { lights[size_t(i) * 3], lights[size_t(i) * 3 + 1], lights[size_t(i) * 3 + 2] };
            // the patch counter (4CA0E0) counts down from the number of patches
            TerrainView::DrawPatch(header, m.patchData.data() + p.offset, int(p.seed) + (count - i), x + p.x, y + p.y, l,
                                   0x3400, p.mode == 1, target, dither, table);
        }
    }

    void IsoObject::AddPointLight(const IobModel& m, std::vector<uint8_t>& lights, int dx, int dy, int dz, int intensity,
                                  int range, int inner, int cap)
    {
        if (cap < 8) cap = 8;
        const float times = float(intensity);
        for (size_t i = 0; i < m.patches.size() && i * 3 + 2 < lights.size(); i++)
        {
            if (m.smooth)
            {
                for (int k = 0; k < 3; k++)
                {
                    const size_t n = m.patches[i].normals[k];
                    if (n >= m.normals.size()) continue;
                    const auto& v = m.normals[n];
                    const double add = TerrainView::PointLight(dx, dy, dz, v[0], v[2], v[1], range, inner) * double(times);
                    int l = int(add + double(lights[i * 3 + size_t(k)]));
                    if (l > cap) l = cap;
                    lights[i * 3 + size_t(k)] = uint8_t(l);
                }
            }
            else if (i < m.normals.size())
            {
                const auto& v = m.normals[i];
                const int add = int(TerrainView::PointLight(dx, dy, dz, v[0], v[2], v[1], range, inner) * double(times));
                for (int k = 0; k < 3; k++)
                {
                    int l = int(lights[i * 3 + size_t(k)]) + add;
                    if (l > cap) l = cap;
                    lights[i * 3 + size_t(k)] = uint8_t(l);
                }
            }
        }
    }
}
