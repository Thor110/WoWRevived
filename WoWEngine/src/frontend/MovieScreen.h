// Playing a movie (the game's 0x8041 state, 0x44F2C0): the picture centred on the black 640 x 480 screen
// (0x44F2C0 works the position out as (640 - width) / 2, (480 - height) / 2), until its last frame or until
// Escape (0x44E3E0 checks VK_ESCAPE every frame), then the command that comes after it.
#pragma once
#include "frontend/Screen.h"
#include "platform/Movie.h"

#include <deque>
#include <memory>

namespace wow
{
    class MovieScreen : public Screen
    {
    public:
        static constexpr int Id = 0x8041;

        // volume: the sound's gain (the master volume)
        MovieScreen(Resources& res, std::unique_ptr<Movie> movie, Command next, float volume);

        void Update() override;
        void Draw(Canvas& canvas) const override;
        std::optional<Command> OnKey(int sdlKey) override;
        std::optional<Command> OnMouseButton(int, int, bool) override { return std::nullopt; }
        int CursorAt(int, int) const override { return -1; } // no cursor over a movie
        // the movie pauses with the game (0x44E440)
        void SetPaused(bool paused) override { if (audio) audio->SetPaused(paused); }

    private:
        void Finish();

        std::unique_ptr<Movie> movie;
        std::unique_ptr<MovieAudio> audio;
        Command next;
        // Frames are decoded ahead of the clock, so their sound reaches the audio stream in good time: up to
        // Ahead seconds of it, or Most frames when there's no sound.
        static constexpr double Ahead = 0.3;
        static constexpr size_t Most = 12;
        struct Decoded { double time; std::vector<uint32_t> pixels; };
        std::deque<Decoded> queue;
        bool ended = false;           // the movie has no more frames
        std::vector<uint32_t> shown;  // the frame on screen
        int shownWidth = 0, shownHeight = 0;
        bool finished = false;
        double clock = 0;             // seconds into the movie
        std::vector<int16_t> sound;
        bool audioEnded = false;      // a separate sound track has no more
        // With a separate sound track: kept this far ahead, and at most this many frames decoded an update (late
        // frames are shown only if their time hasn't passed)
        static constexpr double SoundAhead = 1.0;
        static constexpr int MostFramesAnUpdate = 2;
        void UpdateSeparate();
        // for the log: updates the sound ran dry on, frames decoded and shown, the slowest decode
        int underruns = 0, decoded = 0, shownCount = 0;
        double slowestDecode = 0;
    };
}
