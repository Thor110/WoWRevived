#include "platform/Narration.h"

#include <algorithm>

namespace wow
{
    bool Narration::Play(const std::vector<int16_t>& samples, int rate, int channels, float volume)
    {
        Stop();
        if (samples.empty() || rate <= 0 || channels <= 0) return false;
        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_S16;
        spec.channels = channels;
        spec.freq = rate;
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!stream) { SDL_Log("Narration: no audio stream (%s)", SDL_GetError()); return false; }
        SDL_SetAudioStreamGain(stream, std::clamp(volume, 0.0f, 1.0f));
        // the whole of it queued at once: the stream holds it and plays it out
        SDL_PutAudioStreamData(stream, samples.data(), int(samples.size() * sizeof(int16_t)));
        SDL_FlushAudioStream(stream);
        SDL_ResumeAudioStreamDevice(stream);
        return true;
    }

    void Narration::Stop()
    {
        if (stream) SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }
}
