using System.Globalization;
using System.Text;
using Microsoft.Win32;

namespace WoWRevived.Shared
{
    /// <summary>
    /// WoWRevived.ini, the portable replacement for the game's registry key
    /// HKLM\SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000.
    ///
    /// [Main] is the 1.00.000 key itself and every other section is a subkey path below it ([Sound\Volume]).
    /// Every value is plain text: both game builds read all of their settings from text, so the vanilla/network
    /// DWORD-versus-string differences no longer apply. _inmm.dll redirects the game's registry calls here and
    /// shares this exact format (WoWPatches/Common/IniStore.h). Names are case-insensitive like the registry and
    /// bytes are kept as-is (Latin-1 maps every byte to one character and back).
    ///
    /// Changes are saved straight away unless a batch is open (see <see cref="Batch"/>).
    /// </summary>
    public sealed class GameSettings
    {
        public const string FileName = "WoWRevived.ini";
        public const string RootSection = "Main";

        private static GameSettings? instance;
        /// <summary>The settings file next to the running program (the game folder).</summary>
        public static GameSettings Instance => instance ??= new GameSettings(Path.Combine(AppContext.BaseDirectory, FileName));

        private sealed class Section
        {
            public string Name = "";
            public List<KeyValuePair<string, string>> Entries = new List<KeyValuePair<string, string>>();
        }

        private readonly List<Section> sections = new List<Section>();
        private int batchDepth;
        public string FilePath { get; }
        /// <summary>True if the file already existed when it was loaded.</summary>
        public bool Existed { get; }
        public bool IsDirty { get; private set; }

        public GameSettings(string path)
        {
            FilePath = path;
            Existed = File.Exists(path);
            if (Existed) { Parse(File.ReadAllText(path, Encoding.Latin1)); }
        }

        /// <summary>A view of one section, with the same GetValue/SetValue shape the code used with RegistryKey.</summary>
        public SettingsKey Key(string section = RootSection) => new SettingsKey(this, section.Length == 0 ? RootSection : section);

        public bool HasAnySection => sections.Count > 0;
        public IEnumerable<string> SectionNames => sections.Select(s => s.Name);
        public IEnumerable<string> ValueNames(string section) => FindSection(section)?.Entries.Select(e => e.Key) ?? Enumerable.Empty<string>();

        public string? Get(string section, string name)
        {
            Section? s = FindSection(section);
            if (s == null) { return null; }
            int index = s.Entries.FindIndex(e => Same(e.Key, name));
            return index >= 0 ? s.Entries[index].Value : null;
        }

        public void Set(string section, string name, string value)
        {
            if (value.Contains('\n') || value.Contains('\r')) { throw new ArgumentException("Setting values can't contain line breaks.", nameof(value)); }
            Section s = EnsureSection(section);
            int index = s.Entries.FindIndex(e => Same(e.Key, name));
            if (index >= 0)
            {
                if (s.Entries[index].Value == value) { return; }
                s.Entries[index] = new KeyValuePair<string, string>(s.Entries[index].Key, value);
            }
            else { s.Entries.Add(new KeyValuePair<string, string>(name, value)); }
            Changed();
        }

        public bool Delete(string section, string name)
        {
            Section? s = FindSection(section);
            int index = s?.Entries.FindIndex(e => Same(e.Key, name)) ?? -1;
            if (index < 0) { return false; }
            s!.Entries.RemoveAt(index);
            Changed();
            return true;
        }

        /// <summary>Removes every setting (used when the game files are freshly copied from the discs).</summary>
        public void Clear()
        {
            if (sections.Count == 0) { return; }
            sections.Clear();
            Changed();
        }

        /// <summary>Holds off saving until the returned object is disposed, then saves once.</summary>
        public IDisposable Batch()
        {
            batchDepth++;
            return new BatchScope(this);
        }

        private sealed class BatchScope : IDisposable
        {
            private GameSettings? owner;
            public BatchScope(GameSettings owner) { this.owner = owner; }
            public void Dispose()
            {
                if (owner == null) { return; }
                owner.batchDepth--;
                if (owner.batchDepth == 0) { owner.Save(); }
                owner = null;
            }
        }

        private void Changed()
        {
            IsDirty = true;
            if (batchDepth == 0) { Save(); }
        }

        /// <summary>Writes the file if anything changed, through a temporary file so it is never left half written.</summary>
        public void Save()
        {
            if (!IsDirty) { return; }
            string temp = FilePath + ".tmp";
            File.WriteAllText(temp, Serialize(), Encoding.Latin1);
            File.Move(temp, FilePath, overwrite: true);
            IsDirty = false;
        }

        public string Serialize()
        {
            var text = new StringBuilder();
            text.Append("; WoW Revived settings, used instead of the registry key\r\n");
            text.Append("; HKLM\\SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000 so the installation is portable.\r\n");
            text.Append("; [Main] is that key itself, the other sections are its subkeys. Every value is plain text.\r\n");
            foreach (Section s in sections)
            {
                text.Append("\r\n[").Append(s.Name).Append("]\r\n");
                foreach (var entry in s.Entries) { text.Append(entry.Key).Append('=').Append(entry.Value).Append("\r\n"); }
            }
            return text.ToString();
        }

        // same rules as IniStore::Parse in WoWPatches/Common/IniStore.h
        private void Parse(string text)
        {
            Section? current = null;
            foreach (string rawLine in text.Split('\n'))
            {
                string line = rawLine.EndsWith('\r') ? rawLine[..^1] : rawLine;
                if (current == null && sections.Count == 0 && line.StartsWith("ï»¿")) { line = line[3..]; } // stray UTF-8 BOM read as Latin-1
                string trimmed = line.TrimStart(' ', '\t');
                if (trimmed.Length == 0 || trimmed[0] == ';' || trimmed[0] == '#') { continue; }
                if (trimmed[0] == '[')
                {
                    int close = trimmed.LastIndexOf(']');
                    if (close <= 0) { continue; }
                    current = EnsureSection(trimmed[1..close]);
                    continue;
                }
                int equals = line.IndexOf('=');
                if (equals < 0 || current == null) { continue; }
                string name = line[..equals].Trim(' ', '\t');
                if (name.Length == 0) { continue; }
                string value = line[(equals + 1)..]; // values are kept exactly as written
                int index = current.Entries.FindIndex(e => Same(e.Key, name));
                if (index >= 0) { current.Entries[index] = new KeyValuePair<string, string>(current.Entries[index].Key, value); }
                else { current.Entries.Add(new KeyValuePair<string, string>(name, value)); }
            }
            IsDirty = false;
        }

        private static bool Same(string a, string b) => string.Equals(a, b, StringComparison.OrdinalIgnoreCase);
        private Section? FindSection(string name) => sections.FirstOrDefault(s => Same(s.Name, name));
        private Section EnsureSection(string name)
        {
            Section? s = FindSection(name);
            if (s != null) { return s; }
            s = new Section { Name = name };
            sections.Add(s);
            IsDirty = true;
            return s;
        }
    }

    /// <summary>One section of <see cref="GameSettings"/>. Values come back as strings (or null when missing).</summary>
    public sealed class SettingsKey
    {
        private readonly GameSettings settings;
        public string Section { get; }
        public SettingsKey(GameSettings settings, string section)
        {
            this.settings = settings;
            Section = section;
        }

        public object? GetValue(string name) => settings.Get(Section, name);
        public object? GetValue(string name, object? defaultValue) => settings.Get(Section, name) ?? defaultValue;
        public string[] GetValueNames() => settings.ValueNames(Section).ToArray();

        /// <summary>Numbers are stored in decimal. The kind is accepted so existing registry style calls keep working, it doesn't change the text.</summary>
        public void SetValue(string name, object value, RegistryValueKind kind = RegistryValueKind.Unknown) =>
            settings.Set(Section, name, Convert.ToString(value, CultureInfo.InvariantCulture) ?? "");

        public bool DeleteValue(string name) => settings.Delete(Section, name);
    }
}
