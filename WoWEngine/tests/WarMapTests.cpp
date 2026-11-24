// The war map's rules and tables (warmap/WarMapData.h, core/AStar.h), read from the original executable: checks
// that they agree with each other, and the calendar and route search the war map uses.
#include "Test.h"
#include "core/AStar.h"
#include "warmap/WarMapData.h"

#include <set>
#include <string>
#include <vector>

using namespace wow;
using namespace wow::warmap;

TEST("war map: every sector's waypoint is inside its bounds")
{
    for (int s = 1; s <= SectorCount; s++)
    {
        const auto p = Waypoint(s);
        const SectorInfo& b = Sectors[s];
        CHECK(p.x >= b.left && p.x <= b.right && p.y >= b.top && p.y <= b.bottom);
        CHECK(b.left < b.right && b.top < b.bottom && b.right <= MapWidth && b.bottom <= MapHeight);
    }
}

TEST("war map: sectors that share a border are neighbours both ways")
{
    for (int a = 1; a <= SectorCount; a++)
    {
        CHECK(!Adjacent(a, a));
        for (int b = 1; b <= SectorCount; b++) CHECK_EQ(Adjacent(a, b), Adjacent(b, a));
    }
    CHECK(!Adjacent(0, 1));
    CHECK(!Adjacent(1, 31));
}

TEST("war map: every land link joins two neighbours")
{
    for (int a = 1; a <= SectorCount; a++)
        for (int b = 1; b <= SectorCount; b++)
            if (LandLink(a, b)) CHECK(Adjacent(a, b));
}

TEST("war map: sector map values")
{
    CHECK_EQ(SectorOfValue(0), 0);
    CHECK_EQ(SectorOfValue(30), 30);
    CHECK_EQ(SectorOfValue(31), 0);
    CHECK_EQ(BorderOfValue(32), 1);
    CHECK_EQ(BorderOfValue(117), 86);
    CHECK_EQ(BorderOfValue(5), 0);
    CHECK_EQ(NameText(30), 0x020);          // "London"
}

TEST("calendar: hours carry into days, months and years (no leap years, as the game)")
{
    uint16_t d[6] = { 0, 0, 23, 27, 1, 1898 };    // 28 February 1898, 23:00
    AdvanceDate(d, 1);
    CHECK_EQ(d[2], uint16_t(0));
    CHECK_EQ(d[3], uint16_t(0));
    CHECK_EQ(d[4], uint16_t(2));                  // 1 March
    CHECK_EQ(d[0], uint16_t(1));
    uint16_t e[6] = { 0, 0, 12, 30, 11, 1898 };   // 31 December 1898, noon
    AdvanceDate(e, 24);
    CHECK_EQ(e[3], uint16_t(0));
    CHECK_EQ(e[4], uint16_t(0));
    CHECK_EQ(e[5], uint16_t(1899));
}

TEST("calendar: route arrival labels, each side's way")
{
    const uint16_t now[6] = { 0, 0, 9, 3, 8, 1898 };   // 4 September 1898, 09:00
    auto month = [](int m) { const char* names[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" }; return std::string(names[m]); };
    CHECK_EQ(ArrivalText(now, 6.0f, true, month), std::string("03pm"));
    CHECK_EQ(ArrivalText(now, 30.0f, true, month), std::string("03pm 5 Sep"));
    CHECK_EQ(ArrivalText(now, 36.0f, false, month), std::string("1.50"));
}

TEST("route search: the shortest way on a small graph")
{
    // 1 - 2 - 3 - 4 in a line, and a costly shortcut 1 - 4
    auto next = [](int id) { std::vector<int> n; if (id > 1) n.push_back(id - 1); if (id < 4) n.push_back(id + 1); if (id == 1) n.push_back(4); if (id == 4) n.push_back(1); return n; };
    auto cost = [](int a, int b) { return (a == 1 && b == 4) || (a == 4 && b == 1) ? 100 : 10; };
    auto goal = [](int id, int) { return id == 4; };
    const std::vector<int> path = AStarSearch(1, 4, cost, next, goal, 1000);
    const std::vector<int> want = { 1, 2, 3, 4 };
    CHECK(path == want);
}

TEST("route search: nothing when there's no way")
{
    auto next = [](int id) { return id == 1 ? std::vector<int>{ 2 } : std::vector<int>{ 1 }; };
    auto cost = [](int, int) { return 1; };
    auto goal = [](int id, int) { return id == 9; };
    CHECK(AStarSearch(1, 9, cost, next, goal, 1000).empty());
}

TEST("route search: every sector reaches London over its neighbours")
{
    auto next = [](int s) { std::vector<int> n; for (int b = 1; b <= SectorCount; b++) if (Adjacent(s, b)) n.push_back(b); return n; };
    auto cost = [](int, int) { return 1; };
    auto goal = [](int s, int) { return s == 30; };
    std::set<int> islands;
    for (int s = 1; s <= SectorCount; s++)
        if (AStarSearch(s, 30, cost, next, goal, 1000).empty()) islands.insert(s);
    // by land, only the sectors the sea cuts off can't: the islands (Landlocked() is false and they have no land way)
    for (int s : islands) CHECK(next(s).empty() || s == 30);
}
