#include "platform/Music.h"

#include <algorithm>

namespace wow
{
    Music::~Music() { Stop(); }

    bool Music::Play(const std::filesystem::path& wav)
    {
        Stop();
        SDL_AudioSpec spec{};
        Uint8* data = nullptr;
        Uint32 length = 0;
        if (!SDL_LoadWAV(wav.string().c_str(), &spec, &data, &length))
        {
            SDL_Log("Can't load %s: %s", wav.string().c_str(), SDL_GetError());
            return false;
        }
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!stream)
        {
            SDL_Log("Can't open the audio device: %s", SDL_GetError());
            SDL_free(data);
            return false;
        }
        SDL_PutAudioStreamData(stream, data, int(length));
        SDL_FlushAudioStream(stream);
        pcm.assign(data, data + length);
        SDL_free(data);
        frameSize = std::max(1u, uint32_t(SDL_AUDIO_FRAMESIZE(spec)));
        bytesPerSecond = uint32_t(SDL_AUDIO_FRAMESIZE(spec)) * uint32_t(spec.freq);
        if (bytesPerSecond == 0) bytesPerSecond = 1;
        lengthBytes = length;
        lengthMs = uint32_t(uint64_t(length) * 1000 / bytesPerSecond);
        paused = false;
        SDL_SetAudioStreamGain(stream, volume);
        SDL_ResumeAudioStreamDevice(stream);
        return true;
    }

    void Music::Stop()
    {
        if (stream) SDL_DestroyAudioStream(stream);
        stream = nullptr;
        lengthBytes = lengthMs = 0;
        paused = false;
        pcm.clear();
    }

    void Music::Seek(int deltaMs)
    {
        if (!stream || pcm.empty()) return;
        const int64_t now = int64_t(PositionMs()) + deltaMs;
        const int64_t to = std::clamp<int64_t>(now, 0, int64_t(lengthMs));
        uint64_t offset = uint64_t(to) * bytesPerSecond / 1000;
        offset -= offset % frameSize;
        offset = std::min<uint64_t>(offset, pcm.size());
        SDL_ClearAudioStream(stream);
        if (offset < pcm.size()) SDL_PutAudioStreamData(stream, pcm.data() + offset, int(pcm.size() - offset));
        SDL_FlushAudioStream(stream);
    }

    void Music::SetPaused(bool p)
    {
        if (!stream || p == paused) return;
        paused = p;
        if (paused) SDL_PauseAudioStreamDevice(stream);
        else SDL_ResumeAudioStreamDevice(stream);
    }

    void Music::SetVolume(float v)
    {
        volume = std::clamp(v, 0.0f, 1.0f);
        if (stream) SDL_SetAudioStreamGain(stream, volume);
    }

    bool Music::Finished() const { return stream && SDL_GetAudioStreamQueued(stream) == 0; }

    uint32_t Music::PositionMs() const
    {
        if (!stream) return 0;
        int queued = SDL_GetAudioStreamQueued(stream);
        uint32_t played = queued < 0 ? 0 : lengthBytes - std::min<uint32_t>(lengthBytes, uint32_t(queued));
        return uint32_t(uint64_t(played) * 1000 / bytesPerSecond);
    }
}
