// The texture packs (core/Png, core/TexturePack, IobModel::Repainted): the PNG reader, the colours turned into the
// game's indices, a building's picture painted back into its patches, a unit's atlas replaced.
#include "Test.h"
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
