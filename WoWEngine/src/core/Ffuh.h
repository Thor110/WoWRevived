// "FFUH" Huffman compression used by many files inside the KAT! archives.
// Reverse engineered from the game's decompression loop (vanilla WoW.exe 0x415A80), see WoWViewer/FfuhDecoder.cs.
//
// Layout: "FFUH", uint32 uncompressed size, uint32 compressed size, uint32 compressed bit count,
// then 256 uint32 symbol frequencies and an LSB-first bit stream (0 = left, 1 = right).
// A bit count of 0 means the whole output is one repeated byte, stored straight after the header.
#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace wow::ffuh
{
    bool IsCompressed(std::span<const uint8_t> data);

    // Returns the data unchanged if it isn't FFUH compressed. Throws FormatError on damaged data.
    std::vector<uint8_t> Decompress(std::span<const uint8_t> data);
}
