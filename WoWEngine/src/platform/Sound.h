// Sound effects, speech and the menu and credits music that come from the sound libraries (SFX.ojd, see
// core/SoundTable.h), mixed in software into one SDL audio stream.
//
// The game's sound objects (0x4648C0, 0x54 bytes) pick a sample from the sound's first variation group, play it
// at the sound's volume scaled by the [Sound\Volume] setting for its type and by Master, and can fade in over a
// number of ticks (0x464E60). Looping sounds (opcode 7) play until they're stopped; others end with their sample.
#pragma once
#include "core/SoundTable.h"

#include <SDL3/SDL.h>

#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace wow
{
    class Sound
    {
    public:
        Sound(const GameFiles& files, bool human);
        ~Sound();
        Sound(const Sound&) = delete;
        Sound& operator=(const Sound&) = delete;

        // Starts a sound by its SFX.ojd id. Returns a handle, or 0 if it can't be played (unknown id, no sample,
        // no audio device).
        int Play(int id, float volume = 1.0f);
        void Stop(int handle);
        // Ramps the sound up from silence over this many game ticks (30 a second).
        void FadeIn(int handle, int ticks);
        bool Active(int handle) const;
        // Fades the sound out to silence over this many ticks and stops it (0x464EB0); 0 stops it at once.
        void FadeOut(int handle, int ticks);
        // Fades every sound playing now out to silence over this many ticks and stops it (0x464AB0, "Fading out
        // all current sounds"); 0 stops them at once.
        void FadeOutAll(int ticks);

        // [Sound\Volume] Master and the per-type volumes, 0-1.
        void SetVolumes(float master, const float typeVolumes[6]);

        // Once per game tick: fades.
        void Update();

        // Stops or restarts the mixing altogether (the game deactivated, 0x404BC0).
        void SetPaused(bool paused);

        // While the war map menu (0x80B5) is open every sound but speech is at a quarter (0x41CF6C / 0x41CFAC).
        void SetMenuDuck(bool on);

        bool Available() const { return stream != nullptr; }
        float Master() const { return master; }
        const SoundTable& Table() const { return table; }

    private:
        struct Voice
        {
            int handle = 0;
            std::shared_ptr<const std::vector<int16_t>> pcm;
            double position = 0, step = 1; // in frames
            int channels = 1;                 // 2 for a stereo sound (SFX.ojd record 15)
            bool loop = false;
            int type = 0;
            float gain = 1;           // the sound's own volume times the volume it was played at
            float fade = 1;           // 0-1 while fading in
            float speechFade = 1;     // 0.7 once newer speech starts over it (0x463640)
            uint64_t age = 0;         // when it started, for stealing the oldest
            int fadeTicks = 0, fadeDone = 0;
            int fadeOutTicks = 0, fadeOutDone = 0; // fading out, then stopped
        };

        static void SDLCALL Feed(void* self, SDL_AudioStream* stream, int additional, int total);
        void Mix(float* out, int frames);
        std::shared_ptr<const std::vector<int16_t>> Sample(const SoundSample& sample);

        SoundTable table;
        SoundLibrary library;
        std::map<std::string, std::shared_ptr<const std::vector<int16_t>>> cache;
        SDL_AudioStream* stream = nullptr;
        mutable std::mutex lock;
        std::vector<Voice> voices;
        int nextHandle = 1;
        uint64_t started = 0;
        bool menuDuck = false;
        std::map<int, size_t> nextVariation; // round-robin cursor per sound id (0x466BC0)
        static float Curve(float volume);     // the 128-step decibel table (0x465B20)
        float master = 1.0f, typeVolume[6] = { 1, 1, 1, 1, 1, 1 };
        std::vector<float> mixBuffer;
    };
}
