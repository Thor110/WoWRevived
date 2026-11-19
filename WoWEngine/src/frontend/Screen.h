// A front-end screen: what the game builds as a full-screen window (WFullScreenMenu and friends) together with
// the "Fullscreen View" behind it (0x4061B0), which draws the backdrop and picks the palette group.
//
// Controls work like the game's. Each has a solid plate and usually a glass panel drawn as a 50% blend:
//   Push      WGraphicPushButton (0x430AF0). Frames: 0 normal, 1 highlighted, 2 pressed.
//   Graphic   plain sprite button (0x42F9B0). Same frames, plus 3 for disabled.
//   Check     checkbox (0x430CD0, rows built at 0x40A320 / 0x40A480). Frames: 0 cross, 1 cross highlighted,
//             2 tick highlighted, 3 tick.
//   Radio     radio button (0x430E10, rows built at 0x40A630): drawn like a checkbox, one per group is set.
//   Slider    scrollbar (0x431EF0, rows built at 0x40AC80). Plate frames 0-10 put the knob at 11 positions,
//             11-21 are the same with the highlight; the glass has frames 0 and 1.
// Buttons send a command id plus a parameter, like the game's WM_COMMAND, when released over the control they
// were pressed on. Escape sends 0x8123 (back). Checkboxes, radios and sliders change their value and call
// Changed() instead.
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
        enum class Kind { Push, Graphic, Check, Radio, Slider };
        Kind kind = Kind::Push;
        Rect rect;
        int plate = 0;
        int glass = 0;
        int font = 0;
        int level = 0;          // text shade level
        std::string text;
        int textOffset = -1;    // -1: text centred on the control; otherwise left-aligned this far in
        Command command;
        int value = 0;          // Check / Radio: 0 or 1; Slider: 0 to max
        int max = 0;            // Slider
        int group = 0;          // Radio
        bool visible = true;
        bool focusable = true;  // headings and similar are drawn like controls but don't take part in input
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
        // The screen Escape returns to (0: none).
        int Previous() const { return previous; }
        void SetPrevious(int screen) { previous = screen; }

        virtual void Update() {}
        virtual void Draw(Canvas& canvas) const;
        // A screen can handle some of its own commands; returns true if it did.
        virtual bool Handle(const Command&) { return false; }

        virtual std::optional<Command> OnKey(int sdlKey);
        void OnMouseMove(int x, int y);
        virtual std::optional<Command> OnMouseButton(int x, int y, bool down);

        // A command the screen sends by itself (from Update), picked up by the front end after each update.
        std::optional<Command> TakeCommand() { auto c = sent; sent.reset(); return c; }

    protected:
        // Called after a checkbox, radio or slider changed its value.
        virtual void Changed(size_t) {}

        // Adds a control sized from its plate sprite; returns its index.
        size_t Place(Button button);
        void DrawButton(Canvas& canvas, const Button& button, bool highlighted, bool pressedDown) const;
        Rect SpriteRect(int gtype, int x, int y) const;

        int id;
        int background;
        int paletteGroup;
        int previous = 0;
        Resources& res;
        std::vector<Button> buttons;
        int selected = -1;
        int pressed = -1;
        std::optional<Command> sent;

    private:
        bool Usable(int index) const;
        int NextUsable(int from, int step) const;
        int HitTest(int x, int y) const;
        std::optional<Command> Activate(int index);
        void SetSliderFromX(int index, int x);
        void SetValue(int index, int value);
    };
}
