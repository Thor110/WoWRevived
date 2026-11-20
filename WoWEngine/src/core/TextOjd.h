// TEXT.ojd: every string the game shows, read the way the game does it (vanilla 0x414410).
//
// Same record stream as OBJ.ojd: 3 header bytes, int16 record type, then:
//   types 0, 1, 2   uint16 id, uint16 len, string[len]   0 = Martian games only, 1 = Human only, 2 = both
//   type 3          uint16 a, uint16 b                    string a is the same as string b
//   type 4          uint16 a, uint16 b                    string a's speech: SFX.ojd sound b (Extra), played by
//                                                         PlayTextSpeech (0x414700) at the listener
// "\n" and "\t" in the text are escapes. The game also runs every character through Latin1ToFont (Charset.h)
// at load time; here the text stays Latin-1 and the font code does that conversion when drawing.
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace wow
{
    class TextTable
    {
    public:
        static constexpr size_t MaxIds = 0x29A8 / 8; // the game allocates room for 1333 ids

        // human picks which side's strings are kept, as the game only loads the side being played.
        // Throws FormatError if a record runs past the end of the data.
        static TextTable Load(std::span<const uint8_t> data, bool human);

        // The string for an id, or an empty string if there isn't one.
        const std::string& Get(int id) const;
        bool Has(int id) const { return id >= 0 && size_t(id) < entries.size() && entries[size_t(id)].present; }
        int Extra(int id) const { return Has(id) ? entries[size_t(id)].extra : 0; }
        size_t Count() const;

    private:
        struct Entry
        {
            std::string text;
            int extra = 0;
            bool present = false;
        };
        std::vector<Entry> entries = std::vector<Entry>(MaxIds);
    };
}
