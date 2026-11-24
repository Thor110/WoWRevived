#include "core/Png.h"
#include "core/Bytes.h"

#include <cstdlib>
#include <cstring>

namespace wow
{
    namespace
    {
        struct BitReader
        {
            std::span<const uint8_t> d;
            size_t pos = 0;
            uint32_t bits = 0;
            int count = 0;
            uint32_t Get(int n)
            {
                while (count < n)
                {
                    if (pos >= d.size()) throw FormatError("PNG: the compressed data ends early");
                    bits |= uint32_t(d[pos++]) << count;
                    count += 8;
                }
                const uint32_t v = bits & ((n == 32 ? 0u : (1u << n)) - 1u);
                bits >>= n;
                count -= n;
                return v;
            }
            void Align() { bits = 0; count = 0; }
        };

        // a canonical Huffman code (RFC 1951 3.2.2), decoded a bit at a time by its counts and symbols
        struct Huffman
        {
            std::array<uint16_t, 16> counts{};
            std::vector<uint16_t> symbols;
            void Build(const uint8_t* lengths, int n)
            {
                counts.fill(0);
                for (int i = 0; i < n; i++) counts[lengths[i]]++;
                counts[0] = 0;
                std::array<uint16_t, 16> offs{};
                for (int i = 1; i < 16; i++) offs[size_t(i)] = uint16_t(offs[size_t(i - 1)] + counts[size_t(i - 1)]);
                symbols.assign(size_t(n), 0);
                for (int i = 0; i < n; i++)
                    if (lengths[i]) symbols[offs[lengths[i]]++] = uint16_t(i);
            }
            int Decode(BitReader& br) const
            {
                int code = 0, first = 0, index = 0;
                for (int len = 1; len < 16; len++)
                {
                    code |= int(br.Get(1));
                    const int c = counts[size_t(len)];
                    if (code - c < first) return symbols[size_t(index + (code - first))];
                    index += c;
                    first += c;
                    first <<= 1;
                    code <<= 1;
                }
                throw FormatError("PNG: a bad Huffman code");
            }
        };

        constexpr uint16_t LengthBase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
        constexpr uint8_t LengthExtra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
        constexpr uint16_t DistBase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
        constexpr uint8_t DistExtra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

        void Codes(BitReader& br, std::vector<uint8_t>& out, const Huffman& lit, const Huffman& dist)
        {
            for (;;)
            {
                const int sym = lit.Decode(br);
                if (sym < 256) { out.push_back(uint8_t(sym)); continue; }
                if (sym == 256) return;
                const int l = sym - 257;
                if (l >= 29) throw FormatError("PNG: a bad length code");
                const size_t len = LengthBase[l] + br.Get(LengthExtra[l]);
                const int d = dist.Decode(br);
                if (d >= 30) throw FormatError("PNG: a bad distance code");
                const size_t back = DistBase[d] + br.Get(DistExtra[d]);
                if (back > out.size()) throw FormatError("PNG: a distance past the start");
                const size_t from = out.size() - back;
                for (size_t i = 0; i < len; i++) out.push_back(out[from + i]);
            }
        }

        int Paeth(int a, int b, int c)
        {
            const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
            return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
        }

        uint32_t Be32(std::span<const uint8_t> d, size_t at)
        {
            return uint32_t(d[at]) << 24 | uint32_t(d[at + 1]) << 16 | uint32_t(d[at + 2]) << 8 | d[at + 3];
        }
    }

    std::vector<uint8_t> Inflate(std::span<const uint8_t> zlib)
    {
        if (zlib.size() < 2 || (zlib[0] & 0x0F) != 8 || ((zlib[0] << 8) | zlib[1]) % 31 != 0 || (zlib[1] & 0x20))
            throw FormatError("PNG: not a zlib stream");
        BitReader br{ zlib.subspan(2) };
        std::vector<uint8_t> out;
        bool last = false;
        while (!last)
        {
            last = br.Get(1) != 0;
            const uint32_t type = br.Get(2);
            if (type == 0)
            {
                br.Align();
                if (br.pos + 4 > br.d.size()) throw FormatError("PNG: a stored block runs past the end");
                const size_t len = br.d[br.pos] | (br.d[br.pos + 1] << 8);
                br.pos += 4;
                if (br.pos + len > br.d.size()) throw FormatError("PNG: a stored block runs past the end");
                out.insert(out.end(), br.d.begin() + std::ptrdiff_t(br.pos), br.d.begin() + std::ptrdiff_t(br.pos + len));
                br.pos += len;
            }
            else if (type == 1)
            {
                uint8_t lengths[320];
                for (int i = 0; i < 144; i++) lengths[i] = 8;
                for (int i = 144; i < 256; i++) lengths[i] = 9;
                for (int i = 256; i < 280; i++) lengths[i] = 7;
                for (int i = 280; i < 288; i++) lengths[i] = 8;
                for (int i = 0; i < 30; i++) lengths[288 + i] = 5;
                Huffman lit, dist;
                lit.Build(lengths, 288);
                dist.Build(lengths + 288, 30);
                Codes(br, out, lit, dist);
            }
            else if (type == 2)
            {
                const int hlit = int(br.Get(5)) + 257, hdist = int(br.Get(5)) + 1, hclen = int(br.Get(4)) + 4;
                static constexpr uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
                uint8_t cl[19] = {};
                for (int i = 0; i < hclen; i++) cl[order[i]] = uint8_t(br.Get(3));
                Huffman clh;
                clh.Build(cl, 19);
                uint8_t lengths[320] = {};
                int n = 0;
                while (n < hlit + hdist)
                {
                    const int sym = clh.Decode(br);
                    if (sym < 16) { lengths[n++] = uint8_t(sym); continue; }
                    int repeat = 0;
                    uint8_t value = 0;
                    if (sym == 16) { if (n == 0) throw FormatError("PNG: a repeat with nothing before it"); value = lengths[n - 1]; repeat = 3 + int(br.Get(2)); }
                    else if (sym == 17) repeat = 3 + int(br.Get(3));
                    else repeat = 11 + int(br.Get(7));
                    if (n + repeat > hlit + hdist) throw FormatError("PNG: code lengths run over");
                    while (repeat--) lengths[n++] = value;
                }
                Huffman lit, dist;
                lit.Build(lengths, hlit);
                dist.Build(lengths + hlit, hdist);
                Codes(br, out, lit, dist);
            }
            else throw FormatError("PNG: a bad block type");
        }
        return out;
    }

    PngImage DecodePng(std::span<const uint8_t> d)
    {
        static constexpr uint8_t Signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
        if (d.size() < 8 || std::memcmp(d.data(), Signature, 8) != 0) throw FormatError("PNG: no signature");
        PngImage img;
        int depth = 0, colour = 0, interlace = 0;
        std::vector<uint8_t> idat;
        std::vector<uint32_t> palette;
        std::vector<uint8_t> alphas;
        bool header = false;
        for (size_t at = 8; at + 12 <= d.size();)
        {
            const uint32_t len = Be32(d, at);
            if (at + 12 + size_t(len) > d.size()) throw FormatError("PNG: a chunk runs past the end");
            const char* type = reinterpret_cast<const char*>(&d[at + 4]);
            const auto body = d.subspan(at + 8, len);
            if (std::memcmp(type, "IHDR", 4) == 0 && len >= 13)
            {
                img.width = int(Be32(body, 0));
                img.height = int(Be32(body, 4));
                depth = body[8];
                colour = body[9];
                interlace = body[12];
                header = true;
            }
            else if (std::memcmp(type, "PLTE", 4) == 0)
                for (size_t i = 0; i + 2 < len; i += 3) palette.push_back(0xFF000000u | uint32_t(body[i]) << 16 | uint32_t(body[i + 1]) << 8 | body[i + 2]);
            else if (std::memcmp(type, "tRNS", 4) == 0) alphas.assign(body.begin(), body.end());
            else if (std::memcmp(type, "IDAT", 4) == 0) idat.insert(idat.end(), body.begin(), body.end());
            else if (std::memcmp(type, "IEND", 4) == 0) break;
            at += 12 + size_t(len);
        }
        if (!header || img.width <= 0 || img.height <= 0 || img.width > 16384 || img.height > 16384) throw FormatError("PNG: no picture");
        if (interlace) throw FormatError("PNG: interlaced pictures aren't read (save it without interlacing)");
        int channels = 0;
        switch (colour)
        {
        case 0: channels = 1; break;
        case 2: channels = 3; break;
        case 3: channels = 1; break;
        case 4: channels = 2; break;
        case 6: channels = 4; break;
        default: throw FormatError("PNG: an unknown colour type");
        }
        if (!(depth == 8 || depth == 16 || (colour == 3 && (depth == 1 || depth == 2 || depth == 4)) || (colour == 0 && depth < 8)))
            throw FormatError("PNG: a bit depth it doesn't read");
        const size_t bitsPerPixel = size_t(channels) * size_t(depth);
        const size_t stride = (size_t(img.width) * bitsPerPixel + 7) / 8;
        const size_t bpp = std::max<size_t>(1, bitsPerPixel / 8);
        const std::vector<uint8_t> raw = Inflate(idat);
        if (raw.size() < (stride + 1) * size_t(img.height)) throw FormatError("PNG: the picture data is short");
        std::vector<uint8_t> rows(stride * size_t(img.height));
        for (int y = 0; y < img.height; y++)
        {
            const uint8_t filter = raw[size_t(y) * (stride + 1)];
            const uint8_t* in = &raw[size_t(y) * (stride + 1) + 1];
            uint8_t* out = &rows[size_t(y) * stride];
            const uint8_t* up = y ? &rows[size_t(y - 1) * stride] : nullptr;
            for (size_t x = 0; x < stride; x++)
            {
                const int a = x >= bpp ? out[x - bpp] : 0, b = up ? up[x] : 0, c = up && x >= bpp ? up[x - bpp] : 0;
                int v = in[x];
                switch (filter)
                {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: v += Paeth(a, b, c); break;
                default: throw FormatError("PNG: a bad filter");
                }
                out[x] = uint8_t(v);
            }
        }
        img.rgba.resize(size_t(img.width) * size_t(img.height));
        auto sample = [&](const uint8_t* row, int x, int ch) -> int {
            if (depth == 16) return row[(size_t(x) * size_t(channels) + size_t(ch)) * 2];
            if (depth == 8) return row[size_t(x) * size_t(channels) + size_t(ch)];
            const int perByte = 8 / depth;
            const int v = (row[size_t(x / perByte)] >> (8 - depth * (x % perByte + 1))) & ((1 << depth) - 1);
            return colour == 3 ? v : v * 255 / ((1 << depth) - 1);
        };
        if (colour == 3)
        {
            img.paletted = true;
            img.palette = palette;
            for (size_t i = 0; i < alphas.size() && i < img.palette.size(); i++) img.palette[i] = (img.palette[i] & 0xFFFFFFu) | uint32_t(alphas[i]) << 24;
            img.indices.resize(img.rgba.size());
        }
        for (int y = 0; y < img.height; y++)
        {
            const uint8_t* row = &rows[size_t(y) * stride];
            for (int x = 0; x < img.width; x++)
            {
                uint32_t px = 0;
                const size_t at = size_t(y) * size_t(img.width) + size_t(x);
                if (colour == 3)
                {
                    const int i = sample(row, x, 0);
                    img.indices[at] = uint8_t(i);
                    px = size_t(i) < img.palette.size() ? img.palette[size_t(i)] : 0xFF000000u;
                }
                else if (colour == 0 || colour == 4)
                {
                    const uint32_t g = uint32_t(sample(row, x, 0));
                    uint32_t a = colour == 4 ? uint32_t(sample(row, x, 1)) : 255u;
                    // a grey picture's tRNS key (a 16-bit grey level) marks one level see-through
                    if (colour == 0 && alphas.size() >= 2 && depth <= 8)
                    {
                        const int key = alphas[1], raw = depth == 8 ? int(g) : (row[size_t(x / (8 / depth))] >> (8 - depth * (x % (8 / depth) + 1))) & ((1 << depth) - 1);
                        if (raw == key) a = 0;
                    }
                    px = a << 24 | g << 16 | g << 8 | g;
                }
                else
                {
                    const uint32_t r = uint32_t(sample(row, x, 0)), g = uint32_t(sample(row, x, 1)), b = uint32_t(sample(row, x, 2));
                    const uint32_t a = colour == 6 ? uint32_t(sample(row, x, 3)) : 255u;
                    px = a << 24 | r << 16 | g << 8 | b;
                    // a tRNS colour key (RGB at 16 bits a sample) marks one colour see-through
                    if (colour == 2 && alphas.size() >= 6 && depth == 8 && r == alphas[1] && g == alphas[3] && b == alphas[5]) px &= 0xFFFFFFu;
                }
                img.rgba[at] = px;
            }
        }
        return img;
    }
}
