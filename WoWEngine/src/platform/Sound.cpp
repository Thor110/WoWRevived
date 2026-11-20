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

    int Sound::Play(int id, float volume)
    {
        if (!stream) return 0;
        const SoundDef* def = table.Find(id);
        if (!def) return 0;
        const auto& group = def->groups[0];
        if (group.empty()) return 0;
        // the sound's samples are taken in turn, not at random (0x466BC0)
        size_t& cursor = nextVariation[id];
        const SoundSample& sample = group[cursor++ % group.size()];
        auto pcm = Sample(sample);
        if (!pcm || pcm->empty()) return 0;

        Voice v;
        v.pcm = pcm;
        v.step = double(sample.rate) / OutputRate;
        v.channels = def->stereo ? 2 : 1; // each sample's buffer is mono or stereo by the flag (0x465D10)
        v.loop = def->loop;
        v.type = std::clamp(def->type, 0, 5);
        v.gain = def->volume * volume;
        std::lock_guard<std::mutex> l(lock);
        // Voices (0x463640): 8 at once; Ambient 2, Engine 4, Explosions 4, Weapons 4, Special 2, Speech any.
        // At a type's limit, Ambient and Engine take the quietest of their own voices, Explosions and Weapons the
        // oldest, and Special fails. With all 8 in use a sound takes the lowest-type voice below it, else fails.
        // New speech drops the speech already playing to 0.7.
        static constexpr int Limit[6] = { 2, 4, 4, 4, 2, 1000 };
        constexpr size_t Voices = 8;
        auto loudness = [&](const Voice& o) { return o.gain * o.fade; };
        auto erase = [&](size_t i) { voices.erase(voices.begin() + std::ptrdiff_t(i)); };
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
        else if (voices.size() >= Voices)
        {
            size_t pick = voices.size();
            for (size_t i = 0; i < voices.size(); i++)
                if (voices[i].type < v.type && (pick == voices.size() || voices[i].type < voices[pick].type)) pick = i;
            if (pick == voices.size()) return 0;
            erase(pick);
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
        for (Voice& v : voices) if (v.handle == handle && !v.fadeOutTicks) { v.fadeOutTicks = ticks; v.fadeOutDone = 0; }
    }

    void Sound::FadeOutAll(int ticks)
    {
        std::lock_guard<std::mutex> l(lock);
        if (ticks <= 0) { voices.clear(); return; }
        for (Voice& v : voices) { v.fadeOutTicks = ticks; v.fadeOutDone = 0; }
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
        std::lock_guard<std::mutex> l(lock);
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
            // the duck (0x4CA998): 0.5 for everything but speech while speech plays, 0.25 under the war map menu
            float duck = v.type == SoundDef::Speech ? 1.0f : menuDuck ? 0.25f : speechPlaying ? 0.5f : 1.0f;
            float gain = Curve(v.gain * v.fade * v.speechFade * typeVolume[v.type] * master * duck) / 32768.0f;
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
                out[i * 2] += left;
                out[i * 2 + 1] += right;
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
