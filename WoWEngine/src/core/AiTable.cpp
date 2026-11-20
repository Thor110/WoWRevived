#include "core/AiTable.h"
#include "core/Bytes.h"

namespace wow
{
    AiTable AiTable::Load(std::span<const uint8_t> data)
    {
        AiTable table;
        size_t pos = 0;
        auto u16 = [&]() {
            if (pos + 2 > data.size()) throw FormatError("AI.ojd: record runs past the end of the file");
            uint16_t v = uint16_t(data[pos] | (data[pos + 1] << 8));
            pos += 2;
            return v;
        };
        int id = 0, side = 0, list = 0;
        while (pos < data.size())
        {
            if (pos + 5 > data.size()) throw FormatError("AI.ojd: record runs past the end of the file");
            pos += 3;
            int op = int16_t(u16());
            switch (op)
            {
            case 0: id = u16(); table.entries[id]; break;
            case 1: table.entries[id].value = int16_t(u16()); break;
            case 2: table.entries[id].weight = int16_t(u16()); break;
            case 3: side = u16() == 1 ? 1 : 0; break;
            case 4: list = int16_t(u16()); break;
            case 5: case 6:
            {
                AiRule rule;
                rule.count = op == 5 ? 3 : 5;
                for (int i = 0; i < rule.count; i++) rule.words[i] = u16();
                table.rules[side][list].push_back(rule);
                break;
            }
            default: break; // the loader ignores other opcodes, which have no arguments
            }
        }
        return table;
    }
}
