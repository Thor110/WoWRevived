#include "frontend/FrontEnd.h"

#include "core/AiTable.h"
#include "core/Random.h"
#include "frontend/MovieScreen.h"
#include "frontend/Splash.h"
#include "core/SaveGame.h"
#include "frontend/ResearchScreen.h"
#include "warmap/WarMap.h"
#include "battlemap/BattleMap.h"
#include "core/ObjOjd.h"

#include <fstream>
#include <iterator>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace wow
{
    namespace
    {
        // Command ids sent by the front-end buttons
        constexpr int CmdMainMenu = 0x90;
        constexpr int CmdOptions = 0x8E;
        constexpr int CmdQuitMenu = 0x80B6;
        constexpr int CmdQuitGame = 0x70;
        constexpr int CmdLoadGame = 0x6F;
        constexpr int CmdNewCampaign = 0x8138;
        constexpr int CmdSinglePlayer = 0xA0;     // "Single Player Campaign" (network version's main menu)
        constexpr int CmdNetwork = 0xA4;          // "Network Campaign" / "Join Network Campaign": service providers
        constexpr int CmdNetworkHost = 0x9F;      // Human games host ...
        constexpr int CmdJoinNetwork = 0xA6;      // ... Martian games join
        constexpr int CmdNewNetworkGame = 0xA1;   // "Start New Network Game": waiting for the other player
        constexpr int CmdLoadNetworkGame = 0xA2;  // "Load Network Campaign"
        constexpr int CmdStartNetworkGame = 0xA5; // a session picked (also 0xAB)
        constexpr int CmdWaitingForPlayer = 0xAC;
        constexpr int CmdInsertCd = 0x8133;       // param: the command to send once the right disc is in
        constexpr int CmdInsertCdProceed = 0x8024;
        constexpr int CmdReplaceCampaign = 0x8140; // also "no campaign in that slot": on to naming it
        constexpr int CmdEditCampaign = 0x8139;   // "Begin Campaign"
        constexpr int CmdIntro = 0x8037;          // plays INTRO, then 0x8A
        constexpr int CmdRageLogo = 0x8036;       // plays RAGELOGO, then 0x8038 (start-up, after the disc check)
        constexpr int CmdTitle = 0x8038;          // plays TITLE, then the main menu
        constexpr int CmdStartCampaign = 0x8A;    // the war map
        constexpr int CmdGameOver = 0x80FE;       // the war map's end (lParam the ending, 0x4212A0)
        constexpr int CmdWarMapMenu = 0x80B5;     // the war map's Escape menu
        constexpr int CmdSoundControl = 0x3ED;
        constexpr int CmdDisplaySettings = 0x3EF;
        constexpr int CmdCriticalEvents = 0x8117;
        constexpr int CmdSaveMenu = 0x6E;
        constexpr int CmdLoadSlot = 0x80B3;  // + slot: load that save (starts the campaign)
        constexpr int CmdSaveSlot = 0x8142;  // + slot: save the campaign there

        constexpr const char* SoundSection = "Sound\\Volume";

        // The screen modes Display Settings offers: WoWRevived's list (WoWLauncher's supportedResolutions), which
        // goes up to 1920 x 1080 because the extended war map background is only that big. Bigger displays get the
        // picture scaled up. (The original's own table at 0x4B8CA0 is 512 x 384 to 1280 x 1024.)
        struct Mode { int w, h; };
        constexpr Mode Modes[] = {
            { 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1152, 864 }, { 1280, 768 }, { 1280, 800 }, { 1280, 1024 },
            { 1360, 768 }, { 1366, 768 }, { 1600, 900 }, { 1600, 1024 }, { 1680, 1050 }, { 1920, 1080 },
        };

        // The critical event options, in menu order: TEXT.ojd string, [Event] setting, default (0x41E160...)
        struct EventOption { int text; const char* name; bool on; };
        constexpr EventOption Events[] = {
            { 0x413, "Construction Completed", false }, { 0x414, "Unit Manufactured", false },
            { 0x415, "Command Centre Built", false },   { 0x416, "Constructor Idle", true },
            { 0x417, "Facility Idle", true },           { 0x418, "Unit Arrived At Destination", false },
            { 0x419, "Travel Interrupted", true },      { 0x41A, "Research Completed", true },
            { 0x41B, "Resource Warning", false },       { 0x41C, "Resource Alert", true },
        };

        std::string Format(const std::string& text, const char* format, int a, int b = 0)
        {
            char buffer[160];
            std::snprintf(buffer, sizeof(buffer), format, text.c_str(), a, b);
            return buffer;
        }

        // The menus' "Fullscreen View" (0x4061B0): GType 0x36F (humanbd.spr or martbd.spr) with palette group
        // 0x2F6 (F6) in Human games or 0x2F7 (F7) in Martian games.
        constexpr int MenuBackdrop = 0x36F;
    }

    FrontEnd::FrontEnd(Resources& r, FrontEndSettings s, Settings& i, const GameFiles& f)
        : res(r), settings(s), ini(i), files(f), sound(f, r.Human())
    {
        ApplyVolumes();
        // start-up (WGame::Start 0x402A30): with "Skip" ([Main], byte_4D1361) set, straight to Start New Campaign;
        // otherwise 0x74, the legal splash screens, then the disc check, RAGELOGO, TITLE and the main menu
        if (ini.GetBool("Main", "Skip", false)) Execute({ CmdNewCampaign, 0 });
        else Show(std::make_unique<SplashScreen>(res, Command{ CmdInsertCd, CmdRageLogo }));
    }

    void FrontEnd::ApplyVolumes()
    {
        // [Sound\Volume], 0-1 (the settings at 0x4CA858-0x4CA870, one per sound type, and Master over them all)
        static const char* const Types[6] = { "Ambient", "Engine", "Explosions", "Weapons", "Special", "Speech" };
        float types[6];
        for (int t = 0; t < 6; t++) types[t] = ini.GetFloat(SoundSection, Types[t], 1.0f);
        float master = ini.GetFloat(SoundSection, "Master", 1.0f);
        sound.SetVolumes(master, types);
        music.SetVolume(master);
    }

    void FrontEnd::StartAmbient(bool restart)
    {
        // The main menu (0x405ED0) and Game Options (0x406130) start the menu sound only when it isn't playing (the
        // handle released once it has played to its end, 0x464E20), fading it in over 60 ticks; while it plays,
        // going from menu to menu leaves it be.
        if (!restart && sound.Active(ambient)) return;
        sound.Stop(ambient);
        ambient = sound.Play(0x18C);
        sound.FadeIn(ambient, 60);
    }

    void FrontEnd::Show(std::unique_ptr<Screen> next)
    {
        int from = screen ? screen->Id() : 0;
        SDL_Log("Screen 0x%X", next->Id());
        // Leaving the CD Player stops its music. The original doesn't (its CD player has no destructor of its own,
        // so the disc played on until something else used the drive); [WoW Engine] "CD Player Music Continues"
        // = 1 keeps that.
        if (from == CdPlayer::Id && next->Id() != CdPlayer::Id && !ini.GetBool("WoW Engine", "CD Player Music Continues", false))
            music.Stop();
        if (from == CmdSoundControl && (pendingAmbient >= 0 || pendingSpeech >= 0))
        {
            if (pendingAmbient >= 0) ini.SetFloat(SoundSection, "Ambient", pendingAmbient);
            if (pendingSpeech >= 0) ini.SetFloat(SoundSection, "Speech", pendingSpeech);
            pendingAmbient = pendingSpeech = -1.0f;
            ApplyVolumes();
        }
        // a tip belongs to the screen it was asked of
        tip.reset();
        hoverOwner = -2;
        screen = std::move(next);
        sound.SetMenuDuck(screen->Id() == CmdWarMapMenu || screen->Id() == BattleMap::MenuCommand);
        delayed.reset();
        overlay = 0;
        fadeIn = 0;
        // a Fullscreen View fades in from black (0x406310 sends 0x811D), except Display Settings (+0x94, 0x409713)
        if (Fades(*screen) && screen->Id() != CmdDisplaySettings) { fadeIn = 2; overlay = 2; }
        // the "not rebuilt" placeholder returns to the menu it was opened from
        if (!screen->Previous() && screen->Id() != CmdMainMenu)
            screen->SetPrevious(from == CmdOptions || from == CmdLoadGame || from == CmdSaveMenu ? from : CmdMainMenu);
        if (screen->PaletteGroup() != loadedGroup)
        {
            res.LoadShadeGroup(screen->PaletteGroup());
            loadedGroup = screen->PaletteGroup();
        }
    }

    // The menu a menu's "Previous Menu" goes back to: the one it was opened from, kept when it's opened again on
    // the way back from one of its own (Options from Sound Control, say), so going back never loops between them.
    int FrontEnd::BackTo(int menu, int current)
    {
        if (screen && screen->Previous() == menu)
        {
            auto kept = menuParents.find(menu);
            if (kept != menuParents.end()) return kept->second;
        }
        menuParents[menu] = current;
        return current;
    }

    void FrontEnd::ShowMenu(std::unique_ptr<Screen> menu, int back)
    {
        menu->SetPrevious(back);
        Show(std::move(menu));
    }

    std::unique_ptr<Menu> FrontEnd::NewMenu(int id)
    {
        return std::make_unique<Menu>(id, res, settings.colours, MenuBackdrop, res.Human() ? 0x2F6 : 0x2F7);
    }

    bool FrontEnd::AnySaveGame() const
    {
        // the same check as the main menu's and load menu's constructors
        for (int slot = SaveSlot::First; slot <= SaveSlot::Last; slot++)
            if (SaveSlot::Exists(files, res.Human(), slot)) return true;
        return false;
    }

    std::unique_ptr<Screen> FrontEnd::LoadGame(int previous)
    {
        auto m = std::make_unique<Menu>(CmdLoadGame, res, settings.colours, MenuBackdrop, res.Human() ? 0x2F6 : 0x2F7, 2);
        m->AddHeading(AnySaveGame() ? 0x406 : 0x434); // "Resume Campaign" or "No Saved Campaigns"
        for (int slot = SaveSlot::First; slot <= SaveSlot::Last; slot++)
            if (auto name = SaveSlot::Name(files, res.Human(), slot)) m->AddButtonText(CmdLoadSlot, *name, slot);
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x40F);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::SaveGame(int previous)
    {
        auto m = std::make_unique<Menu>(CmdSaveMenu, res, settings.colours, MenuBackdrop, res.Human() ? 0x2F6 : 0x2F7, 2);
        m->AddHeading(0x52D);
        for (int slot = SaveSlot::First; slot <= SaveSlot::Last; slot++)
        {
            auto name = SaveSlot::Name(files, res.Human(), slot);
            m->AddButtonText(CmdSaveSlot, name ? *name : res.String(0x408), slot); // "Empty"
        }
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x40F);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::MainMenu()
    {
        auto m = NewMenu(CmdMainMenu);
        StartAmbient(false);
        m->AddHeading(0x3FB);
        if (settings.networkEnabled)
        {
            m->AddButton(CmdSinglePlayer, 0x3FE);
            m->AddButton(CmdNetwork, res.Human() ? 0x3FF : 0x400);
        }
        else if (AnySaveGame())
        {
            m->AddButton(CmdLoadGame, 0x406);
            m->AddButton(CmdNewCampaign, 0x40C);
        }
        else m->AddButton(CmdNewCampaign, 0x40C);
        m->AddButton(CmdOptions, 0x401);
        m->AddButton(CdPlayer::Id, 0x402);
        m->AddBottomButton(CmdQuitMenu, 0x403);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::GameOptions(int previous)
    {
        auto m = NewMenu(CmdOptions);
        StartAmbient(false);
        m->AddHeading(0x401);
        m->AddButton(0x3EF, 0x410);   // Display Settings
        m->AddButton(0x3ED, 0x411);   // Sound Control
        m->AddButton(0x8117, 0x412);  // Critical event options
        m->AddButton(0x80CA, 0x534);  // Credits
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x40F);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::QuitGame(int previous)
    {
        auto m = NewMenu(CmdQuitGame);
        m->AddHeading(previous == CmdMainMenu ? 0x403 : 0x404);
        m->AddButton(CmdQuitGame, 0x445);
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x446);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::SoundControl(int previous)
    {
        auto m = NewMenu(CmdSoundControl);
        m->AddHeading(0x411);
        // the menu works in tenths (0x4B8A7C-0x4B8A84); the settings hold 0-1 like the game's. Master applies at
        // once (the 0x812B timer, 0x4644A0); Ambient (sound type 0) and Speech (type 5) when the menu closes
        // (0x4059A0, 0x464490)
        auto volume = [&](int textId, const char* name, float* pending) {
            int tenths = int(std::lround(std::clamp(ini.GetFloat(SoundSection, name, 1.0f), 0.0f, 1.0f) * 10));
            std::string text = res.String(textId);
            m->AddSlider(tenths, 10, [text](int v) { return Format(text, "%s - %d%%", v * 10); },
                [this, name, pending](Button& b) {
                    if (pending) { *pending = float(b.value) / 10.0f; return; }
                    ini.SetFloat(SoundSection, name, float(b.value) / 10.0f);
                    ApplyVolumes();
                });
        };
        pendingAmbient = pendingSpeech = -1.0f;
        volume(0x43E, "Master", nullptr);
        volume(0x43F, "Ambient", &pendingAmbient);
        volume(0x440, "Speech", &pendingSpeech);
        m->AddCheck(0x441, ini.GetBool(SoundSection, "CD", true), [this](Button& b) { ini.SetBool(SoundSection, "CD", b.value != 0); });
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x40F);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::DisplaySettings(int previous)
    {
        auto m = std::make_unique<Menu>(CmdDisplaySettings, res, settings.colours, MenuBackdrop, res.Human() ? 0x2F6 : 0x2F7, 1);
        m->AddHeading(0x410);

        // TODO: the engine draws at 640 x 480 for now; these are saved for the launcher and for later
        std::vector<Mode> modes;
        SDL_DisplayID display = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode* desktop = display ? SDL_GetDesktopDisplayMode(display) : nullptr;
        for (const Mode& mode : Modes)
            if (!desktop || (mode.w <= desktop->w && mode.h <= desktop->h)) modes.push_back(mode);
        if (modes.empty()) modes.push_back({ 640, 480 });
        std::string size = ini.GetString("Screen", "Size", "640,480");
        int w = std::atoi(size.c_str()), h = 0;
        if (auto comma = size.find(','); comma != std::string::npos) h = std::atoi(size.c_str() + comma + 1);
        int current = 0;
        for (size_t i = 0; i < modes.size(); i++) if (modes[i].w == w && modes[i].h == h) current = int(i);
        std::string resolution = res.String(0x449);
        m->AddSlider(current, int(modes.size()) - 1,
            [resolution, modes](int v) { return Format(resolution, "%s: %d * %d", modes[size_t(v)].w, modes[size_t(v)].h); },
            [this, modes](Button& b) {
                const Mode& mode = modes[size_t(b.value)];
                ini.SetString("Screen", "Size", std::to_string(mode.w) + "," + std::to_string(mode.h));
            });

        bool eightBit = ini.GetInt("Screen", "BPP", 16) == 8;
        // the radios' tool tips are their labels; the options' are 0x453-0x456 (0x4098D5-0x4099A4)
        m->AddRadio(0x447, 0x8110, eightBit, [this](Button& b) { if (b.value) ini.SetInt("Screen", "BPP", 8); }, 0x447);
        m->AddRadio(0x448, 0x8110, !eightBit, [this](Button& b) { if (b.value) ini.SetInt("Screen", "BPP", 16); }, 0x448);

        auto option = [&](int textId, const char* name, int tooltip) {
            m->AddCheck(textId, ini.GetBool("Options", name, true), [this, name](Button& b) { ini.SetBool("Options", name, b.value != 0); }, tooltip);
        };
        option(0x44D, "Show lights", 0x453);
        option(0x450, "Show Smoke & particles", 0x456);
        option(0x44F, "Show gouraud shading", 0x455);
        option(0x44E, "Show vehicle shadows", 0x454);

        // [Screen] "Scroll Speed" (default 40) shows as (speed - 10) / 10
        int scroll = std::clamp((ini.GetInt("Screen", "Scroll Speed", 40) - 10) / 10, 0, 10);
        std::string scrollText = res.String(0x451);
        m->AddSlider(scroll, 10, [scrollText](int v) { return Format(scrollText, "%s: %d", v); },
            [this](Button& b) { ini.SetInt("Screen", "Scroll Speed", b.value * 10 + 10); });

        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x40F);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::CriticalEvents(int previous)
    {
        auto m = std::make_unique<Menu>(CmdCriticalEvents, res, settings.colours, MenuBackdrop, res.Human() ? 0x2F6 : 0x2F7, 2);
        m->AddHeading(0x412);
        for (size_t i = 0; i < std::size(Events); i++)
        {
            if (i == 5) m->NextColumn();
            const EventOption& e = Events[i];
            m->AddEventCheck(e.text, ini.GetBool("Event", e.name, e.on), [this, e](Button& b) { ini.SetBool("Event", e.name, b.value != 0); });
        }
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x40F);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::SinglePlayer(int previous)
    {
        auto m = NewMenu(CmdSinglePlayer);
        m->AddHeading(0x40B);                          // "Single Player Options"
        m->AddButton(CmdInsertCd, 0x40C, CmdIntro);    // "Start New Campaign": straight to the intro, via the disc check
        m->FocusLast();
        m->AddButton(CmdLoadGame, 0x406);              // "Resume Campaign"
        m->AddButton(CmdOptions, 0x401);
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x40F);
        return m;
    }

    bool FrontEnd::CorrectCdInserted() const
    {
        // The game (0x408B70) looks for Human.cd or Martian.cd, whichever side is being played, in the root of the
        // CD drive. The engine runs from the copy WoWLauncher made of both discs, music included, so the disc is
        // never needed: the check always passes and the Insert CD menu goes straight on.
        return true;
    }

    std::unique_ptr<Screen> FrontEnd::InsertCd(int next)
    {
        auto m = NewMenu(CmdInsertCd);
        m->AddHeading(0x531);                          // "Please Insert Human CD"
        m->AddButton(CmdInsertCdProceed, 0x469);       // "Proceed"
        m->AddBottomButton(CmdMainMenu, 0x530);        // "Return to Main Menu"
        m->FocusLast();
        insertCdNext = next;
        m->SetPrevious(CmdMainMenu);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::ReplaceCampaign(int previous)
    {
        // the game builds this with the Quit Game class (id 0x70, "Quit Game" in its debug name)
        auto m = NewMenu(CmdQuitGame);
        m->AddHeading(0x532);                          // "Replace Existing Campaign?"
        m->AddButton(CmdReplaceCampaign, 0x469);       // "Proceed"
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x446); // "Cancel"
        m->FocusLast();
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::EditCampaign(int previous)
    {
        auto m = std::make_unique<Menu>(CmdEditCampaign, res, settings.colours, MenuBackdrop, res.Human() ? 0x2F6 : 0x2F7, 2);
        m->AddHeading(0x52E);                          // "Enter name of Campaign"
        m->AddTextEntry(campaignName, int(SaveSlot::NameLength), CmdEditCampaign, [this](const std::string& text) { campaignName = text; });
        m->FocusLast();
        m->AddButton(CmdEditCampaign, 0x52F);          // "Begin Campaign"
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x40F);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::ServiceProvider(int previous, int next)
    {
        auto m = std::make_unique<Menu>(CmdNetwork, res, settings.colours, MenuBackdrop, res.Human() ? 0x2F6 : 0x2F7, 2);
        m->AddHeading(0x430);                          // "Select Service Provider"
        // The game lists DirectPlay's service providers (DirectPlayEnumerateA, up to 10), one button each, and
        // picking one opens the next menu (next: 0x9F to host in Human games, 0xA6 to join in Martian ones).
        // The engine has no network play yet, so there are none to list.
        (void)next;
        m->AddGap(0x1A - 0x0A);                        // 0x4B8DFC - 0x4B8DF8
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x40F);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::NetworkHost(int previous)
    {
        auto m = NewMenu(CmdNetworkHost);
        m->AddHeading(0x3FF);                          // "Network Campaign"
        m->AddButton(CmdNewNetworkGame, 0x40D);        // "Start New Network Game"
        m->FocusLast();
        m->AddButton(CmdLoadNetworkGame, 0x40E);       // "Load Network Campaign"
        m->AddButton(CmdOptions, 0x401);
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x40F);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::WaitingForPlayer(int previous)
    {
        auto m = NewMenu(CmdWaitingForPlayer);
        m->AddHeading(0x433);                          // "Waiting for Player..."
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x40F);
        m->FocusLast();
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::NetworkSessions(int previous)
    {
        auto m = std::make_unique<Menu>(CmdJoinNetwork, res, settings.colours, MenuBackdrop, res.Human() ? 0x2F6 : 0x2F7, 2);
        // The game rebuilds this every 30 ticks (0x4073D0, 0x407260) from the sessions DirectPlay finds: heading
        // 0x431 "Select Network Campaign" over a button per session (up to 10), or 0x432 "Waiting for Network
        // Campaign..." while there are none. With no network play yet there never are.
        m->AddHeading(0x432);
        m->AddGap(0x1A - 0x0A);
        m->AddBottomButton(previous, 0x40F);
        m->FocusLast();
        return m;
    }

    std::filesystem::path FrontEnd::SlotFile(int slot) const
    {
        char name[32];
        std::snprintf(name, sizeof(name), "%s.%03d", res.Human() ? "Human" : "Martian", slot);
        // the SaveGame folder as it is on disk (any case), else a new one (WinMain makes it, 0x401940)
        std::filesystem::path folder = files.Root() / "SaveGame";
        if (auto existing = files.Resolve("SaveGame")) folder = *existing;
        if (auto existing = files.Resolve(std::string("SaveGame\\") + name)) return *existing;
        return folder / name;
    }

    // A save written whole to a file beside it, then put in its place, so a failed write leaves the old one
    bool FrontEnd::WriteSaveFile(const std::filesystem::path& path, const std::vector<uint8_t>& data)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::filesystem::path temp = path;
        temp += ".new";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
            out.flush();
            if (!out) { std::filesystem::remove(temp, ec); return false; }
        }
        std::filesystem::rename(temp, path, ec);
        if (ec)
        {
            // some systems won't rename over a file: replace it
            std::filesystem::remove(path, ec);
            std::filesystem::rename(temp, path, ec);
        }
        return !ec;
    }

    bool FrontEnd::NewCampaignFile(int slot, const std::string& name)
    {
        auto start = files.Resolve(res.Human() ? "Human.000" : "Martian.000");
        if (!start || slot < SaveSlot::First || slot > SaveSlot::Last) return false;
        std::ifstream in(*start, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), {});
        try
        {
            // a new GAME header with the name, then the start file's body as it is (0x44EBB0)
            SaveFile save = SaveFile::Parse(data);
            save.SetName(name);
            data = save.Write();
        }
        catch (const std::exception& e)
        {
            SDL_Log("New campaign: %s: %s", start->string().c_str(), e.what());
            return false;
        }
        std::filesystem::path path = SlotFile(slot);
        if (!WriteSaveFile(path, data)) { SDL_Log("New campaign: can't write %s", path.string().c_str()); return false; }
        SDL_Log("New campaign \"%s\" in %s", name.c_str(), path.string().c_str());
        return true;
    }

    void FrontEnd::SaveCampaign()
    {
        WarMap* map = ActiveWarMap();
        if (!map || playingSlot < SaveSlot::First || playingSlot > SaveSlot::Last) return;
        auto data = map->SaveData();
        if (!data) { SDL_Log("The war map didn't come from a save it can write, so it isn't saved"); return; }
        bool ok = WriteSaveFile(SlotFile(playingSlot), *data);
        SDL_Log(ok ? "Saving game to slot %d" : "Saving game to slot %d failed", playingSlot);
    }

    void FrontEnd::StartWarMap(const std::string& gamePath, const std::string& name, int slot, bool loaded)
    {
        std::vector<uint8_t> data;
        if (auto path = files.Resolve(gamePath))
        {
            std::ifstream in(*path, std::ios::binary);
            data.assign(std::istreambuf_iterator<char>(in), {});
        }
        SaveInfo info;
        try { info = SaveInfo::Read(data); }
        catch (const std::exception& e)
        {
            SDL_Log("Can't start the war map from %s: %s", gamePath.c_str(), e.what());
            Show(NotRebuilt(CmdStartCampaign, CmdMainMenu));
            return;
        }
        if (!name.empty()) info.name = name;
        SDL_Log("War map from %s: \"%s\", %d/%d/%d, view %u,%u sector %u", gamePath.c_str(), info.name.c_str(), info.Day(), info.Month(),
            info.Year(), info.warMap[0], info.warMap[1], info.warMap[2]);
        // WWarMapWorld (0x417CD0) fades every sound out over 30 ticks, stops the CD and has the music start 5
        // seconds later (0x4181BF)
        sound.FadeOutAll(30);
        ambient = 0;
        warMap.reset();
        StopWarMapSounds();
        music.Stop();
        musicTimer = 5 * 30;
        std::map<int, ObjOjd::UnitData> units;
        std::map<int, ObjOjd::ArrowType> arrows;
        std::map<int, std::array<int, 6>> deposits;
        if (auto obj = files.Resolve("OBJ.ojd"))
        {
            std::ifstream in(*obj, std::ios::binary);
            std::vector<uint8_t> objData((std::istreambuf_iterator<char>(in)), {});
            try { units = ObjOjd::LoadUnits(objData); }
            catch (const std::exception& e) { SDL_Log("OBJ.ojd units: %s", e.what()); }
            try { arrows = ObjOjd::LoadArrowTypes(objData); }
            catch (const std::exception& e) { SDL_Log("OBJ.ojd arrow types: %s", e.what()); }
            try { researchPages = ObjOjd::LoadResearchPages(objData); }
            catch (const std::exception& e) { SDL_Log("OBJ.ojd research pages: %s", e.what()); }
            try { deposits = ObjOjd::LoadSectorDeposits(objData); }
            catch (const std::exception& e) { SDL_Log("OBJ.ojd sector deposits: %s", e.what()); }
            try { sectorMaps = ObjOjd::LoadSectorMaps(objData); }
            catch (const std::exception& e) { SDL_Log("OBJ.ojd sector maps: %s", e.what()); }
            try
            {
                auto tables = ObjOjd::LoadBattleTables(objData);
                dayTimes = tables.dayTimes;
                wofTypes = tables.six;
                lightTypes = tables.thirteen;
                moveCosts = tables.ten;
                moveSlopes = tables.eleven;
                weaponData = tables.twelve;
            }
            catch (const std::exception& e) { SDL_Log("OBJ.ojd day times: %s", e.what()); }
        }
        // each object type's class (AI.ojd, 0x441580 -> 0x518B14)
        std::map<int, int> classes, weights;
        std::map<int, std::vector<WarMap::AiSite>> sites;
        if (auto ai = files.Resolve("AI.ojd"))
        {
            std::ifstream in(*ai, std::ios::binary);
            std::vector<uint8_t> aiData((std::istreambuf_iterator<char>(in)), {});
            // each weight times the side's multiplier as the file's read (0x441625: fild, fmul, ftol)
            const float mult = res.Human() ? ini.GetFloat("Tweak", "AI strength table Human multiplier", 1.0f)
                                           : ini.GetFloat("Tweak", "AI strength table Martian multiplier", 2.0f);
            try
            {
                AiTable table = AiTable::Load(aiData);
                for (const auto& [id, e] : table.entries)
                {
                    classes[id] = e.value;
                    weights[id] = int(int16_t(int(double(e.weight) * double(mult))));
                }
                // the computer player's sites: the side not being played (0x518814)
                for (const auto& [sector, rules] : table.rules[res.Human() ? 1 : 0])
                    for (const AiRule& r : rules)
                    {
                        WarMap::AiSite site{ int16_t(r.words[0]), int16_t(r.words[1]), int16_t(r.words[2]), 0, 0 };
                        if (r.count == 5) { site.x2 = int16_t(r.words[3]); site.y2 = int16_t(r.words[4]); }
                        sites[sector].push_back(site);
                    }
            }
            catch (const std::exception& e) { SDL_Log("AI.ojd: %s", e.what()); }
        }
        // the save read exactly, so it can be written again; failing that, read as before (and not saved)
        std::optional<SaveFile> exact;
        try { exact = SaveFile::Parse(data); }
        catch (const std::exception& e) { SDL_Log("%s can't be read exactly (%s), so the campaign won't be saved", gamePath.c_str(), e.what()); }
        std::vector<SaveInfo::WarMapObject> objects = exact ? WarMap::ObjectsOf(*exact) : SaveInfo::WarMapObjects(data);
        std::vector<int> owners = SaveInfo::SectorOwners(data);
        playingSlot = slot;
        auto map = std::make_unique<WarMap>(res, settings.colours, info, std::move(objects), std::move(units), std::move(classes),
            std::move(owners), ini.GetInt("Screen", "Scroll Speed", 40), ini.GetFloat("Tweak", "Clunting Speed", 7.0f));
        map->SetArrowTypes(std::move(arrows));
        map->SetEventOptions([this](const char* name, bool on) { return ini.GetBool("Event", name, on); });
        map->SetDeposits(std::move(deposits), ini.GetFloat("Tweak", "Resource Drop Off Rate", 3.0f));
        map->SetPodInterval(ini.GetInt("Tweak", "Pod Interval (hours)", 24));
        map->SetClunting(ini.GetInt("Tweak", "WM clunting res", 36), ini.GetInt("Tweak", "WM clunting adj", 4));
        map->SetRouteFormation(ini.GetInt("Tweak", "Route clunting width", 4), ini.GetInt("Tweak", "Route clunting spacing", 4));
        map->SetEconomy(ini.GetFloat("Tweak", "Distance Crippler VX", 0.5f), ini.GetFloat("Tweak", "Distance Crippler VY", 0.5f),
                        ini.GetFloat("Tweak", "Resource Buffer", 30.0f));
        map->SetQuickCancelTimeout(ini.GetInt("Tweak", "Quick Cancel Timeout", 5));
        map->SetSectorLimits(ini.GetInt("Tweak", "Max units in sector", 15), ini.GetInt("Tweak", "Max boats in sector", 5));
        ObjOjd::ManufactureModifiers manufacture;
        manufacture.humanFriend = ini.GetFloat("Tweak", "HumanFriend Manufacture Modifier", 1.0f);
        manufacture.humanEnemy = ini.GetFloat("Tweak", "HumanEnemy Manufacture Modifier", 0.85f);
        manufacture.martianFriend = ini.GetFloat("Tweak", "MartianFriend Manufacture Modifier", 1.0f);
        manufacture.martianEnemy = ini.GetFloat("Tweak", "MartianEnemy Manufacture Modifier", 0.85f);
        map->SetManufacture(manufacture);
        map->SetBuildListPlace(ini.GetInt("BuildList", "Left", 0), ini.GetInt("BuildList", "Top", 70));
        WarMap::AiTweaks aiTweaks;
        auto tweak = [&](const char* name, float& v) { v = ini.GetFloat("Tweak", name, v); };
        tweak("AI Frontline Attractor for Defences", aiTweaks.frontlineDefences);
        tweak("AI Eff Threshold", aiTweaks.effThreshold);
        tweak("AI Panic Build Threshold", aiTweaks.panicBuild);
        tweak("AI Move Threshold", aiTweaks.moveThreshold);
        tweak("AI Frontline Attractor", aiTweaks.frontlineAttractor);
        tweak("AI Build Resfac Eagerness", aiTweaks.resfacEagerness);
        tweak("AI Aggression Value", aiTweaks.aggression);
        tweak("AI Invasion Threshold PC", aiTweaks.invasionThreshold);
        tweak("AI Outnumber Force", aiTweaks.outnumberForce);
        tweak("AI Retain Force", aiTweaks.retainForce);
        tweak("AI Invader NFriend Mul", aiTweaks.invaderNFriendMul);
        tweak("AI safe sector repeller Mul", aiTweaks.safeRepeller);
        tweak("AI Minimum Rr", aiTweaks.minimumRr);
        tweak("AI Movement Temperature", aiTweaks.temperature);
        map->SetAi(aiTweaks, std::move(weights), std::move(sites));
        if (exact) map->UseSave(std::move(*exact));
        ObjOjd::ResearchModifiers modifiers;
        modifiers.humanFriend = ini.GetFloat("Tweak", "HumanFriend Research Modifier", 0.75f);
        modifiers.humanEnemy = ini.GetFloat("Tweak", "HumanEnemy Research Modifier", 0.75f);
        modifiers.martianFriend = ini.GetFloat("Tweak", "MartianFriend Research Modifier", 0.75f);
        modifiers.martianEnemy = ini.GetFloat("Tweak", "MartianEnemy Research Modifier", 0.75f);
        map->StartResearch(modifiers);
        if (loaded) map->Message(res.String(0x465)); // 0x80B3: "Game Loaded"
        Show(std::move(map));
    }

    void FrontEnd::PlayMovie(const std::string& name, Command next)
    {
        std::string error;
        auto movie = Movie::Open(files, name, res.Human(), error);
        if (!movie)
        {
            SDL_Log("%s: %s", name.c_str(), error.c_str());
            Execute(next);
            return;
        }
        // every sound stops for the movie (0x44DEFE -> 0x465B20)
        sound.FadeOutAll(0);
        ambient = 0;
        Show(std::make_unique<MovieScreen>(res, std::move(movie), next, sound.Master()));
    }

    std::unique_ptr<Screen> FrontEnd::WarMapMenu()
    {
        // 0x49B980, "Warmap menu": drawn over the map, with the war map's palette group
        if (screen && screen->Id() == WarMap::Id) warMap.reset(static_cast<WarMap*>(screen.release()));
        auto m = std::make_unique<Menu>(CmdWarMapMenu, res, settings.colours, 0, warmap::PaletteGroup);
        m->AddHeading(0x3FC);                          // "Campaign Options"
        m->AddButton(Commands::Back, 0x405);           // "Resume Campaign (Escape)"
        m->FocusLast();
        m->AddButton(CmdMainMenu, 0x40A);              // "Return to Main Menu"
        m->AddButton(CmdCriticalEvents, 0x412);
        m->AddButton(CmdSoundControl, 0x411);
        m->AddBottomButton(CmdQuitMenu, 0x404);        // "Save & Exit Game"
        m->SetPrevious(WarMap::Id);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::BattleMapMenu()
    {
        // 0x49BD50, "Battlemap menu" (0x80B8 from the battle map's Escape, 0x484FB0): drawn over the battle map with
        // its palette group, sound effects at a quarter and the world paused (+0x44) while it's open. Resume (0x8123)
        // goes back; that's all but Sound Control: no save, no quit.
        if (screen && screen->Id() == BattleMap::Id) battleMap = std::move(screen);
        auto m = std::make_unique<Menu>(BattleMap::MenuCommand, res, settings.colours, 0, 0x2F0);
        m->AddHeading(0x3FD);                          // "Battle Options"
        m->AddButton(Commands::Back, 0x405);
        m->FocusLast();
        m->AddBottomButton(CmdSoundControl, 0x411);
        m->SetPrevious(BattleMap::Id);
        return m;
    }

    std::unique_ptr<Screen> FrontEnd::NotRebuilt(int command, int previous)
    {
        auto m = NewMenu(command);
        char text[96];
        std::snprintf(text, sizeof(text), "Not rebuilt yet\n(command 0x%X)", command);
        m->AddHeadingText(text);
        m->AddBottomButton(previous, 0x40F);
        return m;
    }

    void FrontEnd::Execute(const Command& command)
    {
        if (screen && screen->Handle(command)) return;
        int current = screen ? screen->Id() : CmdMainMenu;
        SDL_Log("Command 0x%X (%d) from screen 0x%X", command.id, command.param, current);
        switch (command.id)
        {
        case CmdMainMenu:
            // leaving the war map saves the campaign in its slot first (0x41CD50: 0x90 -> 0x80B4)
            SaveCampaign();
            topMenu = CmdMainMenu; warMap.reset(); playingSlot = 0; Show(MainMenu()); break;
        case CmdOptions: { int back = BackTo(command.id, current); ShowMenu(GameOptions(back), back); } break;
        case CmdQuitMenu: { int back = BackTo(command.id, current); ShowMenu(QuitGame(back), back); } break;
        case CmdQuitGame: SaveCampaign(); quit = true; break; // 0x70 -> 0x80B4 as well
        case CmdSoundControl: { int back = BackTo(command.id, current); ShowMenu(SoundControl(back), back); } break;
        case CmdDisplaySettings: { int back = BackTo(command.id, current); ShowMenu(DisplaySettings(back), back); } break;
        case CmdCriticalEvents: { int back = BackTo(command.id, current); ShowMenu(CriticalEvents(back), back); } break;
        case CmdLoadGame: networkLoad = false; { int back = BackTo(command.id, current); ShowMenu(LoadGame(back), back); } break;
        case CmdLoadNetworkGame: networkLoad = true; { int back = BackTo(command.id, current); ShowMenu(LoadGame(back), back); } break;
        case CmdSaveMenu: { int back = BackTo(command.id, current); ShowMenu(SaveGame(back), back); } break;
        case CmdSinglePlayer: topMenu = CmdSinglePlayer; Show(SinglePlayer(CmdMainMenu)); break;
        case CmdNetwork: Show(ServiceProvider(CmdMainMenu, res.Human() ? CmdNetworkHost : CmdJoinNetwork)); break;
        case CmdNetworkHost: topMenu = CmdNetworkHost; Show(NetworkHost(CmdNetwork)); break;
        case CmdNewNetworkGame: Show(WaitingForPlayer(CmdNetworkHost)); break;
        case CmdJoinNetwork: Show(NetworkSessions(CmdNetwork)); break;

        // Start New Campaign (0x404230): pick the save slot it goes in, with the save menu's "Select Campaign to
        // Replace", returning to the menu it was chosen from
        case CmdNewCampaign: Show(SaveGame(topMenu)); break;
        case CmdSaveSlot:
            // 0x4043D0: a slot that's in use asks first, an empty one goes straight to naming the campaign
            campaignSlot = command.param;
            if (auto name = SaveSlot::Name(files, res.Human(), campaignSlot)) { campaignName = *name; Show(ReplaceCampaign(CmdNewCampaign)); }
            else { campaignName.clear(); Execute({ CmdReplaceCampaign, 0 }); }
            break;
        case CmdReplaceCampaign: Show(EditCampaign(CmdNewCampaign)); break;
        case CmdEditCampaign:
            // 0x404660 sets up the new campaign in the slot, then on to the disc check and the intro
            if (!NewCampaignFile(campaignSlot, campaignName)) campaignSlot = 0;
            Execute({ CmdInsertCd, CmdIntro });
            break;
        case CmdInsertCd:
            // the war map's own 0x8133 (0x41B210) starts the briefing first
            if (current == WarMap::Id) BriefingTransition();
            // 0x403FA0: with the right disc in, the command goes straight through
            if (CorrectCdInserted()) Execute({ command.param, 0 });
            else Show(InsertCd(command.param));
            break;
        case CmdInsertCdProceed:
            if (CorrectCdInserted()) Execute({ insertCdNext, 0 });
            break;
        // the movies (0x402B80: each plays through 0x42CCF0, with the command to send when it's over)
        case CmdRageLogo: PlayMovie("RAGELOGO", { CmdTitle, 0 }); break;
        case CmdTitle: PlayMovie("TITLE", { CmdMainMenu, 0 }); break;
        case CmdIntro: PlayMovie("INTRO", { CmdStartCampaign, 0 }); break;
        case CmdStartCampaign:
            // a new campaign plays from its slot, set up from Human.000 or Martian.000 (0x404660); if that
            // couldn't be written, from the start file itself, unsaved
            if (campaignSlot >= SaveSlot::First && SaveSlot::Exists(files, res.Human(), campaignSlot))
                StartWarMap(SaveSlot::GamePath(res.Human(), campaignSlot), {}, campaignSlot);
            else StartWarMap(res.Human() ? "Human.000" : "Martian.000", campaignName);
            break;
        case CmdLoadSlot:
            StartWarMap(SaveSlot::GamePath(res.Human(), command.param), {}, command.param, true);
            break;
        case CmdWarMapMenu: Show(WarMapMenu()); break;
        case BattleMap::MenuCommand: Show(BattleMapMenu()); break;
        case WarMap::ResearchCommand:
            // 0x7D (0x419A4D): time stops, the war map steps back and the local player's research opens (0x458150)
            if (screen && screen->Id() == WarMap::Id) warMap.reset(screen.release());
            if (WarMap* map = ActiveWarMap(); map && map->LocalResearch())
            {
                ResearchScreen::Colours c;
                c.current = ini.GetInt("Colours", "Research Current", c.current);
                c.normal = ini.GetInt("Colours", "Research Normal", c.normal);
                c.fresh = ini.GetInt("Colours", "Research New", c.fresh);
                c.heading = ini.GetInt("Colours", "Research Heading", c.heading);
                c.proposal = ini.GetInt("Colours", "Research Proposal", c.proposal);
                c.detail = ini.GetInt("Colours", "Research Detail", c.detail);
                c.specification = ini.GetInt("Colours", "Research Specification", c.specification);
                c.enhancement = ini.GetInt("Colours", "Research Enhancement", c.enhancement);
                c.menuItems = settings.colours.items;
                c.glowFirst = settings.colours.glowFirst;
                c.glowLast = settings.colours.glowLast;
                const int rate = res.Human() ? ini.GetInt("Research", "Human Open Rate", 20) : ini.GetInt("Research", "Martian Open Rate", 10);
                Show(std::make_unique<ResearchScreen>(res, *map->LocalResearch(), researchPages, c, rate));
            }
            break;
        case ResearchScreen::CloseCommand:
            // 0xBD: the research closes (0x458380, the cabinet 0x19D) and the war map comes back (0x41A10F)
            if (current == ResearchScreen::Id && warMap)
            {
                if (WarMap* map = ActiveWarMap(); map && map->LocalResearch()) map->LocalResearch()->ShowPage(-1);
                if (screen && screen->Id() == ResearchScreen::Id)
                    if (int h = static_cast<ResearchScreen*>(screen.get())->Speech()) sound.FadeOut(h, 30); // 0x45E4A0
                sound.Play(0x19D);
                Show(std::move(warMap));
            }
            break;
        case CmdStartNetworkGame:
        case 0xAB:
            // TODO: network games need network play
            Show(NotRebuilt(command.id, CmdMainMenu));
            break;
        case CdPlayer::Id:
            sound.FadeOutAll(60); // 0x407A21: the menu sound makes way for the CD
            Show(std::make_unique<CdPlayer>(res, settings.colours, CmdMainMenu, music, files)); break;
        case Credits::Id:
            // the credits fade out every sound over 60 ticks (0x464AB0) before their own music; the menu sound
            // comes back with the next menu (after the game's end they go back to the main menu, 0x80C9)
            sound.FadeOutAll(60);
            Show(std::make_unique<Credits>(res, command.param ? command.param : current, creditsEasterEgg, sound));
            break;
        case Briefing::Id:
            // 0x8019 (0x41A13C): the Briefing for the battle's sector (0x499930)
            if (WarMap* map = ActiveWarMap())
            {
                if (screen && screen->Id() == WarMap::Id) warMap.reset(screen.release());
                Show(std::make_unique<Briefing>(res, BattleColoursFromIni(), map->Briefing(), ini.GetInt("Briefing", "Delay", 30)));
            }
            break;
        case 0x71:
        case 0x98:
            // 0x41A082: on to the battle map (0x98 flushes the queue first, 0x435600, taking the waiting 0x71 with
            // it). The battle map shows the sector's ground; leaving it (0x97) settles the battle. Without the map's
            // files the battle is settled at once.
            if (ActiveWarMap() && StartBattleMap()) break;
            [[fallthrough]];
        case BattleMap::ReturnCommand:  // Return to Warmap: 0x76 to the world flushes the queues and sends 0x97
        case BattleMap::EndCommand:
            battleMap.reset();
            // 0x97 (0x41A0DC): the battle's end. The battle isn't fought on the battle map yet, so the war map
            // settles it (*provisional*), then the Debriefing (0x49A7B0)
            if (WarMap* map = ActiveWarMap())
            {
                sound.FadeOut(briefingSound, 30);
                briefingSound = 0;
                map->FightBattle();
                auto d = std::make_unique<Debriefing>(res, BattleColoursFromIni(), map->Debriefing(), ini.GetInt("Debriefing", "Delay", 300));
                const int h = sound.Play(d->Sound());   // +0x98, faded in over 60 ticks (0x464E60)
                sound.FadeIn(h, 60);
                Show(std::move(d));
            }
            break;
        case 0x99:
        case 0x8017:
            // 0x419AA2: back to the war map. Every sound fades out over 30 ticks, the ambient sound goes, and the
            // music comes back with 0x8147 30 ticks later (0x41B070); then the view's set-up announces the result
            // (0x808D) and the events waiting go on (0x41E730)
            if (warMap)
            {
                sound.FadeOutAll(30);
                warAmbient = clockSound = 0;
                clockSpeed = -1;
                inBattle = false;
                musicTimer = 30;
                WarMap* map = static_cast<WarMap*>(warMap.get());
                Show(std::move(warMap));
                map->ReturnFromBattle();
            }
            break;
        case CmdGameOver:
            // 0x80FE (0x402B80): the war map is shut down, then VICTORY for 0x80FA / 0x80FC or DEFEAT for 0x80FB /
            // 0x80FD, then the credits and the main menu
            warMap.reset();
            playingSlot = 0;
            topMenu = CmdMainMenu;
            PlayMovie(command.param == 0x80FA || command.param == 0x80FC ? "VICTORY" : "DEFEAT", { Credits::Id, CmdMainMenu });
            break;
        case Commands::Back:
            // on the war map Escape opens its menu, and in that menu it (and Resume Campaign, which sends the same
            // 0x8123) goes back to the map
            if (current == WarMap::Id) { Execute({ CmdWarMapMenu, 0 }); break; }
            if ((current == CmdWarMapMenu || current == WarMap::ResearchCommand) && warMap) { Show(std::move(warMap)); break; }
            if (current == BattleMap::MenuCommand && battleMap) { Show(std::move(battleMap)); break; }
            // Escape goes back to the menu this one was opened from; the main menu itself ignores it, and so does
            // the Insert CD menu (0x408AB0)
            if (current != CmdMainMenu && current != CmdInsertCd) Execute({ screen->Previous() ? screen->Previous() : CmdMainMenu, 0 });
            break;
        default: Show(NotRebuilt(command.id, current == CmdOptions ? CmdOptions : CmdMainMenu)); break;
        }
    }

    bool FrontEnd::Fades(const Screen& s) const
    {
        // every front-end screen derives from the Fullscreen View (0x4061B0), the war map menu included; the
        // war map itself and the movie player don't
        return s.Id() != WarMap::Id && s.Id() != MovieScreen::Id && s.Id() != BattleMap::Id; // nor the battle map (a WView, 0x484A30)
    }

    void FrontEnd::Dispatch(const Command& command, bool fromControl)
    {
        if (delayed) return; // the view is fading out: it takes no more input
        if (fromControl && screen && Fades(*screen))
        {
            delayed = command;
            delayTicks = 0;
            overlay = 1;
            return;
        }
        Execute(command);
    }

    void FrontEnd::Update()
    {
        sound.Update();
        if (!screen) return;
        if (delayed)
        {
            // 0x811C: 50% black, then black; 0x811B: the command 5 ticks after the click
            delayTicks++;
            overlay = 2;
            if (delayTicks >= 5)
            {
                Command c = *delayed;
                delayed.reset();
                Execute(c);
            }
            return;
        }
        if (fadeIn > 0) { fadeIn--; overlay = fadeIn; }
        screen->Update();
        stillTicks++;
        UpdateTooltip();
        for (int id : screen->TakeSounds()) sound.Play(id);
        if (screen->Id() == ResearchScreen::Id)
        {
            auto* rs = static_cast<ResearchScreen*>(screen.get());
            if (rs->TakeSpeechFade() && rs->Speech()) { sound.FadeOut(rs->Speech(), 30); rs->SetSpeech(0); }
            if (int id = rs->TakeSpeechStart())
            {
                if (rs->Speech()) sound.FadeOut(rs->Speech(), 30);
                rs->SetSpeech(sound.Play(id));
            }
        }
        if (WarMap* map = ActiveWarMap()) { if (!inBattle) WarMapSounds(*map); }
        else if (musicTimer >= 0) StopWarMapSounds();
        // the credits' own exit is sent straight on (0x4056F0); the rest come from the view's controls
        if (auto command = screen->TakeCommand()) Dispatch(*command, screen->Id() != Credits::Id);
    }

    // The tool-tip manager (WDesktop 0x433870), every update: with a tip up for the part of the screen under the
    // mouse and the mouse still for less than [Screen] "Tool Tip Delay" (70), it stays (until a mouse button goes
    // down). Otherwise a button held takes it away; a new part under the mouse marks where the hovering started;
    // and the time the mouse has been still picks a level, 1 past "Quick Tool Tip Delay" (15), 2 past 70, 3 past
    // "Slow Tool Tip Delay" (150), which is asked for (then the levels below it) and shown if there's any text.
    void FrontEnd::UpdateTooltip()
    {
        if (!ini.GetBool("Screen", "Tool Tip Enable", true) || !screen || mouseX < 0 || screen->Id() == MovieScreen::Id)
        {
            tip.reset();
            return;
        }
        const int quick = ini.GetInt("Screen", "Quick Tool Tip Delay", 15), normal = ini.GetInt("Screen", "Tool Tip Delay", 70),
                  slow = ini.GetInt("Screen", "Slow Tool Tip Delay", 150);
        int owner = screen->TipOwner(mouseX, mouseY);
        const bool held = mouseHeld || rightHeld; // GetAsyncKeyState(1) / (2)
        if (tip && owner == hoverOwner && stillTicks < normal)
        {
            if (held) tip.reset();
            return;
        }
        if (held) { tip.reset(); return; }
        if (owner != hoverOwner) { hoverX = mouseX; hoverY = mouseY; }
        hoverOwner = owner;
        int level = stillTicks > slow ? 3 : stillTicks > normal ? 2 : stillTicks > quick ? 1 : 0;
        std::string text;
        for (; level >= 1 && text.empty(); level--) text = screen->TipText(owner, level);
        if (text.empty()) { tip.reset(); return; }
        tip = Tip{ text, hoverX, hoverY };
    }

    // The tip (0x49D470): boxed text in font 0x6F made with grey (dword_4DAF90, 6-bit 48, 48, 48) in Human games
    // and black (dword_4DAF88) in Martian ones, the text 1 in. Its top left goes 32 right and
    // [Screen] "Tool Tip Offset" (16) less 32 down from where the hovering started; it's kept 4 in from the
    // right edge and its middle at least 4 down, then grown by 2 on the left and top and 1 on the right and bottom.
    void FrontEnd::DrawTooltip(Canvas& canvas) const
    {
        if (!tip) return;
        const Font* font = res.GetFont(0x6F);
        if (!font) return;
        int tw = 0, lines = 0;
        size_t start = 0;
        for (;;)
        {
            size_t end = tip->text.find('\n', start);
            tw = std::max(tw, font->Measure(tip->text.substr(start, end == std::string::npos ? std::string::npos : end - start)));
            lines++;
            if (end == std::string::npos) break;
            start = end + 1;
        }
        int th = font->Height() * lines + 2 * (lines - 1);
        int w = tw + 2, h = th + 2;
        int offset = ini.GetInt("Screen", "Tool Tip Offset", 16);
        int cx = tip->x + 32 + w / 2, cy = tip->y - offset + 32 + h / 2;
        if (cx + w / 2 > canvas.Width()) cx = canvas.Width() - w / 2 - 4;
        if (cy < 4) cy = 4;
        Rect text{ cx - w / 2 + 1, cy - h / 2 + 1, tw, th };
        Rect box{ cx - w / 2 - 2, cy - h / 2 - 2, w + 3, h + 3 };
        // BoxedText's draw (0x49D300): the box filled with the colour it was made with, then a 1-pixel outline
        // (dword_4DAF88 black for the Humans, the Martians' side colour dword_4DAF9C, 6-bit 12, 42, 4)
        const uint32_t fill = res.Human() ? 0xC0C0C0 : 0;
        auto six = [](int v) { return uint32_t((v * 255 + 31) / 63); };
        const uint32_t outline = res.Human() ? 0 : (six(12) << 16 | six(42) << 8 | six(4));
        uint32_t* out = canvas.Pixels();
        for (int y = box.y; y < box.y + box.h; y++)
            for (int x = box.x; x < box.x + box.w; x++)
            {
                if (x < 0 || y < 0 || x >= canvas.Width() || y >= canvas.Height()) continue;
                bool edge = x == box.x || y == box.y || x == box.x + box.w - 1 || y == box.y + box.h - 1;
                out[size_t(y) * size_t(canvas.Width()) + size_t(x)] = edge ? outline : fill;
            }
        canvas.DrawString(0x6F, tip->text, text, TextCentreX | TextCentreY);
    }

    // EnterBattleMap (0x439770), as far as it goes: the sector's landscape (0x47AF10, lit for the calendar's hour,
    // ftol(+0x28) % 24) and its shade tables (0x40B570, from the enemy count of the battle before), the briefing
    // sound stopped (0x41B360), then the count for the next battle (0x43983E)
    bool FrontEnd::StartBattleMap()
    {
        WarMap* map = ActiveWarMap();
        if (!map) return false;
        BriefingInfo b = map->Briefing();
        auto name = sectorMaps.find(b.sector);
        if (name == sectorMaps.end()) return false;
        BattleMap::Setup s;
        s.sector = b.sector;
        s.mapName = name->second;
        s.hours = b.hours;
        if (auto d = dayTimes.find(int(b.hours) % 24); d != dayTimes.end()) s.dayTime = &d->second;
        s.shadeClass = Landscape::ShadeClass(lastEnemyCount);
        s.human = res.Human();
        s.quickCancelTimeout = ini.GetInt("Tweak", "Quick Cancel Timeout", 5);
        s.enemyCount = map->EnemyObjects(b.sector);
        s.loadScenery = ini.GetBool("Debug", "Load scenery", true);
        s.units = &map->Units();
        s.six = &wofTypes;
        s.lights = &lightTypes;
        s.moveCosts = &moveCosts;
        s.moveSlopes = &moveSlopes;
        s.weapons = &weaponData;
        s.showLights = ini.GetBool("Options", "Show lights", true);
        s.showSmoke = ini.GetBool("Options", "Show Smoke & particles", true);
        s.groups = map->BattleGroups(b.sector);
        // the owner as a faction (0x43CA30): the side being played, the other, or no one
        const int owner = map->SectorOwner(b.sector), played = res.Human() ? 0 : 1;
        s.ownerFaction = owner == 2 ? 2 : owner == 0 ? played : 1 - played;
        s.noBuildBorder = ini.GetInt("Tweak", "No-build border width (vertices)", 16);
        s.vehicles.maxAcc = ini.GetFloat("Vehicles", "maxAcc", 1.0f);
        s.vehicles.targAccWeight = ini.GetFloat("Vehicles", "targAccWeight", 0.003f);
        s.vehicles.targAccMax = ini.GetFloat("Vehicles", "targAccMax", 0.15f);
        s.vehicles.arrivalRadius = ini.GetFloat("Vehicles", "arrivalRadius", 1.5f);
        s.vehicles.speedCostDivisor = ini.GetFloat("Vehicles", "speedCostDivisor", 25.0f);
        s.vehicles.separationAccWeight = ini.GetFloat("Vehicles", "separationAccWeight", 0.2f);
        s.vehicles.separationAccMax = ini.GetFloat("Vehicles", "separationAccMax", 0.1f);
        s.vehicles.velMatchAccWeight = ini.GetFloat("Vehicles", "velMatchAccWeight", 0.08f);
        s.vehicles.velMatchAccMax = ini.GetFloat("Vehicles", "velMatchAccMax", 0.03f);
        s.vehicles.avoidanceAccWeight = ini.GetFloat("Vehicles", "avoidanceAccWeight", 0.15f);
        s.vehicles.avoidanceAccMax = ini.GetFloat("Vehicles", "avoidanceAccMax", 0.1f);
        s.vehicles.brakingAccWeight = ini.GetFloat("Vehicles", "brakingAccWeight", 0.1f);
        s.vehicles.brakingAccMax = ini.GetFloat("Vehicles", "brakingAccMax", 0.2f);
        s.vehicles.collisionCheckDist = ini.GetFloat("Vehicles", "collisionCheckDist", 0.5f);
        s.vehicles.attributeAccWeight = ini.GetFloat("Vehicles", "attributeAccWeight", 0.3f);
        s.vehicles.attributeAccMax = ini.GetFloat("Vehicles", "attributeAccMax", 0.2f);
        s.vehicles.arrivalAccWeight = ini.GetFloat("Vehicles", "arrivalAccWeight", 0.33f);
        s.vehicles.arrivalAccMax = ini.GetFloat("Vehicles", "arrivalAccMax", 0.2f);
        s.vehicles.attractAccWeight = ini.GetFloat("Vehicles", "attractAccWeight", 0.001f);
        s.vehicles.attractAccMax = ini.GetFloat("Vehicles", "attractAccMax", 0.1f);
        s.zMapRes = ini.GetInt("System", "ZMapRes", 400);
        s.sound = &sound;
        s.armour.scenery = ini.GetFloat("Tweak", "Scenery Armour Modifier", 1.2f);
        s.armour.humanFriend = ini.GetFloat("Tweak", "HumanFriend Armour Modifier", 1.8f);
        s.armour.humanEnemy = ini.GetFloat("Tweak", "HumanEnemy Armour Modifier", 1.8f);
        s.armour.martianFriend = ini.GetFloat("Tweak", "MartianFriend Armour Modifier", 1.8f);
        s.armour.martianEnemy = ini.GetFloat("Tweak", "MartianEnemy Armour Modifier", 1.8f);
        s.researchDone = [this, human = res.Human()](bool side, int index) {
            WarMap* wm = ActiveWarMap();
            if (!wm) return false;
            Research* r = side == human ? wm->LocalResearch() : wm->OtherResearch();
            if (!r || index < 0 || size_t(index) >= r->Items().size()) return false;
            return r->Items()[size_t(index)].state == 2;
        };
        if (s.zMapRes < 1) s.zMapRes = 400;
        try
        {
            auto battle = std::make_unique<BattleMap>(res, s);
            sound.FadeOut(briefingSound, 30);
            briefingSound = 0;
            lastEnemyCount = map->EnemyObjects(b.sector);
            SDL_Log("Battle map: sector %d, %s, hour %d, view at %d,%d", s.sector, s.mapName.c_str(), int(b.hours) % 24, battle->ScrollX(), battle->ScrollY());
            Show(std::move(battle));
            return true;
        }
        catch (const std::exception& e)
        {
            SDL_Log("battle map: %s", e.what());
            return false;
        }
    }

    WarMap* FrontEnd::ActiveWarMap() const
    {
        if (screen && screen->Id() == WarMap::Id) return static_cast<WarMap*>(screen.get());
        return static_cast<WarMap*>(warMap.get());
    }

    void FrontEnd::WarMapSounds(WarMap& map)
    {
        if (musicTimer > 0 && --musicTimer == 0) MusicStep();
        // the clock (0x41F860, on each change of speed): a Human clock ticks only while stopped (0x1A2, looping,
        // faded out over 15 ticks once it runs again); a Martian one has a sound for every speed (0x1A1 + speed,
        // the old one faded over 5 ticks and the new one faded in over 5)
        int speed = map.Speed();
        if (speed == clockSpeed) return;
        clockSpeed = speed;
        if (res.Human())
        {
            if (speed) { sound.FadeOut(clockSound, 15); clockSound = 0; }
            else if (!sound.Active(clockSound)) clockSound = sound.Play(0x1A2);
        }
        else
        {
            sound.FadeOut(clockSound, 5);
            clockSound = sound.Play(0x1A1 + speed);
            sound.FadeIn(clockSound, 5);
        }
    }

    void FrontEnd::MusicStep()
    {
        // the ambient sound from the last step fades out over 60 ticks
        sound.FadeOut(warAmbient, 60);
        warAmbient = 0;
        constexpr int TenSeconds = 10 * 30;
        if (ini.GetBool(SoundSection, "CD", true))
        {
            // still playing (the CD player's music, say): stop it and look again in 10 seconds
            if (music.Playing() && !music.Finished()) { music.Stop(); musicTimer = TenSeconds; return; }
            static constexpr int Tracks[2] = { 2, 3 };  // ArgList 0x4B9DD8
            musicTrack = (musicTrack + 1) % 2;
            int track = Tracks[musicTrack];
            char name[64];
            std::snprintf(name, sizeof(name), "Music-%s\\%02d Track%02d.wav", res.Human() ? "Human" : "Martian", track, track);
            auto path = files.Resolve(name);
            if (path && music.Play(*path) && music.LengthMs() / 1000 > 10)
            {
                SDL_Log("Playing track %d for %u seconds", track, unsigned(music.LengthMs() / 1000));
                musicTimer = int(music.LengthMs() / 1000) * 30;
                return;
            }
            musicTimer = TenSeconds;
            return;
        }
        warAmbient = sound.Play(0xFD + random::Rand() % 3);
        sound.FadeIn(warAmbient, 60);
        musicTimer = TenSeconds;
    }

    // 0x41B210 ("Fading up briefing sound over 1 second"): every sound fades out over 30 ticks, the ambient sound
    // goes, the CD stops and its position is reset (0x412C00, 0x412940), the view gets 0x6E, and the briefing sound
    // 0x18E starts at the mouse, faded in over 60 ticks (dword_4E9B84). The war map's music waits until it's back.
    void FrontEnd::BriefingTransition()
    {
        if (screen && screen->Id() == WarMap::Id) warMap.reset(screen.release());
        sound.FadeOutAll(30);
        warAmbient = clockSound = 0;
        music.Stop();
        inBattle = true;
        briefingSound = sound.Play(0x18E);
        sound.FadeIn(briefingSound, 60);
    }

    BattleColours FrontEnd::BattleColoursFromIni() const
    {
        BattleColours c;
        c.sectorName = ini.GetInt("Colours", "Briefing Sector Name", c.sectorName);
        c.heading = ini.GetInt("Colours", "Briefing Heading", c.heading);
        c.detail = ini.GetInt("Colours", "Briefing Detail", c.detail);
        return c;
    }

    void FrontEnd::StopWarMapSounds()
    {
        if (musicTimer >= 0) music.Stop();
        musicTimer = -1;
        sound.FadeOut(warAmbient, 0);
        sound.FadeOut(clockSound, 0);
        warAmbient = clockSound = 0;
        clockSpeed = -1;
    }

    void FrontEnd::Draw(Canvas& canvas) const
    {
        canvas.Clear(0);
        if (!screen) return;
        // the war map's menus are drawn over the map
        if (battleMap && !screen->Background()) battleMap->Draw(canvas);
        else if (warMap && !screen->Background()) warMap->Draw(canvas);
        if (screen->Background()) canvas.DrawSprite(screen->Background(), 0, 0, 0);
        screen->Draw(canvas);
        DrawTooltip(canvas);
        if (overlay) canvas.DrawSprite(0x5C, 0, 0, 0, 0, overlay == 1 ? Canvas::Blend::Translucent : Canvas::Blend::Opaque);
        DrawCursor(canvas);
    }

    void FrontEnd::DrawCursor(Canvas& canvas) const
    {
        // The cursor table (0x4B9200, read by 0x40CD30): GType and an offset from the mouse position for each
        // cursor the controls can ask for. The sprite's own OBJ.ojd hotspot applies as well.
        struct CursorDef { int gtype, dx, dy; };
        static constexpr CursorDef Cursors[] = {
            { 0x1, 0, 0 },     { 0x2, -16, -16 }, { 0x3, -16, -16 }, { 0x4, -16, -16 }, { 0x9, -16, -16 }, { 0xA, -16, -16 },
            { 0x1, -8, -8 },   { 0x1, -8, -8 },   { 0x1, -8, -8 },   { 0x1, -8, -8 },   { 0x1, -8, -8 },   { 0x1, -8, -8 },
            { 0xB, 0, 0 },     { 0xC, -16, -16 }, { 0xD, 0, 0 },     { 0xE, 0, 0 },     { 0xF, 0, 0 },     { 0x10, 0, 0 },
        };
        if (!GameCursor() || mouseX < 0 || !screen) return;
        if (screen->Id() == MovieScreen::Id) return; // no cursor over a movie
        int index = screen->CursorAt(mouseX, mouseY);
        if (index < 0 || index >= int(std::size(Cursors))) index = 0;
        const CursorDef& c = Cursors[index];
        canvas.DrawSprite(c.gtype, 0, mouseX + c.dx, mouseY + c.dy);
    }

    void FrontEnd::OnKey(int key)
    {
        if (key >= int(SDLK_A) && key <= int(SDLK_Z))
        {
            static constexpr char Code[] = "englishrose";
            char letter = char('a' + (key - int(SDLK_A)));
            cheatProgress = letter == Code[cheatProgress] ? cheatProgress + 1 : (letter == Code[0] ? 1 : 0);
            if (Code[cheatProgress] == 0)
            {
                creditsEasterEgg = true;
                cheatProgress = 0;
                SDL_Log("Cheat: englishrose");
            }
        }
        if (!screen) return;
        // Escape's back (0x40A1B0 sends 0x8123 itself) goes straight on; a control's command fades out first
        if (auto command = screen->OnKey(key)) Dispatch(*command, key != SDLK_ESCAPE);
    }

    void FrontEnd::OnMouseMove(int x, int y)
    {
        if (x != mouseX || y != mouseY) stillTicks = 0; // dword_4D14A4: the UI time of the last move
        mouseX = x;
        mouseY = y;
        if (screen) screen->OnMouseMove(x, y);
    }

    void FrontEnd::SetPaused(bool paused)
    {
        if (paused) musicWasPaused = music.Paused();
        if (!musicWasPaused) music.SetPaused(paused);
        sound.SetPaused(paused);
        if (screen) screen->SetPaused(paused);
    }

    void FrontEnd::OnRightButton(int x, int y, bool down)
    {
        rightHeld = down;
        if (!screen) return;
        if (auto command = screen->OnRightButton(x, y, down)) Dispatch(*command, true);
    }

    void FrontEnd::OnMouseButton(int x, int y, bool down)
    {
        mouseHeld = down;
        if (!screen) return;
        if (auto command = screen->OnMouseButton(x, y, down)) Dispatch(*command, true);
    }
}
