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
        // each heading, row and bottom button is centred across the menu (0x42F020 with 0x10 from 0x40A7D0, 0x40A320
        // and 0x40AB00), whatever the cursor's x; the bottom button then centres the union of them all (Centre).
        // *Provisional*: only the wide (style 2) menus centre each control; the others keep their left edges together
        // (from x = 0), as the engine's check and slider controls are only their box sprite's width, not the whole
        // control's (0x430CD0), so centring them alone would misplace them
        if (column >= 0) nextY = std::max(nextY, columnBottom); // rows after the columns go below the longer one
        button.rect.y = FirstItemY + nextY;
        size_t index = Place(std::move(button));
        if (style == 2) buttons[index].rect.x += (ScreenW - buttons[index].rect.w) / 2;   // (from x = 0, less any hot spot)
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

    void Menu::AddCheck(int textId, bool value, OnChange changed, int tooltip)
    {
        Button b;
        b.kind = Button::Kind::Check;
        b.plate = style == 2 ? 0x4B : 0x4D;
        b.glass = style == 2 ? 0x4C : 0x4E;
        b.font = ButtonFont;
        b.level = colours.staticText;
        b.text = res.String(textId);
        b.value = value ? 1 : 0;
        b.tooltip = tooltip;
        Stack(std::move(b));
        onChange[buttons.size() - 1] = std::move(changed);
    }

    void Menu::AddEventCheckText(std::string text, bool value, OnChange changed)
    {
        AddEventCheck(0, value, std::move(changed));
        buttons.back().text = std::move(text);
    }

    void Menu::AddRadio(int textId, int group, bool value, OnChange changed, int tooltip)
    {
        AddCheck(textId, value, std::move(changed), tooltip);
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
        // the columns at W / 2 less the checkbox control's width and at W / 2 (0x40A54A-0x40A580), then centred with
        // the rest by the bottom button
        const IndexedImage* plate = res.GetFrame(0x4B, 0);
        const int ColumnWidth = plate ? plate->width : 270;
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

    void Menu::AddTextEntry(std::string text, int maxLength, int command, std::function<void(const std::string&)> changed)
    {
        Button b;
        b.kind = Button::Kind::Edit;
        b.plate = style == 0 ? 0x38 : style == 1 ? 0x43 : 0x3B;
        b.glass = style == 0 ? 0x39 : style == 1 ? 0x44 : 0x3C;
        b.font = ButtonFont;
        b.level = colours.items;
        b.maxLength = maxLength;
        b.text = text.substr(0, size_t(std::max(maxLength, 0)));
        b.command = { command, 0 };
        Stack(std::move(b));
        onText[buttons.size() - 1] = std::move(changed);
    }

    void Menu::TextChanged(size_t index)
    {
        auto handler = onText.find(index);
        if (handler != onText.end() && handler->second) handler->second(buttons[index].text);
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
        // the game moves the whole stack so its bounding box is in the middle of the screen (0x42E630 over the union
        // of the children's rects, applied by 0x42EEC0, 0x40ABDA), every style alike: only the event menu's columns,
        // placed by hand, move it
        int dx = (ScreenW - (right - left)) / 2 - left;
        int dy = (ScreenH - (bottom - top)) / 2 - top;
        for (Button& b : buttons) { b.rect.x += dx; b.rect.y += dy; }
    }

    void Menu::Update()
    {
        Screen::Update();
        if (++glowTicks < 2) return;
        glowTicks = 0;
        if (glowLevel < 0) glowLevel = colours.glowFirst;
        if (selected < 0 || size_t(selected) >= buttons.size()) return;
        if (selected != glowing)
        {
            if (glowing >= 0 && size_t(glowing) < buttons.size()) buttons[size_t(glowing)].level = colours.items;
            glowing = selected;
        }
        glowLevel += glowStep;
        if (glowLevel > colours.glowLast) { glowLevel--; glowStep = -1; }
        if (glowLevel < colours.glowFirst) { glowLevel++; glowStep = 1; }
        buttons[size_t(selected)].level = glowLevel;
    }
}
