// Smacker video (SMK2 / SMK4), the format of the game's movies (FMV\*.smk, played by SMACKW32.DLL through
// 0x44F2C0). Every movie the game ships is SMK2, 640 x 180, Y-interlaced, 25 frames a second, with one track of
// compressed 16-bit stereo sound at 22050 Hz.
//
// Layout: a 104 byte header ("SMK2", width, height, frames, frame rate, flags, 7 audio buffer sizes, the size of
// the trees, 4 tree sizes, 7 audio rates, a spare word), each frame's size (the low 2 bits are flags) and type
// (bit 0: a palette change, bits 1-7: audio tracks 0-6), the four Huffman trees (MMap, MClr, Full, Type), then the
// frames: the palette change, each audio track (a 32-bit size including itself, then the data) and the video.
// Bit streams are read lowest bit first.
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace wow
{
    class Smacker
    {
    public:
        // Opens the movie; its frames are read from the file as they're decoded. Throws FormatError when the file
        // can't be read or isn't a Smacker movie, or its header or trees are damaged.
        explicit Smacker(const std::filesystem::path& path);

        int Width() const { return width; }
        int Height() const { return height; }         // as stored
        int DisplayHeight() const { return (flags & (InterlacedY | DoubledY)) ? height * 2 : height; }
        int Frames() const { return frameCount; }
        double FrameSeconds() const;                  // how long each frame shows
        bool Interlaced() const { return flags & InterlacedY; } // shown on every other line
        bool Doubled() const { return flags & DoubledY; }       // each line shown twice

        // Sound track 0, if it has one: its rate and channels; DecodeFrame adds its 16-bit samples to audio.
        int AudioRate() const { return int(audioRate[0] & 0xFFFFFF); }
        int AudioChannels() const { return (audioRate[0] & AudioStereo) ? 2 : 1; }
        bool HasAudio() const { return (audioRate[0] & AudioPresent) != 0; }

        // Decodes the next frame into Pixels() / Palette() and appends its sound (interleaved if stereo).
        // Returns false after the last frame.
        bool DecodeFrame(std::vector<int16_t>& audio);
        int FrameIndex() const { return next - 1; }

        const std::vector<uint8_t>& Pixels() const { return pixels; } // Width() x Height(), palette indices
        const std::array<uint32_t, 256>& Palette() const { return palette; } // 0xRRGGBB

    private:
        enum : uint32_t { RingFrame = 1, InterlacedY = 2, DoubledY = 4 };
        enum : uint32_t { AudioCompressed = 0x80000000, AudioPresent = 0x40000000, Audio16Bit = 0x20000000, AudioStereo = 0x10000000 };

        // One of the video's "big" Huffman trees: 16-bit values, with three of them kept as a small cache
        // (the last values decoded) that each frame starts with at 0.
        struct BigTree
        {
            std::vector<int32_t> table; // nodes: Node | the size of the left branch, else a value
            int last[3] = { 0, 0, 0 };
            static constexpr int32_t Node = 0x80000000;
        };
        struct BitReader;
        static void ReadBigTree(BitReader& bits, BigTree& tree);
        static int Get(BitReader& bits, BigTree& tree);

        void DecodePalette(std::span<const uint8_t> data);
        void DecodeVideo(std::span<const uint8_t> data);
        void DecodeAudio(std::span<const uint8_t> data, std::vector<int16_t>& audio) const;

        std::ifstream file;
        uint64_t fileSize = 0;
        std::vector<uint8_t> frameData;   // the frame being decoded
        uint32_t version = 2;
        uint32_t audioSize = 0;           // the largest decoded audio chunk for track 0, in bytes (header)
        int width = 0, height = 0, frameCount = 0;
        int32_t rate = 0;
        uint32_t flags = 0;
        uint32_t audioRate[7] = {};
        std::vector<uint32_t> frameSizes;
        std::vector<uint8_t> frameTypes;
        uint64_t frameStart = 0, offset = 0;
        int next = 0;
        BigTree mmap, mclr, full, type;
        std::vector<uint8_t> pixels;
        std::array<uint32_t, 256> palette{};
    };
}
