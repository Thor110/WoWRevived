#include "install/DriveAudio.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <fstream>
#include <map>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winioctl.h>
#include <ntddcdrm.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <linux/cdrom.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace wow::install
{
    namespace
    {
        constexpr size_t FrameBytes = 2352;

        // start frame of each track and of the lead-out, from the drive's table of contents
        struct Toc
        {
            std::map<int, uint32_t> start; // track -> first frame
            std::map<int, bool> audio;
            uint32_t leadOut = 0;

            std::vector<AudioTrack> AudioTracks() const
            {
                std::vector<AudioTrack> tracks;
                for (auto it = start.begin(); it != start.end(); ++it)
                {
                    if (!audio.at(it->first)) continue;
                    auto next = std::next(it);
                    uint32_t end = next == start.end() ? leadOut : next->second;
                    if (end > it->second) tracks.push_back({ it->first, end - it->second });
                }
                return tracks;
            }
        };

#if defined(_WIN32)
        class WindowsDrive : public DriveAudio
        {
        public:
            explicit WindowsDrive(HANDLE handle) : handle(handle) { ReadToc(); }
            ~WindowsDrive() override { CloseHandle(handle); }

            std::vector<AudioTrack> Tracks() override { return toc.AudioTracks(); }

            bool ReadTrack(const AudioTrack& track, const std::function<void(const uint8_t*, size_t)>& out) override
            {
                auto it = toc.start.find(track.number);
                if (it == toc.start.end()) return false;
                constexpr DWORD Batch = 20; // drives limit how many CD-DA frames one request may read
                std::vector<uint8_t> buffer(Batch * FrameBytes);
                for (uint64_t done = 0; done < track.frames;)
                {
                    DWORD count = DWORD(std::min<uint64_t>(Batch, track.frames - done));
                    RAW_READ_INFO info{};
                    info.DiskOffset.QuadPart = int64_t(it->second + done) * 2048; // the drive expects a 2048-byte offset
                    info.SectorCount = count;
                    info.TrackMode = CDDA;
                    DWORD got = 0;
                    if (!DeviceIoControl(handle, IOCTL_CDROM_RAW_READ, &info, sizeof(info), buffer.data(), DWORD(buffer.size()), &got, nullptr))
                        return false;
                    out(buffer.data(), size_t(count) * FrameBytes);
                    done += count;
                }
                return true;
            }

        private:
            void ReadToc()
            {
                CDROM_TOC raw{};
                DWORD got = 0;
                if (!DeviceIoControl(handle, IOCTL_CDROM_READ_TOC, nullptr, 0, &raw, sizeof(raw), &got, nullptr)) return;
                auto frame = [](const TRACK_DATA& t) { return uint32_t(t.Address[1]) * 4500 + uint32_t(t.Address[2]) * 75 + t.Address[3] - 150; };
                int count = raw.LastTrack - raw.FirstTrack + 1;
                for (int i = 0; i <= count && i < MAXIMUM_NUMBER_TRACKS; i++)
                {
                    const TRACK_DATA& t = raw.TrackData[i];
                    if (t.TrackNumber == 0xAA) { toc.leadOut = frame(t); break; }
                    toc.start[t.TrackNumber] = frame(t);
                    toc.audio[t.TrackNumber] = (t.Control & 4) == 0;
                }
            }

            HANDLE handle;
            Toc toc;
        };
#elif defined(__linux__)
        class LinuxDrive : public DriveAudio
        {
        public:
            explicit LinuxDrive(int fd) : fd(fd) { ReadToc(); }
            ~LinuxDrive() override { close(fd); }

            std::vector<AudioTrack> Tracks() override { return toc.AudioTracks(); }

            bool ReadTrack(const AudioTrack& track, const std::function<void(const uint8_t*, size_t)>& out) override
            {
                auto it = toc.start.find(track.number);
                if (it == toc.start.end()) return false;
                constexpr int Batch = 75; // one second
                std::vector<uint8_t> buffer(Batch * FrameBytes);
                for (uint64_t done = 0; done < track.frames;)
                {
                    int count = int(std::min<uint64_t>(Batch, track.frames - done));
                    cdrom_read_audio request{};
                    request.addr.lba = int(it->second + done);
                    request.addr_format = CDROM_LBA;
                    request.nframes = count;
                    request.buf = buffer.data();
                    if (ioctl(fd, CDROMREADAUDIO, &request) < 0) return false;
                    out(buffer.data(), size_t(count) * FrameBytes);
                    done += uint64_t(count);
                }
                return true;
            }

        private:
            void ReadToc()
            {
                cdrom_tochdr header{};
                if (ioctl(fd, CDROMREADTOCHDR, &header) < 0) return;
                for (int t = header.cdth_trk0; t <= header.cdth_trk1; t++)
                {
                    cdrom_tocentry entry{};
                    entry.cdte_track = uint8_t(t);
                    entry.cdte_format = CDROM_LBA;
                    if (ioctl(fd, CDROMREADTOCENTRY, &entry) < 0) continue;
                    toc.start[t] = uint32_t(entry.cdte_addr.lba);
                    toc.audio[t] = (entry.cdte_ctrl & CDROM_DATA_TRACK) == 0;
                }
                cdrom_tocentry leadOut{};
                leadOut.cdte_track = CDROM_LEADOUT;
                leadOut.cdte_format = CDROM_LBA;
                if (ioctl(fd, CDROMREADTOCENTRY, &leadOut) == 0) toc.leadOut = uint32_t(leadOut.cdte_addr.lba);
            }

            int fd;
            Toc toc;
        };
#endif
    }

    std::unique_ptr<DriveAudio> OpenDriveAudio(const std::string& device)
    {
#if defined(_WIN32)
        if (device.size() < 2 || device[1] != ':') return nullptr;
        std::string root = device.substr(0, 2) + "\\";
        if (GetDriveTypeA(root.c_str()) != DRIVE_CDROM) return nullptr;
        std::string path = "\\\\.\\" + device.substr(0, 2);
        HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return nullptr;
        return std::make_unique<WindowsDrive>(handle);
#elif defined(__linux__)
        int fd = open(device.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) return nullptr;
        if (ioctl(fd, CDROM_GET_CAPABILITY, 0) < 0) { close(fd); return nullptr; }
        return std::make_unique<LinuxDrive>(fd);
#else
        (void)device;
        return nullptr;
#endif
    }

    std::string DeviceForPath(const std::filesystem::path& path)
    {
#if defined(_WIN32)
        std::string s = path.string();
        if (s.size() >= 2 && s[1] == ':') return s.substr(0, 2);
        return "";
#elif defined(__linux__)
        std::error_code ec;
        auto target = std::filesystem::weakly_canonical(path, ec);
        std::ifstream mounts("/proc/mounts");
        std::string line, best;
        size_t bestLength = 0;
        while (std::getline(mounts, line))
        {
            std::istringstream fields(line);
            std::string device, mountPoint;
            fields >> device >> mountPoint;
            if (device.rfind("/dev/", 0) != 0) continue;
            std::string t = target.string();
            if (t.rfind(mountPoint, 0) == 0 && mountPoint.size() > bestLength) { best = device; bestLength = mountPoint.size(); }
        }
        return best;
#else
        (void)path;
        return "";
#endif
    }
}
