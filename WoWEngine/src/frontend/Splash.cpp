#include "frontend/Splash.h"

#include <SDL3/SDL_keycode.h>

namespace wow
{
    std::optional<Command> SplashScreen::OnKey(int sdlKey)
    {
        if (sdlKey == SDLK_ESCAPE) Skip(); // only Escape (WM_KEYDOWN 0x1B), not any key
        return std::nullopt;
    }
}
