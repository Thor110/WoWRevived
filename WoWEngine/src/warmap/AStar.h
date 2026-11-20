// The game's A* (0x44EE70 sets it up, 0x44EFA0 runs it, 0x44F120 tries each neighbour), used for the units' routes
// (0x424160, cost 0x42ABD0) and the sectors' cached paths (WSectorPath 0x4A39E0, cost 0x4A3D10). Copied as it is,
// since what it finds depends on its quirks:
//   - there's no separate estimate: a node's h is the cost function asked for (node, goal), and the start's for
//     (goal, start), so it's -1 (impassable) unless the two are next to each other
//   - the open list is kept in order of f = g + h; a new node goes in before the first with an f no lower than its
//     own (the newest first among equals)
//   - a node already open or closed that's reached more cheaply takes the new parent and g (f = h + g) but isn't
//     moved in the open list, and a closed one isn't opened again
//   - the neighbours are tried in the node table's order (0x4BD260, warmap::Neighbours)
//   - it gives up once the neighbours tried, less one for each node taken off the list, reach the limit (1000)
#pragma once
#include "warmap/WarMapData.h"

#include <vector>

namespace wow::warmap
{
    // cost(a, b): what the step from a to b costs, or less than 0 if it can't be taken. Returns the path from
    // `from` to `to`, both included, or nothing.
    template <class Cost>
    std::vector<int> AStar(int from, int to, Cost&& cost, int limit = 1000)
    {
        struct Node { int f, g, h, id, parent; };
        std::vector<Node> nodes;
        std::vector<int> open, closed; // indices into nodes; open in f order, closed newest first
        {
            int h = cost(to, from);
            nodes.push_back({ h, 0, h, from, -1 });
            open.push_back(0);
        }
        int tried = 0;
        while (!open.empty())
        {
            int current = open.front();
            open.erase(open.begin());
            closed.insert(closed.begin(), current);
            if (nodes[size_t(current)].id == to)
            {
                std::vector<int> path;
                for (int n = current; n >= 0; n = nodes[size_t(n)].parent) path.insert(path.begin(), nodes[size_t(n)].id);
                return path;
            }
            int count = 0;
            for (const int* nb = Neighbours(nodes[size_t(current)].id); *nb >= 0; nb++)
            {
                count++;
                const int id = *nb;
                int c = cost(nodes[size_t(current)].id, id);
                if (c < 0) continue;
                const int g = nodes[size_t(current)].g + c;
                int found = -1;
                for (int n : open) if (nodes[size_t(n)].id == id) { found = n; break; }
                if (found < 0) for (int n : closed) if (nodes[size_t(n)].id == id) { found = n; break; }
                if (found >= 0)
                {
                    Node& o = nodes[size_t(found)];
                    if (g < o.g) { o.parent = current; o.g = g; o.f = o.h + g; }
                    continue;
                }
                int h = cost(id, to);
                nodes.push_back({ g + h, g, h, id, current });
                int added = int(nodes.size()) - 1;
                auto at = open.begin();
                while (at != open.end() && nodes[size_t(*at)].f < g + h) ++at;
                open.insert(at, added);
            }
            tried += count - 1;
            if (tried >= limit) return {};
        }
        return {};
    }
}
