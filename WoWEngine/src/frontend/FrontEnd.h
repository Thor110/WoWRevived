// The front end: the screens and the command handling that moves between them (WGame's command handler, vanilla
// 0x402B80). Commands are the ids the game's buttons send.
#pragma once
#include "frontend/CdPlayer.h"
#include "frontend/Credits.h"
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

        // Opens a screen by its command id, as if a button had sent it (the --open start option).
        void Open(int command) { Execute({ command, 0 }); }

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
        std::unique_ptr<Screen> LoadGame(int previous);     // 0x6F, 0x409560
        std::unique_ptr<Screen> SaveGame(int previous);     // 0x6E, 0x4090A0 (opened from the in-game menu)
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
        // typed cheat codes (0x401000): "englishrose" shows the hidden end of the credits
        size_t cheatProgress = 0;
        bool creditsEasterEgg = false;
    };
}
