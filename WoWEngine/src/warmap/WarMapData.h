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
#include <string>

namespace wow::warmap
{
    constexpr int SectorCount = 30;
    constexpr int MapWidth = 1498, MapHeight = 2544;
    constexpr int MapGType = 0x7D, SectorMapGType = 0x7E;
    constexpr int MiniMapGType = 0x7F, MiniMapHighlightGType = 0x82, MiniSectorGType = 0x83;
    constexpr int PaletteGroup = 0x2ED; // hw: {S}W{S}W in slot 6, {S}W{S}I in 5, {S}WGI in 9
    constexpr int NormalLevel = 11;     // 0x49BE70 sets every value's level to 0xB

    // The sector table at 0x4BAE78, 32 bytes a sector: its bounds on the map (left, top, right, bottom; the war
    // map centres the view on them, 0x418980), then two points that belong to the NEXT sector: 0x437380 reads a
    // sector's route waypoint from 0x4BAE68 + sector x 32, which is the previous entry's ax, ay. So sector n's
    // waypoint is Sectors[n - 1].ax, ay (sector 30's, 1111, 2034, is in London); bx, by isn't worked out yet.
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

    // The point routes pass through in a sector (0x437380).
    struct Point { int x, y; };
    inline Point Waypoint(int sector)
    {
        if (sector < 1 || sector > SectorCount) return { 0, 0 };
        return { Sectors[sector - 1].ax, Sectors[sector - 1].ay };
    }

    // The point naval routes pass through in a sector (0x4373A0: the record's next two numbers); 0, 0 inland.
    inline Point SeaWaypoint(int sector)
    {
        if (sector < 1 || sector > SectorCount) return { 0, 0 };
        return { Sectors[sector - 1].bx, Sectors[sector - 1].by };
    }

    // The sector links (a table of pointers at 0x4BCEB0, one list per sector, 36 bytes a link, read by the route
    // arrows 0x4244E0): the neighbour, then two points where a route over land crosses from this sector into it
    // (leaving, arriving), then two for a route by sea; all zero where there's no such way.
    struct SectorLink { int from, to; int land[4]; int sea[4]; };
    inline constexpr SectorLink SectorLinks[] = {
        { 1, 2, { 332, 401, 289, 405 }, { 184, 417, 141, 421 } },
        { 1, 3, { 473, 407, 416, 401 }, { 0, 0, 0, 0 } },
        { 1, 4, { 513, 399, 509, 428 }, { 601, 287, 581, 320 } },
        { 1, 6, { 0, 0, 0, 0 }, { 84, 561, 41, 565 } },
        { 1, 8, { 0, 0, 0, 0 }, { 937, 419, 897, 428 } },
        { 2, 1, { 289, 439, 334, 435 }, { 141, 455, 186, 451 } },
        { 2, 3, { 343, 520, 333, 577 }, { 0, 0, 0, 0 } },
        { 2, 4, { 0, 0, 0, 0 }, { 378, 2, 386, 52 } },
        { 2, 6, { 334, 659, 289, 666 }, { 126, 675, 81, 658 } },
        { 2, 9, { 0, 0, 0, 0 }, { 84, 897, 34, 904 } },
        { 3, 1, { 421, 436, 477, 445 }, { 0, 0, 0, 0 } },
        { 3, 2, { 372, 580, 382, 523 }, { 0, 0, 0, 0 } },
        { 3, 4, { 565, 500, 553, 589 }, { 0, 0, 0, 0 } },
        { 3, 6, { 408, 694, 372, 678 }, { 0, 0, 0, 0 } },
        { 3, 7, { 514, 650, 455, 675 }, { 0, 0, 0, 0 } },
        { 4, 1, { 539, 433, 544, 401 }, { 599, 337, 620, 301 } },
        { 4, 2, { 0, 0, 0, 0 }, { 574, 53, 580, 2 } },
        { 4, 3, { 598, 569, 610, 480 }, { 0, 0, 0, 0 } },
        { 4, 7, { 607, 659, 586, 624 }, { 0, 0, 0, 0 } },
        { 4, 8, { 690, 627, 645, 634 }, { 834, 611, 789, 618 } },
        { 4, 11, { 0, 0, 0, 0 }, { 874, 651, 825, 658 } },
        { 5, 6, { 0, 0, 0, 0 }, { 176, 1175, 213, 1160 } },
        { 5, 9, { 0, 0, 0, 0 }, { 408, 1232, 420, 1212 } },
        { 5, 12, { 0, 0, 0, 0 }, { 460, 1260, 448, 1284 } },
        { 5, 15, { 0, 0, 0, 0 }, { 459, 1383, 433, 1414 } },
        { 5, 18, { 0, 0, 0, 0 }, { 460, 1427, 437, 1452 } },
        { 5, 22, { 0, 0, 0, 0 }, { 236, 1580, 177, 1568 } },
        { 6, 1, { 0, 0, 0, 0 }, { 41, 599, 86, 595 } },
        { 6, 2, { 289, 700, 334, 693 }, { 81, 716, 126, 709 } },
        { 6, 3, { 357, 709, 393, 725 }, { 0, 0, 0, 0 } },
        { 6, 5, { 0, 0, 0, 0 }, { 197, 1122, 160, 1137 } },
        { 6, 7, { 374, 791, 382, 863 }, { 0, 0, 0, 0 } },
        { 6, 9, { 405, 978, 360, 985 }, { 265, 994, 220, 1001 } },
        { 7, 3, { 455, 709, 514, 684 }, { 0, 0, 0, 0 } },
        { 7, 4, { 550, 647, 571, 682 }, { 0, 0, 0, 0 } },
        { 7, 6, { 440, 870, 432, 798 }, { 0, 0, 0, 0 } },
        { 7, 8, { 513, 783, 518, 852 }, { 0, 0, 0, 0 } },
        { 7, 9, { 506, 997, 457, 992 }, { 0, 0, 0, 0 } },
        { 7, 10, { 534, 957, 529, 991 }, { 0, 0, 0, 0 } },
        { 8, 1, { 0, 0, 0, 0 }, { 895, 469, 936, 473 } },
        { 8, 4, { 645, 668, 690, 661 }, { 773, 664, 830, 677 } },
        { 8, 7, { 563, 845, 558, 776 }, { 0, 0, 0, 0 } },
        { 8, 10, { 647, 916, 602, 923 }, { 0, 0, 0, 0 } },
        { 8, 11, { 699, 873, 678, 905 }, { 779, 777, 758, 809 } },
        { 8, 14, { 0, 0, 0, 0 }, { 919, 889, 890, 909 } },
        { 9, 2, { 0, 0, 0, 0 }, { 53, 990, 105, 970 } },
        { 9, 5, { 0, 0, 0, 0 }, { 376, 1188, 364, 1208 } },
        { 9, 6, { 360, 1019, 405, 1012 }, { 220, 1035, 265, 1028 } },
        { 9, 7, { 457, 1026, 506, 1031 }, { 0, 0, 0, 0 } },
        { 9, 10, { 557, 1056, 579, 1085 }, { 0, 0, 0, 0 } },
        { 9, 12, { 617, 1097, 569, 1102 }, { 465, 1193, 437, 1214 } },
        { 10, 7, { 583, 1004, 588, 970 }, { 0, 0, 0, 0 } },
        { 10, 8, { 602, 957, 647, 950 }, { 0, 0, 0, 0 } },
        { 10, 9, { 621, 1074, 593, 1039 }, { 0, 0, 0, 0 } },
        { 10, 11, { 699, 1008, 718, 1043 }, { 0, 0, 0, 0 } },
        { 10, 12, { 685, 1154, 656, 1118 }, { 0, 0, 0, 0 } },
        { 10, 13, { 762, 1192, 717, 1199 }, { 0, 0, 0, 0 } },
        { 10, 14, { 759, 1147, 759, 1181 }, { 0, 0, 0, 0 } },
        { 11, 4, { 0, 0, 0, 0 }, { 829, 720, 878, 717 } },
        { 11, 8, { 715, 931, 736, 899 }, { 795, 835, 816, 803 } },
        { 11, 10, { 763, 1036, 744, 1001 }, { 0, 0, 0, 0 } },
        { 11, 14, { 852, 1107, 810, 1109 }, { 952, 1095, 910, 1097 } },
        { 11, 17, { 0, 0, 0, 0 }, { 1020, 1111, 986, 1137 } },
        { 12, 5, { 0, 0, 0, 0 }, { 464, 1296, 476, 1272 } },
        { 12, 9, { 585, 1136, 630, 1129 }, { 465, 1240, 490, 1213 } },
        { 12, 10, { 631, 1144, 653, 1183 }, { 0, 0, 0, 0 } },
        { 12, 13, { 680, 1280, 685, 1321 }, { 0, 0, 0, 0 } },
        { 12, 15, { 701, 1385, 656, 1392 }, { 525, 1389, 480, 1396 } },
        { 12, 18, { 0, 0, 0, 0 }, { 517, 1425, 468, 1420 } },
        { 13, 10, { 717, 1233, 762, 1226 }, { 0, 0, 0, 0 } },
        { 13, 12, { 726, 1323, 721, 1282 }, { 0, 0, 0, 0 } },
        { 13, 14, { 809, 1263, 844, 1311 }, { 0, 0, 0, 0 } },
        { 13, 15, { 793, 1438, 759, 1400 }, { 0, 0, 0, 0 } },
        { 13, 16, { 863, 1443, 818, 1461 }, { 0, 0, 0, 0 } },
        { 13, 17, { 884, 1378, 871, 1413 }, { 0, 0, 0, 0 } },
        { 14, 8, { 0, 0, 0, 0 }, { 919, 959, 948, 939 } },
        { 14, 10, { 804, 1174, 804, 1140 }, { 0, 0, 0, 0 } },
        { 14, 11, { 813, 1140, 852, 1141 }, { 913, 1128, 952, 1129 } },
        { 14, 13, { 880, 1285, 845, 1237 }, { 0, 0, 0, 0 } },
        { 14, 17, { 995, 1315, 950, 1322 }, { 1107, 1255, 1070, 1278 } },
        { 14, 21, { 0, 0, 0, 0 }, { 1183, 1351, 1146, 1374 } },
        { 15, 5, { 0, 0, 0, 0 }, { 471, 1448, 497, 1417 } },
        { 15, 12, { 656, 1426, 701, 1419 }, { 480, 1430, 525, 1423 } },
        { 15, 13, { 735, 1439, 769, 1477 }, { 0, 0, 0, 0 } },
        { 15, 16, { 769, 1533, 743, 1591 }, { 0, 0, 0, 0 } },
        { 15, 18, { 624, 1619, 601, 1584 }, { 564, 1519, 545, 1480 } },
        { 15, 19, { 711, 1646, 666, 1653 }, { 0, 0, 0, 0 } },
        { 16, 13, { 834, 1496, 879, 1478 }, { 0, 0, 0, 0 } },
        { 16, 15, { 782, 1602, 808, 1544 }, { 0, 0, 0, 0 } },
        { 16, 17, { 914, 1500, 938, 1531 }, { 0, 0, 0, 0 } },
        { 16, 19, { 804, 1689, 783, 1657 }, { 0, 0, 0, 0 } },
        { 16, 20, { 929, 1657, 884, 1664 }, { 0, 0, 0, 0 } },
        { 16, 21, { 961, 1586, 961, 1620 }, { 0, 0, 0, 0 } },
        { 17, 11, { 0, 0, 0, 0 }, { 1025, 1176, 1056, 1153 } },
        { 17, 13, { 910, 1428, 923, 1393 }, { 0, 0, 0, 0 } },
        { 17, 14, { 950, 1356, 995, 1349 }, { 1090, 1312, 1127, 1289 } },
        { 17, 16, { 970, 1498, 946, 1467 }, { 0, 0, 0, 0 } },
        { 17, 21, { 1078, 1516, 1033, 1523 }, { 1198, 1496, 1153, 1503 } },
        { 17, 26, { 0, 0, 0, 0 }, { 1202, 1492, 1169, 1515 } },
        { 18, 5, { 0, 0, 0, 0 }, { 456, 1482, 485, 1465 } },
        { 18, 12, { 0, 0, 0, 0 }, { 468, 1454, 517, 1459 } },
        { 18, 15, { 556, 1602, 581, 1641 }, { 504, 1514, 537, 1533 } },
        { 18, 19, { 607, 1704, 596, 1757 }, { 0, 0, 0, 0 } },
        { 18, 22, { 535, 1794, 490, 1801 }, { 383, 1786, 338, 1765 } },
        { 18, 27, { 0, 0, 0, 0 }, { 155, 1922, 82, 1889 } },
        { 19, 15, { 666, 1687, 711, 1680 }, { 0, 0, 0, 0 } },
        { 19, 16, { 754, 1685, 775, 1717 }, { 0, 0, 0, 0 } },
        { 19, 18, { 639, 1779, 650, 1726 }, { 0, 0, 0, 0 } },
        { 19, 20, { 808, 1767, 809, 1820 }, { 0, 0, 0, 0 } },
        { 19, 22, { 632, 1880, 632, 1840 }, { 0, 0, 0, 0 } },
        { 19, 23, { 725, 1900, 670, 1894 }, { 0, 0, 0, 0 } },
        { 19, 24, { 804, 1868, 772, 1900 }, { 0, 0, 0, 0 } },
        { 20, 16, { 884, 1698, 929, 1691 }, { 0, 0, 0, 0 } },
        { 20, 19, { 849, 1806, 845, 1768 }, { 0, 0, 0, 0 } },
        { 20, 21, { 1002, 1703, 1021, 1758 }, { 0, 0, 0, 0 } },
        { 20, 24, { 950, 1863, 897, 1863 }, { 0, 0, 0, 0 } },
        { 20, 25, { 1021, 1813, 999, 1844 }, { 0, 0, 0, 0 } },
        { 21, 14, { 0, 0, 0, 0 }, { 1170, 1420, 1207, 1397 } },
        { 21, 16, { 1007, 1630, 1013, 1598 }, { 0, 0, 0, 0 } },
        { 21, 17, { 1033, 1557, 1078, 1550 }, { 1153, 1537, 1198, 1530 } },
        { 21, 20, { 1060, 1733, 1036, 1686 }, { 0, 0, 0, 0 } },
        { 21, 25, { 1161, 1779, 1116, 1786 }, { 1454, 1626, 1445, 1674 } },
        { 21, 26, { 1186, 1722, 1185, 1758 }, { 1186, 1594, 1185, 1630 } },
        { 22, 5, { 0, 0, 0, 0 }, { 169, 1618, 229, 1621 } },
        { 22, 18, { 490, 1835, 535, 1828 }, { 326, 1811, 375, 1804 } },
        { 22, 19, { 580, 1848, 576, 1888 }, { 0, 0, 0, 0 } },
        { 22, 23, { 568, 1951, 561, 1987 }, { 532, 2091, 525, 2115 } },
        { 22, 27, { 0, 0, 0, 0 }, { 348, 2087, 397, 2091 } },
        { 23, 19, { 670, 1928, 725, 1934 }, { 0, 0, 0, 0 } },
        { 23, 22, { 601, 1999, 608, 1963 }, { 565, 2115, 564, 2083 } },
        { 23, 24, { 763, 1975, 777, 2029 }, { 0, 0, 0, 0 } },
        { 23, 27, { 700, 2151, 648, 2137 }, { 580, 2099, 564, 2085 } },
        { 23, 28, { 778, 2100, 760, 2141 }, { 0, 0, 0, 0 } },
        { 24, 19, { 808, 1932, 840, 1896 }, { 0, 0, 0, 0 } },
        { 24, 20, { 897, 1897, 950, 1897 }, { 0, 0, 0, 0 } },
        { 24, 23, { 822, 2017, 808, 1963 }, { 0, 0, 0, 0 } },
        { 24, 25, { 983, 1906, 994, 1946 }, { 0, 0, 0, 0 } },
        { 24, 28, { 928, 2057, 863, 2053 }, { 0, 0, 0, 0 } },
        { 24, 30, { 989, 1982, 969, 2024 }, { 0, 0, 0, 0 } },
        { 25, 20, { 1042, 1872, 1064, 1841 }, { 0, 0, 0, 0 } },
        { 25, 21, { 1116, 1820, 1161, 1813 }, { 1492, 1737, 1550, 1744 } },
        { 25, 24, { 1035, 1933, 1024, 1893 }, { 0, 0, 0, 0 } },
        { 25, 26, { 1236, 1840, 1274, 1869 }, { 1360, 1952, 1398, 1981 } },
        { 25, 28, { 0, 0, 0, 0 }, { 1481, 2156, 1437, 2142 } },
        { 25, 29, { 0, 0, 0, 0 }, { 1361, 2011, 1320, 2015 } },
        { 25, 30, { 1177, 1936, 1109, 1933 }, { 0, 0, 0, 0 } },
        { 26, 17, { 0, 0, 0, 0 }, { 1197, 1549, 1230, 1526 } },
        { 26, 21, { 1232, 1769, 1234, 1728 }, { 1232, 1641, 1234, 1600 } },
        { 26, 25, { 1292, 1843, 1254, 1814 }, { 1416, 1955, 1378, 1926 } },
        { 26, 29, { 0, 0, 0, 0 }, { 1448, 1967, 1414, 1970 } },
        { 27, 18, { 0, 0, 0, 0 }, { 70, 2023, 147, 1988 } },
        { 27, 22, { 0, 0, 0, 0 }, { 401, 2067, 348, 2059 } },
        { 27, 23, { 610, 2167, 670, 2185 }, { 526, 2115, 558, 2121 } },
        { 27, 28, { 752, 2216, 752, 2250 }, { 752, 2312, 752, 2346 } },
        { 27, 29, { 0, 0, 0, 0 }, { 896, 2364, 896, 2398 } },
        { 28, 23, { 805, 2162, 822, 2111 }, { 0, 0, 0, 0 } },
        { 28, 24, { 863, 2087, 928, 2091 }, { 0, 0, 0, 0 } },
        { 28, 25, { 0, 0, 0, 0 }, { 1368, 2269, 1400, 2311 } },
        { 28, 27, { 797, 2243, 797, 2209 }, { 797, 2339, 797, 2305 } },
        { 28, 29, { 1028, 2193, 1028, 2227 }, { 1032, 2297, 1032, 2331 } },
        { 28, 30, { 987, 2112, 1037, 2146 }, { 0, 0, 0, 0 } },
        { 29, 25, { 0, 0, 0, 0 }, { 1319, 2047, 1360, 2043 } },
        { 29, 26, { 0, 0, 0, 0 }, { 1408, 2028, 1442, 2025 } },
        { 29, 27, { 0, 0, 0, 0 }, { 961, 2395, 961, 2361 } },
        { 29, 28, { 1073, 2220, 1073, 2186 }, { 1077, 2324, 1077, 2290 } },
        { 29, 30, { 1126, 2151, 1171, 2144 }, { 1270, 2071, 1295, 2064 } },
        { 30, 24, { 1014, 2035, 1034, 1993 }, { 0, 0, 0, 0 } },
        { 30, 25, { 1099, 1967, 1167, 1970 }, { 0, 0, 0, 0 } },
        { 30, 28, { 1056, 2115, 1002, 2086 }, { 0, 0, 0, 0 } },
        { 30, 29, { 1171, 2110, 1126, 2117 }, { 1271, 2038, 1254, 2053 } },
    };

    // The from sector's link to another, whatever its points (0x4244E0 looks it up by the neighbour alone)
    inline const SectorLink* FindLink(int from, int to)
    {
        for (const SectorLink& l : SectorLinks)
            if (l.from == from && l.to == to) return &l;
        return nullptr;
    }

    inline const SectorLink* LandLink(int from, int to)
    {
        for (const SectorLink& l : SectorLinks)
            if (l.from == from && l.to == to && (l.land[0] || l.land[1])) return &l;
        return nullptr;
    }

    // The ways a route can go from sector to sector: the route finder's graph (31 lists at 0x4BD260) with each
    // pair's kind from the table 0x435710 builds at 0x4EAA18 (a switch on to x 100 + from; bit 1 widens to 7).
    // 0x42ABD0 lets land units (class 0-2) take 1, 2 or 4, boats (class 3) 0x10, and fliers (4) any of them.
    // What 8 means isn't known (only two links have it, both by sea).
    namespace RouteKind { inline constexpr int Land = 0x07, Sea = 0x10; }
    struct RouteLink { int a, b, kind; };
    inline constexpr RouteLink RouteLinks[] = {
        { 1, 2, 0x1f }, { 1, 3, 0xf }, { 1, 4, 0x1f }, { 2, 3, 0xf }, { 2, 6, 0x1f }, { 2, 9, 0x10 },
        { 3, 4, 0xf }, { 3, 6, 0xf }, { 3, 7, 0xf }, { 4, 7, 0xf }, { 4, 8, 0x1f }, { 4, 11, 0x10 },
        { 5, 9, 0x18 }, { 5, 12, 0x18 }, { 5, 15, 0x10 }, { 5, 18, 0x10 }, { 5, 22, 0x10 }, { 6, 7, 0xf },
        { 6, 9, 0x1f }, { 7, 8, 0xf }, { 7, 9, 0xf }, { 7, 10, 0xf }, { 8, 10, 0xf }, { 8, 11, 0x1f },
        { 8, 14, 0x10 }, { 9, 10, 0xf }, { 9, 12, 0x1f }, { 10, 11, 0xf }, { 10, 12, 0xf }, { 10, 13, 0xf },
        { 10, 14, 0xf }, { 11, 14, 0x1f }, { 11, 17, 0x10 }, { 12, 13, 0xf }, { 12, 15, 0x1f }, { 12, 18, 0x10 },
        { 13, 14, 0xf }, { 13, 15, 0xf }, { 13, 16, 0xf }, { 13, 17, 0xf }, { 14, 17, 0x1f }, { 14, 21, 0x10 },
        { 15, 16, 0xf }, { 15, 18, 0x1f }, { 15, 19, 0xf }, { 16, 17, 0xf }, { 16, 19, 0xf }, { 16, 20, 0xf },
        { 16, 21, 0xf }, { 17, 21, 0x1f }, { 17, 26, 0x10 }, { 18, 19, 0xf }, { 18, 22, 0x1f }, { 19, 20, 0xf },
        { 19, 22, 0xf }, { 19, 23, 0xf }, { 19, 24, 0xf }, { 20, 21, 0xf }, { 20, 24, 0xf }, { 20, 25, 0xf },
        { 21, 25, 0xf }, { 21, 26, 0x1f }, { 22, 23, 0x1f }, { 22, 27, 0x10 }, { 23, 24, 0xf }, { 23, 27, 0x1f },
        { 23, 28, 0xf }, { 24, 25, 0xf }, { 24, 28, 0xf }, { 24, 30, 0xf }, { 25, 26, 0x1f }, { 25, 29, 0x10 },
        { 25, 30, 0x1f }, { 26, 29, 0x10 }, { 27, 28, 0x1f }, { 27, 29, 0x10 }, { 28, 29, 0x1f }, { 28, 30, 0xf },
        { 29, 30, 0x1f },
    };

    // The route finder's node table (0x4BD260, built from 0x4BCF2C): each sector's neighbours in the order the A*
    // tries them, ended by -1.
    inline constexpr int NeighbourTable[SectorCount + 1][8] = {
        { -1 },
        { 2, 3, 4, -1 },
        { 1, 3, 6, 9, -1 },
        { 1, 2, 4, 6, 7, -1 },
        { 1, 3, 7, 8, 11, -1 },
        { 9, 12, 15, 18, 22, -1 },
        { 2, 3, 7, 9, -1 },
        { 3, 4, 6, 8, 9, 10, -1 },
        { 4, 7, 10, 11, 14, -1 },
        { 5, 6, 7, 10, 12, 2, -1 },
        { 7, 8, 9, 11, 12, 13, 14, -1 },
        { 8, 10, 14, 4, 17, -1 },
        { 5, 9, 10, 13, 15, 18, -1 },
        { 10, 12, 14, 15, 16, 17, -1 },
        { 10, 11, 13, 17, 8, 21, -1 },
        { 5, 12, 13, 16, 18, 19, -1 },
        { 13, 15, 17, 19, 20, 21, -1 },
        { 13, 14, 16, 21, 11, 26, -1 },
        { 5, 15, 19, 22, 12, -1 },
        { 15, 16, 18, 20, 22, 23, 24, -1 },
        { 16, 19, 21, 24, 25, -1 },
        { 16, 17, 20, 25, 26, 14, -1 },
        { 18, 19, 23, 5, 27, -1 },
        { 19, 22, 24, 27, 28, -1 },
        { 19, 20, 23, 25, 28, 30, -1 },
        { 20, 21, 24, 26, 30, 29, -1 },
        { 21, 25, 17, 29, -1 },
        { 23, 28, 22, 29, -1 },
        { 23, 24, 27, 29, 30, -1 },
        { 28, 30, 25, 26, 27, -1 },
        { 24, 25, 28, 29, -1 },
    };
    inline const int* Neighbours(int sector)
    {
        return sector >= 1 && sector <= SectorCount ? NeighbourTable[sector] : NeighbourTable[0];
    }

    inline int RouteLinkKind(int a, int b)
    {
        for (const RouteLink& l : RouteLinks)
            if ((l.a == a && l.b == b) || (l.a == b && l.b == a)) return l.kind;
        return 0;
    }

    // A sea crossing's two points, as LandLink gives a land one
    inline const SectorLink* SeaLink(int from, int to)
    {
        for (const SectorLink& l : SectorLinks)
            if (l.from == from && l.to == to && (l.sea[0] || l.sea[1])) return &l;
        return nullptr;
    }

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
    // (0x41F640 keeps speed / "Game Frequency" at +0x2C, 0x41F6C0 adds it), and each whole hour moves the date on
    // (0x41B770). The speed is the speed slider's position, 0-10 (a scrollbar of 11, 0x431E30), and the table at
    // 0x4BA470 is floats 0.0-10.0, so it's that many game hours every 30 updates.
    inline constexpr float Speeds[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }; // 0x4BA470

    // Moves a date (the DtTm words: hours elapsed, minute, hour, day and month from 0, year) on by some hours.
    inline void AdvanceDate(uint16_t (&d)[6], int hours);

    // A route's arrival label (0x41B9A0, called by 0x424160 and every game hour by 0x425080 with the hours still
    // to go): for the Humans the arrival's hour, "03pm" (the hour mod 12, so noon is "00pm"), with " 4 Sep" after
    // it unless it's within 24 hours on the same day, and the year when it isn't this year's; for the Martians
    // the days since the start at the arrival, with hundredths: "12.50". The month names are TEXT 0x521-0x52C
    // (MonthText, loaded by 0x41BF90 from the ids at 0x4B9F60).
    inline int MonthText(int month) { return 0x521 + month; } // month from 0
    template <typename MonthName>
    inline std::string ArrivalText(const uint16_t (&now)[6], float hours, bool human, MonthName monthName)
    {
        uint16_t at[6];
        for (int i = 0; i < 6; i++) at[i] = now[i];
        int whole = int(hours), minutes = now[1] + int((hours - float(whole)) * 60.0f);
        if (minutes >= 60) { minutes -= 60; whole++; }
        at[1] = uint16_t(minutes);
        AdvanceDate(at, whole);
        std::string text;
        if (human)
        {
            int h = at[2] % 12;
            text = { char('0' + h / 10), char('0' + h % 10) };
            text += at[2] >= 12 ? "pm" : "am";
            if (!(hours < 24.0f && at[3] == now[3]))
            {
                text += " " + std::to_string(at[3] + 1) + " " + monthName(at[4] % 12);
                if (at[5] != now[5]) text += " " + std::to_string(at[5]);
            }
        }
        else
        {
            int days = at[0] / 24, hundredths = (at[0] % 24) * 100 / 24;
            text = std::to_string(days) + "." + char('0' + hundredths / 10) + char('0' + hundredths % 10);
        }
        return text;
    }

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
