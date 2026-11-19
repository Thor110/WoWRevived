// The game draws into an 8-bit paletted frame buffer (it ran in 640x480x8 or 16 bit DirectDraw modes).
// Video keeps that model: the engine draws palette indices into Pixels(), and Present() converts them through the
// current palette and scales the result to the window with nearest-neighbour filtering and letterboxing.
#pragma once
#include "core/Palette.h"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace wow
{
    class Video
    {
    public:
        Video() = default;
        ~Video();
        Video(const Video&) = delete;
        Video& operator=(const Video&) = delete;

        bool Open(const char* title, int width, int height, bool fullScreen, std::string& error);

        int Width() const { return width; }
        int Height() const { return height; }
        uint8_t* Pixels() { return pixels.data(); }
        std::array<Rgb, 256>& Colours() { return palette; }
        void SetPalette(const Palette& p) { palette = p.colours; }

        void ToggleFullScreen();
        void Present();

        SDL_Window* Window() const { return window; }

    private:
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* texture = nullptr;
        int width = 0, height = 0;
        std::vector<uint8_t> pixels;
        std::vector<uint32_t> converted;
        std::array<Rgb, 256> palette{};
    };
}
