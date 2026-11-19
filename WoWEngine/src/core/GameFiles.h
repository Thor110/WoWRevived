// Finding the original game files on disk.
//
// The game was written for Windows, where file names are case-insensitive, and the files on the discs use a mix of
// cases (DAT\Dat.wow, MAPS\MAPS.WoW, Human.cd, MARTIAN.cd). Linux and most macOS setups are case-sensitive, so every
// game path goes through Resolve(), which matches each part of the path without regard to case.
#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wow
{
    class GameFiles
    {
    public:
        explicit GameFiles(std::filesystem::path root) : root(std::move(root)) {}

        const std::filesystem::path& Root() const { return root; }

        // A game path such as "DAT\\Dat.wow" (either slash) to the real file, or nothing if it doesn't exist.
        std::optional<std::filesystem::path> Resolve(std::string_view gamePath) const;

        // The files the engine can't start without that are missing from Root().
        std::vector<std::string> Missing() const;
        bool IsComplete() const { return Missing().empty(); }

        // Every file that must be present for a complete install (shared with the installer).
        static const std::vector<std::string>& RequiredFiles();

        // Looks for a complete install in the usual places: the folder given on the command line, the working
        // folder, the folder the engine lives in and a "game" folder next to it.
        static std::optional<GameFiles> Locate(const std::vector<std::filesystem::path>& candidates);

    private:
        std::filesystem::path root;
    };
}
