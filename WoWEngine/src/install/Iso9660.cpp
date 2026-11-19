#include "install/Iso9660.h"

#include <algorithm>
#include <stdexcept>

namespace wow::install
{
    namespace
    {
        constexpr uint32_t SectorSize = 2048;

        uint32_t Le32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }

        std::string Name(const uint8_t* raw, size_t length, bool joliet)
        {
            std::string name;
            if (joliet)
            {
                for (size_t i = 0; i + 1 < length; i += 2)
                {
                    unsigned c = (unsigned(raw[i]) << 8) | raw[i + 1];
                    name += c < 0x80 ? char(c) : '_'; // the game's files are all ASCII
                }
            }
            else name.assign(reinterpret_cast<const char*>(raw), length);
            if (auto semi = name.find(';'); semi != std::string::npos) name.resize(semi);
            if (!name.empty() && name.back() == '.') name.pop_back();
            return name;
        }

        void Walk(const SectorReader& read, uint32_t lba, uint32_t size, bool joliet, const std::string& prefix,
                  std::vector<IsoEntry>& out, int depth)
        {
            if (depth > 16) throw std::runtime_error("ISO 9660: directories nested too deeply");
            std::vector<uint8_t> data(size_t((size + SectorSize - 1) / SectorSize) * SectorSize);
            for (uint32_t i = 0; i * SectorSize < size; i++)
                if (!read(lba + i, data.data() + size_t(i) * SectorSize)) throw std::runtime_error("ISO 9660: can't read a directory");
            size_t pos = 0;
            while (pos < size)
            {
                uint8_t length = data[pos];
                if (length == 0) { pos = (pos / SectorSize + 1) * SectorSize; continue; } // records don't cross sectors
                if (pos + length > data.size() || length < 34) break;
                const uint8_t* r = data.data() + pos;
                uint32_t extent = Le32(r + 2), bytes = Le32(r + 10);
                uint8_t flags = r[25], nameLength = r[32];
                pos += length;
                if (nameLength == 1 && (r[33] == 0 || r[33] == 1)) continue; // "." and ".."
                std::string name = Name(r + 33, nameLength, joliet);
                if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos) continue;
                std::string path = prefix.empty() ? name : prefix + "/" + name;
                if (flags & 2) Walk(read, extent, bytes, joliet, path, out, depth + 1);
                else out.push_back({ { path, bytes }, extent });
            }
        }
    }

    std::vector<IsoEntry> ReadIsoDirectory(const SectorReader& read)
    {
        uint8_t sector[SectorSize];
        const uint8_t* rootRecord = nullptr;
        uint8_t primary[34], joliet[34];
        bool havePrimary = false, haveJoliet = false;
        for (uint32_t lba = 16; lba < 64; lba++)
        {
            if (!read(lba, sector)) break;
            if (sector[1] != 'C' || sector[2] != 'D' || sector[3] != '0' || sector[4] != '0' || sector[5] != '1') break;
            if (sector[0] == 255) break; // terminator
            if (sector[0] == 1 && !havePrimary) { std::copy(sector + 156, sector + 190, primary); havePrimary = true; }
            if (sector[0] == 2 && sector[88] == '%' && sector[89] == '/' && (sector[90] == '@' || sector[90] == 'C' || sector[90] == 'E'))
            {
                std::copy(sector + 156, sector + 190, joliet);
                haveJoliet = true;
            }
        }
        if (!havePrimary) throw std::runtime_error("no ISO 9660 file system found");
        rootRecord = haveJoliet ? joliet : primary;
        std::vector<IsoEntry> entries;
        Walk(read, Le32(rootRecord + 2), Le32(rootRecord + 10), haveJoliet, "", entries, 0);
        return entries;
    }

    bool ReadIsoFile(const SectorReader& read, const IsoEntry& entry, const std::function<void(const uint8_t*, size_t)>& out)
    {
        std::vector<uint8_t> buffer(SectorSize * 64);
        uint64_t remaining = entry.file.size;
        uint32_t lba = entry.lba;
        while (remaining > 0)
        {
            uint32_t sectors = uint32_t(std::min<uint64_t>(64, (remaining + SectorSize - 1) / SectorSize));
            for (uint32_t i = 0; i < sectors; i++)
                if (!read(lba + i, buffer.data() + size_t(i) * SectorSize)) return false;
            size_t bytes = size_t(std::min<uint64_t>(remaining, uint64_t(sectors) * SectorSize));
            out(buffer.data(), bytes);
            remaining -= bytes;
            lba += sectors;
        }
        return true;
    }
}
