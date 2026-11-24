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
#include "core/Landscape.h"
#include "core/TerrainView.h"
#include "core/Scenery.h"
#include "core/WofView.h"
#include "core/Random.h"
#include "core/ObjOjd.h"
#include "core/Research.h"
#include "core/SaveFile.h"
#include "core/SaveGame.h"
#include "core/Smacker.h"
#include "core/Palette.h"
#include "core/Png.h"
#include "core/TexturePack.h"
#include "core/SoundTable.h"
#include "core/ShadeTable.h"
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
#include <functional>
#include <map>
#include <set>
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
        "       wowdump --iob <archive> <name> <palette> <out.bmp>\n"
        "       wowdump --textures <game folder> <outdir> [level]\n"
        "                                          every unit's and building's texture as 8-bit BMPs, by side\n"
        "       wowdump --landscape <map.CLS> <map.ATM> [<OBJ.ojd> <hour> <LANDR.INT> <out.bmp>]\n"
        "                                          build a battle map's terrain; light it for an hour\n"
        "       wowdump --terrain <map.CLS> <map.ATM> <OBJ.ojd> <hour> <dat folder> <out.bmp> | <x> <y> <out.bmp>\n"
        "                                          draw the ground as the battle map does: all of it, or 640x480 at x,y\n");
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

// An 8-bit BMP: the image's own colour indices kept, with a 256-colour palette (index 0 is transparent in the game
// and is written as magenta), so a texture can be edited and its indices read back.
static void WriteIndexedBmp(const fs::path& path, const uint8_t* indices, int width, int height, const uint32_t* colours)
{
    const int stride = (width + 3) & ~3;
    const uint32_t offset = 14 + 40 + 1024, size = offset + uint32_t(stride) * uint32_t(height);
    std::vector<uint8_t> out(size, 0);
    auto put32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) out[at + size_t(i)] = uint8_t(v >> (i * 8)); };
    auto put16 = [&](size_t at, uint16_t v) { out[at] = uint8_t(v); out[at + 1] = uint8_t(v >> 8); };
    out[0] = 'B'; out[1] = 'M';
    put32(2, size); put32(10, offset);
    put32(14, 40); put32(18, uint32_t(width)); put32(22, uint32_t(height)); put16(26, 1); put16(28, 8);
    put32(34, uint32_t(stride) * uint32_t(height)); put32(46, 256); put32(50, 256);
    for (int i = 0; i < 256; i++) put32(54 + size_t(i) * 4, i == 0 ? 0xFF00FFu : colours[i] & 0xFFFFFFu);
    for (int y = 0; y < height; y++)
        std::memcpy(&out[offset + size_t(height - 1 - y) * size_t(stride)], indices + size_t(y) * size_t(width), size_t(width));
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("can't write " + path.string());
    f.write(reinterpret_cast<const char*>(out.data()), std::streamsize(out.size()));
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

        // --tables OBJ.ojd: the loader's tables kept by graphics id (records 6, 10, 11) and unit type (12, 13)
        if (first == "--tables" && argc == 3)
        {
            auto t = wow::ObjOjd::LoadBattleTables(ReadFile(argv[2]));
            for (const auto& [id, e] : t.six) std::printf("6  id %4d  %g %g %d %d\n", id, double(e.a), double(e.b), e.c, e.d);
            for (const auto& [id, e] : t.ten) { std::printf("10 id %4d ", id); for (int v : e) std::printf(" %d", v); std::printf("\n"); }
            for (const auto& [id, e] : t.eleven) std::printf("11 id %4d  %d %g\n", id, e.first, double(e.second));
            for (const auto& [ty, e] : t.twelve) { std::printf("12 type %4d", ty); for (int v : e) std::printf(" %d", v); std::printf("\n"); }
            for (const auto& [ty, e] : t.thirteen) std::printf("13 type %4d  %g %d %d %d\n", ty, double(e.f), e.a, e.b, e.c);
            for (const auto& [h, e] : t.dayTimes)
            {
                std::printf("9  hour %2d  angle %3d", h, e.angle);
                for (const auto& set : e.sets) std::printf("  '%s' %g %g %g %g", set.name.c_str(), double(set.f[0]), double(set.f[1]), double(set.f[2]), double(set.f[3]));
                std::printf("\n");
            }
            return 0;
        }

        if (first == "--units" && argc == 3)
        {
            for (const auto& [id, u] : wow::ObjOjd::LoadUnits(ReadFile(argv[2])))
                if (u.warMapSpeed || u.field52 || u.field56 || u.category)
                {
                    std::printf("unit %3d %-4s built by %d %d %d  build %g days  faction %d (research %d, %d days)  category %d  battle units %d  counter %3d  class %d  +52 %3d  speed %4d (%.2f map px/hour)  +56 %4d  upkeep %g %g %g  working %g %g %g\n",
                                id, u.code.c_str(), u.builders[0], u.builders[1], u.builders[2], double(u.buildDays), u.faction, u.researchFaction, u.researchDays, u.category, u.battleUnits, u.counterFrame, u.moveClass, u.field52, u.warMapSpeed, u.warMapSpeed * 0.05, u.field56,
                                u.upkeep[0], u.upkeep[1], u.upkeep[2], u.activity[0], u.activity[1], u.activity[2]);
                    std::printf("          base %d  upgrade %d  max %d  needs %d  info %d  +28 %d  scale %g  parts %zu  emitters %zu  hp %d/%d  sizes %d %d  +43 %d  vision %d (%d)  armour %d (Human game %d, Martian %d)  +4C %d  +50 %d  +92 %d  +98 %d  +A0 %d  +A1 %d  +A2 %d  +A4 %d\n",
                                u.baseType, u.upgrade, u.maxPerSector, u.prerequisite, u.infoPage, u.field28, double(u.scale), u.parts.size(), u.emitters.size(),
                                u.hitPoints, u.currentHitPoints, u.size40, u.size41, u.field43, wow::ObjOjd::Vision(u, {}), u.visionRaw,
                                u.armourRaw, wow::ObjOjd::Armour(u, true, {}), wow::ObjOjd::Armour(u, false, {}), u.field4C, u.field50, u.field92, u.field98, u.fieldA0, u.fieldA1, u.fieldA2, u.fieldA4);
                }
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
                        std::printf("%-12s IOB  %5d points %5d patches %s  header %dx%d  picture %dx%d\n", name.c_str(), m.faceCount, m.litTriangles, m.smooth ? "smooth" : "flat  ", m.width, m.height, pic.width, pic.height);
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
        if (first == "--landscape" && (argc == 4 || argc == 8))
        {
            wow::Terrain t = wow::Terrain::Load(ReadFile(argv[2]), ReadFile(argv[3]));
            wow::Landscape l = wow::Landscape::Create(t);
            int lo = 1 << 30, hi = -(1 << 30), water = 0, passable = 0, build = 0, line = 0;
            for (int y = 0; y < l.Height(); y++)
                for (int x = 0; x < l.Width(); x++)
                {
                    const auto& v = l.At(x, y);
                    lo = std::min(lo, int(v.height));
                    hi = std::max(hi, int(v.height));
                    water += wow::Landscape::IsWater(v.atm & 15);
                    passable += l.IsAreaPassable(x, y, x + 1, y + 1);
                    build += l.CanBuildAt(x, y, x + 2, y + 2, false);
                    line += l.CanBuildLine(x, y, x + 5, y + 3, false);
                }
            std::printf("%dx%d vertices, cell %.3f, world %d wide, top y %d, heights %d to %d\n", l.Width(), l.Height(), l.Cell() / 65536.0, l.WorldWidth(), l.TopY(), lo, hi);
            std::printf("water vertices %d, passable %d, 2x2 buildable %d, line starts %d\n", water, passable, build, line);
            int cx = l.WorldWidth() / 2, cy = (l.Height() * (l.Cell() >> 16)) / 2;
            int n[3];
            l.NormalAt(cx, cy, n);
            std::printf("centre (%d, %d): height %d (nearest %d), normal %d %d %d, screen row 100 looks at world y %d\n", cx, cy, l.HeightAt(cx, cy), l.HeightAtNearest(cx, cy), n[0], n[1], n[2], l.ScreenYToWorldY(cx, 100));
            if (argc == 8)
            {
                auto tables = wow::ObjOjd::LoadBattleTables(ReadFile(argv[4]));
                int hour = std::atoi(argv[5]);
                auto it = tables.dayTimes.find(hour);
                if (it == tables.dayTimes.end()) throw std::runtime_error("OBJ.ojd has no lighting for that hour");
                const auto& set = it->second.sets[0];
                auto sun = wow::Landscape::SunFor(it->second.angle, set.f);
                sun.shade = wow::Landscape::ShadeLevel(ReadFile(argv[6]), set.f[2], 0.0f);
                l.LightTheMap(sun);
                std::printf("hour %d: angle %d, set '%s' %g %g %g %g, shade level %g, point light %g\n", hour, it->second.angle, set.name.c_str(), double(set.f[0]), double(set.f[1]), double(set.f[2]), double(set.f[3]), double(sun.shade), double(sun.pointLight));
                std::vector<uint32_t> px(size_t(l.Width()) * size_t(l.Height()));
                for (size_t i = 0; i < px.size(); i++)
                {
                    uint32_t g = l.Vertices()[i].light;
                    px[i] = (g << 16) | (g << 8) | g;
                }
                wow::WriteBmp(argv[7], px.data(), l.Width(), l.Height());
            }
            return 0;
        }
        if (first == "--terrain" && (argc == 8 || argc == 10))
        {
            fs::path dat = argv[6];
            wow::Terrain t = wow::Terrain::Load(ReadFile(argv[2]), ReadFile(argv[3]));
            wow::Landscape l = wow::Landscape::Create(t);
            auto tiles = wow::TerrainTiles::Load(t);
            auto tables = wow::ObjOjd::LoadBattleTables(ReadFile(argv[4]));
            auto it = tables.dayTimes.find(std::atoi(argv[5]));
            if (it == tables.dayTimes.end()) throw std::runtime_error("OBJ.ojd has no lighting for that hour");
            const auto& set = it->second.sets[0];
            std::string letter = set.name.empty() ? "M" : set.name.substr(0, 1);
            auto ints = ReadFile(dat / ("LANDR0" + letter + ".INT"));
            auto sun = wow::Landscape::SunFor(it->second.angle, set.f);
            sun.shade = wow::Landscape::ShadeLevel(ints, set.f[2], 0.0f);
            l.LightTheMap(sun);
            auto table = wow::ShadeTable::LoadShh(ReadFile(dat / ("LANDR0" + letter + ".SHH")));
            wow::random::Seed(1);
            wow::random::FillTable();
            auto dither = wow::DitherTables::Build(ReadFile(dat / "DITH.DAT"));
            int scrollX = 0, scrollY = 0, width = (l.Cell() >> 16) * l.Width(), height = (l.RowStep() >> 16) * l.Height() + l.TopY() + 64;
            if (argc == 10) { scrollX = std::atoi(argv[7]); scrollY = std::atoi(argv[8]); width = 640; height = 480; }
            std::vector<uint32_t> px(size_t(width) * size_t(height), 0);
            wow::TerrainView::Target target{ px.data(), width, 0, 0, width, height };
            wow::TerrainView::Render(l, tiles, table, ints, dither, target, -scrollX, -scrollY);
            wow::WriteBmp(argc == 10 ? argv[9] : argv[7], px.data(), width, height);
            std::printf("%s: %d patches, drew %dx%d at %d,%d (hour %d, '%s')\n", argv[2], int(tiles.offsets.size()), width, height, scrollX, scrollY, it->first, set.name.c_str());
            return 0;
        }
        if (first == "--scenery" && argc == 4)
        {
            auto objects = wow::LoadScenery(ReadFile(argv[2]));
            auto obj = ReadFile(argv[3]);
            auto units = wow::ObjOjd::LoadUnits(obj);
            auto gtypes = wow::ObjOjd::LoadGTypes(obj, true);
            std::map<int, int> counts;
            for (const auto& o : objects) counts[o.type]++;
            std::printf("%zu objects\n", objects.size());
            for (const auto& [type, n] : counts)
            {
                auto u = units.find(type);
                int g = u != units.end() ? u->second.field28 : 0;
                auto gt = gtypes.find(g);
                std::printf("type %4d x%-5d %-6s gtype 0x%03X class %2d  %s\n", type, n, u != units.end() ? u->second.code.c_str() : "?", g,
                            gt != gtypes.end() ? int(gt->second.gclass) : -1, gt != gtypes.end() ? gt->second.file.c_str() : "");
                if (u != units.end())
                    for (const auto& p : u->second.parts)
                    {
                        auto cu = units.find(p.a);
                        int cg = cu != units.end() ? cu->second.field28 : 0;
                        auto cgt = gtypes.find(cg);
                        std::printf("    part type %d at %d,%d,%d scale %g: gtype 0x%03X class %d %s\n", p.a, int16_t(p.b), int16_t(p.c), int16_t(p.d), double(p.f), cg,
                                    cgt != gtypes.end() ? int(cgt->second.gclass) : -1, cgt != gtypes.end() ? cgt->second.file.c_str() : "");
                    }
            }
            return 0;
        }
        if (first == "--wof" && (argc >= 8 && argc <= 10))
        {
            // --wof <OBJ.ojd> <gtype> <dat folder> <map.CLS> <map.ATM> <out.bmp> [heading [frame]]: the model at the map's
            // middle at noon, through the noon table of its slot
            auto obj = ReadFile(argv[2]);
            int gtype = int(std::strtol(argv[3], nullptr, 0));
            fs::path dat = argv[4];
            auto gtypes = wow::ObjOjd::LoadGTypes(obj, true);
            auto tables = wow::ObjOjd::LoadBattleTables(obj);
            auto g = gtypes.find(gtype);
            if (g == gtypes.end() || g->second.gclass != 2) throw std::runtime_error("not a WOF GType");
            auto six = tables.six.find(gtype);
            float scale = six != tables.six.end() ? six->second.a : 65536.0f;
            int follow = six != tables.six.end() ? six->second.c : 0;
            std::string file = g->second.file;
            if (auto slash = file.rfind('\\'); slash != std::string::npos) file = file.substr(slash + 1);
            for (char& c : file) c = char(std::toupper(static_cast<unsigned char>(c)));
            wow::WofInstance model(ReadFile(dat / file), scale, follow);
            wow::Terrain t = wow::Terrain::Load(ReadFile(argv[5]), ReadFile(argv[6]));
            wow::Landscape land = wow::Landscape::Create(t);
            static const char* const SlotFiles[] = { "", "BMHBM", "BMHVM", "BMMVM", "BMMBM", "", "", "", "", "", "BMR0NAM" };
            int slot = g->second.paletteSlot;
            std::string shade = slot >= 1 && slot <= 10 && SlotFiles[slot][0] ? SlotFiles[slot] : "BMHVM";
            auto table = wow::ShadeTable::LoadShh(ReadFile(dat / (shade + ".SHH")));
            auto day = tables.dayTimes.find(12);
            const float noon[4] = { 0.9f, 0.1f, 0.75f, 0.0f };
            auto sun = wow::Landscape::SunFor(90, day != tables.dayTimes.end() ? day->second.sets[0].f : noon);
            int heading = argc >= 9 ? std::atoi(argv[8]) : 512;
            if (argc == 10) model.SetFrame(std::atoi(argv[9]));
            int x = land.WorldWidth() / 2, y = land.Height() * (land.Cell() >> 16) / 2;
            model.Transform(land, heading, x, y, land.HeightAt(x, y), true);
            const int W = 320, H = 240;
            std::vector<uint32_t> px(size_t(W) * H, 0x404040);
            wow::TerrainView::Target target{ px.data(), W, 0, 0, W, H };
            int ox = W / 2 - x, oy = H / 2 - land.ProjectY(y, land.HeightAt(x, y));
            model.Draw(sun, table, target, ox, oy);
            wow::WriteBmp(argv[7], px.data(), W, H);
            std::printf("%s: scale %g, ground follow %d, slot %d (%s), bounds x %d..%d\n", file.c_str(), double(scale), follow, slot,
                        shade.c_str(), model.MinX(), model.MaxX());
            return 0;
        }
        if (first == "--textures" && (argc == 4 || argc == 5))
        {
            // Every WOF (vehicles) and IOB (buildings) the graphics table names, sorted by side and kind. A unit
            // type's model (+0x28, record 24) and its parts' (record 28) say whose it is; models no unit type
            // names go by their shade slot (1 Human buildings, 2 Human vehicles, 3 Martian vehicles, 4 Martian
            // buildings), slot 1's being the towns' and the countryside's (scenery/), as are slot 10's (the trees
            // and the red weed). Each is coloured by its slot's midday table (BM??M.SHH) at one level (44 of 64
            // unless given: about full light; above it the colours run to white), its own indices kept.
            const fs::path game = argv[2], out = argv[3];
            const int level = argc == 5 ? std::atoi(argv[4]) : 44;
            const auto obj = ReadFile(game / "OBJ.ojd");
            wow::Archive archive(game / "DAT" / "Dat.wow");
            auto gtypes = wow::ObjOjd::LoadGTypes(obj, true);
            for (auto& [id, g] : wow::ObjOjd::LoadGTypes(obj, false)) gtypes.emplace(id, g);
            const auto units = wow::ObjOjd::LoadUnits(obj);
            std::map<int, std::pair<int, bool>> owner;     // gtype -> faction, building
            // a part (record 28) is a unit type of its own, whose model is its +0x28; parts can have parts
            std::function<void(int, int, bool, int)> claim = [&](int type, int faction, bool building, int depth)
            {
                auto u = units.find(type);
                if (u == units.end() || depth > 4) return;
                owner.emplace(u->second.field28, std::make_pair(faction, building));
                for (const auto& part : u->second.parts) claim(part.a, faction, building, depth + 1);
            };
            for (const auto& [type, u] : units)
                if (u.faction <= 1) claim(type, u.faction, u.category >= 2, 0);
            const std::map<int, std::string> tables = { { 1, "BMHBM" }, { 2, "BMHVM" }, { 3, "BMMVM" }, { 4, "BMMBM" }, { 10, "BMR0NAM" } };
            std::map<int, wow::ShadeTable> shade;
            for (const auto& [slot, name] : tables) shade[slot] = wow::ShadeTable::LoadShh(archive.Read(name + ".SHH"));
            std::map<std::string, int> counts;
            std::set<std::string> done;
            for (const auto& [id, g] : gtypes)
            {
                std::string file = g.file.substr(g.file.find_last_of('\\') + 1);
                std::string ext = file.substr(file.find_last_of('.') + 1);
                for (char& c : ext) c = char(std::toupper(static_cast<unsigned char>(c)));
                if ((ext != "WOF" && ext != "IOB") || !shade.count(g.paletteSlot)) continue;
                std::string folder;
                std::string lower = file;
                for (char& c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
                if (lower.rfind("bridge", 0) == 0) folder = "scenery";       // the bridges belong to no one
                else if (auto o = owner.find(id); o != owner.end())
                    folder = std::string(o->second.first == 0 ? "human/" : "martian/") + (o->second.second ? "buildings" : "units");
                else if (g.paletteSlot == 2) folder = "human/units";
                else if (g.paletteSlot == 3) folder = "martian/units";
                else if (g.paletteSlot == 4) folder = "martian/buildings";
                else folder = "scenery";
                const std::string stem = file.substr(0, file.find_last_of('.'));
                if (!done.insert(folder + "/" + stem).second) continue;
                auto data = archive.Find(file);
                if (!data) { std::printf("%-14s not in the archive\n", file.c_str()); continue; }
                const uint32_t* colours = shade[g.paletteSlot].Level(level);
                fs::create_directories(out / folder);
                if (ext == "WOF")
                {
                    auto m = wow::WofModel::Load(archive.Read(*data));
                    if (m.textureHeight <= 0 || m.texture.empty()) { std::printf("%-14s no texture\n", file.c_str()); continue; }
                    WriteIndexedBmp(out / folder / (stem + ".bmp"), m.texture.data(), 256, m.textureHeight, colours);
                }
                else
                {
                    auto m = wow::IobModel::Load(archive.Read(*data));
                    auto pic = m.Picture();
                    if (pic.pixels.empty()) { std::printf("%-14s empty picture\n", file.c_str()); continue; }
                    WriteIndexedBmp(out / folder / (stem + ".bmp"), pic.pixels.data(), pic.width, pic.height, colours);
                    // where each lit triangle's patch sits in the picture (its pixels' bounds, decoded as Picture()
                    // does), so a reworked picture can be cut back up
                    std::vector<uint32_t> starts;
                    for (const auto& p : m.patches) starts.push_back(p.offset);
                    starts.push_back(uint32_t(m.patchData.size()));
                    std::sort(starts.begin(), starts.end());
                    struct Box { int x0, y0, x1, y1; };
                    std::vector<Box> boxes;
                    int minX = 0, minY = 0;
                    for (const auto& p : m.patches)
                    {
                        Box b{ 1 << 30, 1 << 30, -(1 << 30), -(1 << 30) };
                        size_t pos = p.offset;
                        const size_t end = p.offset < m.patchData.size() ? *std::upper_bound(starts.begin(), starts.end(), p.offset) : pos;
                        for (int row = 0; pos + 1 < end; row++)
                        {
                            const int skip = m.patchData[pos++], run = m.patchData[pos++];
                            if (skip == 0 && run == 0) break;
                            const int count = run & 0x7F;
                            pos += (run & 0x80) ? 1 : size_t(count);
                            if (count == 0) continue;
                            b.x0 = std::min(b.x0, p.x + skip); b.x1 = std::max(b.x1, p.x + skip + count);
                            b.y0 = std::min(b.y0, p.y + row); b.y1 = std::max(b.y1, p.y + row + 1);
                        }
                        if (b.x1 > b.x0) { minX = std::min(minX, b.x0); minY = std::min(minY, b.y0); }
                        boxes.push_back(b);
                    }
                    std::ofstream layout(out / folder / (stem + ".patches.txt"));
                    layout << "# " << file << ": " << m.patches.size() << " patches (one per lit triangle), picture " << pic.width << "x" << pic.height
                           << ", the building's anchor at " << -minX << "," << -minY << " in it\n"
                           << "# patch: left top right bottom of its pixels in the picture (right, bottom exclusive), then its\n"
                           << "# colour and mode bytes; later patches are drawn over earlier ones\n";
                    for (size_t i = 0; i < boxes.size(); i++)
                    {
                        const Box& b = boxes[i];
                        layout << i << ": ";
                        if (b.x1 > b.x0) layout << b.x0 - minX << ' ' << b.y0 - minY << ' ' << b.x1 - minX << ' ' << b.y1 - minY;
                        else layout << "empty";
                        layout << "  " << int(m.patches[i].colour) << ' ' << int(m.patches[i].mode) << '\n';
                    }
                }
                counts[folder]++;
                std::printf("%-30s %s (GType 0x%03X, slot %d)\n", (folder + "/" + stem).c_str(), ext.c_str(), unsigned(id), g.paletteSlot);
            }
            for (const auto& [folder, n] : counts) std::printf("%-20s %d\n", folder.c_str(), n);
            return 0;
        }
        if (first == "--texcheck" && argc == 4)
        {
            // every model a texture pack has a picture for, read as the battle map reads it, against the game's own:
            // the same indices (an unchanged export gives the game's bytes back), or how many differ
            const fs::path game = argv[2];
            const auto obj = ReadFile(game / "OBJ.ojd");
            wow::Archive archive(game / "DAT" / "Dat.wow");
            auto gtypes = wow::ObjOjd::LoadGTypes(obj, true);
            for (auto& [id, g] : wow::ObjOjd::LoadGTypes(obj, false)) gtypes.emplace(id, g);
            const auto folders = wow::textures::Folders(gtypes, wow::ObjOjd::LoadUnits(obj));
            wow::TexturePack pack;
            if (!pack.Open(argv[3])) throw std::runtime_error("no pictures in the pack");
            std::map<int, std::vector<uint32_t>> palettes;
            int same = 0, differ = 0, refused = 0;
            for (const auto& [id, folder] : folders)
            {
                const wow::GType& g = gtypes.at(id);
                const std::string stem = wow::textures::Stem(g.file);
                auto path = pack.Find(folder, stem, false);
                if (!path) continue;
                auto& pal = palettes[g.paletteSlot];
                if (pal.empty())
                {
                    auto t = wow::ShadeTable::LoadShh(archive.Read(wow::textures::PaletteTable(g.paletteSlot) + ".SHH"));
                    pal.assign(t.Level(wow::textures::ExportLevel), t.Level(wow::textures::ExportLevel) + 256);
                    for (auto& c : pal) c &= 0xFFFFFFu;
                }
                const auto png = wow::DecodePng(ReadFile(*path));
                const auto indices = wow::textures::Indices(png, pal.data());
                auto entry = archive.Find(g.file.substr(g.file.find_last_of('\\') + 1));
                if (!entry) continue;
                auto data = archive.Read(*entry);
                std::vector<uint8_t> was, now;
                if (wow::textures::WofAtlasHeight(data) > 0)
                {
                    const int h = wow::textures::WofAtlasHeight(data);
                    auto m = wow::WofModel::Load(data);
                    was = m.texture;
                    if (!wow::textures::ReplaceWofAtlas(data, indices, png.width, png.height)) { refused++; std::printf("%s/%s: the wrong size (%dx%d, not 256x%d)\n", folder.c_str(), stem.c_str(), png.width, png.height, h); continue; }
                    now = wow::WofModel::Load(data).texture;
                }
                else
                {
                    auto m = wow::IobModel::Load(data);
                    const auto pic = m.Picture();
                    if (png.width != pic.width || png.height != pic.height) { refused++; std::printf("%s/%s: the wrong size\n", folder.c_str(), stem.c_str()); continue; }
                    wow::IndexedImage img{ pic.width, pic.height, indices };
                    // what the game draws of each patch: its header height's rows (TerrainView::DrawPatch), each
                    // row's skip, count and pixels
                    auto drawn = [](const wow::IobModel& im) {
                        std::vector<uint8_t> out;
                        for (const auto& p : im.patches)
                        {
                            size_t pos = p.offset;
                            for (int row = 0; row < int(p.height) && pos + 1 < im.patchData.size(); row++)
                            {
                                const int skip = im.patchData[pos], run = im.patchData[pos + 1], count = run & 0x7F;
                                pos += 2;
                                out.push_back(uint8_t(skip));
                                out.push_back(uint8_t(count));
                                for (int k = 0; k < count && pos < im.patchData.size(); k++) out.push_back(im.patchData[(run & 0x80) ? pos : pos + size_t(k)]);
                                pos += (run & 0x80) ? 1 : size_t(count);
                            }
                        }
                        return out;
                    };
                    was = drawn(m);
                    now = drawn(m.Repainted(img));
                }
                size_t n = 0;
                for (size_t i = 0; i < was.size() && i < now.size(); i++) n += was[i] != now[i];
                if (n || was.size() != now.size()) { differ++; std::printf("%s/%s: %zu pixels differ\n", folder.c_str(), stem.c_str(), n); }
                else same++;
            }
            std::printf("%d the same, %d differ, %d refused\n", same, differ, refused);
            return differ || refused ? 1 : 0;
        }
        if (first == "--png" && argc == 4)
        {
            // a PNG read as the texture packs are, its pixels written raw (0xAARRGGBB, little-endian) for checking
            auto img = wow::DecodePng(ReadFile(argv[2]));
            std::ofstream out(argv[3], std::ios::binary);
            out.write(reinterpret_cast<const char*>(img.rgba.data()), std::streamsize(img.rgba.size() * 4));
            std::printf("%dx%d %s\n", img.width, img.height, img.paletted ? "paletted" : "colour");
            return 0;
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
                std::printf("0x%03X  %-10s vol %.2f%s%s%s%s", unsigned(id), wow::SoundTable::TypeName(s.type), double(s.volume),
                    s.loop ? "  loop" : "", s.positional ? "  positional" : "", s.stereo ? "  stereo" : "", s.flag6 ? "  queued" : "");
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
                for (int id = 0; id < int(wow::TextTable::GameIds); id++)
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
