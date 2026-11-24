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
    ///
    /// The file is kept as it was written, as IniStore.h keeps it (WoWEngine update 248): comments (; or #) and blank
    /// lines stay where they are, a changed value is changed in its own line, and a new value takes the place of its
    /// commented example ("; Name=value") in its section when there is one, else goes after the section's last value.
    /// So the documented WoWRevived.ini WoWEngine writes keeps its documentation whichever program saves it.
    /// </summary>
    public sealed class GameSettings
    {
        public const string FileName = "WoWRevived.ini";
        public const string RootSection = "Main";

        private static GameSettings? instance;
        /// <summary>The settings file next to the running program (the game folder).</summary>
        public static GameSettings Instance => instance ??= new GameSettings(Path.Combine(AppContext.BaseDirectory, FileName));

        // a section's lines in order: its values, and the comments and blank lines between them (Text)
        private sealed class Line
        {
            public bool IsValue;
            public string Name = "", Value = "", Text = "";
        }

        private sealed class Section
        {
            public string Name = "";
            public List<Line> Items = new List<Line>();
            public IEnumerable<Line> Values => Items.Where(i => i.IsValue);
            public int IndexOf(string name) => Items.FindIndex(i => i.IsValue && Same(i.Name, name));
        }

        private readonly List<Section> sections = new List<Section>();
        private readonly List<string> preamble = new List<string>(); // the lines before the first section
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
        public IEnumerable<string> ValueNames(string section) => FindSection(section)?.Values.Select(e => e.Name) ?? Enumerable.Empty<string>();

        public string? Get(string section, string name)
        {
            Section? s = FindSection(section);
            if (s == null) { return null; }
            int index = s.IndexOf(name);
            return index >= 0 ? s.Items[index].Value : null;
        }

        public void Set(string section, string name, string value)
        {
            if (value.Contains('\n') || value.Contains('\r')) { throw new ArgumentException("Setting values can't contain line breaks.", nameof(value)); }
            Section s = EnsureSection(section);
            int index = s.IndexOf(name);
            if (index >= 0)
            {
                if (s.Items[index].Value == value) { return; }
                s.Items[index].Value = value;
            }
            else
            {
                var added = new Line { IsValue = true, Name = name, Value = value };
                int example = ExampleOf(s, name);
                if (example >= 0) { s.Items[example] = added; } // in place of its commented example
                else { s.Items.Insert(InsertAt(s), added); }
            }
            Changed();
        }

        public bool Delete(string section, string name)
        {
            Section? s = FindSection(section);
            int index = s?.IndexOf(name) ?? -1;
            if (index < 0) { return false; }
            s!.Items.RemoveAt(index);
            Changed();
            return true;
        }

        /// <summary>Removes every setting (used when the game files are freshly copied from the discs).</summary>
        public void Clear()
        {
            if (sections.Count == 0) { return; }
            sections.Clear();
            preamble.Clear();
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
            bool blankLast = false;
            void Write(string line)
            {
                text.Append(line).Append("\r\n");
                blankLast = line.Trim(' ', '\t').Length == 0;
            }
            if (preamble.Count == 0)
            {
                Write("; WoW Revived settings, used instead of the registry key");
                Write("; HKLM\\SOFTWARE\\Rage\\Jeff Wayne's 'The War Of The Worlds'\\1.00.000 so the installation is portable.");
                Write("; [Main] is that key itself, the other sections are its subkeys. Every value is plain text.");
            }
            else { foreach (string p in preamble) { Write(p); } }
            foreach (Section s in sections)
            {
                if (text.Length > 0 && !blankLast) { Write(""); }
                Write("[" + s.Name + "]");
                foreach (Line item in s.Items) { Write(item.IsValue ? item.Name + "=" + item.Value : item.Text); }
            }
            return text.ToString();
        }

        private static bool IsBlank(Line l) => !l.IsValue && l.Text.Trim(' ', '\t').Length == 0;
        private static bool IsComment(Line l)
        {
            if (l.IsValue) { return false; }
            string t = l.Text.TrimStart(' ', '\t');
            return t.Length > 0 && (t[0] == ';' || t[0] == '#');
        }
        // the name of a commented example ("; Name=value"), or "" for any other line
        private static string ExampleName(Line l)
        {
            if (!IsComment(l)) { return ""; }
            string rest = l.Text.TrimStart(' ', '\t')[1..];
            int eq = rest.IndexOf('=');
            return eq < 0 ? "" : rest[..eq].Trim(' ', '\t');
        }
        private static int ExampleOf(Section s, string name) => s.Items.FindIndex(i => Same(ExampleName(i), name));
        // where a new value goes: after the section's last value or commented example, else after its leading
        // comment (the section's own) and the blank lines after it, else first
        private static int InsertAt(Section s)
        {
            int at = s.Items.FindLastIndex(i => i.IsValue || ExampleName(i).Length > 0);
            if (at >= 0) { return at + 1; }
            int lead = 0;
            while (lead < s.Items.Count && (IsComment(s.Items[lead]) || IsBlank(s.Items[lead]))) { lead++; }
            return lead;
        }

        // same rules as IniStore::Parse in WoWPatches/Common/IniStore.h
        private void Parse(string text)
        {
            Section? current = null;
            string[] lines = text.Split('\n');
            // (the newline after the last line doesn't start another)
            int count = lines.Length > 0 && lines[^1].Length == 0 ? lines.Length - 1 : lines.Length;
            for (int n = 0; n < count; n++)
            {
                string rawLine = lines[n];
                string line = rawLine.EndsWith('\r') ? rawLine[..^1] : rawLine;
                if (current == null && sections.Count == 0 && preamble.Count == 0 && line.StartsWith("ï»¿")) { line = line[3..]; } // stray UTF-8 BOM read as Latin-1
                string trimmed = line.TrimStart(' ', '\t');
                // blank lines, comments and anything else that isn't a value are kept where they are
                void Keep()
                {
                    if (current != null) { current.Items.Add(new Line { Text = line }); }
                    else { preamble.Add(line); }
                }
                if (trimmed.Length == 0 || trimmed[0] == ';' || trimmed[0] == '#') { Keep(); continue; }
                if (trimmed[0] == '[')
                {
                    int close = trimmed.LastIndexOf(']');
                    if (close <= 0) { Keep(); continue; }
                    current = EnsureSection(trimmed[1..close]);
                    continue;
                }
                int equals = line.IndexOf('=');
                if (equals < 0 || current == null) { Keep(); continue; }
                string name = line[..equals].Trim(' ', '\t');
                if (name.Length == 0) { Keep(); continue; }
                string value = line[(equals + 1)..]; // values are kept exactly as written
                int index = current.IndexOf(name);
                if (index >= 0) { current.Items[index].Value = value; }
                else { current.Items.Add(new Line { IsValue = true, Name = name, Value = value }); }
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
