// wowinstall-cli - the command line version of WoWLauncher's installer: sets up a game folder from the original Human and Martian discs, or images of them.
//
//   wowinstall-cli --drives                                  list the optical drives
//   wowinstall-cli <game folder> [<disc>] [<disc>] [options] install (no disc: use whatever is in the drives)
//     <disc> is a drive or mounted disc (D:\, /media/cdrom, /dev/sr0), an .iso, or a .cue (with its .bin)
//     --no-music   don't rip the music tracks
// The discs can be given in either order. Both discs in two drives at once: they're found and copied without
// asking (the same disc twice is only copied once). One drive: after the first disc it asks for the
// other one in the same drive.
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
        "usage: wowinstall-cli --drives\n"
        "       wowinstall-cli <game folder> [<disc>] [<disc>] [--no-music]\n"
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
        if (argc < 2) return Usage();

        InstallOptions options;
        options.target = argv[1];
        std::vector<std::string> sources;
        for (int i = 2; i < argc; i++)
        {
            std::string arg = argv[i];
            if (arg == "--no-music") options.music = false;
            else sources.push_back(arg);
        }

        int lastPercent = -1;
        Progress progress = [&](const std::string& message, double fraction) {
            int percent = int(fraction * 100);
            if (percent != lastPercent || message.rfind("Ripping", 0) == 0) std::printf("[%3d%%] %s\n", percent, message.c_str());
            lastPercent = percent;
        };
        InstallResult result;
        // copies a disc unless that side has been copied already (the same disc in two drives, given twice...)
        auto copy = [&](const std::string& source) {
            auto disc = OpenDisc(source);
            DiscInfo info = Identify(*disc);
            std::printf("%s: %s %s disc\n", disc->Describe().c_str(), info.language.c_str(),
                info.side == Side::Human ? "Human" : info.side == Side::Martian ? "Martian" : "unknown");
            if ((info.side == Side::Human && result.humanDisc) || (info.side == Side::Martian && result.martianDisc))
            {
                std::printf("  already copied, skipping it\n");
                return;
            }
            lastPercent = -1;
            CopyDisc(*disc, options, progress, result);
        };
        for (const std::string& source : sources) copy(source);

        // look through the drives for whichever disc is still missing (both discs in two drives at once,
        // or no disc given at all)
        auto scanDrives = [&]() {
            for (const auto& drive : FindDrives())
            {
                if (result.humanDisc && result.martianDisc) return;
                try
                {
                    auto disc = OpenDisc(drive);
                    Side side = Identify(*disc).side;
                    if ((side == Side::Human && !result.humanDisc) || (side == Side::Martian && !result.martianDisc)) copy(drive.string());
                }
                catch (const std::exception&) {} // empty drive, or not one of the game's discs
            }
        };
        bool usesDrive = sources.empty();
        std::error_code ec;
        for (const std::string& source : sources)
            usesDrive |= std::filesystem::is_directory(source, ec) || source.rfind("/dev/", 0) == 0;
        if (usesDrive) scanDrives();
        if (sources.empty() && !result.humanDisc && !result.martianDisc)
        {
            std::fprintf(stderr, "wowinstall-cli: no game disc found in the drives\n");
            return 1;
        }

        // one drive: ask for the other disc in the same drive
        std::string swapDrive = sources.size() == 1 ? sources[0] : std::string();
        if (swapDrive.empty() && sources.empty())
            for (const auto& drive : FindDrives()) { swapDrive = drive.string(); break; }
        bool singleDrive = usesDrive && !swapDrive.empty();
        while (singleDrive && !(result.humanDisc && result.martianDisc))
        {
            std::printf("Put the %s disc in %s and press Enter (or type s and Enter to skip it): ",
                result.humanDisc ? "Martian" : "Human", swapDrive.c_str());
            std::fflush(stdout);
            std::string answer;
            if (!std::getline(std::cin, answer) || answer == "s" || answer == "S") break;
            try
            {
                scanDrives(); // the other disc may have gone into a different drive
                if (result.humanDisc && result.martianDisc) break;
                auto disc = OpenDisc(swapDrive);
                Side side = Identify(*disc).side;
                if ((side == Side::Human && result.humanDisc) || (side == Side::Martian && result.martianDisc))
                {
                    std::printf("That's the disc that was just copied.\n");
                    continue;
                }
                copy(swapDrive);
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
        std::fprintf(stderr, "wowinstall-cli: %s\n", ex.what());
        return 1;
    }
}
