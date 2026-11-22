// WoW Engine - an SDL3 rebuild of the engine from Jeff Wayne's 'The War Of The Worlds' (Rage Software, 1998).
// Needs the original game files, which the installer copies from the two discs.
#include "Version.h"
#include "platform/Launch.h"
#include "core/Bmp.h"
#include "core/GameFiles.h"
#include "core/Vfs.h"
#include "frontend/Canvas.h"
#include "core/Random.h"
#include "core/Crumb.h"
#include "core/Playback.h"
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
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <exception>
#include <thread>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr int GameWidth = 640;
    constexpr int GameHeight = 480;
    // the timer's period (0x401CAA-0x401CBD): 1000 / "Game Frequency" ms in whole numbers, 33 ms at 30
    uint64_t TickNs = 33'000'000ull;
    constexpr int WaitKey = -100'000'000; // a "wait:n" in the screenshot keys, less n
    constexpr int MoveKey = -50'000'000;  // a "move:x:y", less x << 12 | y
    constexpr int DownKey = -60'000'000;  // "down:x:y"
    constexpr int UpKey = -70'000'000;    // "up:x:y"
    constexpr int RightDownKey = -80'000'000; // "rdown:x:y", the right button
    constexpr int RightUpKey = -90'000'000;   // "rup:x:y" // "Game Frequency" defaults to 30 updates a second

    // Command line: [game folder] [--human | --martian] [--open command] [--screenshot file.bmp [keys]]
    // --human / --martian pick the side (WoWLauncher passes one); without either it's Human.
    // --open starts on the screen a menu command opens (0x8E Game Options, 0x6E Save Game...).
    // --ticks N runs N game updates (30 a second) after the keys, before the screenshot.
    // --mouse x,y moves the mouse there first (game coordinates), for screenshots of the cursor and highlights.
    // --screenshot presses the comma separated keys (up, down, enter, escape, f1-f9, 0-9, click:x:y, move:x:y, down:x:y, up:x:y, rdown:x:y, rup:x:y, wait:n updates), saves the frame and exits.
    struct Options
    {
        fs::path gameFolder;
        fs::path screenshot;
        std::vector<int> keys;
        int open = 0;
        int openParam = 0;   // --load: the slot
        int then = 0;        // --then: a command after the ticks (before the screenshot)
        int ticks = 0;
        int mouseX = -1, mouseY = -1;
        int side = -1; // 1 Human, 0 Martian, -1 not given
        bool record = false, playback = false;  // --record / --playback: the game's playback recordings (core/Playback.h)
        bool noLoose = false;                   // --no-loose: the engine's own DAT and MAPS files aren't used (the game's files only)
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
            else if (arg == "--record") o.record = true;
            else if (arg == "--no-loose") o.noLoose = true;
            else if (arg == "--playback") o.playback = true;
            else if (arg == "--load" && i + 1 < argc) { o.open = 0x80B3; o.openParam = std::atoi(argv[++i]); }
            else if (arg == "--then" && i + 1 < argc) o.then = int(std::strtol(argv[++i], nullptr, 0));
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
                        else if (k == "home") o.keys.push_back(SDLK_HOME);
                        else if (k == "plus") o.keys.push_back(SDLK_EQUALS);
                        else if (k == "minus") o.keys.push_back(SDLK_MINUS);
                        // shift / ctrl hold that key for the steps after it, nomods lets go
                        else if (k == "shift") o.keys.push_back(SDLK_LSHIFT);
                        else if (k == "ctrl") o.keys.push_back(SDLK_LCTRL);
                        else if (k == "nomods") o.keys.push_back(SDLK_LALT);
                        else if (k == "comma") o.keys.push_back(SDLK_COMMA);
                        else if (k == "period") o.keys.push_back(SDLK_PERIOD);
                        else if (k.size() == 2 && k[0] == 'f' && k[1] >= '1' && k[1] <= '9') o.keys.push_back(int(SDLK_F1) + (k[1] - '1'));
                        else if (k.size() == 1 && k[0] >= '0' && k[0] <= '9') o.keys.push_back(int(SDLK_0) + (k[0] - '0'));
                        else if (k == "click") o.keys.push_back(-1); // a left click where --mouse put the mouse
                        else if (k.rfind("wait:", 0) == 0)   // wait:n runs n updates
                            o.keys.push_back(WaitKey - std::max(0, std::atoi(k.c_str() + 5)));
                        else if (k.rfind("move:", 0) == 0 || k.rfind("down:", 0) == 0 || k.rfind("up:", 0) == 0 ||
                                 k.rfind("rdown:", 0) == 0 || k.rfind("rup:", 0) == 0)
                        {
                            // move:x:y moves the mouse there; down:x:y and up:x:y press and let go the left button there
                            size_t at = k.find(':') + 1;
                            int mx = std::atoi(k.c_str() + at), my = 0;
                            if (auto colon = k.find(':', at); colon != std::string::npos) my = std::atoi(k.c_str() + colon + 1);
                            int base = k[0] == 'm' ? MoveKey : k[0] == 'd' ? DownKey : k[0] == 'u' ? UpKey : k[1] == 'd' ? RightDownKey : RightUpKey;
                            o.keys.push_back(base - (mx << 12 | my));
                        }
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
            if (ini.Get(wow::IniStore::RootSection(), "Game Frequency", value)) settings.gameFrequency = std::max(1, std::atoi(value.c_str()));
            if (ini.Get("Colours", "Menu Heading", value)) settings.colours.heading = std::atoi(value.c_str());
            if (ini.Get("Colours", "Menu Items", value)) settings.colours.items = std::atoi(value.c_str());
            if (ini.Get("Colours", "Menu Static Text", value)) settings.colours.staticText = std::atoi(value.c_str());
            if (ini.Get("Colours", "Menu Glow First", value)) settings.colours.glowFirst = std::atoi(value.c_str());
            if (ini.Get("Colours", "Menu Glow Last", value)) settings.colours.glowLast = std::atoi(value.c_str());
            if (ini.Get("Tweak", "Text Line Spacing", value)) settings.colours.lineSpacing = std::atoi(value.c_str());
            if (ini.Get("Colours", "Message Text", value)) settings.colours.messageText = std::atoi(value.c_str());
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
    // it in), started afresh each run (the run before's kept as WoWEngine.previous.log).
    std::FILE* logFile = nullptr;
    SDL_LogOutputFunction defaultLog = nullptr;
    void* defaultLogData = nullptr;
    void LogToFile(void* userdata, int category, SDL_LogPriority priority, const char* message)
    {
        if (logFile) { std::fprintf(logFile, "%8.3f %s\n", double(SDL_GetTicks()) / 1000.0, message); std::fflush(logFile); }
        if (defaultLog) defaultLog(defaultLogData, category, priority, message);
        (void)userdata;
    }
    // A fault that isn't a C++ exception (an access violation, a stack overflow, an abort) closes the window with
    // nothing said. These put a line in WoWEngine.log first: what it was, where, and what the engine was doing
    // (wow::crumb), then let the fault go on as it would have.
#ifdef _WIN32
    LONG WINAPI CrashFilter(EXCEPTION_POINTERS* e)
    {
        if (logFile && e && e->ExceptionRecord)
        {
            const EXCEPTION_RECORD& r = *e->ExceptionRecord;
            const auto base = reinterpret_cast<unsigned long long>(GetModuleHandleW(nullptr));
            const auto at = reinterpret_cast<unsigned long long>(r.ExceptionAddress);
            std::fprintf(logFile, "CRASH: exception 0x%08lX at 0x%llX (the engine's base 0x%llX, offset 0x%llX) while: %s\n",
                         static_cast<unsigned long>(r.ExceptionCode), at, base, at - base, wow::crumb.load());
            if (r.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r.NumberParameters >= 2)
                std::fprintf(logFile, "CRASH: %s address 0x%llX\n", r.ExceptionInformation[0] ? "writing" : "reading",
                             static_cast<unsigned long long>(r.ExceptionInformation[1]));
            if (r.ExceptionCode == EXCEPTION_STACK_OVERFLOW) std::fprintf(logFile, "CRASH: the stack ran out (a routine calling itself without end)\n");
            std::fflush(logFile);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
#else
    void CrashSignal(int sig)
    {
        if (logFile) { std::fprintf(logFile, "CRASH: signal %d while: %s\n", sig, wow::crumb.load()); std::fflush(logFile); }
        std::signal(sig, SIG_DFL);
        std::raise(sig);
    }
#endif
    // A recording keeps keys as the window's virtual keys (wParam of 0x100 / 0x101). The keys the game uses, both
    // ways; any other key is kept as its SDL code with the top bit set, which no virtual key has.
    struct KeyPair { int sdl; uint32_t vk; };
    constexpr KeyPair KeyPairs[] = {
        { SDLK_RETURN, 0x0D }, { SDLK_ESCAPE, 0x1B }, { SDLK_BACKSPACE, 0x08 }, { SDLK_TAB, 0x09 }, { SDLK_SPACE, 0x20 },
        { SDLK_LEFT, 0x25 }, { SDLK_UP, 0x26 }, { SDLK_RIGHT, 0x27 }, { SDLK_DOWN, 0x28 }, { SDLK_HOME, 0x24 }, { SDLK_END, 0x23 },
        { SDLK_PAGEUP, 0x21 }, { SDLK_PAGEDOWN, 0x22 }, { SDLK_INSERT, 0x2D }, { SDLK_DELETE, 0x2E }, { SDLK_PAUSE, 0x13 },
        { SDLK_LSHIFT, 0x10 }, { SDLK_RSHIFT, 0x10 }, { SDLK_LCTRL, 0x11 }, { SDLK_RCTRL, 0x11 }, { SDLK_LALT, 0x12 }, { SDLK_RALT, 0x12 },
        { SDLK_COMMA, 0xBC }, { SDLK_PERIOD, 0xBE }, { SDLK_MINUS, 0xBD }, { SDLK_EQUALS, 0xBB }, { SDLK_KP_PLUS, 0x6B }, { SDLK_KP_MINUS, 0x6D },
    };
    uint32_t VirtualFromKey(int key)
    {
        if (key >= int(SDLK_A) && key <= int(SDLK_Z)) return uint32_t('A' + (key - int(SDLK_A)));
        if (key >= int(SDLK_0) && key <= int(SDLK_9)) return uint32_t('0' + (key - int(SDLK_0)));
        if (key >= int(SDLK_F1) && key <= int(SDLK_F12)) return uint32_t(0x70 + (key - int(SDLK_F1)));
        for (const KeyPair& p : KeyPairs) if (p.sdl == key) return p.vk;
        return uint32_t(key) | 0x80000000u;
    }
    int KeyFromVirtual(uint32_t vk)
    {
        if (vk & 0x80000000u) return int(vk & 0x7FFFFFFFu);
        if (vk >= 'A' && vk <= 'Z') return int(SDLK_A) + int(vk - 'A');
        if (vk >= '0' && vk <= '9') return int(SDLK_0) + int(vk - '0');
        if (vk >= 0x70 && vk <= 0x7B) return int(SDLK_F1) + int(vk - 0x70);
        for (const KeyPair& p : KeyPairs) if (p.vk == vk) return p.sdl;
        return 0;
    }

    // The watchdog: a thread that looks at the main loop's heartbeat once a second. Five seconds with no pass puts a
    // STALL line in the log with what the engine was doing; a pass after that, a line saying how long it was.
    void Watch()
    {
        unsigned last = wow::heartbeat.load();
        int still = 0;
        bool said = false;
        for (;;)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            const unsigned now = wow::heartbeat.load();
            if (now != last)
            {
                if (said && logFile) { std::fprintf(logFile, "%8.3f STALL over after %d s\n", double(SDL_GetTicks()) / 1000.0, still); std::fflush(logFile); }
                last = now;
                still = 0;
                said = false;
                continue;
            }
            if (++still == 5 && logFile)
            {
                std::fprintf(logFile, "%8.3f STALL: the main loop hasn't run for 5 s, while: %s\n", double(SDL_GetTicks()) / 1000.0, wow::crumb.load());
                std::fflush(logFile);
                said = true;
            }
        }
    }
    void StartLogFile()
    {
        std::string path = std::string(SDL_GetBasePath() ? SDL_GetBasePath() : "") + "WoWEngine.log";
        // the run before's log is kept as WoWEngine.previous.log: a player who starts the game again after a crash
        // still has the log that says what happened
        {
            std::error_code ec;
            if (fs::exists(path, ec)) fs::rename(path, std::string(SDL_GetBasePath() ? SDL_GetBasePath() : "") + "WoWEngine.previous.log", ec);
        }
        logFile = std::fopen(path.c_str(), "w");
        SDL_GetLogOutputFunction(&defaultLog, &defaultLogData);
        SDL_SetLogOutputFunction(LogToFile, nullptr);
#ifdef _WIN32
        SetUnhandledExceptionFilter(CrashFilter);
        ULONG reserve = 64 * 1024;          // room for the filter to run when the stack itself has run out
        SetThreadStackGuarantee(&reserve);
#else
        for (int sig : { SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS }) std::signal(sig, CrashSignal);
#endif
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
    // (the engine's own: with a recording being made or played the random numbers start from 1, as the world's do
    // (0x417DA3), so that what was recorded meets the same game when it's played)
    if (options.record || options.playback)
    {
        wow::random::Seed(1);
        wow::random::FillTable();
    }
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
    // MODERN (docs/MODERN.md "Files"): the engine's own loose files, in "dat" and "maps" beside the program (a
    // release) or in the source folder it was built from (a build run where it was made), are used before the
    // game's. --no-loose leaves them out.
    if (!options.noLoose)
    {
        std::vector<fs::path> places;
        if (const char* base = SDL_GetBasePath()) places.emplace_back(base);
#ifdef WOWENGINE_SOURCE_DIR
        places.emplace_back(WOWENGINE_SOURCE_DIR);
#endif
        for (const fs::path& place : places)
        {
            std::error_code ec;
            if (fs::equivalent(place, files->Root(), ec)) break;      // the game is installed there: its DAT is the one
            if (!fs::is_directory(place / "dat", ec) && !fs::is_directory(place / "maps", ec) &&
                !fs::is_directory(place / "DAT", ec) && !fs::is_directory(place / "MAPS", ec)) continue;
            files->AddOverlay(place);
            SDL_Log("Loose files: %s", place.string().c_str());
            break;
        }
    }

    int exitCode = 0;
    try
    {
        // the side: as WoWLauncher says (--human / --martian), Human without either. (The game itself, 0x402630,
        // looks for Human.cd; the install doesn't copy the side markers.)
        bool human = options.side != 0;
        wow::Vfs vfs(*files);
        wow::Resources resources(vfs, human);
        wow::Settings ini(vfs.Files());
        const wow::FrontEndSettings settings = ReadSettings(*files);
        TickNs = uint64_t(1000 / settings.gameFrequency) * 1'000'000ull;
        wow::FrontEnd frontEnd(resources, settings, ini, vfs.Files());

        // MODERN FIX: the first start takes the desktop's resolution (docs/MODERN.md "Resolution"). With no [Screen]
        // "Size" yet, the view size is worked out from the desktop's and kept; after that the setting is the
        // player's. (Not when a screenshot is asked for: those runs leave the settings alone.)
        if (options.screenshot.empty() && ini.GetString("Screen", "Size", "").empty())
        {
            const SDL_DisplayID display = SDL_GetPrimaryDisplay();
            const SDL_DisplayMode* desktop = display ? SDL_GetDesktopDisplayMode(display) : nullptr;
            int w = GameWidth, h = GameHeight;
            if (desktop) wow::FrontEnd::ViewSizeFor(desktop->w, desktop->h, w, h);
            ini.SetString("Screen", "Size", std::to_string(w) + "," + std::to_string(h));
            SDL_Log("First start: the desktop is %d x %d, the view size %d x %d", desktop ? desktop->w : 0, desktop ? desktop->h : 0, w, h);
        }
        wow::Video video;
        std::string error;
        // the picture fills the screen unless [WoW Engine] "Windowed" = 1 (Alt + Enter changes it while playing)
        const bool windowed = !options.screenshot.empty() || ini.GetBool("WoW Engine", "Windowed", false);
        if (!video.Open("Jeff Wayne's 'The War Of The Worlds' - WoW Engine v" WOWENGINE_VERSION, GameWidth, GameHeight, !windowed, error))
            throw std::runtime_error(error);
        wow::Screen::windowed = windowed;
        // MODERN: the window's size. The one the player last gave it ([WoW Engine] "Window Size", kept as the
        // window is resized); with none yet, the view size, or as much of it as the desktop has room for at the
        // same shape. Used whenever the game is in a window, at the start or after Alt + Enter.
        int setW = 0, setH = 0;         // the size the engine gave the window itself (not kept as the player's)
        auto sizeWindow = [&]() {
            if (!options.screenshot.empty()) return;
            int w = 0, h = 0;
            const std::string kept = ini.GetString("WoW Engine", "Window Size", "");
            if (std::sscanf(kept.c_str(), "%d,%d", &w, &h) != 2 || w < 320 || h < 240)
            {
                const std::string size = ini.GetString("Screen", "Size", "640,480");
                if (std::sscanf(size.c_str(), "%d,%d", &w, &h) != 2 || w < 640 || h < 480) { w = GameWidth; h = GameHeight; }
                SDL_Rect room{};
                const SDL_DisplayID display = SDL_GetDisplayForWindow(video.Window());
                if (display && SDL_GetDisplayUsableBounds(display, &room) && room.w > 0 && room.h > 0)
                {
                    // (a little less than the room, for the window's own frame)
                    const double fit = std::min({ 1.0, room.w * 0.92 / w, room.h * 0.92 / h });
                    w = int(w * fit);
                    h = int(h * fit);
                }
            }
            setW = w;
            setH = h;
            SDL_SetWindowSize(video.Window(), w, h);
            SDL_SetWindowPosition(video.Window(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        };
        if (windowed) sizeWindow();
        bool windowSized = false;       // the window has had its size set for windowed play
        windowSized = windowed;
        video.SetStretch(ini.GetBool("WoW Engine", "Stretch", false));
        wow::Canvas canvas(video.Pixels(), video.Width(), video.Height(), resources);
        // the picture is the size the front end asks for (the view size for the menus, 640 x 480 for the maps and
        // movies until their views take the size too); it changes with the screen and with the setting, at once
        bool windowSetting = frontEnd.WindowedSetting(), cursorSetting = frontEnd.GameCursor();
        auto fitPicture = [&]() {
            // the Modern Options menu's window switches, taken as they change
            if (options.screenshot.empty() && frontEnd.WindowedSetting() != windowSetting)
            {
                windowSetting = frontEnd.WindowedSetting();
                video.SetFullScreen(!windowSetting);
                wow::Screen::windowed = windowSetting;
                if (windowSetting && !windowSized) { sizeWindow(); windowSized = true; }
            }
            video.SetStretch(frontEnd.StretchSetting());
            if (frontEnd.GameCursor() != cursorSetting)
            {
                cursorSetting = frontEnd.GameCursor();
                if (cursorSetting) SDL_HideCursor(); else SDL_ShowCursor();
            }
            int w = GameWidth, h = GameHeight;
            frontEnd.ViewSize(w, h);
            if (w == video.Width() && h == video.Height()) return;
            if (video.Resize(w, h)) canvas.Rebind(video.Pixels(), video.Width(), video.Height());
        };

        // typed text for the campaign name field (Edit Campaign)
        SDL_StartTextInput(video.Window());
        // the game draws its own cursor over the picture
        if (frontEnd.GameCursor()) SDL_HideCursor();
        if (options.open) frontEnd.Open(options.open, options.openParam);
        bool done = false;
        if (!options.screenshot.empty())
        {
            if (options.mouseX >= 0) frontEnd.OnMouseMove(options.mouseX, options.mouseY);
            for (int key : options.keys)
            {
                if (key == int(SDLK_LSHIFT)) { SDL_SetModState(SDL_KMOD_LSHIFT); continue; }
                if (key == int(SDLK_LCTRL)) { SDL_SetModState(SDL_KMOD_LCTRL); continue; }
                if (key == int(SDLK_LALT)) { SDL_SetModState(SDL_KMOD_NONE); continue; }
                if (key <= MoveKey && key > WaitKey)
                {
                    int base = key <= RightUpKey ? RightUpKey : key <= RightDownKey ? RightDownKey : key <= UpKey ? UpKey : key <= DownKey ? DownKey : MoveKey;
                    int packed = base - key, px = packed >> 12, py = packed & 0xFFF;
                    frontEnd.OnMouseMove(px, py);
                    if (base == DownKey || base == UpKey) frontEnd.OnMouseButton(px, py, base == DownKey);
                    if (base == RightDownKey || base == RightUpKey) frontEnd.OnRightButton(px, py, base == RightDownKey);
                    continue;
                }
                if (key <= WaitKey) { for (int t = 0; t < WaitKey - key; t++) frontEnd.Update(); continue; }
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
            if (options.then) { frontEnd.Open(options.then); for (int t = 0; t < 10; t++) frontEnd.Update(); }
            fitPicture();
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
        std::thread(Watch).detach();    // it only reads the heartbeat and writes the log; it ends with the process
        // The playback recordings (0x42D2D0 / 0x42D520): input reaches the game as the window's messages, through
        // Record; played back, the same messages come from the file
        wow::Playback playback;
        playback.SetFiles(files->Root(), [&](const std::string& name) { return files->Resolve(name); }, frontEnd.PlaybackFiles());
        playback.SetRecording(options.record);
        playback.SetPlaying(options.playback);
        frontEnd.SetPlaybackState(options.record, options.playback);
        // the Modern Options menu's switches: a recording or a playback started there starts the random numbers
        // from 1 as the command line's do, so a recording made from the menu meets the same game when it is
        // played from the menu in the same place
        auto followMenu = [&]() {
            if (frontEnd.RecordWanted() != playback.Recording())
            {
                playback.SetRecording(frontEnd.RecordWanted());
                if (playback.Recording()) { wow::random::Seed(1); wow::random::FillTable(); }
                SDL_Log(playback.Recording() ? "Recording input to Playback.1" : "Recording stopped");
            }
            if (frontEnd.PlayWanted() != playback.Playing())
            {
                playback.SetPlaying(frontEnd.PlayWanted());
                if (playback.Playing()) { wow::random::Seed(1); wow::random::FillTable(); }
                SDL_Log(playback.Playing() ? "Playing back recorded input" : "Playback stopped");
            }
        };
        if (options.record) SDL_Log("Recording input to Playback.1");
        if (options.playback) SDL_Log("Playing back recorded input (playback.1 to playback.%d)", frontEnd.PlaybackFiles());
        auto act = [&](const wow::Playback::Message& m) {
            const int x = int(int16_t(m.lParam & 0xFFFF)), y = int(int16_t(m.lParam >> 16));
            switch (m.msg)
            {
            case 0x100: frontEnd.OnKey(KeyFromVirtual(m.wParam)); break;
            case 0x101: frontEnd.OnKeyUp(KeyFromVirtual(m.wParam)); break;
            case 0x102: { const char text[3] = { m.wParam < 0x80 ? char(m.wParam) : char(0xC0 | (m.wParam >> 6)), m.wParam < 0x80 ? char(0) : char(0x80 | (m.wParam & 0x3F)), 0 }; frontEnd.OnText(text); break; }
            case 0x200: frontEnd.OnMouseMove(x, y); break;
            case 0x201: case 0x202: frontEnd.OnMouseButton(x, y, m.msg == 0x201); break;
            case 0x204: case 0x205: frontEnd.OnRightButton(x, y, m.msg == 0x204); break;
            default: break;
            }
        };
        auto input = [&](uint32_t msg, uint32_t wParam, int x, int y) {
            const wow::Playback::Message m{ msg, wParam, uint32_t(uint16_t(x)) | uint32_t(uint16_t(y)) << 16 };
            if (playback.Record(frontEnd.Frames(), m)) act(m);
        };
        while (!done && !frontEnd.QuitRequested())
        {
            wow::Beat();
            SDL_Event event;
            bool closed = false;
            while (SDL_PollEvent(&event))
            {
                int x = 0, y = 0;
                switch (event.type)
                {
                case SDL_EVENT_QUIT: closed = true; break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_RETURN && (event.key.mod & SDL_KMOD_ALT))
                    {
                        if (!event.key.repeat)
                        {
                            video.ToggleFullScreen();
                            wow::Screen::windowed = !(SDL_GetWindowFlags(video.Window()) & SDL_WINDOW_FULLSCREEN);
                            if (wow::Screen::windowed && !windowSized) { sizeWindow(); windowSized = true; }
                        }
                    }
                    else if (event.key.key == SDLK_PAUSE) { if (!event.key.repeat) setActive(!active); }
                    // (the engine's own: Escape ends a playback; the game drops all input until the file ends)
                    else if (playback.Playing() && event.key.key == SDLK_ESCAPE) frontEnd.SetPlaybackState(playback.Recording(), false);
                    else if (active) input(0x100, VirtualFromKey(int(event.key.key)), 0, 0);
                    break;
                // MODERN: the window's size is kept as the player changes it (not while it fills the screen)
                case SDL_EVENT_WINDOW_RESIZED:
                    if (windowSized && !(SDL_GetWindowFlags(video.Window()) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) &&
                        event.window.data1 >= 320 && event.window.data2 >= 240 && (event.window.data1 != setW || event.window.data2 != setH))
                        ini.SetString("WoW Engine", "Window Size", std::to_string(event.window.data1) + "," + std::to_string(event.window.data2));
                    break;
                case SDL_EVENT_WINDOW_FOCUS_LOST: setActive(false); break;
                case SDL_EVENT_WINDOW_FOCUS_GAINED: setActive(true); break;
                case SDL_EVENT_KEY_UP: input(0x101, VirtualFromKey(int(event.key.key)), 0, 0); break;
                case SDL_EVENT_TEXT_INPUT:
                    // 0x102, a character at a time (the game's text is Latin-1; anything past it isn't recorded)
                    if (active)
                        for (const unsigned char* c = reinterpret_cast<const unsigned char*>(event.text.text); *c; c++)
                        {
                            uint32_t ch = *c;
                            if (ch >= 0xC0 && ch < 0xE0 && c[1]) { ch = (ch & 0x1F) << 6 | (c[1] & 0x3F); c++; }
                            else if (ch >= 0x80) continue;
                            if (ch < 0x100) input(0x102, ch, 0, 0);
                        }
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    if (video.ToGame(event.motion.x, event.motion.y, x, y)) input(0x200, 0, x, y);
                    else if (!playback.Playing()) frontEnd.OnMouseLeave();
                    break;
                case SDL_EVENT_WINDOW_MOUSE_LEAVE: if (!playback.Playing()) frontEnd.OnMouseLeave();
                    break;
                // MODERN: the wheel zooms the battle map (the game has no use for the wheel; it isn't recorded)
                case SDL_EVENT_MOUSE_WHEEL:
                    if (active && !playback.Playing()) frontEnd.OnWheel(event.wheel.y > 0 ? 1 : event.wheel.y < 0 ? -1 : 0);
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
                    if (event.button.button == SDL_BUTTON_LEFT) input(down ? 0x201 : 0x202, 0, x, y);
                    else if (event.button.button == SDL_BUTTON_RIGHT) input(down ? 0x204 : 0x205, 0, x, y);
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
                if (active)
                {
                    // 0x403680: the playback's messages for this frame, then the frame
                    followMenu();
                    const bool was = playback.Playing();
                    for (const wow::Playback::Message& m : playback.Pump(frontEnd.Frames())) act(m);
                    if (was && !playback.Playing()) { SDL_Log("Playback finished"); frontEnd.SetPlaybackState(playback.Recording(), false); }
                    frontEnd.Update();
                }
                lag -= TickNs;
            }

            fitPicture();
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
