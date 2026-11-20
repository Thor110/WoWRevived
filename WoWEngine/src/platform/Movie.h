// The game's movies (0x44DE20 -> 0x44F2C0): RAGELOGO and TITLE at start-up, INTRO for a new campaign, VICTORY
// and DEFEAT at the end. The original plays FMV\<name>.smk with SMACKW32.DLL; WoWRevived's Smackw32 wrapper plays
// its own .mp4 versions instead, TITLE and RAGELOGO from FMV, the rest from FMV-Human or FMV-Martian by the side.
// WoWEngine looks in the same places: the .mp4 first (Windows: Media Foundation), then the .smk there, then the
// .smk in FMV (an install from before the movies were split by side).
#pragma once
#include "core/GameFiles.h"

#include <SDL3/SDL_audio.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace wow
{
    class Movie
    {
    public:
        virtual ~Movie() = default;

        // Finds and opens the movie, or returns null (with the reason in error).
        static std::unique_ptr<Movie> Open(const GameFiles& files, const std::string& name, bool human, std::string& error);

        virtual int Width() const = 0;      // the picture as shown
        virtual int Height() const = 0;
        virtual int AudioRate() const = 0;  // 0 without sound
        virtual int AudioChannels() const = 0;
        // Decodes the next frame (Frame(), shown from Time() seconds in) and appends its sound, 16-bit and
        // interleaved. False after the last.
        virtual bool Next(std::vector<int16_t>& audio) = 0;
        virtual double Time() const = 0;
        virtual const std::vector<uint32_t>& Frame() const = 0; // Width() x Height(), 0xRRGGBB
    };

    // A movie's sound, on a stream of its own next to the sound effects.
    class MovieAudio
    {
    public:
        MovieAudio(int rate, int channels);
        ~MovieAudio();
        MovieAudio(const MovieAudio&) = delete;
        MovieAudio& operator=(const MovieAudio&) = delete;

        bool Available() const { return stream != nullptr; }
        void Push(const std::vector<int16_t>& samples);
        void SetVolume(float gain);
        double Queued() const;   // seconds not played yet
        double Played() const;   // seconds played so far

    private:
        SDL_AudioStream* stream = nullptr;
        int rate = 0, channels = 0;
        uint64_t pushed = 0;     // sample frames
    };

#ifdef _WIN32
    std::unique_ptr<Movie> OpenMp4(const std::filesystem::path& path, std::string& error); // Mp4Movie.cpp
#endif
}
