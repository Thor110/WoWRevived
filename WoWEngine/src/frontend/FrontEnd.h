// The front end: the screens and the command handling that moves between them (WGame's command handler, vanilla
// 0x402B80). Commands are the ids the game's buttons send.
#pragma once
#include "battlemap/BattleMap.h"
#include "frontend/Briefing.h"
#include "frontend/CdPlayer.h"
#include "frontend/Credits.h"
#include "frontend/Menu.h"
#include "frontend/Settings.h"
#include "platform/Music.h"
#include "platform/Sound.h"

#include <map>
#include <memory>

namespace wow
{
    struct FrontEndSettings
    {
        bool networkEnabled = false; // [Main] "Enable Network Version"
        int gameFrequency = 30;      // [Main] "Game Frequency" (dword_4B84C8): ticks a second
        MenuColours colours;
    };

    class WarMap;

    class FrontEnd
    {
    public:
        FrontEnd(Resources& res, FrontEndSettings settings, Settings& ini, const GameFiles& files);

        void Update();
        void Draw(Canvas& canvas) const;
        // The size the picture is to be for what is showing now (docs/MODERN.md "Resolution"): the view size of
        // [Screen] "Size" for the menus and the other 640 x 480 screens, which are centred in it over a background
        // made for the size; 640 x 480 still for the war map, the battle map, their menus and the movies, which
        // the window scales up (the maps' own views come next).
        void ViewSize(int& width, int& height) const;
        // The view size for a desktop of this size (the first start, with no [Screen] "Size" yet): the desktop's
        // own if it is one of the sizes there are backgrounds for; past 1920 x 1080 the one nearest its shape;
        // else the largest that fits, of its shape if there is one
        static void ViewSizeFor(int desktopW, int desktopH, int& width, int& height);
        uint32_t Frames() const { return uint32_t(frames); }    // dword_4D2558
        // byte_4D1279: the [Debug] "Mat" setting's own value, which the curtains cheat also sets (0x401372)
        bool Mat() const { return cheats.mat || ini.GetBool("Debug", "Mat", false); }
        int PlaybackFiles() const { return ini.GetInt("Demo", "Playback Files", 4); }   // dword_4BAAD8
        void OnKey(int sdlKey);
        void OnKeyUp(int sdlKey) { if (screen) screen->OnKeyUp(sdlKey); }
        void OnText(const char* utf8) { if (screen) screen->OnText(utf8); }
        void OnMouseMove(int x, int y);
        void OnMouseLeave() { mouseInside = false; }
        // Draw the game's own mouse cursor (on by default; [WoW Engine] "System Cursor" = 1 uses the system's)
        bool GameCursor() const { return !ini.GetBool("WoW Engine", "System Cursor", false); }
        // [WoW Engine] "Windowed" and "Stretch", which the Modern Options menu can change while playing
        bool WindowedSetting() const { return ini.GetBool("WoW Engine", "Windowed", false); }
        bool StretchSetting() const { return ini.GetBool("WoW Engine", "Stretch", false); }
        void OnMouseButton(int x, int y, bool down);
        void OnRightButton(int x, int y, bool down);
        // The game deactivated (the window lost the focus, or the Pause key, 0x42CD40): no updates come, the music
        // and sounds stop and a movie pauses (0x404BC0); reactivated, they go on (0x404C10).
        void SetPaused(bool paused);

        // Opens a screen by its command id, as if a button had sent it (the --open start option).
        void Open(int command, int param = 0) { Execute({ command, param }); }

        bool QuitRequested() const { return quit; }
        int CurrentScreen() const { return screen ? screen->Id() : 0; }

    private:
        void Execute(const Command& command);
        void Show(std::unique_ptr<Screen> next);

        std::unique_ptr<Menu> NewMenu(int id);
        std::unique_ptr<Menu> MenuOver(int id, int previous, int style); // over the map whose menu opened it
        int BackTo(int menu, int current);
        void ShowMenu(std::unique_ptr<Screen> menu, int back);
        std::map<int, int> menuParents; // each menu's way back, as BackTo keeps it
        std::unique_ptr<Screen> MainMenu();                 // 0x90, built at 0x405ED0
        std::unique_ptr<Screen> GameOptions(int previous);  // 0x8E, 0x408CF0
        std::unique_ptr<Screen> QuitGame(int previous);     // 0x70, 0x409460
        std::unique_ptr<Screen> SoundControl(int previous); // 0x3ED, 0x4057F0
        std::unique_ptr<Screen> DisplaySettings(int previous); // 0x3EF, 0x4096D0
        std::unique_ptr<Screen> CriticalEvents(int previous);
        std::unique_ptr<Screen> ModernOptions(int previous);
        void ApplyModernOptions();  // 0x8117, 0x409DE0
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
        // slot: the campaign's save slot (1-5) it's saved to when left, 0 for none
        void StartWarMap(const std::string& gamePath, const std::string& name, int slot = 0, bool loaded = false);
        // 0x404660: a new campaign's save: the side's start file (Human.000 / Martian.000) under the new name in
        // its slot. False if it couldn't be written.
        bool NewCampaignFile(int slot, const std::string& name);
        // 0x80B4 -> 0x4042D0: the war map written to its campaign's slot ("Saving game to slot %d")
        void SaveCampaign();
        std::filesystem::path SlotFile(int slot) const;
        static bool WriteSaveFile(const std::filesystem::path& path, const std::vector<uint8_t>& data);
        int playingSlot = 0;       // the slot the war map's campaign is saved in (world +0x128)
        std::map<int, ObjOjd::ResearchPageInfo> researchPages; // OBJ.ojd records 63 / 64

        bool AnySaveGame() const;
        void DrawCursor(Canvas& canvas) const;
        int mouseX = -1, mouseY = -1; // where the mouse is, from the showing screen's 0, 0 (it can be off its edges)
        bool mouseInside = false;     // false: outside the picture
        int sizedW = 0, sizedH = 0;   // the view size the resources were last told
        void Origin(int& x, int& y) const;   // the showing screen's 0, 0 in the picture
        mutable int pictureW = 640, pictureH = 480;   // the picture's size, as last drawn
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
        float pendingAmbient = -1.0f, pendingSpeech = -1.0f; // Sound Control's, applied when it closes
        void ApplyVolumes();
        void StartAmbient(bool restart);
        std::unique_ptr<Screen> screen;
        // The Fullscreen View's fades (0x4065E0, bigBlack.SPR, GType 0x5C): a command from one of its controls
        // starts the fade-out (50% black, then black a tick later) and arrives 5 ticks later (0x811B); a new view
        // fades in (black, then 50%, then clear: 0x811D). overlay: 0 none, 1 half, 2 black.
        void Dispatch(const Command& command, bool fromControl);
        bool Fades(const Screen& s) const;
        std::optional<Command> delayed;
        int delayTicks = 0;
        // the cursor's sprite and its frame, 8.8 (0x40CD30 / 0x40CDC0): half a frame a tick, round again at the end
        mutable int cursorGtype = 0, cursorFrame = 0;
        int frames = 0;          // dword_4D2558: the game's frame counter (0x403749), carried from screen to screen
        int fadeIn = 0;
        int overlay = 0;
        std::unique_ptr<Screen> warMap;   // kept while its menus are showing
        std::unique_ptr<Screen> battleMap; // kept while its menu is showing
        std::unique_ptr<Screen> BattleMapMenu();
        WarMap* ActiveWarMap() const;
        // tool tips (0x433870)
        void UpdateTooltip();
        void DrawTooltip(Canvas& canvas) const;
        struct Tip { std::string text; int x, y; };
        std::optional<Tip> tip;
        int stillTicks = 0;        // updates since the mouse last moved
        int hoverOwner = -2;       // dword_4EA554: the part of the screen the mouse was last over
        int hoverX = 0, hoverY = 0; // dword_4EA548 / 4EA54C: where the hovering started
        bool mouseHeld = false, rightHeld = false;
        bool musicWasPaused = false;
        // The war map's music (0x41B070, command 0x813C): CD tracks 2 and 3 in turn, or without CD music one of
        // the ambient sounds 0xFD-0xFF every 10 seconds; and its clock sound (0x41F860)
        void WarMapSounds(WarMap& map);
        void MusicStep();
        void StopWarMapSounds();
        // a battle: the briefing transition (0x41B210) and its sound (dword_4E9B84); the war map's music and
        // clock wait while the Briefing and Debriefing are up
        void BriefingTransition();
        BattleColours BattleColoursFromIni() const;
        int briefingSound = 0;
        std::map<int, std::string> sectorMaps;                  // OBJ.ojd record 8
        std::map<int, ObjOjd::BattleTables::DayTime> dayTimes;   // OBJ.ojd record 9
        std::map<int, ObjOjd::BattleTables::Six> wofTypes;       // OBJ.ojd record 6
        std::map<int, ObjOjd::BattleTables::Thirteen> lightTypes; // OBJ.ojd record 13
        std::map<int, std::array<int, 17>> moveCosts;             // OBJ.ojd record 10
        std::map<int, std::pair<int, float>> moveSlopes;          // OBJ.ojd record 11
        std::map<int, std::array<int, 8>> weaponData;             // OBJ.ojd record 12
        int lastEnemyCount = 0;                                 // dword_4F7430 from the battle before
        bool StartBattleMap();
        // The battle map is built behind the Briefing (from its second update, once its first picture is up), so
        // leaving the Briefing goes straight to it; StartBattleMap builds it then if that hasn't happened.
        std::unique_ptr<BattleMap> BuildBattleMap();
        std::unique_ptr<BattleMap> pendingBattle;
        int briefingTicks = 0;
        bool inBattle = false;
        int musicTimer = -1;       // ticks until the next music step, -1 with no war map
        int musicTrack = 1;        // dword_4B9DE0: 0 for track 2, 1 for track 3 (the first step goes to track 2)
        // the battle map's music (0x4861E0, its 0x813C): CD tracks 4 and 5 in turn (dword_4D0118), from 4
        int battleMusicTimer = -1; // ticks until its next step, -1 off the battle map
        int battleTrack = 1;       // dword_4D0124
        void BattleMusicStep();
        int warAmbient = 0;        // world +0x12C
        int infoSpeech = 0;         // the war map's information page's speech
        int clockSound = 0;        // the calendar's +0x34
        int clockSpeed = -1;       // the speed it was last set for
        int loadedGroup = -1;
        bool quit = false;
        // typed cheat codes (0x401000): "englishrose" shows the hidden end of the credits
        size_t cheatProgress = 0;
        int cheatCode = 0;                  // dword_4D1130: 0 none yet, 1-8 the code being typed
        // the cheats' flags (reset by 0x4011D0 as a war map is made or loaded)
        struct Cheats
        {
            bool used = false;          // byte_4D1138: a cheat was typed
            bool resources = false;     // byte_58913C, youlikeit: every product of supply ratios taken as 1
            bool shym = false;          // byte_4D25A0: the next battle's Damage Layer cleared first
            bool noFog = false;         // byte_4BAD38 0, datsun: no fog of war
            bool mat = false;           // byte_4D1279, curtains: the [Debug] "Mat" flag (what it does isn't traced)
        } cheats;
        void ResetCheats();             // 0x4011D0
        void CheatTyped(int code);
        bool creditsEasterEgg = false;
    };
}
