#include "core/TextOjd.h"
#include "core/Bytes.h"

namespace wow
{
    std::vector<TextEntry> TextOjd::Load(std::span<const uint8_t> data)
    {
        if (data.size() < FirstEntry) throw FormatError("TEXT.ojd: too small");
        std::vector<TextEntry> entries;
        size_t offset = FirstEntry;
        // each entry is 10 header bytes plus the string; the file ends with a single 0x00
        while (entries.size() < MaxEntries && offset + 10 <= data.size())
        {
            uint16_t length = uint16_t(data[offset + 8] | (data[offset + 9] << 8));
            if (length == 0) break;
            if (offset + 10 + length - 1 > data.size()) throw FormatError("TEXT.ojd: entry is cut short");
            TextEntry e;
            e.faction = data[offset + 4];
            e.id = uint16_t(data[offset + 6] | (data[offset + 7] << 8));
            const char* s = reinterpret_cast<const char*>(data.data() + offset + 10);
            std::string raw(s, length - 1);
            for (size_t i = 0; i < raw.size(); i++)
            {
                if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == 'n') { e.text += '\n'; i++; }
                else e.text += raw[i];
            }
            entries.push_back(std::move(e));
            offset += size_t(length) + 9; // same stride as the viewer: length includes the null
        }
        return entries;
    }
}
