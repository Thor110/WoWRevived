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
                priority = type == SoundDef::Speech ? 0x80 : 0xC0;
                break;
            case 1:
            {
                int id = s16();
                rate = DefaultRate;
                group = 0;
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
                    auto& samples = entry().groups[group];
                    // the game keeps at most 5 per group, and shares a sample used twice
                    bool already = false;
                    for (const SoundSample& s : samples) already |= s.name == name;
                    if (!already && samples.size() < 5) samples.push_back({ name, type == SoundDef::Speech, rate });
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
#include <fstream>
#include <iterator>

namespace wow
{
    SoundLibrary::SoundLibrary(const GameFiles& f) : files(f)
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

    std::optional<std::vector<int16_t>> SoundLibrary::Load(const SoundSample& sample)
    {
        auto toPcm = [](const std::vector<uint8_t>& bytes) {
            std::vector<int16_t> pcm(bytes.size() / 2);
            for (size_t i = 0; i < pcm.size(); i++) pcm[i] = int16_t(bytes[i * 2] | (bytes[i * 2 + 1] << 8));
            return pcm;
        };
        // a loose replacement first
        if (auto loose = files.Resolve(std::string(sample.vox ? "VOX\\" : "SFX\\") + sample.name + ".snd"))
        {
            std::ifstream in(*loose, std::ios::binary);
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
            if (!bytes.empty()) return toPcm(bytes);
        }
        for (auto& library : libraries)
        {
            if (!library) continue;
            if (auto index = library->Find(sample.name)) return toPcm(library->Read(*index, true));
        }
        return std::nullopt;
    }
}
