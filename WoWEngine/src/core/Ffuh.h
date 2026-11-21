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

    // The game's encoder (0x415520: the counts, 0x415D20(1): the tree as above, the code lengths, the bit total
    // +0x38 = the sum of length x count and the packed size +0x24 = bits / 8, one more for a part byte, plus 4;
    // 0x4157E0: each byte's code root first, LSB first; a single symbol is 0 bits, a packed size of 1 and that byte).
    // The header (0x415720): "FFUH", the size, the packed size, the bits and the 256 counts; the packed bytes follow
    // (so a single symbol's byte is after the counts, where 0x4155B0 / 0x4158E0 read it, not where Decompress does
    // for the archives' files).
    std::vector<uint8_t> Compress(std::span<const uint8_t> data);
}
