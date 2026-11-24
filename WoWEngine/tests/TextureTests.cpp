// The texture packs (core/Png, core/TexturePack, IobModel::Repainted): the PNG reader, the colours turned into the
// game's indices, a building's picture painted back into its patches, a unit's atlas replaced.
#include "Test.h"
#include <algorithm>
#include <cstring>
#include "core/WofMesh.h"
#include "core/Models.h"
#include "core/Bytes.h"
#include "core/Png.h"
#include "core/TexturePack.h"

#include <filesystem>
#include <fstream>
#include <vector>

using namespace wow;

namespace
{
    // a 3 x 2 RGBA picture (red, green, blue / magenta, see-through, (200, 100, 50)), as PIL writes it
    const std::vector<uint8_t> RgbaPng = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x03,
        0x00, 0x00, 0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x9D, 0x74, 0x66, 0x1A, 0x00, 0x00, 0x00, 0x21, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x9C, 0x05, 0xC1, 0x31, 0x01, 0x00, 0x20, 0x10, 0x00, 0xA1, 0xFB, 0xD9, 0xD5, 0xDD, 0x4A, 0x26, 0xB6, 0x1A,
        0x42, 0x4A, 0x52, 0x8C, 0x58, 0xFB, 0xCC, 0xBB, 0xDB, 0x07, 0x97, 0x48, 0x0B, 0x59, 0xC3, 0x26, 0xBB, 0x39, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82 };
    // a 2 x 2 two-bit palette picture, indices 0 1 / 2 3, palette magenta, (1,2,3), (4,5,6), (7,8,9)
    const std::vector<uint8_t> PalettePng = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x02, 0x02, 0x03, 0x00, 0x00, 0x00, 0x0F, 0xD8, 0xE5, 0xB7, 0x00, 0x00, 0x00, 0x0C, 0x50, 0x4C, 0x54,
        0x45, 0xFF, 0x00, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x44, 0x51, 0x2E, 0xD7, 0x00, 0x00, 0x00,
        0x0C, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x10, 0x60, 0xD8, 0x00, 0x00, 0x00, 0xE4, 0x00, 0xC1, 0x27, 0xA8, 0xE8,
        0x57, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82 };

    // a palette with entries 1-3 the PalettePng's colours and the rest far grey
    std::vector<uint32_t> TestPalette()
    {
        std::vector<uint32_t> p(256, 0x808080);
        p[0] = 0xFF00FF;
        p[1] = 0x010203; p[2] = 0x040506; p[3] = 0x070809;
        p[10] = 0xFF0000; p[11] = 0x00FF00; p[12] = 0x0000FF; p[13] = 0xC86432;
        return p;
    }
}

TEST("png: an RGBA picture read")
{
    const PngImage img = DecodePng(RgbaPng);
    CHECK_EQ(img.width, 3);
    CHECK_EQ(img.height, 2);
    CHECK(!img.paletted);
    CHECK_EQ(img.rgba[0], 0xFFFF0000u);
    CHECK_EQ(img.rgba[2], 0xFF0000FFu);
    CHECK_EQ(img.rgba[4] >> 24, 0u);
    CHECK_EQ(img.rgba[5], 0xFFC86432u);
}

TEST("png: a palette picture keeps its indices")
{
    const PngImage img = DecodePng(PalettePng);
    CHECK(img.paletted);
    const std::vector<uint8_t> want = { 0, 1, 2, 3 };
    CHECK(img.indices == want);
    CHECK_EQ(img.rgba[3], 0xFF070809u);
}

TEST("png: what isn't a PNG throws")
{
    bool threw = false;
    try { DecodePng(std::vector<uint8_t>{ 1, 2, 3 }); } catch (const FormatError&) { threw = true; }
    CHECK(threw);
}

TEST("texture pack: colours to the game's indices")
{
    const std::vector<uint32_t> palette = TestPalette();
    // the export's palette: read by index, entry 0 see-through
    const std::vector<uint8_t> exact = textures::Indices(DecodePng(PalettePng), palette.data());
    const std::vector<uint8_t> want = { 0, 1, 2, 3 };
    CHECK(exact == want);
    // full colour: the nearest; magenta and alpha under 128 see-through
    const std::vector<uint8_t> near = textures::Indices(DecodePng(RgbaPng), palette.data());
    const std::vector<uint8_t> want2 = { 10, 11, 12, 0, 0, 13 };
    CHECK(near == want2);
}

TEST("texture pack: a building's picture painted back into its patches")
{
    // two patches: one of two rows (a literal row of 3 and a run of 2), one sharing nothing
    IobModel m;
    m.patches.push_back({ 0, 0, 3, 2, 0, 0, {}, 0 });
    m.patches.push_back({ 4, 1, 1, 1, 0, 0, {}, 8 });
    m.patchData = { 0, 3, 5, 6, 7,   1, 0x82, 9,   0, 1, 4, 0, 0 };
    const IndexedImage before = m.Picture();
    CHECK_EQ(before.width, 5);
    CHECK_EQ(before.height, 2);
    // painted with itself: the same picture
    const IobModel same = m.Repainted(before);
    const IndexedImage again = same.Picture();
    CHECK(again.pixels == before.pixels);
    // a pixel of the run changed, and a hole that keeps the old
    IndexedImage edit = before;
    edit.pixels[size_t(1) * 5 + 2] = 42;
    edit.pixels[0] = 0;
    const IndexedImage after = m.Repainted(edit).Picture();
    CHECK_EQ(after.pixels[size_t(1) * 5 + 2], uint8_t(42));
    CHECK_EQ(after.pixels[size_t(1) * 5 + 1], uint8_t(9));
    CHECK_EQ(after.pixels[0], uint8_t(5));
    CHECK_EQ(after.pixels[size_t(1) * 5 + 4], uint8_t(4));
}

TEST("texture pack: a unit's atlas replaced only at its size")
{
    std::vector<uint8_t> file(0x30 + 512, 7);
    auto put = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) file[at + size_t(i)] = uint8_t(v >> (8 * i)); };
    put(0x28, 0x30);
    put(0x2C, 0x30 + 512);
    CHECK_EQ(textures::WofAtlasHeight(file), 2);
    std::vector<uint8_t> atlas(512, 3);
    CHECK(!textures::ReplaceWofAtlas(file, std::vector<uint8_t>(256, 1), 256, 1));
    CHECK(textures::ReplaceWofAtlas(file, atlas, 256, 2));
    CHECK_EQ(file[0x30], uint8_t(3));
    CHECK_EQ(file[0x30 + 511], uint8_t(3));
    CHECK_EQ(file[0x2F], uint8_t(0));
}

TEST("texture pack: folders by the unit table, then the slot")
{
    std::map<int, GType> g;
    g[100] = { 2, 2, "dat\\tank.WOF" };
    g[101] = { 1, 4, "dat\\Base.iob" };
    g[102] = { 1, 1, "dat\\House1.iob" };
    g[103] = { 2, 2, "dat\\bridge_1.WOF" };
    g[104] = { 1, 1, "dat\\Factory.iob" };
    g[105] = { 0, 5, "dat\\cursor.spr" };
    std::map<int, ObjOjd::UnitData> u;
    ObjOjd::UnitData f; f.faction = 0; f.category = 2; f.field28 = 104;
    u[50] = f;
    const auto folders = textures::Folders(g, u);
    CHECK_EQ(folders.at(100), std::string("human/units"));
    CHECK_EQ(folders.at(101), std::string("martian/buildings"));
    CHECK_EQ(folders.at(102), std::string("scenery"));
    CHECK_EQ(folders.at(103), std::string("scenery"));
    CHECK_EQ(folders.at(104), std::string("human/buildings"));
    CHECK(folders.find(105) == folders.end());
}

TEST("texture pack: files found by folder and name, any case, the civil war's apart")
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root = fs::temp_directory_path(ec) / "wowtests-textures";
    fs::remove_all(root, ec);
    fs::create_directories(root / "Human" / "Units", ec);
    fs::create_directories(root / "civilwar" / "human" / "units", ec);
    std::ofstream(root / "Human" / "Units" / "AR_LOR_1.PNG") << "x";
    std::ofstream(root / "civilwar" / "human" / "units" / "ar_lor_1.png") << "x";
    std::ofstream(root / "Human" / "Units" / "notes.txt") << "x";
    TexturePack pack;
    REQUIRE(pack.Open(root));
    CHECK_EQ(pack.Count(), size_t(2));
    auto mine = pack.Find("human/units", "ar_lor_1", false);
    auto theirs = pack.Find("human/units", "AR_LOR_1", true);
    REQUIRE(mine && theirs);
    CHECK(*mine != *theirs);
    CHECK(!pack.Find("human/units", "ar_lor_2", false));
    CHECK(!pack.Find("martian/units", "ar_lor_1", false));
    fs::remove_all(root, ec);
}

TEST("texture pack: a picture 2, 4 or 8 times the size made the game's")
{
    // 4 x 2 at twice 2 x 1: the left block red and one see-through pixel, the right block mostly see-through
    PngImage big;
    big.width = 4;
    big.height = 2;
    big.rgba = { 0xFFFF0000u, 0xFFFF0000u, 0x00000000u, 0xFF00FF00u,
                 0xFFFF0000u, 0x00123456u, 0xFFFF00FFu, 0x00000000u };
    auto small = textures::FitTo(big, 2, 1);
    REQUIRE(small.has_value());
    CHECK_EQ(small->width, 2);
    CHECK_EQ(small->rgba[0], 0xFFFF0000u);
    CHECK_EQ(small->rgba[1] >> 24, 0u);         // three of four see-through (magenta counts)
    CHECK(textures::FitTo(big, 4, 2).has_value());              // its own size
    CHECK(!textures::FitTo(big, 3, 1).has_value());             // not a multiple
    PngImage odd = big;
    odd.width = 6; odd.height = 2; odd.rgba.assign(12, 0xFF000000u);
    CHECK(!textures::FitTo(odd, 2, 1).has_value());             // 3 x 2: not the same both ways
    PngImage three = big;
    three.width = 6; three.height = 3; three.rgba.assign(18, 0xFF000000u);
    CHECK(!textures::FitTo(three, 2, 1).has_value());           // 3 times: not a power of two
}

TEST("texture pack: a colour the table repeats keeps the game's index")
{
    std::vector<uint32_t> palette = TestPalette();
    palette[20] = 0xFF0000;                     // red twice: 10 and 20
    PngImage img;
    img.width = 2; img.height = 1;
    img.rgba = { 0xFFFF0000u, 0xFFFF0000u };
    const std::vector<uint8_t> game = { 20, 11 };
    const std::vector<uint8_t> got = textures::Indices(img, palette.data(), &game);
    CHECK_EQ(got[0], uint8_t(20));              // the game's own red
    CHECK_EQ(got[1], uint8_t(10));              // painted red over green: the first red
}

namespace
{
    // a WOF of one piece "body" (pivot 10, 20, 30), three points, one triangle in material 0, a 256 x 2 atlas, and
    // the smooth pieces' table (the piece not smooth)
    std::vector<uint8_t> TinyWof()
    {
        std::vector<uint8_t> w(0x30 + 0x61, 0);
        auto put32 = [&](size_t o, uint32_t v) { for (int i = 0; i < 4; i++) w[o + size_t(i)] = uint8_t(v >> (8 * i)); };
        auto put16 = [&](size_t o, int v) { w[o] = uint8_t(v); w[o + 1] = uint8_t(unsigned(v) >> 8); };
        put16(0, 1);
        const size_t r = 0x30;
        std::memcpy(&w[r], "body", 4);
        w[r + 0x11] = 3; w[r + 0x12] = 1;
        put16(r + 0x13, 10); put16(r + 0x15, 20); put16(r + 0x17, 30);
        for (int c = 0; c < 16; c++) put32(r + 0x21 + size_t(c) * 4, 0xFFFFFFFFu);
        const size_t base = w.size();
        put32(0x0C, uint32_t(base));
        put32(r + 0x19, 0);
        for (int v : { 0, 0, 0, 100, 0, 0, 0, 50, 0 }) { w.push_back(uint8_t(v)); w.push_back(uint8_t(unsigned(v) >> 8)); }
        put32(r + 0x1D, uint32_t(w.size()));
        for (int b : { 0, 1, 2, 0, 1, 1, 200, 1, 1, 0 }) w.push_back(uint8_t(b));
        put32(0x24, uint32_t(w.size()));
        for (int i = 0; i < 4; i++) w.push_back(0);             // material 0 at the atlas' start
        put32(0x28, uint32_t(w.size()));
        w.resize(w.size() + 512, 7);                          // the atlas
        put32(0x2C, uint32_t(w.size()));
        for (int i = 0; i < 4; i++) w.push_back(0);
        return w;
    }
}

TEST("models: a WOF to OBJ and back is the same model")
{
    const std::vector<uint8_t> wof = TinyWof();
    std::string obj, mtl;
    REQUIRE(wow::ExportObj(wof, "tiny.mtl", "tiny.png", obj, mtl));
    CHECK(obj.find("o body") != std::string::npos);
    CHECK(obj.find("v 110 -20 30") != std::string::npos);          // the root's pivot, y turned round
    CHECK(mtl.find("map_Kd tiny.png") != std::string::npos);
    std::vector<std::string> warnings;
    const auto pieces = wow::ParseObj(obj, warnings);
    CHECK(warnings.empty());
    std::vector<uint8_t> back = wof;
    CHECK(wow::ApplyMeshes(back, pieces, warnings));
    const auto a = wow::WofModel::Load(wof), b = wow::WofModel::Load(back);
    REQUIRE(b.pieces.size() == size_t(1));
    CHECK(a.pieces[0].vertices == b.pieces[0].vertices);
    REQUIRE(b.pieces[0].faces.size() == size_t(1));
    CHECK(std::memcmp(&a.pieces[0].faces[0], &b.pieces[0].faces[0], sizeof(wow::WofFace)) == 0);
    CHECK(std::equal(wof.begin(), wof.end(), back.begin()) == false);  // (the records point at the new data)
    CHECK(std::equal(wof.begin() + 0x30 + 0x61, wof.end(), back.begin() + 0x30 + 0x61));   // everything else kept
}

TEST("models: an OBJ quad is two triangles; texture places; too many points or an unknown piece say so")
{
    std::vector<std::string> warnings;
    const std::string quad =
        "o BODY\nv 10 -20 30\nv 20 -20 30\nv 20 -10 30\nv 10 -10 30\n"
        "vt 0 1\nvt 0.5 1\nvt 0.5 0\nvt 0 0\nusemtl material_0\nf 1/1 2/2 3/3 4/4\n";
    auto pieces = wow::ParseObj(quad, warnings);
    REQUIRE(pieces.size() == size_t(1));
    CHECK(pieces[0].triangles.size() == size_t(2));
    std::vector<uint8_t> w = TinyWof();
    CHECK(wow::ApplyMeshes(w, pieces, warnings));
    const auto m = wow::WofModel::Load(w);
    REQUIRE(m.pieces[0].faces.size() == size_t(2));
    CHECK(m.pieces[0].vertices[1] == (std::array<int16_t, 3>{ 10, 0, 0 }));   // less the piece's place
    CHECK(int(m.pieces[0].faces[0].uv[1][0]) == 128);                       // u 0.5 across 256
    CHECK(int(m.pieces[0].faces[0].uv[2][1]) == 1);                         // v 0: the atlas' last row
    std::string many = "o body\n";
    for (int i = 0; i < 300; i++) many += "v " + std::to_string(i) + " 0 0\n";
    many += "f 1 2 3\n";
    warnings.clear();
    std::vector<uint8_t> w2 = TinyWof();
    CHECK(!wow::ApplyMeshes(w2, wow::ParseObj(many, warnings), warnings));
    CHECK(!warnings.empty());
    warnings.clear();
    CHECK(!wow::ApplyMeshes(w2, wow::ParseObj("o turret\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n", warnings), warnings));
    CHECK(warnings.size() == size_t(1));
}
