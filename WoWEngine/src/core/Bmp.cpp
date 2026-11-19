#include "core/Bmp.h"

#include <fstream>
#include <stdexcept>
#include <vector>

namespace wow
{
    namespace
    {
        void Put16(std::vector<uint8_t>& out, uint32_t v) { out.push_back(uint8_t(v)); out.push_back(uint8_t(v >> 8)); }
        void Put32(std::vector<uint8_t>& out, uint32_t v) { Put16(out, v & 0xFFFF); Put16(out, v >> 16); }
    }

    void WriteBmp(const std::filesystem::path& path, const uint32_t* pixels, int width, int height)
    {
        uint32_t stride = (uint32_t(width) * 3 + 3) & ~3u;
        uint32_t pixelBytes = stride * uint32_t(height);
        std::vector<uint8_t> out;
        out.reserve(54 + pixelBytes);
        out.push_back('B'); out.push_back('M');
        Put32(out, 54 + pixelBytes); Put32(out, 0); Put32(out, 54);
        Put32(out, 40); Put32(out, uint32_t(width)); Put32(out, uint32_t(height));
        Put16(out, 1); Put16(out, 24); Put32(out, 0); Put32(out, pixelBytes);
        Put32(out, 2835); Put32(out, 2835); Put32(out, 0); Put32(out, 0);
        for (int y = height - 1; y >= 0; y--) // bottom-up
        {
            size_t rowStart = out.size();
            for (int x = 0; x < width; x++)
            {
                uint32_t p = pixels[size_t(y) * width + x];
                out.push_back(uint8_t(p)); out.push_back(uint8_t(p >> 8)); out.push_back(uint8_t(p >> 16));
            }
            while (out.size() - rowStart < stride) out.push_back(0);
        }
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(out.data()), std::streamsize(out.size()));
        if (!file) throw std::runtime_error("can't write " + path.string());
    }
}
