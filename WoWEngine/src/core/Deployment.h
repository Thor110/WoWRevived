// The battle map's deployment (0x421FE0 / 0x4222C0, called by EnterBattleMap 0x439770 for each of the sector's six
// lists): where each war map object's battle units start. See docs/BATTLEMAP.md.
#pragma once
#include "core/Landscape.h"
#include <functional>
#include <utility>
#include <vector>

namespace wow
{
    // A war map object in the sector, as the battle map takes it: its type, which of the sector's lists it's in (0
    // HUNI, 1 AUNI, 2 HCON, 3 ACON, 4 HRES, 5 ARES: the H lists the side being played), the sector it came from
    // (+0x58, 0 for none) and its battle units' saved places (+0x78 / +0x7C, each BAMO's second and third floats)
    struct BattleGroup
    {
        int type = 0;
        int list = 0;
        int previous = 0;
        int ref = -1;           // the war map object (its index in the war map's table) the group stands for
        int aiWeight = 0;       // AI.ojd's weight of its type (word_518B10)
        int aiClass = 0;        // AI.ojd's class of its type (0x518B14 via 0x441EB0)
        std::vector<std::pair<float, float>> places;
        std::vector<int> healths;   // each battle unit's saved health (+0xC4, BAMO's last value); empty: none known
        // a construction's or resource's state, the war map object's +0x7C / +0x78 (0x48E9D0 and 0x48F7C0 read it
        // through the battle object's +0x48): under 1 while it's being built
        float progress = 1.0f;
        bool Players() const { return (list & 1) == 0; }
    };

    namespace deploy
    {
        // The command buildings (0x421FE0, 0x439B7E): the Martian one is 830 in sector 3 and 120 elsewhere, the Human
        // 278 (Parliament) in sector 30 and 58 (the Command Post) elsewhere. The game's `sub eax,1Eh; neg; sbb eax,eax;
        // and al,24h; add eax,116h` masks only the low byte, so the "else" case is 0xFFFFFF24 + 0x116, which wraps to
        // 0x3A, not 0x13A (0x422089, 0x439B94, 0x48ACA0, 0x495E0D and the rest).
        inline int MartianHq(int sector) { return sector == 3 ? 830 : 120; }
        inline int HumanHq(int sector) { return sector == 30 ? 278 : 58; }

        // 0x436050: where units coming from sector previous into sector enter its map, by the pair (a switch on
        // previous x 100 + sector): boats (move class 5 or 6) at its second point, the rest at its first. (0, 0) when
        // previous isn't one of the sector's neighbours (0x4BD260) or the pair has no point for them.
        void EntryPoint(int previous, int sector, int moveClass, int& x, int& y);
    }
}
