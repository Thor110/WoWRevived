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
#include <functional>
#include <vector>

namespace wow
{
    // One search, kept between calls: 0x44EE70 sets it up (Begin) and 0x44EFA0 runs it (Step) until the goal is
    // taken off the open list (1), the open list is empty (2), or the neighbours tried in this call, less one for
    // each node taken off the list, reach the limit (0: still running; call again).
    class AStarRun
    {
    public:
        using Cost = std::function<int(int, int)>;               // the step's cost, less than 0 if it can't be taken
        using Next = std::function<std::vector<int>(int)>;       // the node's neighbours in order
        using IsGoal = std::function<bool(int, int)>;

        void Begin(int from, int to, Cost c, Next n, IsGoal g)
        {
            cost = std::move(c); next = std::move(n); isGoal = std::move(g);
            goal = to;
            nodes.clear(); open.clear(); closed.clear(); path.clear();
            const int h = cost(to, from);
            nodes.push_back({ h, 0, h, from, -1 });
            open.push_back(0);
            state = 0;
        }

        int Step(int limit)
        {
            if (state != 0) return state;
            int tried = 0;
            while (!open.empty())
            {
                int current = open.front();
                open.erase(open.begin());
                closed.insert(closed.begin(), current);
                if (isGoal(nodes[size_t(current)].id, goal))
                {
                    for (int n = current; n >= 0; n = nodes[size_t(n)].parent) path.insert(path.begin(), nodes[size_t(n)].id);
                    return state = 1;
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
                    int h = cost(id, goal);
                    nodes.push_back({ g + h, g, h, id, current });
                    int added = int(nodes.size()) - 1;
                    auto at = open.begin();
                    while (at != open.end() && nodes[size_t(*at)].f < g + h) ++at;
                    open.insert(at, added);
                }
                tried += count - 1;
                if (tried >= limit) return 0;
            }
            return state = 2;
        }

        int State() const { return state; }
        const std::vector<int>& Path() const { return path; }   // from the start to the goal node reached, both included

    private:
        struct Node { int f, g, h, id, parent; };
        std::vector<Node> nodes;
        std::vector<int> open, closed; // indices into nodes; open in f order, closed newest first
        std::vector<int> path;
        Cost cost; Next next; IsGoal isGoal;
        int goal = 0, state = 2;
    };

    // The search run once (the war map's way, with 1000): the path from `from` to the goal node it reached, both
    // included, or nothing if it didn't finish within the limit or there's no way.
    template <class Cost, class Next, class IsGoal>
    std::vector<int> AStarSearch(int from, int to, Cost&& cost, Next&& next, IsGoal&& isGoal, int limit)
    {
        AStarRun run;
        run.Begin(from, to, cost, [&](int id) { return std::vector<int>(next(id)); }, isGoal);
        return run.Step(limit) == 1 ? run.Path() : std::vector<int>{};
    }
}
