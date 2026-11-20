#include "core/Models.h"
#include "core/Bytes.h"

#include <algorithm>
#include <cstring>

namespace wow
{
    namespace
    {
        uint16_t U16(std::span<const uint8_t> d, size_t o)
        {
            if (o + 2 > d.size()) throw FormatError("read past the end of the data");
            return uint16_t(d[o] | (d[o + 1] << 8));
        }
        int16_t S16(std::span<const uint8_t> d, size_t o) { return int16_t(U16(d, o)); }
        uint32_t U32(std::span<const uint8_t> d, size_t o)
        {
            if (o + 4 > d.size()) throw FormatError("read past the end of the data");
            return uint32_t(d[o]) | (uint32_t(d[o + 1]) << 8) | (uint32_t(d[o + 2]) << 16) | (uint32_t(d[o + 3]) << 24);
        }
        int32_t S32(std::span<const uint8_t> d, size_t o) { return int32_t(U32(d, o)); }
    }

    WofModel WofModel::Load(std::span<const uint8_t> d)
    {
        constexpr size_t TableStart = 0x30, RecordSize = 97;
        if (d.size() < TableStart) throw FormatError("WOF: too short");
        WofModel m;
        for (int i = 0; i < 12; i++) m.header[i] = U32(d, size_t(i) * 4);
        int count = U16(d, 0);
        uint32_t vertexBase = m.header[3];
        uint32_t materials = m.header[9], texture = m.header[10], end = m.header[11];
        if (count <= 0 || count > 500 || vertexBase == 0 || vertexBase >= d.size())
            throw FormatError("WOF: bad piece count or vertex offset");
        if (TableStart + size_t(count) * RecordSize > d.size()) throw FormatError("WOF: piece table runs past the end");

        m.pieces.resize(size_t(count));
        for (int p = 0; p < count; p++)
        {
            size_t r = TableStart + size_t(p) * RecordSize;
            WofPiece& piece = m.pieces[size_t(p)];
            piece.name.assign(reinterpret_cast<const char*>(&d[r]), strnlen(reinterpret_cast<const char*>(&d[r]), 16));
            piece.flags = d[r + 0x10];
            int vertexCount = d[r + 0x11], faceCount = d[r + 0x12];
            for (int a = 0; a < 3; a++) piece.pivot[a] = S16(d, r + 0x13 + size_t(a) * 2);
            uint32_t vertexOffset = U32(d, r + 0x19);
            uint32_t faceOffset = U32(d, r + 0x1C) >> 8;
            for (int c = 0; c < 16; c++)
            {
                int child = S32(d, r + 0x21 + size_t(c) * 4);
                if (child < 0) break;
                piece.children.push_back(child);
            }
            for (int v = 0; v < vertexCount; v++)
            {
                size_t o = size_t(vertexBase) + vertexOffset + size_t(v) * 6;
                piece.vertices.push_back({ S16(d, o), S16(d, o + 2), S16(d, o + 4) });
            }
            for (int f = 0; f < faceCount; f++)
            {
                size_t o = size_t(faceOffset) + size_t(f) * 10;
                if (o + 10 > d.size()) throw FormatError("WOF: faces run past the end");
                WofFace face;
                for (int k = 0; k < 3; k++) { face.v[k] = d[o + size_t(k)]; face.uv[k][0] = d[o + 4 + size_t(k) * 2]; face.uv[k][1] = d[o + 5 + size_t(k) * 2]; }
                face.material = d[o + 3];
                piece.faces.push_back(face);
            }
        }
        // parents, then the pivots summed from the root down
        for (int p = 0; p < count; p++)
            for (int c : m.pieces[size_t(p)].children)
                if (c >= 0 && c < count) m.pieces[size_t(c)].parent = p;
        std::vector<bool> done(size_t(count), false);
        for (bool progress = true; progress;)
        {
            progress = false;
            for (int p = 0; p < count; p++)
            {
                WofPiece& piece = m.pieces[size_t(p)];
                if (done[size_t(p)] || (piece.parent >= 0 && !done[size_t(piece.parent)])) continue;
                for (int a = 0; a < 3; a++)
                    piece.accumulated[a] = piece.pivot[a] + (piece.parent >= 0 ? m.pieces[size_t(piece.parent)].accumulated[a] : 0);
                done[size_t(p)] = progress = true;
            }
        }

        if (texture > 0 && end > texture && end <= d.size())
        {
            m.texture.assign(d.begin() + texture, d.begin() + end);
            m.textureHeight = int((end - texture) / 256);
        }
        if (materials > 0 && texture > materials && texture <= d.size()) m.materials.assign(d.begin() + materials, d.begin() + texture);
        return m;
    }

    IobModel IobModel::Load(std::span<const uint8_t> d)
    {
        constexpr size_t Header = 0x22;
        if (d.size() < Header) throw FormatError("IOB: too short");
        IobModel m;
        m.views = U16(d, 0);
        m.faceCount = S32(d, 0x02);
        m.litTriangles = S32(d, 0x06);
        m.animated = U32(d, 0x0A) != 0;
        m.width = S32(d, 0x0E);
        m.field12 = S32(d, 0x12);
        m.field16 = S32(d, 0x16);
        m.height = m.field12; // what WoWViewer draws with (it notes 0x16 is the y offset)
        uint32_t planeBytes = U32(d, 0x1A), indexOffset = U32(d, 0x1E);
        if (m.faceCount <= 0 || planeBytes != uint32_t(m.faceCount) * 6) throw FormatError("IOB: the point section doesn't match the face count");
        size_t pos = Header;
        for (int i = 0; i < m.faceCount; i++, pos += 6) m.points.push_back({ S16(d, pos), S16(d, pos + 2), S16(d, pos + 4) });
        int normalCount = m.animated ? m.faceCount : m.litTriangles;
        if (pos + size_t(normalCount) * 3 > d.size()) throw FormatError("IOB: normals run past the end");
        for (int i = 0; i < normalCount; i++, pos += 3) m.normals.push_back({ int8_t(d[pos]), int8_t(d[pos + 1]), int8_t(d[pos + 2]) });
        size_t index = Header + indexOffset;
        if (m.litTriangles < 0 || index + size_t(m.litTriangles) * 18 > d.size()) throw FormatError("IOB: index table runs past the end");
        for (int i = 0; i < m.litTriangles; i++)
        {
            size_t o = index + size_t(i) * 18;
            IobPatch p;
            p.width = d[o]; p.height = d[o + 1]; p.colour = d[o + 2]; p.mode = d[o + 3];
            p.x = S16(d, o + 4); p.y = S16(d, o + 6);
            for (int k = 0; k < 3; k++) p.normals[k] = U16(d, o + 8 + size_t(k) * 2);
            p.offset = U32(d, o + 14);
            m.patches.push_back(p);
        }
        m.patchData.assign(d.begin() + Header, d.end());
        return m;
    }

    IndexedImage IobModel::Picture() const
    {
        // Each patch is rows of [skip][run]: run bit 7 set repeats the next byte (run & 0x7F) times, otherwise
        // run bytes follow; [0][0] ends the patch, as does the next patch's data.
        std::vector<uint32_t> starts;
        for (const IobPatch& p : patches) starts.push_back(p.offset);
        std::sort(starts.begin(), starts.end());
        starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
        starts.push_back(uint32_t(patchData.size()));

        struct Pixel { int x, y; uint8_t c; };
        std::vector<Pixel> pixels;
        int maxX = 0, maxY = 0, minX = 0, minY = 0;
        for (const IobPatch& p : patches)
        {
            if (p.offset >= patchData.size()) continue;
            size_t pos = p.offset;
            size_t end = *std::upper_bound(starts.begin(), starts.end(), p.offset);
            for (int row = 0; pos + 1 < end; row++)
            {
                int skip = patchData[pos++], run = patchData[pos++];
                if (skip == 0 && run == 0) break;
                int count = run & 0x7F;
                int y = p.y + row;
                if (run & 0x80)
                {
                    if (pos >= end) break;
                    uint8_t c = patchData[pos++];
                    for (int k = 0; k < count; k++) pixels.push_back({ p.x + skip + k, y, c });
                }
                else
                    for (int k = 0; k < count && pos < end; k++) pixels.push_back({ p.x + skip + k, y, patchData[pos++] });
            }
        }
        for (const Pixel& px : pixels)
        {
            maxX = std::max(maxX, px.x + 1); maxY = std::max(maxY, px.y + 1);
            minX = std::min(minX, px.x); minY = std::min(minY, px.y);
        }
        IndexedImage image;
        image.width = maxX - minX;
        image.height = maxY - minY;
        image.pixels.assign(size_t(image.width) * size_t(image.height), 0);
        for (const Pixel& px : pixels) image.pixels[size_t(px.y - minY) * size_t(image.width) + size_t(px.x - minX)] = px.c;
        return image;
    }

    Terrain Terrain::Load(std::span<const uint8_t> cls, std::span<const uint8_t> atm)
    {
        constexpr size_t Header = 0x44;
        if (cls.size() < Header) throw FormatError("CLS: too short");
        Terrain t;
        t.width = S32(cls, 0);
        t.height = S32(cls, 4);
        t.triangles = S32(cls, 0x0C);
        t.heightScale = U32(cls, 0x2C); // 0x47BB92 copies it to WLandscape +0x3C
        t.yBase = S32(cls, 0x18) + S32(cls, 0x30);
        if (t.width <= 1 || t.height <= 1 || t.width > 4096 || t.height > 4096) throw FormatError("CLS: bad grid size");
        size_t count = size_t(t.width) * size_t(t.height);
        if (Header + count > cls.size()) throw FormatError("CLS: heights run past the end");
        t.heights.assign(cls.begin() + Header, cls.begin() + Header + count);
        for (int i = 0; i < 17; i++) t.header[i] = U32(cls, size_t(i) * 4);
        t.rest.assign(cls.begin() + Header + count, cls.end());
        if (!atm.empty()) t.tiles.assign(atm.begin(), atm.end());
        return t;
    }
}
