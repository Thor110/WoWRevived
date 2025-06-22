using System.Text;

namespace WoWViewer
{
    public partial class OJDParser : Form
    {
        private List<OjdEntry> entries = new List<OjdEntry>();
        public OJDParser()
        {
            InitializeComponent();
        }
        // this is a test method to parse the TEXT.OJD file and log the results to a text file
        /*public void parseTEXTOJD() // no longer used, but kept for reference. // results will be used to create a script to search through the assembly for string references
        {
            listBox1.Items.Clear();
            string inputPath = "TEXT.ojd";
            string outputPath = "text-ojd-log.txt";
            byte[] data = File.ReadAllBytes(inputPath);
            using (StreamWriter log = new StreamWriter(outputPath, false, Encoding.UTF8))
            {
                int offset = 0x289; // first string starts at 0x289
                //int count = 0; // count checker for total number of entries
                for (int i = 0; i < 1396; i++) // there are only 1396 entries
                {
                    ushort lookupID = (ushort)(data[offset + 2] | (data[offset + 3] << 8));
                    byte category = data[offset + 4];  // Faction: 00 = Martian, 01 = Human, 02 = UI
                    ushort purposeID = (ushort)(data[offset + 6] | (data[offset + 7] << 8));
                    ushort length = (ushort)(data[offset + 8] | (data[offset + 9] << 8)); // bytes 9 and 10 are the string length
                    int stringOffset = offset + 10; // string offset
                    string text = Encoding.ASCII.GetString(data, stringOffset, length - 1); // string length is one less than the byte length
                    string faction =
                        category == 0x00 ? "Martian" :
                        category == 0x01 ? "Human" :
                        category == 0x02 ? "UI" : "Unknown"; // faction type or user interface
                    //00 FF + LookupID + Faction + PurposeID + String Length ( Each 2 Bytes )
                    log.WriteLine($"Offset : [{offset:X}] : Unknown : [{lookupID:D}] : Faction : [{faction}] : String ID : {purposeID:D} : String Length : [{length:D}] : Text : {text}");
                    //log.WriteLine($"{stringID:X}h : {text}");
                    listBox1.Items.Add(text);
                    offset += length + 9; // move offset to next entry // not + 10 because length contains the null operator ( hence - 1 above at text )
                    //count++; // increase count checker
                }
                //log.WriteLine($"Total valid entries: {count}"); // log the total number of entries
            }
            label1.Text = $"Total Strings: 1396"; // known total number of entries in TEXT.ojd
        }*/
        // this is a test method to parse the SFX.OJD file and log the results to a text file
        public void parseSFXOJD(string filename)
        {
            listBox1.Items.Clear();
            string logPath = Path.ChangeExtension(filename, "-dump.csv");
            byte[] data = File.ReadAllBytes(filename);
            int count = 0;

            using (StreamWriter log = new StreamWriter(logPath, false, Encoding.UTF8))
            {
                log.WriteLine("Index,Offset,HeaderID,Length,Type,Text");

                for (int i = 0; i < data.Length - 1; i++)
                {
                    if (!IsAsciiChar(data[i]) || data[i + 1] == 0x00) continue;

                    int start = i;
                    int length = 0;

                    while (i < data.Length && IsAsciiChar(data[i])) { i++; length++; }
                    if (length < 2 || i >= data.Length || data[i] != 0x00) continue;

                    // Candidate string found
                    int stringOffset = start;
                    string text = Encoding.ASCII.GetString(data, stringOffset, length);

                    // Try to backtrack
                    int headerOffset = stringOffset - 7;
                    string type = "Unverified";
                    string headerID = "??";

                    if (headerOffset >= 0 && data[headerOffset] == 0xFF)
                    {
                        ushort id = BitConverter.ToUInt16(data, headerOffset + 1);
                        ushort maybeLength = BitConverter.ToUInt16(data, headerOffset + 5);

                        headerID = id.ToString("X4");

                        if (maybeLength == length + 1)
                        {
                            type = "StringEntry";
                            listBox1.Items.Add(text);
                        }
                        else
                        {
                            type = "MismatchedLength";
                        }
                    }

                    log.WriteLine($"{count},{stringOffset:X},{headerID},{length},{type},\"{text}\"");
                    count++;
                }

                label1.Text = $"Total Strings: {count}";
            }

            //bool IsAsciiChar(byte b) => b >= 0x20 && b <= 0x7E;
        }
        private static bool IsAsciiChar(byte b) => b >= 0x20 && b <= 0x7E;
        // this is a test method to parse the OBJ.OJD file and log the results
        public void parseOBJOJD()
        {
            listBox1.Items.Clear();
            entries = ParseOjdFile();
            string logPath = "ojd_log.txt";
            if (File.Exists(logPath)) { File.Delete(logPath); }
            foreach (var entry in entries)
            {
                listBox1.Items.Add(entry.Name);
                File.AppendAllText(logPath, $"{entry}\n");
            }
            label1.Text = $"Total Entries: {entries.Count}";
        }
        // This method parses the OJD file and returns a list of OjdEntry objects
        public static List<OjdEntry> ParseOjdFile()
        {
            var entries = new List<OjdEntry>();
            byte[] data = File.ReadAllBytes("OBJ.ojd");
            int index = 0;
            while (index < data.Length)
            {
                if (data[index] == 0xFF)
                {
                    ushort id = BitConverter.ToUInt16(data, index + 1);
                    ushort type = BitConverter.ToUInt16(data, index + 3);
                    ushort length = BitConverter.ToUInt16(data, index + 5);

                    int strStart = index + 7;

                    if (strStart < data.Length && data[strStart] == 0xFF)
                    {
                        index += 1;
                        continue;
                    }
                    if (!IsAsciiChar(data[strStart]))
                    {
                        index += 1;
                        continue;
                    }
                    if (length == 0 || strStart + length > data.Length)
                    {
                        index += 1;
                        continue;
                    }
                    if (length >= 33 || length <= 7)
                    {
                        index += 1;
                        continue;
                    }

                    int strEnd = Array.IndexOf(data, (byte)0x00, strStart);

                    string name = Encoding.ASCII.GetString(data, strStart, strEnd - strStart);

                    name = name.Replace("\n", "").Replace("\r", "").Replace("\t", "").Replace("\x1A", "").Replace("\0", "");

                    if(String.IsNullOrWhiteSpace(name))
                    {
                        index += 1;
                        continue;
                    }

                    entries.Add(new OjdEntry
                    {
                        Id = id,
                        Type = type,
                        Length = length,
                        Name = name
                    });

                    index = strEnd; // Move to next possible entry
                }
                else
                {
                    index++; // Keep scanning for 0xFF
                }
            }
            //WriteCleanedOjdWithInjectedPadding(); // at \WoWRevived\WoWPatches\theory
            //theory on obj.ojd malformed entries
            return entries;
        }
        public static void WriteCleanedOjdWithInjectedPadding()
        {
            byte[] data = File.ReadAllBytes("OBJ.ojd");
            List<byte> newData = new List<byte>();

            // Preserve the initial 0x411 bytes
            newData.AddRange(data.Take(0x431));

            int index = 0x431;
            while (index < data.Length)
            {
                if (data[index] != 0xFF)
                {
                    index++;
                    continue;
                }

                if (index + 3 >= data.Length)
                    break;

                ushort id = BitConverter.ToUInt16(data, index + 1);
                ushort type = BitConverter.ToUInt16(data, index + 3);
                ushort len = BitConverter.ToUInt16(data, index + 5);

                int strStart = index + 7;
                if (len == 0 || strStart + len > data.Length)
                {
                    index++;
                    continue;
                }

                // Copy: FF + 6-byte header
                newData.AddRange(data.Skip(index).Take(7));

                // Copy: ASCII string + null
                newData.AddRange(data.Skip(strStart).Take(len));

                // Inject: FF 00 (junk)
                newData.Add(0xFF);
                newData.Add(0x00);

                index = strStart + len;
            }

            File.WriteAllBytes("OBJ-TEST.ojd", newData.ToArray());
        }
        // both methods return 755 entries for SFX.ojd
        // seems more accurate for OBJ.ojd
        // OBJ.ojd
        private void button1_Click(object sender, EventArgs e) { parseOBJOJD(); } // 2072 entries
        // SFX.ojd
        private void button2_Click(object sender, EventArgs e) { parseSFXOJD("SFX.ojd"); } // 755 entries
        // TEXT.ojd
        private void button3_Click(object sender, EventArgs e) { MessageBox.Show("TEXT.ojd file fully decoded!"); } // 1396 Entries ( 0 - 1395 )
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            textBox1.Text = entries[listBox1.SelectedIndex].Id.ToString();          // ID
            textBox2.Text = entries[listBox1.SelectedIndex].Type.ToString();        // Type
            textBox3.Text = entries[listBox1.SelectedIndex].Length.ToString();      // Flags
            textBox4.Text = entries[listBox1.SelectedIndex].Name;                   // Path
        }
    }
}
