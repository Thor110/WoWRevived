#include "core/WofView.h"
#include "core/Bytes.h"
#include <algorithm>
#include <cmath>

namespace wow
{
    namespace
    {
        int32_t Mul(int32_t a, int32_t b) { return int32_t(uint32_t(a) * uint32_t(b)); }
        int32_t Add(int32_t a, int32_t b) { return int32_t(uint32_t(a) + uint32_t(b)); }
        uint32_t U32At(const std::vector<uint8_t>& d, size_t o)
        {
            if (o + 4 > d.size()) throw FormatError("WOF: offset past the end");
            return uint32_t(d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (uint32_t(d[o + 3]) << 24));
        }
        int16_t S16At(const std::vector<uint8_t>& d, size_t o)
        {
            if (o + 2 > d.size()) throw FormatError("WOF: offset past the end");
            return int16_t(d[o] | (d[o + 1] << 8));
        }

        // 0x4A2FA0: a matrix (column major, 4.12) times a vector
        void Apply(const int32_t m[9], int32_t x, int32_t y, int32_t z, int32_t out[3])
        {
            for (int i = 0; i < 3; i++)
                out[i] = Add(Add(Mul(m[i], x), Mul(m[3 + i], y)), Mul(m[6 + i], z)) >> 12;
        }
        // 0x4A2C50: out = a after b
        void MatMul(const int32_t a[9], const int32_t b[9], int32_t out[9])
        {
            int32_t r[9];
            for (int k = 0; k < 3; k++)
                for (int i = 0; i < 3; i++)
                    r[3 * k + i] = Add(Add(Mul(b[3 * k], a[i]), Mul(b[3 * k + 1], a[3 + i])), Mul(b[3 * k + 2], a[6 + i])) >> 12;
            std::copy(r, r + 9, out);
        }
        // 0x4A2E00: a rotation from three angles (4096 a turn)
        void Rotation(int a0, int a1, int a2, int32_t out[9])
        {
            // *Provisional* to the last bit: the game keeps some of these as floats between steps
            const double k = 0.001533980787841797;
            const double r0 = int16_t(a0) * k, r1 = int16_t(a1) * k, r2 = int16_t(a2) * k;
            const double c0 = std::cos(r0), s0 = std::sin(r0), c1 = std::cos(r1), s1 = std::sin(r1), c2 = std::cos(r2), s2 = std::sin(r2);
            auto f = [](double v) { return int32_t(v * 4096.0); };
            out[0] = f(c2 * c1);
            out[3] = f(-(s2 * c1));
            out[6] = f(s1);
            out[1] = f(s2 * c0 + s1 * s0 * c2);
            out[4] = f(c2 * c0 - s0 * (s2 * s1));
            out[7] = f(-(s0 * c1));
            out[2] = f(s2 * s0 - s1 * c2 * c0);
            out[5] = f(s0 * c2 + c0 * (s2 * s1));
            out[8] = f(c1 * c0);
        }
        // 0x4A3020: the turn from the model's axes (y up) to the world's (z up), tilted to the ground's normal at a
        // world point when asked for; when not, y and z just swap
        void GroundTilt(const Landscape& land, int x, int y, bool follow, int32_t out[9])
        {
            if (!follow)
            {
                const int32_t swap[9] = { 0x1000, 0, 0, 0, 0, 0x1000, 0, 0x1000, 0 };
                std::copy(swap, swap + 9, out);
                return;
            }
            int n[3];
            land.NormalAt(x, y, n);
            int32_t a = n[0], b = n[1], c = n[2] ? n[2] : 0x1000;
            int32_t len = int32_t(std::sqrt(double(Add(Add(Mul(a, a), Mul(b, b)), Mul(c, c)))));
            if (!len) len = 1;
            a = Mul(a, 4096) / len;
            b = Mul(b, 4096) / len;
            c = Mul(c, 4096) / len;
            int32_t e = Mul(-b, 4096) >> 12, f = Mul(c, 4096) >> 12;
            int32_t len2 = int32_t(std::sqrt(double(Add(Mul(e, e), Mul(f, f)))));
            if (!len2) len2 = 1;
            int32_t u0 = 0, u1 = Mul(f, 4096) / len2, u2 = Mul(e, 4096) / len2; // (0, c', -b')
            int32_t s0 = (Mul(u1, c) - Mul(u2, b)) >> 12;
            int32_t s1 = (Mul(u2, a) - Mul(u0, c)) >> 12;
            int32_t s2 = (Mul(u0, b) - Mul(u1, a)) >> 12;
            int32_t len3 = int32_t(std::sqrt(double(Add(Add(Mul(s0, s0), Mul(s1, s1)), Mul(s2, s2)))));
            if (!len3) len3 = 1;
            out[0] = Mul(s0, 4096) / len3;
            out[1] = Mul(s1, 4096) / len3;
            out[2] = Mul(s2, 4096) / len3;
            out[3] = a; out[4] = b; out[5] = c;
            out[6] = u0; out[7] = u1; out[8] = u2;
        }
        // the iso projection (as 0x49EA21), kept to a word
        int16_t ProjectY(int topY, int32_t y, int32_t h)
        {
            int32_t a = int32_t(uint32_t(y) << 16);
            int32_t u = int32_t((int64_t(a) * int32_t(0xB4B10D5D)) >> 32) + a;
            u >>= 16;
            u += int32_t(uint32_t(u) >> 31);
            int32_t b = int32_t(uint32_t(h) << 16);
            int32_t v = int32_t((int64_t(b) * 0x2C3E63FB) >> 32) >> 14;
            v += int32_t(uint32_t(v) >> 31);
            return int16_t(int16_t(topY - v) + u);
        }

        uint8_t vertexLight[256]; // byte_59F768: kept from piece to piece, as the game's is
    }

    WofInstance::WofInstance(std::vector<uint8_t> data, float scale, int follow) : file(std::move(data)), groundFollow(follow)
    {
        if (file.size() < 0x30) throw FormatError("WOF: too short");
        const int count = uint16_t(S16At(file, 0));
        root = int32_t(U32At(file, 0x10));
        vertices = U32At(file, 0x0C);
        materials = U32At(file, 0x24);
        texture = U32At(file, 0x28);
        smooth = U32At(file, 0x2C);
        // 0x49DCA0: the scales start at 1 and are multiplied by the GType's float
        sx = sy = sz = int(1.0f * scale);
        pieces.resize(size_t(count));
        for (int p = 0; p < count; p++)
        {
            size_t r = 0x30 + size_t(p) * 0x61;
            if (r + 0x61 > file.size()) throw FormatError("WOF: piece table runs past the end");
            Piece& piece = pieces[size_t(p)];
            piece.flags = file[r + 0x10];
            piece.vertexCount = file[r + 0x11];
            piece.faceCount = file[r + 0x12];
            for (int a = 0; a < 3; a++) piece.pivot[a] = S16At(file, r + 0x13 + size_t(a) * 2);
            piece.vertexOffset = U32At(file, r + 0x19);
            piece.faceOffset = U32At(file, r + 0x1D);
            for (int c = 0; c < 16; c++)
            {
                int32_t child = int32_t(U32At(file, r + 0x21 + size_t(c) * 4));
                if (child < 0) break;
                piece.children.push_back(child);
            }
        }
        if (root < 0 || root >= count) root = 0;
        // 0x49E330: the animation, checked here so a bad table just leaves the model still
        frames = int(U32At(file, 0x1C));
        keys = U32At(file, 0x20);
        keyScale = float(int32_t(U32At(file, 0x14)));
        if (frames > 0 && frames < 1000 && keyScale != 0.0f && size_t(keys) + 0x18 + size_t(count) * 4 <= file.size())
        {
            for (int p = 0; p < count && frames; p++)
            {
                const size_t start = size_t(keys) + U32At(file, size_t(keys) + 0x18 + size_t(p) * 4);
                if (start + size_t(frames) * (p == root ? 32 : 20) > file.size()) frames = 0;
            }
        }
        else frames = 0;
        world.resize(pieces.size());
        screen.resize(pieces.size());
        nearest.assign(pieces.size(), 0);
        SetVariant(1);
    }

    void WofInstance::SetVariant(int v)
    {
        hidden.assign(pieces.size(), false);
        for (size_t p = 0; p < pieces.size(); p++)
        {
            int f = pieces[p].flags & 3;
            hidden[p] = f != v && f != 0;
        }
    }

    void WofInstance::Transform(const Landscape& land, int heading, int x, int y, int z, bool upright)
    {
        topY = land.TopY();
        for (int k = 0; k < 3; k++) { boundMin[k] = 0x7FFF; boundMax[k] = -0x8000; }
        int32_t tilt[9];
        GroundTilt(land, x, y, !upright && groundFollow != 0, tilt);
        // the camera's angles: -110, 0x400 less the heading, 0x800
        Rotation(int16_t(0xFF92), 0x400 - heading, 0x800, m);
        const Piece& r = pieces[size_t(root)];
        if (frames)
        {
            // 0x49E734: the root's place from its key, kept to words as the game's are
            const size_t k = Key(root);
            const auto place = [&](size_t o, float sign) { return int16_t(int32_t(float(int32_t(U32At(file, k + o))) * sign * 16384.0f / keyScale)); };
            Apply(m, place(0x14, 1.0f), place(0x18, -1.0f), place(0x1C, 1.0f), t);
        }
        else Apply(m, r.pivot[0], -r.pivot[1], r.pivot[2], t);
        MatMul(tilt, m, m);
        // 0x49E83E: the root's pivot taken off y and z alike
        const int32_t d = (Mul(t[1], sy) >> 16) + (Mul(t[2], sz) >> 16);
        Walk(root, x, y - d, z - d);
        const int16_t sy0 = ProjectY(topY, y, z);
        anchorX = x;
        anchorY = sy0;
        boundMin[0] -= x; boundMax[0] -= x;
        boundMin[2] -= y; boundMax[2] -= y;
        boundMin[1] -= sy0; boundMax[1] -= sy0;
    }

    size_t WofInstance::Key(int p) const
    {
        return size_t(keys) + U32At(file, size_t(keys) + 0x18 + size_t(p) * 4) + size_t(frame) * (p == root ? 32 : 20);
    }

    // 0x49E900
    void WofInstance::Walk(int p, int ox, int oy, int oz)
    {
        const Piece& piece = pieces[size_t(p)];
        if (frames)
        {
            // 0x4A2A90: the piece's key turns it, before its points and its children's pivots
            const size_t k = Key(p);
            int32_t key[9];
            for (int i = 0; i < 9; i++) key[i] = S16At(file, k + size_t(i) * 2);
            MatMul(m, key, m);
        }
        auto& w = world[size_t(p)];
        auto& s = screen[size_t(p)];
        w.resize(size_t(piece.vertexCount));
        s.resize(size_t(piece.vertexCount));
        int least = 0x7FFFFFFF;
        for (int i = 0; i < piece.vertexCount; i++)
        {
            size_t o = size_t(vertices) + piece.vertexOffset + size_t(i) * 6;
            int32_t v[3];
            Apply(m, S16At(file, o), S16At(file, o + 2), S16At(file, o + 4), v);
            int32_t wx = Add(v[0], t[0]), wy = Add(v[1], t[1]), wz = Add(v[2], t[2]);
            wx = Add(Mul(wx, sx) >> 16, ox);
            wy = Add(Mul(wy, sy) >> 16, oy);
            wz = Add(Mul(wz, sz) >> 16, oz);
            w[size_t(i)] = { wx, wy, wz };
            int16_t px = int16_t(wx), py = ProjectY(topY, wy, wz), depth = int16_t(wy);
            s[size_t(i)] = { px, py, depth };
            if (depth < least) least = depth;
            boundMin[2] = std::min<int>(boundMin[2], depth); boundMax[2] = std::max<int>(boundMax[2], depth);
            boundMin[0] = std::min<int>(boundMin[0], px); boundMax[0] = std::max<int>(boundMax[0], px);
            boundMin[1] = std::min<int>(boundMin[1], py); boundMax[1] = std::max<int>(boundMax[1], py);
        }
        nearest[size_t(p)] = least;
        for (int c : piece.children)
        {
            if (c < 0 || c >= int(pieces.size())) continue;
            int32_t saveM[9], saveT[3];
            std::copy(m, m + 9, saveM);
            std::copy(t, t + 3, saveT);
            const Piece& child = pieces[size_t(c)];
            int32_t d[3];
            Apply(m, child.pivot[0], child.pivot[1], child.pivot[2], d);
            for (int k = 0; k < 3; k++) t[k] = Add(t[k], d[k]);
            Walk(c, ox, oy, oz);
            std::copy(saveM, saveM + 9, m);
            std::copy(saveT, saveT + 3, t);
        }
    }

    void WofInstance::Stitch(const BridgeJoint& joint, bool end)
    {
        // {piece, point, joint point}: the deck (0) between the two decks' edges, the sides (1, 2 and 6, 7) from
        // the ground up to them, the rails (3, 4 and 5, 8) from the deck to the rail's top
        struct Copy { int piece, point, from; };
        static const Copy Start[] = {
            { 0, 0, 1 }, { 0, 1, 4 }, { 1, 0, 3 }, { 1, 3, 4 }, { 2, 0, 3 }, { 2, 3, 4 }, { 3, 0, 1 }, { 3, 3, 2 },
            { 4, 0, 1 }, { 4, 3, 2 }, { 5, 0, 4 }, { 5, 3, 5 }, { 6, 0, 0 }, { 6, 3, 1 }, { 7, 0, 0 }, { 7, 3, 1 },
            { 8, 0, 4 }, { 8, 3, 5 } };
        static const Copy End[] = {
            { 0, 2, 4 }, { 0, 3, 1 }, { 1, 1, 3 }, { 1, 2, 4 }, { 2, 1, 3 }, { 2, 2, 4 }, { 3, 1, 1 }, { 3, 2, 2 },
            { 4, 1, 1 }, { 4, 2, 2 }, { 5, 1, 4 }, { 5, 2, 5 }, { 6, 1, 0 }, { 6, 2, 1 }, { 7, 1, 0 }, { 7, 2, 1 },
            { 8, 1, 4 }, { 8, 2, 5 } };
        for (const Copy& c : end ? std::span<const Copy>(End) : std::span<const Copy>(Start))
        {
            if (size_t(c.piece) >= pieces.size() || size_t(c.point) >= world[size_t(c.piece)].size()) continue;
            for (int k = 0; k < 3; k++)
            {
                world[size_t(c.piece)][size_t(c.point)][size_t(k)] = joint.world[c.from][k];
                screen[size_t(c.piece)][size_t(c.point)][size_t(k)] = joint.screen[c.from][k];
            }
        }
        // 0x498110: the first four points of the nine pieces widen the bounds, from the anchor
        for (size_t p = 0; p < 9 && p < pieces.size(); p++)
            for (size_t i = 0; i < 4 && i < screen[p].size(); i++)
            {
                const int16_t x = int16_t(screen[p][i][0] - anchorX), y = int16_t(screen[p][i][1] - anchorY);
                boundMin[0] = std::min<int>(boundMin[0], x); boundMax[0] = std::max<int>(boundMax[0], x);
                boundMin[1] = std::min<int>(boundMin[1], y); boundMax[1] = std::max<int>(boundMax[1], y);
            }
    }

    // 0x49ED90: the face's normal from its three world points, lit by the sun, x the set's third float x 256
    // (point lights would be added here, x "WOF Brightness"), at most 255
    int WofInstance::FaceLight(const std::array<int32_t, 3>& a, const std::array<int32_t, 3>& b, const std::array<int32_t, 3>& c,
                               const Landscape::Sun& sun) const
    {
        int32_t dx1 = b[0] - a[0], dy1 = b[1] - a[1], dz1 = b[2] - a[2];
        int32_t dx2 = c[0] - a[0], dy2 = c[1] - a[1], dz2 = c[2] - a[2];
        int32_t nx = Mul(dy2, dz1) - Mul(dz2, dy1);
        int32_t ny = Mul(dz2, dx1) - Mul(dx2, dz1);
        int32_t nz = Mul(dx2, dy1) - Mul(dy2, dx1);
        double l = Landscape::SlopeLight(nx, ny, nz, sun.strength, sun.ambient, sun) * 1.0 * double(sun.third) * 256.0;
        int v = int(l);
        return v > 255 ? 255 : v;
    }

    namespace
    {
        struct PolyPoint { int x, y, u, v, shade; };

        // 0x456DD0 and its span routines: *provisional*, an affine textured triangle shaded between its corners'
        // levels (shade >> 2) through the shade table; flat when asked; texel 0 left out for a masked material.
        // The game's rasteriser (its edge set-up, rounding and 48 span routines) isn't traced yet; this follows
        // what's known of it: triangles taller than 250 rows are refused, u / v index a 256 wide texture.
        void FillTriangle(PolyPoint p[3], const uint8_t* tex, size_t texSize, bool flat, bool masked, const ShadeTable& table,
                          const TerrainView::Target& t, const uint16_t* blend = nullptr)
        {
            // the flat shade is the first point's as the caller passed it, read before the sort (0x456DF5-0x456E00:
            // ([esi+40h] & 0xFC) << 6); in a piece that isn't smooth only that point holds the face's light
            const int flatLevel = p[0].shade >> 2;
            std::sort(p, p + 3, [](const PolyPoint& a, const PolyPoint& b) { return a.y < b.y; });
            if (p[2].y - p[0].y > 250 || p[2].y == p[0].y) return;
            const double area = double(p[1].x - p[0].x) * (p[2].y - p[0].y) - double(p[2].x - p[0].x) * (p[1].y - p[0].y);
            if (area == 0) return;
            const int y0 = std::max(p[0].y, t.top), y1 = std::min(p[2].y, t.bottom);
            for (int y = y0; y < y1; y++)
            {
                double py = y + 0.5;
                // the long edge and the short one this row crosses
                auto cross = [&](const PolyPoint& a, const PolyPoint& b, double& x, double& u, double& v, double& s) {
                    double f = (py - a.y) / double(b.y - a.y);
                    x = a.x + (b.x - a.x) * f; u = a.u + (b.u - a.u) * f; v = a.v + (b.v - a.v) * f; s = a.shade + (b.shade - a.shade) * f;
                };
                double xa, ua, va, sa, xb, ub, vb, sb;
                cross(p[0], p[2], xa, ua, va, sa);
                if (py < p[1].y) cross(p[0], p[1], xb, ub, vb, sb);
                else cross(p[1], p[2], xb, ub, vb, sb);
                if (xa > xb) { std::swap(xa, xb); std::swap(ua, ub); std::swap(va, vb); std::swap(sa, sb); }
                int xs = std::max(int(std::ceil(xa - 0.5)), t.left), xe = std::min(int(std::ceil(xb - 0.5)), t.right);
                double w = xb - xa;
                uint32_t* out = t.pixels + size_t(y) * size_t(t.pitch);
                for (int x = xs; x < xe; x++)
                {
                    double f = w > 0 ? (x + 0.5 - xa) / w : 0;
                    int u = int(ua + (ub - ua) * f) & 0xFF, v = int(va + (vb - va) * f) & 0xFF;
                    size_t at = size_t(v) * 256 + size_t(u);
                    if (at >= texSize) continue;
                    uint8_t texel = tex[at];
                    if (blend)
                    {
                        // the shadows' span (mode 0x20, 0x4566FE in 16 bits): each of the pixel's 5 / 6 / 5 bit
                        // channels through dat\b16.hsh's row for the texel (red at 0, green at 64, blue at 128),
                        // the three words added
                        const uint32_t px = out[x];
                        const uint16_t* row = blend + size_t(texel & 63) * 256;
                        const uint16_t c = uint16_t(row[(px >> 19) & 31] + row[64 + ((px >> 10) & 63)] + row[128 + ((px >> 3) & 31)]);
                        const uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
                        out[x] = (out[x] & 0xFF000000u) | ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
                        continue;
                    }
                    if (masked && !texel) continue;
                    int level = flat ? flatLevel : int(sa + (sb - sa) * f) >> 2;
                    out[x] = table.Level(level)[texel];
                }
            }
        }
    }

    // 0x49EC00: the pieces by depth (flag 8 first, 0x10 last, the rest by their nearest point), then 0x49EEF0
    void WofInstance::Draw(const Landscape::Sun& sun, const ShadeTable& table, const TerrainView::Target& target, int ox, int oy) const
    {
        std::vector<std::pair<int, int>> order; // {piece, key}
        for (size_t p = 0; p < pieces.size(); p++)
        {
            int key = (pieces[p].flags & 8) ? -1000000 : (pieces[p].flags & 0x10) ? 1000000 : nearest[p];
            order.push_back({ int(p), key });
        }
        std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        uint8_t faceLight[256];
        for (const auto& [p, key] : order)
        {
            const Piece& piece = pieces[size_t(p)];
            if (hidden[size_t(p)]) continue;
            const auto& w = world[size_t(p)];
            const auto& s = screen[size_t(p)];
            auto face = [&](int f) { return &file[size_t(piece.faceOffset) + size_t(f) * 10]; };
            if (size_t(piece.faceOffset) + size_t(piece.faceCount) * 10 > file.size()) continue;
            for (int f = 0; f < piece.faceCount; f++)
            {
                const uint8_t* fd = face(f);
                if (fd[0] >= w.size() || fd[1] >= w.size() || fd[2] >= w.size()) { faceLight[f] = 0; continue; }
                faceLight[f] = uint8_t(FaceLight(w[fd[0]], w[fd[1]], w[fd[2]], sun));
            }
            int flatFlag = 0;
            if (piece.flags & 0x20)
            {
                // smooth: each group averages its faces' lights into its points
                uint32_t off = U32At(file, size_t(smooth) + size_t(p) * 4);
                if (int32_t(off) > 0 && off < file.size())
                {
                    const uint8_t* q = &file[off];
                    const uint8_t* end = file.data() + file.size();
                    int groups = *q++;
                    for (int g = 0; g < groups && q < end; g++)
                    {
                        int n = *q++;
                        int sum = 0;
                        for (int i = 0; i < n && q < end; i++) sum += faceLight[*q++];
                        int avg = n > 0 ? sum / n : 0;
                        for (int i = 0; i < n && q < end; i++) vertexLight[*q++] = uint8_t(avg);
                    }
                }
            }
            else
            {
                for (int f = 0; f < piece.faceCount; f++) vertexLight[face(f)[0]] = faceLight[f];
                flatFlag = 4;
            }
            for (int f = 0; f < piece.faceCount; f++)
            {
                const uint8_t* fd = face(f);
                if (fd[0] >= s.size() || fd[1] >= s.size() || fd[2] >= s.size()) continue;
                const auto &a = s[fd[0]], &b = s[fd[1]], &c = s[fd[2]];
                int32_t cr = Mul(b[0] - a[0], c[1] - b[1]) - Mul(c[0] - b[0], b[1] - a[1]);
                if (cr >= 0) continue; // facing away
                int material = fd[3];
                if (material >= 0xFF) continue;
                uint32_t entry = U32At(file, size_t(materials) + size_t(material) * 4);
                size_t texAt = size_t(texture) + (entry & 0x7FFFFFFF);
                if (texAt >= file.size()) continue;
                int l0 = vertexLight[fd[0]], l1 = vertexLight[fd[1]], l2 = vertexLight[fd[2]];
                bool flat = (l0 == l2 && l0 == l1) || flatFlag;
                PolyPoint pts[3] = {
                    { a[0] + ox, a[1] + oy, fd[4], fd[5], l0 },
                    { c[0] + ox, c[1] + oy, fd[8], fd[9], l2 },
                    { b[0] + ox, b[1] + oy, fd[6], fd[7], l1 } };
                FillTriangle(pts, &file[texAt], file.size() - texAt, flat, (entry & 0x80000000u) != 0, table, target, shadowTable);
            }
        }
    }
}
