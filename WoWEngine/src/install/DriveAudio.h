// Raw reading of a physical CD's audio tracks (the table of contents and 2352-byte CD-DA frames).
// Windows: IOCTL_CDROM_READ_TOC / IOCTL_CDROM_RAW_READ on \\.\X:.  Linux: CDROMREADTOCENTRY / CDROMREADAUDIO on
// the drive's device. macOS doesn't allow raw reads; there the audio has to come from a .bin/.cue image.
#pragma once
#include "install/Disc.h"

#include <memory>
#include <string>

namespace wow::install
{
    class DriveAudio
    {
    public:
        virtual ~DriveAudio() = default;
        virtual std::vector<AudioTrack> Tracks() = 0;
        virtual bool ReadTrack(const AudioTrack& track, const std::function<void(const uint8_t*, size_t)>& out) = 0;
    };

    // device: "D:" on Windows, "/dev/sr0" on Linux. Returns nullptr if it isn't an audio-capable optical drive.
    std::unique_ptr<DriveAudio> OpenDriveAudio(const std::string& device);

    // The device behind a mounted path ("D:\" -> "D:", "/media/cdrom" -> "/dev/sr0"), or "" if there isn't one.
    std::string DeviceForPath(const std::filesystem::path& path);
}
