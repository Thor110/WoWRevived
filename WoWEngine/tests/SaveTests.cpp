// The save format (core/SaveFile): a save made here, written, read back and written again, byte for byte. The
// owner's 24 saves are checked the same way by `wowdump --resave`; these need no game files.
#include "Test.h"
#include "core/SaveFile.h"

#include <cstring>
#include <string>
#include <vector>

using namespace wow;

namespace
{
    // a sector's data after its lists: the owner, an empty Damage Layer ("DMGL", width 0, height 0, size 0) and
    // a few bytes of its own
    std::vector<uint8_t> SectorRest(uint32_t owner)
    {
        std::vector<uint8_t> rest(20, 0);
        std::memcpy(&rest[0], &owner, 4);
        std::memcpy(&rest[4], "DMGL", 4);
        rest.insert(rest.end(), { 1, 2, 3, 4 });
        return rest;
    }

    SaveFile MakeSave()
    {
        SaveFile s;
        s.SetName("Test campaign");
        s.warMap[0] = 803; s.warMap[1] = 1805; s.warMap[2] = 30; s.warMap[3] = 1;
        s.hours = 12.5f;
        s.dateTime[2] = 9; s.dateTime[3] = 0; s.dateTime[4] = 8; s.dateTime[5] = 1898;
        s.messages.push_back({ "Game Loaded", 100, 0, 12 });
        s.podHours = 48;
        s.pods = 2;
        s.sector0.rest = SectorRest(2);
        for (int i = 1; i <= 30; i++) s.Sector(i).rest = SectorRest(i == 3 ? 1 : i % 3 == 0 ? 2 : 0);
        return s;
    }
}

TEST("save: written, read and written again, the same bytes")
{
    const SaveFile s = MakeSave();
    const std::vector<uint8_t> first = s.Write();
    const SaveFile back = SaveFile::Parse(first);
    CHECK_EQ(back.Name(), std::string("Test campaign"));
    CHECK_EQ(back.warMap[2], 30u);
    CHECK_EQ(back.hours, 12.5f);
    CHECK_EQ(back.dateTime[5], uint16_t(1898));
    REQUIRE(back.messages.size() == 1);
    CHECK_EQ(back.messages[0].text, std::string("Game Loaded"));
    CHECK_EQ(back.pods, 2u);
    CHECK_EQ(back.Sector(3).Owner(), 1);
    CHECK_EQ(back.Sector(1).Owner(), 0);
    CHECK(back.Write() == first);
}

TEST("save: new units and buildings in the sectors' lists, read back the same")
{
    SaveFile s = MakeSave();
    s.Sector(30).lists[0].push_back(SaveObject::MakeUnit(12, 0, 30, 3, 100, false));
    s.Sector(30).lists[0].push_back(SaveObject::MakeUnit(30, 0, 30, 1, 80, true));
    // the lists: Human units, Martian units, Human constructions, Martian constructions, Human resources, Martian
    // resources (SaveFile.cpp, ListTags)
    s.Sector(5).lists[2].push_back(SaveObject::MakeBuilding(47, 2, 5, 200, false, SaveObject::BuildingClass::Plain));
    const std::vector<uint8_t> first = s.Write();
    const SaveFile back = SaveFile::Parse(first);
    REQUIRE(back.Sector(30).lists[0].size() == 2);
    CHECK_EQ(back.Sector(30).lists[0][0].Type(), 12);
    CHECK_EQ(back.Sector(30).lists[0][1].Type(), 30);
    CHECK_EQ(back.Sector(30).lists[0][0].BattleUnitCount(), size_t(3));
    REQUIRE(back.Sector(5).lists[2].size() == 1);
    CHECK_EQ(back.Sector(5).lists[2][0].Type(), 47);
    CHECK(back.Write() == first);
}

TEST("save: a sector's Damage Layer packed and read back")
{
    SaveFile s = MakeSave();
    std::vector<uint8_t> grid(64 * 48);
    for (size_t i = 0; i < grid.size(); i++) grid[i] = uint8_t((i / 64 + i % 64) % 5);
    s.Sector(7).SetDamage(64, 48, grid);
    int w = 0, h = 0;
    std::vector<uint8_t> got;
    REQUIRE(s.Sector(7).Damage(w, h, got));
    CHECK_EQ(w, 64);
    CHECK_EQ(h, 48);
    CHECK(got == grid);
    const SaveFile back = SaveFile::Parse(s.Write());
    got.clear();
    REQUIRE(back.Sector(7).Damage(w, h, got));
    CHECK(got == grid);
    CHECK(!back.Sector(8).Damage(w, h, got));       // an empty layer
}

TEST("save: object ids, the lowest free one first")
{
    SaveFile s = MakeSave();
    std::vector<bool> used = s.UsedIds();
    CHECK_EQ(used.size(), size_t(32768));
    const int a = SaveFile::TakeId(used), b = SaveFile::TakeId(used);
    CHECK(a >= 0 && b == a + 1);
}

TEST("save: the name is 36 bytes at most")
{
    SaveFile s = MakeSave();
    s.SetName(std::string(50, 'x'));
    CHECK_EQ(s.Name().size(), size_t(36));
}

TEST("save: data that isn't a save throws")
{
    const std::vector<uint8_t> junk = { 5, 0, 0, 0, 'N', 'O', 'P', 'E' };
    CHECK_THROWS(SaveFile::Parse(junk));
}
