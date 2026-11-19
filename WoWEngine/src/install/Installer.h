// Sets up a game folder from the two original discs (or their images):
//   1. works out which disc is which: the Human disc has Human.cd, the Martian disc MARTIAN.CD, and the
//      language comes from ENGLISH.CD / FRENCH.CD / GERMAN.CD / ITALIAN.CD / SPANISH.CD
//   2. copies the game files, leaving out everything WoWLauncher's clean-up deletes (the setup program, DirectX,
//      the original FMV\*.smk movies...). The discs can come in either order: where both have a file, the Human
//      disc's copy wins (the Human disc always overwrites, the Martian disc never does)
//   3. rips the music (tracks 2-5 of each disc) to the folders the _inmm wrapper reads, named after the disc:
//      the Human disc's to Music-Human\NN TrackNN.wav, the Martian disc's to Music-Martian\NN TrackNN.wav
//   4. writes language.txt and leaves the side marker for the side picked (Human.cd or MARTIAN.cd, the file
//      WoWLauncher renames to switch sides)
//   5. checks that every file the engine needs is there
#pragma once
#include "install/Disc.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace wow::install
{
    enum class Side { Unknown, Human, Martian };

    struct DiscInfo
    {
        Side side = Side::Unknown;
        std::string language; // "English", ... or empty
    };

    DiscInfo Identify(Disc& disc);

    struct InstallOptions
    {
        std::filesystem::path target;
        bool music = true;
        bool human = true; // which side the install starts on
    };

    // message and overall progress (0-1)
    using Progress = std::function<void(const std::string& message, double fraction)>;

    struct InstallResult
    {
        std::vector<std::string> warnings;  // things that didn't stop the install (no music on an .iso, ...)
        std::vector<std::string> missing;   // files the engine needs that still aren't there
        bool humanDisc = false, martianDisc = false; // which discs were copied
    };

    // Throws std::runtime_error on a failure that stops the install (can't write the target folder, ...).
    InstallResult Install(const std::vector<Disc*>& discs, const InstallOptions& options, const Progress& progress);

    // The same in two steps, for one drive and the discs swapped by hand: CopyDisc for each disc as it is
    // inserted (in any order), then Finish.
    void CopyDisc(Disc& disc, const InstallOptions& options, const Progress& progress, InstallResult& result);
    void Finish(const InstallOptions& options, InstallResult& result);
    // The music folder a disc's tracks go to: named after the disc (see 3. above).
    const char* MusicFolder(Side discSide);

    // Files and folders on the discs that aren't copied (the same list WoWLauncher removes).
    bool Skipped(const std::string& discPath);
}
