// MODERN (update 270, the owner): a custom campaign's briefing read out. One sound held whole (a minute or two of
// speech decoded at 44.1 kHz stereo is about 20 MB), on an audio stream of its own, so it plays over whatever the
// menus' music is doing and stops as the briefing goes.
#pragma once
#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace wow
{
    class Narration
    {
    public:
        Narration() = default;
        ~Narration() { Stop(); }
        Narration(const Narration&) = delete;
        Narration& operator=(const Narration&) = delete;

        // Interleaved 16-bit samples at this rate and channel count; false if no audio device takes it
        bool Play(const std::vector<int16_t>& samples, int rate, int channels, float volume);
        void Stop();
        bool Playing() const { return stream != nullptr; }

    private:
        SDL_AudioStream* stream = nullptr;
    };
}
