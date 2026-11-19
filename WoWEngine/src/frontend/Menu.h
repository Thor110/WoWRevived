// The full-screen menus (WFullScreenMenu in the game, constructor 0x40A060).
//
// A menu is built from a heading and push buttons, stacked from the top in the order they're added, each taking
// its sprite's height. Adding the bottom button (0x40AB00) then centres the whole stack on the screen.
// Each item has two graphics: a plate and a highlight shown on hover. Which pair is used depends on the menu's style:
//                    heading       button        bottom button
//   style 0          0x3E / 0x3F   0x38 / 0x39   0x38 / 0x3A
//   style 1          0x3E / 0x3F   0x43 / 0x44   0x38 / 0x3A
//   style 2          0x40 / 0x41   0x3B / 0x3C   0x3B / 0x3D
// The heading's text uses font 0x6D and buttons use font 0x73, centred both ways on the plate.
// Buttons send their command id when pressed; Escape sends 0x8123 (back).
#pragma once
#include "frontend/Canvas.h"

#include <optional>
#include <string>
#include <vector>

namespace wow
{
    namespace Command
    {
        constexpr int Back = 0x8123;
    }

    class Menu
    {
    public:
        Menu(int id, Resources& res, int style = 0) : id(id), style(style), res(res) {}

        int Id() const { return id; }

        void AddHeading(int textId);
        void AddButton(int command, int textId);
        void AddBottomButton(int command, int textId); // also centres the menu
        // a heading or button with text that isn't in TEXT.ojd (used for screens not rebuilt yet)
        void AddHeadingText(std::string text);

        void Draw(Canvas& canvas) const;

        // Input. Each returns a command id when one is sent.
        std::optional<int> OnKey(int sdlKey);
        void OnMouseMove(int x, int y);
        std::optional<int> OnMouseButton(int x, int y, bool down);

    private:
        enum class Kind { Heading, Button };
        struct Item
        {
            Kind kind = Kind::Button;
            int command = 0;
            std::string text;
            int plate = 0, highlight = 0, font = 0;
            Rect rect;
        };

        void Add(Item item);
        void Centre();
        int NextButton(int from, int step) const;

        int id;
        int style;
        Resources& res;
        std::vector<Item> items;
        int nextY = 0;
        int selected = -1;
        int pressed = -1;
        int screenW = 640, screenH = 480;
    };
}
