// The game's A* (0x44EE70 sets it up, 0x44EFA0 runs it, 0x44F120 tries each neighbour), shared by the war map
// (warmap::AStar: the units' routes and the sectors' cached paths) and the battle map's route finder (WRouteFinder
// 0x494440). Copied as it is, since what it finds depends on its quirks:
//   - there's no separate estimate: a node's h is the cost function asked for (node, goal), and the start's for
//     (goal, start)
//   - the open list is kept in order of f = g + h; a new node goes in before the first with an f no lower than its
//     own (the newest first among equals)
//   - a node already open or closed that's reached more cheaply takes the new parent and g (f = h + g) but isn't
//     moved in the open list, and a closed one isn't opened again
//   - the neighbours are tried in the graph's order (each node's list, 0x44EFA0 through [+8][node])
//   - the goal test is the graph's (vt+8, asked with the node taken off the list and the goal)
//   - it gives up once the neighbours tried, less one for each node taken off the list, reach the limit (the war
//     map's caller runs it once with 1000; the battle map's runs it 200 at a time until the open list is empty)
#pragma once
#include <vector>

namespace wow
{
    // cost(a, b): the step's cost, less than 0 if it can't be taken; next(id): the node's neighbours in order;
    // isGoal(id, goal). Returns the path from `from` to the goal node it reached, both included, or nothing.
    template <class Cost, class Next, class IsGoal>
    std::vector<int> AStarSearch(int from, int to, Cost&& cost, Next&& next, IsGoal&& isGoal, int limit)
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
            if (isGoal(nodes[size_t(current)].id, to))
            {
                std::vector<int> path;
                for (int n = current; n >= 0; n = nodes[size_t(n)].parent) path.insert(path.begin(), nodes[size_t(n)].id);
                return path;
            }
            int count = 0;
            const std::vector<int> neighbours = next(nodes[size_t(current)].id);
            for (int id : neighbours)
            {
                count++;
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
