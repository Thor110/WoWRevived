// Reading the game's discs, from a mounted drive or folder, an .iso image or a .bin/.cue image.
//
// Both discs are mixed mode: track 1 is an ISO 9660 data track, tracks 2-5 are Red Book audio (the music, which
// the game played straight from the CD). An .iso holds only the data track; a .bin/.cue holds everything, so it
// is the only image the music can be ripped from. On a physical drive the audio is read with the drive's raw
// read command (Windows, Linux; see DriveAudio.cpp).
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace wow::install
{
    struct DiscFile
    {
        std::string path; // relative, '/' separated, as on the disc
        uint64_t size = 0;
    };

    struct AudioTrack
    {
        int number = 0;
        uint64_t frames = 0; // 1 frame = 2352 bytes = 588 stereo 16-bit samples at 44.1 kHz
    };

    class Disc
    {
    public:
        virtual ~Disc() = default;
        virtual std::string Describe() const = 0;
        virtual std::vector<DiscFile> Files() = 0;
        // Copies a file to out in chunks; returns false if it can't be read.
        virtual bool ReadFile(const DiscFile& file, const std::function<void(const uint8_t*, size_t)>& out) = 0;
        virtual std::vector<AudioTrack> Tracks() = 0;
        // Reads an audio track's raw 16-bit stereo samples in chunks; returns false if it can't be read.
        virtual bool ReadTrack(const AudioTrack& track, const std::function<void(const uint8_t*, size_t)>& out) = 0;
    };

    // Opens a folder (a mounted disc), an .iso or a .cue. Throws std::runtime_error if it can't be opened.
    std::unique_ptr<Disc> OpenDisc(const std::filesystem::path& source);

    // Optical drives this computer has, as paths OpenDisc accepts ("D:\", "/dev/sr0" and so on).
    std::vector<std::filesystem::path> FindDrives();
}
