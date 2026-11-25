// MODERN (update 246, docs/CUSTOM.md): custom campaigns. Each is a text file in the Campaigns folder of the main
// folder (the game's; since update 247, and the engine's own beside the program or in the source folder, as its
// loose dat is; the game's first where both have a file of the same name), saying what kind of
// campaign it is (a Human or Martian one, or either civil war), which start file it begins from, whether the pods
// land, and, for a civil war, the start's layout in CivilWar.txt's lines. Started from the main menu's "Custom
// Campaigns" (under the civil war), saved as SaveGame\Custom.001 on with the engine's tag (core/Campaign.h).
#pragma once
#include "core/Campaign.h"
#include "core/CivilWar.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wow
{
    struct CustomCampaign
    {
        std::string file;           // the definition's file name
        std::filesystem::path folder;   // the Campaigns folder it was found in (its start file is looked for there)
        std::string name;           // shown in the menu: "name =", else the file's name without .txt
        Campaign kind = Campaign::Regular;
        bool human = true;          // the player's faction (a civil war's is its own)
        std::string start;          // "start =": a start file in its Campaigns folder, else empty (Human.000 / Martian.000)
        bool pods = true;           // "pods =": on unless off; never in a civil war
        CivilWarLayout layout;      // a civil war's start (as CivilWar.txt's section)

        // the campaign word's flags for its saves (core/Campaign.h)
        uint32_t Flags() const;

        // MODERN (update 264): its own files beside it in its Campaigns folder, named as it is (the .txt's name
        // without .txt, any case), each optional: "<name>.intro.mp4" or ".smk" played as it starts, "<name>.outro"
        // the same when it's won (in place of VICTORY), "<name>.briefing.txt" shown before the war map, and a folder
        // "<name>" laid out as TEXTURES for its units' and buildings' new looks
        std::string Stem() const;
        std::optional<std::filesystem::path> Movie(const char* which) const;      // "intro" or "outro"
        std::optional<std::filesystem::path> Briefing() const;
        std::optional<std::filesystem::path> Reskins() const;

        // Throws nothing: lines it can't read are noted in warnings (and left out). `ok` is false without a kind.
        static CustomCampaign Parse(std::string_view text, std::string file, std::vector<std::string>& warnings, bool& ok);
    };
}
