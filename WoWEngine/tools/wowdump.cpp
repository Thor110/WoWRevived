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
//   wowdump --resave <save file> [out]     read a save exactly and write it again (checks the reader and writer)
//   wowdump --models <archive>             read every WOF, IOB and CLS/ATM in an archive and summarise them
//   wowdump --iob <archive> <name> <palette> <out.bmp>  save a building's pre-rendered picture
#include "core/AiTable.h"
#include "core/Archive.h"
#include "core/Bmp.h"
#include "core/BuildList.h"
#include "core/Ffuh.h"
#include "core/GameFiles.h"
#include "core/Models.h"
#include "core/ObjOjd.h"
#include "core/Research.h"
#include "core/SaveFile.h"
#include "core/SaveGame.h"
#include "core/Smacker.h"
#include "core/Palette.h"
#include "core/SoundTable.h"
#include "core/Sprite.h"
#include "core/TextOjd.h"

#include <cstring>
#include <algorithm>
#include <cstdlib>
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
        "       wowdump --units <OBJ.ojd>          print each unit type's counter frame, class and war map speed\n"
        "       wowdump --arrows <OBJ.ojd>         print the route arrow styles\n"
        "       wowdump --smk <movie.smk> <outdir> [every]  decode a Smacker movie: frames as BMPs, sound as a WAV\n"
        "       wowdump --save <save file>         print a save's name, date and sections\n"
        "       wowdump --resave <save> [out]      read a save exactly, write it again and compare\n"
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

        if (first == "--smk" && (argc == 4 || argc == 5))
        {
            wow::Smacker movie{ fs::path(argv[2]) };
            fs::path out = argv[3];
            fs::create_directories(out);
            int every = argc == 5 ? std::max(1, std::atoi(argv[4])) : 100;
            std::printf("%d x %d (shown %d high), %d frames at %.4f s, audio %s %d Hz %d ch\n", movie.Width(), movie.Height(),
                        movie.DisplayHeight(), movie.Frames(), movie.FrameSeconds(), movie.HasAudio() ? "yes" : "no",
                        movie.AudioRate(), movie.AudioChannels());
            std::vector<int16_t> audio;
            while (movie.DecodeFrame(audio))
            {
                if (movie.FrameIndex() % every) continue;
                // a 24-bit BMP of the frame as shown (every other line when interlaced)
                int w = movie.Width(), h = movie.DisplayHeight();
                const size_t row = (size_t(w) * 3 + 3) & ~size_t(3); // rows are padded to 4 bytes
                std::vector<uint8_t> bmp(54 + row * size_t(h));
                auto put32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) bmp[at + size_t(i)] = uint8_t(v >> (8 * i)); };
                bmp[0] = 'B'; bmp[1] = 'M'; put32(2, uint32_t(bmp.size())); put32(10, 54); put32(14, 40);
                put32(18, uint32_t(w)); put32(22, uint32_t(-h)); bmp[26] = 1; bmp[28] = 24;
                for (int y = 0; y < h; y++)
                {
                    int source = movie.Interlaced() || movie.Doubled() ? y / 2 : y;
                    bool blank = movie.Interlaced() && (y & 1);
                    for (int x = 0; x < w; x++)
                    {
                        uint32_t c = blank ? 0 : movie.Palette()[movie.Pixels()[size_t(source) * size_t(w) + size_t(x)]];
                        size_t at = 54 + size_t(y) * row + size_t(x) * 3;
                        bmp[at] = uint8_t(c); bmp[at + 1] = uint8_t(c >> 8); bmp[at + 2] = uint8_t(c >> 16);
                    }
                }
                char name[32];
                std::snprintf(name, sizeof(name), "frame%05d.bmp", movie.FrameIndex());
                std::ofstream(out / name, std::ios::binary).write(reinterpret_cast<const char*>(bmp.data()), std::streamsize(bmp.size()));
            }
            // the sound as a WAV
            uint32_t bytes = uint32_t(audio.size() * 2), rate = uint32_t(movie.AudioRate()), ch = uint32_t(movie.AudioChannels());
            std::vector<uint8_t> wav(44);
            auto put = [&](size_t at, uint32_t v, int n) { for (int i = 0; i < n; i++) wav[at + size_t(i)] = uint8_t(v >> (8 * i)); };
            std::memcpy(wav.data(), "RIFF", 4); put(4, 36 + bytes, 4); std::memcpy(wav.data() + 8, "WAVEfmt ", 8);
            put(16, 16, 4); put(20, 1, 2); put(22, ch, 2); put(24, rate, 4); put(28, rate * ch * 2, 4); put(32, ch * 2, 2); put(34, 16, 2);
            std::memcpy(wav.data() + 36, "data", 4); put(40, bytes, 4);
            std::ofstream w(out / "audio.wav", std::ios::binary);
            w.write(reinterpret_cast<const char*>(wav.data()), 44);
            w.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(bytes));
            std::printf("%zu samples of sound\n", audio.size());
            return 0;
        }

        if (first == "--arrows" && argc == 3)
        {
            for (const auto& [id, t] : wow::ObjOjd::LoadArrowTypes(ReadFile(argv[2])))
                std::printf("arrow %d  gtype 0x%X  %d %d %d %d  %g\n", id, t.gtype, t.a, t.b, t.c, t.d, t.f);
            return 0;
        }

        // --research OBJ.ojd TEXT.ojd save [human|martian]: both research trees as the save has them, and a check
        // that writing them again gives the same bytes
        if (first == "--research" && (argc == 5 || argc == 6))
        {
            const bool playingHuman = !(argc == 6 && std::string(argv[5]) == "martian");
            auto units = wow::ObjOjd::LoadUnits(ReadFile(argv[2]));
            auto text = wow::TextTable::Load(ReadFile(argv[3]), playingHuman);
            auto save = ReadFile(argv[4]);
            std::vector<uint8_t> blob;
            for (bool human : { true, false })
            {
                wow::Research r(human, human == playingHuman, units, text, {}, playingHuman, {});
                size_t at = 0;
                const char* tag = wow::Research::Tag(human);
                for (; at + 4 <= save.size(); at++) if (std::memcmp(&save[at], tag, 4) == 0) break;
                size_t start = at;
                at += 4;
                bool ok = r.Load(save, at);
                std::printf("%s research: %s, %zu bytes, current %d\n", tag, ok ? "loaded" : "FAILED", at - start, r.CurrentNumber());
                std::vector<uint8_t> again;
                r.Save(again);
                bool same = again.size() == at - start && std::equal(again.begin(), again.end(), save.begin() + std::ptrdiff_t(start));
                std::printf("  written again: %s\n", same ? "identical" : "DIFFERENT");
                {
                    wow::Research fresh(human, human == playingHuman, units, text, {}, playingHuman, {});
                    std::vector<uint8_t> made;
                    fresh.Save(made);
                    bool asNew = made.size() == at - start && std::equal(made.begin(), made.end(), save.begin() + std::ptrdiff_t(start));
                    std::printf("  a new tree writes: %s\n", asNew ? "the same" : "something else");
                }
                for (const auto& item : r.Items())
                    std::printf("  %3d  %-4s 0x%03X  %-44s %5d h  state %d  page %2d  pre %d %d %d %d\n", item.number,
                        item.kind == 0 ? "unit" : item.kind == 1 ? "bldg" : item.kind == 2 ? "enh" : "trig", item.type,
                        item.name.c_str(), item.total, item.state, item.page, item.pre[0], item.pre[1], item.pre[2], item.pre[3]);
                blob.insert(blob.end(), again.begin(), again.end());
            }
            return 0;
        }

        // --factories save: every object's factory (its WMOB tail), and a check that writing them again changes nothing
        if (first == "--factories" && argc == 3)
        {
            auto save = wow::SaveFile::Parse(ReadFile(argv[2]));
            int objects = 0, read = 0, factories = 0, same = 0;
            for (int s = 1; s <= 30; s++)
                for (const auto& list : save.Sector(s).lists)
                    for (const wow::SaveObject& o : list)
                    {
                        objects++;
                        auto t = wow::ReadTail(o);
                        if (!t)
                        {
                            std::printf("sector %d type %d: tail not found (%zu bytes), last 48:", s, o.Type(), o.bytes.size());
                            for (size_t i = o.bytes.size() > 48 ? o.bytes.size() - 48 : 0; i < o.bytes.size(); i++) std::printf(" %02X", o.bytes[i]);
                            std::printf("\n");
                            continue;
                        }
                        read++;
                        wow::SaveObject copy = o;
                        wow::WriteTail(copy, *t);
                        if (copy.bytes == o.bytes) same++;
                        if (t->hasFactory)
                        {
                            factories++;
                            std::printf("sector %2d type %3d: factory, %zu queued%s\n", s, o.Type(), t->queue.entries.size(), t->queue.busy ? ", busy" : "");
                            for (const auto& e : t->queue.entries)
                                std::printf("    type %d target %08X repeat %d hours %.1f\n", e.type, e.target, e.repeat, e.hours);
                        }
                    }
            std::printf("%d objects, %d tails read (%d written back the same), %d factories\n", objects, read, same, factories);
            return 0;
        }

        if (first == "--units" && argc == 3)
        {
            for (const auto& [id, u] : wow::ObjOjd::LoadUnits(ReadFile(argv[2])))
                if (u.warMapSpeed || u.field52 || u.field56 || u.category)
                    std::printf("unit %3d  built by %d %d %d  build %g days  faction %d (research %d, %d days)  category %d  battle units %d  counter %3d  class %d  +52 %3d  speed %4d (%.2f map px/hour)  +56 %4d  upkeep %g %g %g  working %g %g %g\n",
                                id, u.builders[0], u.builders[1], u.builders[2], double(u.buildDays), u.faction, u.researchFaction, u.researchDays, u.category, u.battleUnits, u.counterFrame, u.moveClass, u.field52, u.warMapSpeed, u.warMapSpeed * 0.05, u.field56,
                                u.upkeep[0], u.upkeep[1], u.upkeep[2], u.activity[0], u.activity[1], u.activity[2]);
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
        if (first == "--resave" && (argc == 3 || argc == 4))
        {
            auto data = ReadFile(argv[2]);
            auto save = wow::SaveFile::Parse(data);
            auto written = save.Write();
            size_t objects = 0;
            for (const auto& sec : save.sectors) for (const auto& list : sec.lists) objects += list.size();
            std::printf("\"%s\": %zu objects, %zu routes, %zu messages, pods %u (%u h); written %s the original\n",
                save.Name().c_str(), objects, save.routes.size(), save.messages.size(), save.pods, save.podHours,
                written == data ? "the same as" : "DIFFERENT from");
            if (argc == 4)
            {
                std::ofstream out(argv[3], std::ios::binary);
                out.write(reinterpret_cast<const char*>(written.data()), std::streamsize(written.size()));
            }
            return written == data ? 0 : 1;
        }
        if (first == "--save" && argc == 3)
        {
            auto data = ReadFile(argv[2]);
            auto info = wow::SaveInfo::Read(data);
            std::printf("version %u  \"%s\"  %02d/%02d/%d %02d:%02d (%.2f hours)  war map %u %u %u %u  hours elapsed %u\n",
                info.version, info.name.c_str(), info.Day(), info.Month(), info.Year(), info.Hour(), info.Minute(), double(info.ticks),
                info.warMap[0], info.warMap[1], info.warMap[2], info.warMap[3], info.dateTime[0]);
            std::map<std::string, int> counts;
            for (const auto& t : wow::SaveInfo::Tags(data)) counts[t.name]++;
            for (const auto& [n, c] : counts) std::printf("  %s x%d\n", n.c_str(), c);
            static const char* const Lists[6] = { "HUNI", "AUNI", "HCON", "ACON", "HRES", "ARES" };
            auto owners = wow::SaveInfo::SectorOwners(data);
            std::printf("  owners (sector 1-30, 0 Human 1 Martian 2 no one):");
            for (int i = 1; i <= 30; i++) std::printf(" %d", owners[size_t(i)]);
            std::printf("\n");
            for (const auto& o : wow::SaveInfo::WarMapObjects(data))
            {
                std::printf("  sector %2d %s type %3d  id %3d at %d,%d", o.sector, Lists[o.list], o.unitType, o.id, o.x, o.y);
                if (o.list >= 2) std::printf("  (%g, %g)", o.value[0], o.value[1]);
                std::printf("\n");
            }
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
                std::printf("0x%03X  %-10s vol %.2f%s%s%s", unsigned(id), wow::SoundTable::TypeName(s.type), double(s.volume),
                    s.loop ? "  loop" : "", s.positional ? "  positional" : "", s.stereo ? "  stereo" : "");
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
