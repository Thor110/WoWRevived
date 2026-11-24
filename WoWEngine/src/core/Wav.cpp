#include "core/Wav.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace wow
{
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
        const size_t width = bits / 8, frame = width * channels, frames = samples.size() / frame;
        const int outs = outChannels == 2 ? 2 : 1;
        auto value = [&](size_t f, uint32_t c) {
            const uint8_t* p = samples.data() + f * frame + c * width;
            if (floats) { float x; std::memcpy(&x, p, 4); return double(x); }
            if (bits == 8) return (double(p[0]) - 128.0) / 128.0;
            if (bits == 16) return double(int16_t(p[0] | p[1] << 8)) / 32768.0;
            if (bits == 24) return double(int32_t(uint32_t(p[0]) << 8 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 24) >> 8) / 8388608.0;
            return double(int32_t(uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24)) / 2147483648.0;
        };
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
