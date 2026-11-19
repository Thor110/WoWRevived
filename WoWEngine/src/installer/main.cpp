// WoW Install - the graphical installer, laid out like WoWLauncher and the original game's launcher: the
// fighting machine picture with the controls underneath.
//
// It watches the drives (and any disc images picked with "Use Disc Images") for the Human and Martian discs,
// copies whichever are found, and if only one is in, asks for the other and carries on by itself when it
// appears, whichever order they come in and whether they're in one drive or two. The copying is the
// wowinstall library's (src/install), the same as the wowinstall-cli command line tool.
//
// Options, mainly for testing: --target <folder>, --image <.cue or .iso> (repeatable), --start (begin straight
// away), --screenshot <file.bmp> <seconds> (save the window after that long and close).
#include "Version.h"
#include "core/Bmp.h"
#include "core/GameFiles.h"
#include "install/Installer.h"
#include "installer/Ui.h"
#include "platform/Video.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace wow;
using namespace wow::install;

namespace
{
    constexpr int Width = 424, Height = 504; // WoWLauncher's client size

    struct FoundDisc
    {
        std::string source;
        Side side = Side::Unknown;
        std::string language;
        size_t tracks = 0;
    };

    enum class Phase { Waiting, Copying, NeedOtherDisc, Done, Failed };

    struct Cancelled {};

    // Everything the worker thread and the window share, behind one lock.
    struct State
    {
        std::mutex lock;
        std::vector<FoundDisc> discs;          // last scan of the drives and picked images
        std::vector<std::string> images;       // .cue / .iso files picked by hand
        fs::path target;
        bool music = true;
        Phase phase = Phase::Waiting;
        std::string message;
        double progress = 0;
        InstallResult result;
        bool startRequested = false, skipRequested = false;
        std::atomic<bool> quit{ false };
        std::condition_variable wake;
    };

    std::string SideName(Side s) { return s == Side::Human ? "Human" : s == Side::Martian ? "Martian" : "?"; }

    std::vector<FoundDisc> Scan(const std::vector<std::string>& images)
    {
        std::vector<FoundDisc> found;
        std::vector<std::string> sources = images;
        for (const auto& drive : FindDrives()) sources.push_back(drive.string());
        for (const std::string& source : sources)
        {
            try
            {
                auto disc = OpenDisc(source);
                DiscInfo info = Identify(*disc);
                if (info.side == Side::Unknown) continue;
                found.push_back({ source, info.side, info.language, disc->Tracks().size() });
            }
            catch (const std::exception&) {} // an empty drive or something that isn't one of the discs
        }
        return found;
    }

    void Worker(State& s)
    {
        while (!s.quit)
        {
            std::vector<std::string> images;
            bool start = false, skip = false;
            Phase phase;
            {
                std::lock_guard<std::mutex> l(s.lock);
                images = s.images;
                phase = s.phase;
                start = s.startRequested;
                skip = s.skipRequested;
                s.startRequested = s.skipRequested = false;
            }

            if (phase == Phase::Waiting || phase == Phase::NeedOtherDisc)
            {
                std::vector<FoundDisc> found = Scan(images);
                std::lock_guard<std::mutex> l(s.lock);
                s.discs = found;
                if (phase == Phase::NeedOtherDisc && skip)
                {
                    Finish(InstallOptions{ s.target, s.music, true }, s.result);
                    s.phase = Phase::Done;
                    continue;
                }
                // start when asked, or carry on by itself once the missing disc turns up
                bool missingArrived = false;
                if (phase == Phase::NeedOtherDisc)
                    for (const FoundDisc& d : found)
                        missingArrived |= (d.side == Side::Human && !s.result.humanDisc) || (d.side == Side::Martian && !s.result.martianDisc);
                if ((start && !found.empty()) || missingArrived) s.phase = Phase::Copying;
            }

            Phase now;
            {
                std::lock_guard<std::mutex> l(s.lock);
                now = s.phase;
            }
            if (now == Phase::Copying)
            {
                InstallOptions options;
                std::vector<FoundDisc> discs;
                InstallResult result;
                {
                    std::lock_guard<std::mutex> l(s.lock);
                    options = { s.target, s.music, true };
                    discs = s.discs;
                    result = s.result;
                }
                try
                {
                    for (const FoundDisc& d : discs)
                    {
                        if ((d.side == Side::Human && result.humanDisc) || (d.side == Side::Martian && result.martianDisc)) continue;
                        auto disc = OpenDisc(d.source);
                        CopyDisc(*disc, options, [&](const std::string& message, double fraction) {
                            if (s.quit) throw Cancelled{};
                            std::lock_guard<std::mutex> l(s.lock);
                            s.message = ui::ToLatin1(message);
                            s.progress = fraction;
                        }, result);
                    }
                    std::lock_guard<std::mutex> l(s.lock);
                    s.result = result;
                    if (result.humanDisc && result.martianDisc)
                    {
                        Finish(options, s.result);
                        s.phase = Phase::Done;
                    }
                    else s.phase = Phase::NeedOtherDisc;
                }
                catch (const Cancelled&) { return; }
                catch (const std::exception& ex)
                {
                    std::lock_guard<std::mutex> l(s.lock);
                    s.phase = Phase::Failed;
                    s.message = ui::ToLatin1(ex.what());
                }
                continue;
            }

            std::unique_lock<std::mutex> l(s.lock);
            s.wake.wait_for(l, std::chrono::milliseconds(1500), [&] { return s.quit.load() || s.startRequested || s.skipRequested; });
        }
    }

    // SDL's file dialogs answer on their own schedule (and thread); the answer is picked up by the main loop.
    struct DialogAnswer
    {
        std::mutex lock;
        std::optional<std::string> folder;
        std::vector<std::string> images;
    };

    void SDLCALL FolderPicked(void* userdata, const char* const* files, int)
    {
        auto* answer = static_cast<DialogAnswer*>(userdata);
        if (!files || !files[0]) return;
        std::lock_guard<std::mutex> l(answer->lock);
        answer->folder = files[0];
    }

    void SDLCALL ImagesPicked(void* userdata, const char* const* files, int)
    {
        auto* answer = static_cast<DialogAnswer*>(userdata);
        if (!files) return;
        std::lock_guard<std::mutex> l(answer->lock);
        for (int i = 0; files[i]; i++) answer->images.push_back(files[i]);
    }

    struct Picture
    {
        std::vector<uint32_t> pixels;
        int w = 0, h = 0;
    };

    Picture LoadPicture()
    {
        Picture p;
        std::string path = std::string(SDL_GetBasePath() ? SDL_GetBasePath() : "") + "launcher.bmp";
        SDL_Surface* loaded = SDL_LoadBMP(path.c_str());
        if (!loaded) return p;
        SDL_Surface* rgb = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_XRGB8888);
        SDL_DestroySurface(loaded);
        if (!rgb) return p;
        p.w = rgb->w;
        p.h = rgb->h;
        p.pixels.resize(size_t(p.w) * p.h);
        for (int y = 0; y < p.h; y++)
            std::memcpy(p.pixels.data() + size_t(y) * p.w, static_cast<uint8_t*>(rgb->pixels) + size_t(y) * rgb->pitch, size_t(p.w) * 4);
        SDL_DestroySurface(rgb);
        return p;
    }

    fs::path EnginePath()
    {
        fs::path base = SDL_GetBasePath() ? SDL_GetBasePath() : "";
#ifdef _WIN32
        return base / "WoWEngine.exe";
#else
        return base / "WoWEngine";
#endif
    }
}

int main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    State state;
    state.target = SDL_GetBasePath() ? fs::path(SDL_GetBasePath()) : fs::current_path();
    fs::path screenshot;
    double screenshotSeconds = 0;
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--target" && i + 1 < argc) state.target = argv[++i];
        else if (arg == "--image" && i + 1 < argc) state.images.push_back(argv[++i]);
        else if (arg == "--start") state.startRequested = true;
        else if (arg == "--screenshot" && i + 2 < argc) { screenshot = argv[++i]; screenshotSeconds = std::atof(argv[++i]); }
    }
    const uint64_t startedAt = SDL_GetTicks();
    std::thread worker(Worker, std::ref(state));
    DialogAnswer answer;

    int exitCode = 0;
    {
        Video video;
        std::string error;
        std::string title = std::string("Jeff Wayne's 'The War Of The Worlds' - Installer v") + WOWENGINE_VERSION;
        if (!video.Open(title.c_str(), Width, Height, false, error))
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "WoW Install", error.c_str(), nullptr);
            exitCode = 1;
        }
        else
        {
            Picture picture = LoadPicture();
            const ui::Box pictureBox{ 12, 12, 400, 300 };
            const ui::Box installButton{ 12, 420, 270, 23 }, imagesButton{ 12, 449, 270, 23 }, folderButton{ 12, 478, 270, 23 };
            const ui::Box musicBox{ 296, 422, 116, 20 }, exitButton{ 296, 478, 116, 23 };
            int mouseX = -1, mouseY = -1, pressed = -1;
            bool running = true;

            while (running)
            {
                // what the window shows comes from a snapshot of the shared state
                Phase phase;
                std::vector<FoundDisc> discs;
                std::string message;
                double progress;
                InstallResult result;
                fs::path target;
                bool music;
                {
                    std::lock_guard<std::mutex> l(state.lock);
                    phase = state.phase;
                    discs = state.discs;
                    message = state.message;
                    progress = state.progress;
                    result = state.result;
                    target = state.target;
                    music = state.music;
                }
                bool busy = phase == Phase::Copying;
                bool engineThere = fs::exists(EnginePath());
                std::string installLabel = phase == Phase::NeedOtherDisc ? "Skip the Other Disc"
                                         : phase == Phase::Done ? "Start the Game" : "Install The War Of The Worlds";
                bool installEnabled = phase == Phase::NeedOtherDisc || (phase == Phase::Done && engineThere && result.missing.empty())
                                   || (phase == Phase::Waiting && !discs.empty()) || (phase == Phase::Failed && !discs.empty());

                // buttons: 0 install, 1 images, 2 folder, 3 music, 4 exit
                const ui::Box* boxes[] = { &installButton, &imagesButton, &folderButton, &musicBox, &exitButton };
                bool enabled[] = { installEnabled, !busy && phase != Phase::Done, !busy && phase == Phase::Waiting, !busy && phase == Phase::Waiting, true };
                auto activate = [&](int index) {
                    switch (index)
                    {
                    case 0:
                        if (phase == Phase::Done)
                        {
                            std::string engine = EnginePath().string(), folder = target.string();
                            const char* args[] = { engine.c_str(), folder.c_str(), nullptr };
                            if (SDL_Process* p = SDL_CreateProcess(args, false)) { SDL_DestroyProcess(p); running = false; }
                        }
                        else
                        {
                            std::lock_guard<std::mutex> l(state.lock);
                            if (phase == Phase::NeedOtherDisc) state.skipRequested = true;
                            else { state.startRequested = true; state.phase = Phase::Waiting; state.message.clear(); }
                            state.wake.notify_all();
                        }
                        break;
                    case 1:
                    {
                        static const SDL_DialogFileFilter filters[] = { { "Disc images (.cue, .iso)", "cue;iso" } };
                        SDL_ShowOpenFileDialog(ImagesPicked, &answer, video.Window(), filters, 1, nullptr, true);
                        break;
                    }
                    case 2:
                        SDL_ShowOpenFolderDialog(FolderPicked, &answer, video.Window(), target.string().c_str(), false);
                        break;
                    case 3:
                    {
                        std::lock_guard<std::mutex> l(state.lock);
                        state.music = !state.music;
                        break;
                    }
                    case 4: running = false; break;
                    }
                };

                SDL_Event event;
                while (SDL_PollEvent(&event))
                {
                    int x = 0, y = 0;
                    switch (event.type)
                    {
                    case SDL_EVENT_QUIT: running = false; break;
                    case SDL_EVENT_MOUSE_MOTION:
                        if (video.ToGame(event.motion.x, event.motion.y, x, y)) { mouseX = x; mouseY = y; }
                        else mouseX = mouseY = -1;
                        break;
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    case SDL_EVENT_MOUSE_BUTTON_UP:
                    {
                        if (event.button.button != SDL_BUTTON_LEFT || !video.ToGame(event.button.x, event.button.y, x, y)) break;
                        int hit = -1;
                        for (int i = 0; i < 5; i++) if (enabled[i] && boxes[i]->Contains(x, y)) hit = i;
                        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) pressed = hit;
                        else
                        {
                            if (hit >= 0 && hit == pressed) activate(hit);
                            pressed = -1;
                        }
                        break;
                    }
                    case SDL_EVENT_KEY_DOWN:
                        if (event.key.key == SDLK_ESCAPE) running = false;
                        else if ((event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) && installEnabled) activate(0);
                        break;
                    default: break;
                    }
                }

                // answers from the folder / image dialogs
                {
                    std::lock_guard<std::mutex> a(answer.lock);
                    std::lock_guard<std::mutex> l(state.lock);
                    if (answer.folder) { state.target = *answer.folder; answer.folder.reset(); }
                    for (const std::string& image : answer.images) state.images.push_back(image);
                    if (!answer.images.empty()) state.wake.notify_all();
                    answer.images.clear();
                }

                // draw
                ui::Painter p(video.Pixels(), Width, Height);
                p.Fill({ 0, 0, Width, Height }, ui::Colour::Dialog);
                if (!picture.pixels.empty()) p.Image(picture.pixels.data(), picture.w, picture.h, pictureBox.x, pictureBox.y);
                else p.Fill(pictureBox, 0x101010);

                auto discLine = [&](Side side, int y) {
                    const FoundDisc* disc = nullptr;
                    for (const FoundDisc& d : discs) if (d.side == side) { disc = &d; break; }
                    bool copied = side == Side::Human ? result.humanDisc : result.martianDisc;
                    p.Text(SideName(side) + " disc:", 12, y, ui::Colour::Text);
                    std::string text;
                    if (copied) text = "copied";
                    else if (disc)
                    {
                        // drives by name ("D:\"), images by file name
                        fs::path source(disc->source);
                        text = ui::ToLatin1(source.has_extension() ? source.filename().string() : source.string());
                        if (!disc->language.empty()) text += "  (" + disc->language + ")";
                        text += disc->tracks ? "" : "  - no music on this source";
                    }
                    else text = "not found - insert it, or use a disc image";
                    p.Text(ui::Painter::Fit(text, 412 - 110), 110, y, copied ? 0x107010 : disc ? ui::Colour::Text : ui::Colour::GreyText);
                };
                discLine(Side::Human, 320);
                discLine(Side::Martian, 338);
                p.Text("Install to:", 12, 356, ui::Colour::Text);
                p.Text(ui::Painter::Fit(ui::ToLatin1(target.string()), 412 - 110), 110, 356, ui::Colour::Text);

                std::string status;
                uint32_t statusColour = ui::Colour::Text;
                switch (phase)
                {
                case Phase::Waiting: status = discs.empty() ? "Insert the Human or Martian disc." : "Ready to install."; break;
                case Phase::Copying: status = message; break;
                case Phase::NeedOtherDisc:
                    status = std::string("Insert the ") + (result.humanDisc ? "Martian" : "Human") + " disc to carry on, or skip it.";
                    break;
                case Phase::Done:
                    if (!result.missing.empty()) { status = "Some game files are missing: " + result.missing.front(); statusColour = ui::Colour::Warning; }
                    else if (!result.warnings.empty()) { status = "Installed. " + result.warnings.front(); statusColour = ui::Colour::Warning; }
                    else status = engineThere ? "Installed. Enjoy the game!" : "Installed.";
                    break;
                case Phase::Failed: status = "The install stopped: " + message; statusColour = ui::Colour::Warning; break;
                }
                if (busy || phase == Phase::Done) p.Progress({ 12, 378, 400, 14 }, phase == Phase::Done ? 1.0 : progress);
                p.Text(ui::Painter::Fit(status, 400), 12, 396, statusColour);

                for (int i = 0; i < 5; i++)
                {
                    ui::Painter::State st = !enabled[i] ? ui::Painter::State::Disabled
                                          : pressed == i ? ui::Painter::State::Pressed
                                          : boxes[i]->Contains(mouseX, mouseY) ? ui::Painter::State::Hot : ui::Painter::State::Normal;
                    switch (i)
                    {
                    case 0: p.Button(*boxes[i], installLabel, st, true); break;
                    case 1: p.Button(*boxes[i], "Use Disc Images...", st, false); break;
                    case 2: p.Button(*boxes[i], "Choose Install Folder...", st, false); break;
                    case 3: p.CheckBox(*boxes[i], "Rip the music", music, st); break;
                    case 4: p.Button(*boxes[i], "Exit", st, false); break;
                    }
                }
                video.Present();
                if (!screenshot.empty() && double(SDL_GetTicks() - startedAt) >= screenshotSeconds * 1000.0)
                {
                    WriteBmp(screenshot, video.Pixels(), Width, Height);
                    running = false;
                }
            }
        }
    }

    state.quit = true;
    state.wake.notify_all();
    worker.join();
    SDL_Quit();
    return exitCode;
}
