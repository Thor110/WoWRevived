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
        // the wide (style 2) menus line their rows up with the two event columns
        button.rect.x = style == 2 ? ScreenW / 2 - 260 : 0;
        if (column >= 0) nextY = std::max(nextY, columnBottom); // rows after the columns go below the longer one
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

    void Menu::AddButton(int command, int textId, int param) { AddButtonText(command, res.String(textId), param); }

    void Menu::AddButtonText(int command, std::string text, int param)
    {
        Button b;
        b.plate = style == 0 ? 0x38 : style == 1 ? 0x43 : 0x3B;
        b.glass = style == 0 ? 0x39 : style == 1 ? 0x44 : 0x3C;
        b.font = ButtonFont;
        b.level = colours.items;
        b.text = std::move(text);
        b.command = { command, param };
        Stack(std::move(b));
    }

    void Menu::AddCheck(int textId, bool value, OnChange changed)
    {
        Button b;
        b.kind = Button::Kind::Check;
        b.plate = style == 2 ? 0x4B : 0x4D;
        b.glass = style == 2 ? 0x4C : 0x4E;
        b.font = ButtonFont;
        b.level = colours.staticText;
        b.text = res.String(textId);
        b.value = value ? 1 : 0;
        Stack(std::move(b));
        onChange[buttons.size() - 1] = std::move(changed);
    }

    void Menu::AddRadio(int textId, int group, bool value, OnChange changed)
    {
        AddCheck(textId, value, std::move(changed));
        buttons.back().kind = Button::Kind::Radio;
        buttons.back().group = group;
    }

    void Menu::AddSlider(int value, int max, std::function<std::string(int)> label, OnChange changed)
    {
        Button b;
        b.kind = Button::Kind::Slider;
        b.plate = style == 2 ? 0x347 : 0x345;
        b.glass = style == 2 ? 0x348 : 0x346;
        b.font = ButtonFont;
        b.level = colours.staticText;
        b.max = std::max(max, 0);
        b.value = std::clamp(value, 0, b.max);
        b.text = label(b.value);
        Stack(std::move(b));
        onChange[buttons.size() - 1] = std::move(changed);
        labels[buttons.size() - 1] = std::move(label);
    }

    void Menu::AddEventCheck(int textId, bool value, OnChange changed)
    {
        constexpr int ColumnWidth = 260; // the checkbox control's width (the sprite has 10 spare pixels)
        if (column < 0) { column = 0; columnTop = FirstItemY + nextY; }
        Button b;
        b.kind = Button::Kind::Check;
        b.plate = 0x4B;
        b.glass = 0x4C;
        b.font = ButtonFont;
        b.level = colours.staticText;
        b.text = res.String(textId);
        b.textOffset = 37;
        b.value = value ? 1 : 0;
        b.rect.x = column == 0 ? ScreenW / 2 - ColumnWidth : ScreenW / 2;
        b.rect.y = FirstItemY + nextY;
        size_t index = Place(std::move(b));
        nextY += buttons[index].rect.h;
        columnBottom = std::max(columnBottom, nextY);
        onChange[index] = std::move(changed);
    }

    void Menu::NextColumn()
    {
        column = 1;
        nextY = columnTop - FirstItemY;
    }

    void Menu::Changed(size_t index)
    {
        auto label = labels.find(index);
        if (label != labels.end()) buttons[index].text = label->second(buttons[index].value);
        auto handler = onChange.find(index);
        if (handler != onChange.end() && handler->second) handler->second(buttons[index]);
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
        int dx = style == 2 ? 0 : (ScreenW - (right - left)) / 2 - left; // the wide menus are placed already
        int dy = (ScreenH - (bottom - top)) / 2 - top;
        for (Button& b : buttons) { b.rect.x += dx; b.rect.y += dy; }
    }
}
