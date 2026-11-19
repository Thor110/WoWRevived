#include "frontend/Menu.h"

#include <algorithm>

namespace wow
{
    namespace
    {
        constexpr int HeadingFont = 0x6D;
        constexpr int ButtonFont = 0x73;
        constexpr int FirstItemY = 0xA0;
        constexpr int ScreenW = 640, ScreenH = 480;
    }

    void Menu::Stack(Button button)
    {
        button.rect.x = 0;
        button.rect.y = FirstItemY + nextY;
        size_t index = Place(std::move(button));
        nextY += buttons[index].rect.h;
    }

    void Menu::AddHeadingText(std::string text)
    {
        Button b;
        b.plate = style == 2 ? 0x40 : 0x3E;
        b.glass = style == 2 ? 0x41 : 0x3F;
        b.font = HeadingFont;
        b.level = colours.heading;
        b.text = std::move(text);
        b.focusable = false;
        Stack(std::move(b));
    }

    void Menu::AddButton(int command, int textId, int param)
    {
        Button b;
        b.plate = style == 0 ? 0x38 : style == 1 ? 0x43 : 0x3B;
        b.glass = style == 0 ? 0x39 : style == 1 ? 0x44 : 0x3C;
        b.font = ButtonFont;
        b.level = colours.items;
        b.text = res.String(textId);
        b.command = { command, param };
        Stack(std::move(b));
    }

    void Menu::AddBottomButton(int command, int textId)
    {
        Button b;
        b.plate = style == 2 ? 0x3B : 0x38;
        b.glass = style == 2 ? 0x3D : 0x3A;
        b.font = ButtonFont;
        b.level = colours.items;
        b.text = res.String(textId);
        b.command = { command, 0 };
        Stack(std::move(b));
        Centre();
    }

    void Menu::Centre()
    {
        if (buttons.empty()) return;
        int left = buttons[0].rect.x, top = buttons[0].rect.y, right = left, bottom = top;
        for (const Button& b : buttons)
        {
            left = std::min(left, b.rect.x);
            top = std::min(top, b.rect.y);
            right = std::max(right, b.rect.x + b.rect.w);
            bottom = std::max(bottom, b.rect.y + b.rect.h);
        }
        // the game moves the whole stack so its bounding box is in the middle of the screen; items keep their
        // left edges (they're all added at x = 0)
        int dx = (ScreenW - (right - left)) / 2 - left;
        int dy = (ScreenH - (bottom - top)) / 2 - top;
        for (Button& b : buttons) { b.rect.x += dx; b.rect.y += dy; }
    }
}
