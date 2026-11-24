// MODERN (update 248, docs/MODELS.md): a unit's model (WOF) in common formats. A WOF is a tree of named pieces, each
// with a pivot, up to 255 points and up to 255 triangles, each triangle's corners placed in the model's 256-wide
// texture atlas through its material (an offset into the atlas, and whether its colour 0 is see-through). The
// engine poses and animates the pieces by their place in the tree, so a replacement keeps the game's tree, pivots,
// animation, materials and atlas, and gives the pieces it names new points and triangles.
//
// Export: ExportObj writes the model as OBJ (one group a piece, named as the piece, in the model's own space: each
// point plus its piece's place in the tree at rest) with an MTL naming the atlas picture, so it opens in any 3D tool.
// Import: an OBJ, glTF or GLB is read into pieces by name (ParseObj, ParseGltf), and ApplyMeshes writes them into a
// copy of the game's WOF: the new points and triangles are added at the file's end and the named pieces' records
// pointed at them, so everything else in the file stays as it was.
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace wow
{
    struct MeshPiece
    {
        std::string name;                               // the WOF piece it replaces (any case)
        std::vector<std::array<float, 3>> points;       // in the model's space, as ExportObj writes them
        struct Triangle
        {
            int point[3] = {};
            float uv[3][2] = {};                        // 0..1 across the atlas, v up (OBJ's and glTF's own sense, v
            bool hasUv = false;                         //   from the bottom for OBJ; ParseGltf turns glTF's round)
            int material = -1;                          // "material_N" (or a name ending _N): the WOF's material N
        };
        std::vector<Triangle> triangles;
    };

    // The WOF as OBJ text and its MTL (both name `texture`, the atlas picture, e.g. "AIRSHIP3.png"). Empty if the
    // data isn't a WOF.
    bool ExportObj(std::span<const uint8_t> wof, const std::string& mtlName, const std::string& texture, std::string& obj, std::string& mtl);

    // OBJ text (v, vt, f with any polygons fanned into triangles, o / g naming the piece, usemtl the material); what
    // can't be read is noted in warnings. A piece's points are the v lines under its o / g, in their order (the
    // engine finds some points by number: keep an exported piece's points in order, adding new ones after)
    std::vector<MeshPiece> ParseObj(const std::string& text, std::vector<std::string>& warnings);
    // glTF (.gltf with its buffers inside as data: URIs, or .glb): each node with a mesh is the piece of its name,
    // its points through the node's place in the scene; the first texture coordinates; material names as OBJ's.
    // Vertices at the same place are one point (glTF splits a point where its texture places differ).
    std::vector<MeshPiece> ParseGltf(std::span<const uint8_t> data, std::vector<std::string>& warnings);

    // A copy of the WOF with those pieces' points and triangles (the others as they were). False, with why in
    // warnings, if nothing could be put in: a piece no WOF piece is called, more than 255 points or triangles in a
    // piece, a triangle whose corners no material of the model can reach, are each noted and that piece kept.
    bool ApplyMeshes(std::vector<uint8_t>& wof, const std::vector<MeshPiece>& pieces, std::vector<std::string>& warnings);
}
