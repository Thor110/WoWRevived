// The credits (command 0x80CA): constructor 0x4052B0, messages 0x4055C0, scrolling 0x4056F0, set-up 0x405470.
//
// The whole screen is CREDITS.SPR (GType 0x344, 644 x 5692) in palette group 0x2F3 (F3), scrolled upwards while
// the credits music (sound 0x191) plays:
//   position   starts at -60 and moves by speed every step (message 0x812E, which re-posts itself for the next
//              tick, so a step comes every other game tick, about 15 a second)
//   speed      (image height - 640) / 3150 in Human games, / 4200 in Martian ones (0x4A9334 / 0x4A9338), so each
//              side's scroll fits its own credits music
//   end        once the position passes image height - 960 and the music has finished, it returns to the menu it
//              was opened from
// With the "englishrose" cheat typed (flag 0x4D25A4, codes at 0x401000/0x401280) the end is image height - 480
// instead, which scrolls on to a hidden picture of the team at the bottom of the image (Dave, James, Mat, Dobbin,
// Craig, Charlie and Andy), and it stays there for 300 steps before leaving.
// Opening the credits fades every sound out over 60 ticks (0x464AB0) and starts the credits music.
// Escape or a click leaves too, but not in the first 60 ticks (message 0x80F2); one in that time is remembered.
#pragma once
#include "frontend/Screen.h"
#include "platform/Sound.h"

namespace wow
{
    class Credits : public Screen
    {
    public:
        static constexpr int Id = 0x80CA;

        Credits(Resources& res, int previous, bool easterEgg, Sound& sound);
        ~Credits() override;

        void Update() override;
        void Draw(Canvas& canvas) const override;
        std::optional<Command> OnKey(int sdlKey) override;
        std::optional<Command> OnMouseButton(int x, int y, bool down) override;
        std::optional<Command> OnRightButton(int x, int y, bool down) override { return OnMouseButton(x, y, down); } // 0x405672

    private:
        std::optional<Command> Leave();

        static constexpr int Image = 0x344;
        static constexpr int MinimumTicks = 60;
        float position = -60.0f;
        float speed = 0.0f;
        int end = 0;
        int ticks = 0;
        bool leaveRequested = false;
        bool easterEgg = false;
        int holdTicks = -1; // counting down at the end in the easter egg version
        Sound& sound;
        int musicHandle = 0; // +0xB0
    };
}
