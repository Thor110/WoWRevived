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
        std::vector<std::pair<float, float>> places;
        bool Players() const { return (list & 1) == 0; }
    };

    namespace deploy
    {
        // The command buildings (0x421FE0, 0x439B7E): the Martian one is 830 in sector 3 and 120 elsewhere, the Human
        // 278 in sector 30 and 314 elsewhere
        inline int MartianHq(int sector) { return sector == 3 ? 830 : 120; }
        inline int HumanHq(int sector) { return sector == 30 ? 278 : 314; }

        // 0x436050: where units coming from sector previous into sector enter its map, by the pair (a switch on
        // previous x 100 + sector): boats (move class 5 or 6) at its second point, the rest at its first. (0, 0) when
        // previous isn't one of the sector's neighbours (0x4BD260) or the pair has no point for them.
        void EntryPoint(int previous, int sector, int moveClass, int& x, int& y);
    }
}
