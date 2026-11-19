// WoW Engine - an SDL3 rebuild of the engine from Jeff Wayne's 'The War Of The Worlds' (Rage Software, 1998).
// Needs the original game files, which the installer copies from the two discs.
#include "core/GameFiles.h"
#include "platform/Video.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr int GameWidth = 640;
    constexpr int GameHeight = 480;
    constexpr uint64_t TickNs = 1'000'000'000ull / 30; // "Game Frequency" defaults to 30 updates a second

    std::vector<fs::path> CandidateFolders(int argc, char** argv)
    {
        std::vector<fs::path> folders;
        if (argc > 1) folders.emplace_back(argv[1]);
        std::error_code ec;
        folders.push_back(fs::current_path(ec));
        if (const char* base = SDL_GetBasePath())
        {
            folders.emplace_back(base);
            folders.push_back(fs::path(base) / "game");
        }
        return folders;
    }

    // Placeholder until the front end is rebuilt: a palette ramp so scaling and colour conversion can be checked.
    void DrawTestPattern(wow::Video& video, uint32_t frame)
    {
        for (int i = 0; i < 256; i++) video.Colours()[i] = { uint8_t(i), uint8_t((i * 3) & 255), uint8_t(255 - i) };
        uint8_t* p = video.Pixels();
        for (int y = 0; y < video.Height(); y++)
            for (int x = 0; x < video.Width(); x++)
                p[y * video.Width() + x] = uint8_t((x / 2 + y / 2 + frame) & 255);
    }
}

int main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    auto files = wow::GameFiles::Locate(CandidateFolders(argc, argv));
    if (!files)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "WoW Engine",
            "The original game files weren't found.\n\n"
            "Run the installer to copy them from the Human and Martian discs, "
            "or start the engine with the game folder as its first argument.", nullptr);
        SDL_Quit();
        return 1;
    }
    SDL_Log("Game files: %s", files->Root().string().c_str());

    int exitCode = 0;
    {
        wow::Video video;
        std::string error;
        if (!video.Open("Jeff Wayne's 'The War Of The Worlds'", GameWidth, GameHeight, false, error))
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "WoW Engine", error.c_str(), nullptr);
            exitCode = 1;
        }
        else
        {
            // fixed rate game updates, drawing as often as the display allows
            uint64_t previous = SDL_GetTicksNS();
            uint64_t lag = 0;
            uint32_t tick = 0;
            bool running = true;
            while (running)
            {
                SDL_Event event;
                while (SDL_PollEvent(&event))
                {
                    if (event.type == SDL_EVENT_QUIT) running = false;
                    else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
                    {
                        if (event.key.key == SDLK_ESCAPE) running = false;
                        else if (event.key.key == SDLK_RETURN && (event.key.mod & SDL_KMOD_ALT)) video.ToggleFullScreen();
                    }
                }

                uint64_t now = SDL_GetTicksNS();
                lag += now - previous;
                previous = now;
                if (lag > TickNs * 5) lag = TickNs * 5; // don't try to catch up after a long stall
                while (lag >= TickNs)
                {
                    tick++; // game update goes here
                    lag -= TickNs;
                }

                DrawTestPattern(video, tick);
                video.Present();
            }
        }
    }
    SDL_Quit();
    return exitCode;
}
