// MODERN (docs/MODERN.md "The civil wars"): a civil war's start, made when one begins from the faction's own start
// file (Human.000 for the Humans', Martian.000 for the Martians'), so no start files of the engine's need shipping.
//
// The start file is the player's faction's, whose H lists hold that faction and A lists the other (core/Campaign.h).
// The layout is a first draft, to be evened out (the owner, update 236):
//   Human Civil War, from Human.000: the Martians taken out; the Human objects and sectors in Scotland
//     (CivilWarScotland) stay the player's, every other one goes to the other side (London's); the player gets a
//     copy of London's objects (its HQ) in the Grampian Mountains (3), where the Martians were.
//   Martian Civil War, from Martian.000 (after the Humans have lost): the Humans taken out; the Martians' base in
//     the Grampian Mountains becomes the other side's, and the player gets a copy of it in London (30). Every other
//     sector is no one's.
// Neither has pods (both cleared here; the war map lands none in a civil war either). The research holds the
// player's side's block and then the other's, both the start file's block for the faction. The message list is
// emptied. The owners keep the save's faction codes: the player's side the player's code, the other side the other.
//
// The layout is data (update 237): which sectors go to which side, and where the player's copy of a base comes
// from and goes, are a CivilWarLayout, read from dat\CivilWar.txt when there is one (the engine's own dat folder
// is read first, so the copy shipped beside the program is the one used; --no-loose leaves it out) and otherwise
// the draft above (CivilWarLayout::Default). The homes stay where they are: the war map, the battle map and the
// endings know the Humans' as the Grampian Mountains (3) or London (30) by the campaign, so the file doesn't move
// them. The file's form is in dat\CivilWar.txt itself.
#pragma once
#include "core/Campaign.h"
#include "core/SaveFile.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace wow
{
    // the sectors that are Scotland: North Western Highlands, Isle of Skye, Grampian Mountains, Strathmore, Argyll,
    // Southern Uplands, Edinburgh, Galloway (TEXT.ojd 0x003 on)
    inline constexpr std::array<int, 8> CivilWarScotland = { 1, 2, 3, 4, 6, 7, 8, 9 };
    inline constexpr int CivilWarNorthHome = 3, CivilWarSouthHome = 30;

    struct CivilWarLayout
    {
        // Where each sector the start file's faction holds goes: the player's side, no one's (its objects taken
        // out), or the other side's. "rest" in one of the three lists (restTo) takes the sectors the others don't
        // name. Sectors the other faction held are no one's, their objects taken out, whatever the lists say.
        enum class To { Player, Other, Neutral };
        std::vector<int> player, other, neutral;
        To restTo = To::Other;
        // the player's home is given a copy of this sector's objects in the start file (0: none), their middle put
        // at copyX, copyY on the war map
        int copyFrom = 0, copyX = 0, copyY = 0;

        To Where(int sector) const;
        // the draft (update 236) for a civil war
        static CivilWarLayout Default(Campaign campaign);
        // The section for the campaign in a CivilWar.txt; what's missing from it is the draft's. Lines it can't
        // read are left out, each noted in warnings.
        static CivilWarLayout Parse(std::string_view text, Campaign campaign, std::vector<std::string>& warnings);
        // MODERN (update 246): the same lines in a custom campaign's file (core/CustomCampaign.h), which has no
        // sections; the keys named in `skip` are the custom campaign's own, and are left to it
        static CivilWarLayout ParseLines(std::string_view text, Campaign campaign, const std::vector<std::string>& skip, std::vector<std::string>& warnings);
    };

    // Throws FormatError when the start file's research can't be split into its two blocks.
    SaveFile MakeCivilWarStart(const SaveFile& start, Campaign campaign, const CivilWarLayout& layout);
    inline SaveFile MakeCivilWarStart(const SaveFile& start, Campaign campaign) { return MakeCivilWarStart(start, campaign, CivilWarLayout::Default(campaign)); }
}
