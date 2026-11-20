#include "core/Bridge.h"
#include <cmath>

namespace wow
{
    void HeadingAndLength(float x0, float y0, float x1, float y1, int& heading, float& length)
    {
        const float dx = x1 - x0, dy = y1 - y0;
        const float sq = float(double(dx) * dx + double(dy) * dy);
        if (!(sq > 0.0f))
        {
            heading = 0;
            length = sq;
            return;
        }
        length = float(std::sqrt(double(sq)));
        // asin of dy over the length, as 4096 a turn; mirrored for a leftward run, then kept to -2048..2047
        double h = std::asin(double(dy) / double(length)) * 0.1591549430964443 * 4096.0;
        if (dx < 0.0f) h = 2048.0 - h;
        while (h < -2048.0) h -= -4096.0;
        while (h > 2047.0) h -= 4096.0;
        heading = int(h);
    }

    Bridge Bridge::Create(const Landscape& land, int ax, int ay, int bx, int by)
    {
        Bridge b;
        // 0x48D0A0: each end kept on the map (x under the world's width, y under the rows' extent)
        const int w = land.WorldWidth(), h = int((int64_t(land.Cell()) * land.Height()) >> 16);
        auto keep = [&](int& x, int& y) {
            if (x >= w) x = w - 1;
            if (y >= h) y = h - 1;
            if (x < 0) x = 0;
            if (y < 0) y = 0;
        };
        keep(ax, ay);
        keep(bx, by);
        // 0x486490: each end to the vertex nearest it
        const int32_t cell = land.Cell(), half = cell / 2;
        auto snap = [&](int v) {
            int32_t n = int32_t((int64_t(int32_t(uint32_t(v) << 16)) + half) / cell);
            return int(int32_t(int64_t(n) * cell) >> 16);
        };
        b.ax = snap(ax); b.ay = snap(ay);
        b.bx = snap(bx); b.by = snap(by);
        float len = 0;
        HeadingAndLength(float(b.ax), float(b.ay), float(b.bx), float(b.by), b.heading, len);
        b.length = int(len);
        // 0x497900: a segment for each 64 of the length, and three more
        b.count = int(double(b.length) * 0.015625 - -3.0);
        if (b.count < 1) b.count = 1;
        return b;
    }

    void Bridge::SegmentAt(int i, float px, float py, int& x, int& y) const
    {
        const float n = float(count);
        const float sx = float(double(bx - ax) / n), sy = float(double(by - ay) / n);
        float rx = float(double(ax) - double(sx) * -0.5 - double(px));
        float ry = float(double(ay) - double(sy) * -0.5 - double(py));
        for (int k = 0; k < i; k++)
        {
            rx = float(double(rx) + sx);
            ry = float(double(ry) + sy);
        }
        x = int(px + rx);
        y = int(py + ry);
    }

    float Bridge::JointDistance(int i) const
    {
        const float step = float(double(length) / count);
        float d = 0;
        for (int k = 0; k < i; k++) d = float(double(step) + d);
        return d;
    }

    BridgeJoint Bridge::Joint(const Landscape& land, float dist, int index) const
    {
        const double hA = double(float(land.HeightAt(ax, ay)));
        const double hB = double(float(land.HeightAt(bx, by)));
        const double len = double(length);
        const double ux = double(bx - ax) / len, uy = double(by - ay) / len;
        // the point on the line, kept as floats, and 40 across it either side
        const float px = float(double(ax) + ux * dist);
        const double pyWide = double(ay) + uy * dist;
        const float py = float(pyWide);
        const float acrossX = float(double(float(ux)) * 40.0), acrossY = float(double(float(uy)) * 40.0);
        const int ground = land.HeightAt(int(px), int(pyWide));
        // the deck: the straight line between the ends' heights, or the ground where that's higher
        double deck = (hB - hA) * dist / len + hA;
        if (double(ground) > deck) deck = double(ground);
        int lower = int(deck - -8.0);
        int upper = int(float(deck - -8.0 - -8.0));
        if (index == 0 || index == count)
        {
            lower = ground;
            upper = int(float(double(ground) - -4.0));
        }
        BridgeJoint j;
        const int x0 = int(double(acrossY) + px), y0 = int(double(py) - acrossX);
        const int x1 = int(double(px) - acrossY), y1 = int(double(acrossX) + py);
        const int heights[3] = { ground, lower, upper };
        for (int k = 0; k < 3; k++)
        {
            j.world[k][0] = x0; j.world[k][1] = y0; j.world[k][2] = heights[k];
            j.world[3 + k][0] = x1; j.world[3 + k][1] = y1; j.world[3 + k][2] = heights[k];
        }
        // 0x498300 through 0x49E630: x, the iso projection, y as depth (kept to words)
        for (int k = 0; k < 6; k++)
        {
            j.screen[k][0] = int16_t(j.world[k][0]);
            j.screen[k][1] = int16_t(land.ProjectY(j.world[k][1], j.world[k][2]));
            j.screen[k][2] = int16_t(j.world[k][1]);
        }
        return j;
    }
}
