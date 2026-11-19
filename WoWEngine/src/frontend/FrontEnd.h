// The front end: the screens and the command handling that moves between them (WGame's command handler, vanilla
// 0x402B80). Commands are the ids the game's buttons send.
#pragma once
#include "frontend/CdPlayer.h"
#include "frontend/Menu.h"
#include "frontend/Settings.h"
#include "platform/Music.h"

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
        FrontEnd(Resources& res, FrontEndSettings settings, Settings& ini, const GameFiles& files);

        void Update();
        void Draw(Canvas& canvas) const;
        void OnKey(int sdlKey);
        void OnMouseMove(int x, int y);
        void OnMouseButton(int x, int y, bool down);

        bool QuitRequested() const { return quit; }
        int CurrentScreen() const { return screen ? screen->Id() : 0; }

    private:
        void Execute(const Command& command);
        void Show(std::unique_ptr<Screen> next);

        std::unique_ptr<Menu> NewMenu(int id);
        std::unique_ptr<Screen> MainMenu();                 // 0x90, built at 0x405ED0
        std::unique_ptr<Screen> GameOptions(int previous);  // 0x8E, 0x408CF0
        std::unique_ptr<Screen> QuitGame(int previous);     // 0x70, 0x409460
        std::unique_ptr<Screen> SoundControl(int previous); // 0x3ED, 0x4057F0
        std::unique_ptr<Screen> DisplaySettings(int previous); // 0x3EF, 0x4096D0
        std::unique_ptr<Screen> CriticalEvents(int previous);  // 0x8117, 0x409DE0
        std::unique_ptr<Screen> NotRebuilt(int command, int previous);

        bool AnySaveGame() const;

        Resources& res;
        FrontEndSettings settings;
        Settings& ini;
        const GameFiles& files;
        Music music;
        std::unique_ptr<Screen> screen;
        int loadedGroup = -1;
        bool quit = false;
    };
}
