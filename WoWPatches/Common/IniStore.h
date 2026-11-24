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
// The file is kept as it was written (WoWEngine update 248): comments (; or #) and blank lines stay where they
// are, a changed value is changed in its own line, and a new value takes the place of its commented example
// ("; Name=value") in its section when there is one, else goes after the section's last value. So a documented
// WoWRevived.ini (Document, WoWEngine core/SettingsTable.h) keeps its documentation as the game saves settings.
// Comments go on lines of their own: a value is everything after its '=', so "1 ; note" is the value "1 ; note".
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
            preamble.clear();
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
            else {
                Entry added{ true, name, value, "" };
                const size_t example = ExampleOf(s, name);
                if (example != std::string::npos) s.items[example] = added;    // in place of its commented example
                else s.items.insert(s.items.begin() + std::ptrdiff_t(InsertAt(s)), added);
            }
            dirty = true;
        }

        bool Delete(const std::string& section, const std::string& name)
        {
            std::lock_guard<std::mutex> lock(mutex);
            Section* s = FindSection(section);
            if (!s) return false;
            for (size_t i = 0; i < s->items.size(); i++) {
                if (s->items[i].isValue && Equal(s->items[i].name, name)) {
                    s->items.erase(s->items.begin() + std::ptrdiff_t(i));
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
            std::string out;
            bool blankLast = false;
            auto line = [&](const std::string& text) {
                out += text + "\r\n";
                blankLast = text.find_first_not_of(" \t") == std::string::npos;
            };
            if (preamble.empty()) {
                line("; WoW Revived settings, used instead of the registry key");
                line("; HKLM\\SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000 so the installation is portable.");
                line("; [Main] is that key itself, the other sections are its subkeys. Every value is plain text.");
            }
            else for (const std::string& p : preamble) line(p);
            for (const Section& s : sections) {
                if (!out.empty() && !blankLast) line("");
                line("[" + s.name + "]");
                for (const Entry& e : s.items) line(e.isValue ? e.name + "=" + e.value : e.text);
            }
            return out;
        }

        // Documentation for a value (WoWEngine's settings table): the comment lines above it (each written as
        // "; " + the line) and, when the value isn't set, its commented example "; Name=example". A value already
        // set keeps its value and gets the comment only if no comment is above it; nothing is added twice, so it
        // can be run on every start. sectionComment goes at the top of a section that has no comment there.
        // True if anything was added.
        bool Document(const std::string& section, const std::vector<std::string>& sectionComment, const std::string& name,
                      const std::string& example, const std::vector<std::string>& comment)
        {
            std::lock_guard<std::mutex> lock(mutex);
            const bool existed = FindSection(section) != nullptr;
            Section& s = EnsureSectionLocked(section);
            bool changed = !existed;
            if (!sectionComment.empty() && (s.items.empty() || !IsComment(s.items.front()))) {
                std::vector<Entry> lines;
                for (const std::string& c : sectionComment) lines.push_back(Comment(c));
                lines.push_back(Entry{ false, "", "", "" });    // (a blank line: the section's comment isn't its first value's)
                s.items.insert(s.items.begin(), lines.begin(), lines.end());
                changed = true;
            }
            size_t at = std::string::npos;
            for (size_t i = 0; i < s.items.size() && at == std::string::npos; i++)
                if (s.items[i].isValue && Equal(s.items[i].name, name)) at = i;
            if (at == std::string::npos) at = ExampleOf(s, name);
            if (at != std::string::npos) {
                // there already: its comment unless the line above is one (a blank line before it, for reading)
                if (comment.empty() || (at > 0 && IsComment(s.items[at - 1]))) { if (changed) dirty = true; return changed; }
                std::vector<Entry> lines;
                if (at > 0 && !IsBlank(s.items[at - 1])) lines.push_back(Entry{ false, "", "", "" });
                for (const std::string& c : comment) lines.push_back(Comment(c));
                s.items.insert(s.items.begin() + std::ptrdiff_t(at), lines.begin(), lines.end());
                dirty = true;
                return true;
            }
            std::vector<Entry> lines;
            const size_t where = InsertAt(s);
            // (a value without a comment of its own goes straight under the one before, as a list)
            if (!comment.empty() && where > 0 && !IsBlank(s.items[where - 1])) lines.push_back(Entry{ false, "", "", "" });
            for (const std::string& c : comment) lines.push_back(Comment(c));
            lines.push_back(Comment(name + "=" + example));
            s.items.insert(s.items.begin() + std::ptrdiff_t(where), lines.begin(), lines.end());
            dirty = true;
            return true;
        }

        void Parse(const std::string& text)
        {
            sections.clear();
            preamble.clear();
            Section* current = nullptr;
            size_t pos = 0;
            while (pos < text.size()) {      // (the newline after the last line doesn't start another)
                size_t end = text.find('\n', pos);
                if (end == std::string::npos) end = text.size();
                std::string line = text.substr(pos, end - pos);
                pos = end + 1;
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (sections.empty() && current == nullptr && preamble.empty() && line.size() >= 3 &&
                    (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) line.erase(0, 3); // stray BOM
                size_t first = line.find_first_not_of(" \t");
                // blank lines, comments and anything else that isn't a value are kept where they are
                auto keep = [&]() {
                    if (current) current->items.push_back(Entry{ false, "", "", line });
                    else preamble.push_back(line);
                };
                if (first == std::string::npos || line[first] == ';' || line[first] == '#') { keep(); continue; }
                if (line[first] == '[') {
                    size_t close = line.rfind(']');
                    if (close == std::string::npos || close <= first) { keep(); continue; }
                    current = &EnsureSectionLocked(line.substr(first + 1, close - first - 1));
                    continue;
                }
                size_t eq = line.find('=');
                if (eq == std::string::npos || current == nullptr) { keep(); continue; }
                std::string name = Trim(line.substr(0, eq));
                if (name.empty()) { keep(); continue; }
                std::string value = line.substr(eq + 1); // values are kept exactly as written
                Entry* e = FindEntry(*current, name);
                if (e) e->value = value; else current->items.push_back(Entry{ true, name, value, "" });
            }
        }

    private:
        // a section's lines in order: its values, and the comments and blank lines between them (text)
        struct Entry { bool isValue; std::string name, value, text; };
        struct Section { std::string name; std::vector<Entry> items; };

        std::vector<Section> sections;
        std::vector<std::string> preamble;  // the lines before the first section
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
            for (Entry& e : s.items) if (e.isValue && Equal(e.name, name)) return &e;
            return nullptr;
        }
        static const Entry* FindEntry(const Section& s, const std::string& name)
        {
            for (const Entry& e : s.items) if (e.isValue && Equal(e.name, name)) return &e;
            return nullptr;
        }
        static Entry Comment(const std::string& text) { return Entry{ false, "", "", text.empty() ? ";" : "; " + text }; }
        static bool IsBlank(const Entry& e) { return !e.isValue && e.text.find_first_not_of(" \t") == std::string::npos; }
        static bool IsComment(const Entry& e)
        {
            if (e.isValue) return false;
            const size_t first = e.text.find_first_not_of(" \t");
            return first != std::string::npos && (e.text[first] == ';' || e.text[first] == '#');
        }
        // the name of a commented example ("; Name=value"), or "" for any other line
        static std::string ExampleName(const Entry& e)
        {
            if (!IsComment(e)) return "";
            const size_t first = e.text.find_first_not_of(" \t");
            const std::string rest = e.text.substr(first + 1);
            const size_t eq = rest.find('=');
            if (eq == std::string::npos) return "";
            return Trim(rest.substr(0, eq));
        }
        static size_t ExampleOf(const Section& s, const std::string& name)
        {
            for (size_t i = 0; i < s.items.size(); i++) if (Equal(ExampleName(s.items[i]), name)) return i;
            return std::string::npos;
        }
        // where a new value goes: after the section's last value or commented example, else after its leading
        // comment (the section's own) and the blank lines after it, else first
        static size_t InsertAt(const Section& s)
        {
            size_t at = std::string::npos;
            for (size_t i = 0; i < s.items.size(); i++) if (s.items[i].isValue || !ExampleName(s.items[i]).empty()) at = i;
            if (at != std::string::npos) return at + 1;
            size_t lead = 0;
            while (lead < s.items.size() && (IsComment(s.items[lead]) || IsBlank(s.items[lead]))) lead++;
            return lead;
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
