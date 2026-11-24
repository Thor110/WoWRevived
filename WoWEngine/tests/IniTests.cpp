// WoWRevived.ini (WoWPatches/Common/IniStore.h), shared by the engine, WoWLauncher and the patch DLLs: values kept
// as text, names matched without regard to case, as the registry it stands in for.
#include "Test.h"
#include "IniStore.h"
#include "core/SettingsTable.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

namespace
{
    std::string TempIni(const char* name)
    {
        const fs::path p = fs::temp_directory_path() / name;
        std::error_code ec;
        fs::remove(p, ec);
        return p.string();
    }
}

TEST("ini: set, save, load back")
{
    const std::string path = TempIni("wowtests-a.ini");
    {
        wow::IniStore ini;
        CHECK(!ini.Load(path));                       // not there yet
        ini.Set("Modern", "Menu scale", "200");
        ini.Set("Sound\\Volume", "Master", "0.750000");
        ini.Set("Main", "Full Screen", "1");
        CHECK(ini.Save());
    }
    wow::IniStore back;
    REQUIRE(back.Load(path));
    std::string v;
    CHECK(back.Get("Modern", "Menu scale", v) && v == "200");
    CHECK(back.Get("sound\\volume", "MASTER", v) && v == "0.750000");     // either case
    CHECK(back.Get("Main", "Full Screen", v) && v == "1");
    CHECK(!back.Get("Modern", "Nothing here", v));
    std::error_code ec;
    fs::remove(path, ec);
}

TEST("ini: a value set again replaces the old one; delete takes it out")
{
    const std::string path = TempIni("wowtests-b.ini");
    {
        std::ofstream f(path);
        f << "; comment\n[Modern]\nHUD scale=100\n";
    }
    wow::IniStore ini;
    REQUIRE(ini.Load(path));
    std::string v;
    CHECK(ini.Get("Modern", "HUD scale", v) && v == "100");
    ini.Set("modern", "hud scale", "150");
    CHECK(ini.Get("Modern", "HUD scale", v) && v == "150");
    CHECK(ini.IsDirty());
    CHECK(ini.Delete("Modern", "HUD scale"));
    CHECK(!ini.Get("Modern", "HUD scale", v));
    std::error_code ec;
    fs::remove(path, ec);
}

namespace
{
    std::string ReadAll(const std::string& path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), {});
    }
    void WriteAll(const std::string& path, const std::string& text)
    {
        std::ofstream out(path, std::ios::binary);
        out << text;
    }
}

TEST("ini: a file in the old form is written back as it was")
{
    const std::string path = TempIni("wowtests-old.ini");
    const std::string text =
        "; WoW Revived settings, used instead of the registry key\r\n"
        "; HKLM\\SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000 so the installation is portable.\r\n"
        "; [Main] is that key itself, the other sections are its subkeys. Every value is plain text.\r\n"
        "\r\n[Main]\r\nFull Screen=1\r\n"
        "\r\n[Sound\\Volume]\r\nMaster=0.750000\r\n";
    WriteAll(path, text);
    wow::IniStore ini;
    REQUIRE(ini.Load(path));
    CHECK(ini.Serialize() == text);
    // and a new store's file has the header and a blank line before each section
    wow::IniStore fresh;
    fresh.Set("Main", "Full Screen", "1");
    fresh.Set("Sound\\Volume", "Master", "0.750000");
    CHECK(fresh.Serialize() == text);
}

TEST("ini: comments and blank lines stay; a set value replaces its commented example")
{
    const std::string path = TempIni("wowtests-comments.ini");
    WriteAll(path,
        "; my settings\n"
        "\n[Modern]\n"
        "; The menus' size\n"
        "; Menu scale=100\n"
        "\n"
        "# a note\n"
        "Battle zoom=1\n"
        "\n[Sound]\n"
        "; Search Libraries First=0\n");
    {
        wow::IniStore ini;
        REQUIRE(ini.Load(path));
        ini.Set("Modern", "Menu scale", "150");          // replaces its example, where it was
        ini.Set("Modern", "Battle zoom", "0");           // changed in its own line
        ini.Set("Modern", "Selection keys", "1");        // new: after the last value
        ini.Set("Sound", "Search Libraries First", "1");
        CHECK(ini.Save());
    }
    const std::string back = ReadAll(path);
    CHECK(back ==
        "; my settings\r\n"
        "\r\n[Modern]\r\n"
        "; The menus' size\r\n"
        "Menu scale=150\r\n"
        "\r\n"
        "# a note\r\n"
        "Battle zoom=0\r\n"
        "Selection keys=1\r\n"
        "\r\n[Sound]\r\n"
        "Search Libraries First=1\r\n");
    wow::IniStore again;
    REQUIRE(again.Load(path));
    std::string v;
    CHECK(again.Get("Modern", "Menu scale", v) && v == "150");
    CHECK(!again.Get("Modern", "The menus' size", v));   // a comment isn't a value
}

TEST("ini: documenting adds comments and examples once, and keeps values that are set")
{
    wow::IniStore ini;
    ini.Set("Modern", "Battle zoom", "0");
    CHECK(ini.Document("Modern", { "The engine's options" }, "Battle zoom", "1", { "Zoom on the battle map." }));
    CHECK(ini.Document("Modern", { "The engine's options" }, "Menu scale", "100", { "The menus' size." }));
    CHECK(ini.Document("Sound", {}, "Search Libraries First", "0", { "Libraries first." }));
    const std::string once = ini.Serialize();
    CHECK(!ini.Document("Modern", { "The engine's options" }, "Battle zoom", "1", { "Zoom on the battle map." }));
    CHECK(!ini.Document("Modern", { "The engine's options" }, "Menu scale", "100", { "The menus' size." }));
    CHECK(!ini.Document("Sound", {}, "Search Libraries First", "0", { "Libraries first." }));
    CHECK(ini.Serialize() == once);
    std::string v;
    CHECK(ini.Get("Modern", "Battle zoom", v) && v == "0");
    CHECK(!ini.Get("Modern", "Menu scale", v));       // documented, not set
    CHECK(once.find("[Modern]\r\n; The engine's options\r\n\r\n; Zoom on the battle map.\r\nBattle zoom=0\r\n") != std::string::npos);
    CHECK(once.find("; The menus' size.\r\n; Menu scale=100\r\n") != std::string::npos);
    ini.Set("Modern", "Menu scale", "125");             // and setting it takes its example's place
    CHECK(ini.Serialize().find("; The menus' size.\r\nMenu scale=125\r\n") != std::string::npos);
}

TEST("settings table: readme/WoWRevived.default.ini is the table's (wowdump --default-ini writes it)")
{
    wow::IniStore ini;
    wow::settingstable::Document(ini);
    // (line ends aside: the checkout may have either)
    auto lf = [](std::string t) { t.erase(std::remove(t.begin(), t.end(), '\r'), t.end()); return t; };
    const std::string shipped = ReadAll(std::string(WOWENGINE_SOURCE_DIR) + "/readme/WoWRevived.default.ini");
    CHECK(!shipped.empty());
    CHECK(lf(ini.Serialize()) == lf(shipped));
    // every name once in its section, and each section described
    for (size_t i = 0; i < std::size(wow::settingstable::Settings); i++)
    {
        const auto& a = wow::settingstable::Settings[i];
        bool described = false;
        for (const auto& s : wow::settingstable::Sections) described |= std::string(s.name) == a.section;
        CHECK(described);
        for (size_t j = i + 1; j < std::size(wow::settingstable::Settings); j++)
        {
            const auto& b = wow::settingstable::Settings[j];
            CHECK(!(std::string(a.section) == b.section && std::string(a.name) == b.name));
        }
    }
}

TEST("settings table: documenting an install's ini keeps what's set, sets nothing, and adds nothing twice")
{
    const std::string path = TempIni("wowtests-doc.ini");
    WriteAll(path,
        "; WoW Revived settings, used instead of the registry key\r\n"
        "\r\n[Main]\r\nFull Screen=1\r\n"
        "\r\n[Sound\\Volume]\r\nMaster=0.500000\r\n"
        "\r\n[WoW Engine]\r\nLanguage=Italian\r\n");
    {
        wow::IniStore ini;
        REQUIRE(ini.Load(path));
        CHECK(wow::settingstable::Document(ini));
        CHECK(ini.Save());
    }
    wow::IniStore back;
    REQUIRE(back.Load(path));
    std::string v;
    CHECK(back.Get("Main", "Full Screen", v) && v == "1");             // (the original's, not in the table: kept)
    CHECK(back.Get("Sound\\Volume", "Master", v) && v == "0.500000");
    CHECK(back.Get("WoW Engine", "Language", v) && v == "Italian");
    CHECK(!back.Get("Modern", "Battle zoom", v));                       // documented, not set
    CHECK(!back.Get("Screen", "Size", v));                              // (so the first start still takes the desktop's)
    const std::string once = back.Serialize();
    CHECK(once.find("; The CD music. 1 on, 0 off.\r\n; CD=1\r\n") != std::string::npos);
    CHECK(!wow::settingstable::Document(back));
    CHECK(back.Serialize() == once);
}
