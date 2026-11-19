#include "core/GameFiles.h"
#include "core/Archive.h"

#include <system_error>

namespace fs = std::filesystem;

namespace wow
{
    const std::vector<std::string>& GameFiles::RequiredFiles()
    {
        static const std::vector<std::string> files = {
            "DAT\\Dat.wow",
            "MAPS\\MAPS.WoW",
            "AI.ojd",
            "OBJ.ojd",
            "SFX.ojd",
            "TEXT.ojd",
        };
        return files;
    }

    std::optional<fs::path> GameFiles::Resolve(std::string_view gamePath) const
    {
        fs::path current = root;
        size_t start = 0;
        while (start <= gamePath.size())
        {
            size_t end = gamePath.find_first_of("\\/", start);
            if (end == std::string_view::npos) end = gamePath.size();
            std::string_view part = gamePath.substr(start, end - start);
            start = end + 1;
            if (part.empty() || part == ".") continue;
            if (part == "..") return std::nullopt; // game paths never leave the install folder

            std::error_code ec;
            fs::path exact = current / fs::path(std::string(part));
            if (fs::exists(exact, ec)) { current = exact; continue; }

            bool found = false;
            for (fs::directory_iterator it(current, ec), endIt; !ec && it != endIt; it.increment(ec))
            {
                if (EqualsNoCase(it->path().filename().string(), part))
                {
                    current = it->path();
                    found = true;
                    break;
                }
            }
            if (!found) return std::nullopt;
        }
        return current;
    }

    std::vector<std::string> GameFiles::Missing() const
    {
        std::vector<std::string> missing;
        for (const std::string& file : RequiredFiles())
        {
            auto path = Resolve(file);
            std::error_code ec;
            if (!path || !fs::is_regular_file(*path, ec)) missing.push_back(file);
        }
        return missing;
    }

    std::optional<GameFiles> GameFiles::Locate(const std::vector<fs::path>& candidates)
    {
        for (const fs::path& candidate : candidates)
        {
            if (candidate.empty()) continue;
            GameFiles files(candidate);
            if (files.IsComplete()) return files;
        }
        return std::nullopt;
    }
}
