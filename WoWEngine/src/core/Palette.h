// .PAL files (inside DAT\Dat.wow): 66304 bytes.
//   0..767       256 colours, 3 bytes each (R, G, B), 6-bit VGA values 0..63
//   768..66303   256 x 256 blend table used for translucent effects (smoke, glass...)
// Colour 0 is transparent in sprites.
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace wow
{
    struct Rgb { uint8_t r = 0, g = 0, b = 0; };

    struct Palette
    {
        static constexpr size_t FileSize = 66304;
        static constexpr size_t BlendOffset = 768;

        std::array<Rgb, 256> colours{};
        std::vector<uint8_t> blend; // 65536 bytes, blend[a * 256 + b], empty if the file didn't have one

        // Throws FormatError if the data is smaller than the colour table.
        static Palette Load(std::span<const uint8_t> data);
    };
}
