// WoW Engine - an SDL3 rebuild of the engine from Jeff Wayne's 'The War Of The Worlds' (Rage Software, 1998).
// Needs the original game files, which the installer copies from the two discs.
#include "Version.h"
#include "platform/Launch.h"
#include "core/Bmp.h"
#include "core/GameFiles.h"
#include "core/Vfs.h"
#include "frontend/Canvas.h"
#include "core/Random.h"
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
#include <cstdio>
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

    // Command line: [game folder] [--human | --martian] [--open command] [--screenshot file.bmp [keys]]
    // --human / --martian pick the side (WoWLauncher passes one); without either it's Human.
    // --open starts on the screen a menu command opens (0x8E Game Options, 0x6E Save Game...).
    // --ticks N runs N game updates (30 a second) after the keys, before the screenshot.
    // --mouse x,y moves the mouse there first (game coordinates), for screenshots of the cursor and highlights.
    // --screenshot presses the comma separated keys (up, down, enter, escape), saves the frame and exits.
    struct Options
    {
        fs::path gameFolder;
        fs::path screenshot;
        std::vector<int> keys;
        int open = 0;
        int ticks = 0;
        int mouseX = -1, mouseY = -1;
        int side = -1; // 1 Human, 0 Martian, -1 not given
    };

    Options ParseOptions(int argc, char** argv)
    {
        Options o;
        for (int i = 1; i < argc; i++)
        {
            std::string arg = argv[i];
            if (arg == "--human") o.side = 1;
            else if (arg == "--martian") o.side = 0;
            else if (arg == "--open" && i + 1 < argc) o.open = int(std::strtol(argv[++i], nullptr, 0));
            else if (arg == "--ticks" && i + 1 < argc) o.ticks = std::atoi(argv[++i]);
            else if (arg == "--mouse" && i + 1 < argc)
            {
                std::string at = argv[++i];
                o.mouseX = std::atoi(at.c_str());
                if (auto comma = at.find(','); comma != std::string::npos) o.mouseY = std::atoi(at.c_str() + comma + 1);
            }
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
                        else if (k == "backspace") o.keys.push_back(SDLK_BACKSPACE);
                        else if (k == "click") o.keys.push_back(-1); // a left click where --mouse put the mouse
                        else if (k.rfind("click:", 0) == 0)   // click:x:y clicks there
                        {
                            int cx = std::atoi(k.c_str() + 6), cy = 0;
                            if (auto colon = k.find(':', 6); colon != std::string::npos) cy = std::atoi(k.c_str() + colon + 1);
                            o.keys.push_back(-2 - (cx << 12 | cy)); // packed into a negative "key"
                        }
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
            if (ini.Get("Colours", "Menu Glow First", value)) settings.colours.glowFirst = std::atoi(value.c_str());
            if (ini.Get("Colours", "Menu Glow Last", value)) settings.colours.glowLast = std::atoi(value.c_str());
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

namespace
{
    // Everything SDL_Log says also goes to WoWEngine.log next to the engine (a Windows build has no console to see
    // it in), started afresh each run.
    std::FILE* logFile = nullptr;
    SDL_LogOutputFunction defaultLog = nullptr;
    void* defaultLogData = nullptr;
    void LogToFile(void* userdata, int category, SDL_LogPriority priority, const char* message)
    {
        if (logFile) { std::fprintf(logFile, "%8.3f %s\n", double(SDL_GetTicks()) / 1000.0, message); std::fflush(logFile); }
        if (defaultLog) defaultLog(defaultLogData, category, priority, message);
        (void)userdata;
    }
    void StartLogFile()
    {
        std::string path = std::string(SDL_GetBasePath() ? SDL_GetBasePath() : "") + "WoWEngine.log";
        logFile = std::fopen(path.c_str(), "w");
        SDL_GetLogOutputFunction(&defaultLog, &defaultLogData);
        SDL_SetLogOutputFunction(LogToFile, nullptr);
    }
}

int main(int argc, char** argv)
{
    StartLogFile();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    // srand(timeGetTime()) and the table of 1024 rand() values (WinMain 0x401AE8, WGame 0x402630)
    wow::random::Seed(uint32_t(SDL_GetTicks()));
    wow::random::FillTable();

    Options options = ParseOptions(argc, argv);
    auto files = wow::GameFiles::Locate(CandidateFolders(options));
    if (!files)
    {
        // offer to run WoWLauncher from the same folder
        std::string installer = std::string(SDL_GetBasePath() ? SDL_GetBasePath() : "") +
#ifdef _WIN32
            "WoWLauncher.exe";
#else
            "WoWLauncher";
#endif
        std::error_code ec;
        bool haveInstaller = fs::exists(installer, ec);
        const SDL_MessageBoxButtonData buttons[] = {
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Install from the discs" },
            { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit" },
        };
        SDL_MessageBoxData box{};
        box.flags = SDL_MESSAGEBOX_WARNING;
        box.title = "WoW Engine " WOWENGINE_VERSION;
        box.message = "The original game files weren't found.\n\n"
                      "Install them from the Human and Martian discs (or their images) with WoWLauncher, "
                      "or start the engine with the game folder as its first argument.";
        box.numbuttons = haveInstaller ? 2 : 1;
        box.buttons = haveInstaller ? buttons : buttons + 1;
        int choice = 0;
        SDL_ShowMessageBox(&box, &choice);
        if (choice == 1)
        {
            if (!wow::LaunchDetached({ installer }))
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "WoW Engine " WOWENGINE_VERSION, SDL_GetError(), nullptr);
        }
        SDL_Quit();
        return 1;
    }
    SDL_Log("Game files: %s", files->Root().string().c_str());

    int exitCode = 0;
    try
    {
        // the side: as WoWLauncher says (--human / --martian), Human without either. (The game itself, 0x402630,
        // looks for Human.cd; the install doesn't copy the side markers.)
        bool human = options.side != 0;
        wow::Vfs vfs(*files);
        wow::Resources resources(vfs, human);
        wow::Settings ini(vfs.Files());
        wow::FrontEnd frontEnd(resources, ReadSettings(*files), ini, vfs.Files());

        wow::Video video;
        std::string error;
        if (!video.Open("Jeff Wayne's 'The War Of The Worlds' - WoW Engine v" WOWENGINE_VERSION, GameWidth, GameHeight, false, error))
            throw std::runtime_error(error);
        wow::Canvas canvas(video.Pixels(), video.Width(), video.Height(), resources);

        // typed text for the campaign name field (Edit Campaign)
        SDL_StartTextInput(video.Window());
        // the game draws its own cursor over the picture
        if (frontEnd.GameCursor()) SDL_HideCursor();
        if (options.open) frontEnd.Open(options.open);
        bool done = false;
        if (!options.screenshot.empty())
        {
            if (options.mouseX >= 0) frontEnd.OnMouseMove(options.mouseX, options.mouseY);
            for (int key : options.keys)
            {
                if (key == -1) { frontEnd.OnMouseButton(options.mouseX, options.mouseY, true); frontEnd.OnMouseButton(options.mouseX, options.mouseY, false); continue; }
                if (key <= -2)
                {
                    int packed = -2 - key, cx = packed >> 12, cy = packed & 0xFFF;
                    frontEnd.OnMouseMove(cx, cy);
                    frontEnd.OnMouseButton(cx, cy, true);
                    frontEnd.OnMouseButton(cx, cy, false);
                    continue;
                }
                frontEnd.OnKey(key);
                // letters are typed too, as SDL's text input would send them
                if (key >= int(SDLK_A) && key <= int(SDLK_Z)) { char text[2] = { char('a' + (key - int(SDLK_A))), 0 }; frontEnd.OnText(text); }
            }
            for (int t = 0; t < options.ticks; t++) frontEnd.Update();
            frontEnd.Draw(canvas);
            wow::WriteBmp(options.screenshot, video.Pixels(), video.Width(), video.Height());
            SDL_Log("Saved %s", options.screenshot.string().c_str());
            done = true;
        }

        // fixed rate game updates, drawing as often as the display allows
        uint64_t previous = SDL_GetTicksNS();
        uint64_t lag = 0;
        // byte_4EA3E0: the game is active; the window losing the focus or the Pause key stops it (0x42CD40)
        bool active = true;
        auto setActive = [&](bool on) {
            if (on == active) return;
            active = on;
            frontEnd.SetPaused(!on);
            SDL_Log("OnActivateApp %s", on ? "On" : "Off");
        };
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
                    else if (event.key.key == SDLK_PAUSE) { if (!event.key.repeat) setActive(!active); }
                    else if (active) frontEnd.OnKey(int(event.key.key));
                    break;
                case SDL_EVENT_WINDOW_FOCUS_LOST: setActive(false); break;
                case SDL_EVENT_WINDOW_FOCUS_GAINED: setActive(true); break;
                case SDL_EVENT_KEY_UP: frontEnd.OnKeyUp(int(event.key.key)); break;
                case SDL_EVENT_TEXT_INPUT: if (active) frontEnd.OnText(event.text.text); break;
                case SDL_EVENT_MOUSE_MOTION:
                    if (video.ToGame(event.motion.x, event.motion.y, x, y)) frontEnd.OnMouseMove(x, y);
                    else frontEnd.OnMouseLeave();
                    break;
                case SDL_EVENT_WINDOW_MOUSE_LEAVE: frontEnd.OnMouseLeave();
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                {
                    // a press counts only inside the game's picture and while the game is active; a release always
                    // does (off the picture it's at -1, -1, over nothing), so nothing is left held
                    bool down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                    bool inside = video.ToGame(event.button.x, event.button.y, x, y);
                    if (!inside) x = y = -1;
                    if (down && (!inside || !active)) break;
                    if (event.button.button == SDL_BUTTON_LEFT) frontEnd.OnMouseButton(x, y, down);
                    else if (event.button.button == SDL_BUTTON_RIGHT) frontEnd.OnRightButton(x, y, down);
                }
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
                if (active) frontEnd.Update();
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
