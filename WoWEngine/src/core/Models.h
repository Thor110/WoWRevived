// The 3D formats, ported from WoWViewer's decoders (WOFDecoder.cs, IOBDecoder.cs, CLSDecoder.cs), which were
// worked out from the game (WOF: 0x49E900; IOB: 0x49DF40, 0x49E1E0, patches 0x4579A4 / 0x45702D; CLS:
// 0x47B900, 0x47BD80). Only reading is here; drawing them belongs to the battle map.
//
// WOF (units): u16 piece count at 0, then header fields up to 0x30, then 97-byte piece records:
//   name[16], flags, vertex count, face count, s16 pivot x/y/z (relative to the parent piece), u32 vertex byte
//   offset (from the vertex section at header 0x0C), u32 face offset (>> 8), then up to 16 s32 child pieces
//   ending with -1. Vertices are s16 x/y/z; faces are 10 bytes (v0 v1 v2 material, then u/v for each corner).
//   The texture atlas is 256 pixels wide, from header 0x28 to 0x2C; materials (4 bytes each) from 0x24 to it.
// IOB (buildings): a 0x22 byte header (u16 view count, u32 face count, u32 lit triangle count, u32 animated,
//   u32 width, u32 0x12, u32 height(?), u32 plane section size = faces x 6, u32 index table offset from 0x22),
//   then s16 x/y/z points, s8 normals, and an 18 byte index entry per lit triangle placing an RLE patch of the
//   pre-rendered building picture.
// CLS (terrain heights): u32 grid width, height, vertex count, triangle count, 13 more header words to 0x44
//   (0x38 repeats the vertex count; 0x3C and 0x40 grow with the map), then a height byte per vertex, row by
//   row. Several megabytes follow, which WoWViewer reads as u16 triangle strips (0xFFFF restarts) until the
//   values stop making sense; what's really there isn't worked out yet, so it's kept as it is. ATM, beside it,
//   is a tile type byte per grid cell ((width - 1) x (height - 1)), which all 30 maps match.
#pragma once
#include "core/Sprite.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace wow
{
    struct WofFace
    {
        uint8_t v[3] = {};
        uint8_t material = 0;
        uint8_t uv[3][2] = {};
    };

    struct WofPiece
    {
        std::string name;
        uint8_t flags = 0;
        int16_t pivot[3] = {};
        int accumulated[3] = {}; // pivot summed down from the root piece
        int parent = -1;
        std::vector<int> children;
        std::vector<std::array<int16_t, 3>> vertices;
        std::vector<WofFace> faces;
    };

    struct WofModel
    {
        uint32_t header[12] = {}; // the raw header words (0x00-0x2C)
        std::vector<WofPiece> pieces;
        int textureHeight = 0;    // rows of the 256 wide atlas
        std::vector<uint8_t> texture;
        std::vector<uint8_t> materials;

        // Throws FormatError if it isn't a WOF.
        static WofModel Load(std::span<const uint8_t> data);
    };

    struct IobPatch
    {
        int x = 0, y = 0;            // where the patch goes, from the building's anchor
        uint8_t width = 0, height = 0, colour = 0, mode = 0;
        uint16_t normals[3] = {};
        uint32_t offset = 0;         // of its RLE rows, from 0x22
    };

    struct IobModel
    {
        int views = 0, faceCount = 0, litTriangles = 0;
        bool animated = false;
        int width = 0, height = 0, field12 = 0, field16 = 0;
        std::vector<std::array<int16_t, 3>> points;
        std::vector<std::array<int8_t, 3>> normals;
        std::vector<IobPatch> patches;
        std::vector<uint8_t> patchData; // everything from 0x22, which the patch offsets index

        static IobModel Load(std::span<const uint8_t> data);

        // Draws the pre-rendered building (every patch's rows) into an indexed picture, 0 transparent. The size
        // is the furthest any patch reaches.
        IndexedImage Picture() const;
    };

    struct Terrain
    {
        int width = 0, height = 0;      // vertices
        int triangles = 0;
        uint32_t heightScale = 0;       // world y = height x scale >> 16 (header 0x2C, WLandscape +0x3C)
        int yBase = 0;                  // header 0x18 + 0x30
        std::vector<uint8_t> heights;   // width x height, 0 is water
        uint32_t header[17] = {};       // the raw header words
        std::vector<uint8_t> rest;      // everything after the heights, not worked out yet
        std::vector<uint8_t> tiles;     // ATM: (width - 1) x (height - 1), 1 water, 2 and up land types

        static Terrain Load(std::span<const uint8_t> cls, std::span<const uint8_t> atm = {});
    };
}
