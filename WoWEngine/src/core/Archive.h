// The game's two archive formats.
//
// KAT! (DAT\Dat.wow, MAPS\MAPS.WoW): "KAT!", int32 count, then 44 bytes per entry:
//   int32 unknown, int32 offset, int32 length, char[12] name (null terminated, 0xCD filled), 20 bytes of 0xCD.
//   Entries may be FFUH compressed, see Ffuh.h.
// SfxL (sound effect libraries): "SfxL", int32 count, then 16 bytes per entry:
//   char[8] name, int32 length, int32 offset. Data is raw 16-bit mono PCM.
#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wow
{
    enum class ArchiveKind { Kat, Sfxl };

    struct ArchiveEntry
    {
        std::string name;
        uint32_t offset = 0;
        uint32_t length = 0;
        uint32_t unknown = 0; // KAT! only, kept for writing the archive back unchanged
    };

    class Archive
    {
    public:
        // Throws FormatError if the file isn't a valid archive, std::runtime_error if it can't be opened.
        explicit Archive(const std::filesystem::path& path);

        ArchiveKind Kind() const { return kind; }
        const std::filesystem::path& Path() const { return path; }
        const std::vector<ArchiveEntry>& Entries() const { return entries; }

        // Case-insensitive, like the game's lookups on Windows.
        std::optional<size_t> Find(std::string_view name) const;

        // The stored bytes of an entry, decompressed if they are FFUH compressed (unless raw is true).
        std::vector<uint8_t> Read(size_t index, bool raw = false);
        std::vector<uint8_t> Read(std::string_view name, bool raw = false);

    private:
        std::filesystem::path path;
        std::ifstream file;
        uint64_t fileSize = 0;
        ArchiveKind kind = ArchiveKind::Kat;
        std::vector<ArchiveEntry> entries;
    };

    bool EqualsNoCase(std::string_view a, std::string_view b);
}
