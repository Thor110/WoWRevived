// wowdump - lists and extracts the game's KAT! and SfxL archives.
//
//   wowdump <archive>                      list the entries
//   wowdump <archive> <outdir> [--raw]     extract every entry (FFUH entries are decompressed unless --raw)
//   wowdump --check <game folder>          report which required game files are missing
//   wowdump --sprite <archive> <sprite> <palette> <out.bmp> [frame]
//                                          render a sprite from an archive (e.g. DAT/Dat.wow CD_BD1.SPR CD.PAL)
//   wowdump --text <TEXT.ojd> [martian]    print every string with its id (Human strings unless "martian")
//   wowdump --obj <obj.ojd> [martian]      print the graphics table (id, class, palette slot, file)
//   wowdump --sfx <SFX.ojd> [martian]      print the sound table (id, type, volume, flags, samples)
//   wowdump --ai <AI.ojd>                  print the AI table (entries, then each side's rule lists)
//   wowdump --save <save file>             print a save's name, date and time, and where its sections are
//   wowdump --models <archive>             read every WOF, IOB and CLS/ATM in an archive and summarise them
//   wowdump --iob <archive> <name> <palette> <out.bmp>  save a building's pre-rendered picture
#include "core/AiTable.h"
#include "core/Archive.h"
#include "core/Bmp.h"
#include "core/Ffuh.h"
#include "core/GameFiles.h"
#include "core/Models.h"
#include "core/ObjOjd.h"
#include "core/SaveGame.h"
#include "core/Palette.h"
#include "core/SoundTable.h"
#include "core/Sprite.h"
#include "core/TextOjd.h"

#include <cctype>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
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
        "       wowdump --text <TEXT.ojd> [martian] print the game's strings\n"
        "       wowdump --obj <obj.ojd> [martian]   print the graphics table\n"
        "       wowdump --sfx <SFX.ojd> [martian]   print the sound table\n"
        "       wowdump --ai <AI.ojd>              print the AI table\n"
        "       wowdump --save <save file>         print a save's name, date and sections\n"
        "       wowdump --models <archive>         check and summarise every WOF, IOB and CLS\n"
        "       wowdump --iob <archive> <name> <palette> <out.bmp>\n");
    return 2;
}

static std::vector<uint8_t> ReadFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("can't open " + path.string());
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

// transparent pixels (index 0) are shown as magenta
static void WriteSpriteBmp(const fs::path& path, const wow::IndexedImage& image, const wow::Palette& palette)
{
    std::vector<uint32_t> pixels(image.pixels.size());
    for (size_t i = 0; i < pixels.size(); i++)
    {
        uint8_t index = image.pixels[i];
        wow::Rgb c = index == 0 ? wow::Rgb{ 255, 0, 255 } : palette.colours[index];
        pixels[i] = (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | c.b;
    }
    wow::WriteBmp(path, pixels.data(), image.width, image.height);
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
            std::printf("%ux%u, %u frame(s)\n", info.width, info.height, info.frames);
            WriteSpriteBmp(argv[5], sprite.Decode(frame), palette);
            return 0;
        }

        if (first == "--ai" && argc == 3)
        {
            auto table = wow::AiTable::Load(ReadFile(argv[2]));
            for (const auto& [id, e] : table.entries) std::printf("entry %3d  value %6d  weight %6d\n", id, e.value, e.weight);
            for (int side = 0; side < 2; side++)
                for (const auto& [list, rules] : table.rules[side])
                {
                    std::printf("%s list %d: %zu rules\n", side ? "Martian" : "Human", list, rules.size());
                    for (const auto& r : rules)
                    {
                        std::printf("   ");
                        for (int i = 0; i < r.count; i++) std::printf(" %5u", r.words[i]);
                        std::printf("\n");
                    }
                }
            std::printf("%zu entries\n", table.entries.size());
            return 0;
        }
        if (first == "--save" && argc == 3)
        {
            auto data = ReadFile(argv[2]);
            auto info = wow::SaveInfo::Read(data);
            std::printf("version %u  \"%s\"  %02d/%02d/%d %02d:%02d (%.1f ticks)  war map %u %u %u %u  date fields %u %u %u\n",
                info.version, info.name.c_str(), info.Day(), info.Month(), info.Year(), info.Hour(), info.Minute(), double(info.ticks),
                info.warMap[0], info.warMap[1], info.warMap[2], info.warMap[3], info.dateTime[0], info.dateTime[1], info.dateTime[2]);
            std::map<std::string, int> counts;
            for (const auto& t : wow::SaveInfo::Tags(data)) counts[t.name]++;
            for (const auto& [n, c] : counts) std::printf("  %s x%d\n", n.c_str(), c);
            return 0;
        }
        if (first == "--models" && argc == 3)
        {
            wow::Archive archive(argv[2]);
            int ok = 0, failed = 0;
            auto ext = [](const std::string& n) { auto d = n.rfind('.'); std::string e = d == std::string::npos ? "" : n.substr(d + 1); for (char& c : e) c = char(std::toupper(static_cast<unsigned char>(c))); return e; };
            for (size_t i = 0; i < archive.Entries().size(); i++)
            {
                const std::string& name = archive.Entries()[i].name;
                std::string e = ext(name);
                if (e != "WOF" && e != "IOB" && e != "CLS") continue;
                try
                {
                    auto data = archive.Read(i);
                    if (e == "WOF")
                    {
                        auto m = wow::WofModel::Load(data);
                        size_t v = 0, f = 0;
                        for (const auto& p : m.pieces) { v += p.vertices.size(); f += p.faces.size(); }
                        std::printf("%-12s WOF  %3zu pieces %5zu vertices %5zu faces  texture 256x%d  %zu materials\n", name.c_str(), m.pieces.size(), v, f, m.textureHeight, m.materials.size() / 4);
                    }
                    else if (e == "IOB")
                    {
                        auto m = wow::IobModel::Load(data);
                        auto pic = m.Picture();
                        std::printf("%-12s IOB  %5d points %5d patches %s  header %dx%d  picture %dx%d\n", name.c_str(), m.faceCount, m.litTriangles, m.animated ? "animated" : "static  ", m.width, m.height, pic.width, pic.height);
                    }
                    else
                    {
                        std::string atmName = name.substr(0, name.size() - 3) + "ATM";
                        std::vector<uint8_t> atm;
                        if (auto a = archive.Find(atmName)) atm = archive.Read(*a);
                        auto t = wow::Terrain::Load(data, atm);
                        int land = 0;
                        for (uint8_t h : t.heights) land += h != 0;
                        bool atmOk = t.tiles.size() == size_t(t.width - 1) * size_t(t.height - 1);
                        std::printf("%-12s CLS  %dx%d  %d triangles  %zu more bytes  land %d%%  ATM %s\n", name.c_str(), t.width, t.height, t.triangles, t.rest.size(), int(100.0 * land / double(t.heights.size())), atmOk ? "matches" : t.tiles.empty() ? "missing" : "wrong size");
                    }
                    ok++;
                }
                catch (const std::exception& ex) { std::printf("%-12s FAILED: %s\n", name.c_str(), ex.what()); failed++; }
            }
            std::printf("%d read, %d failed\n", ok, failed);
            return failed ? 1 : 0;
        }
        if (first == "--iob" && argc == 6)
        {
            wow::Archive archive(argv[2]);
            auto model = wow::IobModel::Load(archive.Read(argv[3]));
            wow::Palette palette = wow::Palette::Load(archive.Read(argv[4]));
            WriteSpriteBmp(argv[5], model.Picture(), palette);
            return 0;
        }
        if (first == "--sfx")
        {
            if (argc != 3 && argc != 4) return Usage();
            bool human = !(argc == 4 && std::string(argv[3]) == "martian");
            auto table = wow::SoundTable::Load(ReadFile(argv[2]), human);
            for (const auto& [id, s] : table.Sounds())
            {
                std::printf("0x%03X  %-10s vol %.2f%s%s", unsigned(id), wow::SoundTable::TypeName(s.type), double(s.volume),
                    s.loop ? "  loop" : "", s.positional ? "  positional" : "");
                for (int g = 0; g < 3; g++)
                {
                    if (s.groups[g].empty()) continue;
                    std::printf("  [");
                    for (size_t i = 0; i < s.groups[g].size(); i++)
                        std::printf("%s%s%s@%d", i ? " " : "", s.groups[g][i].vox ? "VOX:" : "", s.groups[g][i].name.c_str(), s.groups[g][i].rate);
                    std::printf("]");
                }
                std::printf("\n");
            }
            std::printf("%zu sounds\n", table.Sounds().size());
            return 0;
        }
        if (first == "--text" || first == "--obj")
        {
            if (argc != 3 && argc != 4) return Usage();
            bool human = !(argc == 4 && std::string(argv[3]) == "martian");
            auto data = ReadFile(argv[2]);
            if (first == "--text")
            {
                auto table = wow::TextTable::Load(data, human);
                for (int id = 0; id < int(wow::TextTable::MaxIds); id++)
                {
                    if (!table.Has(id)) continue;
                    std::string text = table.Get(id);
                    for (char& c : text) if (c == '\n') c = '|';
                    std::printf("0x%03X  %s\n", id, text.c_str());
                }
                std::printf("%zu strings\n", table.Count());
            }
            else
            {
                auto table = wow::ObjOjd::LoadGTypes(data, human);
                for (const auto& [id, g] : table)
                    std::printf("0x%04X  class %3u  palette %2u  hotspot %4d,%-4d %s  %s\n", unsigned(id), g.gclass, g.paletteSlot,
                        g.hotX, g.hotY, g.loadOnUse ? "     " : "early", g.file.c_str());
                std::printf("%zu graphics entries\n", table.size());
            }
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
