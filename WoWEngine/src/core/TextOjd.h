// TEXT.ojd: every string the game shows, in one of five languages (ISO-8859-1).
// Ported from WoWViewer/Reusables.cs.
//   header up to 0x289, then per entry: 4 bytes, uint8 faction (0 Martian, 1 Human, 2 UI), 1 byte,
//   uint16 id, uint16 length (including the terminating null), the string bytes.
//   Line breaks are stored as a literal backslash-n.
// The retail files have 1396 entries; WoW Revived's patched files add a 1397th (the Credits button).
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace wow
{
    struct TextEntry
    {
        uint16_t id = 0;
        uint8_t faction = 0;
        std::string text; // ISO-8859-1, line breaks turned into '\n'
    };

    class TextOjd
    {
    public:
        static constexpr size_t FirstEntry = 0x289;
        static constexpr size_t MaxEntries = 1397;

        // Reads up to MaxEntries entries, stopping early if the data runs out. Throws FormatError if an entry is cut short.
        static std::vector<TextEntry> Load(std::span<const uint8_t> data);
    };
}
