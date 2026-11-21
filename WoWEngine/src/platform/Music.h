// Music playback for the CD Player and, later, the in-game music.
// The game played Red Book CD audio through MCI; WoW Revived's installer rips tracks 2-5 of each disc to
// "Music-Human\NN TrackNN.wav" and "Music-Martian\NN TrackNN.wav" (the names the _inmm wrapper already uses).
// A track is loaded whole and queued on an SDL audio stream; the position comes from how much is left queued.
#pragma once
#include <vector>
#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>

namespace wow
{
    class Music
    {
    public:
        Music() = default;
        ~Music();
        Music(const Music&) = delete;
        Music& operator=(const Music&) = delete;

        // Starts a WAV file from the beginning. Returns false (and stays stopped) if it can't be loaded or played.
        bool Play(const std::filesystem::path& wav);
        void Stop();
        void SetPaused(bool paused);
        void SetVolume(float volume); // 0-1, kept across tracks

        bool Playing() const { return stream != nullptr; }
        bool Paused() const { return paused; }
        bool Finished() const;          // played to the end
        uint32_t PositionMs() const;
        uint32_t LengthMs() const { return lengthMs; }
        // Moves the position by this many milliseconds, kept to the track (the CD's 0x413740 / 0x413990)
        void Seek(int deltaMs);

    private:
        std::vector<uint8_t> pcm;       // the whole track, to seek in
        uint32_t frameSize = 1;
        SDL_AudioStream* stream = nullptr;
        uint32_t lengthBytes = 0, lengthMs = 0, bytesPerSecond = 1;
        bool paused = false;
        float volume = 1.0f;
    };
}
