// A few Windows-style controls for WoWLauncher, drawn into a frame buffer so the installer looks like
// WoWLauncher (and the original game's launcher) on every platform: light grey dialog, plain push buttons,
// checkboxes and a progress bar, with DejaVu Sans baked in (UiFont.h) in place of the Windows dialog font.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wow::ui
{
    struct Box
    {
        int x = 0, y = 0, w = 0, h = 0;
        bool Contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    };

    namespace Colour
    {
        constexpr uint32_t Dialog = 0xF0F0F0;
        constexpr uint32_t Text = 0x000000;
        constexpr uint32_t GreyText = 0x6D6D6D;
        constexpr uint32_t DisabledText = 0xA0A0A0;
        constexpr uint32_t ButtonFace = 0xE1E1E1, ButtonBorder = 0xADADAD;
        constexpr uint32_t HotFace = 0xE5F1FB, HotBorder = 0x0078D7;
        constexpr uint32_t PressedFace = 0xCCE4F7, PressedBorder = 0x005499;
        constexpr uint32_t DisabledFace = 0xCCCCCC, DisabledBorder = 0xBFBFBF;
        constexpr uint32_t ProgressBack = 0xE6E6E6, ProgressFill = 0x06B025, ProgressBorder = 0xBCBCBC;
        constexpr uint32_t Warning = 0xA01010;
    }

    class Painter
    {
    public:
        Painter(uint32_t* pixels, int width, int height) : pixels(pixels), width(width), height(height) {}

        void Fill(const Box& b, uint32_t colour);
        void Frame(const Box& b, uint32_t colour);
        void Image(const uint32_t* source, int w, int h, int x, int y);

        // Latin-1 text. Returns the width drawn.
        int Text(std::string_view latin1, int x, int y, uint32_t colour);
        static int Measure(std::string_view latin1);
        void TextCentred(std::string_view latin1, const Box& b, uint32_t colour);
        // Shortens text that doesn't fit into width with "...".
        static std::string Fit(std::string_view latin1, int width);
        // Breaks text into lines no wider than width, at spaces.
        static std::vector<std::string> Wrap(std::string_view latin1, int width);

        enum class State { Normal, Hot, Pressed, Disabled };
        void Button(const Box& b, std::string_view label, State state, bool isDefault);
        void CheckBox(const Box& b, std::string_view label, bool checked, State state);
        void Progress(const Box& b, double fraction);

    private:
        void Blend(int x, int y, uint32_t colour, uint8_t alpha);

        uint32_t* pixels;
        int width, height;
    };

    // UTF-8 (paths from the system, SDL's dialogs) to Latin-1 for drawing; other characters become '?'.
    std::string ToLatin1(std::string_view utf8);
}
