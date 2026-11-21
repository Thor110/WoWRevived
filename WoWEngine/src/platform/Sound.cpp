#include "platform/Sound.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace wow
{
    namespace
    {
        constexpr int OutputRate = 22050; // the rate nearly every sample uses

        SoundTable LoadTable(const GameFiles& files, bool human)
        {
            auto path = files.Resolve("SFX.ojd");
            if (!path) return {};
            std::ifstream in(*path, std::ios::binary);
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), {});
            try { return SoundTable::Load(data, human); }
            catch (const std::exception& e) { SDL_Log("SFX.ojd: %s", e.what()); return {}; }
        }
    }

    Sound::Sound(const GameFiles& files, bool human) : table(LoadTable(files, human)), library(files)
    {
        // as the game's primary buffer (0x465C40): 22050 Hz, 16-bit stereo
        SDL_AudioSpec spec{ SDL_AUDIO_F32, 2, OutputRate };
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Feed, this);
        if (!stream) { SDL_Log("Sound effects are off, the audio device can't be opened: %s", SDL_GetError()); return; }
        SDL_ResumeAudioStreamDevice(stream);
    }

    Sound::~Sound()
    {
        if (stream) SDL_DestroyAudioStream(stream); // stops the callback before the voices go
    }

    std::shared_ptr<const std::vector<int16_t>> Sound::Sample(const SoundSample& sample)
    {
        auto it = cache.find(sample.name);
        if (it != cache.end()) return it->second;
        std::shared_ptr<const std::vector<int16_t>> pcm;
        if (auto loaded = library.Load(sample)) pcm = std::make_shared<const std::vector<int16_t>>(std::move(*loaded));
        else SDL_Log("Couldn't find Sfx %s in Library", sample.name.c_str()); // the game's own message
        cache[sample.name] = pcm;
        return pcm;
    }

    // 0x464FF0: 1 for a sound not at a point; else, against the listener, 0 outside the outer border, 1 inside the
    // inner, and between them each way 1 - (the distance past the inner) / (outer - inner), 0 when the across one is
    // under 0.05; the two multiplied. Ambient sounds have the larger borders.
    namespace
    {
        float Attenuation(int type, int dx, int dy)
        {
            const bool ambient = type == SoundDef::Ambient;
            const int innerX = ambient ? 640 : 320, innerY = ambient ? 400 : 200;
            const int outerX = ambient ? 960 : 640, outerY = ambient ? 600 : 400;
            auto axis = [](int d, int inner, int outer) {
                d = std::abs(d);
                if (d >= outer) return 0.0f;
                if (d <= inner) return 1.0f;
                return 1.0f - float(d - inner) / float(outer - inner);
            };
            const float fx = axis(dx, innerX, outerX), fy = axis(dy, innerY, outerY);
            if (fx < 0.05f) return 0.0f;
            return fx * fy;
        }
    }

    void Sound::Place(Voice& v) const
    {
        if (!v.placed) return;
        v.attenuation = Attenuation(v.type, v.x - listenerX, v.y - listenerY);
        v.pan = v.type == SoundDef::Speech ? 0.0f : float(std::clamp((v.x - listenerX) * 4, -10000, 10000));
    }

    void Sound::SetListener(int x, int y)
    {
        std::lock_guard<std::mutex> l(lock);
        listenerX = x;
        listenerY = y;
        for (Voice& v : voices) Place(v);
    }

    int Sound::Play(int id, float volume, int groupIndex, int x, int y)
    {
        if (!stream) return 0;
        const SoundDef* def = table.Find(id);
        if (!def) return 0;
        if (groupIndex < 0 || groupIndex > 2) groupIndex = 0;
        // 0x4652B0: speech with opcode 6 (+0x6C) goes into the speech queue (0x466D00) unless that id's already
        // there; the queue's head plays once no speech is playing (0x462FF0, each tick)
        if (def->type == SoundDef::Speech && def->flag6 && !fromQueue)
        {
            std::lock_guard<std::mutex> l(lock);
            for (const Queued& q : speechQueue)
                if (q.id == id) return 0;
            speechQueue.insert(speechQueue.begin(), { id, volume, groupIndex, x, y });
            return 0;
        }
        // an empty group past the first falls back to the first (0x466BC0)
        if (def->groups[groupIndex].empty() && groupIndex > 0 && !def->groups[0].empty()) groupIndex = 0;
        const auto& group = def->groups[groupIndex];
        if (group.empty()) return 0;
        const bool placed = x != 0 || y != 0;
        // opcode 18: not even made when its point is beyond the outer border
        if (def->positional && placed)
        {
            const bool ambient = def->type == SoundDef::Ambient;
            if (std::abs(x - listenerX) >= (ambient ? 960 : 640) || std::abs(y - listenerY) >= (ambient ? 600 : 400)) return 0;
        }
        // the sound's samples are taken in turn, not at random (0x466BC0): the group's cursor (+0x4C, 0 at load)
        // moves on before it's used, so a three-sample sound plays 1, 2, 0, 1 ...
        size_t& cursor = nextVariation[{ id, groupIndex }];
        cursor = cursor + 1 < group.size() ? cursor + 1 : 0;
        const SoundSample& sample = group[cursor];
        auto pcm = Sample(sample);
        if (!pcm || pcm->empty()) return 0;

        Voice v;
        v.pcm = pcm;
        v.step = double(sample.rate) / OutputRate;
        v.channels = def->stereo ? 2 : 1; // each sample's buffer is mono or stereo by the flag (0x465D10)
        v.loop = def->loop;
        v.type = std::clamp(def->type, 0, 5);
        v.gain = def->volume * volume;
        v.placed = placed;
        v.x = x;
        v.y = y;
        std::lock_guard<std::mutex> l(lock);
        Place(v);
        // Voices (0x463640): 8 at once; Ambient 2, Engine 4, Explosions 4, Weapons 4, Special 2, Speech any.
        // At a type's limit, Ambient and Engine take the quietest of their own voices, Explosions and Weapons the
        // oldest, and Special fails. With all 8 in use a sound takes the lowest-type voice below it, else fails.
        // New speech drops the speech already playing to 0.7.
        static constexpr int Limit[6] = { 2, 4, 4, 4, 2, 1000 };
        constexpr size_t Voices = 8;
        auto loudness = [&](const Voice& o) { return o.gain * o.fade; };
        auto erase = [&](size_t i) { voices.erase(voices.begin() + std::ptrdiff_t(i)); };
        // ambient and engine sounds take no voice now: the update hands them out (0x463C30)
        if (v.type == SoundDef::Ambient || v.type == SoundDef::Engine)
        {
            v.voiced = false;
            v.hand = 0;
            v.handle = nextHandle++;
            v.age = started++;
            voices.push_back(v);
            return v.handle;
        }
        int sameType = 0;
        for (const Voice& o : voices) if (o.type == v.type) sameType++;
        if (v.type == SoundDef::Speech)
            for (Voice& o : voices) if (o.type == SoundDef::Speech) o.speechFade = 0.7f;
        if (sameType >= Limit[v.type])
        {
            if (v.type == SoundDef::Special) return 0;
            size_t pick = voices.size();
            for (size_t i = 0; i < voices.size(); i++)
            {
                if (voices[i].type != v.type) continue;
                if (pick == voices.size()) { pick = i; continue; }
                bool better = v.type <= SoundDef::Engine ? loudness(voices[i]) < loudness(voices[pick]) : voices[i].age < voices[pick].age;
                if (better) pick = i;
            }
            if (pick < voices.size()) erase(pick);
        }
        else if (size_t(std::count_if(voices.begin(), voices.end(), [](const Voice& o) { return o.voiced; })) >= Voices)
        {
            size_t pick = voices.size();
            for (size_t i = 0; i < voices.size(); i++)
                if (voices[i].voiced && voices[i].type < v.type && (pick == voices.size() || voices[i].type < voices[pick].type)) pick = i;
            if (pick == voices.size()) return 0;
            if (voices[pick].type <= SoundDef::Engine) { voices[pick].voiced = false; voices[pick].hand = 0; voices[pick].handOut = 0; }
            else erase(pick);
        }
        v.handle = nextHandle++;
        v.age = started++;
        voices.push_back(v);
        return v.handle;
    }

    void Sound::Stop(int handle)
    {
        std::lock_guard<std::mutex> l(lock);
        voices.erase(std::remove_if(voices.begin(), voices.end(), [&](const Voice& v) { return v.handle == handle; }), voices.end());
    }

    void Sound::Move(int handle, int x, int y)
    {
        std::lock_guard<std::mutex> l(lock);
        for (Voice& v : voices)
            if (v.handle == handle && v.placed) { v.x = x; v.y = y; }
    }

    void Sound::FadeIn(int handle, int ticks)
    {
        std::lock_guard<std::mutex> l(lock);
        for (Voice& v : voices)
            if (v.handle == handle) { v.fadeTicks = std::max(ticks, 1); v.fadeDone = 0; v.fade = 0; }
    }

    bool Sound::Active(int handle) const
    {
        std::lock_guard<std::mutex> l(lock);
        for (const Voice& v : voices) if (v.handle == handle) return true;
        return false;
    }

    void Sound::FadeOut(int handle, int ticks)
    {
        std::lock_guard<std::mutex> l(lock);
        if (ticks <= 0) { voices.erase(std::remove_if(voices.begin(), voices.end(), [&](const Voice& v) { return v.handle == handle; }), voices.end()); return; }
        // 0x464EB0: the end is the earlier of the one set and now + ticks, the start the earlier too
        for (Voice& v : voices)
            if (v.handle == handle)
            {
                if (!v.fadeOutTicks) { v.fadeOutTicks = ticks; v.fadeOutDone = 0; }
                else v.fadeOutTicks = std::min(v.fadeOutTicks, v.fadeOutDone + ticks);
            }
    }

    void Sound::FadeOutAll(int ticks)
    {
        std::lock_guard<std::mutex> l(lock);
        if (ticks <= 0) { voices.clear(); return; }
        for (Voice& v : voices)
        {
            if (!v.fadeOutTicks) { v.fadeOutTicks = ticks; v.fadeOutDone = 0; }
            else v.fadeOutTicks = std::min(v.fadeOutTicks, v.fadeOutDone + ticks);   // the earlier end kept
        }
    }

    void Sound::SetPaused(bool paused)
    {
        if (!stream) return;
        if (paused) SDL_PauseAudioStreamDevice(stream);
        else SDL_ResumeAudioStreamDevice(stream);
    }

    void Sound::SetMenuDuck(bool on)
    {
        std::lock_guard<std::mutex> l(lock);
        menuDuck = on;
    }

    float Sound::Curve(float volume)
    {
        // DirectSound volume (hundredths of a dB) = table[trunc(127 v)], table[i] = trunc(4745.61156 log10((i + 1) / 128)):
        // -10000 (silence) at 0 up to 0 at 127
        static const std::array<float, 128> gains = [] {
            std::array<float, 128> g{};
            for (int i = 0; i < 128; i++)
            {
                int hundredths = int(4745.61156 * std::log10(double(i + 1) / 128.0));
                g[size_t(i)] = i == 0 ? 0.0f : float(std::pow(10.0, hundredths / 2000.0));
            }
            return g;
        }();
        int i = std::clamp(int(127.0f * volume), 0, 127);
        return gains[size_t(i)];
    }

    void Sound::SetVolumes(float m, const float types[6])
    {
        std::lock_guard<std::mutex> l(lock);
        master = std::clamp(m, 0.0f, 1.0f);
        for (int i = 0; i < 6; i++) typeVolume[i] = std::clamp(types[i], 0.0f, 1.0f);
    }

    void Sound::Update()
    {
        // the speech queue (0x462FF0): with no speech voice playing, its head is played.
        // *Provisional*: 0x462FF0 is collapsed in the listing; it reads the head (the newest, 0x466D00 pushes at the
        // front), taken here as the one played.
        {
            Queued next{};
            bool play = false;
            {
                std::lock_guard<std::mutex> l(lock);
                bool speaking = false;
                for (const Voice& v : voices) if (v.type == SoundDef::Speech) speaking = true;
                if (!speaking && !speechQueue.empty())
                {
                    next = speechQueue.front();
                    speechQueue.erase(speechQueue.begin());
                    play = true;
                }
            }
            if (play)
            {
                fromQueue = true;
                Play(next.id, next.volume, next.group, next.x, next.y);
                fromQueue = false;
            }
        }
        std::lock_guard<std::mutex> l(lock);
        HandOutVoices();
        for (Voice& v : voices)
        {
            if (v.fadeTicks && v.fadeDone < v.fadeTicks)
            {
                v.fadeDone++;
                v.fade = float(v.fadeDone) / float(v.fadeTicks);
            }
            if (v.fadeOutTicks)
            {
                v.fadeOutDone++;
                v.fade = std::min(v.fade, 1.0f - float(v.fadeOutDone) / float(v.fadeOutTicks));
            }
        }
        voices.erase(std::remove_if(voices.begin(), voices.end(), [](const Voice& v) { return v.fadeOutTicks && v.fadeOutDone >= v.fadeOutTicks; }), voices.end());
    }

    // The update's hand-out (0x462FF0): of the ambient sounds the loudest audible one (its own volume against the
    // listener) has the voice; when that changes, the one playing fades out over 150 ticks and the new one fades in
    // over 15 (0x463C30; the voice's own fade-in is forced to 15, 0x4655F0). Of the engine sounds the 4 loudest
    // audible ones are voiced (the loudest silent one first). Audible: at least [Sound] "Minimum Audible Volume"
    // (0.05). Called with the lock held.
    void Sound::HandOutVoices()
    {
        auto loud = [](const Voice& v) { return v.gain * v.attenuation; };
        // ambient
        Voice* best = nullptr;
        for (Voice& v : voices)
            if (v.type == SoundDef::Ambient && !v.fadeOutTicks && loud(v) >= minimumAudible && (!best || loud(v) > loud(*best))) best = &v;
        for (Voice& v : voices)
        {
            if (v.type != SoundDef::Ambient) continue;
            if (&v == best)
            {
                if (!v.voiced || v.handOut) { v.voiced = true; v.handOut = 0; v.handIn = 1; v.hand = 0; }
                if (v.handIn) { v.hand = std::min(1.0f, float(v.handIn) / 15.0f); if (++v.handIn > 15) v.handIn = 0; }
            }
            else if (v.voiced)
            {
                if (!v.handOut) { v.handOut = 1; v.handIn = 0; }
                v.hand = std::max(0.0f, 1.0f - float(v.handOut) / 150.0f);
                if (++v.handOut > 150) { v.voiced = false; v.handOut = 0; v.hand = 0; }
            }
        }
        // engine: the 4 loudest audible
        std::vector<Voice*> engines;
        for (Voice& v : voices) if (v.type == SoundDef::Engine) engines.push_back(&v);
        std::stable_sort(engines.begin(), engines.end(), [&](const Voice* a, const Voice* b) { return loud(*a) > loud(*b); });
        for (size_t i = 0; i < engines.size(); i++)
        {
            const bool on = i < 4 && loud(*engines[i]) >= minimumAudible;
            engines[i]->voiced = on;
            engines[i]->hand = on ? 1.0f : 0.0f;
        }
    }

    void SDLCALL Sound::Feed(void* self, SDL_AudioStream* stream, int additional, int)
    {
        auto* sound = static_cast<Sound*>(self);
        int frames = additional / int(2 * sizeof(float));
        if (frames <= 0) return;
        sound->mixBuffer.assign(size_t(frames) * 2, 0.0f);
        sound->Mix(sound->mixBuffer.data(), frames);
        SDL_PutAudioStreamData(stream, sound->mixBuffer.data(), frames * int(2 * sizeof(float)));
    }

    void Sound::Mix(float* out, int frames)
    {
        std::lock_guard<std::mutex> l(lock);
        bool speechPlaying = false;
        for (const Voice& v : voices) if (v.type == SoundDef::Speech) speechPlaying = true;
        for (Voice& v : voices)
        {
            const auto& pcm = *v.pcm;
            const double length = double(pcm.size() / size_t(v.channels)); // frames
            if (!v.voiced)
            {
                // a virtual voice goes on in time without a sound (0x4657B0's resume point)
                v.position += v.step * frames;
                if (v.loop && length > 0) v.position = std::fmod(v.position, length);
                continue;
            }
            // the duck (0x4CA998): 0.5 for everything but speech while speech plays, 0.25 under the war map menu
            float duck = v.type == SoundDef::Speech ? 1.0f : menuDuck ? 0.25f : speechPlaying ? 0.5f : 1.0f;
            float gain = Curve(v.gain * v.fade * v.speechFade * v.hand * v.attenuation * typeVolume[v.type] * master * duck) / 32768.0f;
            // the pan (DSBPAN, hundredths of a dB taken off the far side)
            const float panLeft = v.pan > 0 ? std::pow(10.0f, -v.pan / 2000.0f) : 1.0f;
            const float panRight = v.pan < 0 ? std::pow(10.0f, v.pan / 2000.0f) : 1.0f;
            for (int i = 0; i < frames; i++)
            {
                if (v.position >= length)
                {
                    if (!v.loop) break;
                    v.position -= length;
                }
                size_t at = size_t(v.position) * size_t(v.channels);
                float left = float(pcm[at]) * gain;
                float right = v.channels == 2 ? float(pcm[at + 1]) * gain : left;
                out[i * 2] += left * panLeft;
                out[i * 2 + 1] += right * panRight;
                v.position += v.step;
            }
        }
        // finished one-shot sounds go
        voices.erase(std::remove_if(voices.begin(), voices.end(), [](const Voice& v) {
            return !v.loop && v.position >= double(v.pcm->size() / size_t(v.channels));
        }), voices.end());
        for (int i = 0; i < frames * 2; i++) out[i] = std::clamp(out[i], -1.0f, 1.0f);
    }
}
