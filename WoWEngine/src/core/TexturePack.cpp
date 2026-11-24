#include "core/TexturePack.h"

#include <algorithm>
#include <cctype>
#include <functional>

namespace wow
{
    namespace
    {
        std::string Lower(std::string s)
        {
            for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }
        uint32_t Le32(const std::vector<uint8_t>& d, size_t at)
        {
            return uint32_t(d[at]) | uint32_t(d[at + 1]) << 8 | uint32_t(d[at + 2]) << 16 | uint32_t(d[at + 3]) << 24;
        }
    }

    namespace textures
    {
        std::string PaletteTable(int slot)
        {
            switch (slot)
            {
            case 1: return "BMHBM";
            case 2: return "BMHVM";
            case 3: return "BMMVM";
            case 4: return "BMMBM";
            case 10: return "BMR0NAM";
            default: return {};
            }
        }

        std::string Stem(const std::string& gameFile)
        {
            const size_t slash = gameFile.find_last_of("\\/");
            std::string name = slash == std::string::npos ? gameFile : gameFile.substr(slash + 1);
            const size_t dot = name.find_last_of('.');
            return dot == std::string::npos ? name : name.substr(0, dot);
        }

        std::map<int, std::string> Folders(const std::map<int, GType>& gtypes, const std::map<int, ObjOjd::UnitData>& units)
        {
            std::map<int, std::pair<int, bool>> owner;      // gtype -> faction, building
            std::function<void(int, int, bool, int)> claim = [&](int type, int faction, bool building, int depth) {
                auto u = units.find(type);
                if (u == units.end() || depth > 4) return;
                owner.emplace(u->second.field28, std::make_pair(faction, building));
                for (const auto& part : u->second.parts) claim(part.a, faction, building, depth + 1);
            };
            for (const auto& [type, u] : units)
                if (u.faction <= 1) claim(type, u.faction, u.category >= 2, 0);
            std::map<int, std::string> out;
            for (const auto& [id, g] : gtypes)
            {
                const std::string file = Lower(g.file);
                const bool wof = file.size() > 4 && file.compare(file.size() - 4, 4, ".wof") == 0;
                const bool iob = file.size() > 4 && file.compare(file.size() - 4, 4, ".iob") == 0;
                if ((!wof && !iob) || PaletteTable(g.paletteSlot).empty()) continue;
                const std::string stem = Lower(Stem(g.file));
                std::string folder;
                if (stem.rfind("bridge", 0) == 0) folder = "scenery";       // the bridges belong to no one
                else if (auto o = owner.find(id); o != owner.end())
                    folder = std::string(o->second.first == 0 ? "human/" : "martian/") + (o->second.second ? "buildings" : "units");
                else if (g.paletteSlot == 2) folder = "human/units";
                else if (g.paletteSlot == 3) folder = "martian/units";
                else if (g.paletteSlot == 4) folder = "martian/buildings";
                else folder = "scenery";
                out[id] = folder;
            }
            return out;
        }

        std::vector<uint8_t> Indices(const PngImage& png, const uint32_t* palette)
        {
            std::vector<uint8_t> out(png.rgba.size(), 0);
            // an 8-bit picture with the export's palette (entries 1-255 the same colours) is read by its indices
            if (png.paletted && png.palette.size() >= 2)
            {
                bool same = true;
                for (size_t i = 1; i < png.palette.size() && i < 256 && same; i++)
                    same = (png.palette[i] & 0xFFFFFFu) == (palette[i] & 0xFFFFFFu);
                if (same)
                {
                    for (size_t i = 0; i < out.size(); i++)
                    {
                        const uint8_t v = png.indices[i];
                        const bool clear = v == 0 || (size_t(v) < png.palette.size() && (png.palette[v] >> 24) < 128);
                        out[i] = clear ? 0 : v;
                    }
                    return out;
                }
            }
            // otherwise the nearest of entries 1-255, each colour worked out once
            std::map<uint32_t, uint8_t> seen;
            for (size_t i = 0; i < out.size(); i++)
            {
                const uint32_t px = png.rgba[i];
                if ((px >> 24) < 128 || (px & 0xFFFFFFu) == 0xFF00FFu) continue;
                const uint32_t rgb = px & 0xFFFFFFu;
                auto it = seen.find(rgb);
                if (it == seen.end())
                {
                    const int r = int(rgb >> 16), g = int((rgb >> 8) & 0xFF), b = int(rgb & 0xFF);
                    int best = 1, bestD = 1 << 30;
                    for (int k = 1; k < 256; k++)
                    {
                        const int pr = int((palette[k] >> 16) & 0xFF), pg = int((palette[k] >> 8) & 0xFF), pb = int(palette[k] & 0xFF);
                        const int d = (r - pr) * (r - pr) + (g - pg) * (g - pg) + (b - pb) * (b - pb);
                        if (d < bestD) { bestD = d; best = k; }
                    }
                    it = seen.emplace(rgb, uint8_t(best)).first;
                }
                out[i] = it->second;
            }
            return out;
        }

        int WofAtlasHeight(const std::vector<uint8_t>& file)
        {
            if (file.size() < 0x30) return 0;
            const uint32_t texture = Le32(file, 0x28), end = Le32(file, 0x2C);
            if (texture == 0 || end <= texture || end > file.size()) return 0;
            return int((end - texture) / 256);
        }

        bool ReplaceWofAtlas(std::vector<uint8_t>& file, const std::vector<uint8_t>& indices, int width, int height)
        {
            const int h = WofAtlasHeight(file);
            if (h <= 0 || width != 256 || height != h || indices.size() != size_t(256) * size_t(h)) return false;
            std::copy(indices.begin(), indices.end(), file.begin() + std::ptrdiff_t(Le32(file, 0x28)));
            return true;
        }
    }

    bool TexturePack::Open(const std::filesystem::path& folder)
    {
        files.clear();
        root = folder;
        std::error_code ec;
        if (!std::filesystem::is_directory(folder, ec)) return false;
        for (auto it = std::filesystem::recursive_directory_iterator(folder, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
        {
            if (!it->is_regular_file(ec)) continue;
            const std::filesystem::path& p = it->path();
            if (Lower(p.extension().string()) != ".png") continue;
            std::string rel = Lower(std::filesystem::relative(p, folder, ec).replace_extension().generic_string());
            files[rel] = p;
        }
        return !files.empty();
    }

    std::optional<std::filesystem::path> TexturePack::Find(const std::string& folder, const std::string& stem, bool civilOther) const
    {
        const std::string key = Lower((civilOther ? "civilwar/" + folder : folder) + "/" + stem);
        auto it = files.find(key);
        if (it == files.end()) return std::nullopt;
        return it->second;
    }
}
