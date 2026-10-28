// WoWRevived.ini - the portable replacement for the game's registry key
// HKLM\SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000
//
// Format (shared with WoWLauncher / WoWViewer, see Shared/GameSettings.cs):
//   [Main]            the 1.00.000 key itself
//   Full Screen=1
//   [Sound\Volume]    sections are the subkey paths below 1.00.000
//   Master=1.000000
// Every value is plain text. Both game builds accept text for every setting they read (their readers
// fall back to atoi/atof/sscanf for anything that isn't a DWORD), so numbers are simply stored in decimal.
// Section and value names are case-insensitive like the registry. Bytes are stored as-is (Windows-1252).
//
// Standard C++ only apart from the atomic file replace, so the logic can be tested off Windows.
#pragma once
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace wow
{
    class IniStore
    {
    public:
        static const char* FileName() { return "WoWRevived.ini"; }
        static const char* RootSection() { return "Main"; }

        // Loads the file. Returns false (leaving the store empty) if it doesn't exist.
        bool Load(const std::string& path)
        {
            std::lock_guard<std::mutex> lock(mutex);
            this->path = path;
            sections.clear();
            dirty = false;
            FILE* f = std::fopen(path.c_str(), "rb");
            if (!f) return false;
            std::string text;
            char buffer[4096];
            size_t read;
            while ((read = std::fread(buffer, 1, sizeof(buffer), f)) > 0) text.append(buffer, read);
            std::fclose(f);
            Parse(text);
            dirty = false; // nothing to write back after a plain load
            return true;
        }

        bool Get(const std::string& section, const std::string& name, std::string& value) const
        {
            std::lock_guard<std::mutex> lock(mutex);
            const Section* s = FindSection(section);
            if (!s) return false;
            const Entry* e = FindEntry(*s, name);
            if (!e) return false;
            value = e->value;
            return true;
        }

        void Set(const std::string& section, const std::string& name, const std::string& value)
        {
            std::lock_guard<std::mutex> lock(mutex);
            Section& s = EnsureSectionLocked(section);
            Entry* e = FindEntry(s, name);
            if (e) {
                if (e->value == value) return;
                e->value = value;
            }
            else s.entries.push_back({ name, value });
            dirty = true;
        }

        bool Delete(const std::string& section, const std::string& name)
        {
            std::lock_guard<std::mutex> lock(mutex);
            Section* s = FindSection(section);
            if (!s) return false;
            for (size_t i = 0; i < s->entries.size(); i++) {
                if (Equal(s->entries[i].name, name)) {
                    s->entries.erase(s->entries.begin() + i);
                    dirty = true;
                    return true;
                }
            }
            return false;
        }

        // True if the section exists, or any section below it does (like a registry key with subkeys).
        bool KeyExists(const std::string& section) const
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const Section& s : sections) {
                if (Equal(s.name, section)) return true;
                if (s.name.size() > section.size() && s.name[section.size()] == '\\' && Equal(s.name.substr(0, section.size()), section)) return true;
            }
            return false;
        }

        bool HasAnySection() const
        {
            std::lock_guard<std::mutex> lock(mutex);
            return !sections.empty();
        }

        void EnsureSection(const std::string& section)
        {
            std::lock_guard<std::mutex> lock(mutex);
            EnsureSectionLocked(section);
        }

        bool IsDirty() const { std::lock_guard<std::mutex> lock(mutex); return dirty; }

        // Writes the file if anything changed: to a temporary file first, which then replaces the original.
        bool Save()
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!dirty || path.empty()) return true;
            std::string text = Serialize();
            std::string temp = path + ".tmp";
            FILE* f = std::fopen(temp.c_str(), "wb");
            if (!f) return false;
            bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
            ok = (std::fclose(f) == 0) && ok;
            if (!ok) { std::remove(temp.c_str()); return false; }
#ifdef _WIN32
            ok = MoveFileExA(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
            ok = std::rename(temp.c_str(), path.c_str()) == 0;
#endif
            if (ok) dirty = false;
            else std::remove(temp.c_str());
            return ok;
        }

        std::string Serialize() const
        {
            std::string out =
                "; WoW Revived settings, used instead of the registry key\r\n"
                "; HKLM\\SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000 so the installation is portable.\r\n"
                "; [Main] is that key itself, the other sections are its subkeys. Every value is plain text.\r\n";
            for (const Section& s : sections) {
                out += "\r\n[" + s.name + "]\r\n";
                for (const Entry& e : s.entries) out += e.name + "=" + e.value + "\r\n";
            }
            return out;
        }

        void Parse(const std::string& text)
        {
            sections.clear();
            Section* current = nullptr;
            size_t pos = 0;
            while (pos <= text.size()) {
                size_t end = text.find('\n', pos);
                if (end == std::string::npos) end = text.size();
                std::string line = text.substr(pos, end - pos);
                pos = end + 1;
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (sections.empty() && current == nullptr && line.size() >= 3 &&
                    (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) line.erase(0, 3); // stray BOM
                size_t first = line.find_first_not_of(" \t");
                if (first == std::string::npos) continue; // blank
                if (line[first] == ';' || line[first] == '#') continue; // comment
                if (line[first] == '[') {
                    size_t close = line.rfind(']');
                    if (close == std::string::npos || close <= first) continue;
                    current = &EnsureSectionLocked(line.substr(first + 1, close - first - 1));
                    continue;
                }
                size_t eq = line.find('=');
                if (eq == std::string::npos || current == nullptr) continue;
                std::string name = Trim(line.substr(0, eq));
                if (name.empty()) continue;
                std::string value = line.substr(eq + 1); // values are kept exactly as written
                Entry* e = FindEntry(*current, name);
                if (e) e->value = value; else current->entries.push_back({ name, value });
            }
        }

    private:
        struct Entry { std::string name, value; };
        struct Section { std::string name; std::vector<Entry> entries; };

        std::vector<Section> sections;
        std::string path;
        bool dirty = false;
        mutable std::mutex mutex;

        static bool Equal(const std::string& a, const std::string& b)
        {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); i++) {
                unsigned char x = (unsigned char)a[i], y = (unsigned char)b[i];
                if (x >= 'A' && x <= 'Z') x = (unsigned char)(x - 'A' + 'a');
                if (y >= 'A' && y <= 'Z') y = (unsigned char)(y - 'A' + 'a');
                if (x != y) return false;
            }
            return true;
        }

        static std::string Trim(const std::string& s)
        {
            size_t a = s.find_first_not_of(" \t");
            if (a == std::string::npos) return "";
            size_t b = s.find_last_not_of(" \t");
            return s.substr(a, b - a + 1);
        }

        Section* FindSection(const std::string& name)
        {
            for (Section& s : sections) if (Equal(s.name, name)) return &s;
            return nullptr;
        }
        const Section* FindSection(const std::string& name) const
        {
            for (const Section& s : sections) if (Equal(s.name, name)) return &s;
            return nullptr;
        }
        static Entry* FindEntry(Section& s, const std::string& name)
        {
            for (Entry& e : s.entries) if (Equal(e.name, name)) return &e;
            return nullptr;
        }
        static const Entry* FindEntry(const Section& s, const std::string& name)
        {
            for (const Entry& e : s.entries) if (Equal(e.name, name)) return &e;
            return nullptr;
        }
        Section& EnsureSectionLocked(const std::string& name)
        {
            Section* s = FindSection(name);
            if (s) return *s;
            sections.push_back({ name, {} });
            dirty = true;
            return sections.back();
        }
    };
}
