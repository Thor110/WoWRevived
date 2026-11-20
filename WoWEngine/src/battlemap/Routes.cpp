// The battle map's route finder (WRouteFinder 0x493DF0-0x494960 on the A* 0x44EE70, with the BatRoute cost
// 0x499420-0x4998D0) and a vehicle following a route (0x802E: 0x4911B0, 0x491790). See docs/BATTLEMAP.md "Routes".
#include "battlemap/BattleMap.h"
#include "core/AStar.h"
#include "core/Bridge.h"
#include "core/Random.h"
#include <climits>
#include <cmath>

namespace wow
{
    namespace
    {
        // 0x4D09F8: the turn penalty for the way to the goal against the way of the step (8 x 8, by how far apart
        // the two of the 8 directions are), and straight after it 0x4D0AF8, the direction of a difference (3 x 3 x 3
        // by the signs of dy, dx and |dx| - |dy|; 8 for none). An index of 64 or more (a direction of 8) reads on
        // into the second table, as the game does.
        constexpr int kTurn[64 + 27] = {
            1, 3, 6, 10, 16, 10, 6, 3,   3, 1, 3, 6, 10, 16, 10, 6,   6, 3, 1, 3, 6, 10, 16, 10,
            10, 6, 3, 1, 3, 6, 10, 16,   16, 10, 6, 3, 1, 3, 6, 10,   10, 16, 10, 6, 3, 1, 3, 6,
            6, 10, 16, 10, 6, 3, 1, 3,   3, 6, 10, 16, 10, 6, 3, 1,
            7, 7, 6, 0, 8, 8, 0, 1, 1,   8, 8, 6, 8, 8, 8, 8, 8, 2,   4, 5, 5, 4, 8, 8, 3, 3, 2 };

        int Direction(int dx, int dy)
        {
            auto three = [](int v) { return (v > 0 ? 1 : 0) + (v >= 0 ? 1 : 0); };
            const int d = std::abs(dx) - std::abs(dy);
            return kTurn[64 + (three(dy) * 3 + three(dx)) * 3 + three(d)];
        }

        // 0x43FF00: the larger difference and half the smaller
        int Octagonal(int x0, int y0, int x1, int y1)
        {
            const int a = std::abs(x1 - x0), b = std::abs(y1 - y0);
            return b - (std::min(a, b) >> 1) + a;
        }
    }

    // 0x493DF0 (from the landscape's set-up): a node every 4 vertices from 4 each way, (width - 4) / 4 across and
    // (height - 4) / 4 down, each joined to the ones round it in the order left, right, up, down, up-left,
    // down-right, up-right, down-left (those on the grid). Nothing is checked for crossing here; the costs do that.
    void BattleMap::SetUpRouteFinder()
    {
        rfCols = (land.Width() - 4) / 4;
        rfRows = (land.Height() - 4) / 4;
        if (rfCols < 1 || rfRows < 1)
        {
            rfCols = rfRows = 0;
            return;
        }
        const int count = rfCols * rfRows;
        rfX.assign(size_t(count), 0);
        rfY.assign(size_t(count), 0);
        rfFlags.assign(size_t(count), 0);
        rfLinks.assign(size_t(count), {});
        for (int j = 0; j < rfRows; j++)
            for (int i = 0; i < rfCols; i++)
            {
                const int n = j * rfCols + i;
                rfX[size_t(n)] = int16_t(i * 4 + 4);
                rfY[size_t(n)] = int16_t(j * 4 + 4);
                auto& l = rfLinks[size_t(n)];
                const bool left = i > 0, right = i < rfCols - 1, up = j > 0, down = j < rfRows - 1;
                if (left) l.push_back(n - 1);
                if (right) l.push_back(n + 1);
                if (up) l.push_back(n - rfCols);
                if (down) l.push_back(n + rfCols);
                if (left && up) l.push_back(n - rfCols - 1);
                if (right && down) l.push_back(n + rfCols + 1);
                if (right && up) l.push_back(n - rfCols + 1);
                if (left && down) l.push_back(n + rfCols - 1);
            }
    }

    // 0x4994B0 (vertices): Bresenham from the first to the last, each vertex's strict cost (0x480680) added up;
    // -1 at the first one that can't be crossed, else the straight length (whole) x the sum / the vertices counted
    int BattleMap::LineCost(int group, int x0, int y0, int x1, int y1) const
    {
        const int length = int(std::sqrt(double((x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1))));
        const int ax = std::abs(x1 - x0) * 2, ay = std::abs(y1 - y0) * 2;
        const int sx = x1 - x0 >= 0 ? 1 : -1, sy = y1 - y0 >= 0 ? 1 : -1;
        int x = x0, y = y0, sum = 0, n = 0;
        if (ax > ay)
        {
            int d = ay - (ax >> 1);
            for (;;)
            {
                const int c = CellCost(group, x, y, true);
                if (c == 0) return -1;
                sum += c;
                n++;
                if (x == x1) break;
                if (d >= 0)
                {
                    y += sy;
                    d -= ax;
                }
                x += sx;
                d += ay;
            }
        }
        else
        {
            int d = ax - (ay >> 1);
            for (;;)
            {
                const int c = CellCost(group, x, y, true);
                if (c == 0) return -1;
                sum += c;
                n++;
                if (y == y1) break;
                if (d >= 0)
                {
                    x += sx;
                    d -= ay;
                }
                y += sy;
                d += ax;
            }
        }
        return length * sum / n;
    }

    // 0x499660 (a, b, the goal at +0xC): the line's cost from a to b times (the turn penalty between the way to the
    // goal and the way to b) x 5 / 16 + 1. Nodes sharing a link byte (both set) cost their straight distance x the
    // group's +0x5A instead.
    // *Provisional*: no node has a link byte yet (the bridges don't add theirs), so that's never used.
    int BattleMap::StepCost(int group, int goal, int a, int b) const
    {
        const int axv = rfX[size_t(a)], ayv = rfY[size_t(a)];
        const int toGoal = Direction(rfX[size_t(goal)] - axv, rfY[size_t(goal)] - ayv);
        const int toB = Direction(rfX[size_t(b)] - axv, rfY[size_t(b)] - ayv);
        const int line = LineCost(group, axv, ayv, rfX[size_t(b)], rfY[size_t(b)]);
        return ((kTurn[toGoal + toB * 8] * 5 >> 4) + 1) * line;
    }

    // 0x494440 (start and goal in vertices, the radius in vertices, 4 for a move; the limit 200; the group): each
    // end to its node (((v - 4) / 4), kept to 1 .. count - 1). Within the radius of the start (radius / the float
    // distance over 1) the goal becomes the start: no route. Else the goal is pulled back the radius along the
    // line from the start and, when that line can be crossed (0x4994B0 not -1), that's where it goes: no route.
    // Otherwise an A* over the nodes (0x44EE70: StepCost, the goal reached within the radius by 0x43FF00 (0x499800))
    // is started; the group polls it (0x483B70 -> 0x483270), 200 neighbours tried each update.
    // *Provisional*: the search is run to its end at once.
    bool BattleMap::RequestRoute(int group, int sx, int sy, int& dx, int& dy, int radius, std::vector<int>& path)
    {
        path.clear();
        if (rfCols == 0) return false;
        auto node = [&](int x, int y) {
            int i = (x - 4) / 4, j = (y - 4) / 4;
            if (i >= rfCols) i = rfCols - 1;
            if (j >= rfRows) j = rfRows - 1;
            if (i < 1) i = 1;
            if (j < 1) j = 1;
            return j * rfCols + i;
        };
        const int start = node(sx, sy), goal = node(dx, dy);
        const float fdx = float(dx - sx), fdy = float(dy - sy);
        const float dist = float(std::sqrt(double(fdx) * fdx + double(fdy) * fdy));
        const float f = float(double(radius) / dist);
        if (f > 1.0f)
        {
            dx = sx;
            dy = sy;
            return false;
        }
        const int ny = int(double(dy) - double(dy - sy) * f);
        const int nx = int(double(dx) - double(dx - sx) * f);
        if (LineCost(group, sx, sy, nx, ny) >= 0)
        {
            dx = nx;
            dy = ny;
            return false;
        }
        auto cost = [&](int a, int b) { return StepCost(group, goal, a, b); };
        auto next = [&](int id) -> const std::vector<int>& { return rfLinks[size_t(id)]; };
        auto isGoal = [&](int a, int g) {
            return Octagonal(rfX[size_t(a)], rfY[size_t(a)], rfX[size_t(g)], rfY[size_t(g)]) <= radius;
        };
        path = AStarSearch(start, goal, cost, next, isGoal, INT_MAX);
        return true;
    }

    // 0x494690: a node for each step of the path (x, y, link byte). Each between the ends moves to the average of
    // it and the two either side (whole numbers, toward 0) when the lines from the one before and to the one after
    // can both be crossed (0x4994B0). Each node's heading (0x43FDA0, less 0x400): the first from it to the next,
    // the last from the one before, the rest from the one before to the one after.
    std::shared_ptr<const BattleMap::Route> BattleMap::BuildRoute(int group, const std::vector<int>& path) const
    {
        auto r = std::make_shared<Route>();
        const size_t n = path.size();
        if (n == 0) return r;
        r->nodes.resize(n);
        for (size_t k = 1; k + 1 < n; k++)
        {
            const int p = path[k - 1], c = path[k], q = path[k + 1];
            const int cx = (rfX[size_t(q)] + rfX[size_t(c)] + rfX[size_t(p)]) / 3;
            const int cy = (rfY[size_t(q)] + rfY[size_t(c)] + rfY[size_t(p)]) / 3;
            RouteNode& node = r->nodes[k];
            if (LineCost(group, rfX[size_t(p)], rfY[size_t(p)], cx, cy) >= 0 &&
                LineCost(group, cx, cy, rfX[size_t(q)], rfY[size_t(q)]) >= 0)
            {
                node.x = cx;
                node.y = cy;
            }
            else
            {
                node.x = rfX[size_t(c)];
                node.y = rfY[size_t(c)];
            }
            node.link = rfFlags[size_t(c)];
        }
        for (size_t k : { size_t(0), n - 1 })
        {
            r->nodes[k].x = rfX[size_t(path[k])];
            r->nodes[k].y = rfY[size_t(path[k])];
            r->nodes[k].link = rfFlags[size_t(path[k])];
        }
        auto heading = [&](const RouteNode& a, const RouteNode& b) {
            int h = 0;
            float len = 0;
            HeadingAndLength(float(a.x), float(a.y), float(b.x), float(b.y), h, len);
            return h - 0x400;
        };
        if (n >= 2)
        {
            r->nodes[0].heading = heading(r->nodes[0], r->nodes[1]);
            r->nodes[n - 1].heading = heading(r->nodes[n - 2], r->nodes[n - 1]);
            for (size_t k = 1; k + 1 < n; k++) r->nodes[k].heading = heading(r->nodes[k - 1], r->nodes[k + 1]);
        }
        else r->nodes[0].heading = -0x400;
        return r;
    }

    // 0x802E (0x4911B0): the route kept (+0xEC); the offset (+0x13A) rand() % (value + 1); the waypoint the nearest
    // node to the unit (0x43FF00 from its position as whole numbers; the last of equals), set by 0x491790; then
    // 0x808B(0) after the same stagger as 0x80C2's (none when it's already going faster than 0.01).
    void BattleMap::FollowRoute(Unit& u, int value, std::shared_ptr<const Route> route)
    {
        auto& d = u.drive;
        d.route = std::move(route);
        d.offset = int16_t(random::Rand() % (value + 1));
        int best = 100000, index = 0;
        const int px = int(u.x), py = int(u.y);
        const int32_t cell = land.Cell(), half = land.HalfCell();
        for (size_t k = 0; k < d.route->nodes.size(); k++)
        {
            const RouteNode& nd = d.route->nodes[k];
            const int wx = int32_t(int64_t(nd.x) * cell - half) >> 16, wy = int32_t(int64_t(nd.y) * cell - half) >> 16;
            const int dist = Octagonal(px, py, wx, wy);
            if (dist <= best)
            {
                best = dist;
                index = int(k);
            }
        }
        d.waypoint = index;
        SetWaypoint(u, index);
        int delay = 0;
        if (!(double(d.speed) > 0.01) && d.order >= 0)
        {
            const MoveOrder& o = orders[size_t(d.order)];
            const float share = float(double(5 * o.members) * double(d.toPoint - o.nearest) / (double(o.farthest - o.nearest) - -1.0));
            delay = int(double(random::Rand() & 7) + share);
        }
        if (delay < 0) delay = 0;
        Post808B(u, 0, delay);
    }

    // 0x491790: the target (+0xF4 / +0xF8) the node in the world; within the route, a node with a link byte has the
    // arrival radius the unit's radius + 20 and no growth, the rest its radius x arrivalRadius + 20 and the growth
    // the group's average war map speed (+0x1D0) x 0.01 x members x 0.5
    void BattleMap::SetWaypoint(Unit& u, int index)
    {
        auto& d = u.drive;
        const RouteNode& nd = d.route->nodes[size_t(index)];
        const int32_t cell = land.Cell(), half = land.HalfCell();
        d.tx = int32_t(int64_t(nd.x) * cell - half) >> 16;
        d.ty = int32_t(int64_t(nd.y) * cell - half) >> 16;
        if (index >= int(d.route->nodes.size())) return;
        if (nd.link)
        {
            d.grow = 0;
            d.arrival = float(double(d.radius) - -20.0);
            return;
        }
        d.arrival = float(double(float(double(d.radius) * setup.vehicles.arrivalRadius)) - -20.0);
        int members = 0;
        double speeds = 0;
        for (const Unit& o : units)
            if (o.group == u.group)
            {
                members++;
                if (auto t = setup.units->find(o.type); t != setup.units->end()) speeds += int16_t(t->second.warMapSpeed);
            }
        const float average = members ? float(speeds / members) : 0.0f;
        d.grow = float(double(float(average)) * 0.01 * (double(members) * 0.5));
    }
}
