// MODERN (the HUD scale, docs/MODERN.md): the panels drawn at another size than the picture they lie on.
// The picture is scaled into a buffer of the panels' size, the panels are drawn on that as they are drawn in the
// game (so their see-through parts mix with what is under them), and every pixel the panels changed is put back
// on the picture, scaled; the rest of the picture is left as it was, untouched by the scaling.
#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace wow
{
    // one picture into another of a different size: each pixel the nearest when the target is a whole number of
    // times the source, else the four around it mixed
    inline void ScaleInto(const uint32_t* src, int sw, int sh, uint32_t* dst, int dw, int dh)
    {
        if (sw == dw && sh == dh) { std::copy(src, src + size_t(sw) * size_t(sh), dst); return; }
        if (dw % sw == 0 && dh % sh == 0 && dw / sw == dh / sh)
        {
            const int n = dw / sw;
            for (int y = 0; y < dh; y++)
            {
                const uint32_t* in = src + size_t(y / n) * size_t(sw);
                uint32_t* out = dst + size_t(y) * size_t(dw);
                for (int x = 0; x < dw; x++) out[x] = in[x / n];
            }
            return;
        }
        // 16.16 steps through the source, sampled at the pixels' middles
        const int64_t stepX = (int64_t(sw) << 16) / dw, stepY = (int64_t(sh) << 16) / dh;
        for (int y = 0; y < dh; y++)
        {
            int64_t fy = stepY * y + stepY / 2 - 0x8000;
            if (fy < 0) fy = 0;
            const int y0 = std::min(sh - 1, int(fy >> 16)), y1 = std::min(sh - 1, y0 + 1);
            const uint32_t wy = uint32_t(fy & 0xFFFF) >> 8;
            const uint32_t* r0 = src + size_t(y0) * size_t(sw);
            const uint32_t* r1 = src + size_t(y1) * size_t(sw);
            uint32_t* out = dst + size_t(y) * size_t(dw);
            for (int x = 0; x < dw; x++)
            {
                int64_t fx = stepX * x + stepX / 2 - 0x8000;
                if (fx < 0) fx = 0;
                const int x0 = std::min(sw - 1, int(fx >> 16)), x1 = std::min(sw - 1, x0 + 1);
                const uint32_t wx = uint32_t(fx & 0xFFFF) >> 8;
                auto mix = [&](int shift) {
                    const uint32_t a = (r0[x0] >> shift) & 0xFF, b = (r0[x1] >> shift) & 0xFF;
                    const uint32_t c = (r1[x0] >> shift) & 0xFF, d = (r1[x1] >> shift) & 0xFF;
                    const uint32_t top = a * (256 - wx) + b * wx, bottom = c * (256 - wx) + d * wx;
                    return ((top * (256 - wy) + bottom * wy) >> 16) << shift;
                };
                out[x] = 0xFF000000u | mix(16) | mix(8) | mix(0);
            }
        }
    }

    // picture: pw x ph, already holding what lies under the panels. draw(pixels, w, h) draws the panels on a
    // w x h picture. under / over are buffers kept between frames.
    template <typename DrawPanels>
    void LayPanels(uint32_t* picture, int pw, int ph, int panelW, int panelH, std::vector<uint32_t>& under, std::vector<uint32_t>& over, DrawPanels draw)
    {
        if (panelW == pw && panelH == ph) { draw(picture, pw, ph); return; }
        const size_t count = size_t(panelW) * size_t(panelH);
        under.resize(count);
        ScaleInto(picture, pw, ph, under.data(), panelW, panelH);
        over = under;
        draw(over.data(), panelW, panelH);
        // which of the panels' pixels changed, widened by one so a scaled edge takes its neighbours with it
        std::vector<uint8_t> changed(count, 0);
        bool any = false;
        for (int y = 0; y < panelH; y++)
            for (int x = 0; x < panelW; x++)
                if (over[size_t(y) * size_t(panelW) + size_t(x)] != under[size_t(y) * size_t(panelW) + size_t(x)])
                {
                    any = true;
                    for (int j = std::max(0, y - 1); j <= std::min(panelH - 1, y + 1); j++)
                        for (int i = std::max(0, x - 1); i <= std::min(panelW - 1, x + 1); i++) changed[size_t(j) * size_t(panelW) + size_t(i)] = 1;
                }
        if (!any) return;
        under.resize(size_t(pw) * size_t(ph));          // (reused for the scaled panels)
        ScaleInto(over.data(), panelW, panelH, under.data(), pw, ph);
        for (int y = 0; y < ph; y++)
        {
            const size_t hy = size_t(int64_t(y) * panelH / ph) * size_t(panelW);
            uint32_t* out = picture + size_t(y) * size_t(pw);
            const uint32_t* in = under.data() + size_t(y) * size_t(pw);
            for (int x = 0; x < pw; x++)
                if (changed[hy + size_t(int64_t(x) * panelW / pw)]) out[x] = in[x];
        }
    }
}
