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
#pragma once
#include "core/Campaign.h"
#include "core/SaveFile.h"

#include <array>

namespace wow
{
    // the sectors that are Scotland: North Western Highlands, Isle of Skye, Grampian Mountains, Strathmore, Argyll,
    // Southern Uplands, Edinburgh, Galloway (TEXT.ojd 0x003 on)
    inline constexpr std::array<int, 8> CivilWarScotland = { 1, 2, 3, 4, 6, 7, 8, 9 };
    inline constexpr int CivilWarNorthHome = 3, CivilWarSouthHome = 30;

    // Throws FormatError when the start file's research can't be split into its two blocks.
    SaveFile MakeCivilWarStart(const SaveFile& start, Campaign campaign);
}
