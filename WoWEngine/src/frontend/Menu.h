// The stacked full-screen menus (WFullScreenMenu, constructor 0x40A060).
//
// A menu is a heading and push buttons stacked from y = 160 in the order they're added, each taking its plate's
// height. Adding the bottom button (0x40AB00) then centres the whole stack on the screen. The graphics depend on
// the menu's style (the game's +0x94):
//                    heading       button        bottom button
//   style 0          0x3E / 0x3F   0x38 / 0x39   0x38 / 0x3A
//   style 1          0x3E / 0x3F   0x43 / 0x44   0x38 / 0x3A
//   style 2          0x40 / 0x41   0x3B / 0x3C   0x3B / 0x3D
// (plate / glass). The heading's text uses font 0x6D and buttons font 0x73, at the [Colours] levels
// "Menu Heading" (default 7) and "Menu Items" (default 4).
#pragma once
#include "frontend/Screen.h"

#include <functional>
#include <map>

namespace wow
{
    struct MenuColours
    {
        int heading = 7;      // [Colours] "Menu Heading"
        int items = 4;        // [Colours] "Menu Items"
        int staticText = 4;   // [Colours] "Menu Static Text"
        int lineSpacing = 4;  // [Tweak] "Text Line Spacing", for free-standing text
    };

    class Menu : public Screen
    {
    public:
        Menu(int id, Resources& res, MenuColours colours, int background, int paletteGroup, int style = 0)
            : Screen(id, res, background, paletteGroup), style(style), colours(colours) {}

        void AddHeading(int textId) { AddHeadingText(res.String(textId)); }
        void AddHeadingText(std::string text);
        void AddButton(int command, int textId, int param = 0);
        void AddButtonText(int command, std::string text, int param = 0); // a caption that isn't a TEXT.ojd string
        void AddBottomButton(int command, int textId); // also centres the menu

        // Rows that change a value; changed is called with the control after every change.
        using OnChange = std::function<void(Button&)>;
        void AddCheck(int textId, bool value, OnChange changed);                 // 0x40A320
        void AddRadio(int textId, int group, bool value, OnChange changed);      // 0x40A630
        // label turns the value into the row's text ("Master Volume - 80%")
        void AddSlider(int value, int max, std::function<std::string(int)> label, OnChange changed); // 0x40AC80

        // The critical event menu's rows (0x40A480, style 2): two columns either side of the screen's centre,
        // the label left-aligned 37 pixels in (0x4B8E00 + 0x4B8E04 + 0x4B8E08). NextColumn starts the second.
        void AddEventCheck(int textId, bool value, OnChange changed);
        void NextColumn();

    protected:
        void Changed(size_t index) override;

    private:
        void Stack(Button button);
        void Centre();

        int style;
        MenuColours colours;
        int nextY = 0;
        int column = -1, columnTop = 0, columnBottom = 0;
        std::map<size_t, OnChange> onChange;
        std::map<size_t, std::function<std::string(int)>> labels;
    };
}
