#include "core/TextOjd.h"
#include "core/Bytes.h"

namespace wow
{
    namespace
    {
        std::string Unescape(const std::string& raw)
        {
            std::string out;
            out.reserve(raw.size());
            for (size_t i = 0; i < raw.size(); i++)
            {
                if (raw[i] == '\\' && i + 1 < raw.size() && (raw[i + 1] == 'n' || raw[i + 1] == 't'))
                {
                    out += raw[i + 1] == 'n' ? '\n' : '\t';
                    i++;
                }
                else out += raw[i];
            }
            return out;
        }
    }

    TextTable TextTable::Load(std::span<const uint8_t> data, bool human)
    {
        TextTable table;
        ByteReader r(data);
        while (r.Remaining() > 0)
        {
            if (r.Remaining() < 5)
            {
                // the files end with a single 0x00 after the last record
                break;
            }
            r.Skip(3);
            int type = int16_t(r.U16());
            if (type < 0 || type > 4) throw FormatError("TEXT.ojd: unknown record type " + std::to_string(type));
            uint16_t a = r.U16();
            uint16_t b = r.U16();
            if (a >= MaxIds) throw FormatError("TEXT.ojd: string id out of range");
            if (type <= 2)
            {
                std::string raw = r.Name(b); // b is the length, including the null
                bool wanted = type == 2 || (type == 1 && human) || (type == 0 && !human);
                if (wanted)
                {
                    table.entries[a].text = Unescape(raw);
                    table.entries[a].present = true;
                }
            }
            else if (type == 3)
            {
                if (b >= MaxIds) throw FormatError("TEXT.ojd: string id out of range");
                table.entries[a].text = table.entries[b].text;
                table.entries[a].present = table.entries[b].present;
            }
            else table.entries[a].extra = b;
        }
        return table;
    }

    const std::string& TextTable::Get(int id) const
    {
        static const std::string empty;
        return Has(id) ? entries[size_t(id)].text : empty;
    }

    size_t TextTable::Count() const
    {
        size_t n = 0;
        for (const Entry& e : entries) n += e.present;
        return n;
    }
}
