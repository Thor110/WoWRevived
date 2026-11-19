#include "frontend/FrontEnd.h"

#include <SDL3/SDL_log.h>

#include <cstdio>

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

        // The menus' "Fullscreen View" (0x4061B0): GType 0x36F (humanbd.spr or martbd.spr) with palette group
        // 0x2F6 (F6) in Human games or 0x2F7 (F7) in Martian games.
        constexpr int MenuBackdrop = 0x36F;
    }

    FrontEnd::FrontEnd(Resources& r, FrontEndSettings s, const GameFiles& f) : res(r), settings(s), files(f)
    {
        Show(MainMenu());
    }

    void FrontEnd::Show(std::unique_ptr<Screen> next)
    {
        screen = std::move(next);
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
        case CdPlayer::Id: Show(std::make_unique<CdPlayer>(res, settings.colours, CmdMainMenu, music, files)); break;
        case Commands::Back:
            // Escape on a sub-menu goes back to the main menu; the main menu itself ignores it
            if (current != CmdMainMenu) Show(MainMenu());
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
