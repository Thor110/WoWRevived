// The Briefing (0x499930 / 0x4999E0) and Debriefing (0x49A7B0 / 0x49A820): the full-screen views the war map
// shows on either side of a battle (made at 0x41A17A and 0x41A108).
//
// Both are Fullscreen Views (0x4061B0) with the backdrop GType 0x91 in palette group 0x2EE (hb), built from text
// objects in font 0x73 (line height = the font's height + 2) laid out down the left half of the screen:
//   columns    Human games x 55 and 285, Martian games 85 and 276; the centre column half-way between them
//   first y    170 for the Human player, 72 for the Martian (the local player's side, dword_4E9B74 +0x24)
//   colours    [Colours] "Briefing Sector Name" (17) for the heading, "Briefing Heading" (17) for the section
//              titles and "Briefing Detail" (18) for the rest
//   mini map   the sector's battle map in miniature (RTypeMiniBattleMap 0x4A1F90, maps\hmin%02d.spr or
//              mmin%02d.spr with the sector's number, shade slot 0x15 level 0), centred in the box x 354-609,
//              y 95-380
// Text is anchored as the game's text objects are: flag 1 from the left at x, 2 centred on x, 8 ending at x.
//
// The Briefing: "VIEWING" (0x497) when the sector hasn't both sides in it (0x43D010), otherwise "ATTACKING"
// (0x498) when it isn't the player's, else "DEFENDING" (0x499), with the sector's name below; then "Local
// Conditions:" (0x4A0), "Date/Time:" (0x4A1) with the time and the date on the next line (Human games, the
// calendar's 0x41B440 "hh:mmap dd mmm yyyy" split after the am/pm) or the day ("%07.2f" of the game time in
// days, Martian games); and, if the player has units or constructions there, "Forces Present:" (0x4A4),
// "Units (Vehicles):" (0x4A6) "%d (%d)" and "Facilities:" (0x4A7) "%d". Any key or a click sends 0x98 at once;
// otherwise 0x71 comes after [Briefing] "Delay" (30) ticks. Both go to the battle (0x41A082).
//
// The Debriefing: with no battle fought (the report's +0x54 clear), "RETURNING TO WAR ROOMS" (0x4A8) and the
// sound 0x18E; with one, "VICTORY!" (0x4A9, sound 0x18F) if the player has anything left there (+0x28 + +0x48),
// else "DEFEAT" (0x4AA, sound 0x190, also whenever byte_4E9B94 is set). After a battle: the result, 0x4AB ("...
// were eliminated") or 0x4AC ("... are occupying this sector"), wrapped to the columns' span; "Forces lost:"
// (0x4AD) and "Enemy forces destroyed:" (0x4AE), each with "Units (Vehicles):" and "Facilities:". The sound
// fades in over 60 ticks. Any key or a click sends 0x99 five ticks later; otherwise 0x8017 comes [Debriefing]
// "Delay" (300) ticks after it's first drawn. Both go back to the war map.
//
// The section titles' text objects have +0x3C set, which nothing in the text drawing (0x40F770) reads; what it's
// for is unknown.
#pragma once
#include "frontend/Screen.h"

#include <cstdint>
#include <string>
#include <vector>

namespace wow
{
    struct BattleColours
    {
        int sectorName = 17; // [Colours] "Briefing Sector Name" (word_4B9430)
        int heading = 17;    // [Colours] "Briefing Heading" (word_4B9434)
        int detail = 18;     // [Colours] "Briefing Detail" (word_4B9438)
    };

    // What the briefing shows, from the war map
    struct BriefingInfo
    {
        int sector = 0;
        int heading = 0x497;         // 0x497 VIEWING, 0x498 ATTACKING, 0x499 DEFENDING
        uint16_t dateTime[6] = {};   // the calendar (DtTm)
        float hours = 0;             // the game time (CALN)
        int units = 0, vehicles = 0; // the player's groups there and the vehicles in them
        int facilities = 0;          // and constructions
    };

    // What the debriefing shows, from the battle report (dword_4E9B7C)
    struct DebriefingInfo
    {
        int sector = 0;
        bool fought = false;         // +0x54
        bool forfeit = false;        // byte_4E9B94
        bool survivors = false;      // +0x28 + +0x48: the player has something left there
        int lostUnits = 0, lostVehicles = 0, lostFacilities = 0;       // +0x58, +0x60, +0x18 - +0x28
        int killedUnits = 0, killedVehicles = 0, killedFacilities = 0; // +0x5C, +0x64, +0x1C - +0x2C
    };

    class BattleScreen : public Screen
    {
    public:
        BattleScreen(int id, Resources& res, BattleColours colours);
        void Draw(Canvas& canvas) const override;
        int CursorAt(int, int) const override { return 0; }

    protected:
        static constexpr int TextFont = 0x73;
        struct Line { std::string text; int x, y, flags, level; bool wrap = false; };
        void Add(std::string text, int x, int flags, int level);
        void AddPair(int label, std::string value);  // a label in the left column, its value ending at the right
        void SetMap(int sector) { mapSector = sector; }

        BattleColours colours;
        int left = 0, right = 0, centre = 0, y = 0, lineHeight = 0;
        std::vector<Line> lines;
        int mapSector = 0;
    };

    class Briefing : public BattleScreen
    {
    public:
        static constexpr int Id = 0x8019;
        Briefing(Resources& res, BattleColours colours, const BriefingInfo& info, int delay);

        void Update() override;
        std::optional<Command> OnKey(int sdlKey) override;
        std::optional<Command> OnMouseButton(int x, int y, bool down) override;
        std::optional<Command> OnRightButton(int x, int y, bool down) override;

    private:
        int ticksLeft;
        bool done = false;
        std::optional<Command> Leave();
    };

    class Debriefing : public BattleScreen
    {
    public:
        static constexpr int Id = 0x97;
        Debriefing(Resources& res, BattleColours colours, const DebriefingInfo& info, int delay);

        // the sound it plays (0x18E, 0x18F or 0x190)
        int Sound() const { return sound; }
        void Update() override;
        std::optional<Command> OnKey(int sdlKey) override;
        std::optional<Command> OnMouseButton(int x, int y, bool down) override;
        std::optional<Command> OnRightButton(int x, int y, bool down) override;

    private:
        int sound = 0x18E;
        int ticksLeft;
        int leaveIn = -1;
        bool done = false;
        void Leave();
    };
}
