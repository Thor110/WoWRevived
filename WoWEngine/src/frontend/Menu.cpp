#include "frontend/Menu.h"

#include <SDL3/SDL_keycode.h>

#include <algorithm>

namespace wow
{
    namespace
    {
        constexpr int HeadingFont = 0x6D;
        constexpr int ButtonFont = 0x73;
        constexpr int FirstItemY = 0xA0; // the menus start their stack at y = 160 before centring
    }

    void Menu::Add(Item item)
    {
        const IndexedImage* plate = res.GetFrame(item.plate);
        int w = plate ? plate->width : 200, h = plate ? plate->height : 24;
        item.rect = { 0, FirstItemY + nextY, w, h };
        nextY += h;
        items.push_back(std::move(item));
        if (selected < 0 && items.back().kind == Kind::Button) selected = int(items.size()) - 1;
    }

    void Menu::AddHeading(int textId) { AddHeadingText(res.String(textId)); }

    void Menu::AddHeadingText(std::string text)
    {
        Item item;
        item.kind = Kind::Heading;
        item.text = std::move(text);
        item.plate = style == 2 ? 0x40 : 0x3E;
        item.highlight = style == 2 ? 0x41 : 0x3F;
        item.font = HeadingFont;
        Add(std::move(item));
    }

    void Menu::AddButton(int command, int textId)
    {
        Item item;
        item.kind = Kind::Button;
        item.command = command;
        item.text = res.String(textId);
        item.plate = style == 0 ? 0x38 : style == 1 ? 0x43 : 0x3B;
        item.highlight = style == 0 ? 0x39 : style == 1 ? 0x44 : 0x3C;
        item.font = ButtonFont;
        Add(std::move(item));
    }

    void Menu::AddBottomButton(int command, int textId)
    {
        Item item;
        item.kind = Kind::Button;
        item.command = command;
        item.text = res.String(textId);
        item.plate = style == 2 ? 0x3B : 0x38;
        item.highlight = style == 2 ? 0x3D : 0x3A;
        item.font = ButtonFont;
        Add(std::move(item));
        Centre();
    }

    void Menu::Centre()
    {
        if (items.empty()) return;
        int left = items[0].rect.x, top = items[0].rect.y, right = left, bottom = top;
        for (const Item& it : items)
        {
            left = std::min(left, it.rect.x);
            top = std::min(top, it.rect.y);
            right = std::max(right, it.rect.x + it.rect.w);
            bottom = std::max(bottom, it.rect.y + it.rect.h);
        }
        // The game moves the whole stack so its bounding box sits in the middle of the screen. Items keep their
        // left edges (they're all added at x = 0), which lines up as long as the plates are the same width.
        int dx = (screenW - (right - left)) / 2 - left;
        int dy = (screenH - (bottom - top)) / 2 - top;
        for (Item& it : items) { it.rect.x += dx; it.rect.y += dy; }
    }

    void Menu::Draw(Canvas& canvas) const
    {
        for (size_t i = 0; i < items.size(); i++)
        {
            const Item& it = items[i];
            bool lit = int(i) == selected;
            canvas.DrawSprite(lit ? it.highlight : it.plate, 0, it.rect.x, it.rect.y);
            canvas.DrawString(it.font, it.text, it.rect, TextCentreX | TextCentreY);
        }
    }

    int Menu::NextButton(int from, int step) const
    {
        int n = int(items.size());
        for (int i = 1; i <= n; i++)
        {
            int k = ((from + step * i) % n + n) % n;
            if (items[size_t(k)].kind == Kind::Button) return k;
        }
        return from;
    }

    std::optional<int> Menu::OnKey(int key)
    {
        if (key == SDLK_ESCAPE) return Command::Back;
        if (items.empty()) return std::nullopt;
        if (key == SDLK_UP) selected = NextButton(std::max(selected, 0), -1);
        else if (key == SDLK_DOWN || key == SDLK_TAB) selected = NextButton(std::max(selected, 0), 1);
        else if ((key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) && selected >= 0)
            return items[size_t(selected)].command;
        return std::nullopt;
    }

    void Menu::OnMouseMove(int x, int y)
    {
        for (size_t i = 0; i < items.size(); i++)
            if (items[i].kind == Kind::Button && items[i].rect.Contains(x, y)) selected = int(i);
    }

    std::optional<int> Menu::OnMouseButton(int x, int y, bool down)
    {
        int hit = -1;
        for (size_t i = 0; i < items.size(); i++)
            if (items[i].kind == Kind::Button && items[i].rect.Contains(x, y)) hit = int(i);
        if (down) { pressed = hit; return std::nullopt; }
        // like a Windows button: the command is sent when the button is released over the one that was pressed
        int was = pressed;
        pressed = -1;
        if (hit >= 0 && hit == was) return items[size_t(hit)].command;
        return std::nullopt;
    }
}
