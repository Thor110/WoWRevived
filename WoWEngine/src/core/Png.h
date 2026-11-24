// MODERN (docs/TEXTURES.md): a PNG reader for the texture packs, the engine's own (no library): the zlib stream
// inflated (RFC 1950 / 1951), the scanlines unfiltered, every colour type at 8 bits a sample and the palettes at
// 1, 2, 4 and 8 (16-bit samples are taken by their high byte). Interlaced (Adam7) pictures aren't read.
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace wow
{
    struct PngImage
    {
        int width = 0, height = 0;
        std::vector<uint32_t> rgba;                 // each pixel 0xAARRGGBB
        // a palette picture (colour type 3) keeps its indices and its palette (0xAARRGGBB, tRNS's alphas)
        bool paletted = false;
        std::vector<uint8_t> indices;
        std::vector<uint32_t> palette;
    };

    // Throws FormatError for data that isn't a PNG it can read.
    PngImage DecodePng(std::span<const uint8_t> data);
    // The zlib stream's data (the inflate), for the reader and the tests.
    std::vector<uint8_t> Inflate(std::span<const uint8_t> zlib);
}
