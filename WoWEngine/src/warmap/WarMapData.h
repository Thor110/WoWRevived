// The war map's fixed data, read from the exe's data section (vanilla WoW_patched.exe).
//
// Sectors are numbered 1-30 (0 is "The Sea"); a sector's name is TEXT.ojd string 0x002 + sector.
//
// The map (GType 0x7D: HWM.SPR or MWM.SPR, 1498 x 2544, palette slot 6) has a twin, the sector map (GType 0x7E:
// HWMHI.SPR or MWMHI.SPR, the same size), whose pixels aren't colours but numbers:
//   1-30     the sector the pixel is in
//   32-117   a border line: border (value - 31). Borders 1-63 lie between two sectors, 65-86 are coastlines
//   0        the sea
// The highlight object (0x49BE70) gives every value a shade level (11 to start with) and the map is drawn with
// each pixel's colour from its own level, so a sector or a border can be lit or darkened by changing one entry.
#pragma once
#include <array>
#include <cstdint>

namespace wow::warmap
{
    constexpr int SectorCount = 30;
    constexpr int MapWidth = 1498, MapHeight = 2544;
    constexpr int MapGType = 0x7D, SectorMapGType = 0x7E;
    constexpr int MiniMapGType = 0x7F, MiniMapHighlightGType = 0x82, MiniSectorGType = 0x83;
    constexpr int PaletteGroup = 0x2ED; // hw: {S}W{S}W in slot 6, {S}W{S}I in 5, {S}WGI in 9
    constexpr int NormalLevel = 11;     // 0x49BE70 sets every value's level to 0xB

    // The sector table at 0x4BAE78, 32 bytes a sector: its bounds on the map (left, top, right, bottom; the war
    // map centres the view on them, 0x418980) and two more points, not worked out yet.
    struct SectorInfo { int left, top, right, bottom; int ax, ay, bx, by; };
    inline constexpr SectorInfo Sectors[SectorCount + 1] = {
        { 0, 0, 0, 0, 416, 274, 428, 26 },
        { 215, 85, 653, 454, 293, 542, 65, 506 },     { 118, 403, 393, 727, 461, 555, 0, 0 },
        { 346, 413, 595, 715, 679, 497, 895, 345 },   { 518, 354, 832, 691, 371, 1307, 371, 1306 },
        { 329, 1248, 409, 1360, 313, 803, 117, 883 }, { 179, 673, 433, 1013, 474, 834, 0, 0 },
        { 396, 616, 602, 1028, 643, 799, 771, 755 },  { 534, 627, 753, 949, 424, 1071, 240, 1171 },
        { 274, 993, 638, 1194, 679, 1064, 0, 0 },     { 551, 928, 799, 1215, 791, 999, 919, 943 },
        { 669, 855, 885, 1127, 624, 1251, 492, 1299 },{ 517, 1104, 735, 1416, 793, 1333, 0, 0 },
        { 682, 1194, 914, 1495, 881, 1226, 1017, 1158 }, { 771, 1113, 1047, 1355, 694, 1522, 538, 1474 },
        { 569, 1394, 813, 1673, 873, 1580, 0, 0 },    { 740, 1442, 1004, 1725, 1003, 1429, 1183, 1377 },
        { 889, 1303, 1131, 1556, 508, 1682, 260, 1546 }, { 313, 1519, 628, 1828, 717, 1787, 0, 0 },
        { 590, 1640, 852, 1940, 937, 1772, 0, 0 },    { 818, 1650, 1065, 1886, 1083, 1674, 1211, 1574 },
        { 960, 1525, 1229, 1810, 449, 1929, 123, 1956 }, { 192, 1804, 626, 2085, 689, 2026, 238, 2069 },
        { 576, 1905, 810, 2185, 900, 1974, 0, 0 },    { 777, 1866, 1031, 2092, 1169, 1876, 1377, 1972 },
        { 993, 1793, 1355, 2017, 1307, 1750, 1423, 1654 }, { 1187, 1647, 1415, 1920, 453, 2230, 69, 2454 },
        { 101, 2110, 780, 2459, 895, 2170, 862, 2463 }, { 758, 2061, 1068, 2316, 1197, 2174, 1353, 2294 },
        { 1037, 2064, 1365, 2275, 1111, 2034, 1379, 2042 }, { 978, 1941, 1269, 2149, 2, 332, 401, 289 },
    };

    // The border table at 0x4D0C00, 8 borders a sector (0 ends the list). Two sectors share a land border
    // (1-63); a coastline (65-86) belongs to one. 0x49BE70 turns this into border -> (sector, sector).
    inline constexpr int SectorBorders[SectorCount + 1][8] = {
        {},
        { 1, 2, 3, 65 }, { 1, 4, 5, 66 }, { 2, 4, 6, 7, 8 }, { 3, 8, 14, 17, 67 }, { 85 },
        { 5, 6, 9, 10, 68 }, { 7, 9, 11, 12, 13, 14 }, { 13, 17, 18, 19, 69 }, { 10, 11, 15, 16, 71 },
        { 12, 15, 18, 20, 21, 22, 23 }, { 19, 23, 24, 70 }, { 16, 20, 25, 26, 72 }, { 21, 25, 27, 28, 29, 31 },
        { 22, 24, 30, 31, 73 }, { 26, 27, 32, 33, 34, 75 }, { 28, 32, 35, 36, 37, 38 }, { 29, 30, 35, 39, 74 },
        { 34, 40, 41, 76 }, { 33, 38, 40, 42, 43, 44, 45 }, { 37, 42, 46, 47, 48 }, { 36, 39, 46, 49, 50, 86 },
        { 41, 45, 51, 77 }, { 44, 51, 52, 53, 54, 81 }, { 43, 48, 52, 55, 56, 57 }, { 47, 49, 57, 58, 59, 79 },
        { 50, 58, 78 }, { 54, 60, 82 }, { 53, 55, 60, 61, 62, 83 }, { 62, 63, 84 }, { 56, 59, 61, 63, 80 },
    };

    inline bool Adjacent(int a, int b)
    {
        if (a < 1 || b < 1 || a > SectorCount || b > SectorCount || a == b) return false;
        for (int x : SectorBorders[a])
            if (x > 0 && x < 64)
                for (int y : SectorBorders[b]) if (x == y) return true;
        return false;
    }

    // What a sector map value is: a sector (1-30), a border, or the sea (0).
    inline int SectorOfValue(int value) { return value >= 1 && value <= SectorCount ? value : 0; }
    inline int BorderOfValue(int value) { return value >= 32 ? value - 31 : 0; }
    inline int NameText(int sector) { return 0x002 + sector; }

    // The game clock (the calendar, 0x41F550): its time in hours goes up by Speeds[speed] / 30 every game update
    // (0x41F640, 0x41F6C0; 30 is "Game Frequency"), and each whole hour moves the date on (0x41B770).
    inline constexpr float Speeds[] = { 0, 1, 2, 3, 4, 6, 8, 10 }; // 0x4BA470

    // Moves a date (the DtTm words: hours elapsed, minute, hour, day and month from 0, year) on by some hours.
    inline void AdvanceDate(uint16_t (&d)[6], int hours)
    {
        static constexpr int Days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        d[0] = uint16_t(d[0] + hours);
        int hour = d[2] + hours;
        while (hour >= 24)
        {
            hour -= 24;
            int year = d[5], month = d[4];
            int length = Days[month % 12] + (month == 1 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) ? 1 : 0);
            if (++d[3] >= length) { d[3] = 0; if (++d[4] >= 12) { d[4] = 0; d[5]++; } }
        }
        d[2] = uint16_t(hour);
    }
}
