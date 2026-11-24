// WoWRevived.ini (WoWPatches/Common/IniStore.h), shared by the engine, WoWLauncher and the patch DLLs: values kept
// as text, names matched without regard to case, as the registry it stands in for.
#include "Test.h"
#include "IniStore.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
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
