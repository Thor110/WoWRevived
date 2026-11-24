#include "core/WofMesh.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>

#include "../../third_party/cgltf.h"

namespace wow
{
    namespace
    {
        // The parts of a WOF this needs (as WofInstance and WofModel read them): the piece table at 0x30, 0x61 bytes
        // a piece (name 16, flags +0x10, point count +0x11, triangle count +0x12, pivot +0x13, its points at
        // +0x19 from the point base 0x0C, its triangles at +0x1D, its 16 children +0x21); the root 0x10; the
        // materials 0x24 (a word each: its offset into the atlas, the top bit its colour 0 see-through); the atlas
        // 0x28 to 0x2C, 256 wide; the smooth pieces' table 0x2C (a word a piece). A triangle is 10 bytes: its three
        // points, its material, then each corner's atlas place from the material's offset.
        constexpr size_t Table = 0x30, Record = 0x61;

        uint32_t U32(std::span<const uint8_t> d, size_t o) { return o + 4 <= d.size() ? uint32_t(d[o]) | uint32_t(d[o + 1]) << 8 | uint32_t(d[o + 2]) << 16 | uint32_t(d[o + 3]) << 24 : 0; }
        int16_t S16(std::span<const uint8_t> d, size_t o) { return o + 2 <= d.size() ? int16_t(d[o] | d[o + 1] << 8) : 0; }
        void Put32(std::vector<uint8_t>& d, size_t o, uint32_t v) { for (int i = 0; i < 4; i++) d[o + size_t(i)] = uint8_t(v >> (8 * i)); }

        struct Layout
        {
            int count = 0, root = 0, atlasHeight = 0;
            uint32_t base = 0, materials = 0, texture = 0, smooth = 0;
            struct Piece { std::string name; uint8_t flags = 0; int points = 0, triangles = 0; int16_t pivot[3] = {}; uint32_t pointAt = 0, triangleAt = 0; std::vector<int> children; int parent = -1; };
            std::vector<Piece> pieces;
            struct Material { uint32_t offset = 0; bool see = false; };
            std::vector<Material> mats;
            std::vector<std::array<double, 3>> rest;   // each piece's place in the model at rest

            bool Read(std::span<const uint8_t> d)
            {
                if (d.size() < Table) return false;
                count = d[0] | d[1] << 8;
                root = int(int32_t(U32(d, 0x10)));
                base = U32(d, 0x0C);
                materials = U32(d, 0x24);
                texture = U32(d, 0x28);
                smooth = U32(d, 0x2C);
                if (count <= 0 || count > 500 || Table + size_t(count) * Record > d.size()) return false;
                if (root < 0 || root >= count) root = 0;
                atlasHeight = texture > 0 && smooth > texture && smooth <= d.size() ? int((smooth - texture) / 256) : 0;
                pieces.resize(size_t(count));
                for (int p = 0; p < count; p++)
                {
                    const size_t r = Table + size_t(p) * Record;
                    Piece& piece = pieces[size_t(p)];
                    piece.name.assign(reinterpret_cast<const char*>(&d[r]), strnlen(reinterpret_cast<const char*>(&d[r]), 16));
                    piece.flags = d[r + 0x10];
                    piece.points = d[r + 0x11];
                    piece.triangles = d[r + 0x12];
                    for (int a = 0; a < 3; a++) piece.pivot[a] = S16(d, r + 0x13 + size_t(a) * 2);
                    piece.pointAt = U32(d, r + 0x19);
                    piece.triangleAt = U32(d, r + 0x1D);
                    for (int c = 0; c < 16; c++)
                    {
                        const int child = int(int32_t(U32(d, r + 0x21 + size_t(c) * 4)));
                        if (child < 0) break;
                        if (child < count) piece.children.push_back(child);
                    }
                }
                for (int p = 0; p < count; p++)
                    for (int c : pieces[size_t(p)].children) pieces[size_t(c)].parent = p;
                if (materials > 0 && texture > materials && texture <= d.size())
                    for (uint32_t at = materials; at + 4 <= texture; at += 4)
                    {
                        const uint32_t e = U32(d, at);
                        mats.push_back({ e & 0x7FFFFFFF, (e & 0x80000000u) != 0 });
                    }
                // at rest: the root at its pivot with y turned round (WofInstance::Transform), each child at its
                // parent's place plus its pivot (Walk)
                rest.assign(size_t(count), { 0, 0, 0 });
                std::vector<int> stack{ root };
                std::vector<bool> seen(size_t(count), false);
                rest[size_t(root)] = { double(pieces[size_t(root)].pivot[0]), -double(pieces[size_t(root)].pivot[1]), double(pieces[size_t(root)].pivot[2]) };
                seen[size_t(root)] = true;
                while (!stack.empty())
                {
                    const int p = stack.back();
                    stack.pop_back();
                    for (int c : pieces[size_t(p)].children)
                    {
                        if (seen[size_t(c)]) continue;
                        seen[size_t(c)] = true;
                        for (int a = 0; a < 3; a++) rest[size_t(c)][size_t(a)] = rest[size_t(p)][size_t(a)] + pieces[size_t(c)].pivot[a];
                        stack.push_back(c);
                    }
                }
                return true;
            }
        };

        std::string Lower(std::string s) { for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c))); return s; }
        std::string Trim(const std::string& s)
        {
            const size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
            return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
        }
        // "material_7", "Material.7" or any name ending in a number: that number; else -1
        int MaterialNumber(const std::string& name)
        {
            size_t end = name.size();
            while (end > 0 && std::isspace(static_cast<unsigned char>(name[end - 1]))) end--;
            size_t start = end;
            while (start > 0 && std::isdigit(static_cast<unsigned char>(name[start - 1]))) start--;
            if (start == end || end - start > 3) return -1;
            return std::atoi(name.substr(start, end - start).c_str());
        }
    }

    bool ExportObj(std::span<const uint8_t> wof, const std::string& mtlName, const std::string& texture, std::string& obj, std::string& mtl)
    {
        Layout l;
        if (!l.Read(wof)) return false;
        std::ostringstream o, m;
        o << "# WoWEngine's export of a WOF model (docs/MODELS.md): one group a piece, named as the piece; points in\n"
             "# the model's space at rest; texture coordinates across the whole atlas (" << texture << ").\n"
             "# Put back as <model>.obj in the texture pack's folder for the model; keep the groups' names.\n";
        o << "mtllib " << mtlName << "\n";
        std::vector<int> used;
        size_t firstPoint = 1;
        const double h = l.atlasHeight > 0 ? double(l.atlasHeight) : 256.0;
        for (int p = 0; p < l.count; p++)
        {
            const auto& piece = l.pieces[size_t(p)];
            o << "o " << (piece.name.empty() ? "piece" + std::to_string(p) : piece.name) << "\n";
            for (int i = 0; i < piece.points; i++)
            {
                const size_t at = size_t(l.base) + piece.pointAt + size_t(i) * 6;
                o << "v " << S16(wof, at) + l.rest[size_t(p)][0] << " " << S16(wof, at + 2) + l.rest[size_t(p)][1] << " " << S16(wof, at + 4) + l.rest[size_t(p)][2] << "\n";
            }
            int material = -2;
            for (int f = 0; f < piece.triangles; f++)
            {
                const size_t at = size_t(piece.triangleAt) + size_t(f) * 10;
                if (at + 10 > wof.size()) break;
                const int mat = wof[at + 3];
                if (mat != material)
                {
                    o << "usemtl material_" << mat << "\n";
                    material = mat;
                    if (std::find(used.begin(), used.end(), mat) == used.end()) used.push_back(mat);
                }
                const uint32_t off = mat < int(l.mats.size()) ? l.mats[size_t(mat)].offset : 0;
                for (int k = 0; k < 3; k++)
                {
                    const double ax = double(off % 256) + wof[at + 4 + size_t(k) * 2], ay = double(off / 256) + wof[at + 5 + size_t(k) * 2];
                    o << "vt " << (ax + 0.5) / 256.0 << " " << 1.0 - (ay + 0.5) / h << "\n";
                }
                o << "f";
                for (int k = 0; k < 3; k++) o << " " << firstPoint + wof[at + size_t(k)] << "/" << -(3 - k);
                o << "\n";
            }
            firstPoint += size_t(piece.points);
        }
        std::sort(used.begin(), used.end());
        m << "# WoWEngine's export of a WOF model's materials: each places its triangles in the atlas\n";
        for (int mat : used)
        {
            m << "newmtl material_" << mat << "\n";
            if (mat < int(l.mats.size()) && l.mats[size_t(mat)].see) m << "# colour 0 see-through\n";
            m << "Kd 1 1 1\nmap_Kd " << texture << "\n";
        }
        obj = o.str();
        mtl = m.str();
        return true;
    }

    std::vector<MeshPiece> ParseObj(const std::string& text, std::vector<std::string>& warnings)
    {
        std::vector<std::array<float, 3>> v;
        std::vector<std::array<float, 2>> vt;
        std::vector<MeshPiece> pieces;
        std::vector<std::map<int, int>> local;      // each piece's OBJ point to its own
        int current = -1, material = -1, number = 0, unnamed = 0;
        std::istringstream in(text);
        std::string line;
        auto piece = [&](const std::string& name) {
            for (size_t i = 0; i < pieces.size(); i++)
                if (Lower(pieces[i].name) == Lower(name)) return int(i);
            pieces.push_back({ name, {}, {} });
            local.emplace_back();
            return int(pieces.size()) - 1;
        };
        while (std::getline(in, line))
        {
            number++;
            if (const size_t hash = line.find('#'); hash != std::string::npos) line.erase(hash);
            line = Trim(line);
            if (line.empty()) continue;
            std::istringstream words(line);
            std::string key;
            words >> key;
            if (key == "v")
            {
                // (a point belongs to the group it's listed under, in order, used by a face or not: the engine
                // finds some of a piece's points by their number, a gun's muzzle, a walker's feet)
                float x = 0, y = 0, z = 0;
                words >> x >> y >> z;
                v.push_back({ x, y, z });
                if (current >= 0)
                {
                    pieces[size_t(current)].points.push_back({ x, y, z });
                    local[size_t(current)][int(v.size()) - 1] = int(pieces[size_t(current)].points.size()) - 1;
                }
            }
            else if (key == "vt") { float a = 0, b = 0; words >> a >> b; vt.push_back({ a, b }); }
            else if (key == "o" || key == "g") { std::string rest; std::getline(words, rest); rest = Trim(rest); if (!rest.empty()) current = piece(rest); }
            else if (key == "usemtl") { std::string rest; std::getline(words, rest); material = MaterialNumber(Trim(rest)); }
            else if (key == "f")
            {
                if (current < 0) { unnamed++; continue; }
                std::vector<std::pair<int, int>> corners;
                for (std::string c; words >> c;)
                {
                    int vi = 0, ti = 0;
                    const size_t s1 = c.find('/');
                    vi = std::atoi(c.substr(0, s1).c_str());
                    if (s1 != std::string::npos)
                    {
                        const size_t s2 = c.find('/', s1 + 1);
                        const std::string t = c.substr(s1 + 1, s2 == std::string::npos ? std::string::npos : s2 - s1 - 1);
                        if (!t.empty()) ti = std::atoi(t.c_str());
                    }
                    vi = vi < 0 ? int(v.size()) + vi : vi - 1;
                    ti = ti < 0 ? int(vt.size()) + ti : ti - 1;
                    if (vi < 0 || vi >= int(v.size())) { warnings.push_back("line " + std::to_string(number) + ": a point that isn't there"); corners.clear(); break; }
                    corners.push_back({ vi, ti >= 0 && ti < int(vt.size()) ? ti : -1 });
                }
                MeshPiece& p = pieces[size_t(current)];
                auto pointOf = [&](int vi) {
                    auto& map = local[size_t(current)];
                    auto it = map.find(vi);
                    if (it != map.end()) return it->second;
                    p.points.push_back(v[size_t(vi)]);
                    return map[vi] = int(p.points.size()) - 1;
                };
                // a polygon fanned into triangles from its first corner
                for (size_t k = 1; k + 1 < corners.size(); k++)
                {
                    MeshPiece::Triangle t;
                    const std::pair<int, int> three[3] = { corners[0], corners[k], corners[k + 1] };
                    t.hasUv = true;
                    for (int c = 0; c < 3; c++)
                    {
                        t.point[c] = pointOf(three[c].first);
                        if (three[c].second >= 0) { t.uv[c][0] = vt[size_t(three[c].second)][0]; t.uv[c][1] = vt[size_t(three[c].second)][1]; }
                        else t.hasUv = false;
                    }
                    t.material = material;
                    p.triangles.push_back(t);
                }
            }
        }
        if (unnamed) warnings.push_back(std::to_string(unnamed) + " faces before any o or g line (no piece named): left out");
        return pieces;
    }

    std::vector<MeshPiece> ParseGltf(std::span<const uint8_t> data, std::vector<std::string>& warnings)
    {
        std::vector<MeshPiece> pieces;
        cgltf_options options{};
        cgltf_data* gltf = nullptr;
        if (cgltf_parse(&options, data.data(), data.size(), &gltf) != cgltf_result_success) { warnings.push_back("not a glTF file this reads"); return pieces; }
        if (cgltf_load_buffers(&options, gltf, nullptr) != cgltf_result_success)
        {
            warnings.push_back("its buffers can't be read (a .gltf's must be inside it, as data: URIs; or use .glb)");
            cgltf_free(gltf);
            return pieces;
        }
        for (cgltf_size n = 0; n < gltf->nodes_count; n++)
        {
            const cgltf_node& node = gltf->nodes[n];
            if (!node.mesh) continue;
            const std::string name = node.name ? node.name : (node.mesh->name ? node.mesh->name : "");
            if (name.empty()) { warnings.push_back("a mesh node with no name: left out"); continue; }
            MeshPiece* p = nullptr;
            for (MeshPiece& q : pieces) if (Lower(q.name) == Lower(name)) p = &q;
            if (!p) { pieces.push_back({ name, {}, {} }); p = &pieces.back(); }
            float world[16];
            cgltf_node_transform_world(&node, world);
            std::vector<std::array<float, 3>> normals;  // each point's normal (all 0 without normals)
            for (cgltf_size pr = 0; pr < node.mesh->primitives_count; pr++)
            {
                const cgltf_primitive& prim = node.mesh->primitives[pr];
                if (prim.type != cgltf_primitive_type_triangles) { warnings.push_back(name + ": a part that isn't triangles: left out"); continue; }
                const cgltf_accessor *pos = nullptr, *uv = nullptr, *normal = nullptr;
                for (cgltf_size a = 0; a < prim.attributes_count; a++)
                {
                    if (prim.attributes[a].type == cgltf_attribute_type_position) pos = prim.attributes[a].data;
                    if (prim.attributes[a].type == cgltf_attribute_type_normal) normal = prim.attributes[a].data;
                    if (prim.attributes[a].type == cgltf_attribute_type_texcoord && prim.attributes[a].index == 0) uv = prim.attributes[a].data;
                }
                if (!pos) continue;
                // glTF keeps a texture place a vertex, so a point on a seam comes as several vertices: those at
                // the same place (and with the same normal, where there are normals: a hard edge's stay apart, as
                // the game's own models keep a point for each side of one) are one point again, the first one's, so
                // a piece keeps its points and their order
                std::vector<int> pointOf(pos->count);
                for (cgltf_size i = 0; i < pos->count; i++)
                {
                    float x[3] = {}, nv[3] = {};
                    cgltf_accessor_read_float(pos, i, x, 3);
                    if (normal) cgltf_accessor_read_float(normal, i, nv, 3);
                    const std::array<float, 3> at = { world[0] * x[0] + world[4] * x[1] + world[8] * x[2] + world[12],
                                                      world[1] * x[0] + world[5] * x[1] + world[9] * x[2] + world[13],
                                                      world[2] * x[0] + world[6] * x[1] + world[10] * x[2] + world[14] };
                    const std::array<float, 3> facing = { nv[0], nv[1], nv[2] };
                    int found = -1;
                    for (size_t k = 0; k < p->points.size() && found < 0; k++)
                        if (p->points[k] == at && (k >= normals.size() || normals[k] == facing)) found = int(k);
                    if (found >= 0) pointOf[i] = found;
                    else
                    {
                        pointOf[i] = int(p->points.size());
                        p->points.push_back(at);
                        normals.resize(p->points.size() - 1, facing);
                        normals.push_back(facing);
                    }
                }
                const int material = prim.material && prim.material->name ? MaterialNumber(prim.material->name) : -1;
                const cgltf_size corners = prim.indices ? prim.indices->count : pos->count;
                for (cgltf_size i = 0; i + 2 < corners; i += 3)
                {
                    MeshPiece::Triangle t;
                    t.hasUv = uv != nullptr;
                    for (int c = 0; c < 3; c++)
                    {
                        const cgltf_size index = prim.indices ? cgltf_accessor_read_index(prim.indices, i + cgltf_size(c)) : i + cgltf_size(c);
                        t.point[c] = index < pointOf.size() ? pointOf[index] : -1;
                        if (uv)
                        {
                            float st[2] = {};
                            cgltf_accessor_read_float(uv, index, st, 2);
                            t.uv[c][0] = st[0];
                            t.uv[c][1] = 1.0f - st[1];      // (glTF's v runs down the picture, OBJ's up)
                        }
                    }
                    t.material = material;
                    p->triangles.push_back(t);
                }
            }
        }
        cgltf_free(gltf);
        return pieces;
    }

    bool ApplyMeshes(std::vector<uint8_t>& wof, const std::vector<MeshPiece>& meshes, std::vector<std::string>& warnings)
    {
        Layout l;
        if (!l.Read(wof)) { warnings.push_back("the game's model isn't a WOF this reads"); return false; }
        const size_t originalSize = wof.size();
        const int h = l.atlasHeight > 0 ? l.atlasHeight : 256;
        bool any = false;
        for (const MeshPiece& mesh : meshes)
        {
            int p = -1;
            for (int i = 0; i < l.count; i++) if (Lower(l.pieces[size_t(i)].name) == Lower(mesh.name)) p = i;
            if (p < 0) { warnings.push_back("\"" + mesh.name + "\": the model has no piece of that name; left out"); continue; }
            if (mesh.points.size() > 255 || mesh.triangles.size() > 255)
            {
                warnings.push_back("\"" + mesh.name + "\": " + std::to_string(mesh.points.size()) + " points and " + std::to_string(mesh.triangles.size()) +
                                   " triangles, where a piece holds 255 of each; the game's is kept");
                continue;
            }
            const auto& piece = l.pieces[size_t(p)];
            // the piece's own commonest material, for triangles that don't name one
            int fallback = 0;
            {
                std::map<int, int> counts;
                for (int f = 0; f < piece.triangles; f++)
                    if (size_t(piece.triangleAt) + size_t(f) * 10 + 4 <= originalSize) counts[wof[size_t(piece.triangleAt) + size_t(f) * 10 + 3]]++;
                int best = -1;
                for (const auto& [m, n] : counts) if (n > best && m < int(l.mats.size())) { best = n; fallback = m; }
            }
            // each triangle's material and corners: the named material if its part of the atlas holds the corners,
            // else another of the same kind (see-through or not) that does
            std::vector<std::array<uint8_t, 10>> faces;
            int clamped = 0;
            std::vector<MeshPiece::Triangle> kept;
            for (const MeshPiece::Triangle& t : mesh.triangles)
                if (t.point[0] >= 0 && t.point[1] >= 0 && t.point[2] >= 0 && size_t(t.point[0]) < mesh.points.size() &&
                    size_t(t.point[1]) < mesh.points.size() && size_t(t.point[2]) < mesh.points.size()) kept.push_back(t);
            if (kept.size() != mesh.triangles.size()) warnings.push_back("\"" + mesh.name + "\": triangles with points that aren't there left out");
            for (const MeshPiece::Triangle& t : kept)
            {
                std::array<uint8_t, 10> f{};
                int want = t.material >= 0 && t.material < int(l.mats.size()) ? t.material : fallback;
                int ax[3] = {}, ay[3] = {};
                for (int c = 0; c < 3; c++)
                {
                    ax[c] = t.hasUv ? std::clamp(int(std::floor(t.uv[c][0] * 256.0f)), 0, 255) : 0;
                    ay[c] = t.hasUv ? std::clamp(int(std::floor((1.0f - t.uv[c][1]) * float(h))), 0, h - 1) : 0;
                }
                auto fits = [&](int m) {
                    if (m < 0 || m >= int(l.mats.size())) return false;
                    const int ox = int(l.mats[size_t(m)].offset % 256), oy = int(l.mats[size_t(m)].offset / 256);
                    for (int c = 0; c < 3; c++)
                        if (ax[c] - ox < 0 || ax[c] - ox > 255 || ay[c] - oy < 0 || ay[c] - oy > 255) return false;
                    return true;
                };
                if (t.hasUv && !fits(want))
                    for (int m = 0; m < int(l.mats.size()); m++)
                        if (l.mats[size_t(m)].see == (want < int(l.mats.size()) && l.mats[size_t(want)].see) && fits(m)) { want = m; break; }
                const int ox = want < int(l.mats.size()) ? int(l.mats[size_t(want)].offset % 256) : 0;
                const int oy = want < int(l.mats.size()) ? int(l.mats[size_t(want)].offset / 256) : 0;
                if (t.hasUv && !fits(want)) clamped++;
                for (int c = 0; c < 3; c++)
                {
                    f[size_t(c)] = uint8_t(t.point[c]);
                    f[4 + size_t(c) * 2] = uint8_t(std::clamp(ax[c] - ox, 0, 255));
                    f[5 + size_t(c) * 2] = uint8_t(std::clamp(ay[c] - oy, 0, 255));
                }
                f[3] = uint8_t(want);
                faces.push_back(f);
            }
            if (clamped) warnings.push_back("\"" + mesh.name + "\": " + std::to_string(clamped) + " triangles reach past every material's part of the atlas; kept to the edge");
            // the points, in the piece's own space, then the triangles, at the file's end
            const size_t pointsAt = wof.size();
            for (const auto& pt : mesh.points)
                for (int a = 0; a < 3; a++)
                {
                    const int16_t v = int16_t(std::clamp(std::lround(double(pt[size_t(a)]) - l.rest[size_t(p)][size_t(a)]), -32768L, 32767L));
                    wof.push_back(uint8_t(v));
                    wof.push_back(uint8_t(uint16_t(v) >> 8));
                }
            const size_t facesAt = wof.size();
            for (const auto& f : faces) wof.insert(wof.end(), f.begin(), f.end());
            // a smooth piece's groups (WofInstance::Draw): for each point, the triangles that have it, and the point
            // as many times; else the piece is drawn flat
            const size_t r = Table + size_t(p) * Record;
            uint8_t flags = piece.flags;
            // (with the game's numbers of points and triangles the game's own groups still fit, and are kept: they
            // can give a corner of the same place several lights, a hard edge, which groups by point can't)
            const bool sameCounts = int(mesh.points.size()) == piece.points && int(faces.size()) == piece.triangles;
            if ((flags & 0x20) && !sameCounts)
            {
                const size_t entry = size_t(l.smooth) + size_t(p) * 4;
                if (l.smooth > 0 && entry + 4 <= originalSize)
                {
                    const size_t groupsAt = wof.size();
                    std::vector<std::vector<int>> of(mesh.points.size());
                    for (size_t f = 0; f < kept.size(); f++)
                        for (int c = 0; c < 3; c++)
                        {
                            auto& list = of[size_t(kept[f].point[c])];
                            if (std::find(list.begin(), list.end(), int(f)) == list.end()) list.push_back(int(f));
                        }
                    int groups = 0;
                    for (const auto& list : of) if (!list.empty()) groups++;
                    wof.push_back(uint8_t(groups));
                    for (size_t pt = 0; pt < of.size(); pt++)
                    {
                        if (of[pt].empty()) continue;
                        wof.push_back(uint8_t(of[pt].size()));
                        for (int f : of[pt]) wof.push_back(uint8_t(f));
                        for (size_t i = 0; i < of[pt].size(); i++) wof.push_back(uint8_t(pt));
                    }
                    Put32(wof, entry, uint32_t(groupsAt));
                }
                else flags = uint8_t(flags & ~0x20);
            }
            wof[r + 0x10] = flags;
            wof[r + 0x11] = uint8_t(mesh.points.size());
            wof[r + 0x12] = uint8_t(faces.size());
            Put32(wof, r + 0x19, uint32_t(pointsAt - l.base));
            Put32(wof, r + 0x1D, uint32_t(facesAt));
            any = true;
        }
        return any;
    }
}
