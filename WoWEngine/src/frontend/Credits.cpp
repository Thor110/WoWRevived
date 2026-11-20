#include "frontend/Credits.h"

#include <SDL3/SDL_keycode.h>

namespace wow
{
    Credits::Credits(Resources& r, int previous, bool egg, Sound& snd) : Screen(Id, r, 0, 0x2F3), easterEgg(egg), sound(snd)
    {
        SetPrevious(previous);
        const IndexedImage* image = res.GetFrame(Image);
        int height = image ? image->height : 0;
        speed = float(height - 640) * (res.Human() ? 1.0f / 3150.0f : 1.0f / 4200.0f);
        end = height - (easterEgg ? 480 : 960);
        // the credits music: VOX "hepilog" in Human games, "mepilog" in Martian ones (0x40537B)
        musicHandle = sound.Play(0x191);
    }

    Credits::~Credits() { sound.Stop(musicHandle); }

    std::optional<Command> Credits::Leave() { return Command{ Previous(), 0 }; }

    void Credits::Update()
    {
        ticks++;
        if (ticks == MinimumTicks && leaveRequested) { sent = Leave(); return; }
        // The scroll steps on message 0x812E, which re-posts itself for the next tick, so it runs every other
        // game tick: about 15 steps a second, which is what makes each side's scroll last as long as its music
        // (Human: 2988 steps for 206.8 s of hepilog; Martian: 3984 steps for 272.5 s of mepilog).
        if (ticks % 2) return;
        if (holdTicks >= 0)
        {
            if (--holdTicks <= 0) sent = Leave();
            return;
        }
        if (position + speed > float(end) || speed <= 0.0f)
        {
            // at the bottom, it waits for the music to finish
            if (!easterEgg && sound.Active(musicHandle)) return;
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
