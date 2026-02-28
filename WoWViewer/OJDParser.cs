using System.Text;

namespace WoWViewer
{
    // OBJ.ojd binary format (reverse-engineered for WoWRevived, Claude.ai assisted)
    //
    // Flat sequence of variable-length entries, each beginning with 0xFF:
    //
    //   [+0x00]  0xFF          marker byte
    //   [+0x01]  uint16 LE     id      – unique object ID
    //   [+0x03]  uint16 LE     type    – entry class (see below)
    //   [+0x05]  uint16 LE     length  – byte count of string INCLUDING null terminator
    //                                    (0 = metadata entry; no string, no palSlot)
    //   [+0x07]  char[]        name    – ASCII, length bytes, null-terminated
    //   [+0x07+length]
    //            uint16 LE     palSlot – palette file index 0-15
    //                                    ONLY present for types 2, 3, 4
    //                                    Absent for types 5, 16, 19, 50 (next byte = 0xFF)
    //
    //   type values (confirmed):
    //     2   UI assets          (.spr, .iob, .wof, .pal, .raw)
    //     3   Human-faction      (.spr, .fnt, .raw)
    //     4   Martian-faction    (.spr, .fnt, .raw)
    //     5   Named constant     (no string/palSlot)
    //     16  OTYPE_ enum names  e.g. "OTYPE_EXITARROW" (no palSlot)
    //     19  Named constant     (no palSlot)
    //     50  Named constant     (no palSlot)
    //
    //   palSlot values (types 2/3/4):
    //     Index into the 16 PAL files, in order of first appearance in OBJ.ojd:
    //       0=HW  1=MW  2=HB  3=MB  4=HR  5=MR  6=BM
    //       7=F1  8=F2  9=F3  10=F4 11=F5 12=F6 13=F7  14=SE  15=CD
    //     Values >= 16 are special (in-memory or level-specific palettes).
    //
    //   SPR→PAL mapping: see SprDecoder.GetPaletteForSpr()

    public partial class OJDParser : Form
    {
        private List<OjdEntry> entries = new List<OjdEntry>();
        // Types that carry NO palSlot field after the null terminator
        private static readonly HashSet<ushort> NoPalSlotTypes = new() { 5, 16, 19, 50 };
        public OJDParser()
        {
            InitializeComponent();
        }
        // Parse OBJ.ojd into a flat list of OjdEntry objects.
        public static List<OjdEntry> ParseOjdFile()
        {
            var result = new List<OjdEntry>();
            byte[] data = File.ReadAllBytes("OBJ.ojd");
            int i = 0;

            while (i < data.Length - 6)
            {
                if (data[i] != 0xFF) { i++; continue; }

                ushort id     = BitConverter.ToUInt16(data, i + 1);
                ushort type   = BitConverter.ToUInt16(data, i + 3);
                ushort length = BitConverter.ToUInt16(data, i + 5);
                int strStart  = i + 7;

                // Zero-length = metadata only, no string
                if (length == 0) { i += 7; continue; }

                if (strStart + length > data.Length) { i++; continue; }

                int nullPos = Array.IndexOf(data, (byte)0, strStart, Math.Min(length + 4, data.Length - strStart));
                if (nullPos < 0) { i++; continue; }

                string name;
                try
                {
                    name = Encoding.ASCII.GetString(data, strStart, nullPos - strStart);
                    if (!name.All(c => c >= 0x20 && c <= 0x7E)) { i++; continue; }
                }
                catch { i++; continue; }

                ushort palSlot = 0;
                if (NoPalSlotTypes.Contains(type))
                {
                    i = nullPos + 1;  // no palSlot bytes
                }
                else
                {
                    palSlot = (nullPos + 3 <= data.Length) ? BitConverter.ToUInt16(data, nullPos + 1) : (ushort)0;
                    i = nullPos + 3;
                }

                if(name.Length > 6) // prevent listing non-existent entries
                {
                    result.Add(new OjdEntry { Id = id, Type = type, Length = length, Name = name, PalSlot = palSlot });
                }
                
            }
            return result;
        }

        // ── UI ───────────────────────────────────────────────────────────────────

        public void parseOBJOJD()
        {
            listBox1.Items.Clear();
            entries = ParseOjdFile();
            textBox1.Text = "";
            textBox2.Text = "";
            textBox3.Text = "";
            textBox4.Text = "";
            //
            string logPath = "ojd_log.txt";
            if (File.Exists(logPath)) File.Delete(logPath);
            foreach (var entry in entries)
            {
                listBox1.Items.Add(entry.Name);
                File.AppendAllText(logPath, $"{entry}\n");
            }
            label1.Text = $"Total Entries: {entries.Count}";
        }
        public void parseSFXOJD()
        {
            listBox1.Items.Clear();
            entries.Clear();
            textBox1.Text = "";
            textBox2.Text = "";
            textBox3.Text = "";
            textBox4.Text = "";
            //
            byte[] data = File.ReadAllBytes("SFX.ojd");
            int count = 0;
            for (int i = 0; i < data.Length - 1; i++)
            {
                if (!IsAsciiChar(data[i]) || data[i + 1] == 0x00) { continue; }
                int start = i;
                int length = 0;
                while (i < data.Length && IsAsciiChar(data[i])) { i++; length++; }
                if (length < 2 || i >= data.Length || data[i] != 0x00) { continue; }
                string text = Encoding.ASCII.GetString(data, start, length);
                int headerOffset = start - 7;
                string type = "Unverified";
                string headerID = "??";
                ushort hid = 0;
                if (headerOffset >= 0 && data[headerOffset] == 0xFF)
                {
                    hid = BitConverter.ToUInt16(data, headerOffset + 1);
                    ushort maybeLength = BitConverter.ToUInt16(data, headerOffset + 5);
                    headerID = hid.ToString("X4");
                    if (maybeLength == length + 1)
                    {
                        type = "StringEntry";
                        listBox1.Items.Add(text);
                    }
                    else type = "MismatchedLength";
                }
                count++;
                entries.Add(new OjdEntry { Id = hid, Type = 0xFF, Length = (ushort)length, Name = text });
            }
            label1.Text = $"Total Strings: {count}";
        }

        private static bool IsAsciiChar(byte b) => b >= 0x20 && b <= 0x7E;

        private void button1_Click(object sender, EventArgs e) { parseOBJOJD(); } // OBJ.ojd
        private void button2_Click(object sender, EventArgs e) { parseSFXOJD(); } // SFX.ojd
        private void button3_Click(object sender, EventArgs e) { MessageBox.Show("TEXT.ojd fully decoded!"); }

        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            var entry = entries[listBox1.SelectedIndex];
            textBox1.Text = entry.Id.ToString();
            textBox2.Text = entry.Type != 0xFF ? entry.Type.ToString() : "UNRECORDED";
            textBox3.Text = entry.Length.ToString();
            textBox4.Text = entry.Name;
        }
    }
}