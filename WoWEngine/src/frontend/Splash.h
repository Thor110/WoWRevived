// The legal splash screens at start-up (message 0x74 -> 0x405BF0, handler 0x405DE0): legal1.spr (GType 0x36D) with
// palette group 0x2F5, legal2.SPR (0x36E) from tick 180, and on to the disc check and RAGELOGO (0x8133 with 0x8036)
// at tick 300. Escape or a click before tick 90 (0x80F2) is remembered and acted on then; after it, at once.
#pragma once
#include "frontend/Screen.h"

namespace wow
{
    class SplashScreen : public Screen
    {
    public:
        static constexpr int Id = 0x74;
        SplashScreen(Resources& res, Command exit) : Screen(Id, res, 0x36D, 0x2F5), exit(exit) {}

        void Update() override
        {
            ticks++;
            if (ticks == 90) { enabled = true; if (skipped) sent = exit; }
            if (ticks == 180) background = 0x36E;
            if (ticks == 300 && !sent) sent = exit;
        }
        void Draw(Canvas&) const override {}
        std::optional<Command> OnKey(int sdlKey) override;
        std::optional<Command> OnMouseButton(int, int, bool down) override { if (down) Skip(); return std::nullopt; }
        // a right click skips as a left one does (0x405E6E: 0x204 with 0x201)
        std::optional<Command> OnRightButton(int x, int y, bool down) override { return OnMouseButton(x, y, down); }
        int CursorAt(int, int) const override { return 0; }

    private:
        void Skip() { if (enabled) sent = exit; else skipped = true; }
        Command exit;
        int ticks = 0;
        bool enabled = false, skipped = false;
    };
}
