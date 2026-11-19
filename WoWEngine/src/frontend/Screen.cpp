#include "frontend/Screen.h"

#include <SDL3/SDL_keycode.h>

#include <algorithm>

namespace wow
{
    Rect Screen::SpriteRect(int gtype, int x, int y) const
    {
        const IndexedImage* image = res.GetFrame(gtype);
        return { x, y, image ? image->width : 0, image ? image->height : 0 };
    }

    size_t Screen::Place(Button button)
    {
        Rect size = SpriteRect(button.plate, button.rect.x, button.rect.y);
        button.rect.w = size.w;
        button.rect.h = size.h;
        buttons.push_back(std::move(button));
        if (selected < 0 && Usable(int(buttons.size()) - 1)) selected = int(buttons.size()) - 1;
        return buttons.size() - 1;
    }

    bool Screen::Usable(int index) const
    {
        if (index < 0 || size_t(index) >= buttons.size()) return false;
        const Button& b = buttons[size_t(index)];
        return b.visible && b.focusable;
    }

    void Screen::DrawButton(Canvas& canvas, const Button& b, int frame) const
    {
        if (!b.visible) return;
        if (b.kind == Button::Kind::Push)
        {
            if (b.glass) canvas.DrawSprite(b.glass, frame, b.rect.x, b.rect.y, 0, Canvas::Blend::Translucent);
            canvas.DrawSprite(b.plate, frame, b.rect.x, b.rect.y);
            if (!b.text.empty()) canvas.DrawString(b.font, b.text, b.rect, TextCentreX | TextCentreY, b.level);
        }
        else canvas.DrawSprite(b.plate, frame, b.rect.x, b.rect.y);
    }

    void Screen::Draw(Canvas& canvas) const
    {
        for (size_t i = 0; i < buttons.size(); i++)
        {
            // TODO: check the frame numbers against the button code (WGraphicPushButton, 0x42FC20)
            int frame = int(i) == pressed ? 2 : (int(i) == selected && Usable(int(i))) ? 1 : 0;
            DrawButton(canvas, buttons[i], frame);
        }
    }

    int Screen::NextUsable(int from, int step) const
    {
        int n = int(buttons.size());
        for (int i = 1; i <= n; i++)
        {
            int k = ((from + step * i) % n + n) % n;
            if (Usable(k)) return k;
        }
        return from;
    }

    int Screen::HitTest(int x, int y) const
    {
        for (size_t i = 0; i < buttons.size(); i++)
            if (Usable(int(i)) && buttons[i].rect.Contains(x, y)) return int(i);
        return -1;
    }

    std::optional<Command> Screen::OnKey(int key)
    {
        if (key == SDLK_ESCAPE) return Command{ Commands::Back, 0 };
        if (buttons.empty()) return std::nullopt;
        if (!Usable(selected)) selected = NextUsable(-1, 1);
        if (key == SDLK_UP || key == SDLK_LEFT) selected = NextUsable(std::max(selected, 0), -1);
        else if (key == SDLK_DOWN || key == SDLK_RIGHT || key == SDLK_TAB) selected = NextUsable(std::max(selected, 0), 1);
        else if ((key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) && Usable(selected))
            return buttons[size_t(selected)].command;
        return std::nullopt;
    }

    void Screen::OnMouseMove(int x, int y)
    {
        int hit = HitTest(x, y);
        if (hit >= 0) selected = hit;
    }

    std::optional<Command> Screen::OnMouseButton(int x, int y, bool down)
    {
        int hit = HitTest(x, y);
        if (down) { pressed = hit; return std::nullopt; }
        // like a Windows button: the command is sent when it's released over the button that was pressed
        int was = pressed;
        pressed = -1;
        if (hit >= 0 && hit == was) return buttons[size_t(hit)].command;
        return std::nullopt;
    }
}
