using System.Text;

namespace WoWViewer
{
    public partial class OJDParser : Form
    {
        public OJDParser()
        {
            InitializeComponent();
        }
        // this is a test method to parse the TEXT.OJD file and log the results to a text file
        /*public void parseTEXTOJD() // no longer used, but kept for reference.
        {
            string inputPath = "TEXT.ojd";
            string outputPath = "text-ojd-log.txt";
            byte[] data = File.ReadAllBytes(inputPath);
            using (StreamWriter log = new StreamWriter(outputPath, false, Encoding.UTF8))
            {
                int offset = 0x289; // first string starts at 0x289
                //int count = 0; // count checker for total number of entries
                for (int i = 0; i < 1396; i++) // there are only 1396 entries
                {
                    byte buttonID = data[offset + 2]; // button type???
                    byte category = data[offset + 4];  // Faction: 00 = Martian, 01 = Human, 02 = UI
                    byte buttonFunction = data[offset + 6]; // button function??
                    ushort length = (ushort)(data[offset + 8] | (data[offset + 9] << 8)); // bytes 9 and 10 are the string length
                    int stringOffset = offset + 10; // string offset
                    string text = Encoding.ASCII.GetString(data, stringOffset, length - 1); // string length is one less than the byte length
                    string faction =
                        category == 0x00 ? "Martian" :
                        category == 0x01 ? "Human" :
                        category == 0x02 ? "UI" : "Unknown"; // faction type or user interface
                    log.WriteLine($"{i:D4} [{faction}] : {text} : Offset : [{offset:X}] : Button ID : [{buttonID:X2}] : Button Function : [{buttonFunction:X2}]");
                    offset += length + 9; // move offset to next entry // not + 10 because length contains the null operator ( hence - 1 above at text )
                    //count++; // increase count checker
                }
                //log.WriteLine($"Total valid entries: {count}"); // log the total number of entries
            }
        }*/
        // this is a test method to parse the OBJ + SFX.OJD file and log the results to a text file
        /*public void parseSFXOBJOJD(string filename) // no longer used, but kept for reference.
        {
            listBox1.Items.Clear();

            string inputPath = $"{filename}.ojd";
            string outputPath = $"{filename}-ojd-log.txt";
            byte[] data = File.ReadAllBytes(inputPath);
            using (StreamWriter log = new StreamWriter(outputPath, false, Encoding.UTF8))
            {
                int offset = 0;
                int count = 0;
                while (offset < data.Length - 1)
                {
                    // Look for null-terminated ASCII strings
                    if (IsAsciiChar(data[offset]) && data[offset + 1] != 0x00)
                    {
                        int start = offset;
                        int length = 0;
                        while (offset < data.Length && data[offset] != 0x00)
                        {
                            if (!IsAsciiChar(data[offset])) { break; }
                            offset++;
                            length++;
                        }
                        if (length > 1 && offset < data.Length && data[offset] == 0x00)
                        {
                            // Potential string found
                            int stringStart = start;
                            string text = Encoding.ASCII.GetString(data, stringStart, length); // get string
                            text = text.TrimStart('\uFEFF'); // trim Zero Width No-Break Space (ZWNBSP) Byte Order Mark (BOM) if present
                            // Attempt to backtrack a 7–10 byte header
                            int headerOffset = stringStart - 7;
                            if (headerOffset >= 0 && data[headerOffset] == 0xFF)
                            {
                                byte id = data[headerOffset + 1];
                                ushort maybeLen = BitConverter.ToUInt16(data, headerOffset + 5); // Usually equals string length
                                log.WriteLine($"{count:D4} [{id:X2}] : {text} : Offset : [{headerOffset:X}] : Length (maybe): {maybeLen}");
                                //log.WriteLine($"{text.ToUpperInvariant()}");
                                listBox1.Items.Add(text.ToUpperInvariant()); // add to listbox
                                count++;
                            }
                            label1.Text = $"Total Entries : {count}"; // update label with total entries
                        }
                        offset++; // Move to next byte after null
                    }
                    else
                    {
                        offset++;
                    }
                }
                log.WriteLine($"Total parsed entries: {count}");
            }
            bool IsAsciiChar(byte b)
            {
                return b >= 0x20 && b <= 0x7E;
            }
        }*/
        // this is a test method to parse the OBJ + SFX.OJD file and log the results to a text file
        public void parseSFX(string filename)
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

            bool IsAsciiChar(byte b) => b >= 0x20 && b <= 0x7E;
        }
        // both methods return 755 entries for SFX.ojd
        // seems more accurate for OBJ.ojd
        // OBJ.ojd
        private void button1_Click(object sender, EventArgs e) { parseSFX("OBJ.ojd"); } // 2072 entries
        // SFX.ojd
        private void button2_Click(object sender, EventArgs e) { parseSFX("SFX.ojd"); } // 755 entries
    }
}
