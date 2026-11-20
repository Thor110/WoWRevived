// The front end: the screens and the command handling that moves between them (WGame's command handler, vanilla
// 0x402B80). Commands are the ids the game's buttons send.
#pragma once
#include "frontend/CdPlayer.h"
#include "frontend/Credits.h"
#include "frontend/Menu.h"
#include "frontend/Settings.h"
#include "platform/Music.h"
#include "platform/Sound.h"

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
        void OnKeyUp(int sdlKey) { if (screen) screen->OnKeyUp(sdlKey); }
        void OnText(const char* utf8) { if (screen) screen->OnText(utf8); }
        void OnMouseMove(int x, int y);
        void OnMouseLeave() { mouseX = mouseY = -1; }
        // Draw the game's own mouse cursor (on by default; [WoW Engine] "System Cursor" = 1 uses the system's)
        bool GameCursor() const { return !ini.GetBool("WoW Engine", "System Cursor", false); }
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
        std::unique_ptr<Screen> SaveGame(int previous);     // 0x6E, 0x4090A0 (also "Select Campaign to Replace")
        std::unique_ptr<Screen> SinglePlayer(int previous); // 0xA0, 0x4068F0
        std::unique_ptr<Screen> InsertCd(int next);          // 0x8133, 0x4089D0
        std::unique_ptr<Screen> ReplaceCampaign(int previous); // 0x70, 0x4091E0
        std::unique_ptr<Screen> EditCampaign(int previous);  // 0x8139, 0x4092D0
        std::unique_ptr<Screen> ServiceProvider(int previous, int next); // 0xA4, 0x406C10
        std::unique_ptr<Screen> NetworkHost(int previous);   // 0x9F, 0x406A10
        std::unique_ptr<Screen> WaitingForPlayer(int previous); // 0xAC, 0x406B30
        std::unique_ptr<Screen> NetworkSessions(int previous); // 0xA4 (opened by 0xA6), 0x407110
        std::unique_ptr<Screen> NotRebuilt(int command, int previous);
        void PlayMovie(const std::string& name, Command next);
        std::unique_ptr<Screen> WarMapMenu();                // 0x80B5, 0x49B980
        void StartWarMap(const std::string& gamePath, const std::string& name);

        bool AnySaveGame() const;
        void DrawCursor(Canvas& canvas) const;
        int mouseX = -1, mouseY = -1; // -1: outside the picture
        bool CorrectCdInserted() const;

        // what the game keeps in globals between menus
        int topMenu = 0x90;        // dword_4D2574: the menu "Start New Campaign" was chosen from
        int campaignSlot = 0;      // dword_4D2578: the save slot the new campaign goes in
        std::string campaignName;  // byte_4D1448: its name, up to 36 characters
        int insertCdNext = 0;      // the command the Insert CD menu sends once the disc is there
        bool networkLoad = false;  // byte_4D256C: the load menu was opened for a network game

        Resources& res;
        FrontEndSettings settings;
        Settings& ini;
        const GameFiles& files;
        Music music;
        Sound sound;
        int ambient = 0;           // dword_4D25D0: the menus' ambient sound 0x18C
        void ApplyVolumes();
        void StartAmbient(bool restart);
        std::unique_ptr<Screen> screen;
        std::unique_ptr<Screen> warMap;   // kept while its menus are showing
        int loadedGroup = -1;
        bool quit = false;
        // typed cheat codes (0x401000): "englishrose" shows the hidden end of the credits
        size_t cheatProgress = 0;
        bool creditsEasterEgg = false;
    };
}
