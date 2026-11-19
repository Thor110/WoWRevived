// WoW Engine - an SDL3 rebuild of the engine from Jeff Wayne's 'The War Of The Worlds' (Rage Software, 1998).
// Needs the original game files, which the installer copies from the two discs.
#include "core/Bmp.h"
#include "core/GameFiles.h"
#include "core/Vfs.h"
#include "frontend/Canvas.h"
#include "frontend/FrontEnd.h"
#include "frontend/Resources.h"
#include "frontend/Settings.h"
#include "platform/Video.h"

// IniStore.h is shared with the patch DLLs (WoWPatches/Common), so the engine reads the same WoWRevived.ini.
// It includes <windows.h> on Windows, so keep that header's macros to a minimum.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#include "IniStore.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr int GameWidth = 640;
    constexpr int GameHeight = 480;
    constexpr uint64_t TickNs = 1'000'000'000ull / 30; // "Game Frequency" defaults to 30 updates a second

    // Command line: [game folder] [--open command] [--screenshot file.bmp [keys]]
    // --open starts on the screen a menu command opens (0x8E Game Options, 0x6E Save Game...).
    // --ticks N runs N game updates (30 a second) after the keys, before the screenshot.
    // --screenshot presses the comma separated keys (up, down, enter, escape), saves the frame and exits.
    struct Options
    {
        fs::path gameFolder;
        fs::path screenshot;
        std::vector<int> keys;
        int open = 0;
        int ticks = 0;
    };

    Options ParseOptions(int argc, char** argv)
    {
        Options o;
        for (int i = 1; i < argc; i++)
        {
            std::string arg = argv[i];
            if (arg == "--open" && i + 1 < argc) o.open = int(std::strtol(argv[++i], nullptr, 0));
            else if (arg == "--ticks" && i + 1 < argc) o.ticks = std::atoi(argv[++i]);
            else if (arg == "--screenshot" && i + 1 < argc)
            {
                o.screenshot = argv[++i];
                if (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    std::string keys = argv[++i];
                    size_t start = 0;
                    while (start <= keys.size())
                    {
                        size_t end = keys.find(',', start);
                        std::string k = keys.substr(start, end == std::string::npos ? std::string::npos : end - start);
                        if (k == "up") o.keys.push_back(SDLK_UP);
                        else if (k == "down") o.keys.push_back(SDLK_DOWN);
                        else if (k == "enter") o.keys.push_back(SDLK_RETURN);
                        else if (k == "escape") o.keys.push_back(SDLK_ESCAPE);
                        else if (k == "left") o.keys.push_back(SDLK_LEFT);
                        else if (k == "right") o.keys.push_back(SDLK_RIGHT);
                        else if (k.size() > 1 && k[0] == '=') // =text types the letters
                            for (char c : k.substr(1)) if (c >= 'a' && c <= 'z') o.keys.push_back(int(SDLK_A + (c - 'a')));
                        if (end == std::string::npos) break;
                        start = end + 1;
                    }
                }
            }
            else o.gameFolder = arg;
        }
        return o;
    }

    std::vector<fs::path> CandidateFolders(const Options& options)
    {
        std::vector<fs::path> folders;
        if (!options.gameFolder.empty()) folders.push_back(options.gameFolder);
        std::error_code ec;
        folders.push_back(fs::current_path(ec));
        if (const char* base = SDL_GetBasePath())
        {
            folders.emplace_back(base);
            folders.push_back(fs::path(base) / "game");
        }
        return folders;
    }

    wow::FrontEndSettings ReadSettings(const wow::GameFiles& files)
    {
        wow::FrontEndSettings settings;
        wow::IniStore ini;
        auto path = files.Resolve(wow::IniStore::FileName());
        if (path && ini.Load(path->string()))
        {
            std::string value;
            if (ini.Get(wow::IniStore::RootSection(), "Enable Network Version", value)) settings.networkEnabled = std::atoi(value.c_str()) != 0;
            if (ini.Get("Colours", "Menu Heading", value)) settings.colours.heading = std::atoi(value.c_str());
            if (ini.Get("Colours", "Menu Items", value)) settings.colours.items = std::atoi(value.c_str());
            if (ini.Get("Colours", "Menu Static Text", value)) settings.colours.staticText = std::atoi(value.c_str());
            if (ini.Get("Tweak", "Text Line Spacing", value)) settings.colours.lineSpacing = std::atoi(value.c_str());
        }
        return settings;
    }

    void Fail(const std::string& message)
    {
        SDL_Log("%s", message.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "WoW Engine", message.c_str(), nullptr);
    }
}

int main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    Options options = ParseOptions(argc, argv);
    auto files = wow::GameFiles::Locate(CandidateFolders(options));
    if (!files)
    {
        Fail("The original game files weren't found.\n\n"
             "Run the installer to copy them from the Human and Martian discs, "
             "or start the engine with the game folder as its first argument.");
        SDL_Quit();
        return 1;
    }
    SDL_Log("Game files: %s", files->Root().string().c_str());

    int exitCode = 0;
    try
    {
        // the side is picked the same way as the game (0x402630): Human.cd present means a Human game
        bool human = files->Resolve("Human.cd").has_value();
        wow::Vfs vfs(*files);
        wow::Resources resources(vfs, human);
        wow::Settings ini(vfs.Files());
        wow::FrontEnd frontEnd(resources, ReadSettings(*files), ini, vfs.Files());

        wow::Video video;
        std::string error;
        if (!video.Open("Jeff Wayne's 'The War Of The Worlds'", GameWidth, GameHeight, false, error)) throw std::runtime_error(error);
        wow::Canvas canvas(video.Pixels(), video.Width(), video.Height(), resources);

        if (options.open) frontEnd.Open(options.open);
        bool done = false;
        if (!options.screenshot.empty())
        {
            for (int key : options.keys) frontEnd.OnKey(key);
            for (int t = 0; t < options.ticks; t++) frontEnd.Update();
            frontEnd.Draw(canvas);
            wow::WriteBmp(options.screenshot, video.Pixels(), video.Width(), video.Height());
            SDL_Log("Saved %s", options.screenshot.string().c_str());
            done = true;
        }

        // fixed rate game updates, drawing as often as the display allows
        uint64_t previous = SDL_GetTicksNS();
        uint64_t lag = 0;
        while (!done && !frontEnd.QuitRequested())
        {
            SDL_Event event;
            bool closed = false;
            while (SDL_PollEvent(&event))
            {
                int x = 0, y = 0;
                switch (event.type)
                {
                case SDL_EVENT_QUIT: closed = true; break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_RETURN && (event.key.mod & SDL_KMOD_ALT)) { if (!event.key.repeat) video.ToggleFullScreen(); }
                    else frontEnd.OnKey(int(event.key.key));
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    if (video.ToGame(event.motion.x, event.motion.y, x, y)) frontEnd.OnMouseMove(x, y);
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (event.button.button == SDL_BUTTON_LEFT && video.ToGame(event.button.x, event.button.y, x, y))
                        frontEnd.OnMouseButton(x, y, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                    break;
                default: break;
                }
            }
            if (closed) break;

            uint64_t now = SDL_GetTicksNS();
            lag += now - previous;
            previous = now;
            if (lag > TickNs * 5) lag = TickNs * 5; // don't try to catch up after a long stall
            while (lag >= TickNs)
            {
                frontEnd.Update();
                lag -= TickNs;
            }

            frontEnd.Draw(canvas);
            video.Present();
        }
    }
    catch (const std::exception& ex)
    {
        Fail(std::string("The engine stopped: ") + ex.what());
        exitCode = 1;
    }
    SDL_Quit();
    return exitCode;
}
