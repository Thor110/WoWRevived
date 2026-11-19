// A front-end screen: what the game builds as a full-screen window (WFullScreenMenu and friends) together with
// the "Fullscreen View" behind it (0x4061B0), which draws the backdrop and picks the palette group.
//
// Buttons work like the game's: the ones built by WGraphicPushButton (0x430AF0) have a solid plate and a glass
// panel drawn as a 50% blend, the plain graphic buttons (0x42F9B0) one sprite. Sprites have three or four frames:
// normal, highlighted, pressed (and disabled). Buttons send a command id plus a parameter, like the game's
// WM_COMMAND, when released over the button they were pressed on. Escape sends 0x8123 (back).
#pragma once
#include "frontend/Canvas.h"

#include <optional>
#include <string>
#include <vector>

namespace wow
{
    struct Command
    {
        int id = 0;
        int param = 0;
    };

    namespace Commands
    {
        constexpr int Back = 0x8123;
    }

    struct Button
    {
        enum class Kind { Push, Graphic };
        Kind kind = Kind::Push;
        Rect rect;
        int plate = 0;     // Push: solid plate (frame 0 is usually empty, frames 1-2 the outline); Graphic: the sprite
        int glass = 0;     // Push: glass panel, blended 50%
        int font = 0;
        int level = 0;     // text shade level
        std::string text;
        Command command;
        bool visible = true;
        bool focusable = true; // headings and similar are drawn like buttons but don't take part in input
    };

    class Screen
    {
    public:
        Screen(int id, Resources& res, int background, int paletteGroup)
            : id(id), background(background), paletteGroup(paletteGroup), res(res) {}
        virtual ~Screen() = default;

        int Id() const { return id; }
        int Background() const { return background; }
        int PaletteGroup() const { return paletteGroup; }

        virtual void Update() {}
        virtual void Draw(Canvas& canvas) const;
        // A screen can handle some of its own commands; returns true if it did.
        virtual bool Handle(const Command&) { return false; }

        std::optional<Command> OnKey(int sdlKey);
        void OnMouseMove(int x, int y);
        std::optional<Command> OnMouseButton(int x, int y, bool down);

    protected:
        // Adds a button sized from its plate sprite; returns its index.
        size_t Place(Button button);
        void DrawButton(Canvas& canvas, const Button& button, int frame) const;
        Rect SpriteRect(int gtype, int x, int y) const;

        int id;
        int background;
        int paletteGroup;
        Resources& res;
        std::vector<Button> buttons;
        int selected = -1;
        int pressed = -1;

    private:
        bool Usable(int index) const;
        int NextUsable(int from, int step) const;
        int HitTest(int x, int y) const;
    };
}
