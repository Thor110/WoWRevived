#include "platform/Movie.h"

#include "core/Bytes.h"
#include "core/Smacker.h"

#include <SDL3/SDL.h>

#include <fstream>
#include <iterator>

namespace wow
{
    namespace
    {
        // A Smacker movie as the game shows it: a Y-interlaced one on every other line with black between, a
        // Y-doubled one with each line twice.
        class SmackerMovie final : public Movie
        {
        public:
            explicit SmackerMovie(const std::filesystem::path& path) : smk(path)
            {
                frame.assign(size_t(smk.Width()) * size_t(smk.DisplayHeight()), 0);
            }
            int Width() const override { return smk.Width(); }
            int Height() const override { return smk.DisplayHeight(); }
            int AudioRate() const override { return smk.HasAudio() ? smk.AudioRate() : 0; }
            int AudioChannels() const override { return smk.AudioChannels(); }
            double Time() const override { return smk.FrameIndex() * smk.FrameSeconds(); }
            const std::vector<uint32_t>& Frame() const override { return frame; }

            bool Next(std::vector<int16_t>& audio) override
            {
                if (!smk.DecodeFrame(audio)) return false;
                const int w = smk.Width(), h = smk.DisplayHeight();
                const auto& pixels = smk.Pixels();
                const auto& palette = smk.Palette();
                for (int y = 0; y < h; y++)
                {
                    uint32_t* out = frame.data() + size_t(y) * size_t(w);
                    if (smk.Interlaced() && (y & 1)) { std::fill(out, out + w, 0u); continue; }
                    int source = (smk.Interlaced() || smk.Doubled()) ? y / 2 : y;
                    const uint8_t* in = pixels.data() + size_t(source) * size_t(w);
                    for (int x = 0; x < w; x++) out[x] = palette[in[x]];
                }
                return true;
            }

        private:
            Smacker smk;
            std::vector<uint32_t> frame;
        };
    }

    std::unique_ptr<Movie> Movie::Open(const GameFiles& files, const std::string& name, bool human, std::string& error)
    {
        // the folders WoWRevived's wrapper uses (Smackw32.cpp SmackOpen): TITLE and RAGELOGO stay in FMV
        bool shared = name == "TITLE" || name == "RAGELOGO";
        std::string folder = shared ? "FMV" : human ? "FMV-Human" : "FMV-Martian";
#ifdef _WIN32
        if (auto mp4 = files.Resolve(folder + "\\" + name + ".mp4"))
        {
            std::string why;
            if (auto movie = OpenMp4(*mp4, why)) return movie;
            SDL_Log("%s: %s; trying the .smk", mp4->string().c_str(), why.c_str());
        }
#endif
        auto smk = files.Resolve(folder + "\\" + name + ".smk");
        if (!smk && !shared) smk = files.Resolve("FMV\\" + name + ".smk");
        if (!smk)
        {
            error = "no " + folder + "\\" + name + " movie (.mp4 or .smk)";
            return nullptr;
        }
        try { return std::make_unique<SmackerMovie>(*smk); }
        catch (const std::exception& e)
        {
            error = smk->string() + ": " + e.what();
            return nullptr;
        }
    }

    MovieAudio::MovieAudio(int r, int c) : rate(r), channels(c)
    {
        if (rate <= 0 || channels <= 0) return;
        SDL_AudioSpec spec{ SDL_AUDIO_S16LE, channels, rate };
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!stream) { SDL_Log("The movie's sound can't be played: %s", SDL_GetError()); return; }
        SDL_ResumeAudioStreamDevice(stream);
    }

    MovieAudio::~MovieAudio()
    {
        if (stream) SDL_DestroyAudioStream(stream);
    }

    void MovieAudio::SetVolume(float gain)
    {
        if (stream) SDL_SetAudioStreamGain(stream, gain);
    }

    void MovieAudio::SetPaused(bool paused)
    {
        if (!stream) return;
        if (paused) SDL_PauseAudioStreamDevice(stream);
        else SDL_ResumeAudioStreamDevice(stream);
    }

    void MovieAudio::Push(const std::vector<int16_t>& samples)
    {
        if (!stream || samples.empty()) return;
        SDL_PutAudioStreamData(stream, samples.data(), int(samples.size() * sizeof(int16_t)));
        pushed += samples.size() / size_t(channels);
    }

    double MovieAudio::Queued() const
    {
        if (!stream) return 0;
        int bytes = SDL_GetAudioStreamQueued(stream);
        return bytes > 0 ? double(bytes) / double(sizeof(int16_t) * size_t(channels)) / rate : 0.0;
    }

    double MovieAudio::Played() const
    {
        if (!stream) return 0;
        return double(pushed) / rate - Queued();
    }
}
