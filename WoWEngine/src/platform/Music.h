// Music playback for the CD Player and the in-game music.
// The game played Red Book CD audio through MCI; WoW Revived's installer rips tracks 2-5 of each disc to
// "Music-Human\NN TrackNN.wav" and "Music-Martian\NN TrackNN.wav" (the names the _inmm wrapper already uses).
// A track is streamed from its file as the audio device asks for it: a track is about 55 MB, and read whole it
// stalled the game for seconds on a cold disk as the war map's music started. The position is what's been fed
// less what's still queued.
#pragma once
#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
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

        // Starts a WAV file from the beginning. Returns false (and stays stopped) if it can't be opened or played.
        bool Play(const std::filesystem::path& wav);
        void Stop();
        void SetPaused(bool paused);
        void SetVolume(float volume); // 0-1, kept across tracks

        // the CD track this is (2-5), for the CD Player to pick up (the CD's current track, 0x413D30); 0: none
        void SetTrack(int t) { track = t; }
        int Track() const { return track; }

        bool Playing() const { return stream != nullptr; }
        bool Paused() const { return paused; }
        bool Finished() const;          // played to the end
        uint32_t PositionMs() const;
        uint32_t LengthMs() const { return lengthMs; }
        // Moves the position by this many milliseconds, kept to the track (the CD's 0x413740 / 0x413990)
        void Seek(int deltaMs);

    private:
        // the device asking for more (called with the stream locked; everything that touches the file or `fed`
        // locks the stream too, so it's the one lock)
        static void SDLCALL Feed(void* self, SDL_AudioStream* stream, int additional, int total);

        SDL_AudioStream* stream = nullptr;
        std::FILE* file = nullptr;
        uint64_t dataStart = 0;         // the data chunk's place in the file
        uint32_t lengthBytes = 0, lengthMs = 0, bytesPerSecond = 1, frameSize = 1;
        uint32_t fed = 0;               // bytes of the data handed to the stream so far
        bool paused = false;
        float volume = 1.0f;
        int track = 0;
    };
}
