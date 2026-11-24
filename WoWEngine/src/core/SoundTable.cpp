#include "core/SoundTable.h"
#include "core/Bytes.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace wow
{
    namespace
    {
        constexpr int DefaultRate = 0x5622; // 22050
    }

    const SoundDef* SoundTable::Find(int id) const
    {
        auto it = sounds.find(id);
        return it == sounds.end() ? nullptr : &it->second;
    }

    const char* SoundTable::TypeName(int type)
    {
        static const char* const Names[] = { "Ambient", "Engine", "Explosions", "Weapons", "Special", "Speech" };
        return type >= 0 && type < 6 ? Names[type] : "?";
    }

    SoundTable SoundTable::Load(std::span<const uint8_t> data, bool human)
    {
        SoundTable table;
        size_t pos = 0;
        auto need = [&](size_t n) {
            if (pos + n > data.size()) throw FormatError("SFX.ojd: record runs past the end of the file");
        };
        auto u16 = [&]() { need(2); uint16_t v = uint16_t(data[pos] | (data[pos + 1] << 8)); pos += 2; return v; };
        auto s16 = [&]() { return int(int16_t(u16())); };
        auto f32 = [&]() { need(4); float f; std::memcpy(&f, &data[pos], 4); pos += 4; return f; };

        // the loader's running state (its locals)
        int type = -1;
        float volume = 1.0f;
        int priority = 0;
        int rate = DefaultRate;
        int group = 0;
        SoundDef* current = nullptr;
        auto entry = [&]() -> SoundDef& {
            if (!current) throw FormatError("SFX.ojd: a sound's details come before any sound id");
            return *current;
        };

        while (pos < data.size())
        {
            need(5);
            pos += 3; // 0xFF and the line number
            int op = s16();
            switch (op)
            {
            case 0:
                type = s16();
                volume = 1.0f;
                // 0x463DAB: a type over 5 jumps past the priority's store, keeping the last one
                if (type >= 0 && type <= 5) priority = type == SoundDef::Speech ? 0x80 : 0xC0;
                break;
            case 1:
            {
                int id = s16();
                rate = DefaultRate;
                group = 0;
                // 0x463DE1: an id met again keeps its entry (its loop and other flags, +0x5C, +0x64 and its samples);
                // only +0x58, the type and the volume are set
                if (auto known = table.sounds.find(id); known != table.sounds.end())
                {
                    known->second.field58 = 0;
                    known->second.type = type;
                    known->second.volume = volume;
                    current = &known->second;
                    break;
                }
                SoundDef def;
                def.id = id;
                def.type = type;
                def.volume = volume;
                def.priority = priority;
                current = &(table.sounds[id] = def);
                break;
            }
            case 2: case 3: case 4:
            {
                int length = u16();
                need(size_t(length));
                std::string name(reinterpret_cast<const char*>(&data[pos]), strnlen(reinterpret_cast<const char*>(&data[pos]), size_t(length)));
                pos += size_t(length);
                bool wanted = op == 2 || (op == 4 && human) || (op == 3 && !human);
                if (wanted)
                {
                    // 0x466B40 looks the sample up in all three groups and adds nothing when it's there already (no cap
                    // on a group's count)
                    bool already = false;
                    for (const auto& g : entry().groups)
                        for (const SoundSample& s : g) already |= s.name == name;
                    if (!already) entry().groups[group].push_back({ name, type == SoundDef::Speech, rate });
                }
                break;
            }
            case 5: entry().volume *= f32(); break;
            case 6: entry().flag6 = true; break;
            case 7: entry().loop = true; break;
            case 8: entry().field58 = s16(); break;
            case 9: entry().field5C = s16(); break;
            case 10: entry().field64 = s16(); break;
            case 11:
            {
                int value = s16();
                rate = value == 11 ? 0x2B11 : value == 22 ? DefaultRate : value;
                for (auto& g : entry().groups)
                    for (SoundSample& s : g) s.rate = rate;
                break;
            }
            case 13:
            {
                int from = s16();
                SoundDef& to = entry();
                int id = to.id;
                const SoundDef* source = table.Find(from);
                to = source ? *source : SoundDef{};
                to.id = id;
                break;
            }
            case 14: volume = f32(); break;
            case 15: entry().stereo = true; break;
            case 16: break;
            case 17: group = std::min(std::max(int(u16()) - 1, 0), 2); break;
            case 18: entry().positional = true; break;
            default: break; // 12 and anything unknown have no arguments
            }
        }
        return table;
    }
}

#include "core/Archive.h"
#include "core/Wav.h"
#include <fstream>
#include <iterator>

namespace wow
{
    SoundLibrary::SoundLibrary(const GameFiles& f, bool h) : files(f), human(h)
    {
        const char* const Paths[] = { "SFX\\SFX.WoW", "VOX\\HUMAN.WoW", "VOX\\MARTIAN.WoW" };
        for (int i = 0; i < 3; i++)
            if (auto path = files.Resolve(Paths[i]))
            {
                try { libraries[i] = std::make_unique<Archive>(*path); }
                catch (const std::exception&) {} // a damaged library is treated as missing
            }
    }

    SoundLibrary::~SoundLibrary() = default;

    std::optional<std::vector<int16_t>> SoundLibrary::Load(const SoundSample& sample, bool stereo)
    {
        auto toPcm = [](const std::vector<uint8_t>& bytes) {
            std::vector<int16_t> pcm(bytes.size() / 2);
            for (size_t i = 0; i < pcm.size(); i++) pcm[i] = int16_t(bytes[i * 2] | (bytes[i * 2 + 1] << 8));
            return pcm;
        };
        // a loose replacement first, unless the libraries are searched first
        if (!librariesFirst)
            if (auto loose = LoadLoose(sample, stereo)) return loose;
        for (auto& library : libraries)
        {
            if (!library) continue;
            if (auto index = library->Find(sample.name)) return toPcm(library->Read(*index, true));
        }
        if (librariesFirst) return LoadLoose(sample, stereo);
        return std::nullopt;
    }

    std::optional<std::vector<int16_t>> SoundLibrary::LoadLoose(const SoundSample& sample, bool stereo)
    {
        std::vector<std::string> folders;
        if (sample.vox) folders = { std::string("VOX\\") + (human ? "HUMAN\\" : "MARTIAN\\"), "VOX\\" };
        else folders = { "SFX\\" };
        for (const std::string& folder : folders)
            for (const char* ext : { ".wav", ".ogg", ".snd" })
            {
                auto path = files.Resolve(folder + sample.name + ext);
                if (!path) continue;
                std::ifstream in(*path, std::ios::binary);
                std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
                if (bytes.empty()) continue;
                if (ext[1] == 'o')
                {
                    std::string error;
                    if (auto pcm = DecodeOgg(bytes, sample.rate, error, stereo ? 2 : 1))
                    {
                        if (log) log("Sound file: " + path->string());
                        return pcm;
                    }
                    if (log) log("Sound file " + path->string() + ": " + error + "; not used");
                    continue;
                }
                if (ext[1] == 's')
                {
                    if (log) log("Sound file: " + path->string());
                    std::vector<int16_t> pcm(bytes.size() / 2);
                    for (size_t i = 0; i < pcm.size(); i++) pcm[i] = int16_t(bytes[i * 2] | (bytes[i * 2 + 1] << 8));
                    return pcm;
                }
                std::string error;
                if (auto pcm = DecodeWav(bytes, sample.rate, error, stereo ? 2 : 1))
                {
                    if (log) log("Sound file: " + path->string());
                    return pcm;
                }
                if (log) log("Sound file " + path->string() + ": " + error + "; not used");
            }
        return std::nullopt;
    }
}
