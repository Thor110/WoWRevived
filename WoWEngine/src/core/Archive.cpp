#include "core/Archive.h"
#include "core/Bytes.h"
#include "core/Ffuh.h"

namespace wow
{
    namespace
    {
        constexpr size_t KatEntrySize = 44;
        constexpr size_t SfxlEntrySize = 16;
        constexpr uint32_t MaxEntries = 100000;

        char Lower(char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; }
    }

    bool EqualsNoCase(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); i++)
            if (Lower(a[i]) != Lower(b[i])) return false;
        return true;
    }

    Archive::Archive(const std::filesystem::path& p) : path(p), file(p, std::ios::binary)
    {
        if (!file) throw std::runtime_error("can't open " + p.string());
        file.seekg(0, std::ios::end);
        fileSize = uint64_t(file.tellg());
        file.seekg(0);

        uint8_t header[8] = {};
        if (!file.read(reinterpret_cast<char*>(header), sizeof(header))) throw FormatError(p.string() + ": too small to be an archive");
        ByteReader h(header);
        auto magic = h.Bytes(4);
        uint32_t count = h.U32();
        std::string_view m(reinterpret_cast<const char*>(magic.data()), 4);
        if (m == "KAT!") kind = ArchiveKind::Kat;
        else if (m == "SfxL") kind = ArchiveKind::Sfxl;
        else throw FormatError(p.string() + ": not a KAT! or SfxL archive");
        if (count > MaxEntries) throw FormatError(p.string() + ": implausible entry count");

        size_t entrySize = kind == ArchiveKind::Kat ? KatEntrySize : SfxlEntrySize;
        std::vector<uint8_t> directory(count * entrySize);
        if (!file.read(reinterpret_cast<char*>(directory.data()), std::streamsize(directory.size())))
            throw FormatError(p.string() + ": directory is cut short");

        ByteReader r(directory);
        entries.reserve(count);
        for (uint32_t i = 0; i < count; i++)
        {
            ArchiveEntry e;
            if (kind == ArchiveKind::Kat)
            {
                e.unknown = r.U32();
                e.offset = r.U32();
                e.length = r.U32();
                e.name = r.Name(12);
                r.Skip(20);
            }
            else
            {
                e.name = r.Name(8);
                e.length = r.U32();
                e.offset = r.U32();
            }
            if (uint64_t(e.offset) + e.length > fileSize) throw FormatError(p.string() + ": entry " + e.name + " points past the end of the file");
            entries.push_back(std::move(e));
        }
    }

    std::optional<size_t> Archive::Find(std::string_view name) const
    {
        for (size_t i = 0; i < entries.size(); i++)
            if (EqualsNoCase(entries[i].name, name)) return i;
        return std::nullopt;
    }

    std::vector<uint8_t> Archive::Read(size_t index, bool raw)
    {
        const ArchiveEntry& e = entries.at(index);
        std::vector<uint8_t> data(e.length);
        file.clear();
        file.seekg(std::streamoff(e.offset));
        if (!file.read(reinterpret_cast<char*>(data.data()), std::streamsize(data.size())))
            throw FormatError(path.string() + ": can't read " + e.name);
        if (raw || kind != ArchiveKind::Kat) return data;
        return ffuh::Decompress(data);
    }

    std::vector<uint8_t> Archive::Read(std::string_view name, bool raw)
    {
        auto index = Find(name);
        if (!index) throw std::runtime_error(path.string() + ": no entry named " + std::string(name));
        return Read(*index, raw);
    }
}
