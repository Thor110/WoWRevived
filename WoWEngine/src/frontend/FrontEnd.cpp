#include "frontend/FrontEnd.h"
#include "core/SaveGame.h"

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
        constexpr int CmdStartCampaign = 0x8A;    // the war map
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
        Show(MainMenu());
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
        // The main menu (0x405ED0) always starts the menu sound afresh, fading it in over 60 ticks; Game Options
        // (0x406130) only starts it again if it has stopped.
        if (!restart && sound.Active(ambient)) return;
        sound.Stop(ambient);
        ambient = sound.Play(0x18C);
        sound.FadeIn(ambient, 60);
    }

    void FrontEnd::Show(std::unique_ptr<Screen> next)
    {
        int from = screen ? screen->Id() : 0;
        // Leaving the CD Player stops its music. The original doesn't (its CD player has no destructor of its own,
        // so the disc played on until something else used the drive); [WoW Engine] "CD Player Music Continues"
        // = 1 keeps that.
        if (from == CdPlayer::Id && next->Id() != CdPlayer::Id && !ini.GetBool("WoW Engine", "CD Player Music Continues", false))
            music.Stop();
        screen = std::move(next);
        // the "not rebuilt" placeholder returns to the menu it was opened from
        if (!screen->Previous() && screen->Id() != CmdMainMenu)
            screen->SetPrevious(from == CmdOptions || from == CmdLoadGame || from == CmdSaveMenu ? from : CmdMainMenu);
        if (screen->PaletteGroup() != loadedGroup)
        {
            res.LoadShadeGroup(screen->PaletteGroup());
            loadedGroup = screen->PaletteGroup();
        }
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
        StartAmbient(true);
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
        // the menu works in tenths (0x4B8A7C-0x4B8A84); the settings hold 0-1 like the game's
        auto volume = [&](int textId, const char* name) {
            int tenths = int(std::lround(std::clamp(ini.GetFloat(SoundSection, name, 1.0f), 0.0f, 1.0f) * 10));
            std::string text = res.String(textId);
            m->AddSlider(tenths, 10, [text](int v) { return Format(text, "%s - %d%%", v * 10); },
                [this, name](Button& b) {
                    ini.SetFloat(SoundSection, name, float(b.value) / 10.0f);
                    ApplyVolumes();
                });
        };
        volume(0x43E, "Master");
        volume(0x43F, "Ambient");
        volume(0x440, "Speech");
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
        m->AddRadio(0x447, 0x8110, eightBit, [this](Button& b) { if (b.value) ini.SetInt("Screen", "BPP", 8); });
        m->AddRadio(0x448, 0x8110, !eightBit, [this](Button& b) { if (b.value) ini.SetInt("Screen", "BPP", 16); });

        auto option = [&](int textId, const char* name) {
            m->AddCheck(textId, ini.GetBool("Options", name, true), [this, name](Button& b) { ini.SetBool("Options", name, b.value != 0); });
        };
        option(0x44D, "Show lights");
        option(0x450, "Show Smoke & particles");
        option(0x44F, "Show gouraud shading");
        option(0x44E, "Show vehicle shadows");

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
        case CmdMainMenu: topMenu = CmdMainMenu; Show(MainMenu()); break;
        case CmdOptions: Show(GameOptions(current)); break;
        case CmdQuitMenu: Show(QuitGame(current)); break;
        case CmdQuitGame: quit = true; break;
        case CmdSoundControl: Show(SoundControl(current)); break;
        case CmdDisplaySettings: Show(DisplaySettings(current)); break;
        case CmdCriticalEvents: Show(CriticalEvents(current)); break;
        case CmdLoadGame: networkLoad = false; Show(LoadGame(current)); break;
        case CmdLoadNetworkGame: networkLoad = true; Show(LoadGame(current)); break;
        case CmdSaveMenu: Show(SaveGame(current)); break;
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
            // 0x404660 sets up the new campaign in the slot (TODO, with the war map), then on to the disc check
            // and the intro
            SDL_Log("New campaign \"%s\" in slot %d", campaignName.c_str(), campaignSlot);
            Execute({ CmdInsertCd, CmdIntro });
            break;
        case CmdInsertCd:
            // 0x403FA0: with the right disc in, the command goes straight through
            if (CorrectCdInserted()) Execute({ command.param, 0 });
            else Show(InsertCd(command.param));
            break;
        case CmdInsertCdProceed:
            if (CorrectCdInserted()) Execute({ insertCdNext, 0 });
            break;
        case CmdIntro:
            // TODO: the game plays the INTRO movie here, then sends 0x8A
            SDL_Log("INTRO movie (not rebuilt)");
            Execute({ CmdStartCampaign, 0 });
            break;
        case CmdLoadSlot:
        case CmdStartCampaign:
        case CmdStartNetworkGame:
        case 0xAB:
            // TODO: these start the campaign (war map), which isn't rebuilt yet
            Show(NotRebuilt(command.id, CmdMainMenu));
            break;
        case CdPlayer::Id:
            sound.FadeOutAll(60); // 0x407A21: the menu sound makes way for the CD
            Show(std::make_unique<CdPlayer>(res, settings.colours, CmdMainMenu, music, files)); break;
        case Credits::Id:
            // the credits fade out every sound over 60 ticks (0x464AB0) before their own music; the menu sound
            // comes back with the next menu
            sound.FadeOutAll(60);
            Show(std::make_unique<Credits>(res, current, creditsEasterEgg, sound));
            break;
        case Commands::Back:
            // Escape goes back to the menu this one was opened from; the main menu itself ignores it, and so does
            // the Insert CD menu (0x408AB0)
            if (current != CmdMainMenu && current != CmdInsertCd) Execute({ screen->Previous() ? screen->Previous() : CmdMainMenu, 0 });
            break;
        default: Show(NotRebuilt(command.id, current == CmdOptions ? CmdOptions : CmdMainMenu)); break;
        }
    }

    void FrontEnd::Update()
    {
        sound.Update();
        if (!screen) return;
        screen->Update();
        if (auto command = screen->TakeCommand()) Execute(*command);
    }

    void FrontEnd::Draw(Canvas& canvas) const
    {
        canvas.Clear(0);
        if (!screen) return;
        if (screen->Background()) canvas.DrawSprite(screen->Background(), 0, 0, 0);
        screen->Draw(canvas);
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
        if (auto command = screen->OnKey(key)) Execute(*command);
    }

    void FrontEnd::OnMouseMove(int x, int y)
    {
        mouseX = x;
        mouseY = y;
        if (screen) screen->OnMouseMove(x, y);
    }

    void FrontEnd::OnMouseButton(int x, int y, bool down)
    {
        if (!screen) return;
        if (auto command = screen->OnMouseButton(x, y, down)) Execute(*command);
    }
}
