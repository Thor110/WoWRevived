// The window and the frame buffer.
// The game drew each sprite through its own palette (or, in 16-bit mode, its own shade table), so different
// palettes are on screen at once. The frame buffer is therefore true colour (XRGB8888) at the game's resolution,
// scaled to the window with nearest-neighbour filtering and letterboxing.
#pragma once
#include <SDL3/SDL.h>

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
        uint32_t* Pixels() { return pixels.data(); }

        // The picture's size changed (the view size, docs/MODERN.md "Resolution"): a new frame buffer and texture
        bool Resize(int width, int height);
        // Stretched over the whole window, or its shape kept with bars (the default)
        void SetStretch(bool on);
        void SetFullScreen(bool on);
        void ToggleFullScreen();
        void Present();

        // Window coordinates (from mouse events) to frame buffer coordinates.
        bool ToGame(float windowX, float windowY, int& x, int& y) const;

        SDL_Window* Window() const { return window; }

    private:
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* texture = nullptr;
        int width = 0, height = 0;
        bool stretch = false;
        void Present_Filter();
        std::vector<uint32_t> pixels;
    };
}
