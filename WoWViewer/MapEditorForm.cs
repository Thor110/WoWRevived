using System.Text;

namespace WoWViewer
{
    public partial class MapEditorForm : Form
    {
        private List<WowTextEntry> entries = new List<WowTextEntry>();
        private static readonly Encoding Latin1 = Encoding.GetEncoding("iso-8859-1");
        private int entryCount = 1397; // there are only 0-1396 entries
        private byte[]? levelData;
        public MapEditorForm()
        {
            InitializeComponent();
            byte[] data = File.ReadAllBytes("TEXT.ojd"); // read the file into a byte array
            switch (data.Length) // check file size
            {
                case 63839: // english  - 63839 bytes
                case 75224: // french   - 75224 bytes
                case 70448: // german   - 70448 bytes
                case 70218: // italian  - 70218 bytes
                case 71617: // spanish  - 71617 bytes
                    entryCount = 1396; // support for the original TEXT.ojd file without the added Credits entry.
                    break;
            }
            int offset = 0x289; // first string starts at 0x289
            for (int i = 0; i < entryCount; i++) // there are only 1396 entries
            {
                byte category = data[offset + 4];  // Faction: 00 = Martian, 01 = Human, 02 = UI
                ushort tag = (ushort)(data[offset + 6] | (data[offset + 7] << 8)); // TEXT.ojd key (2 bytes)
                ushort length = (ushort)(data[offset + 8] | (data[offset + 9] << 8)); // bytes 9 and 10 are the string length
                int stringOffset = offset + 10; // string offset
                string text = Latin1.GetString(data, stringOffset, length - 1).Replace("\\n", "\n");
                // string length is one less than the ushort length as length contains the null operator // replaces \n with actual new line
                entries.Add(new WowTextEntry { Name = text, Faction = category, Index = (ushort)i, ID = tag });
                offset += (int)length + 9; // move offset to next entry // not + 10 because length contains the null operator ( hence - 1 above at text )
            }
            checkBox1_CheckedChanged(null!, null!); // populate listBox1
            LoadObjLookup(); // populate BmolId on entries from OBJ.ojd
        }
        // this is a test method to parse the .nsb map files and log the results to a text file
        public void parseNSB()
        {
            listBox2.Items.Clear();
            listBox3.Items.Clear();
            int length = (levelData!.Length - 8) / 12; // minus header
            int position = 8;
            for(int i = 0; i < length; i++)
            {
                ushort type = (ushort)(levelData[position] | (levelData[position + 1] << 8));
                ushort x = (ushort)(levelData[position + 4] | (levelData[position + 5] << 8));
                ushort y = (ushort)(levelData[position + 8] | (levelData[position + 9] << 8));
                string name = BmolName(type);
                // Only show entries with a resolved OTYPE name; skip internal engine types
                if (!name.StartsWith("#"))
                {
                    listBox2.Items.Add($"{name} X:{x} Y:{y} 0x{position}");
                }
                else
                {
                    listBox3.Items.Add($"{name} X:{x} Y:{y} 0x{position}");
                }
                position += 12;
            }
        }

        //private string BmolName(int id) => entries.FirstOrDefault(e => e.BmolId == (ushort)id)?.Name ?? $"#{id}";
        private string BmolName(int id)
        {
            var entry = entries.FirstOrDefault(e => e.BmolId == (ushort)id);
            if (entry == null) return $"#{id}";
            // Prefer localised text name; fall back to OTYPE_ string; last resort is raw ID
            return !string.IsNullOrEmpty(entry.Name) ? entry.Name
                 : !string.IsNullOrEmpty(entry.OTypeName) ? entry.OTypeName
                 : $"#{id}";
        }
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            string levelName = $"DAT\\{listBox1.SelectedIndex + 1}.nsb";
            if (!File.Exists(levelName))
            {
                MessageBox.Show($"DAT\\{listBox1.SelectedIndex + 1}.nsb file missing, reinstall the game.");
                return;
            }
            levelData = File.ReadAllBytes($"DAT\\{listBox1.SelectedIndex + 1}.nsb");
            parseNSB();
        }
        // Add this method to SaveEditorForm.
        // Call it from the constructor after entries is populated and TEXT.ojd is parsed.
        // Requires OBJ.ojd alongside TEXT.ojd.
        //
        // OBJ.ojd structure around each OTYPE entry (offsets from 'O' of OTYPE_):
        //   -14: FF XX XX              (0F record start)
        //   -11: 0F 00                 (record type)
        //    -9: BMOL_lo BMOL_hi       (BMOL ID, uint16 LE)
        //    -7: FF XX XX              (10 record start)
        //    -4: 10 00                 (record type)
        //    -2: len 00                (string length including null)
        //     0: O T Y P E _ ...      (OTYPE_ string)
        //   +N+1: FF XX XX 15 00      (15 record, N = strlen including null)
        //   +N+6: TEXT_key_lo          (TEXT.ojd lookup key, uint16 LE)
        //   +N+7: TEXT_key_hi
        //
        // TEXT.ojd structure:
        //   FF XX XX 02 00 key_lo key_hi len 00 string\0

        private void LoadObjLookup()
        {
            if (!File.Exists("OBJ.ojd"))
                return;

            byte[] obj = File.ReadAllBytes("OBJ.ojd");
            byte[] otype = Encoding.ASCII.GetBytes("OTYPE_");

            for (int i = 14; i <= obj.Length - 6; i++)
            {
                // Match OTYPE_ at position i
                bool match = true;
                for (int m = 0; m < 6; m++)
                    if (obj[i + m] != otype[m]) { match = false; break; }
                if (!match) continue;

                // Validate 0F record 14 bytes before OTYPE_
                if (obj[i - 14] != 0xFF || obj[i - 11] != 0x0F || obj[i - 10] != 0x00)
                    continue;
                // Validate 10 record 7 bytes before OTYPE_
                if (obj[i - 7] != 0xFF || obj[i - 4] != 0x10 || obj[i - 3] != 0x00)
                    continue;

                ushort bmolId = (ushort)(obj[i - 9] | (obj[i - 8] << 8));

                // Read OTYPE_ string using length byte at i-2 (includes null terminator in count)
                int strLen = obj[i - 2] - 1; // exclude null terminator
                if (i + strLen > obj.Length) continue;
                string otypeStr = Encoding.ASCII.GetString(obj, i, strLen);

                ushort textKey = 0;
                bool hasTextKey = false;

                // 15 record immediately after the string+null (optional — many effect types have none)
                int r = i + strLen + 1;
                if (r + 7 < obj.Length && obj[r] == 0xFF && obj[r + 3] == 0x15 && obj[r + 4] == 0x00)
                {
                    textKey = (ushort)(obj[r + 5] | (obj[r + 6] << 8));
                    hasTextKey = true;
                }

                // Try to match an existing text entry via TEXT.ojd key
                WowTextEntry? entry = hasTextKey
                    ? entries.FirstOrDefault(e => e.ID == textKey && e.BmolId == null)
                    : null;

                if (entry != null)
                {
                    entry.BmolId = bmolId;
                    entry.OTypeName = otypeStr;
                }
                else
                {
                    // No text entry — add a fallback so BmolName can resolve this type
                    entries.Add(new WowTextEntry { BmolId = bmolId, OTypeName = otypeStr, Name = otypeStr });
                }
            }
        }

        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            // display x / y coordinates.
        }

        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            int index = listBox1.SelectedIndex;
            listBox1.SelectedIndexChanged -= listBox1_SelectedIndexChanged!;
            listBox1.Items.Clear();
            int start = checkBox1.Checked ? 31 : 0; // skip entry 0 (campaign name) in each faction group
            for (int i = 1; i < 31; i++)
            {
                listBox1.Items.Add(entries[start + i].Name);
            }
            listBox1.SelectedIndex = index;
            listBox1.SelectedIndexChanged += listBox1_SelectedIndexChanged!;
        }
    }
}
