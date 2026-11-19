#include "frontend/Credits.h"

#include <SDL3/SDL_keycode.h>

namespace wow
{
    Credits::Credits(Resources& r, int previous, bool egg) : Screen(Id, r, 0, 0x2F3), easterEgg(egg)
    {
        SetPrevious(previous);
        const IndexedImage* image = res.GetFrame(Image);
        int height = image ? image->height : 0;
        speed = float(height - 640) * (res.Human() ? 1.0f / 3150.0f : 1.0f / 4200.0f);
        end = height - (easterEgg ? 480 : 960);
        // TODO: play the credits music (sound 0x191) once sound effects are rebuilt; the game also waits for it
        // to finish before leaving
    }

    std::optional<Command> Credits::Leave() { return Command{ Previous(), 0 }; }

    void Credits::Update()
    {
        ticks++;
        if (ticks == MinimumTicks && leaveRequested) { sent = Leave(); return; }
        if (holdTicks >= 0)
        {
            if (--holdTicks <= 0) sent = Leave();
            return;
        }
        if (position + speed > float(end) || speed <= 0.0f)
        {
            if (easterEgg) holdTicks = 300; // 0x12C: stay on the picture before leaving
            else sent = Leave();
            return;
        }
        position += speed;
    }

    void Credits::Draw(Canvas& canvas) const
    {
        canvas.DrawSprite(Image, 0, 0, -int(position));
    }

    std::optional<Command> Credits::OnKey(int key)
    {
        if (key != SDLK_ESCAPE) return std::nullopt;
        if (ticks >= MinimumTicks) return Leave();
        leaveRequested = true;
        return std::nullopt;
    }

    std::optional<Command> Credits::OnMouseButton(int, int, bool down)
    {
        if (!down) return std::nullopt; // the game leaves on the button going down (0x201 / 0x204)
        if (ticks >= MinimumTicks) return Leave();
        leaveRequested = true;
        return std::nullopt;
    }
}
