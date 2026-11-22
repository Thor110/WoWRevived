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
        return true;
    }

    bool Video::Resize(int w, int h)
    {
        if (w == width && h == height) return true;
        SDL_Texture* next = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        if (!next) return false;
        if (texture) SDL_DestroyTexture(texture);
        texture = next;
        width = w;
        height = h;
        pixels.assign(size_t(w) * size_t(h), 0);
        SDL_SetRenderLogicalPresentation(renderer, w, h, stretch ? SDL_LOGICAL_PRESENTATION_STRETCH : SDL_LOGICAL_PRESENTATION_LETTERBOX);
        return true;
    }

    void Video::SetStretch(bool on)
    {
        stretch = on;
        if (renderer) SDL_SetRenderLogicalPresentation(renderer, width, height, on ? SDL_LOGICAL_PRESENTATION_STRETCH : SDL_LOGICAL_PRESENTATION_LETTERBOX);
    }

    void Video::SetFullScreen(bool on) { if (window) SDL_SetWindowFullscreen(window, on); }

    // Sharp pixels when the picture goes into the window a whole number of times, smoothed when it doesn't (pixels
    // of uneven sizes look worse than a little softness)
    void Video::Present_Filter()
    {
        int ow = 0, oh = 0;
        if (!SDL_GetCurrentRenderOutputSize(renderer, &ow, &oh) || width <= 0 || height <= 0) return;
        const bool whole = ow >= width && oh >= height && ow % width == 0 && oh % height == 0 && ow / width == oh / height;
        SDL_SetTextureScaleMode(texture, whole || (ow == width && oh == height) ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
    }

    void Video::ToggleFullScreen()
    {
        bool isFull = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
        SDL_SetWindowFullscreen(window, !isFull);
    }

    bool Video::ToGame(float windowX, float windowY, int& x, int& y) const
    {
        float gx = 0, gy = 0;
        if (!SDL_RenderCoordinatesFromWindow(renderer, windowX, windowY, &gx, &gy)) return false;
        x = int(gx);
        y = int(gy);
        return x >= 0 && y >= 0 && x < width && y < height;
    }

    void Video::Present()
    {
        Present_Filter();
        SDL_UpdateTexture(texture, nullptr, pixels.data(), width * int(sizeof(uint32_t)));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
}
