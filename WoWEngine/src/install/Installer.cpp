// IniStore.h includes <windows.h> on Windows, so keep that header's macros to a minimum
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#include "IniStore.h"
#include "core/SettingsTable.h"
#include "install/Installer.h"
#include "core/GameFiles.h"
#include "core/EngineText.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
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
            "english.cd", "french.cd", "german.cd", "italian.cd", "spanish.cd", // turned into [WoW Engine] "Language" in the ini
            "human.cd", "martian.cd", // the side markers: WoWLauncher tells WoWEngine the side (--human / --martian)
        };
        std::string lower = Lower(discPath);
        if (lower.rfind("directx/", 0) == 0 || lower.rfind("winsys/", 0) == 0) return true;
        return lower.find('/') == std::string::npos && files.count(lower) > 0;
    }

    // Where a disc file goes. The movies are laid out as WoWRevived's Smackw32 wrapper looks for them: TITLE and
    // RAGELOGO (the same on both discs) in FMV, and each disc's own INTRO, VICTORY and DEFEAT in FMV-Human or
    // FMV-Martian, so both sides keep theirs. Everything else keeps its place.
    std::string TargetPath(const std::string& discPath, Side side, bool& ownSide)
    {
        ownSide = false;
        std::string lower = Lower(discPath);
        if (lower.rfind("fmv/", 0) == 0 && lower.find('/', 4) == std::string::npos && fs::path(lower).extension() == ".smk")
        {
            std::string name = discPath.substr(4);
            std::string upper = name;
            for (char& c : upper) c = char(std::toupper(static_cast<unsigned char>(c)));
            upper = upper.substr(0, upper.size() - 4) + ".smk";
            if (upper == "TITLE.smk" || upper == "RAGELOGO.smk") return "FMV/" + upper;
            ownSide = true;
            return std::string(side == Side::Human ? "FMV-Human/" : "FMV-Martian/") + upper;
        }
        // (update 265, the owner) the manual the French and Italian Human discs have, in the main folder beside
        // TEXT.ojd and named for its language
        if (lower == "extras/manual/wotw-man-f-v1.pdf") return "wotw-man-french-v1.pdf";
        if (lower == "extras/manual/wotw-man-i-v1.pdf") return "wotw-man-italian-v1.pdf";
        return discPath;
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
            // The install's language is kept in WoWRevived.ini ([WoW Engine] "Language"), with the other settings (it
            // used to be a file of its own, language.txt, which is taken over and removed if it is there).
            // Discs from two different language releases make a mixed install (the Human disc's text wins).
            const fs::path iniPath = options.target / IniStore::FileName();
            IniStore ini;
            ini.Load(iniPath.string());
            std::string earlier;
            if (!ini.Get("WoW Engine", "Language", earlier) || earlier.empty())
            {
                std::ifstream old(options.target / "language.txt");
                if (old) std::getline(old, earlier);
                while (!earlier.empty() && (earlier.back() == '\r' || earlier.back() == ' ')) earlier.pop_back();
            }
            if (!earlier.empty() && earlier != info.language)
            {
                result.warnings.push_back("The discs are from different releases (" + earlier + " and " + info.language +
                    "); the game's text comes from the Human disc.");
                if (info.side != Side::Human) info.language = earlier;
            }
            ini.Set("WoW Engine", "Language", info.language);
            ini.Save();
            fs::remove(options.target / "language.txt", ec);
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
            bool ownSide = false;
            std::string target = TargetPath(f.path, info.side, ownSide);
            auto already = existing.Resolve(target);
            if (already && info.side != Side::Human && !ownSide) continue;
            fs::path out = already ? *already : options.target / fs::path(target);
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

    namespace
    {
        fs::path textFolder;
        bool removeTextFolder = false;

        // the five files gone, and their folder if nothing else is in it
        void RemoveTextFolder()
        {
            if (!removeTextFolder || textFolder.empty()) return;
            std::error_code ec;
            for (const char* code : { "EN", "FR", "DE", "IT", "ES" }) fs::remove(textFolder / (std::string("TEXT-") + code + ".ojd"), ec);
            // (and the playlist and English manual, moved to the main folder or not wanted: CopyExtras)
            for (const char* name : { "HumanMartianPlaylist.m3u8", "wotw-man-english-v1.pdf" }) fs::remove(textFolder / name, ec);
            if (fs::is_empty(textFolder, ec)) fs::remove(textFolder, ec);
            textFolder.clear();
        }
    }

    void SetTextFolder(fs::path folder, bool removeAfter)
    {
        textFolder = std::move(folder);
        removeTextFolder = removeAfter;
    }

    // (update 265, the owner) WoWRevived's music playlist into the main folder, and its English manual for an
    // English install (the French and Italian discs bring their own, TargetPath), from the folder the updated texts
    // come from; the release's copies go with that folder afterwards (RemoveTextFolder)
    void CopyExtras(const fs::path& target)
    {
        if (textFolder.empty()) return;
        IniStore ini;
        ini.Load((target / IniStore::FileName()).string());
        std::string language;
        ini.Get("WoW Engine", "Language", language);
        std::error_code ec;
        auto copy = [&](const char* name) {
            const fs::path from = textFolder / name;
            if (fs::is_regular_file(from, ec)) fs::copy_file(from, target / name, fs::copy_options::overwrite_existing, ec);
        };
        copy("HumanMartianPlaylist.m3u8");
        if (Lower(language).rfind("en", 0) == 0) copy("wotw-man-english-v1.pdf");
    }

    bool DocumentSettings(const fs::path& target)
    {
        IniStore ini;
        ini.Load((target / IniStore::FileName()).string());
        if (!settingstable::Document(ini)) return false;
        return ini.Save();
    }

    bool CopyLanguageText(const fs::path& target)
    {
        if (textFolder.empty()) return false;
        IniStore ini;
        ini.Load((target / IniStore::FileName()).string());
        std::string language;
        if (!ini.Get("WoW Engine", "Language", language) || language.empty())
        {
            // (an install made before the language was a setting has it in language.txt)
            std::ifstream old(target / "language.txt");
            if (!old || !std::getline(old, language)) return false;
        }
        language = Lower(language);
        const char* code = language.rfind("en", 0) == 0 ? "EN" : language.rfind("fr", 0) == 0 ? "FR" :
                           language.rfind("ge", 0) == 0 || language.rfind("de", 0) == 0 ? "DE" : language.rfind("it", 0) == 0 ? "IT" :
                           language.rfind("sp", 0) == 0 || language.rfind("es", 0) == 0 ? "ES" : nullptr;
        if (!code) return false;
        std::error_code ec;
        const fs::path updated = textFolder / (std::string("TEXT-") + code + ".ojd");
        if (!fs::is_regular_file(updated, ec)) return false;
        auto read = [](const fs::path& path) {
            std::ifstream in(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(in), {});
        };
        // WoWRevived's file, then the engine's own strings in that language (core/EngineText.h), each one that the
        // file doesn't already have, as one more record at its end: 0xFF, a line number, type 2 (both sides), the
        // id, the length with its 0, the text in Latin-1
        std::string wanted = read(updated);
        if (wanted.empty()) return false;
        {
            std::set<int> present;
            for (size_t pos = 0; pos + 9 <= wanted.size();)
            {
                auto u16 = [&](size_t at) { return int(uint8_t(wanted[at])) | int(uint8_t(wanted[at + 1])) << 8; };
                const int type = int16_t(u16(pos + 3)), id = u16(pos + 5), length = u16(pos + 7);
                pos += 9;
                if (type >= 0 && type <= 2) { present.insert(id); pos += size_t(length); }
                else if (type != 3 && type != 4) break;      // not a record: leave the rest alone
            }
            const enginetext::Language which = code[0] == 'F' ? enginetext::French : code[0] == 'D' ? enginetext::German :
                                               code[0] == 'I' ? enginetext::Italian : code[1] == 'S' ? enginetext::Spanish : enginetext::English;
            for (const enginetext::Entry& e : enginetext::Table)
            {
                if (present.count(e.id)) continue;
                const std::string text = enginetext::Latin1(e.text[which]);
                const size_t length = text.size() + 1;
                const char head[9] = { char(0xFF), char(e.id & 0xFF), char(e.id >> 8), 2, 0, char(e.id & 0xFF), char(e.id >> 8),
                                       char(length & 0xFF), char(length >> 8) };
                wanted.append(head, 9);
                wanted.append(text);
                wanted.push_back('\0');
            }
        }
        // over the game's own, whatever case its name has on disk; nothing is written when it is that already
        const auto existing = wow::GameFiles(target).Resolve("TEXT.ojd");
        const fs::path game = existing ? *existing : target / "TEXT.ojd";
        if (existing && read(game) == wanted) { RemoveTextFolder(); return true; }
        std::ofstream out(game, std::ios::binary | std::ios::trunc);
        out.write(wanted.data(), std::streamsize(wanted.size()));
        out.close();
        if (!out) return false;
        RemoveTextFolder();      // (the copy is in: the files it came from aren't needed again)
        return true;
    }

    void Finish(const InstallOptions& options, InstallResult& result)
    {
        CopyExtras(options.target);         // (before the text, which takes the folder away when it's done)
        CopyLanguageText(options.target);
        DocumentSettings(options.target);   // (update 248) every setting's description and default in WoWRevived.ini
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
