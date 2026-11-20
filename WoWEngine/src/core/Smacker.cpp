#include "core/Smacker.h"

#include "core/Bytes.h"

#include <algorithm>
#include <cstring>

namespace wow
{
    struct Smacker::BitReader
    {
        std::span<const uint8_t> data;
        size_t bit = 0;

        bool Bit()
        {
            size_t byte = bit >> 3;
            if (byte >= data.size()) { bit++; return false; } // past the end reads as 0, as the decoder's reader does
            bool b = (data[byte] >> (bit & 7)) & 1;
            bit++;
            return b;
        }
        uint32_t Bits(int n)
        {
            uint32_t v = 0;
            for (int i = 0; i < n; i++) v |= uint32_t(Bit()) << i;
            return v;
        }
    };

    namespace
    {
        // A small Huffman tree of 8-bit values: a 1 bit is a node (its 0 branch, then its 1 branch), a 0 bit a
        // leaf followed by its 8-bit value. A tree that's a single leaf takes no bits to decode.
        struct SmallTree
        {
            struct NodeEntry { int16_t zero = -1, one = -1; int value = 0; };
            std::vector<NodeEntry> nodes;
            bool present = false;
        };

        template <typename Reader>
        int ReadSmall(Reader& bits, SmallTree& tree, int depth)
        {
            if (depth > 32) throw FormatError("Smacker: Huffman tree too deep");
            int index = int(tree.nodes.size());
            tree.nodes.push_back({});
            if (!bits.Bit())
            {
                tree.nodes[size_t(index)].value = int(bits.Bits(8));
                return index;
            }
            int zero = ReadSmall(bits, tree, depth + 1);
            int one = ReadSmall(bits, tree, depth + 1);
            tree.nodes[size_t(index)].zero = int16_t(zero);
            tree.nodes[size_t(index)].one = int16_t(one);
            return index;
        }

        // Reads a small tree that's there when its first bit is set; the bit after it is padding.
        template <typename Reader>
        SmallTree ReadSmallTree(Reader& bits)
        {
            SmallTree tree;
            if (bits.Bit())
            {
                tree.present = true;
                ReadSmall(bits, tree, 0);
                bits.Bit();
            }
            return tree;
        }

        template <typename Reader>
        int DecodeSmall(Reader& bits, const SmallTree& tree)
        {
            if (tree.nodes.empty()) return 0;
            size_t i = 0;
            while (tree.nodes[i].zero >= 0) i = size_t(bits.Bit() ? tree.nodes[i].one : tree.nodes[i].zero);
            return tree.nodes[i].value;
        }

        // The palette's 6-bit levels as 8-bit ones
        uint8_t Level(int six) { return uint8_t((six * 255 + 31) / 63); }

        // How many blocks a video chunk covers, by the 6 bits above its type
        constexpr int BlockRuns[64] = {
            1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
            23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
            45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 128, 256, 512, 1024, 2048,
        };
    }

    void Smacker::ReadBigTree(BitReader& bits, BigTree& tree)
    {
        tree.table.clear();
        if (!bits.Bit())
        {
            // no tree: everything decodes as 0
            tree.table = { 0, 0, 0, 0 };
            tree.last[0] = 1; tree.last[1] = 2; tree.last[2] = 3;
            return;
        }
        SmallTree low = ReadSmallTree(bits), high = ReadSmallTree(bits);
        int escapes[3];
        for (int& e : escapes) e = int(bits.Bits(16));
        int last[3] = { -1, -1, -1 };

        // the tree itself: nodes and leaves, a leaf's value being a low byte and a high byte from the small trees
        struct Builder
        {
            BitReader& bits;
            BigTree& tree;
            const SmallTree& low;
            const SmallTree& high;
            const int* escapes;
            int* last;
            int Read(int depth)
            {
                if (depth > 32) throw FormatError("Smacker: Huffman tree too deep");
                if (!bits.Bit())
                {
                    int value = DecodeSmall(bits, low) | (DecodeSmall(bits, high) << 8);
                    for (int i = 0; i < 3; i++)
                        if (value == escapes[i]) { last[i] = int(tree.table.size()); value = 0; break; }
                    tree.table.push_back(value);
                    return 1;
                }
                size_t at = tree.table.size();
                tree.table.push_back(0);
                int left = Read(depth + 1);
                tree.table[at] = BigTree::Node | left;
                int right = Read(depth + 1);
                return left + 1 + right;
            }
        } builder{ bits, tree, low, high, escapes, last };
        builder.Read(0);
        bits.Bit(); // padding
        for (int i = 0; i < 3; i++)
        {
            if (last[i] < 0) { last[i] = int(tree.table.size()); tree.table.push_back(0); }
            tree.last[i] = last[i];
        }
    }

    int Smacker::Get(BitReader& bits, BigTree& tree)
    {
        size_t i = 0;
        while (tree.table[i] & BigTree::Node)
        {
            if (bits.Bit()) i += size_t(tree.table[i] & ~BigTree::Node);
            i++;
            if (i >= tree.table.size()) throw FormatError("Smacker: bad Huffman code");
        }
        int value = tree.table[i];
        // the three most recent values move along: an escape code gives one of them back
        if (value != tree.table[size_t(tree.last[0])])
        {
            tree.table[size_t(tree.last[2])] = tree.table[size_t(tree.last[1])];
            tree.table[size_t(tree.last[1])] = tree.table[size_t(tree.last[0])];
            tree.table[size_t(tree.last[0])] = value;
        }
        return value;
    }

    Smacker::Smacker(const std::filesystem::path& path) : file(path, std::ios::binary)
    {
        if (!file) throw FormatError("Smacker: can't open " + path.string());
        file.seekg(0, std::ios::end);
        fileSize = uint64_t(file.tellg());
        file.seekg(0);
        auto read = [&](std::vector<uint8_t>& into, size_t n) {
            into.resize(n);
            if (n && !file.read(reinterpret_cast<char*>(into.data()), std::streamsize(n))) throw FormatError("Smacker: file cut short");
        };
        std::vector<uint8_t> head;
        read(head, 104);
        if (std::memcmp(head.data(), "SMK2", 4) == 0) version = 2;
        else if (std::memcmp(head.data(), "SMK4", 4) == 0) version = 4;
        else throw FormatError("Smacker: not a Smacker movie");
        ByteReader r(head);
        r.Skip(4);
        width = int(r.U32());
        height = int(r.U32());
        frameCount = int(r.U32());
        rate = r.I32();
        flags = r.U32();
        audioSize = r.U32();
        r.Skip(6 * 4); // the other tracks' audio buffer sizes
        uint32_t treeSize = r.U32();
        r.Skip(4 * 4); // the trees' sizes
        for (uint32_t& a : audioRate) a = r.U32();
        r.Skip(4);
        if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || frameCount <= 0 || frameCount > 1000000 ||
            treeSize > fileSize)
            throw FormatError("Smacker: bad header");
        size_t stored = size_t(frameCount) + ((flags & RingFrame) ? 1 : 0);
        std::vector<uint8_t> table;
        read(table, stored * 5);
        frameSizes.resize(stored);
        frameTypes.resize(stored);
        for (size_t i = 0; i < stored; i++)
        {
            frameSizes[i] = uint32_t(table[i * 4]) | uint32_t(table[i * 4 + 1]) << 8 | uint32_t(table[i * 4 + 2]) << 16 |
                            uint32_t(table[i * 4 + 3]) << 24;
            frameTypes[i] = table[stored * 4 + i];
        }
        std::vector<uint8_t> trees;
        read(trees, treeSize);
        BitReader bits{ trees };
        ReadBigTree(bits, mmap);
        ReadBigTree(bits, mclr);
        ReadBigTree(bits, full);
        ReadBigTree(bits, type);
        frameStart = 104 + stored * 5 + treeSize;
        offset = frameStart;
        pixels.assign(size_t(width) * size_t(height), 0);
    }

    double Smacker::FrameSeconds() const
    {
        // positive: milliseconds; negative: tens of microseconds; 0: 10 a second
        if (rate > 0) return rate / 1000.0;
        if (rate < 0) return -rate / 100000.0;
        return 0.1;
    }

    bool Smacker::DecodeFrame(std::vector<int16_t>& audio)
    {
        if (next >= frameCount) return false;
        size_t size = frameSizes[size_t(next)] & ~3u;
        uint8_t kind = frameTypes[size_t(next)];
        if (offset + size > fileSize) throw FormatError("Smacker: frame runs past the end of the file");
        frameData.resize(size);
        file.clear();
        file.seekg(std::streamoff(offset));
        if (size && !file.read(reinterpret_cast<char*>(frameData.data()), std::streamsize(size)))
            throw FormatError("Smacker: can't read a frame");
        std::span<const uint8_t> frame(frameData);
        offset += size;
        next++;

        size_t at = 0;
        if (kind & 1)
        {
            if (frame.empty()) throw FormatError("Smacker: palette chunk missing");
            size_t length = size_t(frame[0]) * 4;
            if (length == 0 || length > frame.size()) throw FormatError("Smacker: bad palette chunk");
            DecodePalette(frame.subspan(1, length - 1));
            at = length;
        }
        for (int track = 0; track < 7; track++)
        {
            if (!(kind & (2 << track))) continue;
            if (at + 4 > frame.size()) throw FormatError("Smacker: audio chunk cut short");
            uint32_t length = uint32_t(frame[at]) | uint32_t(frame[at + 1]) << 8 | uint32_t(frame[at + 2]) << 16 | uint32_t(frame[at + 3]) << 24;
            if (length < 4 || at + length > frame.size()) throw FormatError("Smacker: bad audio chunk");
            if (track == 0) DecodeAudio(frame.subspan(at + 4, length - 4), audio);
            at += length;
        }
        DecodeVideo(frame.subspan(at));
        return true;
    }

    void Smacker::DecodePalette(std::span<const uint8_t> p)
    {
        // changes to the last palette: runs kept, runs copied from elsewhere in it, and new colours
        std::array<uint32_t, 256> old = palette;
        size_t i = 0;
        int entry = 0;
        while (entry < 256 && i < p.size())
        {
            uint8_t t = p[i++];
            if (t & 0x80) entry += (t & 0x7F) + 1;
            else if (t & 0x40)
            {
                if (i >= p.size()) break;
                int from = p[i++];
                int count = (t & 0x3F) + 1;
                while (count-- > 0 && entry < 256 && from < 256) palette[size_t(entry++)] = old[size_t(from++)];
            }
            else
            {
                if (i + 2 > p.size()) break;
                uint8_t r = Level(t), g = Level(p[i] & 0x3F), b = Level(p[i + 1] & 0x3F);
                i += 2;
                palette[size_t(entry++)] = uint32_t(r) << 16 | uint32_t(g) << 8 | b;
            }
        }
    }

    void Smacker::DecodeVideo(std::span<const uint8_t> data)
    {
        BitReader bits{ data };
        for (BigTree* t : { &mmap, &mclr, &full, &type })
            for (int l : t->last) t->table[size_t(l)] = 0;
        const int across = width / 4, blocks = across * (height / 4);
        const size_t stride = size_t(width);
        auto blockAt = [&](int b) { return pixels.data() + size_t(b / across) * stride * 4 + size_t(b % across) * 4; };
        int b = 0;
        while (b < blocks)
        {
            int t = Get(bits, type);
            int run = BlockRuns[(t >> 2) & 0x3F];
            switch (t & 3)
            {
            case 1: // full: every pixel
            {
                int mode = 0;
                if (version == 4)
                {
                    if (bits.Bit()) mode = 1;
                    else if (bits.Bit()) mode = 2;
                }
                for (; run > 0 && b < blocks; run--, b++)
                {
                    uint8_t* out = blockAt(b);
                    if (mode == 0)
                        for (int row = 0; row < 4; row++, out += stride)
                        {
                            int pix = Get(bits, full);
                            out[2] = uint8_t(pix); out[3] = uint8_t(pix >> 8);
                            pix = Get(bits, full);
                            out[0] = uint8_t(pix); out[1] = uint8_t(pix >> 8);
                        }
                    else if (mode == 1) // 2 x 2 pixels: each value's low byte on one line, high byte on the next
                        for (int row = 0; row < 2; row++, out += 2 * stride)
                        {
                            int pix = Get(bits, full);
                            out[2] = out[3] = uint8_t(pix);
                            out[2 + stride] = out[3 + stride] = uint8_t(pix >> 8);
                            pix = Get(bits, full);
                            out[0] = out[1] = uint8_t(pix);
                            out[stride] = out[1 + stride] = uint8_t(pix >> 8);
                        }
                    else // mode 2: each pair of lines the same
                        for (int row = 0; row < 2; row++)
                        {
                            int pix2 = Get(bits, full), pix1 = Get(bits, full);
                            for (int r2 = 0; r2 < 2; r2++, out += stride)
                            {
                                out[0] = uint8_t(pix1); out[1] = uint8_t(pix1 >> 8);
                                out[2] = uint8_t(pix2); out[3] = uint8_t(pix2 >> 8);
                            }
                        }
                }
                break;
            }
            case 0: // mono: two colours and a 16-bit map of which
                for (; run > 0 && b < blocks; run--, b++)
                {
                    int colours = Get(bits, mclr), map = Get(bits, mmap);
                    uint8_t hi = uint8_t(colours >> 8), lo = uint8_t(colours);
                    uint8_t* out = blockAt(b);
                    for (int row = 0; row < 4; row++, out += stride, map >>= 4)
                        for (int x = 0; x < 4; x++) out[x] = (map >> x) & 1 ? hi : lo;
                }
                break;
            case 2: // skip: unchanged
                b += run;
                break;
            case 3: // fill: one colour
            {
                uint8_t colour = uint8_t(t >> 8);
                for (; run > 0 && b < blocks; run--, b++)
                {
                    uint8_t* out = blockAt(b);
                    for (int row = 0; row < 4; row++, out += stride) std::memset(out, colour, 4);
                }
                break;
            }
            }
        }
    }

    void Smacker::DecodeAudio(std::span<const uint8_t> data, std::vector<int16_t>& audio) const
    {
        if (!(audioRate[0] & AudioPresent) || data.size() < 4) return;
        uint32_t unpacked = uint32_t(data[0]) | uint32_t(data[1]) << 8 | uint32_t(data[2]) << 16 | uint32_t(data[3]) << 24;
        // no frame's sound is bigger than the header's buffer for the track (or a second of sound without one)
        uint32_t most = audioSize ? audioSize : uint32_t(AudioRate()) * 4;
        if (unpacked > most) throw FormatError("Smacker: audio chunk larger than the movie's audio buffer");
        if (!(audioRate[0] & AudioCompressed))
        {
            // raw: 16-bit or 8-bit samples as they are
            if (audioRate[0] & Audio16Bit)
                for (size_t i = 0; i + 1 < data.size(); i += 2) audio.push_back(int16_t(data[i] | data[i + 1] << 8));
            else
                for (uint8_t s : data) audio.push_back(int16_t((int(s) - 128) << 8));
            return;
        }
        BitReader bits{ data.subspan(4) };
        if (!bits.Bit()) return; // no sound in this frame
        bool stereo = bits.Bit(), sixteen = bits.Bit();
        int treeCount = 1 << ((sixteen ? 1 : 0) + (stereo ? 1 : 0));
        SmallTree trees[4];
        for (int i = 0; i < treeCount; i++) trees[i] = ReadSmallTree(bits);
        const int channels = stereo ? 2 : 1;
        if (sixteen)
        {
            // each channel starts from a value given in full, then adds 16-bit differences
            int pred[2] = { 0, 0 };
            for (int c = channels - 1; c >= 0; c--)
            {
                uint32_t v = bits.Bits(16);
                pred[c] = int16_t(uint16_t(((v & 0xFF) << 8) | (v >> 8)));
            }
            size_t samples = unpacked / 2;
            for (int c = 0; c < channels && size_t(c) < samples; c++) audio.push_back(int16_t(pred[c]));
            for (size_t i = size_t(channels); i < samples; i++)
            {
                int c = stereo ? int(i & 1) : 0;
                int lo = DecodeSmall(bits, trees[c * 2]), hi = DecodeSmall(bits, trees[c * 2 + 1]);
                pred[c] = int16_t(pred[c] + int16_t(uint16_t(lo | (hi << 8))));
                audio.push_back(int16_t(pred[c]));
            }
        }
        else
        {
            // 8-bit: a start value per channel, then 8-bit differences, widened to 16 bits
            int pred[2] = { 0, 0 };
            for (int c = channels - 1; c >= 0; c--) pred[c] = int(bits.Bits(8));
            size_t samples = unpacked;
            for (int c = 0; c < channels && size_t(c) < samples; c++) audio.push_back(int16_t((pred[c] - 128) << 8));
            for (size_t i = size_t(channels); i < samples; i++)
            {
                int c = stereo ? int(i & 1) : 0;
                pred[c] = (pred[c] + int8_t(DecodeSmall(bits, trees[c]))) & 0xFF;
                audio.push_back(int16_t((pred[c] - 128) << 8));
            }
        }
    }
}
