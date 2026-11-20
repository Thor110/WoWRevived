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
#include "core/AStar.h"
#include "warmap/WarMapData.h"

#include <vector>

namespace wow::warmap
{
    // cost(a, b): what the step from a to b costs, or less than 0 if it can't be taken. Returns the path from
    // `from` to `to`, both included, or nothing.
    template <class Cost>
    std::vector<int> AStar(int from, int to, Cost&& cost, int limit = 1000)
    {
        std::vector<int> list;
        auto next = [&list](int id) -> const std::vector<int>& {
            list.clear();
            for (const int* nb = Neighbours(id); *nb >= 0; nb++) list.push_back(*nb);
            return list;
        };
        return AStarSearch(from, to, cost, next, [](int id, int goal) { return id == goal; }, limit);
    }
}
