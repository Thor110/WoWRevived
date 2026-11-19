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
        constexpr int CmdCdPlayer = 0x803E;
        constexpr int CmdNetwork = 0xA0;
        constexpr int CmdSinglePlayer = 0xA4;

        constexpr int BackgroundGType = 0x36F; // drawn by the "Fullscreen View" behind every menu (0x4061B0)
    }

    FrontEnd::FrontEnd(Resources& r, FrontEndSettings s, const GameFiles& f) : res(r), settings(s), files(f)
    {
        Show(MainMenu());
    }

    void FrontEnd::Show(std::unique_ptr<Menu> next) { menu = std::move(next); }

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

    std::unique_ptr<Menu> FrontEnd::MainMenu()
    {
        auto m = std::make_unique<Menu>(CmdMainMenu, res);
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
        m->AddButton(CmdCdPlayer, 0x402);
        m->AddBottomButton(CmdQuitMenu, 0x403);
        return m;
    }

    std::unique_ptr<Menu> FrontEnd::GameOptions(int previous)
    {
        auto m = std::make_unique<Menu>(CmdOptions, res);
        m->AddHeading(0x401);
        m->AddButton(0x3EF, 0x410);   // Display Settings
        m->AddButton(0x3ED, 0x411);
        m->AddButton(0x8117, 0x412);
        m->AddButton(0x80CA, 0x534);
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x40F);
        return m;
    }

    std::unique_ptr<Menu> FrontEnd::QuitGame(int previous)
    {
        auto m = std::make_unique<Menu>(CmdQuitGame, res);
        m->AddHeading(previous == CmdMainMenu ? 0x403 : 0x404);
        m->AddButton(CmdQuitGame, 0x445);
        m->AddBottomButton(previous, previous == CmdMainMenu ? 0x530 : 0x446);
        return m;
    }

    std::unique_ptr<Menu> FrontEnd::NotRebuilt(int command)
    {
        auto m = std::make_unique<Menu>(command, res);
        char text[96];
        std::snprintf(text, sizeof(text), "Not rebuilt yet\n(command 0x%X)", command);
        m->AddHeadingText(text);
        m->AddBottomButton(CmdMainMenu, 0x530);
        return m;
    }

    void FrontEnd::Execute(int command)
    {
        int current = menu ? menu->Id() : CmdMainMenu;
        SDL_Log("Command 0x%X from menu 0x%X", command, current);
        switch (command)
        {
        case CmdMainMenu: Show(MainMenu()); break;
        case CmdOptions: Show(GameOptions(current)); break;
        case CmdQuitMenu: Show(QuitGame(current)); break;
        case CmdQuitGame: quit = true; break;
        case Command::Back:
            // Escape on a sub-menu goes back to the main menu; the main menu itself ignores it
            if (current != CmdMainMenu) Show(MainMenu());
            break;
        default: Show(NotRebuilt(command)); break;
        }
    }

    void FrontEnd::Draw(Canvas& canvas) const
    {
        canvas.Clear(0);
        canvas.DrawSprite(BackgroundGType, 0, 0, 0);
        if (menu) menu->Draw(canvas);
    }

    void FrontEnd::OnKey(int key)
    {
        if (!menu) return;
        if (auto command = menu->OnKey(key)) Execute(*command);
    }

    void FrontEnd::OnMouseMove(int x, int y)
    {
        if (menu) menu->OnMouseMove(x, y);
    }

    void FrontEnd::OnMouseButton(int x, int y, bool down)
    {
        if (!menu) return;
        if (auto command = menu->OnMouseButton(x, y, down)) Execute(*command);
    }
}
