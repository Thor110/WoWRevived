// WoWInstall - sets up a game folder from the original Human and Martian discs, or images of them.
//
//   WoWInstall --drives                                  list the optical drives
//   WoWInstall <game folder> <disc> [<disc>] [options]   install
//     <disc> is a drive or mounted disc (D:\, /media/cdrom, /dev/sr0), an .iso, or a .cue (with its .bin)
//     --martian    start on the Martian side (default: Human)
//     --no-music   don't rip the music tracks
// The discs can be given in either order. With a single drive, give it once: after the first disc is copied
// WoWInstall asks for the other one to be put in the same drive.
#include "install/Installer.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <exception>
#include <memory>
#include <string>
#include <vector>

using namespace wow::install;

static int Usage()
{
    std::fprintf(stderr,
        "usage: WoWInstall --drives\n"
        "       WoWInstall <game folder> <disc> [<disc>] [--martian] [--no-music]\n"
        "       <disc>: a drive or mounted disc, an .iso, or a .cue\n");
    return 2;
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--drives")
        {
            for (const auto& drive : FindDrives())
            {
                std::printf("%s", drive.string().c_str());
                try
                {
                    auto disc = OpenDisc(drive);
                    DiscInfo info = Identify(*disc);
                    std::printf("  %s %s disc, %zu audio tracks", info.language.c_str(),
                        info.side == Side::Human ? "Human" : info.side == Side::Martian ? "Martian" : "unknown", disc->Tracks().size());
                }
                catch (const std::exception&) { std::printf("  (no disc)"); }
                std::printf("\n");
            }
            return 0;
        }
        if (argc < 3) return Usage();

        InstallOptions options;
        options.target = argv[1];
        std::vector<std::string> sources;
        for (int i = 2; i < argc; i++)
        {
            std::string arg = argv[i];
            if (arg == "--martian") options.human = false;
            else if (arg == "--no-music") options.music = false;
            else sources.push_back(arg);
        }
        if (sources.empty()) return Usage();

        int lastPercent = -1;
        Progress progress = [&](const std::string& message, double fraction) {
            int percent = int(fraction * 100);
            if (percent != lastPercent || message.rfind("Ripping", 0) == 0) std::printf("[%3d%%] %s\n", percent, message.c_str());
            lastPercent = percent;
        };
        InstallResult result;
        auto copy = [&](const std::string& source) {
            auto disc = OpenDisc(source);
            DiscInfo info = Identify(*disc);
            std::printf("%s: %s %s disc\n", disc->Describe().c_str(), info.language.c_str(),
                info.side == Side::Human ? "Human" : info.side == Side::Martian ? "Martian" : "unknown");
            lastPercent = -1;
            CopyDisc(*disc, options, progress, result);
        };
        for (const std::string& source : sources) copy(source);

        // one drive and one disc so far: ask for the other one in the same drive
        std::error_code ec;
        bool singleDrive = sources.size() == 1 && std::filesystem::is_directory(sources[0], ec) ? true
                         : sources.size() == 1 && sources[0].rfind("/dev/", 0) == 0;
        while (singleDrive && !(result.humanDisc && result.martianDisc))
        {
            std::printf("Put the %s disc in %s and press Enter (or type s and Enter to skip it): ",
                result.humanDisc ? "Martian" : "Human", sources[0].c_str());
            std::fflush(stdout);
            std::string answer;
            if (!std::getline(std::cin, answer) || answer == "s" || answer == "S") break;
            try
            {
                auto disc = OpenDisc(sources[0]);
                Side side = Identify(*disc).side;
                if ((side == Side::Human && result.humanDisc) || (side == Side::Martian && result.martianDisc))
                {
                    std::printf("That's the disc that was just copied.\n");
                    continue;
                }
                copy(sources[0]);
            }
            catch (const std::exception& ex) { std::printf("Can't read the disc yet: %s\n", ex.what()); }
        }

        Finish(options, result);
        for (const auto& w : result.warnings) std::printf("warning: %s\n", w.c_str());
        for (const auto& m : result.missing) std::printf("missing: %s\n", m.c_str());
        std::printf(result.missing.empty() ? "The game is installed in %s\n" : "The install in %s is incomplete\n", options.target.string().c_str());
        return result.missing.empty() ? 0 : 1;
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "WoWInstall: %s\n", ex.what());
        return 1;
    }
}
