#include "core/Ffuh.h"
#include "core/Bytes.h"

#include <algorithm>

namespace wow::ffuh
{
    namespace
    {
        constexpr uint32_t MaxOutput = 256u * 1024u * 1024u; // sanity limit, the largest game file is a few MB

        struct Node
        {
            uint32_t freq = 0;
            int left = -1, right = -1; // child node indices, -1 for a leaf
            uint8_t symbol = 0;
        };

        // Must match the game exactly or the bit stream decodes to garbage: leaves in symbol order, stable sorted
        // by frequency, then repeatedly join the two front nodes and insert the parent before the first node with a
        // strictly higher frequency.
        int BuildTree(const uint32_t (&frequencies)[256], std::vector<Node>& nodes)
        {
            std::vector<int> queue;
            for (int symbol = 0; symbol < 256; symbol++)
            {
                if (frequencies[symbol] == 0) continue;
                nodes.push_back({ frequencies[symbol], -1, -1, uint8_t(symbol) });
                queue.push_back(int(nodes.size()) - 1);
            }
            if (queue.empty()) throw FormatError("FFUH: empty frequency table");
            std::stable_sort(queue.begin(), queue.end(), [&](int a, int b) { return nodes[a].freq < nodes[b].freq; });

            while (queue.size() > 1)
            {
                int left = queue[0], right = queue[1];
                queue.erase(queue.begin(), queue.begin() + 2);
                Node parent{ nodes[left].freq + nodes[right].freq, left, right, 0 };
                nodes.push_back(parent);
                int index = int(nodes.size()) - 1;
                auto at = std::find_if(queue.begin(), queue.end(), [&](int n) { return parent.freq < nodes[n].freq; });
                queue.insert(at, index);
            }
            return queue[0];
        }
    }

    bool IsCompressed(std::span<const uint8_t> data)
    {
        return data.size() >= 4 && data[0] == 'F' && data[1] == 'F' && data[2] == 'U' && data[3] == 'H';
    }

    std::vector<uint8_t> Decompress(std::span<const uint8_t> data)
    {
        if (!IsCompressed(data)) return std::vector<uint8_t>(data.begin(), data.end());

        ByteReader r(data);
        r.Skip(4);
        uint32_t size = r.U32();
        r.U32(); // compressed size
        uint32_t bitCount = r.U32();
        if (size > MaxOutput) throw FormatError("FFUH: uncompressed size is implausibly large");

        if (bitCount == 0) return std::vector<uint8_t>(size, r.U8());

        uint32_t frequencies[256];
        for (uint32_t& f : frequencies) f = r.U32();

        std::vector<Node> nodes;
        nodes.reserve(512);
        int root = BuildTree(frequencies, nodes);

        auto bits = r.Bytes(r.Remaining());
        size_t totalBits = bits.size() * 8;
        size_t bitPos = 0;
        std::vector<uint8_t> out(size);
        for (uint32_t i = 0; i < size; i++)
        {
            int n = root;
            while (nodes[n].left >= 0)
            {
                if (bitPos >= totalBits) throw FormatError("FFUH: ran out of bits");
                int bit = (bits[bitPos >> 3] >> (bitPos & 7)) & 1;
                bitPos++;
                n = bit ? nodes[n].right : nodes[n].left;
            }
            out[i] = nodes[n].symbol;
        }
        return out;
    }
}
