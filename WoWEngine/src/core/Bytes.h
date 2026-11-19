// Little-endian reads from a byte buffer with bounds checks. Every game format is little-endian
// (the original engine is 32-bit x86), so the engine never relies on the host's byte order.
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

namespace wow
{
    class FormatError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    class ByteReader
    {
    public:
        explicit ByteReader(std::span<const uint8_t> data) : data(data) {}

        size_t Position() const { return pos; }
        size_t Remaining() const { return data.size() - pos; }
        void Seek(size_t offset)
        {
            if (offset > data.size()) throw FormatError("seek past the end of the data");
            pos = offset;
        }
        void Skip(size_t count) { Seek(pos + Need(count)); }

        uint8_t U8() { Need(1); return data[pos++]; }
        uint16_t U16()
        {
            Need(2);
            uint16_t v = uint16_t(data[pos] | (data[pos + 1] << 8));
            pos += 2;
            return v;
        }
        uint32_t U32()
        {
            Need(4);
            uint32_t v = uint32_t(data[pos]) | (uint32_t(data[pos + 1]) << 8) | (uint32_t(data[pos + 2]) << 16) | (uint32_t(data[pos + 3]) << 24);
            pos += 4;
            return v;
        }
        int32_t I32() { return int32_t(U32()); }

        std::span<const uint8_t> Bytes(size_t count)
        {
            Need(count);
            auto s = data.subspan(pos, count);
            pos += count;
            return s;
        }

        // fixed size name field: stops at the first null
        std::string Name(size_t fieldSize)
        {
            auto field = Bytes(fieldSize);
            size_t len = 0;
            while (len < field.size() && field[len] != 0) len++;
            return std::string(reinterpret_cast<const char*>(field.data()), len);
        }

    private:
        size_t Need(size_t count) const
        {
            if (count > data.size() - pos) throw FormatError("unexpected end of data");
            return count;
        }

        std::span<const uint8_t> data;
        size_t pos = 0;
    };
}
