#include "frontend/Screen.h"

#include <SDL3/SDL_keyboard.h>
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
            // 0x432080: (the frame count / 2 - 1) x the position, divided by the maximum, truncating (0x4320B6 div)
            int position = b.max > 0 ? b.value * 10 / b.max : 0;
            plateFrame = position + (lit ? 11 : 0);
            glassFrame = lit ? 1 : 0;
            break;
        }
        }
        if (b.glass && b.widen > b.rect.w)
            canvas.DrawSpriteWidened(b.glass, glassFrame, b.rect.x - (b.widen - b.rect.w) / 2, b.rect.y, b.widen, Canvas::Blend::Translucent);
        else if (b.glass) canvas.DrawSprite(b.glass, glassFrame, b.rect.x, b.rect.y, 0, Canvas::Blend::Translucent);
        if (b.plate) canvas.DrawSprite(b.plate, plateFrame, b.rect.x, b.rect.y);
        if (b.kind == Button::Kind::Edit)
        {
            // the text being typed, and while the field has the focus the caret (0x4312D0: caret8.spr, GType 0x52,
            // going round its frames at 0x33 / 256 of a frame a draw) at the caret's place in the text
            canvas.DrawString(b.font, b.text, b.rect, TextCentreX | TextCentreY, b.level);
            if (!lit) return;
            const Font* font = res.GetFont(b.font);
            int frames = 0;
            while (frames < 64 && res.GetFrame(0x52, frames)) frames++;
            caretAnimation += 0x33;
            if (frames > 0 && caretAnimation >= frames * 256) caretAnimation -= frames * 256;
            int x = TextLeft(b) + (font ? font->Measure(std::string_view(b.text).substr(0, size_t(std::clamp(b.caret, 0, int(b.text.size()))))) : 0);
            int y = b.rect.y + (b.rect.h - (font ? font->Height() : 0)) / 2;
            canvas.DrawSprite(0x52, frames > 0 ? caretAnimation / 256 : 0, x, y);
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

    std::string Screen::TipText(int owner, int) const
    {
        if (owner < 0 || size_t(owner) >= buttons.size() || !buttons[size_t(owner)].tooltip) return {};
        return res.String(buttons[size_t(owner)].tooltip);
    }

    int Screen::TextLeft(const Button& b) const
    {
        const Font* font = res.GetFont(b.font);
        int w = font ? font->Measure(b.text) : 0;
        return b.rect.x + (b.rect.w - w) / 2;
    }

    // A click in a text field (0x4313F0): left of the text does nothing; otherwise the caret goes after the first
    // character whose right edge is past the click (never before the first), or, past them all, before the last
    void Screen::CaretFromX(Button& b, int x)
    {
        const Font* font = res.GetFont(b.font);
        int left = TextLeft(b);
        if (!font || x < left) return;
        int n = int(b.text.size());
        for (int k = 1; k < n; k++)
            if (x < left + font->Measure(std::string_view(b.text).substr(0, size_t(k)))) { b.caret = k; return; }
        if (n > 1) b.caret = n - 1;
    }

    // A text field's keys (0x431730): Up goes to the control before (0x8008), Down and Enter to the one after
    // (0x8007); Delete takes the character after the caret, Backspace the one before it; Home and End; Left and
    // Right a character, or with Ctrl a word (isspace). True if the key was the field's.
    bool Screen::EditKey(Button& b, int key)
    {
        const bool ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
        const int n = int(b.text.size());
        auto space = [&](int i) { return i < n && (b.text[size_t(i)] == ' ' || (b.text[size_t(i)] >= 9 && b.text[size_t(i)] <= 13)); };
        int& c = b.caret;
        c = std::clamp(c, 0, n);
        switch (key)
        {
        case SDLK_UP: selected = NextUsable(selected, -1); return true;
        case SDLK_DOWN: case SDLK_RETURN: case SDLK_KP_ENTER: selected = NextUsable(selected, 1); return true;
        case SDLK_DELETE:
            if (c < n) { b.text.erase(size_t(c), 1); TextChanged(size_t(selected)); }
            return true;
        case SDLK_BACKSPACE:
            if (c > 0) { b.text.erase(size_t(c - 1), 1); c--; TextChanged(size_t(selected)); }
            return true;
        case SDLK_HOME: c = 0; return true;
        case SDLK_END: c = n; return true;
        case SDLK_LEFT:
            if (!ctrl) { if (c > 0) c--; return true; }
            // back over spaces, then over the word, then forward onto it
            if (c > 0) c--;
            while (c > 0 && space(c)) c--;
            while (c > 0 && !space(c)) c--;
            if (c > 0 && space(c)) c++;
            return true;
        case SDLK_RIGHT:
            if (!ctrl) { if (c < n) c++; return true; }
            while (c < n && !space(c)) c++;
            while (c < n && space(c)) c++;
            return true;
        default: return false;
        }
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
        // the Scrollbar's position (0x431EF0): (max + 1) x (x - left margin) / (length - both margins), no rounding;
        // the options rows (0x40AC80) give margins of 21 (0x15) each side
        const Button& b = buttons[size_t(index)];
        constexpr int Margin = 0x15;
        int span = std::max(1, b.rect.w - 2 * Margin);
        int position = (b.max + 1) * (x - b.rect.x - Margin) / span;
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
            // a text field takes the typing (OnText) and its own keys
            if (EditKey(buttons[size_t(selected)], key)) return std::nullopt;
            if (key == SDLK_SPACE) return std::nullopt;
        }
        if (slider && (key == SDLK_LEFT || key == SDLK_RIGHT))
            SetValue(selected, buttons[size_t(selected)].value + (key == SDLK_RIGHT ? 1 : -1));
        else if (key == SDLK_UP || key == SDLK_LEFT || (key == SDLK_TAB && (SDL_GetModState() & SDL_KMOD_SHIFT)))
            selected = NextUsable(std::max(selected, 0), -1); // Shift+Tab: 0x8008 (WControl 0x433F20)
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
            // a click in a text field gives it the focus and places the caret (0x431500)
            if (hit >= 0 && buttons[size_t(hit)].kind == Button::Kind::Edit) { selected = hit; CaretFromX(buttons[size_t(hit)], x); }
            return std::nullopt;
        }
        // like a Windows button: it acts when released over the control that was pressed
        int was = pressed;
        pressed = -1;
        if (hit >= 0 && hit == was && buttons[size_t(hit)].kind != Button::Kind::Edit) return Activate(hit);
        return std::nullopt;
    }

    std::optional<Command> Screen::OnRightButton(int x, int y, bool down)
    {
        int hit = HitTest(x, y);
        // push buttons (0x42FEF0's jump table 0x4301A0) and radio buttons (0x430E90, 0x430F07) take 0x204 / 0x205 as
        // 0x201 / 0x202. (*Provisional*: checkboxes inherit the push base, but that isn't traced, so they don't.)
        bool push = hit >= 0 && (buttons[size_t(hit)].kind == Button::Kind::Push || buttons[size_t(hit)].kind == Button::Kind::Graphic ||
                                 buttons[size_t(hit)].kind == Button::Kind::Radio);
        if (down) { if (push) pressed = hit; return std::nullopt; }
        int was = pressed;
        pressed = -1;
        if (push && hit == was) return Activate(hit);
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
            // WM_CHAR (0x431500) takes isalnum (ASCII) and space ! " # $ & ' ( ) * . : ; < = > ? ^ _
            static constexpr std::string_view Allowed = " !\"#$&'()*.:;<=>?^_";
            bool alnum = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            if (!alnum && (c > 0x7F || Allowed.find(char(c)) == std::string_view::npos)) continue;
            // inserted at the caret, the text then cut to the field's length (0x431C30); at or past the length
            // it's refused with sound 0x192
            int at = std::clamp(b.caret, 0, int(b.text.size()));
            if (at >= b.maxLength) { sounds.push_back(0x192); continue; }
            b.text.insert(size_t(at), 1, char(c));
            if (int(b.text.size()) > b.maxLength) b.text.resize(size_t(b.maxLength));
            b.caret = at + 1;
            changed = true;
        }
        if (changed) TextChanged(size_t(selected));
    }
}
