#include "platform/Sound.h"

#include <algorithm>
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
        const SoundSample& sample = group[group.size() == 1 ? 0 : size_t(std::rand()) % group.size()];
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
        v.handle = nextHandle++;
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

    void Sound::FadeOutAll(int ticks)
    {
        std::lock_guard<std::mutex> l(lock);
        if (ticks <= 0) { voices.clear(); return; }
        for (Voice& v : voices) { v.fadeOutTicks = ticks; v.fadeOutDone = 0; }
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
        for (Voice& v : voices)
        {
            const auto& pcm = *v.pcm;
            const double length = double(pcm.size() / size_t(v.channels)); // frames
            float gain = v.gain * v.fade * typeVolume[v.type] * master / 32768.0f;
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
