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
//   Edit      text entry (0x430FC0, rows built at 0x40A8C0): a push button's plate and glass with the text
//             being typed, up to maxLength characters of Latin-1.
// Buttons send a command id plus a parameter, like the game's WM_COMMAND, when released over the control they
// were pressed on. Escape sends 0x8123 (back). Checkboxes, radios and sliders change their value and call
// Changed() instead.
#pragma once
#include "frontend/Canvas.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
        enum class Kind { Push, Graphic, Check, Radio, Slider, Edit };
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
        int maxLength = 0;      // Edit
        int caret = 0;          // Edit: the caret, after this many characters (+0xA0)
        bool visible = true;
        bool focusable = true;  // headings and similar are drawn like controls but don't take part in input
        int tooltip = 0;        // its tool tip's text id (the control's answer to 0x807C), 0 for none
        std::string tipText;    // MODERN: a tool tip that isn't a TEXT.ojd string (the engine's own rows)
        int widen = 0;          // MODERN: the width its glass is drawn at, centred on the row (0: its own)
        bool tallGlass = false; // MODERN (update 273): its glass made as tall as the row (a text block's backing)
    };

    class Screen
    {
    public:
        Screen(int id, Resources& res, int background, int paletteGroup)
            : id(id), background(background), paletteGroup(paletteGroup), res(res) {}
        virtual ~Screen() = default;

        // The game's display flag (byte dword_4EA364, set as the display is made, 0x42CA28): clear in a window, set
        // full screen. The views' edge scrolling takes a 32-pixel border in a window and [Screen] "Scroll Border"
        // (4) full screen (0x484E2C). Kept by main as the window changes.
        static inline bool windowed = true;

        int Id() const { return id; }
        int Background() const { return background; }
        int PaletteGroup() const { return paletteGroup; }
        // The screen Escape returns to (0: none).
        int Previous() const { return previous; }
        void SetPrevious(int screen) { previous = screen; }

        virtual void Update() {}
        // The game deactivated or reactivated (no updates come while it's paused).
        virtual void SetPaused(bool) {}
        virtual void Draw(Canvas& canvas) const;
        // A screen can handle some of its own commands; returns true if it did.
        virtual bool Handle(const Command&) { return false; }

        virtual std::optional<Command> OnKey(int sdlKey);
        // The cursor the controls ask for at a point (their WM_SETCURSOR answer, an index into the cursor table at
        // 0x4B9200): 0 is the normal arrow, 0x11 the text field's (0x431500).
        virtual int CursorAt(int x, int y) const;
        // Typed text (UTF-8, from SDL's text input), for an Edit control that has the focus.
        void OnText(std::string_view utf8);
        virtual void OnMouseMove(int x, int y);
        virtual void OnKeyUp(int) {}
        virtual std::optional<Command> OnMouseButton(int x, int y, bool down);
        // The right button: push buttons take it like the left (WGraphicPushButton 0x42FEF0, 0x204 / 0x205); the
        // rest of the controls don't.
        virtual std::optional<Command> OnRightButton(int x, int y, bool down);

        // Tool tips (the desktop's manager 0x433870 asks the window under the mouse): which part of the screen is
        // under the mouse (a control's index, or -1), and that part's text at a detail level of 1-3 (0x807C), empty
        // if it has none at that level.
        virtual int TipOwner(int x, int y) const { return HitTest(x, y); }
        // What the window under the mouse answers 0x80CF (0x4338E5): whether a held mouse button hides its tool
        // tip. The button base answers 1 (0x42E2EC); the popup menus' items (WHotButton 0x434390, 0x434465) answer
        // 0, as does a window that doesn't handle it, and keep their tips while a button is held.
        virtual bool TipHidesOnPress(int owner) const { (void)owner; return true; }
        // What it answers 0x80D0 (the tip's constructor 0x49D470 asks): where the tip's top left goes, in the
        // picture's pixels. A popup item answers its right edge less 4 and its top (0x434486); a window that
        // doesn't answers nothing, and the tip goes by where the hovering started.
        virtual std::optional<std::pair<int, int>> TipAnchor(int owner) const { (void)owner; return std::nullopt; }
        virtual std::string TipText(int owner, int level) const;

        // A command the screen sends by itself (from Update), picked up by the front end after each update.
        std::optional<Command> TakeCommand() { auto c = sent; sent.reset(); return c; }
        // Sounds (SFX.ojd ids) the screen wants played, taken by the front end after each update.
        std::vector<int> TakeSounds() { return std::exchange(sounds, {}); }

    protected:
        // Called after a checkbox, radio or slider changed its value.
        virtual void Changed(size_t) {}

        // Gives a control the keyboard focus (the game's 0x42EBE0).
        void Focus(size_t index) { if (index < buttons.size()) selected = int(index); }
        // Called after an Edit control's text changed.
        virtual void TextChanged(size_t) {}

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
        std::vector<int> sounds;

    private:
        // the text field (WEditField 0x430FC0)
        int TextLeft(const Button& b) const;
        void CaretFromX(Button& b, int x);           // 0x4313F0
        bool EditKey(Button& b, int key);            // 0x431730
        mutable int caretAnimation = 0;              // the caret sprite's frame, in 256ths (0x40EF20 with 0x33)
        bool Usable(int index) const;
        int NextUsable(int from, int step) const;
        int HitTest(int x, int y) const;
        std::optional<Command> Activate(int index);
        void SetSliderFromX(int index, int x);
        void SetValue(int index, int value);
    };
}
