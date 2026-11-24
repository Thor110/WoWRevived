// The battle map's route finder (WRouteFinder 0x493DF0-0x494960 on the A* 0x44EE70, with the BatRoute cost
// 0x499420-0x4998D0) and a vehicle following a route (0x802E: 0x4911B0, 0x491790). See docs/BATTLEMAP.md "Routes".
#include "battlemap/BattleMap.h"

#include <SDL3/SDL.h>
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
        rfExtra.assign(size_t(count), {});
        bridgeLinks.clear();
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

    // A node's neighbours as the A* walks them (0x44EFA0): its own list, then (after a -2) the chain of nodes added
    // round it since, newest first
    const std::vector<int>& BattleMap::RouteNeighbours(int node) const
    {
        static std::vector<int> all;
        all = rfLinks[size_t(node)];
        all.insert(all.end(), rfExtra[size_t(node)].begin(), rfExtra[size_t(node)].end());
        return all;
    }

    // 0x494220 (vertex x, y, link byte): a new node (up to 0x400 more than the grid), its own list the four grid
    // nodes round it (the cell's ((v - 4) / 4 kept to 1 .. count - 2) top left, right, below, below right), and it
    // added to each of theirs: their lists are cut at the first entry of 0 or less (the -1 that ends them, though a
    // list holding node 0 is cut there too) and it goes first in the chain after.
    int BattleMap::InsertNode(int x, int y, int link)
    {
        if (rfCols < 3 || rfRows < 3) return -1;
        const int index = int(rfX.size());
        if (index >= rfCols * rfRows + 0x400) return -1;     // "insertNode: out of extra node space"
        int i = (x - 4) / 4, j = (y - 4) / 4;
        if (i < 1) i = 1;
        if (j < 1) j = 1;
        if (i > rfCols - 2) i = rfCols - 2;
        if (j > rfRows - 2) j = rfRows - 2;
        const int base = j * rfCols + i;
        rfX.push_back(int16_t(x));
        rfY.push_back(int16_t(y));
        rfFlags.push_back(uint8_t(link));
        rfLinks.push_back({ base, base + 1, base + rfCols, base + rfCols + 1 });
        rfExtra.push_back({});
        for (int g : { base, base + 1, base + rfCols, base + rfCols + 1 })
        {
            auto& l = rfLinks[size_t(g)];
            for (size_t k = 0; k < l.size(); k++)
                if (l[k] <= 0) { l.resize(k); break; }
            rfExtra[size_t(g)].insert(rfExtra[size_t(g)].begin(), index);
        }
        return index;
    }

    // vt+0x104 (0x4983D0), as a bridge starts: a node 48 out beyond each end along its heading (0x43FE90), each at
    // the vertex nearest, with the bridge's link number (+0xDC: dword_59D9B8, 1 for the first of the battle, its
    // place in dword_59D9C0), and the two joined (0x486820 -> 0x494400: each in the other's own list after its four)
    void BattleMap::AddBridgeNodes(const Bridge& bridge)
    {
        BridgeLink b;
        b.ax = bridge.EndX(0); b.ay = bridge.EndY(0);
        b.bx = bridge.EndX(1); b.by = bridge.EndY(1);
        b.length = bridge.Length();
        b.segment = float(double(b.length) / bridge.Count());   // +0xF4 (0x497900)
        bridgeLinks.push_back(b);
        const int link = int(bridgeLinks.size());
        const double a = double(bridge.Heading()) * 0.000244140625 * 6.283185307;
        const float ox = float(std::cos(a) * 48.0), oy = float(std::sin(a) * 48.0);
        const int32_t cell = land.Cell(), half = land.HalfCell();
        auto vertex = [&](float v) { return int((int64_t(int32_t(uint32_t(int(v)) << 16)) + half) / cell); };
        const int na = InsertNode(vertex(float(double(b.ax) - ox)), vertex(float(double(b.ay) - oy)), link);
        const int nb = InsertNode(vertex(float(double(b.bx) + ox)), vertex(float(double(b.by) + oy)), link);
        bridgeLinks.back().nodeA = na;
        bridgeLinks.back().nodeB = nb;
        if (na < 0 || nb < 0) return;
        auto& la = rfLinks[size_t(na)];
        la.insert(la.begin() + std::min<ptrdiff_t>(4, ptrdiff_t(la.size())), nb);
        auto& lb = rfLinks[size_t(nb)];
        lb.insert(lb.begin() + std::min<ptrdiff_t>(4, ptrdiff_t(lb.size())), na);
    }

    // 0x494370: the node comes out of the lists of the four grid nodes round it (the first four of its own list),
    // its own entry is emptied, and dword_59E41C is set: a search under way is given up at its group's next poll
    // (0x4833BA: route not found).
    void BattleMap::RemoveNode(int node)
    {
        finderAbort = true;
        if (node < 0 || size_t(node) >= rfLinks.size()) return;
        const std::vector<int> own = rfLinks[size_t(node)];
        for (size_t k = 0; k < own.size() && k < 4; k++)
        {
            const int g = own[k];
            if (g < 0 || size_t(g) >= rfExtra.size()) continue;
            auto& e = rfExtra[size_t(g)];
            e.erase(std::remove(e.begin(), e.end(), node), e.end());
        }
        rfLinks[size_t(node)].clear();
        rfExtra[size_t(node)].clear();
    }

    // A bridge gone: by its death (0x80A0 -> 0x489FA0: its type's destruction effect, +0x98, at its point) or taken
    // out with nothing more (vt+0x78, 0x48D2E0 -> 0x4895A0, a building starting over it). Its segments go with it,
    // its footprint comes off (vt+0xE8), its table entry is cleared (0x4866C0) and it breaks (0x486850).
    // A vehicle still on its link keeps the link and its height: with the table's entry empty neither is touched
    // again (0x493374 / 0x49339F).
    // *Provisional*: a bridge broken but still standing (+0xEC 0 with its table entry: one started with segments
    // lost, 0x4865A0, which only a saved battle can bring) isn't in; that case drops its riders (0x4933D0).
    void BattleMap::BridgeDies(size_t index, bool quietly)
    {
        BridgeLink& b = bridgeLinks[index];
        if (b.gone) return;
        b.gone = true;
        b.dieAt = -1;
        objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const ViewObject& o) {
            return o.wof && std::find(b.segments.begin(), b.segments.end(), o.wof) != b.segments.end(); }), objects.end());
        auto t = setup.units->find(b.type);
        if (t != setup.units->end())
            if (const int c = t->second.category; c == 0 || c == 2 || c == 3) land.ClearOccupied(b.x, b.y, Radius46(b.type), 2);
        RemoveNode(b.nodeA);
        RemoveNode(b.nodeB);
        if (quietly || t == setup.units->end()) return;
        const int z = int(0.0 - double(t->second.size41) * -1.414);
        Explode(t->second.field98, b.x, b.y, z);
    }

    // vt+0x100 (0x4984E0, each update a vehicle's waypoint or +0x18C is this link): within the length (+0xD8) of
    // both ends it's on: the deck there is the straight line between the ends' heights at its distance from the
    // first end, or the ground at that point on the line when that's higher, + 8; over the first and last segment
    // (+0xF4) it ramps from that point's ground up to it. +0x84 is that less the ground under the vehicle (at least
    // 0). Off it, +0x84 is 0 and the answer no.
    bool BattleMap::OnBridge(Unit& u, int link)
    {
        if (link < 1 || link > int(bridgeLinks.size())) { u.drive.lift = 0; return false; }
        const BridgeLink& b = bridgeLinks[size_t(link - 1)];
        auto dist = [&](int ex, int ey) {
            const int dx = int(u.x - float(ex)), dy = int(u.y - float(ey));
            return std::sqrt(double(dx * dx + dy * dy));
        };
        const double toB = dist(b.bx, b.by);
        const float toA = float(dist(b.ax, b.ay));
        const float length = float(b.length);
        if (!(toB <= length) || !(toA <= length)) { u.drive.lift = 0; return false; }
        const float hA = float(land.HeightAt(b.ax, b.ay));
        const int hB = land.HeightAt(b.bx, b.by);
        const double px = double(b.ax) + double(b.bx - b.ax) / length * toA;
        const double py = double(b.ay) + double(b.by - b.ay) / length * toA;
        const double ground = land.HeightAt(int(float(px)), int(float(py)));
        double deck = (double(float(hB)) - hA) * toA / length + hA;
        if (ground > deck) deck = ground;
        float top = float(deck - -8.0);
        if (toA < b.segment) top = float((double(top) - ground) * toA / b.segment + ground);
        else if (double(length) - b.segment < toA) top = float((double(top) - ground) * (double(length) - toA) / b.segment + ground);
        float lift = float(double(top) - land.HeightAt(int(u.x), int(u.y)));
        if (lift < 0.0f) lift = 0.0f;
        u.drive.lift = lift;
        return true;
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
    // group's +0x5A instead (the bridge's two nodes: a group that can't cross has -1 there).
    int BattleMap::StepCost(int group, int goal, int a, int b) const
    {
        const int axv = rfX[size_t(a)], ayv = rfY[size_t(a)];
        const int toGoal = Direction(rfX[size_t(goal)] - axv, rfY[size_t(goal)] - ayv);
        const int toB = Direction(rfX[size_t(b)] - axv, rfY[size_t(b)] - ayv);
        int line = LineCost(group, axv, ayv, rfX[size_t(b)], rfY[size_t(b)]);
        const uint8_t region = rfFlags[size_t(a)];
        if (region && rfFlags[size_t(b)] == region && group >= 0 && size_t(group) < grounds.size())
        {
            const int dx = axv - rfX[size_t(b)], dy = ayv - rfY[size_t(b)];
            line = int(std::sqrt(double(dx * dx + dy * dy)) * grounds[size_t(group)].bridgeFactor);
        }
        return ((kTurn[toGoal + toB * 8] * 5 >> 4) + 1) * line;
    }

    // 0x494440 (start and goal in vertices, the radius in vertices, 4 for a move; the limit 200; the group): each
    // end to its node (((v - 4) / 4), kept to 1 .. count - 1). Within the radius of the start (radius / the float
    // distance over 1) the goal becomes the start: no route. Else the goal is pulled back the radius along the
    // line from the start and, when that line can be crossed (0x4994B0 not -1), that's where it goes: no route.
    // Otherwise an A* over the nodes (0x44EE70: StepCost, the goal reached within the radius by 0x43FF00 (0x499800))
    // is started (true); the group polls it (0x483B70 -> 0x483270, PollRoute), 200 neighbours tried each update.
    bool BattleMap::BeginRoute(int group, int sx, int sy, int& dx, int& dy, int radius)
    {
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
        finder.run.Begin(start, goal,
            [this, group, goal](int a, int b) { return StepCost(group, goal, a, b); },
            [this](int id) { return std::vector<int>(RouteNeighbours(id)); },
            [this, radius](int a, int g) {
                return Octagonal(rfX[size_t(a)], rfY[size_t(a)], rfX[size_t(g)], rfY[size_t(g)]) <= radius;
            });
        finder.group = group;
        finderAbort = false;        // 0x494558
        return true;
    }

    // 0x483270, from the group's update while the route finder is its. It reads the search's state first (A* +4,
    // 0x483278-0x48329F): still running (0), a node taken out since the search began (dword_59E41C) ends it (route
    // not found, 0x4833BA -> 0x4833F0, the flag cleared), else 200 more neighbours (0x44EFA0), whose outcome isn't
    // looked at until the next update's call; found (1), "ROUTE FOUND": the route built (0x494690), the flag and the
    // acknowledgement for a player's order, and each member 0x802E(ftol(sqrt(the group's count now, 0x4216D0) + 1)
    // x its radius, the route); none (2), 0x4833F0. Either way the finder is free again.
    // (Update 244: until then the engine took a search's outcome in the step's own call, an update early, and a
    // node taken out ended a search that had already finished.)
    void BattleMap::PollRoute(int group)
    {
        if (finder.group != group) return;
        int state = finder.run.State();
        if (state == 0)
        {
            if (finderAbort) { finderAbort = false; state = 2; }
            else { finder.run.Step(200); return; }
        }
        const RouteFinder f = finder;
        finder.group = -1;
        std::vector<size_t> members;
        for (size_t i = 0; i < units.size(); i++)
            if (units[i].group == group && units[i].drive.order == f.order) members.push_back(i);
        if (state != 1 || f.run.Path().empty())
        {
            SDL_Log("ROUTE NOT FOUND");
            RouteNotFound(group, members);
            return;
        }
        const std::vector<int>& path = f.run.Path();
        SDL_Log("ROUTE FOUND: %zu nodes, from vertex %d,%d to %d,%d", path.size(), f.sx, f.sy, f.gx, f.gy);
        const auto route = BuildRoute(group, path);
        const int32_t cell = land.Cell(), half = land.HalfCell();
        // the flag at the route's last node and the acknowledgement (0x483590)
        const RouteNode& last = route->nodes.back();
        if (PlayersOrder(group))
        {
            MoveFlag(int32_t(int64_t(last.x) * cell - half) >> 16, int32_t(int64_t(last.y) * cell - half) >> 16);
            if (!f.attacking) Speak(0x10A, group);
            groupOrdered[size_t(group)] = false;
        }
        const int scale = int(std::sqrt(double(std::max(1, GroupCount(group)))) - -1.0);
        for (size_t i : members) FollowRoute(units[i], scale * units[i].drive.radius, route);
    }

    // 0x4833F0, route not found: the members 0x8031, the target dropped (0x80B9, 0x805A, +0x2C..+0x3C cleared), bit 0
    // cleared; the speech and the portrait only for a player's order to a player's group, then bit 3 cleared
    void BattleMap::RouteNotFound(int group, const std::vector<size_t>& members)
    {
        for (size_t i : members) Stop8031(units[i]);
        const bool spoken = PlayersOrder(group);
        StopAttacking(group);
        if (group >= 0 && size_t(group) < groupBusy.size()) groupBusy[size_t(group)] = false;
        if (spoken) Speak(0x10B, group);
        if (group >= 0 && size_t(group) < groupOrdered.size()) groupOrdered[size_t(group)] = false;
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
