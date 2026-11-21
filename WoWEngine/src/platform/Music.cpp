#include "platform/Music.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace wow
{
    namespace
    {
        uint32_t U32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
        uint16_t U16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
    }

    Music::~Music() { Stop(); }

    bool Music::Play(const std::filesystem::path& wav)
    {
        Stop();
#ifdef _WIN32
        std::FILE* f = _wfopen(wav.c_str(), L"rb");
#else
        std::FILE* f = std::fopen(wav.c_str(), "rb");
#endif
        if (!f)
        {
            SDL_Log("Can't open %s", wav.string().c_str());
            return false;
        }
        // RIFF / WAVE: the "fmt " chunk (PCM, 8 or 16 bits) and the "data" chunk
        uint8_t header[12];
        SDL_AudioSpec spec{};
        uint64_t start = 0;
        uint32_t size = 0;
        bool haveFormat = false, haveData = false;
        if (std::fread(header, 1, 12, f) == 12 && std::memcmp(header, "RIFF", 4) == 0 && std::memcmp(header + 8, "WAVE", 4) == 0)
        {
            uint8_t chunk[8];
            while (!haveData && std::fread(chunk, 1, 8, f) == 8)
            {
                const uint32_t length = U32(chunk + 4);
                if (std::memcmp(chunk, "fmt ", 4) == 0)
                {
                    std::vector<uint8_t> fmt(length);
                    if (length < 16 || std::fread(fmt.data(), 1, length, f) != length) break;
                    const uint16_t format = U16(&fmt[0]), bits = U16(&fmt[14]);
                    if (format != 1 || (bits != 8 && bits != 16)) break;
                    spec.channels = U16(&fmt[2]);
                    spec.freq = int(U32(&fmt[4]));
                    spec.format = bits == 8 ? SDL_AUDIO_U8 : SDL_AUDIO_S16LE;
                    haveFormat = true;
                    if (length & 1) std::fseek(f, 1, SEEK_CUR);
                }
                else if (std::memcmp(chunk, "data", 4) == 0)
                {
                    start = uint64_t(std::ftell(f));
                    size = length;
                    haveData = true;
                }
                else std::fseek(f, long(length + (length & 1)), SEEK_CUR);
            }
        }
        if (!haveFormat || !haveData || spec.channels == 0 || spec.freq <= 0)
        {
            SDL_Log("Can't play %s: not a PCM WAV file", wav.string().c_str());
            std::fclose(f);
            return false;
        }
        // the stream starts paused, so nothing's fed until it's all set
        file = f;
        dataStart = start;
        frameSize = std::max(1u, uint32_t(SDL_AUDIO_FRAMESIZE(spec)));
        bytesPerSecond = std::max(1u, frameSize * uint32_t(spec.freq));
        lengthBytes = size - size % frameSize;
        lengthMs = uint32_t(uint64_t(lengthBytes) * 1000 / bytesPerSecond);
        fed = 0;
        paused = false;
        std::fseek(f, long(start), SEEK_SET);
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Feed, this);
        if (!stream)
        {
            SDL_Log("Can't open the audio device: %s", SDL_GetError());
            std::fclose(f);
            file = nullptr;
            lengthBytes = lengthMs = 0;
            return false;
        }
        SDL_SetAudioStreamGain(stream, volume);
        SDL_ResumeAudioStreamDevice(stream);
        return true;
    }

    void SDLCALL Music::Feed(void* self, SDL_AudioStream* stream, int additional, int)
    {
        auto* m = static_cast<Music*>(self);
        if (!m->file || additional <= 0) return;
        uint8_t buffer[16384];
        while (additional > 0 && m->fed < m->lengthBytes)
        {
            uint32_t want = std::min<uint32_t>({ uint32_t(sizeof(buffer)), uint32_t(additional) + m->frameSize, m->lengthBytes - m->fed });
            if (want > m->frameSize) want -= want % m->frameSize;
            const size_t got = std::fread(buffer, 1, want, m->file);
            if (got == 0) { m->fed = m->lengthBytes; break; }    // the file's shorter than it says
            SDL_PutAudioStreamData(stream, buffer, int(got));
            m->fed += uint32_t(got);
            additional -= int(got);
        }
        // the end: what the resampler holds back goes out too
        if (m->fed >= m->lengthBytes) SDL_FlushAudioStream(stream);
    }

    void Music::Stop()
    {
        if (stream) SDL_DestroyAudioStream(stream);     // after this the device won't call Feed again
        stream = nullptr;
        if (file) std::fclose(file);
        file = nullptr;
        lengthBytes = lengthMs = fed = 0;
        paused = false;
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

    bool Music::Finished() const
    {
        if (!stream) return false;
        SDL_LockAudioStream(stream);
        const bool done = fed >= lengthBytes && SDL_GetAudioStreamQueued(stream) == 0;
        SDL_UnlockAudioStream(stream);
        return done;
    }

    uint32_t Music::PositionMs() const
    {
        if (!stream) return 0;
        SDL_LockAudioStream(stream);
        const int queued = SDL_GetAudioStreamQueued(stream);
        const uint32_t played = fed - std::min<uint32_t>(fed, queued < 0 ? 0u : uint32_t(queued));
        SDL_UnlockAudioStream(stream);
        return uint32_t(uint64_t(played) * 1000 / bytesPerSecond);
    }

    void Music::Seek(int deltaMs)
    {
        if (!stream || !file) return;
        const int64_t to = std::clamp<int64_t>(int64_t(PositionMs()) + deltaMs, 0, int64_t(lengthMs));
        uint64_t offset = uint64_t(to) * bytesPerSecond / 1000;
        offset -= offset % frameSize;
        offset = std::min<uint64_t>(offset, lengthBytes);
        SDL_LockAudioStream(stream);
        SDL_ClearAudioStream(stream);
        std::fseek(file, long(dataStart + offset), SEEK_SET);
        fed = uint32_t(offset);
        SDL_UnlockAudioStream(stream);
    }
}
