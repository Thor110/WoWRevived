#include "core/Landscape.h"
#include "core/Bytes.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace wow
{
    namespace
    {
        // The game's 32-bit integer arithmetic: shifts and products wrap as they would in a register
        int32_t Shl16(int v) { return int32_t(uint32_t(v) << 16); }
        int32_t Mul(int32_t a, int32_t b) { return int32_t(uint32_t(a) * uint32_t(b)); }
        int Ftol(double v) { return int(v); } // __ftol truncates

        // 0x43FF00: |dx| + |dy| - min / 2, the quick distance the occupancy marks use
        int QuickDistance(int ax, int ay, int bx, int by)
        {
            int dx = std::abs(bx - ax), dy = std::abs(by - ay);
            int m = dx > dy ? dy : dx;
            return dy - (m >> 1) + dx;
        }

        // 0x43FD10: a 3 x 3 weighted average of a byte grid (32 the centre, 4 each side, 1 each corner, / 52)
        int Smooth(const uint8_t* g, int x, int y, int w)
        {
            const uint8_t* c = g + y * w + x;
            const uint8_t* u = c - w;
            const uint8_t* d = c + w;
            int s = (c[1] + c[0] * 8 + c[-1] + d[0] + u[0]) * 4 + d[1] + d[-1] + u[1] + u[-1];
            return s / 52;
        }

    }

    double Landscape::SlopeLight(int a, int b, int c, float strength, float ambient, const Sun& sun)
    {
        {
            double x = a, y = float(b), z = c;
            double sum = x * x + y * y + z * z;
            if (sum > 0)
            {
                double l = std::sqrt(sum);
                x /= l;
                z /= l;
            }
            double d = z * sun.up + x * sun.across;
            if (d > 0) return std::sqrt(d) * strength + ambient;
            return ambient;
        }
    }

    Landscape::Sun Landscape::SunFor(int angleDegrees, const float set[4])
    {
        Sun s;
        double a = double(angleDegrees) * 0.01745329251944444;
        double sn = std::sin(a), cs = std::cos(a);
        s.strength = set[0];
        s.ambient = set[1];
        s.third = set[2];
        s.cos16 = float(cs * 65536.0);
        s.sin16 = float(sn * 65536.0);
        s.across = float(double(s.cos16) * 0.000015258789);   // flt_4A98DC is a float, 1/65536 to its precision
        s.up = float(double(s.sin16) * 0.000015258789);
        s.pointLight = float((0.2 - sn * -0.5) * set[2] * 64.0);
        return s;
    }

    float Landscape::ShadeLevel(std::span<const uint8_t> table, float third, float previous)
    {
        int limit = Ftol(double(third) * 64.0);
        float level = previous;
        for (int i = 0; i < 256 && i < int(table.size()); i++)
            if (int(table[size_t(i)]) <= limit) level = float(i * 0.00390625);
        return level;
    }

    Landscape Landscape::Create(const Terrain& t, const Tweaks& tweaks)
    {
        Landscape l;
        l.tweaks = tweaks;
        l.width = t.width;
        l.height = t.height;
        l.count = int32_t(t.header[2]);
        l.worldWidth = int32_t(t.header[4]);
        l.worldHeight = int32_t(t.header[5]);
        l.cell = int32_t(t.header[9]);
        l.halfCell = (l.cell - (l.cell >> 31)) >> 1;
        l.rowStep = int32_t(t.header[10]);
        l.heightScale = int32_t(t.header[11]);
        l.heightOffset = int32_t(t.header[12]);
        l.heights = t.heights;
        if (l.cell <= 0) throw FormatError("CLS: bad cell size");
        l.topY = (Mul(t.heights.empty() ? 0 : t.heights[0], l.heightScale) >> 16) + int32_t(t.header[6]) + l.heightOffset;
        l.vertices.assign(size_t(l.width) * size_t(l.height), Vertex{});
        l.scorch.assign(l.vertices.size(), 0);

        // The ATM bytes go to each cell's top left vertex (0x47BC61); the last row and column keep flags 0
        size_t cellsW = size_t(l.width - 1), cellsH = size_t(l.height - 1);
        if (t.tiles.size() >= cellsW * cellsH)
            for (size_t y = 0; y < cellsH; y++)
                for (size_t x = 0; x < cellsW; x++)
                {
                    Vertex& v = l.vertices[y * size_t(l.width) + x];
                    v.atm = t.tiles[y * cellsW + x];
                    v.flags = 1;
                    v.dynamicLight = 0;
                }

        // Heights (0x47BCD6), for as many vertices as the header says
        int n = std::min<int>(l.count, int(l.vertices.size()));
        for (int i = 0; i < n; i++)
        {
            int32_t h = (Mul(l.heights[size_t(i)], l.heightScale) >> 16) + l.heightOffset;
            l.vertices[size_t(i)].height = int16_t(Mul(h, 92682) >> 16);
        }
        l.CalcNormals(0, 0, l.width, l.height);
        return l;
    }

    int Landscape::HeightAtNearest(int x, int y) const
    {
        int cy = Shl16(y) / cell, cx = Shl16(x) / cell;
        if (cy < 0) cy = 0;
        if (cx < 0) cx = 0;
        if (cx >= width) cx = width - 1;
        if (cy >= height) cy = height - 1;
        return At(cx, cy).height;
    }

    int Landscape::HeightAt(int x, int y) const
    {
        int32_t X = Shl16(x), Y = Shl16(y);
        int cy = Y / cell, cx = X / cell;
        if (cy < 0) cy = 0;
        if (cx < 0) cx = 0;
        if (cx >= width - 1) cx = width - 2;
        if (cy >= height - 1) cy = height - 2;
        int h00 = At(cx, cy).height, h10 = At(cx + 1, cy).height;
        int h01 = At(cx, cy + 1).height, h11 = At(cx + 1, cy + 1).height;
        int fy = ((Y % cell) << 8) / cell;
        int fx = ((X % cell) << 8) / cell;
        int top = (((h11 - h10 - h01 + h00) * fy) >> 8) - h00 + h10;
        return (top * fx + (h01 - h00) * fy + (h00 << 8)) >> 8;
    }

    void Landscape::NormalAt(int x, int y, int out[3]) const
    {
        int32_t X = Shl16(x), Y = Shl16(y);
        int fx = ((X % cell) << 8) / cell;
        int fy = ((Y % cell) << 8) / cell;
        int cx = X / cell, cy = Y / cell;
        if (cx <= 1) cx = 1;
        if (cy <= 1) cy = 1;
        if (cx >= width - 2) cx = width - 3;
        if (cy >= height - 2) cy = height - 3;
        for (int k = 0; k < 3; k++)
        {
            int n00 = At(cx, cy).normal[k], n10 = At(cx + 1, cy).normal[k];
            int n01 = At(cx, cy + 1).normal[k], n11 = At(cx + 1, cy + 1).normal[k];
            int v = ((((n11 - n10 - n01 + n00) * fy) >> 8) - n00 + n10) * fx + (n01 - n00) * fy + (n00 << 8);
            out[k] = (v >> 3) & ~31;
        }
    }

    int Landscape::ScreenYToWorldY(int x, int screenY) const
    {
        int col = (Shl16(x) + halfCell) / cell + 1;
        for (int row = height - 1; row >= 0; row--)
        {
            int32_t p = ((Mul(row, rowStep) - Mul(heights[size_t(row) * size_t(width) + size_t(col)], heightScale)) >> 16) - heightOffset + topY;
            if (p <= screenY) return (Mul(row, cell) - halfCell) >> 16;
        }
        return 0;
    }

    bool Landscape::IsAreaPassable(int x0, int y0, int x1, int y1) const
    {
        if (x0 <= 1 || y0 <= 1 || x1 >= width - 1 || y1 >= height - 1) return false;
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++)
            {
                int b = At(x, y).atm, type = b & 15, slope = (b >> 4) + 1;
                if (type == Impassable || slope > tweaks.maxSlope || IsWater(type)) return false;
            }
        return true;
    }

    bool Landscape::CanBuildAt(int x0, int y0, int x1, int y1, bool waterSide) const
    {
        if (x1 == x0) x1 = x0 + 1;
        if (y1 == y0) y1 = y0 + 1;
        int b = tweaks.border;
        if (x0 < b || y0 < b || x1 > width - b || y1 > height - b) return false;
        bool water = false, land = false;
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++)
            {
                const Vertex& v = At(x, y);
                int type = v.atm & 15, slope = (v.atm >> 4) + 1;
                if (type == NoBuild || type == Impassable || slope > tweaks.maxSlope || (v.flags & 6)) return false;
                if (IsWater(type)) water = true;
                else land = true;
            }
        if (waterSide) return water && land;
        return !water;
    }

    bool Landscape::CanBuildLine(int x0, int y0, int x1, int y1, bool overWater) const
    {
        int b = tweaks.border;
        if (x0 < b || y0 < b || x1 > width - b || y1 > height - b) return false;
        double dx = x1 - x0, dy = y1 - y0;
        int n = Ftol(std::sqrt(dx * dx + dy * dy));
        float stepX = float(dx / n), stepY = float(dy / n);
        if (n < 2 || n > 25) return false;
        double fx = x0, fy = y0;
        bool water = false;
        for (int i = 0; i < n; i++)
        {
            const Vertex& v = vertices[size_t(Ftol(fy) * width + Ftol(fx))];
            int type = v.atm & 15, slope = (v.atm >> 4) + 1;
            if (type == Impassable) return false;
            if (IsWater(type))
            {
                if (!overWater) return false;
                water = true;
            }
            if (slope > tweaks.maxSlope || (v.flags & Blocked)) return false;
            fx += stepX;
            fy += stepY;
        }
        return !overWater || water;
    }

    void Landscape::CalcNormals(int x0, int y0, int x1, int y1)
    {
        auto clamp = [](int v, int hi) { if (v >= hi) v = hi - 1; return v < 0 ? 0 : v; };
        x0 = clamp(x0, width);
        y0 = clamp(y0, height);
        x1 = clamp(x1, width);
        y1 = clamp(y1, height);
        int c = cell >> 16;
        double c2 = double(c * c), c4 = c2 * c2;
        for (int y = y0; y < y1 - 1; y++)
            for (int x = x0; x < x1 - 1; x++)
            {
                Vertex& v = At(x, y);
                int h = v.height;
                int nx = -(c * (At(x + 1, y).height - h));
                int ny = -((At(x, y + 1).height - h) * c);
                float fy = float(ny);
                float len = float(std::sqrt(double(nx) * nx + double(fy) * fy + c4) * 0.007874015748031496);
                v.normal[0] = int8_t(Ftol(nx / double(len)));
                v.normal[1] = int8_t(Ftol(fy / double(len)));
                v.normal[2] = int8_t(Ftol(c2 / double(len)));
            }
    }

    void Landscape::MarkOccupied(int x, int y, int radius, uint8_t bits)
    {
        int cx = (Shl16(x) + halfCell) / cell, cy = (Shl16(y) + halfCell) / cell;
        int r = (Shl16(radius) + halfCell) / cell;
        int xa = cx - r, ya = cy - r, xb = r + cx + 1, yb = r + cy + 1;
        if (xa >= width) xa = width - 1;
        if (ya >= height) ya = height - 1;
        if (xa < 0) xa = 0;
        if (ya < 0) ya = 0;
        if (xb >= width) xb = width - 1;
        if (yb >= height) yb = height - 1;
        if (xb < 0) xb = 0;
        if (yb < 0) yb = 0;
        for (int vy = ya; vy < yb; vy++)
            for (int vx = xa; vx < xb; vx++)
                if (QuickDistance(cx, cy, vx, vy) <= r) At(vx, vy).flags |= bits;
    }

    void Landscape::ClearOccupied(int x, int y, int radius, uint8_t bits)
    {
        int cx = (Shl16(x) + halfCell) / cell, cy = (Shl16(y) + halfCell) / cell;
        int r = (Shl16(radius) + halfCell) / cell;
        int xa = cx - r, ya = cy - r, xb = r + cx + 1, yb = r + cy + 1;
        if (xa >= width) xa = width - 1;
        if (ya >= height) ya = height - 1;
        if (xa < 0) xa = 0;
        if (ya < 0) ya = 0;
        if (xb >= width) xb = width - 1;
        if (yb >= height) yb = height - 1;
        if (xb < 0) xb = 0;
        if (yb < 0) yb = 0;
        for (int vy = ya; vy < yb; vy++)
            for (int vx = xa; vx < xb; vx++)
                if (QuickDistance(cx, cy, vx, vy) <= r) At(vx, vy).flags &= uint8_t(~bits);
    }

    void Landscape::Scorch(int x, int y, int radius, int intensity)
    {
        // (The game first works out the screen rectangle to redraw, from the heights at two corners.)
        float k = float(4.0 - intensity * -0.0078125);
        int r = (Shl16(radius) + halfCell) / cell;
        int cx = (Shl16(x) + halfCell) / cell, cy = (Shl16(y) + halfCell) / cell;
        float rf = float(r);
        int xa = Ftol(double(cx) - rf), ya = Ftol(double(cy) - rf);
        int xb = Ftol(double(rf) + cx + 1.0), yb = Ftol(double(rf) + cy + 1.0);
        if (xa >= width) xa = width - 1;
        if (ya >= height) ya = height - 1;
        if (xa < 0) xa = 0;
        if (ya < 0) ya = 0;
        if (xb >= width) xb = width - 1;
        if (yb >= height) yb = height - 1;
        if (xb < 0) xb = 0;
        if (yb < 0) yb = 0;
        if (cx >= width) cx = width - 1;
        if (cy >= height) cy = height - 1;
        if (cx < 0) cx = 0;
        if (cy < 0) cy = 0;
        float fcx = float(cx), fcy = float(cy);
        for (int vy = ya; vy < yb; vy++)
            for (int vx = xa; vx < xb; vx++)
            {
                // 0x43FED0: the distance between two float points
                double dx = double(float(vx)) - fcx, dy = double(float(vy)) - fcy;
                double d = std::sqrt(dx * dx + dy * dy);
                if (d > rf) continue;
                uint8_t& b = scorch[size_t(vy) * size_t(width) + size_t(vx)];
                double v = (rf - d) * k / rf + b;
                if (v > 255.0) v = 255.0;
                b = uint8_t(Ftol(v));
            }
    }

    bool Landscape::ClampWorld(int& x, int& y) const
    {
        bool moved = false;
        if (x >= worldWidth) { x = worldWidth - 1; moved = true; }
        int32_t maxY = Mul(height, cell) >> 16;
        if (y >= maxY) { y = maxY - 1; moved = true; }
        if (x < 0) { x = 0; moved = true; }
        if (y < 0) { y = 0; moved = true; }
        return moved;
    }

    void Landscape::SetPointLight(int x, int y, const Sun& sun)
    {
        int vx = (Shl16(x) + halfCell) / cell, vy = (Shl16(y) + halfCell) / cell;
        if (vx >= width) vx = width - 1;
        if (vy >= height) vy = height - 1;
        if (vx < 0) vx = 0;
        if (vy < 0) vy = 0;
        At(vx, vy).dynamicLight = uint8_t(Ftol(sun.pointLight));
    }

    void Landscape::ClearPointLight(int x, int y)
    {
        int vx = (Shl16(x) + halfCell) / cell, vy = (Shl16(y) + halfCell) / cell;
        if (vx >= width) vx = width - 1;
        if (vy >= height) vy = height - 1;
        if (vx < 0) vx = 0;
        if (vy < 0) vy = 0;
        At(vx, vy).dynamicLight = 0;
    }

    namespace
    {
        // The rectangle both mask routines walk: each corner's vertex plus one, kept on the map
        struct MaskArea { int x0, y0, x1, y1; float stepX, stepY, startU; };
        MaskArea MaskRect(int x0, int y0, int x1, int y1, bool flip, int32_t cell, int w, int h)
        {
            MaskArea a;
            a.x0 = Shl16(x0) / cell + 1;
            a.y0 = Shl16(y0) / cell + 1;
            a.x1 = Shl16(x1) / cell + 1;
            a.y1 = Shl16(y1) / cell + 1;
            if (a.x0 >= w) a.x0 = w - 1;
            if (a.y0 >= h) a.y0 = h - 1;
            if (a.x0 < 0) a.x0 = 0;
            if (a.y0 < 0) a.y0 = 0;
            if (a.x1 >= w) a.x1 = w - 1;
            if (a.y1 >= h) a.y1 = h - 1;
            if (a.x1 < 0) a.x1 = 0;
            if (a.y1 < 0) a.y1 = 0;
            double sx = 32.0 / (a.x1 - a.x0 + 1);
            a.stepY = float(32.0 / (a.y1 - a.y0 + 1));
            a.startU = 0;
            if (flip) { a.startU = 31.0f; sx = -sx; }
            a.stepX = float(sx);
            return a;
        }
    }

    void Landscape::ProjectLight(const uint8_t* mask, int x0, int y0, int x1, int y1, bool flip, const Sun& sun)
    {
        MaskArea a = MaskRect(x0, y0, x1, y1, flip, cell, width, height);
        double v = 0;
        for (int y = a.y0; y < a.y1; y++, v += a.stepY)
        {
            int row = Ftol(v) * 32;
            double u = a.startU;
            for (int x = a.x0; x < a.x1; x++, u += a.stepX)
            {
                int m = mask[Ftol(u) + row] * 4;
                if (m < 255) At(x, y).dynamicLight = uint8_t(Ftol((255.0 - m) * sun.pointLight * 0.00392156862745098));
            }
        }
    }

    void Landscape::ClearLight(const uint8_t* mask, int x0, int y0, int x1, int y1, bool flip)
    {
        MaskArea a = MaskRect(x0, y0, x1, y1, flip, cell, width, height);
        double v = 0;
        for (int y = a.y0; y < a.y1; y++, v += a.stepY)
        {
            int row = Ftol(v) * 32;
            double u = a.startU;
            for (int x = a.x0; x < a.x1; x++, u += a.stepX)
                if (mask[Ftol(u) + row] * 4 < 255) At(x, y).dynamicLight = 0;
        }
    }

    int Landscape::SmoothedHeight(int x, int y) const { return Smooth(heights.data(), x, y, width); }

    void Landscape::LightTheMap(const Sun& sun)
    {
        int level = Ftol(double(sun.shade) * 256.0);
        float slope = float(double(sun.sin16) / sun.cos16);
        bool fromRight = sun.cos16 <= 0.0f;
        int c = cell >> 16;
        std::vector<uint8_t> lit(size_t(count > 0 ? count : 0) + size_t(width) * size_t(height), 0); // "ISOTemp", zeroed
        auto isoAt = [&](int x, int y) -> uint8_t& { return lit[size_t(y) * size_t(width) + size_t(x)]; };
        uint8_t shadowLevel = uint8_t(Ftol(double(level) * sun.ambient));

        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 0; x < width; x++)
            {
                // The vertex past the row's end is the next row's first, as in the game
                const Vertex* v = &vertices[size_t(y) * size_t(width) + size_t(x)];
                int h = v[0].height;
                double l = SlopeLight(h - v[1].height, h - v[width].height, c, sun.strength, sun.ambient, sun) * double(float(level));
                isoAt(x, y) = uint8_t(isoAt(x, y) + uint8_t(Ftol(l)));
            }
            for (int x = 1; x < width - 1; x++)
            {
                int caster = SmoothedHeight(x, y);
                if (fromRight)
                {
                    int end = std::min(x + 1 + 15, width - 1);
                    for (int sx = x + 1, d = -1; sx < end; sx++, d--)
                    {
                        int32_t num = Mul(caster - heights[size_t(y) * size_t(width) + size_t(sx)], heightScale);
                        int32_t den = Mul(d, cell);
                        if (double(num) / den <= slope) isoAt(sx, y) = shadowLevel;
                    }
                }
                else
                {
                    int start = std::max(x - 16, 0);
                    for (int sx = start, d = x - start; sx < x; sx++, d--)
                    {
                        int32_t num = Mul(caster - heights[size_t(y) * size_t(width) + size_t(sx)], heightScale);
                        int32_t den = Mul(d, cell);
                        if (!(double(num) / den < slope)) isoAt(sx, y) = shadowLevel;
                    }
                }
            }
        }
        for (int y = 2; y < height - 2; y++)
            for (int x = 2; x < width - 2; x++)
                At(x, y).light = uint8_t(Smooth(lit.data(), x, y, width));
        for (int y = 0; y < height; y++)
        {
            At(0, y).light = 0;
            At(width - 1, y).light = 0;
        }
        for (int x = 0; x < width; x++)
        {
            At(x, 0).light = 0;
            At(x, height - 1).light = 0;
        }
    }
}
