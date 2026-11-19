// Writes 24-bit BMP files (screenshots, wowdump sprite output).
#pragma once
#include <cstdint>
#include <filesystem>

namespace wow
{
    // pixels are XRGB8888, top row first. Throws std::runtime_error if the file can't be written.
    void WriteBmp(const std::filesystem::path& path, const uint32_t* pixels, int width, int height);
}
