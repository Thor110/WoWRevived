#include "core/Wav.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <functional>

// third_party/stb_vorbis.c (compiled on its own): its one function this needs
extern "C" int stb_vorbis_decode_memory(const unsigned char* mem, int len, int* channels, int* sample_rate, short** output);
// third_party/dr_mp3.c (compiled on its own): its one function this needs
#define DR_MP3_NO_STDIO
#include "../../third_party/dr_mp3.h"

namespace wow
{
    namespace
    {
        // the samples (value(frame, channel), -1..1) as the game keeps them: 16-bit at rate, mono (the channels
        // mixed) or interleaved stereo (the first two; a mono source's one twice), by straight-line interpolation
        std::vector<int16_t> Convert(const std::function<double(size_t, uint32_t)>& value, size_t frames, uint32_t channels,
                                     uint32_t fileRate, int rate, int outChannels)
        {
            const int outs = outChannels == 2 ? 2 : 1;
            // each output channel's frames, as -1..1: mono the channels mixed; stereo the first two (a mono file's one twice)
            std::vector<std::vector<float>> chans(size_t(outs), std::vector<float>(frames, 0.0f));
            for (size_t f = 0; f < frames; f++)
            {
                if (outs == 1)
                {
                    double sum = 0;
                    for (uint32_t c = 0; c < channels; c++) sum += value(f, c);
                    chans[0][f] = float(sum / channels);
                }
                else
                {
                    chans[0][f] = float(value(f, 0));
                    chans[1][f] = float(value(f, channels > 1 ? 1 : 0));
                }
            }
            // to the sample's rate
            const double step = double(fileRate) / double(rate > 0 ? rate : int(fileRate));
            const size_t outFrames = frames == 0 ? 0 : size_t(std::floor(double(frames - 1) / step)) + 1;
            std::vector<int16_t> out(outFrames * size_t(outs));
            for (size_t i = 0; i < outFrames; i++)
            {
                const double at = double(i) * step;
                const size_t a = size_t(at);
                const size_t b = std::min(a + 1, frames - 1);
                const double t = at - double(a);
                for (int c = 0; c < outs; c++)
                {
                    const double v = double(chans[size_t(c)][a]) * (1.0 - t) + double(chans[size_t(c)][b]) * t;
                    out[i * size_t(outs) + size_t(c)] = int16_t(std::clamp(std::lround(v * 32768.0), -32768L, 32767L));
                }
            }
            return out;
        }
    }

    std::optional<std::vector<int16_t>> DecodeWav(std::span<const uint8_t> d, int rate, std::string& error, int outChannels)
    {
        auto u16 = [&](size_t at) { return uint32_t(d[at] | d[at + 1] << 8); };
        auto u32 = [&](size_t at) { return uint32_t(d[at]) | uint32_t(d[at + 1]) << 8 | uint32_t(d[at + 2]) << 16 | uint32_t(d[at + 3]) << 24; };
        if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) != 0 || std::memcmp(d.data() + 8, "WAVE", 4) != 0) { error = "not a WAV (no RIFF / WAVE)"; return std::nullopt; }
        uint32_t format = 0, channels = 0, fileRate = 0, bits = 0;
        bool haveFormat = false;
        std::span<const uint8_t> samples;
        for (size_t at = 12; at + 8 <= d.size();)
        {
            const uint32_t size = u32(at + 4);
            const size_t body = at + 8;
            const size_t length = std::min<size_t>(size, d.size() - body);
            if (std::memcmp(d.data() + at, "fmt ", 4) == 0 && length >= 16)
            {
                format = u16(body);
                channels = u16(body + 2);
                fileRate = u32(body + 4);
                bits = u16(body + 14);
                // WAVE_FORMAT_EXTENSIBLE: the real format is the first two bytes of its sub-format
                if (format == 0xFFFE && length >= 26) format = u16(body + 24);
                haveFormat = true;
            }
            else if (std::memcmp(d.data() + at, "data", 4) == 0) samples = d.subspan(body, length);
            at = body + size + (size & 1);
        }
        if (!haveFormat) { error = "no fmt chunk"; return std::nullopt; }
        if (samples.empty()) { error = "no sound data"; return std::nullopt; }
        const bool pcm = format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32);
        const bool floats = format == 3 && bits == 32;
        if (!pcm && !floats) { error = "format " + std::to_string(format) + " at " + std::to_string(bits) + " bits isn't read (PCM 8, 16, 24 or 32-bit, or 32-bit float)"; return std::nullopt; }
        if (channels == 0 || fileRate == 0) { error = "no channels or rate"; return std::nullopt; }
        // a stereo sound exported with a mono header at twice its rate (WoWViewer before update 248 wrote each entry
        // that way: the data is the entry's own, interleaved stereo at the sample's rate), read as what it is
        if (outChannels == 2 && channels == 1 && rate > 0 && fileRate == uint32_t(rate) * 2) { channels = 2; fileRate = uint32_t(rate); }
        const size_t width = bits / 8, frame = width * channels, frames = samples.size() / frame;
        auto value = [&](size_t f, uint32_t c) {
            const uint8_t* p = samples.data() + f * frame + c * width;
            if (floats) { float x; std::memcpy(&x, p, 4); return double(x); }
            if (bits == 8) return (double(p[0]) - 128.0) / 128.0;
            if (bits == 16) return double(int16_t(p[0] | p[1] << 8)) / 32768.0;
            if (bits == 24) return double(int32_t(uint32_t(p[0]) << 8 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 24) >> 8) / 8388608.0;
            return double(int32_t(uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24)) / 2147483648.0;
        };
        return Convert(value, frames, channels, fileRate, rate, outChannels);
    }

    std::optional<std::vector<int16_t>> DecodeOgg(std::span<const uint8_t> d, int rate, std::string& error, int outChannels)
    {
        if (d.size() < 4 || std::memcmp(d.data(), "OggS", 4) != 0) { error = "not an Ogg file (no OggS)"; return std::nullopt; }
        if (d.size() > size_t(0x7FFFFFFF)) { error = "too large"; return std::nullopt; }
        int channels = 0, fileRate = 0;
        short* decoded = nullptr;
        const int frames = stb_vorbis_decode_memory(d.data(), int(d.size()), &channels, &fileRate, &decoded);
        if (frames < 0 || !decoded) { error = "not an Ogg Vorbis file this reads"; std::free(decoded); return std::nullopt; }
        if (channels <= 0 || fileRate <= 0) { std::free(decoded); error = "no channels or rate"; return std::nullopt; }
        const auto value = [&](size_t f, uint32_t c) { return double(decoded[f * size_t(channels) + c]) / 32768.0; };
        std::vector<int16_t> out = Convert(value, size_t(frames), uint32_t(channels), uint32_t(fileRate), rate, outChannels);
        std::free(decoded);
        return out;
    }

    std::optional<std::vector<int16_t>> DecodeMp3(std::span<const uint8_t> d, int rate, std::string& error, int outChannels)
    {
        drmp3_config config{};
        drmp3_uint64 frames = 0;
        drmp3_int16* decoded = drmp3_open_memory_and_read_pcm_frames_s16(d.data(), d.size(), &config, &frames, nullptr);
        if (!decoded || frames == 0) { error = "not an MP3 file this reads"; drmp3_free(decoded, nullptr); return std::nullopt; }
        if (config.channels == 0 || config.sampleRate == 0) { drmp3_free(decoded, nullptr); error = "no channels or rate"; return std::nullopt; }
        const uint32_t channels = config.channels;
        const auto value = [&](size_t f, uint32_t c) { return double(decoded[f * size_t(channels) + c]) / 32768.0; };
        std::vector<int16_t> out = Convert(value, size_t(frames), channels, config.sampleRate, rate, outChannels);
        drmp3_free(decoded, nullptr);
        return out;
    }

    std::optional<std::vector<int16_t>> DecodeAudio(std::span<const uint8_t> d, int rate, std::string& error, int channels)
    {
        if (d.size() >= 4 && std::memcmp(d.data(), "RIFF", 4) == 0) return DecodeWav(d, rate, error, channels);
        if (d.size() >= 4 && std::memcmp(d.data(), "OggS", 4) == 0) return DecodeOgg(d, rate, error, channels);
        if ((d.size() >= 3 && std::memcmp(d.data(), "ID3", 3) == 0) || (d.size() >= 2 && d[0] == 0xFF && (d[1] & 0xE0) == 0xE0))
            return DecodeMp3(d, rate, error, channels);
        error = "not a WAV, Ogg or MP3 file";
        return std::nullopt;
    }
}
