// The front end: the background and the full-screen menus, and the command handling that moves between them
// (WGame's command handler, vanilla 0x402B80). Commands are the ids the game's buttons send.
#pragma once
#include "frontend/Menu.h"

#include <memory>

namespace wow
{
    struct FrontEndSettings
    {
        bool networkEnabled = false; // [Main] "Enable Network Version"
        MenuColours colours;
    };

    class FrontEnd
    {
    public:
        FrontEnd(Resources& res, FrontEndSettings settings, const GameFiles& files);

        void Draw(Canvas& canvas) const;
        void OnKey(int sdlKey);
        void OnMouseMove(int x, int y);
        void OnMouseButton(int x, int y, bool down);

        bool QuitRequested() const { return quit; }

    private:
        void Execute(int command);
        void Show(std::unique_ptr<Menu> menu);

        std::unique_ptr<Menu> MainMenu();                 // 0x90, built at 0x405ED0
        std::unique_ptr<Menu> GameOptions(int previous);  // 0x8E, 0x408CF0
        std::unique_ptr<Menu> QuitGame(int previous);     // 0x70, 0x409460
        std::unique_ptr<Menu> NotRebuilt(int command);

        bool AnySaveGame() const;

        Resources& res;
        FrontEndSettings settings;
        const GameFiles& files;
        std::unique_ptr<Menu> menu;
        bool quit = false;
    };
}
