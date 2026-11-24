// MODERN (docs/MODERN.md "The civil wars"): which kind of campaign is being played, and each side's faction.
//
// The engine already thinks of the two sides as the player's and the other's, as the save does: the H lists
// (HUNI, HCON, HRES) hold the side being played and the A lists the other side, whatever their factions (in
// Martian.000 the Martians are in HUNI). Only the sectors' owners are factions (0 Human, 1 Martian, 2 no one), which
// the war map turns into the player's (0) or the other's (1) by the player's faction. So a side here is relative:
// 0 the player's, 1 the other's.
//
// In the regular game the other side is the other faction. In a civil war both sides are the player's faction.
// Where the engine wants the other side's faction (its unit types, command building, resource kinds, research
// tree, computer player's rules), it asks OtherFaction() rather than taking "not the player's".
#pragma once
#include <cstddef>
#include <cstdint>

namespace wow
{
    enum class Faction : int { Human = 0, Martian = 1 };

    enum class Campaign : uint32_t
    {
        Regular = 0,
        HumanCivilWar = 1,      // both sides Human: the player's in Scotland, the other London's
        MartianCivilWar = 2,    // both sides Martian: the player's in London, the other in Scotland
    };

    inline bool IsCivilWar(Campaign c) { return c != Campaign::Regular; }

    inline Faction PlayerFaction(bool playerHuman) { return playerHuman ? Faction::Human : Faction::Martian; }
    // side 0 (the player's) or 1 (the other's) to its faction
    inline Faction FactionOf(Campaign c, bool playerHuman, int side)
    {
        if (side == 0 || IsCivilWar(c)) return PlayerFaction(playerHuman);
        return playerHuman ? Faction::Martian : Faction::Human;
    }
    inline Faction OtherFaction(Campaign c, bool playerHuman) { return FactionOf(c, playerHuman, 1); }
    inline bool OtherHuman(Campaign c, bool playerHuman) { return OtherFaction(c, playerHuman) == Faction::Human; }

    // A sector's owner as the save keeps it (a faction code: 0 Human, 1 Martian, 2 no one) for a side. In a civil war
    // there is one faction and two sides, so the codes are taken as the regular game's: the player's side has the
    // player's faction's code and the other side the other code (the war map's conversion, owner to side by the
    // player's faction, works unchanged).
    inline int OwnerCode(bool playerHuman, int side) { return (side == 0) == playerHuman ? 0 : 1; }

    // the faction a civil war is fought within
    inline Faction CivilWarFaction(Campaign c) { return c == Campaign::MartianCivilWar ? Faction::Martian : Faction::Human; }

    // The engine's own saves (a civil war's) start with this tag, then u32 the tag's version (1) and u32 the
    // campaign; the original's save follows unchanged but for the research, which holds the player's side's block
    // and then the other side's (each with its faction's tag), where the original's has the Humans' and then the
    // Martians'. Their files have names of their own (SaveSlot), so the original game, which reads every
    // Human.00n / Martian.00n it finds, never meets one.
    inline constexpr char EngineSaveTag[4] = { 'W', 'O', 'W', 'E' };
    inline constexpr uint32_t EngineSaveVersion = 1;
    inline constexpr size_t EngineSaveHeader = 12;
}
