#include "frontend/Screen.h"

#include <SDL3/SDL_keycode.h>

#include <algorithm>

namespace wow
{
    namespace
    {
        // the slider knob's centre in frame 0 and frame 10, measured from slidr.spr
        constexpr int KnobInset = 22;
    }

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

    void Screen::DrawButton(Canvas& canvas, const Button& b, bool lit, bool down) const
    {
        if (!b.visible) return;
        int plateFrame = 0, glassFrame = 0;
        switch (b.kind)
        {
        case Button::Kind::Push:
        case Button::Kind::Graphic:
        case Button::Kind::Edit:
            // 0x433DB0: 0 normal, 1 highlighted, 2 pressed and highlighted, 3 pressed but not highlighted
            plateFrame = glassFrame = down ? (lit ? 2 : 3) : lit ? 1 : 0;
            break;
        case Button::Kind::Check:
        case Button::Kind::Radio:
            plateFrame = glassFrame = b.value ? (lit ? 2 : 3) : (lit ? 1 : 0);
            break;
        case Button::Kind::Slider:
        {
            int position = b.max > 0 ? (b.value * 10 + b.max / 2) / b.max : 0;
            plateFrame = position + (lit ? 11 : 0);
            glassFrame = lit ? 1 : 0;
            break;
        }
        }
        if (b.glass) canvas.DrawSprite(b.glass, glassFrame, b.rect.x, b.rect.y, 0, Canvas::Blend::Translucent);
        canvas.DrawSprite(b.plate, plateFrame, b.rect.x, b.rect.y);
        if (b.kind == Button::Kind::Edit)
        {
            // the text being typed, with a caret while the field has the focus
            canvas.DrawString(b.font, lit ? b.text + "_" : b.text, b.rect, TextCentreX | TextCentreY, b.level);
            return;
        }
        if (b.text.empty()) return;
        if (b.textOffset < 0) canvas.DrawString(b.font, b.text, b.rect, TextCentreX | TextCentreY, b.level);
        else
        {
            Rect r = b.rect;
            r.x += b.textOffset;
            r.w -= b.textOffset;
            canvas.DrawString(b.font, b.text, r, TextCentreY, b.level);
        }
    }

    void Screen::Draw(Canvas& canvas) const
    {
        for (size_t i = 0; i < buttons.size(); i++)
            DrawButton(canvas, buttons[i], int(i) == selected && Usable(int(i)), int(i) == pressed);
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

    void Screen::SetValue(int index, int value)
    {
        Button& b = buttons[size_t(index)];
        value = std::clamp(value, 0, b.kind == Button::Kind::Slider ? b.max : 1);
        if (b.kind == Button::Kind::Radio)
        {
            for (size_t i = 0; i < buttons.size(); i++)
            {
                Button& other = buttons[i];
                if (int(i) != index && other.kind == Button::Kind::Radio && other.group == b.group && other.value)
                {
                    other.value = 0;
                    Changed(i);
                }
            }
            value = 1;
        }
        if (b.value == value) return;
        b.value = value;
        Changed(size_t(index));
    }

    void Screen::SetSliderFromX(int index, int x)
    {
        const Button& b = buttons[size_t(index)];
        int span = std::max(1, b.rect.w - 2 * KnobInset);
        int position = ((x - b.rect.x - KnobInset) * b.max + span / 2) / span;
        SetValue(index, position);
    }

    std::optional<Command> Screen::Activate(int index)
    {
        Button& b = buttons[size_t(index)];
        switch (b.kind)
        {
        case Button::Kind::Check: SetValue(index, b.value ? 0 : 1); return std::nullopt;
        case Button::Kind::Radio: SetValue(index, 1); return std::nullopt;
        case Button::Kind::Slider: return std::nullopt;
        case Button::Kind::Edit: return b.command.id ? std::optional<Command>(b.command) : std::nullopt;
        default: return b.command;
        }
    }

    std::optional<Command> Screen::OnKey(int key)
    {
        if (key == SDLK_ESCAPE) return Command{ Commands::Back, 0 };
        if (buttons.empty()) return std::nullopt;
        if (!Usable(selected)) selected = NextUsable(-1, 1);
        bool slider = Usable(selected) && buttons[size_t(selected)].kind == Button::Kind::Slider;
        if (Usable(selected) && buttons[size_t(selected)].kind == Button::Kind::Edit)
        {
            // a text field takes the typing (OnText); keys that would press a button only edit it
            Button& b = buttons[size_t(selected)];
            if (key == SDLK_BACKSPACE)
            {
                if (!b.text.empty()) { b.text.pop_back(); TextChanged(size_t(selected)); }
                return std::nullopt;
            }
            if (key == SDLK_SPACE || key == SDLK_LEFT || key == SDLK_RIGHT) return std::nullopt;
        }
        if (slider && (key == SDLK_LEFT || key == SDLK_RIGHT))
            SetValue(selected, buttons[size_t(selected)].value + (key == SDLK_RIGHT ? 1 : -1));
        else if (key == SDLK_UP || key == SDLK_LEFT) selected = NextUsable(std::max(selected, 0), -1);
        else if (key == SDLK_DOWN || key == SDLK_RIGHT || key == SDLK_TAB) selected = NextUsable(std::max(selected, 0), 1);
        else if ((key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) && Usable(selected))
            return Activate(selected);
        return std::nullopt;
    }

    void Screen::OnMouseMove(int x, int y)
    {
        // dragging a slider's knob
        if (pressed >= 0 && buttons[size_t(pressed)].kind == Button::Kind::Slider) { SetSliderFromX(pressed, x); return; }
        int hit = HitTest(x, y);
        if (hit >= 0) selected = hit;
    }

    std::optional<Command> Screen::OnMouseButton(int x, int y, bool down)
    {
        int hit = HitTest(x, y);
        if (down)
        {
            pressed = hit;
            if (hit >= 0 && buttons[size_t(hit)].kind == Button::Kind::Slider) SetSliderFromX(hit, x);
            return std::nullopt;
        }
        // like a Windows button: it acts when released over the control that was pressed
        int was = pressed;
        pressed = -1;
        if (hit >= 0 && hit == was && buttons[size_t(hit)].kind != Button::Kind::Edit) return Activate(hit);
        return std::nullopt;
    }

    int Screen::CursorAt(int x, int y) const
    {
        int hit = HitTest(x, y);
        if (hit >= 0 && buttons[size_t(hit)].kind == Button::Kind::Edit) return 0x11;
        return 0;
    }

    void Screen::OnText(std::string_view utf8)
    {
        if (!Usable(selected) || buttons[size_t(selected)].kind != Button::Kind::Edit) return;
        Button& b = buttons[size_t(selected)];
        bool changed = false;
        for (size_t i = 0; i < utf8.size(); i++)
        {
            // UTF-8 to Latin-1, which is what the fonts and the save files hold; anything else is dropped
            unsigned c = static_cast<unsigned char>(utf8[i]);
            if ((c & 0xE0) == 0xC0 && i + 1 < utf8.size())
            {
                c = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu);
                i++;
            }
            else if (c >= 0x80)
            {
                while (i + 1 < utf8.size() && (static_cast<unsigned char>(utf8[i + 1]) & 0xC0) == 0x80) i++;
                continue;
            }
            if (c < 0x20 || c > 0xFF || (c >= 0x7F && c < 0xA0)) continue;
            if (int(b.text.size()) >= b.maxLength) break;
            b.text += char(c);
            changed = true;
        }
        if (changed) TextChanged(size_t(selected));
    }
}
