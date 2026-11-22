// Sound effects, speech and the menu and credits music that come from the sound libraries (SFX.ojd, see
// core/SoundTable.h), mixed in software into one SDL audio stream.
//
// The game's sound objects (0x4648C0, 0x54 bytes) pick a sample from one of the sound's variation groups in turn,
// play it at the sound's volume scaled by the [Sound\Volume] setting for its type and by Master and, for one
// played at a point, by how far that is from the listener (0x464FF0) with the pan from its x, and can fade in over
// a number of ticks (0x464E60).
// *Provisional*: a quiet positional sound keeps its voice (the game lets it go and takes one again when it's
// louder), and the volume and pan are set every update rather than on alternate ones. Looping sounds (opcode 7) play until they're stopped; others end with their sample.
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

        // Starts a sound by its SFX.ojd id (0x464EE0): a sample from the variation group (speech's three are the
        // voices), at a point in screen space (0, 0 for none). Returns a handle, or 0 if it can't be played (unknown
        // id, no sample, no audio device, or a positional sound beyond the outer border).
        int Play(int id, float volume = 1.0f, int group = 0, int x = 0, int y = 0);
        // The listener (dword_4E9250 / dy): the view's centre in the same screen space.
        void SetListener(int x, int y);
        // [Sound] "Inner border", "Outer border", "Inner Ambient border", "Outer Ambient border" (0x464580-0x464640):
        // each left, top, right, bottom about the listener. A sound is at full volume inside the inner one and
        // silent outside the outer one (0x464FF0); ambient sounds use the larger pair.
        struct Border { int left, top, right, bottom; };
        void SetBorders(const Border& inner, const Border& outer, const Border& innerAmbient, const Border& outerAmbient);
        void Stop(int handle);
        // How many game ticks (30 a second) the sample the sound would play next lasts (its first group), 0 if none.
        int LengthTicks(int id);
        // Moves a playing sound's point (0x489290: its +0x34 / +0x38 to an object's screen point), so its volume
        // and pan follow it.
        void Move(int handle, int x, int y);
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
        void SetMinimumAudible(float v) { minimumAudible = v; }

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
            bool placed = false;      // played at a point (not 0, 0)
            int x = 0, y = 0;
            float attenuation = 1;    // 0x464FF0 against the listener
            float pan = 0;            // DSBPAN units: the point's x from the listener x 4 (not speech)
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
            // Ambient and engine sounds are virtual until the update hands them a voice (0x462FF0 / 0x463C30):
            // unvoiced they go on in time, silent, so a voice given back later picks up where it would be (0x4657B0)
            bool voiced = true;
            float hand = 1;           // the ambient crossfade: in over 15 ticks, out over 150 (0x463C30, 0x4655F0)
            int handIn = 0, handOut = 0;
        };
        void HandOutVoices();           // 0x462FF0's ambient (0x463C30) and engine parts
        float minimumAudible = 0.05f;   // [Sound] "Minimum Audible Volume" (dword_4CA874, read by 0x462FF0)

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
        std::map<std::pair<int, int>, size_t> nextVariation; // round-robin cursor per sound id and group (0x466BC0)
        // The speech queue (0x5891E0): opcode-6 speech waiting for the speech playing to end, newest first
        struct Queued { int id; float volume; int group, x, y; };
        std::vector<Queued> speechQueue;
        bool fromQueue = false;
        static float Curve(float volume);     // the 128-step decibel table (0x465B20)
        float master = 1.0f, typeVolume[6] = { 1, 1, 1, 1, 1, 1 };
        int listenerX = 0, listenerY = 0;
        Border inner{ -320, -200, 320, 200 }, outer{ -640, -400, 640, 400 };
        Border innerAmbient{ -640, -400, 640, 400 }, outerAmbient{ -960, -600, 960, 600 };
        float Attenuation(int type, int dx, int dy) const;
        bool Beyond(int type, int dx, int dy) const;   // outside the outer border
        void Place(Voice& v) const;
        std::vector<float> mixBuffer;
    };
}
