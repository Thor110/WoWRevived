#include "frontend/FrontEnd.h"

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
        constexpr int CmdNetwork = 0xA0;
        constexpr int CmdSinglePlayer = 0xA4;
        constexpr int CmdSoundControl = 0x3ED;
        constexpr int CmdDisplaySettings = 0x3EF;
        constexpr int CmdCriticalEvents = 0x8117;

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

    FrontEnd::FrontEnd(Resources& r, FrontEndSettings s, Settings& i, const GameFiles& f) : res(r), settings(s), ini(i), files(f)
    {
        music.SetVolume(ini.GetFloat(SoundSection, "Master", 1.0f));
        Show(MainMenu());
    }

    void FrontEnd::Show(std::unique_ptr<Screen> next)
    {
        int from = screen ? screen->Id() : 0;
        screen = std::move(next);
        if (!screen->Previous() && screen->Id() != CmdMainMenu) screen->SetPrevious(from == CmdOptions ? CmdOptions : CmdMainMenu);
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
        // SaveGame\Human.001 to .005 (or Martian.*), the same check as the main menu's constructor
        for (int slot = 1; slot <= 5; slot++)
        {
            char name[64];
            std::snprintf(name, sizeof(name), "SaveGame\\%s.%03d", res.Human() ? "Human" : "Martian", slot);
            if (files.Resolve(name)) return true;
        }
        return false;
    }

    std::unique_ptr<Screen> FrontEnd::MainMenu()
    {
        auto m = NewMenu(CmdMainMenu);
        // TODO: the game also starts the menu's ambient sound here (sound 0x18C, fading in over 60 ticks)
        m->AddHeading(0x3FB);
        if (settings.networkEnabled)
        {
            m->AddButton(CmdNetwork, 0x3FE);
            m->AddButton(CmdSinglePlayer, res.Human() ? 0x3FF : 0x400);
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
                    if (std::string(name) == "Master") music.SetVolume(float(b.value) / 10.0f);
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
        case CmdMainMenu: Show(MainMenu()); break;
        case CmdOptions: Show(GameOptions(current)); break;
        case CmdQuitMenu: Show(QuitGame(current)); break;
        case CmdQuitGame: quit = true; break;
        case CmdSoundControl: Show(SoundControl(current)); break;
        case CmdDisplaySettings: Show(DisplaySettings(current)); break;
        case CmdCriticalEvents: Show(CriticalEvents(current)); break;
        case CdPlayer::Id: Show(std::make_unique<CdPlayer>(res, settings.colours, CmdMainMenu, music, files)); break;
        case Commands::Back:
            // Escape goes back to the menu this one was opened from; the main menu itself ignores it
            if (current != CmdMainMenu) Execute({ screen->Previous() ? screen->Previous() : CmdMainMenu, 0 });
            break;
        default: Show(NotRebuilt(command.id, current == CmdOptions ? CmdOptions : CmdMainMenu)); break;
        }
    }

    void FrontEnd::Update()
    {
        if (screen) screen->Update();
    }

    void FrontEnd::Draw(Canvas& canvas) const
    {
        canvas.Clear(0);
        if (!screen) return;
        canvas.DrawSprite(screen->Background(), 0, 0, 0);
        screen->Draw(canvas);
    }

    void FrontEnd::OnKey(int key)
    {
        if (!screen) return;
        if (auto command = screen->OnKey(key)) Execute(*command);
    }

    void FrontEnd::OnMouseMove(int x, int y)
    {
        if (screen) screen->OnMouseMove(x, y);
    }

    void FrontEnd::OnMouseButton(int x, int y, bool down)
    {
        if (!screen) return;
        if (auto command = screen->OnMouseButton(x, y, down)) Execute(*command);
    }
}
