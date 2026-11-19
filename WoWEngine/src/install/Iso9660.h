// ISO 9660 file system on top of a source of 2048-byte sectors (an .iso, or the data track of a .bin).
// Joliet names are used when the disc has them.
#pragma once
#include "install/Disc.h"

#include <functional>

namespace wow::install
{
    using SectorReader = std::function<bool(uint32_t lba, uint8_t* sector2048)>;

    struct IsoEntry
    {
        DiscFile file;
        uint32_t lba = 0;
    };

    // Throws std::runtime_error if there's no ISO 9660 volume.
    std::vector<IsoEntry> ReadIsoDirectory(const SectorReader& read);
    bool ReadIsoFile(const SectorReader& read, const IsoEntry& entry, const std::function<void(const uint8_t*, size_t)>& out);
}
