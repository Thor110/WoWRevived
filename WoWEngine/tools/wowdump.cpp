// wowdump - lists and extracts the game's KAT! and SfxL archives.
//
//   wowdump <archive>                      list the entries
//   wowdump <archive> <outdir> [--raw]     extract every entry (FFUH entries are decompressed unless --raw)
//   wowdump --check <game folder>          report which required game files are missing
//   wowdump --sprite <archive> <sprite> <palette> <out.bmp> [frame]
//                                          render a sprite from an archive (e.g. DAT/Dat.wow CD_BD1.SPR CD.PAL)
//   wowdump --text <TEXT.ojd>              print every string with its id
#include "core/Archive.h"
#include "core/Ffuh.h"
#include "core/GameFiles.h"
#include "core/Palette.h"
#include "core/Sprite.h"
#include "core/TextOjd.h"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <string>

namespace fs = std::filesystem;

static int Usage()
{
    std::fprintf(stderr,
        "usage: wowdump <archive>                  list entries\n"
        "       wowdump <archive> <outdir> [--raw] extract entries\n"
        "       wowdump --check <game folder>      check for the required game files\n"
        "       wowdump --sprite <archive> <sprite> <palette> <out.bmp> [frame]\n"
        "       wowdump --text <TEXT.ojd>          print the game's strings\n");
    return 2;
}

static std::vector<uint8_t> ReadFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("can't open " + path.string());
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

static void Put16(std::vector<uint8_t>& out, uint32_t v) { out.push_back(uint8_t(v)); out.push_back(uint8_t(v >> 8)); }
static void Put32(std::vector<uint8_t>& out, uint32_t v) { Put16(out, v & 0xFFFF); Put16(out, v >> 16); }

// 24-bit BMP, transparent pixels (index 0) shown as magenta
static void WriteBmp(const fs::path& path, const wow::IndexedImage& image, const wow::Palette& palette)
{
    uint32_t stride = (uint32_t(image.width) * 3 + 3) & ~3u;
    uint32_t pixelBytes = stride * uint32_t(image.height);
    std::vector<uint8_t> out;
    out.push_back('B'); out.push_back('M');
    Put32(out, 54 + pixelBytes); Put32(out, 0); Put32(out, 54);
    Put32(out, 40); Put32(out, uint32_t(image.width)); Put32(out, uint32_t(image.height));
    Put16(out, 1); Put16(out, 24); Put32(out, 0); Put32(out, pixelBytes);
    Put32(out, 2835); Put32(out, 2835); Put32(out, 0); Put32(out, 0);
    for (int y = image.height - 1; y >= 0; y--) // bottom-up
    {
        size_t rowStart = out.size();
        for (int x = 0; x < image.width; x++)
        {
            uint8_t index = image.pixels[size_t(y) * image.width + x];
            wow::Rgb c = index == 0 ? wow::Rgb{ 255, 0, 255 } : palette.colours[index];
            out.push_back(c.b); out.push_back(c.g); out.push_back(c.r);
        }
        while (out.size() - rowStart < stride) out.push_back(0);
    }
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(out.data()), std::streamsize(out.size()));
    if (!file) throw std::runtime_error("can't write " + path.string());
}

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2) return Usage();
        std::string first = argv[1];

        if (first == "--check")
        {
            if (argc != 3) return Usage();
            wow::GameFiles files(argv[2]);
            auto missing = files.Missing();
            for (const auto& m : missing) std::printf("missing: %s\n", m.c_str());
            if (missing.empty()) std::printf("all required game files found\n");
            return missing.empty() ? 0 : 1;
        }

        if (first == "--sprite")
        {
            if (argc != 6 && argc != 7) return Usage();
            wow::Archive archive(argv[2]);
            wow::Sprite sprite(archive.Read(argv[3]));
            wow::Palette palette = wow::Palette::Load(archive.Read(argv[4]));
            int frame = argc == 7 ? std::stoi(argv[6]) : 0;
            const auto& info = sprite.Info();
            std::printf("%ux%u, %u frame(s), row header %u\n", info.width, info.height, info.frames, info.rowHeaderSize);
            WriteBmp(argv[5], sprite.Decode(frame), palette);
            return 0;
        }

        if (first == "--text")
        {
            if (argc != 3) return Usage();
            auto entries = wow::TextOjd::Load(ReadFile(argv[2]));
            for (size_t i = 0; i < entries.size(); i++)
            {
                std::string text = entries[i].text;
                for (char& c : text) if (c == '\n') c = '|';
                std::printf("%4zu  id %5u  side %u  %s\n", i, entries[i].id, entries[i].faction, text.c_str());
            }
            std::printf("%zu strings\n", entries.size());
            return 0;
        }

        wow::Archive archive(first);
        bool raw = argc >= 4 && std::string(argv[3]) == "--raw";
        const char* kind = archive.Kind() == wow::ArchiveKind::Kat ? "KAT!" : "SfxL";

        if (argc == 2)
        {
            std::printf("%s archive, %zu entries\n", kind, archive.Entries().size());
            for (size_t i = 0; i < archive.Entries().size(); i++)
            {
                const auto& e = archive.Entries()[i];
                auto head = archive.Read(i, true);
                std::printf("%5zu  %-12s  offset %10u  length %9u%s\n", i, e.name.c_str(), e.offset, e.length,
                    wow::ffuh::IsCompressed(head) ? "  FFUH" : "");
            }
            return 0;
        }

        fs::path outDir = argv[2];
        fs::create_directories(outDir);
        for (size_t i = 0; i < archive.Entries().size(); i++)
        {
            std::string name = archive.Entries()[i].name;
            if (archive.Kind() == wow::ArchiveKind::Sfxl) name += ".raw"; // headerless 16-bit mono PCM
            if (name.empty() || name.find_first_of("/\\:") != std::string::npos || name == "." || name == "..")
            {
                std::fprintf(stderr, "skipping entry %zu with an unsafe name\n", i);
                continue;
            }
            auto data = archive.Read(i, raw);
            std::ofstream out(outDir / name, std::ios::binary);
            out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
            if (!out) throw std::runtime_error("can't write " + (outDir / name).string());
        }
        std::printf("extracted %zu entries to %s\n", archive.Entries().size(), outDir.string().c_str());
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "wowdump: %s\n", ex.what());
        return 1;
    }
}
