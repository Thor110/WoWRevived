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

    std::vector<uint8_t> Compress(std::span<const uint8_t> data)
    {
        uint32_t frequencies[256] = {};
        for (uint8_t b : data) frequencies[b]++;
        std::vector<Node> nodes;
        uint32_t bits = 0, packed = 1;
        std::vector<std::vector<uint8_t>> codes(256);
        int distinct = 0;
        for (uint32_t f : frequencies) distinct += f != 0;
        if (distinct > 1)
        {
            nodes.reserve(512);
            const int root = BuildTree(frequencies, nodes);
            // each leaf's code, root first
            std::vector<std::pair<int, std::vector<uint8_t>>> stack{ { root, {} } };
            while (!stack.empty())
            {
                auto [n, code] = std::move(stack.back());
                stack.pop_back();
                if (nodes[size_t(n)].left < 0) { codes[nodes[size_t(n)].symbol] = code; continue; }
                auto l = code, r = code;
                l.push_back(0);
                r.push_back(1);
                stack.push_back({ nodes[size_t(n)].left, std::move(l) });
                stack.push_back({ nodes[size_t(n)].right, std::move(r) });
            }
            for (int s = 0; s < 256; s++) bits += uint32_t(codes[size_t(s)].size()) * frequencies[s];
            packed = bits / 8 + ((bits & 7) ? 1 : 0) + 4;
        }
        std::vector<uint8_t> out(16 + 1024 + packed, 0);
        auto put = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) out[at + size_t(i)] = uint8_t(v >> (8 * i)); };
        out[0] = 'F'; out[1] = 'F'; out[2] = 'U'; out[3] = 'H';
        put(4, uint32_t(data.size()));
        put(8, packed);
        put(12, bits);
        for (int s = 0; s < 256; s++) put(16 + size_t(s) * 4, frequencies[s]);
        uint8_t* stream = &out[16 + 1024];
        if (bits == 0)
        {
            if (!data.empty()) stream[0] = data[0];
            return out;
        }
        size_t pos = 0;
        for (uint8_t b : data)
            for (uint8_t bit : codes[b])
            {
                if (bit) stream[pos >> 3] |= uint8_t(1u << (pos & 7));
                pos++;
            }
        return out;
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
