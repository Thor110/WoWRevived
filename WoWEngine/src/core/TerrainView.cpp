#include "core/TerrainView.h"
#include "core/Bytes.h"
#include "core/Random.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace wow
{
    namespace
    {
        int32_t Mul(int32_t a, int32_t b) { return int32_t(uint32_t(a) * uint32_t(b)); }
        int32_t Shl16(int v) { return int32_t(uint32_t(v) << 16); }
    }

    TerrainTiles TerrainTiles::Load(const Terrain& t)
    {
        TerrainTiles tiles;
        size_t heights = t.heights.size();
        auto rest = [&](uint32_t off) -> size_t
        {
            if (off < heights || off - heights > t.rest.size()) throw FormatError("CLS: tile table outside the file");
            return off - heights;
        };
        tiles.heightMask = t.header[13];
        size_t mapAt = rest(t.header[14]), offsAt = rest(t.header[15]), patchAt = rest(t.header[16]);
        if (offsAt < mapAt || patchAt < offsAt) throw FormatError("CLS: tile tables out of order");
        size_t halfCells = size_t(t.width * 2 - 2) * size_t(t.height - 1);
        if (mapAt + halfCells * 2 > offsAt) throw FormatError("CLS: tile map cut short");
        tiles.map.resize(halfCells);
        for (size_t i = 0; i < halfCells; i++) tiles.map[i] = uint16_t(t.rest[mapAt + i * 2] | (t.rest[mapAt + i * 2 + 1] << 8));
        size_t count = (patchAt - offsAt) / 4;
        tiles.offsets.resize(count);
        for (size_t i = 0; i < count; i++)
        {
            const uint8_t* p = &t.rest[offsAt + i * 4];
            tiles.offsets[i] = uint32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24));
        }
        tiles.patches.assign(t.rest.begin() + std::ptrdiff_t(patchAt), t.rest.end());
        return tiles;
    }

    DitherTables DitherTables::Build(std::span<const uint8_t> dithDat)
    {
        struct Desc { int set, high, low, cut; };
        std::array<Desc, 16> d{};
        ByteReader r(dithDat);
        for (auto& e : d)
        {
            if (r.Remaining() < 20) break;
            e.set = int32_t(r.U32());
            e.high = int32_t(r.U32());
            e.low = int32_t(r.U32());
            e.cut = int32_t(r.U32());
            r.U32();
        }
        // 0x411011: the first four descriptors' ranges are set in the code
        d[0].high = 8; d[0].low = 0; d[0].cut = 0x800;
        d[1].high = 8; d[1].low = 0; d[1].cut = 0x800;
        d[2].high = 5; d[2].low = 0; d[2].cut = 0x1000;
        d[3].high = 2; d[3].low = 0; d[3].cut = 0x2800;

        DitherTables t;
        t.values.assign(0x1000, 0);
        for (const Desc& e : d)
        {
            int range = e.high - e.low;
            int k = 0;
            for (int limit = 0x6800; limit <= 0x8000; limit += 0x800, k++)
                for (int i = 0; i < 64; i++)
                {
                    size_t at = size_t(e.set) * 256 + size_t(k) * 64 + size_t(i);
                    if (at >= t.values.size()) continue;
                    if (random::Random(0x7FFF) < limit - e.cut)
                    {
                        int v = random::Random(0x7FFF);
                        int q = range ? 0x8000 / range : 0;
                        int lv = (q ? v / q : 0) + e.low;
                        t.values[at] = -std::abs(lv); // stored as -|levels x 0x200|
                    }
                    else t.values[at] = 0;
                }
        }
        return t;
    }

    int DitherTables::LevelAt(int byteOffset) const
    {
        size_t i = size_t(byteOffset) / 4;
        return i < values.size() ? values[i] : 0;
    }

    namespace
    {
        // The span routine's state (dword_4CA054-0x4CA0B8)
        struct Spans
        {
            const TerrainView::Target* t;
            const uint8_t* data;        // 4CA0D0
            int x;                      // 4CA094
            int y;                      // 4CA070
            int line;                   // the row being drawn
            int left, right;            // 4CA084, 4CA08C: the light at each end, levels x 256
            int dl, dr;                 // 4CA088, 4CA090
            uint8_t phase, nextPhase;   // 4CA0B4, 4CA0B8
            int ditherBase;             // 4CA0D8 less the table's start
            const DitherTables* dither;
            const ShadeTable* table;
            bool transparent;           // kind 1 (0x4574CD): texel 0 isn't drawn

            // 0x45702D (kind 0) and 0x4574CD (kind 1)
            void Rows(int rows)
            {
                for (; rows > 0; rows--)
                {
                    y++;
                    if (y > t->bottom) return;
                    int x0 = data[0] + x;
                    int raw = data[1];
                    bool run = (raw & 0x80) != 0;
                    int count = raw & 0x7F;
                    const uint8_t* texels = data + 2;
                    data += 2 + (run ? 1 : count);
                    nextPhase = uint8_t(nextPhase + count * 4);
                    if (y > t->top && count > 0)
                    {
                        int n = count;
                        int over = t->right - x0 - count;
                        if (over <= 0) n = count + over;
                        if (n > 0)
                        {
                            int diff = right - left;
                            int step = diff ? diff / count : 0;
                            int e = left;
                            bool draw = true;
                            if (x0 < t->left)
                            {
                                int skip = t->left - x0;
                                phase = uint8_t(phase + skip * 4);
                                n -= skip;
                                if (n <= 0) draw = false;
                                else
                                {
                                    x0 += skip;
                                    if (!run) texels += skip;
                                    e = left + Mul(skip, step);
                                }
                            }
                            if (draw)
                            {
                                uint32_t* out = t->pixels + size_t(line) * size_t(t->pitch) + size_t(x0);
                                int d = ditherBase ^ phase;
                                for (int i = 0; i < n; i++)
                                {
                                    int texel = run ? texels[0] : texels[i];
                                    if (!transparent || texel)
                                    {
                                        int level = ((e >> 8) & 0xFF) + dither->LevelAt(d + i * 4);
                                        out[i] = table->Level(level)[texel];
                                    }
                                    e += step;
                                }
                            }
                        }
                    }
                    left += dl;
                    right += dr;
                    line++;
                    phase = nextPhase;
                }
            }
        };
    }

    // 0x4579A4
    void TerrainView::DrawPatch(const uint8_t* p, const uint8_t* rows, int phaseSeed, int x, int y, const int lights[3], int ditherBase,
        bool transparent, const Target& t, const DitherTables& dither, const ShadeTable& table)
    {
        {
            int w = p[0] & 0x7F, h = p[1], split = p[2];
            bool flag = (p[0] & 0x80) != 0;
            if (x >= t.right || x + w < t.left || y >= t.bottom || y + h < t.top) return;
            Spans s{};
            s.t = &t;
            s.data = rows;
            s.transparent = transparent;
            s.x = x;
            s.y = y;
            s.line = y;
            // The game seeds the dither phase from the patch's address, its height and the patch counter (4CA0E0);
            // the caller passes an offset standing in for the address
            s.phase = s.nextPhase = uint8_t((phaseSeed + h) & 0xFC);
            s.ditherBase = ditherBase + (h <= 8 ? 0x100 : 0) + (h <= 12 ? 0x100 : 0) + (h <= 16 ? 0x100 : 0);
            s.dither = &dither;
            s.table = &table;
            int a = lights[0] << 8, b = lights[1] << 8, c = lights[2] << 8;
            if (h <= 0) return;
            if (split == 0)
            {
                s.left = a;
                s.right = c;
                s.dl = (b - s.left) / h;
                s.dr = (b - s.right) / h;
                s.Rows(h);
            }
            else if (flag)
            {
                s.left = s.right = a;
                s.dl = (b - a) / h;
                s.dr = (c - a) / split;
                s.Rows(split);
                if (h - split == 0) return;
                s.dr = (b - s.right) / (h - split);
                s.Rows(h - split);
            }
            else
            {
                s.left = s.right = a;
                s.dl = (b - a) / split;
                s.dr = (c - a) / h;
                s.Rows(split);
                if (h - split == 0) return;
                s.dl = (c - s.left) / (h - split);
                s.Rows(h - split);
            }
        }
    }

    void TerrainView::Render(Landscape& land, const TerrainTiles& tiles, const ShadeTable& table, std::span<const uint8_t> levels,
        const DitherTables& dither, const Target& t, int originX, int originY, const Fog* fog,
        const std::function<void(int)>& between, Window* window)
    {
        const int w = land.Width(), h = land.Height();
        const int32_t cell = land.Cell(), rowStep = land.RowStep(), hs = land.HeightScale(), hOff = land.HeightOffset();
        const int baseY = originY + land.TopY();                 // +0x114
        const auto& heights = land.Heights();
        auto& verts = land.Vertices();
        const auto& scorch = land.Scorched();

        // 0x49FDA0: the columns and rows in view
        int first = Shl16(t.left - originX) / cell;
        int last = Shl16(t.right - originX) / cell + 2;
        int firstRow = Shl16(t.top - baseY) / rowStep - 1;
        if (first < 0) first = 0;
        if (firstRow < 0) firstRow = 0;
        if (last >= w) last = w - 1;
        if (firstRow >= h - 1) firstRow = h - 2;
        // the game's window ends at the last row on screen; every row's shown level is worked out here, so it runs
        // to the map's end (the rows below the screen aren't drawn)
        if (window) *window = { first, firstRow, last - first + 1, h - firstRow };

        // Each vertex's shown light: lit level less scorch and moving lights, dimmed by the fog (0x4A0003). The
        // game does the rows on screen; every row is done here, which only differs for rows below the screen.
        for (int y = 0; y < h; y++)
            for (int x = first; x <= last; x++)
            {
                size_t i = size_t(y) * size_t(w) + size_t(x);
                double f = 1.0;
                if (fog && fog->data)
                {
                    int b = fog->data[size_t(y) * size_t(fog->width) + size_t(x)];
                    if (b < fog->threshold)
                    {
                        double q = double(b) / fog->threshold;
                        f = q + (1.0 - q) * fog->keep;
                    }
                }
                int v = int((int(verts[i].light) - int(scorch[i]) - int(verts[i].dynamicLight)) * f);
                verts[i].shown = uint8_t(std::clamp(v, 0, 255));
            }

        auto level = [&](size_t v)
        {
            int l = levels.size() == 256 ? levels[verts[v].shown] : verts[v].shown >> 2;
            return std::clamp(l, 8, 63);
        };
        auto proj = [&](int row, size_t v) { return ((Mul(row, rowStep) - Mul(int32_t(heights[v] & tiles.heightMask), hs)) >> 16) - hOff; };

        // 0x49F610 / 0x49F890: row by row, each row's half cells left to right
        int below = 0;
        for (int row = firstRow;;)
        {
            int minY = 100000;
            for (int tri = first * 2; tri < last * 2; tri++)
            {
                int col = tri >> 1;
                size_t v = size_t(row) * size_t(w) + size_t(col);
                int corner[4] = { level(v), level(v + 1), level(v + size_t(w)), level(v + size_t(w) + 1) };
                int top = proj(row, v + size_t(w)) + baseY;
                if (top < minY) minY = top;
                if (!(verts[v].flags & 1)) continue;
                size_t tileIndex = size_t(w * 2 - 2) * size_t(row) + size_t(tri);
                if (tileIndex >= tiles.map.size()) continue;
                uint16_t tile = tiles.map[tileIndex];
                if (tile == 0xFFFF || tile >= tiles.offsets.size()) continue;
                uint32_t off = tiles.offsets[tile];
                if (off + 4 > tiles.patches.size()) continue;
                const uint8_t* p = &tiles.patches[off];
                // 0x49F770: the patch hangs from the corner its low two bits name
                int y;
                switch (p[3] & 3)
                {
                case 0: y = proj(row, v); break;
                case 1: y = proj(row, v + 1); break;
                case 2: y = proj(row + 1, v + size_t(w)) - int8_t(p[1]); break;
                default: y = proj(row + 1, v + size_t(w) + 1) - int8_t(p[1]); break;
                }
                y += baseY;
                int x = (Mul(cell, col) >> 16) + originX;
                if (y < minY) minY = y - 16;
                int lights[3] = { corner[(p[3] >> 2) & 3], corner[(p[3] >> 4) & 3], corner[(p[3] >> 6) & 3] };
                int atm = verts[v].atm;
                int ditherBase = atm <= 3 ? atm << 10 : 0x2000;
                DrawPatch(p, p + 4, int(off) + int(tileIndex), x, y, lights, ditherBase, false, t, dither, table);
            }
            if (minY > t.bottom) below++;
            if (between) between(Mul(row + 1, cell) >> 16);
            row++;
            if (below >= 5 || row >= h - 1) break;
        }
        if (between) between(INT32_MAX);
    }

    double TerrainView::PointLight(int dx, int dy, int dz, int hx, int hy, int step, int range, int inner)
    {
        // the distance (kept as a float) and the direction to the light
        const int d2 = dx * dx + dy * dy + dz * dz;
        double dist = double(d2);
        float distF = float(d2);
        if (d2 > 0)
        {
            dist = std::sqrt(double(d2));
            distF = float(dist);
        }
        if (!(dist < double(range + inner))) return 0.0;
        const float nx = float(dx / double(distF)), ny = float(dy / double(distF)), nz = float(dz / double(distF));
        // the ground's normal, (hx, hy, step) over its length
        const float s = float(step);
        const float l2 = float(double(hx) * hx + double(hy) * hy + double(s) * s);
        double ax = hx, ay = hy, az = s;
        if (l2 > 0.0f)
        {
            const double l = std::sqrt(double(l2));
            ax = hx / l;
            ay = hy / l;
            az = s / l;
        }
        const double facing = ax * nx + ay * ny + az * nz;
        if (!(facing > 0.0)) return 0.0;
        double q = double(distF - double(inner)) / double(range);
        if (q < 0.0) q = 0.0;
        return (1.0 - q) * std::sqrt(facing);
    }

    void TerrainView::AddLight(Landscape& land, const Window& win, int x, int y, int z, int range, float intensity)
    {
        const int32_t cell = land.Cell();
        const int w = land.Width(), h = land.Height();
        auto& verts = land.Vertices();
        // the light's point and range are words (+0x38..+0x3E)
        x &= 0xFFFF; y &= 0xFFFF; z &= 0xFFFF; range &= 0xFFFF;
        const int vx = Shl16(x) / cell, vy = Shl16(y) / cell, r = Shl16(range) / cell;
        int x0 = vx - r - win.firstCol, y0 = vy - r - win.firstRow;
        int x1 = vx + r - win.firstCol + 1, y1 = vy + r - win.firstRow + 1;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > win.cols) x1 = win.cols;
        if (y1 > win.rows) y1 = win.rows;
        const float level = float(double(intensity) * 256.0);
        float rowY = float(y - (Mul(win.firstRow + y0, cell) >> 16));
        const float step = float(double(cell) * double(0.000015258789f));
        for (int j = y0; j < y1; j++)
        {
            float colX = float(x - (Mul(win.firstCol + x0, cell) >> 16));
            const int stepInt = int(step), dy = int(rowY);
            for (int i = x0; i < x1; i++)
            {
                const int vxi = win.firstCol + i, vyi = win.firstRow + j;
                if (vxi + 1 < w && vyi + 1 < h)
                {
                    Landscape::Vertex& v = verts[size_t(vyi) * size_t(w) + size_t(vxi)];
                    const int h0 = v.height;
                    const int hRight = verts[size_t(vyi) * size_t(w) + size_t(vxi) + 1].height;
                    const int hDown = verts[size_t(vyi + 1) * size_t(w) + size_t(vxi)].height;
                    const int add = int(PointLight(int(colX), dy, z - h0, hRight - h0, hDown - h0, stepInt, range, 0) * level);
                    v.shown = uint8_t(std::min(255, add + v.shown));
                }
                colX = float(double(colX) - step);
            }
            rowY = float(double(rowY) - step);
        }
    }
}
