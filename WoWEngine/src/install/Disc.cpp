#include "install/Disc.h"
#include "install/DriveAudio.h"
#include "install/Iso9660.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace wow::install
{
    namespace
    {
        constexpr size_t RawSector = 2352;

        std::string Lower(std::string s)
        {
            for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        bool Stream(std::ifstream& in, uint64_t offset, uint64_t length, const std::function<void(const uint8_t*, size_t)>& out)
        {
            std::vector<uint8_t> buffer(1 << 20);
            in.clear();
            in.seekg(std::streamoff(offset));
            while (length > 0)
            {
                size_t n = size_t(std::min<uint64_t>(length, buffer.size()));
                if (!in.read(reinterpret_cast<char*>(buffer.data()), std::streamsize(n))) return false;
                out(buffer.data(), n);
                length -= n;
            }
            return true;
        }

        // A mounted disc, or any folder holding a copy of one. Audio comes from the drive, if it is one.
        class FolderDisc : public Disc
        {
        public:
            explicit FolderDisc(fs::path root) : root(std::move(root))
            {
                std::string device = DeviceForPath(this->root);
                if (!device.empty()) drive = OpenDriveAudio(device);
            }

            std::string Describe() const override { return root.string() + (drive ? " (drive)" : " (folder)"); }

            std::vector<DiscFile> Files() override
            {
                std::vector<DiscFile> files;
                std::error_code ec;
                for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
                    if (it->is_regular_file(ec)) files.push_back({ fs::relative(it->path(), root, ec).generic_string(), uint64_t(it->file_size(ec)) });
                return files;
            }

            bool ReadFile(const DiscFile& file, const std::function<void(const uint8_t*, size_t)>& out) override
            {
                std::ifstream in(root / fs::path(file.path), std::ios::binary);
                return in && Stream(in, 0, file.size, out);
            }

            std::vector<AudioTrack> Tracks() override { return drive ? drive->Tracks() : std::vector<AudioTrack>{}; }
            bool ReadTrack(const AudioTrack& t, const std::function<void(const uint8_t*, size_t)>& out) override { return drive && drive->ReadTrack(t, out); }

        private:
            fs::path root;
            std::unique_ptr<DriveAudio> drive;
        };

        // An .iso image (the data track only), or on Linux a drive's device read directly.
        class IsoDisc : public Disc
        {
        public:
            explicit IsoDisc(fs::path path) : path(std::move(path)), in(this->path, std::ios::binary)
            {
                if (!in) throw std::runtime_error("can't open " + this->path.string());
                entries = ReadIsoDirectory(Reader());
                drive = OpenDriveAudio(this->path.string()); // only succeeds for a device such as /dev/sr0
            }

            std::string Describe() const override { return path.string() + (drive ? " (drive)" : " (ISO image, no music)"); }

            std::vector<DiscFile> Files() override
            {
                std::vector<DiscFile> files;
                for (const IsoEntry& e : entries) files.push_back(e.file);
                return files;
            }

            bool ReadFile(const DiscFile& file, const std::function<void(const uint8_t*, size_t)>& out) override
            {
                for (const IsoEntry& e : entries)
                    if (e.file.path == file.path) return ReadIsoFile(Reader(), e, out);
                return false;
            }

            std::vector<AudioTrack> Tracks() override { return drive ? drive->Tracks() : std::vector<AudioTrack>{}; }
            bool ReadTrack(const AudioTrack& t, const std::function<void(const uint8_t*, size_t)>& out) override { return drive && drive->ReadTrack(t, out); }

        private:
            SectorReader Reader()
            {
                return [this](uint32_t lba, uint8_t* sector) {
                    in.clear();
                    in.seekg(std::streamoff(uint64_t(lba) * 2048));
                    return bool(in.read(reinterpret_cast<char*>(sector), 2048));
                };
            }

            fs::path path;
            std::ifstream in;
            std::vector<IsoEntry> entries;
            std::unique_ptr<DriveAudio> drive;
        };

        // A .cue sheet with one or more .bin files: the data track's ISO 9660 file system and the audio tracks.
        class CueDisc : public Disc
        {
        public:
            explicit CueDisc(const fs::path& cue) : cuePath(cue)
            {
                Parse(cue);
                const Track* data = nullptr;
                for (const Track& t : tracks) if (!t.audio) { data = &t; break; }
                if (!data) throw std::runtime_error(cue.string() + ": no data track");
                dataTrack = *data;
                dataFile.open(dataTrack.file, std::ios::binary);
                if (!dataFile) throw std::runtime_error("can't open " + dataTrack.file.string());
                entries = ReadIsoDirectory(Reader());
            }

            std::string Describe() const override { return cuePath.string() + " (BIN/CUE image)"; }

            std::vector<DiscFile> Files() override
            {
                std::vector<DiscFile> files;
                for (const IsoEntry& e : entries) files.push_back(e.file);
                return files;
            }

            bool ReadFile(const DiscFile& file, const std::function<void(const uint8_t*, size_t)>& out) override
            {
                for (const IsoEntry& e : entries)
                    if (e.file.path == file.path) return ReadIsoFile(Reader(), e, out);
                return false;
            }

            std::vector<AudioTrack> Tracks() override
            {
                std::vector<AudioTrack> out;
                for (const Track& t : tracks) if (t.audio) out.push_back({ t.number, t.frames });
                return out;
            }

            bool ReadTrack(const AudioTrack& track, const std::function<void(const uint8_t*, size_t)>& out) override
            {
                for (const Track& t : tracks)
                {
                    if (t.number != track.number || !t.audio) continue;
                    std::ifstream in(t.file, std::ios::binary);
                    return in && Stream(in, t.startFrame * RawSector, t.frames * RawSector, out);
                }
                return false;
            }

        private:
            struct Track
            {
                int number = 0;
                bool audio = false;
                size_t sectorSize = RawSector, dataOffset = 16; // MODE1/2352: user data after the 16-byte header
                fs::path file;
                uint64_t index0 = 0, startFrame = 0, frames = 0; // frames within file
                bool hasIndex0 = false;
            };

            void Parse(const fs::path& cue)
            {
                std::ifstream in(cue);
                if (!in) throw std::runtime_error("can't open " + cue.string());
                fs::path currentFile;
                std::string line;
                while (std::getline(in, line))
                {
                    std::istringstream words(line);
                    std::string keyword;
                    words >> keyword;
                    keyword = Lower(keyword);
                    if (keyword == "file")
                    {
                        // FILE "name.bin" BINARY  (the name may contain spaces)
                        size_t a = line.find('"'), b = line.rfind('"');
                        std::string name = a != std::string::npos && b > a ? line.substr(a + 1, b - a - 1) : std::string();
                        if (name.empty()) { words >> name; }
                        currentFile = cue.parent_path() / fs::path(name);
                        if (!fs::exists(currentFile))
                        {
                            // images copied between systems often change the case of the .bin name
                            std::error_code ec;
                            for (fs::directory_iterator it(cue.parent_path(), ec), end; !ec && it != end; it.increment(ec))
                                if (Lower(it->path().filename().string()) == Lower(fs::path(name).filename().string())) currentFile = it->path();
                        }
                    }
                    else if (keyword == "track")
                    {
                        Track t;
                        std::string mode;
                        words >> t.number >> mode;
                        mode = Lower(mode);
                        t.audio = mode == "audio";
                        if (mode == "mode1/2048") { t.sectorSize = 2048; t.dataOffset = 0; }
                        else if (mode == "mode2/2352") t.dataOffset = 24; // form 1: 16-byte header and 8-byte subheader
                        else if (mode == "mode2/2336") { t.sectorSize = 2336; t.dataOffset = 8; }
                        t.file = currentFile;
                        tracks.push_back(t);
                    }
                    else if (keyword == "index" && !tracks.empty())
                    {
                        int number = 0;
                        std::string msf;
                        words >> number >> msf;
                        int m = 0, s = 0, f = 0;
                        if (std::sscanf(msf.c_str(), "%d:%d:%d", &m, &s, &f) != 3) continue;
                        uint64_t frame = uint64_t(m) * 4500 + uint64_t(s) * 75 + uint64_t(f);
                        if (number == 0) { tracks.back().index0 = frame; tracks.back().hasIndex0 = true; }
                        else if (number == 1) tracks.back().startFrame = frame;
                    }
                }
                if (tracks.empty()) throw std::runtime_error(cue.string() + ": no tracks");
                // each track runs to the next one's pregap (or start) in the same file, or to the end of its file
                for (size_t i = 0; i < tracks.size(); i++)
                {
                    Track& t = tracks[i];
                    uint64_t end = 0;
                    if (i + 1 < tracks.size() && tracks[i + 1].file == t.file)
                        end = tracks[i + 1].hasIndex0 ? tracks[i + 1].index0 : tracks[i + 1].startFrame;
                    else
                    {
                        std::error_code ec;
                        end = uint64_t(fs::file_size(t.file, ec)) / t.sectorSize;
                    }
                    t.frames = end > t.startFrame ? end - t.startFrame : 0;
                }
            }

            SectorReader Reader()
            {
                return [this](uint32_t lba, uint8_t* sector) {
                    dataFile.clear();
                    dataFile.seekg(std::streamoff((dataTrack.startFrame + lba) * dataTrack.sectorSize + dataTrack.dataOffset));
                    return bool(dataFile.read(reinterpret_cast<char*>(sector), 2048));
                };
            }

            fs::path cuePath;
            std::vector<Track> tracks;
            Track dataTrack;
            std::ifstream dataFile;
            std::vector<IsoEntry> entries;
        };
    }

    std::unique_ptr<Disc> OpenDisc(const fs::path& source)
    {
        std::error_code ec;
        if (fs::is_directory(source, ec)) return std::make_unique<FolderDisc>(source);
        std::string ext = Lower(source.extension().string());
        if (ext == ".cue") return std::make_unique<CueDisc>(source);
        if (ext == ".bin")
        {
            fs::path cue = source;
            cue.replace_extension(".cue");
            if (fs::exists(cue, ec)) return std::make_unique<CueDisc>(cue);
            throw std::runtime_error(source.string() + ": open the .cue file that goes with this .bin");
        }
        if (!fs::exists(source, ec)) throw std::runtime_error(source.string() + " doesn't exist");
        return std::make_unique<IsoDisc>(source); // .iso, or a device such as /dev/sr0
    }

    std::vector<fs::path> FindDrives()
    {
        std::vector<fs::path> drives;
#if defined(_WIN32)
        DWORD mask = GetLogicalDrives();
        for (int i = 0; i < 26; i++)
        {
            if (!(mask & (1u << i))) continue;
            std::string root = std::string(1, char('A' + i)) + ":\\";
            if (GetDriveTypeA(root.c_str()) == DRIVE_CDROM) drives.emplace_back(root);
        }
#elif defined(__APPLE__)
        std::error_code ec;
        for (fs::directory_iterator it("/Volumes", ec), end; !ec && it != end; it.increment(ec)) drives.push_back(it->path());
#else
        std::error_code ec;
        for (int i = 0; i < 8; i++)
        {
            fs::path device = "/dev/sr" + std::to_string(i);
            if (fs::exists(device, ec)) drives.push_back(device);
        }
#endif
        return drives;
    }
}
