#include "core/CustomCampaign.h"

#include <cctype>
#include <sstream>

namespace wow
{
    std::string CustomCampaign::Stem() const
    {
        std::string s = file;
        if (s.size() > 4)
        {
            std::string tail = s.substr(s.size() - 4);
            for (char& c : tail) c = char(std::tolower(static_cast<unsigned char>(c)));
            if (tail == ".txt") s.erase(s.size() - 4);
        }
        return s;
    }

    namespace
    {
        // a file or folder in the folder by its name, any case
        std::optional<std::filesystem::path> FindIn(const std::filesystem::path& folder, const std::string& name, bool directory)
        {
            std::string want = name;
            for (char& c : want) c = char(std::tolower(static_cast<unsigned char>(c)));
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(folder, ec))
            {
                std::string have = entry.path().filename().string();
                for (char& c : have) c = char(std::tolower(static_cast<unsigned char>(c)));
                if (have == want && entry.is_directory(ec) == directory) return entry.path();
            }
            return std::nullopt;
        }
    }

    std::optional<std::filesystem::path> CustomCampaign::Movie(const char* which) const
    {
        for (const char* ext : { ".mp4", ".smk" })
            if (auto p = FindIn(folder, Stem() + "." + which + ext, false)) return p;
        return std::nullopt;
    }

    std::optional<std::filesystem::path> CustomCampaign::Briefing() const { return FindIn(folder, Stem() + ".briefing.txt", false); }
    std::optional<std::filesystem::path> CustomCampaign::Reskins() const { return FindIn(folder, Stem(), true); }

    uint32_t CustomCampaign::Flags() const
    {
        uint32_t f = CampaignFlags::Custom;
        if (!pods) f |= CampaignFlags::NoPods;
        if (!human) f |= CampaignFlags::Martian;
        return f;
    }

    CustomCampaign CustomCampaign::Parse(std::string_view text, std::string file, std::vector<std::string>& warnings, bool& ok)
    {
        CustomCampaign c;
        c.file = std::move(file);
        c.name = c.file;
        if (c.name.size() > 4 && (c.name.substr(c.name.size() - 4) == ".txt" || c.name.substr(c.name.size() - 4) == ".TXT")) c.name.resize(c.name.size() - 4);
        auto lower = [](std::string t) { for (char& ch : t) ch = char(std::tolower(static_cast<unsigned char>(ch))); return t; };
        auto trim = [](const std::string& t) {
            const size_t a = t.find_first_not_of(" \t\r"), b = t.find_last_not_of(" \t\r");
            return a == std::string::npos ? std::string() : t.substr(a, b - a + 1);
        };
        bool kindFound = false, podsGiven = false, layoutLines = false;
        std::istringstream in{ std::string(text) };
        std::string line;
        int number = 0;
        while (std::getline(in, line))
        {
            number++;
            if (const size_t hash = line.find('#'); hash != std::string::npos) line.erase(hash);
            line = trim(line);
            if (line.empty()) continue;
            const size_t eq = line.find('=');
            if (eq == std::string::npos) continue;      // (the layout's reading notes it)
            const std::string key = lower(trim(line.substr(0, eq)));
            const std::string value = trim(line.substr(eq + 1));
            const std::string where = "line " + std::to_string(number) + ": ";
            if (key == "name") { if (!value.empty()) c.name = value; }
            else if (key == "kind")
            {
                const std::string v = lower(value);
                kindFound = true;
                if (v == "human") { c.kind = Campaign::Regular; c.human = true; }
                else if (v == "martian") { c.kind = Campaign::Regular; c.human = false; }
                else if (v == "human civil war") { c.kind = Campaign::HumanCivilWar; c.human = true; }
                else if (v == "martian civil war") { c.kind = Campaign::MartianCivilWar; c.human = false; }
                else { kindFound = false; warnings.push_back(where + "kind is human, martian, human civil war or martian civil war, not '" + value + "'"); }
            }
            else if (key == "start")
            {
                if (value.find_first_of("\\/:") != std::string::npos || value == "." || value == "..")
                    warnings.push_back(where + "start is a file's name in the Campaigns folder, without a folder");
                else c.start = value;
            }
            else if (key == "pods")
            {
                const std::string v = lower(value);
                if (v == "on" || v == "yes" || v == "1") { c.pods = true; podsGiven = true; }
                else if (v == "off" || v == "no" || v == "0") { c.pods = false; podsGiven = true; }
                else warnings.push_back(where + "pods is on or off");
            }
            else if (key == "player" || key == "other" || key == "neutral" || key == "copy" || key == "copy at") layoutLines = true;
            else if (!IsCivilWar(c.kind)) warnings.push_back(where + "unknown key '" + key + "'");   // (a civil war's layout notes its own)
        }
        ok = kindFound;
        if (!kindFound) { warnings.push_back("no kind: the file isn't offered"); return c; }
        if (IsCivilWar(c.kind))
        {
            // (the pods are the Martians' against the Humans: never in a civil war, WarMap::DropPod)
            if (podsGiven && c.pods) warnings.push_back("a civil war has no pods: pods = on left out");
            c.pods = false;
            c.layout = CivilWarLayout::ParseLines(text, c.kind, { "name", "kind", "start", "pods" }, warnings);
        }
        else if (layoutLines)
            warnings.push_back("the layout's lines (player, other, neutral, copy) are for a civil war: left out");
        return c;
    }
}
