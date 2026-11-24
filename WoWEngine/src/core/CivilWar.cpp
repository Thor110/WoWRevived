#include "core/CivilWar.h"
#include "core/Bytes.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace wow
{
    namespace
    {
        // a save list's side (0 for HUNI / HCON / HRES, 1 for AUNI / ACON / ARES) and the list of a kind for a side
        int ListSide(int list) { return list % 2; }
        int ListFor(int list, int side) { return list - list % 2 + side; }

        // the research blob split into its HRSH and MRSH blocks (each with its tag)
        void SplitResearch(const std::vector<uint8_t>& blob, std::vector<uint8_t>& human, std::vector<uint8_t>& martian)
        {
            if (blob.size() < 8 || std::memcmp(blob.data(), "HRSH", 4) != 0) throw FormatError("civil war: the start file's research doesn't begin with HRSH");
            size_t m = std::string::npos;
            for (size_t i = 4; i + 4 <= blob.size(); i++)
                if (std::memcmp(&blob[i], "MRSH", 4) == 0) { m = i; break; }
            if (m == std::string::npos) throw FormatError("civil war: the start file's research has no MRSH");
            human.assign(blob.begin(), blob.begin() + std::ptrdiff_t(m));
            martian.assign(blob.begin() + std::ptrdiff_t(m), blob.end());
        }

        // an object copied into another sector and side: its place moved by the two sectors' waypoints' difference
        // (unplaced objects, at 0, 0, stay there), new ids
        SaveObject Moved(const SaveObject& o, int sector, int side, int dx, int dy, std::vector<bool>& used)
        {
            SaveObject c = o;
            c.list = ListFor(o.list, side);
            c.SetSector(sector);
            if (o.X() != 0 || o.Y() != 0) c.SetPosition(o.X() + dx, o.Y() + dy);
            c.Renumber(used);
            // the command building as it is anywhere but its own home: London's Command Post (278) is a Command
            // Post (58) elsewhere, the Grampians' Communications Centre (830) one (120) elsewhere
            // (core/Deployment.h, HumanHq / MartianHq; WarMap::HasCommandPost)
            if (sector != 30) c.Retype(278, 58);
            if (sector != 3) c.Retype(830, 120);
            return c;
        }
    }

    CivilWarLayout::To CivilWarLayout::Where(int sector) const
    {
        auto has = [sector](const std::vector<int>& v) { return std::find(v.begin(), v.end(), sector) != v.end(); };
        if (has(player)) return To::Player;
        if (has(neutral)) return To::Neutral;
        if (has(other)) return To::Other;
        return restTo;
    }

    CivilWarLayout CivilWarLayout::Default(Campaign campaign)
    {
        CivilWarLayout l;
        if (campaign == Campaign::MartianCivilWar)
        {
            // the Martians' base the other side's (and any other Martian sector), the player's a copy of it in London
            l.copyFrom = CivilWarNorthHome;
            l.copyX = 1111; l.copyY = 2034;     // London's route waypoint (warmap/WarMapData.h, sector 30)
        }
        else
        {
            // Scotland the player's, the rest London's; the player's base a copy of London's where the Martians' was
            l.player.assign(CivilWarScotland.begin(), CivilWarScotland.end());
            l.copyFrom = CivilWarSouthHome;
            l.copyX = 468; l.copyY = 540;       // the Martians' base in the Grampian Mountains (Human.000)
        }
        return l;
    }

    namespace
    {
        CivilWarLayout ParseLayout(std::string_view text, Campaign campaign, const std::vector<std::string>* skip, std::vector<std::string>& warnings);
    }

    CivilWarLayout CivilWarLayout::Parse(std::string_view text, Campaign campaign, std::vector<std::string>& warnings)
    {
        return ParseLayout(text, campaign, nullptr, warnings);
    }

    CivilWarLayout CivilWarLayout::ParseLines(std::string_view text, Campaign campaign, const std::vector<std::string>& skip, std::vector<std::string>& warnings)
    {
        return ParseLayout(text, campaign, &skip, warnings);
    }

    namespace
    {
    // (with skip, the whole text is the section: a custom campaign's file)
    CivilWarLayout ParseLayout(std::string_view text, Campaign campaign, const std::vector<std::string>* skip, std::vector<std::string>& warnings)
    {
        using To = CivilWarLayout::To;
        CivilWarLayout l = CivilWarLayout::Default(campaign);
        const std::string want = campaign == Campaign::MartianCivilWar ? "martian civil war" : "human civil war";
        auto lower = [](std::string t) { for (char& c : t) c = char(std::tolower(static_cast<unsigned char>(c))); return t; };
        auto trim = [](std::string t) {
            const size_t a = t.find_first_not_of(" \t\r"), b = t.find_last_not_of(" \t\r");
            return a == std::string::npos ? std::string() : t.substr(a, b - a + 1);
        };
        bool inSection = skip != nullptr, found = skip != nullptr;
        // a list given replaces the draft's; "rest" in it sends the sectors no list names there
        bool listed = false;
        std::istringstream in{ std::string(text) };
        std::string line;
        int number = 0;
        while (std::getline(in, line))
        {
            number++;
            if (const size_t hash = line.find('#'); hash != std::string::npos) line.erase(hash);
            line = trim(line);
            if (line.empty()) continue;
            if (line.front() == '[' && !skip)
            {
                inSection = lower(trim(line.substr(1, line.find(']') - 1))) == want;
                found |= inSection;
                continue;
            }
            if (!inSection) continue;
            const size_t eq = line.find('=');
            if (eq == std::string::npos) { warnings.push_back("line " + std::to_string(number) + ": no '='"); continue; }
            const std::string key = lower(trim(line.substr(0, eq)));
            if (skip && std::find(skip->begin(), skip->end(), key) != skip->end()) continue;
            std::istringstream values(lower(line.substr(eq + 1)));
            std::vector<std::string> words;
            for (std::string w; values >> w;) words.push_back(w);
            auto number_ = [&](const std::string& w, int lo, int hi, int& out) {
                try { size_t used = 0; out = std::stoi(w, &used); if (used == w.size() && out >= lo && out <= hi) return true; } catch (const std::exception&) {}
                warnings.push_back("line " + std::to_string(number) + ": '" + w + "' isn't a number from " + std::to_string(lo) + " to " + std::to_string(hi));
                return false;
            };
            if (key == "player" || key == "other" || key == "neutral")
            {
                if (!listed) { l.player.clear(); l.other.clear(); l.neutral.clear(); l.restTo = To::Other; listed = true; }
                std::vector<int>& list = key == "player" ? l.player : key == "other" ? l.other : l.neutral;
                for (const std::string& w : words)
                {
                    if (w == "rest") { l.restTo = key == "player" ? To::Player : key == "other" ? To::Other : To::Neutral; continue; }
                    int sector = 0;
                    if (number_(w, 1, 30, sector)) list.push_back(sector);
                }
            }
            else if (key == "copy")
            {
                int sector = 0;
                if (words.size() == 1 && (words[0] == "none" || words[0] == "0")) l.copyFrom = 0;
                else if (words.size() == 1 && number_(words[0], 1, 30, sector)) l.copyFrom = sector;
                else if (words.size() != 1) warnings.push_back("line " + std::to_string(number) + ": copy takes one sector, or none");
            }
            else if (key == "copy at")
            {
                int x = 0, y = 0;
                if (words.size() == 2 && number_(words[0], 0, 32767, x) && number_(words[1], 0, 32767, y)) { l.copyX = x; l.copyY = y; }
                else if (words.size() != 2) warnings.push_back("line " + std::to_string(number) + ": copy at takes x and y");
            }
            else warnings.push_back("line " + std::to_string(number) + ": unknown key '" + key + "'");
        }
        if (!found) warnings.push_back("no [" + std::string(campaign == Campaign::MartianCivilWar ? "Martian" : "Human") + " Civil War] section: the draft is used");
        return l;
    }
    }

    SaveFile MakeCivilWarStart(const SaveFile& start, Campaign campaign, const CivilWarLayout& layout)
    {
        // the start file is the player's faction's: its H lists (even) the player's faction, its A lists (odd) the
        // other faction, which a civil war doesn't have
        const bool human = campaign == Campaign::HumanCivilWar;
        const int playerCode = OwnerCode(human, 0), otherCode = OwnerCode(human, 1);
        SaveFile s = start;
        s.campaign = campaign;
        s.messages.clear();
        s.pods = 0;
        s.podHours = 0;
        std::vector<uint8_t> humanBlock, martianBlock;
        SplitResearch(start.research, humanBlock, martianBlock);
        const std::vector<uint8_t>& block = human ? humanBlock : martianBlock;
        s.research = block;                                     // the player's side's, then the other's
        s.research.insert(s.research.end(), block.begin(), block.end());
        // the player's home (fixed: see CivilWar.h), and the sector whose objects it is given a copy of
        const int home = human ? CivilWarNorthHome : CivilWarSouthHome;
        const int from = layout.copyFrom;
        using To = CivilWarLayout::To;

        for (int sector = 1; sector <= 30; sector++)
        {
            SaveSector& sec = s.Sector(sector);
            const To to = layout.Where(sector);
            std::array<std::vector<SaveObject>, 6> lists;
            for (auto& list : sec.lists)
                for (SaveObject& o : list)
                {
                    if (ListSide(o.list) == 1 || to == To::Neutral) continue;   // the other faction out
                    o.list = ListFor(o.list, to == To::Player ? 0 : 1);
                    lists[size_t(o.list)].push_back(o);
                }
            sec.lists = std::move(lists);
            const int owner = sec.Owner();
            if (owner == playerCode) sec.SetOwner(to == To::Player ? playerCode : to == To::Other ? otherCode : 2);
            else if (owner == otherCode) sec.SetOwner(2);
        }

        // the player's home: a copy of the other home's objects (the start file's own, the player's faction's),
        // moved by the difference between their middle and the home's place
        if (from < 1 || from > 30)
        {
            s.warMap[2] = uint32_t(home);
            return s;
        }
        std::vector<bool> used = s.UsedIds();
        const SaveSector& source = start.Sector(from);
        int sx = 0, sy = 0, n = 0;
        for (const auto& list : source.lists)
            for (const SaveObject& o : list)
                if (ListSide(o.list) == 0 && (o.X() != 0 || o.Y() != 0)) { sx += o.X(); sy += o.Y(); n++; }
        if (n) { sx /= n; sy /= n; }
        const int tx = layout.copyX, ty = layout.copyY;
        for (const auto& list : source.lists)
            for (const SaveObject& o : list)
            {
                if (ListSide(o.list) != 0) continue;
                SaveObject c = Moved(o, home, 0, tx - sx, ty - sy, used);
                s.Sector(home).lists[size_t(c.list)].push_back(std::move(c));
            }
        s.Sector(home).SetOwner(playerCode);
        s.warMap[2] = uint32_t(home);       // the view starts on the player's home
        return s;
    }
}
