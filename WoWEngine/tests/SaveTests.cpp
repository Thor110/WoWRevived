// The save format (core/SaveFile): a save made here, written, read back and written again, byte for byte. The
// owner's 24 saves are checked the same way by `wowdump --resave`; these need no game files.
#include "Test.h"
#include "core/SaveFile.h"
#include "core/SaveGame.h"
#include "core/CivilWar.h"

#include <filesystem>
#include <fstream>
#include <optional>

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

TEST("save: a civil war's save carries the engine's tag and reads back as one")
{
    SaveFile s = MakeSave();
    s.campaign = Campaign::HumanCivilWar;
    const std::vector<uint8_t> bytes = s.Write();
    REQUIRE(bytes.size() > EngineSaveHeader);
    CHECK(std::memcmp(bytes.data(), EngineSaveTag, 4) == 0);
    const SaveFile back = SaveFile::Parse(bytes);
    CHECK(back.campaign == Campaign::HumanCivilWar);
    CHECK_EQ(back.Name(), std::string("Test campaign"));
    CHECK(back.Write() == bytes);
    // the original's layout follows the tag unchanged: the same save as a regular one, but for the 12 bytes
    SaveFile regular = back;
    regular.campaign = Campaign::Regular;
    const std::vector<uint8_t> plain = regular.Write();
    CHECK(std::vector<uint8_t>(bytes.begin() + EngineSaveHeader, bytes.end()) == plain);
    CHECK(SaveFile::Parse(plain).campaign == Campaign::Regular);
}

TEST("save: an engine save of an unknown version or campaign throws")
{
    SaveFile s = MakeSave();
    s.campaign = Campaign::MartianCivilWar;
    std::vector<uint8_t> bytes = s.Write();
    bytes[4] = 9;                         // the tag's version
    CHECK_THROWS(SaveFile::Parse(bytes));
    bytes = s.Write();
    bytes[8] = 7;                         // the campaign
    CHECK_THROWS(SaveFile::Parse(bytes));
}

TEST("save slots: civil war saves have their own names, and their names read past the tag")
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "wowtests-slots";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "SaveGame");
    CHECK_EQ(SaveSlot::GamePath(true, 1), std::string("SaveGame\\Human.001"));
    CHECK_EQ(SaveSlot::GamePath(true, 1, true), std::string("SaveGame\\HumanCW.001"));
    CHECK_EQ(SaveSlot::GamePath(false, 12, true), std::string("SaveGame\\MartianCW.012"));
    auto write = [&](const char* name, const SaveFile& s) {
        const std::vector<uint8_t> b = s.Write();
        std::ofstream(root / "SaveGame" / name, std::ios::binary).write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
    };
    SaveFile regular = MakeSave();
    regular.SetName("Regular");
    SaveFile civil = MakeSave();
    civil.SetName("Rebels of the North");
    civil.campaign = Campaign::HumanCivilWar;
    write("Human.001", regular);
    write("HumanCW.001", civil);
    const GameFiles files(root);
    CHECK(SaveSlot::Name(files, true, 1) == std::optional<std::string>("Regular"));
    CHECK(SaveSlot::Name(files, true, 1, true) == std::optional<std::string>("Rebels of the North"));
    CHECK(!SaveSlot::Exists(files, true, 2, true));
    CHECK(!SaveSlot::Exists(files, false, 1, true));
    std::vector<uint8_t> data(fs::file_size(root / "SaveGame" / "HumanCW.001"));
    std::ifstream(root / "SaveGame" / "HumanCW.001", std::ios::binary).read(reinterpret_cast<char*>(data.data()), std::streamsize(data.size()));
    CHECK_EQ(SaveInfo::Read(data).name, std::string("Rebels of the North"));
    fs::remove_all(root, ec);
}

namespace
{
    // a start file like Human.000 (human) or Martian.000 in miniature. Its H lists (even) are the player's
    // faction: Human.000's Humans in Edinburgh (8), Liverpool (15) and London (30), Martian.000's Martians in the
    // Grampian Mountains (3); the other faction in the A lists. Owners are faction codes. A research blob of an
    // HRSH and an MRSH block.
    SaveFile MiniStart(bool human)
    {
        SaveFile s = MakeSave();
        s.research = { 'H', 'R', 'S', 'H', 1, 1, 1, 1, 'M', 'R', 'S', 'H', 2, 2 };
        auto unit = [](int type, int list, int sector, int x, int y) { SaveObject o = SaveObject::MakeUnit(type, list, sector, 1, 100, false); o.SetPosition(x, y); return o; };
        auto building = [](int type, int list, int sector, int x, int y) { SaveObject o = SaveObject::MakeBuilding(type, list, sector, 100, false, SaveObject::BuildingClass::Plain); o.SetPosition(x, y); return o; };
        std::vector<bool> used = s.UsedIds();
        auto add = [&](int sector, SaveObject o) { o.Renumber(used); s.Sector(sector).lists[size_t(o.list)].push_back(std::move(o)); };
        const int h = human ? 0 : 1, m = human ? 1 : 0;   // the Humans' and the Martians' lists' side
        add(8, unit(12, h, 8, 700, 800));
        add(15, unit(12, h, 15, 500, 1300));
        add(30, building(58, 2 + h, 30, 1100, 2030));     // London's HQ
        add(30, unit(13, h, 30, 1120, 2040));
        add(3, unit(89, m, 3, 468, 540));                  // the Martians
        add(3, building(96, 2 + m, 3, 468, 540));
        for (int i = 1; i <= 30; i++) s.Sector(i).SetOwner(2);
        for (int i : { 8, 15, 30 }) s.Sector(i).SetOwner(0);
        s.Sector(3).SetOwner(1);
        s.pods = 3;
        return s;
    }
    size_t Count(const SaveFile& s, int sector, int list) { return s.Sector(sector).lists[size_t(list)].size(); }
}

TEST("civil war start: the Human Civil War from Human.000")
{
    const SaveFile s = MakeCivilWarStart(MiniStart(true), Campaign::HumanCivilWar);
    CHECK(s.campaign == Campaign::HumanCivilWar);
    CHECK_EQ(s.pods, 0u);
    CHECK(s.messages.empty());
    // Scotland's Human objects stay the player's (H lists), England's go to the other side (A lists)
    CHECK_EQ(Count(s, 8, 0), size_t(1));
    CHECK_EQ(Count(s, 15, 0), size_t(0));
    CHECK_EQ(Count(s, 15, 1), size_t(1));
    CHECK_EQ(Count(s, 30, 3), size_t(1));       // London's HQ, now in the other side's constructions
    CHECK_EQ(s.Sector(8).Owner(), 0);           // the player's code (Human)
    CHECK_EQ(s.Sector(15).Owner(), 1);          // the other side's
    CHECK_EQ(s.Sector(30).Owner(), 1);
    // the Martians gone; the player's home in the Grampians a copy of London's, with new ids
    CHECK_EQ(Count(s, 3, 1) + Count(s, 3, 3), size_t(0));
    REQUIRE(Count(s, 3, 2) == 1 && Count(s, 3, 0) == 1);
    CHECK_EQ(s.Sector(3).lists[2][0].Type(), 58);
    CHECK_EQ(s.Sector(3).lists[2][0].Sector(), 3);
    CHECK(s.Sector(3).lists[2][0].Id() != s.Sector(30).lists[3][0].Id());
    CHECK_EQ(s.Sector(3).Owner(), 0);
    CHECK_EQ(s.warMap[2], 3u);
    const std::vector<uint8_t> want = { 'H', 'R', 'S', 'H', 1, 1, 1, 1, 'H', 'R', 'S', 'H', 1, 1, 1, 1 };
    CHECK(s.research == want);
    const std::vector<uint8_t> bytes = s.Write();
    CHECK(SaveFile::Parse(bytes).Write() == bytes);
}

TEST("civil war start: the Martian Civil War from Martian.000")
{
    const SaveFile s = MakeCivilWarStart(MiniStart(false), Campaign::MartianCivilWar);
    // the Humans (the A lists in Martian.000) gone everywhere
    for (int sector = 1; sector <= 30; sector++)
        for (int list : { 0, 2, 4, 1, 3, 5 })
            if (sector != 3 && sector != 30) CHECK_EQ(Count(s, sector, list), size_t(0));
    // the base in the north the other side's (A lists), a copy of it the player's in London (H lists)
    CHECK_EQ(Count(s, 3, 1), size_t(1));
    CHECK_EQ(Count(s, 3, 3), size_t(1));
    CHECK_EQ(Count(s, 3, 0) + Count(s, 3, 2), size_t(0));
    CHECK_EQ(s.Sector(3).Owner(), 0);           // the other side's code (the player is Martian, 1)
    REQUIRE(Count(s, 30, 0) == 1 && Count(s, 30, 2) == 1);
    CHECK_EQ(s.Sector(30).lists[0][0].Type(), 89);
    CHECK_EQ(s.Sector(30).Owner(), 1);
    CHECK_EQ(s.Sector(15).Owner(), 2);
    const std::vector<uint8_t> want = { 'M', 'R', 'S', 'H', 2, 2, 'M', 'R', 'S', 'H', 2, 2 };
    CHECK(s.research == want);
    const std::vector<uint8_t> bytes = s.Write();
    CHECK(SaveFile::Parse(bytes).Write() == bytes);
}

TEST("civil war start: the copied HQ takes its new sector's type")
{
    SaveFile start = MiniStart(true);
    // London's HQ as Human.000 has it: the London Command Post, 278
    start.Sector(30).lists[2][0].Retype(58, 278);
    REQUIRE(start.Sector(30).lists[2][0].Type() == 278);
    const SaveFile s = MakeCivilWarStart(start, Campaign::HumanCivilWar);
    REQUIRE(Count(s, 3, 2) == 1);
    CHECK_EQ(s.Sector(3).lists[2][0].Type(), 58);           // a Command Post in the Grampians
    CHECK_EQ(s.Sector(3).lists[2][0].BattleType(), 58);
    CHECK_EQ(s.Sector(30).lists[3][0].Type(), 278);         // London's stays London's
}
