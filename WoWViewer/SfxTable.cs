using System.Text;

namespace WoWViewer
{
    // Each sound sample's rate and channels, from SFX.ojd (the game's sound table), so a sound library's entries are
    // played, drawn and exported as what they are. The entries themselves are raw 16-bit PCM with no header: every
    // one in the game is 22050 Hz, and the 18 samples of the sounds the table marks stereo (its opcode 15: the war
    // room's ambients, the briefings, the title, victory and defeat music, the epilogues, OPTION) are interleaved
    // stereo, which taken as mono would play at half speed. A port of WoWEngine's SoundTable::Load (WoWEngine/src/core/SoundTable.cpp), keeping only what
    // the samples need; both sides' samples are read (opcodes 3 and 4 are each side's own).
    public static class SfxTable
    {
        public readonly struct SampleFormat
        {
            public SampleFormat(int rate, int channels) { Rate = rate; Channels = channels; }
            public int Rate { get; }
            public int Channels { get; }
        }

        private static Dictionary<string, SampleFormat>? formats;
        private static string loadedFrom = "";

        // SFX.ojd from the game folder: the folder above the library's (SFX\SFX.WoW, VOX\HUMAN.WoW), or the working
        // folder. Nothing when it can't be found or read.
        public static SampleFormat? Find(string libraryPath, string name)
        {
            string? folder = Path.GetDirectoryName(Path.GetDirectoryName(Path.GetFullPath(libraryPath)));
            string[] places = { folder != null ? Path.Combine(folder, "SFX.ojd") : "", "SFX.ojd" };
            foreach (string place in places)
            {
                if (place == "" || !File.Exists(place)) { continue; }
                if (formats == null || loadedFrom != place)
                {
                    try { formats = Load(File.ReadAllBytes(place)); loadedFrom = place; }
                    catch (Exception) { formats = null; loadedFrom = ""; continue; }
                }
                var table = formats;
                if (table == null) { continue; }
                return table.TryGetValue(name.ToUpperInvariant(), out var f) ? f : null;
            }
            return null;
        }

        private sealed class Def
        {
            public bool Stereo;
            public List<(string Name, int Rate)> Samples = new();
        }

        // Each record: 0xFF, a line number (u16), the opcode (s16), then its arguments.
        public static Dictionary<string, SampleFormat> Load(byte[] data)
        {
            const int DefaultRate = 22050;
            var defs = new Dictionary<int, Def>();
            Def? current = null;
            int rate = DefaultRate;
            int pos = 0;
            int U16() { if (pos + 2 > data.Length) throw new InvalidDataException("SFX.ojd ends early"); int v = data[pos] | data[pos + 1] << 8; pos += 2; return v; }
            int S16() => (short)U16();
            void Skip(int n) { if (pos + n > data.Length) throw new InvalidDataException("SFX.ojd ends early"); pos += n; }
            while (pos < data.Length)
            {
                Skip(3);                    // 0xFF and the line number
                int op = S16();
                switch (op)
                {
                    case 0: S16(); break;  // the type
                    case 1:
                    {
                        int id = S16();
                        rate = DefaultRate;
                        if (!defs.TryGetValue(id, out current)) { current = new Def(); defs[id] = current; }
                        break;
                    }
                    case 2: case 3: case 4:
                    {
                        int length = U16();
                        Skip(length);
                        int end = pos - length;
                        int n = 0;
                        while (n < length && data[end + n] != 0) { n++; }
                        string name = Encoding.ASCII.GetString(data, end, n);
                        if (current != null && !current.Samples.Exists(s => s.Name == name)) { current.Samples.Add((name, rate)); }
                        break;
                    }
                    case 5: case 14: Skip(4); break;               // volumes (floats)
                    case 8: case 9: case 10: case 17: S16(); break;
                    case 11:
                    {
                        int value = S16();
                        rate = value == 11 ? 11025 : value == 22 ? DefaultRate : value;
                        if (current != null) { for (int i = 0; i < current.Samples.Count; i++) { current.Samples[i] = (current.Samples[i].Name, rate); } }
                        break;
                    }
                    case 13:
                    {
                        // a copy of another sound's details and samples
                        int from = S16();
                        if (current != null && defs.TryGetValue(from, out var source))
                        {
                            current.Stereo = source.Stereo;
                            current.Samples = new List<(string, int)>(source.Samples);
                        }
                        break;
                    }
                    case 15: if (current != null) { current.Stereo = true; } break;
                    default: break;        // 6, 7, 12, 16, 18 and anything unknown have no arguments
                }
            }
            // a sample is stereo when any sound that plays it is
            var result = new Dictionary<string, SampleFormat>();
            foreach (var def in defs.Values)
            {
                foreach (var (name, r) in def.Samples)
                {
                    string key = name.ToUpperInvariant();
                    int channels = def.Stereo ? 2 : 1;
                    if (result.TryGetValue(key, out var known) && known.Channels >= channels) { continue; }
                    result[key] = new SampleFormat(r, channels);
                }
            }
            return result;
        }
    }
}
