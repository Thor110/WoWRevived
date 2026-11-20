// One row of an NV12 picture (a Y plane, then U and V interleaved at half size) turned into 0xRRGGBB pixels,
// taking every shown pixel from the source pixel it lands on: for the .mp4 movies, whose decoder gives NV12, so only
// the pixels shown (at most 640 x 480) are ever converted. Limited-range BT.709 (HD) or BT.601.
#pragma once
#include <algorithm>
#include <cstdint>

namespace wow
{
    // luma: the source row's Y samples; chroma: its U, V pairs (row sy / 2 of the colour plane). left: the picture's
    // first column in the decoded frame; sourceWidth its width there; out: width pixels.
    inline void Nv12Row(const uint8_t* luma, const uint8_t* chroma, int left, int sourceWidth, uint32_t* out, int width, bool bt709)
    {
        for (int x = 0; x < width; x++)
        {
            int sx = left + int(int64_t(x) * sourceWidth / width);
            int c = int(luma[sx]) - 16, d = int(chroma[sx & ~1]) - 128, e = int(chroma[(sx & ~1) + 1]) - 128;
            int r, g, b;
            if (bt709) { r = (298 * c + 459 * e + 128) >> 8; g = (298 * c - 55 * d - 136 * e + 128) >> 8; b = (298 * c + 541 * d + 128) >> 8; }
            else { r = (298 * c + 409 * e + 128) >> 8; g = (298 * c - 100 * d - 208 * e + 128) >> 8; b = (298 * c + 516 * d + 128) >> 8; }
            out[x] = uint32_t(std::clamp(r, 0, 255)) << 16 | uint32_t(std::clamp(g, 0, 255)) << 8 | uint32_t(std::clamp(b, 0, 255));
        }
    }
}
