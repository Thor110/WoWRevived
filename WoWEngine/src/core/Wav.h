// MODERN (update 247, docs/MODERN.md "Loose sound files"): a WAV file read as the game's samples are kept: 16-bit
// PCM at a given rate, mono, or interleaved stereo for a sound the table marks stereo (opcode 15; 0x465D10 makes its
// buffer two channels). The game's own samples (the SFX and VOX libraries' entries, and a loose .snd) are that
// already; a WAV may be 8, 16, 24 or 32-bit integer PCM or 32-bit float, any number of channels (mixed down to
// mono, or its first two kept for stereo; a mono one doubled), at any rate (made the sample's rate by straight-line
// interpolation).
#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace wow
{
    // Nothing (and why in error) when the data isn't a WAV this reads.
    std::optional<std::vector<int16_t>> DecodeWav(std::span<const uint8_t> data, int rate, std::string& error, int channels = 1);
    // (update 248) an Ogg Vorbis file, the same way: decoded by stb_vorbis (third_party/stb_vorbis.c, public domain)
    std::optional<std::vector<int16_t>> DecodeOgg(std::span<const uint8_t> data, int rate, std::string& error, int channels = 1);
}
