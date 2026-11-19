#include "platform/Video.h"

namespace wow
{
    Video::~Video()
    {
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
    }

    bool Video::Open(const char* title, int w, int h, bool fullScreen, std::string& error)
    {
        width = w;
        height = h;
        SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | (fullScreen ? SDL_WINDOW_FULLSCREEN : 0);
        if (!SDL_CreateWindowAndRenderer(title, w, h, flags, &window, &renderer)) { error = SDL_GetError(); return false; }
        SDL_SetRenderVSync(renderer, 1);
        if (!SDL_SetRenderLogicalPresentation(renderer, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX)) { error = SDL_GetError(); return false; }
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        if (!texture) { error = SDL_GetError(); return false; }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        pixels.assign(size_t(w) * size_t(h), 0);
        converted.assign(pixels.size(), 0);
        return true;
    }

    void Video::ToggleFullScreen()
    {
        bool isFull = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
        SDL_SetWindowFullscreen(window, !isFull);
    }

    void Video::Present()
    {
        uint32_t lookup[256];
        for (int i = 0; i < 256; i++) lookup[i] = (uint32_t(palette[i].r) << 16) | (uint32_t(palette[i].g) << 8) | palette[i].b;
        for (size_t i = 0; i < pixels.size(); i++) converted[i] = lookup[pixels[i]];

        SDL_UpdateTexture(texture, nullptr, converted.data(), width * int(sizeof(uint32_t)));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
}
