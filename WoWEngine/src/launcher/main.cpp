// WoWLauncher - the one program players start. Laid out like WoWRevived's WoWLauncher (and the original game's
// launcher): the fighting machine picture with the controls underneath.
//
// If the game folder is complete it offers Start Human Game / Start Martian Game, which run WoWEngine with
// --human or --martian (no Human.cd / MARTIAN.cd side marker is needed). Otherwise, or with "Install from
// Discs", it's the installer: it watches the drives (and any disc images picked with "Use Disc Images") for
// the Human and Martian discs, copies whichever are found and rips their music, and if only one is in, asks for
// the other and carries on by itself when it appears, whichever order they come in and whether they're in one
// drive or two. The copying is the wowinstall library's (src/install), the same as wowinstall-cli.
//
// Options, mainly for testing: --target <folder>, --install (open on the install screen), --image <.cue or
// .iso> (repeatable), --start (begin installing straight away), --screenshot <file.bmp> <seconds> (save the
// window after that long and close).
#include "Version.h"
#include "core/Bmp.h"
#include "core/GameFiles.h"
#include "install/Installer.h"
#include "launcher/Ui.h"
#include "platform/Launch.h"
#include "platform/Video.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <atomic>
#include <cctype>
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

    enum class Phase { Ready, Waiting, Copying, NeedOtherDisc, Done, Failed }; // Ready: installed, the launcher screen

    struct Cancelled {};

    // Everything the worker thread and the window share, behind one lock.
    struct State
    {
        std::mutex lock;
        std::vector<FoundDisc> discs;          // last scan of the drives and picked images
        std::vector<std::string> images;       // .cue / .iso files picked by hand
        fs::path target;
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
                    Finish(InstallOptions{ s.target, true }, s.result);
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
                    options = { s.target, true };
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

    bool HasMusic(const fs::path& target, const char* folder)
    {
        std::error_code ec;
        auto dir = GameFiles(target).Resolve(folder);
        if (!dir) return false;
        for (const auto& entry : fs::directory_iterator(*dir, ec))
        {
            std::string ext = entry.path().extension().string();
            for (char& c : ext) c = char(std::tolower(static_cast<unsigned char>(c)));
            if (ext == ".wav") return true;
        }
        return false;
    }

    // The launcher screen's notice when music is missing: what's wrong, why, and what it means for the game.
    std::string MusicNotice(const fs::path& target)
    {
        bool human = HasMusic(target, "Music-Human"), martian = HasMusic(target, "Music-Martian");
        if (human && martian) return {};
        std::string which = !human && !martian ? "Human or Martian" : !human ? "Human" : "Martian";
        return "No " + which + " music was found, so the game will play without it. The music needs the real disc "
               "or a .bin/.cue image of it, not an .iso.";
    }

    bool Installed(const fs::path& target) { return GameFiles(target).IsComplete(); }
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
    bool installScreen = false;
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--target" && i + 1 < argc) state.target = argv[++i];
        else if (arg == "--install") installScreen = true;
        else if (arg == "--image" && i + 1 < argc) state.images.push_back(argv[++i]);
        else if (arg == "--start") { state.startRequested = true; installScreen = true; }
        else if (arg == "--screenshot" && i + 2 < argc) { screenshot = argv[++i]; screenshotSeconds = std::atof(argv[++i]); }
    }
    // WoWRevived's updated text for each language is in "resources" beside the programs (a build run where it was
    // made: the source's). The install puts the one for the discs' language in as the game's TEXT.ojd; a game
    // that is installed already gets it now.
    {
        // (a release's folder beside the programs is deleted once its file has been copied in; the source's,
        // which a build run where it was made uses, never is)
        std::vector<std::pair<fs::path, bool>> places;
        if (const char* base = SDL_GetBasePath()) places.emplace_back(fs::path(base) / "resources", true);
#ifdef WOWENGINE_SOURCE_DIR
        places.emplace_back(fs::path(WOWENGINE_SOURCE_DIR) / "resources", false);
#endif
        for (const auto& [place, remove] : places)
        {
            std::error_code ec;
            if (!fs::is_regular_file(place / "TEXT-EN.ojd", ec)) continue;
            // (never the source folder, even if the program was built into it)
            bool source = false;
#ifdef WOWENGINE_SOURCE_DIR
            source = fs::equivalent(place, fs::path(WOWENGINE_SOURCE_DIR) / "resources", ec);
#endif
            SetTextFolder(place, remove && !source);
            break;
        }
    }
    if (screenshot.empty() && Installed(state.target)) CopyLanguageText(state.target);
    // (update 248) the install's WoWRevived.ini documented: each setting's description and default, once
    if (screenshot.empty() && Installed(state.target)) DocumentSettings(state.target);
    state.phase = !installScreen && Installed(state.target) ? Phase::Ready : Phase::Waiting;
    const uint64_t startedAt = SDL_GetTicks();
    std::thread worker(Worker, std::ref(state));
    DialogAnswer answer;

    int exitCode = 0;
    {
        Video video;
        std::string error;
        std::string title = std::string("Jeff Wayne's 'The War Of The Worlds' - WoW Engine v") + WOWENGINE_VERSION;
        if (!video.Open(title.c_str(), Width, Height, false, error))
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "WoWLauncher", error.c_str(), nullptr);
            exitCode = 1;
        }
        else
        {
            Picture picture = LoadPicture();
            const ui::Box pictureBox{ 12, 12, 400, 300 };
            // the launcher screen, where WoWRevived's launcher has its buttons
            const ui::Box humanButton{ 122, 325, 195, 23 }, martianButton{ 122, 354, 195, 23 }, quitButton{ 122, 470, 195, 23 };
            // the install screen
            const ui::Box installButton{ 12, 420, 270, 23 }, imagesButton{ 12, 449, 270, 23 }, folderButton{ 12, 478, 270, 23 };
            const ui::Box backButton{ 296, 449, 116, 23 }, exitButton{ 296, 478, 116, 23 };
            int mouseX = -1, mouseY = -1, pressed = -1;
            bool running = true;
            Phase shown = Phase::Failed; // the phase last drawn, to notice an install finishing
            std::string notice;          // the launcher screen's music notice

            while (running)
            {
                // what the window shows comes from a snapshot of the shared state
                Phase phase;
                std::vector<FoundDisc> discs;
                std::string message;
                double progress;
                InstallResult result;
                fs::path target;
                {
                    std::lock_guard<std::mutex> l(state.lock);
                    // a finished install with every file in place goes straight to the launcher screen
                    if (state.phase == Phase::Done && state.result.missing.empty()) state.phase = Phase::Ready;
                    phase = state.phase;
                    discs = state.discs;
                    message = state.message;
                    progress = state.progress;
                    result = state.result;
                    target = state.target;
                }
                if (phase != shown)
                {
                    if (phase == Phase::Ready)
                    {
                        notice = MusicNotice(target);
                        // the install has just finished: tell the player about anything that went wrong with it
                        if (shown == Phase::Copying || shown == Phase::NeedOtherDisc)
                        {
                            if (!result.warnings.empty())
                            {
                                std::string text = "The game is installed, with ";
                                text += result.warnings.size() == 1 ? "one problem:\n" : "these problems:\n";
                                for (const std::string& w : result.warnings) text += "\n- " + w;
                                if (screenshot.empty())
                                    SDL_ShowSimpleMessageBox(result.musicMissing ? SDL_MESSAGEBOX_WARNING : SDL_MESSAGEBOX_INFORMATION,
                                                             "WoWLauncher", text.c_str(), video.Window());
                            }
                        }
                    }
                    shown = phase;
                }
                bool launcher = phase == Phase::Ready;
                bool busy = phase == Phase::Copying;
                bool engineThere = fs::exists(EnginePath());
                bool installed = !launcher && !busy && Installed(target);
                std::string installLabel = phase == Phase::NeedOtherDisc ? "Skip the Other Disc" : "Install The War Of The Worlds";
                bool installEnabled = phase == Phase::NeedOtherDisc || ((phase == Phase::Waiting || phase == Phase::Failed || phase == Phase::Done) && !discs.empty());

                // launcher screen: 0 human, 1 martian, 2 exit
                // install screen:  0 install, 1 images, 2 folder, 3 back, 4 exit
                std::vector<const ui::Box*> boxes;
                std::vector<bool> enabled;
                std::vector<std::string> labels;
                if (launcher)
                {
                    // (with the game installed there is no "Install from Discs": --install opens the install screen
                    // for anyone who wants to install again)
                    boxes = { &humanButton, &martianButton, &quitButton };
                    enabled = { engineThere, engineThere, true };
                    labels = { "Start Human Game", "Start Martian Game", "Exit" };
                }
                else
                {
                    boxes = { &installButton, &imagesButton, &folderButton, &backButton, &exitButton };
                    bool canGoBack = installed && (phase == Phase::Waiting || phase == Phase::Failed);
                    enabled = { installEnabled, !busy, !busy && phase != Phase::NeedOtherDisc, canGoBack, true };
                    labels = { installLabel, "Use Disc Images...", "Choose Install Folder...", "Back", "Exit" };
                }
                auto startGame = [&](bool human) {
                    std::string engine = EnginePath().string(), folder = target.string();
                    if (LaunchDetached({ engine, folder, human ? "--human" : "--martian" })) running = false;
                    else
                    {
                        std::string text = "WoWEngine couldn't be started:\n" + engine + "\n\n" + SDL_GetError();
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "WoWLauncher", text.c_str(), video.Window());
                    }
                };
                auto activate = [&](int index) {
                    if (launcher)
                    {
                        switch (index)
                        {
                        case 0: startGame(true); break;
                        case 1: startGame(false); break;
                        case 2: running = false; break;
                        }
                        return;
                    }
                    switch (index)
                    {
                    case 0:
                    {
                        std::lock_guard<std::mutex> l(state.lock);
                        if (phase == Phase::NeedOtherDisc) state.skipRequested = true;
                        else { state.startRequested = true; state.phase = Phase::Waiting; state.result = {}; state.message.clear(); }
                        state.wake.notify_all();
                        break;
                    }
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
                        state.phase = Phase::Ready;
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
                        for (size_t i = 0; i < boxes.size(); i++) if (enabled[i] && boxes[i]->Contains(x, y)) hit = int(i);
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
                        else if ((event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) && enabled[0]) activate(0);
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

                if (launcher)
                {
                    std::string text = engineThere ? notice : "WoWEngine wasn't found next to WoWLauncher, so the game can't be started.";
                    if (!text.empty())
                    {
                        std::vector<std::string> lines = ui::Painter::Wrap(text, 400);
                        int y = 412;
                        for (size_t i = 0; i < lines.size() && i < 3; i++, y += 17)
                        {
                            std::string line = i == 2 && lines.size() > 3 ? ui::Painter::Fit(lines[i] + " " + lines[3], 400) : lines[i];
                            p.TextCentred(line, { 12, y, 400, 17 }, ui::Colour::Warning);
                        }
                    }
                }
                else
                {
                    auto discLine = [&](Side side, int y) {
                        const FoundDisc* disc = nullptr;
                        for (const FoundDisc& d : discs) if (d.side == side) { disc = &d; break; }
                        bool copied = side == Side::Human ? result.humanDisc : result.martianDisc;
                        p.Text(SideName(side) + " disc:", 12, y, ui::Colour::Text);
                        std::string text;
                        uint32_t colour = ui::Colour::GreyText;
                        if (copied) { text = "copied"; colour = 0x107010; }
                        else if (disc)
                        {
                            // drives by name ("D:\"), images by file name
                            fs::path source(disc->source);
                            text = ui::ToLatin1(source.has_extension() ? source.filename().string() : source.string());
                            if (!disc->language.empty()) text += "  (" + disc->language + ")";
                            colour = ui::Colour::Text;
                            if (!disc->tracks) { text += "  - no music on it"; colour = ui::Colour::Warning; }
                        }
                        else text = "not found - insert it, or use a disc image";
                        p.Text(ui::Painter::Fit(text, 412 - 110), 110, y, colour);
                    };
                    discLine(Side::Human, 320);
                    discLine(Side::Martian, 338);
                    p.Text("Install to:", 12, 356, ui::Colour::Text);
                    p.Text(ui::Painter::Fit(ui::ToLatin1(target.string()), 412 - 110), 110, 356, ui::Colour::Text);

                    bool noMusic = false;
                    for (const FoundDisc& d : discs) noMusic |= d.tracks == 0;
                    std::string status;
                    uint32_t statusColour = ui::Colour::Text;
                    switch (phase)
                    {
                    case Phase::Waiting:
                        if (discs.empty()) status = "Insert the Human or Martian disc.";
                        else if (noMusic) { status = "Ready, but an .iso image has no music, so the game won't either."; statusColour = ui::Colour::Warning; }
                        else status = "Ready to install.";
                        break;
                    case Phase::Copying: status = message; break;
                    case Phase::NeedOtherDisc:
                        status = std::string("Insert the ") + (result.humanDisc ? "Martian" : "Human") + " disc to carry on, or skip it.";
                        break;
                    case Phase::Done:
                        status = "Some game files are missing: " + (result.missing.empty() ? std::string("?") : result.missing.front());
                        statusColour = ui::Colour::Warning;
                        break;
                    case Phase::Failed: status = "The install stopped: " + message; statusColour = ui::Colour::Warning; break;
                    case Phase::Ready: break;
                    }
                    if (busy) p.Progress({ 12, 378, 400, 14 }, progress);
                    p.Text(ui::Painter::Fit(status, 400), 12, 396, statusColour);
                }

                for (size_t i = 0; i < boxes.size(); i++)
                {
                    if (!launcher && i == 3 && !enabled[3]) continue; // "Back" only when there's a game to go back to
                    ui::Painter::State st = !enabled[i] ? ui::Painter::State::Disabled
                                          : pressed == int(i) ? ui::Painter::State::Pressed
                                          : boxes[i]->Contains(mouseX, mouseY) ? ui::Painter::State::Hot : ui::Painter::State::Normal;
                    p.Button(*boxes[i], labels[i], st, i == 0);
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
