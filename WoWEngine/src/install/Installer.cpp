#include "install/Installer.h"
#include "core/GameFiles.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;

namespace wow::install
{
    namespace
    {
        constexpr int FirstMusicTrack = 2, LastMusicTrack = 5;

        std::string Lower(std::string s)
        {
            for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        // 44.1 kHz 16-bit stereo, as on the CD
        void WriteWavHeader(std::ofstream& out, uint64_t dataBytes)
        {
            auto u32 = [&](uint32_t v) { char b[4] = { char(v), char(v >> 8), char(v >> 16), char(v >> 24) }; out.write(b, 4); };
            auto u16 = [&](uint16_t v) { char b[2] = { char(v), char(v >> 8) }; out.write(b, 2); };
            out.write("RIFF", 4); u32(uint32_t(36 + dataBytes)); out.write("WAVE", 4);
            out.write("fmt ", 4); u32(16); u16(1); u16(2); u32(44100); u32(44100 * 4); u16(4); u16(16);
            out.write("data", 4); u32(uint32_t(dataBytes));
        }
    }

    bool Skipped(const std::string& discPath)
    {
        static const std::set<std::string> files = {
            "_inst32i.ex_", "_isdel.exe", "_setup.dll", "_sys1.cab", "_user1.cab", "autoexec.exe", "autorun.exe",
            "autorun.inf", "cms16.dll", "cms32_95.dll", "cms32_nt.dll", "data.tag", "data1.cab", "dsetup.dll",
            "dsetup16.dll", "dsetup32.dll", "lang.dat", "layout.bin", "os.dat", "readme.txt", "setup.exe", "setup.ini",
            "setup.ins", "setup.lid", "wow.exe", "wowstart.exe",
            "english.cd", "french.cd", "german.cd", "italian.cd", "spanish.cd", // turned into language.txt
            "human.cd", "martian.cd", // the side markers: WoWLauncher tells WoWEngine the side (--human / --martian)
        };
        std::string lower = Lower(discPath);
        if (lower.rfind("directx/", 0) == 0 || lower.rfind("winsys/", 0) == 0) return true;
        // WoWLauncher deletes FMV\*.smk (WoWRevived plays its own versions of the movies)
        if (lower.rfind("fmv/", 0) == 0 && lower.find('/', 4) == std::string::npos && fs::path(lower).extension() == ".smk") return true;
        return lower.find('/') == std::string::npos && files.count(lower) > 0;
    }

    DiscInfo Identify(Disc& disc)
    {
        DiscInfo info;
        static const char* const Languages[] = { "English", "French", "German", "Italian", "Spanish" };
        for (const DiscFile& f : disc.Files())
        {
            std::string lower = Lower(f.path);
            if (lower == "human.cd") info.side = Side::Human;
            else if (lower == "martian.cd" && info.side == Side::Unknown) info.side = Side::Martian;
            for (const char* language : Languages)
                if (lower == Lower(language) + ".cd") info.language = language;
        }
        return info;
    }

    const char* MusicFolder(Side discSide) { return discSide == Side::Human ? "Music-Human" : "Music-Martian"; }

    void CopyDisc(Disc& disc, const InstallOptions& options, const Progress& progress, InstallResult& result)
    {
        std::error_code ec;
        fs::create_directories(options.target, ec);
        if (ec) throw std::runtime_error("can't create " + options.target.string() + ": " + ec.message());
        DiscInfo info = Identify(disc);
        if (info.side == Side::Unknown) throw std::runtime_error(disc.Describe() + " isn't one of the game's discs (no Human.cd or MARTIAN.CD)");
        (info.side == Side::Human ? result.humanDisc : result.martianDisc) = true;
        if (!info.language.empty())
        {
            // discs from two different language releases make a mixed install (the Human disc's text wins)
            std::ifstream previous(options.target / "language.txt");
            std::string earlier;
            if (previous && std::getline(previous, earlier) && !earlier.empty() && earlier != info.language)
            {
                result.warnings.push_back("The discs are from different releases (" + earlier + " and " + info.language +
                    "); the game's text comes from the Human disc.");
                if (info.side != Side::Human) info.language = earlier;
            }
            previous.close();
            std::ofstream out(options.target / "language.txt", std::ios::trunc);
            out << info.language << "\n";
        }

        std::vector<DiscFile> files = disc.Files();
        std::vector<AudioTrack> tracks;
        if (options.music)
            for (const AudioTrack& t : disc.Tracks())
                if (t.number >= FirstMusicTrack && t.number <= LastMusicTrack) tracks.push_back(t);
        uint64_t total = 1, done = 0;
        for (const DiscFile& f : files) if (!Skipped(f.path)) total += f.size;
        for (const AudioTrack& t : tracks) total += t.frames * 2352;

        wow::GameFiles existing(options.target);
        for (const DiscFile& f : files)
        {
            if (Skipped(f.path)) continue;
            // the Human disc always writes its copy, the Martian disc only adds files that aren't there yet,
            // so the result is the same whichever disc comes first
            auto already = existing.Resolve(f.path);
            if (already && info.side != Side::Human) continue;
            fs::path out = already ? *already : options.target / fs::path(f.path);
            fs::create_directories(out.parent_path(), ec);
            progress("Copying " + f.path, double(done) / double(total));
            std::ofstream file(out, std::ios::binary | std::ios::trunc);
            if (!file) throw std::runtime_error("can't write " + out.string());
            bool ok = disc.ReadFile(f, [&](const uint8_t* data, size_t n) {
                file.write(reinterpret_cast<const char*>(data), std::streamsize(n));
                done += n;
            });
            file.close();
            if (!ok || !file) throw std::runtime_error("couldn't copy " + f.path + " from " + disc.Describe());
        }

        const char* folder = MusicFolder(info.side);
        if (options.music && tracks.empty())
        {
            // say what's wrong, why, and what it means for the game
            std::string source = disc.Describe(), lower = Lower(source);
            bool iso = lower.find("iso image") != std::string::npos || lower.find(".iso") != std::string::npos;
            std::string side = info.side == Side::Human ? "Human" : "Martian";
            std::string why = iso ? "an .iso image holds only the data track, not the CD audio tracks the music is on"
                                  : "the music's audio tracks couldn't be read from it";
            result.warnings.push_back("No music was found on the " + side + " disc (" + source + "): " + why +
                ". The game will have no " + side + " music. Use the disc itself or a .bin/.cue image of it to get the music.");
            result.musicMissing = true;
        }
        for (const AudioTrack& t : tracks)
        {
            char name[32];
            std::snprintf(name, sizeof(name), "%02d Track%02d.wav", t.number, t.number);
            fs::path out = options.target / folder / name;
            fs::create_directories(out.parent_path(), ec);
            progress(std::string("Ripping ") + folder + "/" + name, double(done) / double(total));
            std::ofstream wav(out, std::ios::binary | std::ios::trunc);
            if (!wav) throw std::runtime_error("can't write " + out.string());
            WriteWavHeader(wav, t.frames * 2352);
            bool ok = disc.ReadTrack(t, [&](const uint8_t* data, size_t n) {
                wav.write(reinterpret_cast<const char*>(data), std::streamsize(n));
                done += n;
            });
            wav.close();
            if (!ok || !wav)
            {
                fs::remove(out, ec);
                result.warnings.push_back("Couldn't read audio track " + std::to_string(t.number) + " from " + disc.Describe() +
                    ", so that piece of music will be missing from the game.");
                result.musicMissing = true;
            }
        }
        progress("Finished " + disc.Describe(), 1.0);
    }

    void Finish(const InstallOptions& options, InstallResult& result)
    {
        wow::GameFiles files(options.target);
        if (!result.humanDisc) result.warnings.push_back("The Human disc wasn't copied, so its music (Music-Human) is missing.");
        if (!result.martianDisc) result.warnings.push_back("The Martian disc wasn't copied, so its music (Music-Martian) is missing.");
        if (!result.humanDisc || !result.martianDisc) result.musicMissing = true;
        // no side marker is kept: WoWLauncher tells WoWEngine the side (--human / --martian)
        result.missing = files.Missing();
    }

    InstallResult Install(const std::vector<Disc*>& discs, const InstallOptions& options, const Progress& progress)
    {
        InstallResult result;
        for (size_t i = 0; i < discs.size(); i++)
        {
            CopyDisc(*discs[i], options, [&](const std::string& message, double fraction) {
                progress(message, (double(i) + fraction) / double(discs.size()));
            }, result);
        }
        Finish(options, result);
        progress("Done", 1.0);
        return result;
    }
}
