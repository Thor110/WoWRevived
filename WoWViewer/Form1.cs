using System.Drawing.Imaging;
using System.Media;
using System.Text;
using WoWViewer;

namespace WOWViewer
{
    public partial class WOWViewer : Form
    {
        private SoundPlayer soundPlayer = null!;
        private string lastSelectedListItem = string.Empty;
        private string filePath = string.Empty;
        private string outputPath = string.Empty;
        private string magic = string.Empty;
        private int fileCount = 0;
        private List<WowFileEntry> entries = new List<WowFileEntry>();
        private WowFileEntry selected = null!;
        private Dictionary<string, Action<WowFileEntry>> handlers = null!;
        private bool outputPathSelected;
        private void InitializeHandlers()
        {
            handlers = new Dictionary<string, Action<WowFileEntry>>
            {
                //DAT/Dat.wow
                { "DAT", HandleDAT },
                { "FNT", HandleFonts },
                { "HSH", HandleHSH },
                { "HSM", HandleHSM },
                { "INT", HandleINT },
                { "IOB", HandleIOB },
                { "PAL", HandlePalette },
                { "RAW", HandleRAW },
                { "SHH", HandleSHH },
                { "SHL", HandleSHL },
                { "SHM", HandleSHM },
                { "SPR", HandleSPR },
                { "WOF", HandleWOF },
                //MAPS/MAPS.WoW
                { "ATM", HandleATM },
                { "CLS", HandleCLS },
                //SPR files also in MAPS.WoW
            };
        }
        //DAT/Dat.wow
        private void HandleDAT(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("DAT file selected. No action defined.");
        }
        private void HandleFonts(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("Fonts file selected. No action defined.");
        }
        private void HandleHSH(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("HSH file selected. No action defined.");
        }
        private void HandleHSM(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("HSM file selected. No action defined.");
        }
        private void HandleINT(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("INT file selected. No action defined.");
        }
        private void HandleIOB(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("IOB file selected. No action defined.");
        }
        private void HandlePalette(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("Palette file selected. No action defined.");
        }
        private void HandleRAW(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("RAW file selected. No action defined.");
        }
        private void HandleSHH(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("SHH file selected. No action defined.");
        }
        private void HandleSHL(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("SHL file selected. No action defined.");
        }
        private void HandleSHM(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("SHM file selected. No action defined.");
        }
        private void HandleSPR(WowFileEntry entry)
        {
            pictureBox1.Visible = true; // show the picture box
            RenderSPR(entry);
        }
        private void RenderSPR(WowFileEntry entry)
        {
            /* // disabled until SPR format is known or huffman encoding is implemented
            using var br = new BinaryReader(File.OpenRead(filePath));
            br.BaseStream.Seek(entry.Offset, SeekOrigin.Begin);

            string header = Encoding.ASCII.GetString(br.ReadBytes(4));
            if (header != "FFUH")
            {
                MessageBox.Show("Unsupported SPR format (no FFUH header).");
                return;
            }

            ushort width = br.ReadUInt16();   // Little endian
            ushort height = br.ReadUInt16();  // Little endian

            int imageSize = width * height * 3; // Assuming 24-bit RGB

            byte[] imageData = br.ReadBytes(imageSize);
            if (imageData.Length < imageSize)
            {
                MessageBox.Show("Incomplete image data.");
                return;
            }

            Bitmap bmp = new Bitmap(width, height, PixelFormat.Format24bppRgb);

            int index = 0;
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    byte r = imageData[index++];
                    byte g = imageData[index++];
                    byte b = imageData[index++];
                    bmp.SetPixel(x, y, Color.FromArgb(r, g, b));
                }
            }

            pictureBox1.Image = bmp;
            */
            pictureBox1.Image = null;
            MessageBox.Show("SPR file selected. No action defined.");
        }
        private void HandleWOF(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("WOF file selected. No action defined.");
        }
        //MAPS/MAPS.WoW
        private void HandleATM(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("ATM file selected. No action defined.");
        }
        private void HandleCLS(WowFileEntry entry)
        {
            pictureBox1.Image = null;
            MessageBox.Show("CLS file selected. No action defined.");
        }
        public WOWViewer()
        {
            InitializeComponent();
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(ListBox), typeof(Label) });
            InitializeHandlers();
            //parseTEXTOJD();
            //parseSFXOBJOJD("OBJ");
        }
        // this is a test method to parse the TEXT.OJD file and log the results to a text file
        public void parseTEXTOJD()
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
        }
        // this is a test method to parse the OBJ + SFX.OJD file and log the results to a text file
        public void parseSFXOBJOJD(string filename)
        {
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
                            if (!IsAsciiChar(data[offset]))
                                break;

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
                                count++;
                            }
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
        }
        // open file
        private void button1_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog openFileDialog = new OpenFileDialog())
            {
                openFileDialog.InitialDirectory = "C:\\Program Files (x86)\\Jeff Wayne's 'The War Of The Worlds'";
                openFileDialog.Filter = "WoW Container (*.wow)|*.wow|All Files (*.*)|*.*";
                openFileDialog.FilterIndex = 1;
                openFileDialog.RestoreDirectory = true;
                openFileDialog.Title = "Select a Container (.wow) file";

                if (openFileDialog.ShowDialog() == DialogResult.OK)
                {
                    filePath = openFileDialog.FileName;
                    textBox1.Text = filePath;
                    parseFileCount();
                }
            }
        }
        // extract selected file
        private void button2_Click(object sender, EventArgs e) { extractFile(false); }
        // select file output folder
        private void button3_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog folderBrowserDialog = new FolderBrowserDialog();
            if (folderBrowserDialog.ShowDialog() == DialogResult.OK)
            {
                outputPath = folderBrowserDialog.SelectedPath;
                if (!outputPath.EndsWith("\\")) // If Not Root Directory
                {
                    outputPath += "\\"; // Complete Directory String
                }
                textBox2.Text = outputPath;
                button4.Enabled = true; // Enable extract all button
                if(listBox1.SelectedIndex != -1)
                {
                    button2.Enabled = true; // Enable extract button if a file is selected
                }
                outputPathSelected = true;
            }
        }
        // extract all files
        private void button4_Click(object sender, EventArgs e) { extractFile(true); }
        // extracted single or multiple files
        private void extractFile(bool multiple)
        {
            bool isWav;
            if (magic == "KAT!") { isWav = false; }
            else if (magic == "SfxL") { isWav = true; }
            else { return; } // invalid file type
            if (multiple)
            {
                foreach (var entry in entries) { ExtractToFile(entry, isWav); }
                MessageBox.Show("All files extracted successfully.");
            }
            else
            {
                ExtractToFile(selected, isWav);
                MessageBox.Show($"Extracted: {listBox1.SelectedItem!.ToString()}");
            }
        }
        // parse file count method
        private void parseFileCount()
        {
            using (BinaryReader br = new BinaryReader(File.OpenRead(filePath)))
            {
                if (!ReadHeader(br))
                {
                    MessageBox.Show("Invalid or unsupported file.");
                    return;
                }
            }
        }
        // read header method
        private bool ReadHeader(BinaryReader br)
        {
            label3.Text = $"File Size :"; // reset labels
            label4.Text = $"File Offset :";


            magic = new string(br.ReadChars(4));
            if (magic != "KAT!" && magic != "SfxL")
                return false;

            fileCount = br.ReadInt32();                 // file count
            label1.Text = "File Count : " + fileCount.ToString();

            entries.Clear();
            listBox1.Items.Clear();

            if (magic == "KAT!")
            {
                label2.Text = "Container Type : " + "Data or Maps";
                for (int i = 0; i < fileCount; i++)
                {
                    br.ReadInt32();                     // skip 4 bytes
                    int offset = br.ReadInt32();        // file offset
                    int length = br.ReadInt32();        // file size
                    byte[] nameBytes = br.ReadBytes(12);// filename (ASCII padded)
                    // skip 20 bytes
                    br.BaseStream.Seek(20, SeekOrigin.Current);
                    // setup list
                    int zeroIndex = Array.IndexOf(nameBytes, (byte)0);
                    string name = Encoding.ASCII.GetString(nameBytes, 0, zeroIndex >= 0 ? zeroIndex : nameBytes.Length);
                    listBox1.Items.Add($"{name}");
                    entries.Add(new WowFileEntry { Name = name, Length = length, Offset = offset });
                }
                button5.Visible = false; // hide audio player play button
                button6.Visible = false; // hide audio player stop button
                pictureBox1.Visible = false; // hide the picture box
                label5.Visible = false; // hide sound length label
            }
            else if (magic == "SfxL")
            {
                label2.Text = "Container Type : " + "Sound Effects Library";
                // filenames are up to 8 bytes in these file
                for (int i = 0; i < fileCount; i++)
                {
                    byte[] nameBytes = br.ReadBytes(8); // filename (ASCII padded)
                    int length = br.ReadInt32();        // file size
                    int offset = br.ReadInt32();        // file offset
                    // setup list
                    string name = Encoding.ASCII.GetString(nameBytes).TrimEnd('\0');
                    listBox1.Items.Add($"{name}.WAV");
                    entries.Add(new WowFileEntry { Name = name, Length = length, Offset = offset });
                }
                button5.Visible = true; // show audio player play button
                button6.Visible = true; // show audio player stop button
                pictureBox1.Visible = true; // show the picture box
                label5.Visible = true; // show sound length label
            }
            button2.Enabled = false; // disable extract button
            button5.Enabled = false; // disable play button
            button6.Enabled = false; // disable stop button
            return true;
        }
        // create wav header
        public static byte[] CreateWavHeader(int dataSize, int sampleRate = 22050, short bitsPerSample = 16, short channels = 1)
        {
            int byteRate = sampleRate * channels * bitsPerSample / 8;
            short blockAlign = (short)(channels * bitsPerSample / 8);
            int chunkSize = 36 + dataSize;

            using (MemoryStream ms = new MemoryStream(44))
            using (BinaryWriter bw = new BinaryWriter(ms))
            {
                bw.Write(Encoding.ASCII.GetBytes("RIFF"));
                bw.Write(chunkSize);
                bw.Write(Encoding.ASCII.GetBytes("WAVE"));
                bw.Write(Encoding.ASCII.GetBytes("fmt "));
                bw.Write(16); // PCM
                bw.Write((short)1); // format = PCM
                bw.Write(channels);
                bw.Write(sampleRate);
                bw.Write(byteRate);
                bw.Write(blockAlign);
                bw.Write(bitsPerSample);
                bw.Write(Encoding.ASCII.GetBytes("data"));
                bw.Write(dataSize);
                return ms.ToArray();
            }
        }
        // detect sample rate
        private int DetectSampleRate(byte[] rawData)
        {
            int sampleCount = rawData.Length / 2;

            // Calculate duration at both sample rates
            double duration22050 = sampleCount / 22050.0;
            double duration44100 = sampleCount / 44100.0;

            if (rawData.Length > 100000 && duration22050 > 30)
                return 44100;
            // If the file would be unusually long at 22050Hz, it's probably 44100Hz
            if (duration22050 > 30 && duration44100 < 20)
                return 44100;

            // Otherwise, assume default (most are voice clips)
            return 22050;
        }
        // extract to file method
        private void ExtractToFile(WowFileEntry entry, bool asWav = false)
        {
            using var br = new BinaryReader(File.OpenRead(filePath));
            br.BaseStream.Seek(entry.Offset, SeekOrigin.Begin);
            byte[] rawData = br.ReadBytes(entry.Length);
            string filename = asWav ? $"{entry.Name}.wav" : entry.Name;
            using var fs = new FileStream(Path.Combine(outputPath, filename), FileMode.Create);

            if (asWav)
            {
                int sampleRate = DetectSampleRate(rawData);
                byte[] wavHeader = CreateWavHeader(rawData.Length, sampleRate);
                fs.Write(wavHeader, 0, wavHeader.Length);
            }
            //if entry.Name ends with extension .RAW, .SHH, .SHL, .SHM, .SPR, .WOF etc add relevant header when they are determined and implemented.
            fs.Write(rawData, 0, rawData.Length);
        }
        // listbox selection changed
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            selected = entries[listBox1.SelectedIndex];
            if (selected.Name != lastSelectedListItem) // check selection is new
            {
                label3.Text = $"File Size : {selected.Length} bytes";
                label4.Text = $"File Offset : {selected.Offset} bytes";
                // display waveform image if browsing SfxL container
                if (magic == "SfxL")
                {
                    // load the waveform image
                    using var br = new BinaryReader(File.OpenRead(filePath));
                    br.BaseStream.Seek(selected.Offset, SeekOrigin.Begin);
                    byte[] rawData = br.ReadBytes(selected.Length);
                    int sampleRate = DetectSampleRate(rawData);
                    byte[] wavHeader = CreateWavHeader(rawData.Length, sampleRate);
                    using var ms = new MemoryStream();
                    ms.Write(wavHeader, 0, wavHeader.Length);
                    ms.Write(rawData, 0, rawData.Length);
                    ms.Position = 0;
                    byte[] fullWav = ms.ToArray();
                    pictureBox1.Image = DrawWaveform(fullWav, 156, 137, sampleRate);
                    button5.Enabled = true; // Enable play button
                    button6.Enabled = true; // Enable stop button
                }
                else if (magic == "KAT!")
                {
                    string ext = listBox1.SelectedItem!.ToString()!.Split('.')[1];
                    if (handlers.ContainsKey(ext))
                    {
                        handlers[ext].Invoke(selected);
                    }
                }
                lastSelectedListItem = selected.Name;
                if(outputPathSelected)
                {
                    button2.Enabled = true; // Enable extract button
                }
            }
        }
        // play sound button
        private void button5_Click(object sender, EventArgs e) { PlayRawSound(selected); }
        // play sound method
        private void PlayRawSound(WowFileEntry entry)
        {
            using var br = new BinaryReader(File.OpenRead(filePath));
            br.BaseStream.Seek(entry.Offset, SeekOrigin.Begin);
            byte[] rawData = br.ReadBytes(entry.Length);
            int sampleRate = DetectSampleRate(rawData);
            byte[] wavHeader = CreateWavHeader(rawData.Length, sampleRate);
            using var ms = new MemoryStream();
            ms.Write(wavHeader, 0, wavHeader.Length);
            ms.Write(rawData, 0, rawData.Length);
            ms.Position = 0;
            try
            {
                soundPlayer?.Stop(); // stop any currently playing sound
                soundPlayer = new SoundPlayer(ms);
                soundPlayer.Play();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error playing sound: {ex.Message}");
            }
        }
        // stop sound button
        private void button6_Click(object sender, EventArgs e) { soundPlayer?.Stop(); }
        // double click to play sound
        private void listBox1_MouseDoubleClick(object sender, MouseEventArgs e)
        {
            if (magic == "SfxL") { PlayRawSound(selected); }
            //else if (magic == "KAT!") { MessageBox.Show(selected.Name); }
        }
        // draw waveform
        private Bitmap DrawWaveform(byte[] wavData, int width, int height, int sampleRate)
        {
            var bmp = new Bitmap(width, height);
            using var g = Graphics.FromImage(bmp);
            g.Clear(Color.Black);
            short[] samples = Extract16BitMonoSamples(wavData, sampleRate);
            int samplesPerPixel = samples.Length / width;
            Pen colour = Pens.White;
            if (filePath.Contains("sfx")) { colour = Pens.Yellow; }
            else if (filePath.Contains("human")) { colour = Pens.Red; }
            else if (filePath.Contains("martian")) { colour = Pens.LimeGreen; }
            for (int x = 0; x < width; x++)
            {
                int start = x * samplesPerPixel;
                int max = 0;
                for (int i = 0; i < samplesPerPixel && (start + i) < samples.Length; i++)
                {
                    int val = Math.Abs((int)samples[start + i]);
                    if (val > max) max = val;
                }
                float normalized = max / (float)short.MaxValue;
                int y = (int)(normalized * height / 2);
                g.DrawLine(colour, x, height / 2 - y, x, height / 2 + y);
            }
            return bmp;
        }
        // extract 16-bit mono samples from WAV data
        private short[] Extract16BitMonoSamples(byte[] wavData, int sampleRate)
        {
            using var ms = new MemoryStream(wavData);
            using var br = new BinaryReader(ms);
            br.BaseStream.Seek(44, SeekOrigin.Begin); // skip WAV header
            int sampleCount = (wavData.Length - 44) / 2; // calculate number of samples
            short[] samples = new short[sampleCount]; // create array for samples
            for (int i = 0; i < sampleCount; i++) { samples[i] = br.ReadInt16(); } // build samples array
            double duration = sampleCount / (double)sampleRate; // calculate sound length
            // update sound length label
            if (duration > 60) // only calculate minutes and seconds if necessary
            {
                int minutes = (int)duration / 60;
                int seconds = (int)duration % 60;
                label5.Text = $"Sound Length : {minutes:D2}:{seconds:D2}";
            }
            else { label5.Text = $"Sound Length : {duration:F2} seconds"; }
            return samples;
        }
        // save editor
        private void button7_Click(object sender, EventArgs e)
        {
            var saveEditor = new SaveEditorForm();
            saveEditor.StartPosition = FormStartPosition.Manual;
            saveEditor.Location = this.Location;
            saveEditor.Show();
            this.Hide();
            saveEditor.FormClosed += (s, args) => this.Show();
            saveEditor.Move += (s, args) =>
            {
                if (this.Location != saveEditor.Location)
                {
                    this.Location = saveEditor.Location;
                }
            };
        }
        // map editor
        private void button8_Click(object sender, EventArgs e)
        {
            var mapEditor = new MapEditorForm();
            mapEditor.StartPosition = FormStartPosition.Manual;
            mapEditor.Location = this.Location;
            mapEditor.Show();
            this.Hide();
            mapEditor.FormClosed += (s, args) => this.Show();
            mapEditor.Move += (s, args) =>
            {
                if (this.Location != mapEditor.Location)
                {
                    this.Location = mapEditor.Location;
                }
            };
        }
        // open text editor window
        private void button9_Click(object sender, EventArgs e)
        {
            var textEditor = new TextEditorForm();
            textEditor.StartPosition = FormStartPosition.Manual;
            textEditor.Location = this.Location;
            textEditor.Show();
            this.Hide();
            textEditor.FormClosed += (s, args) => this.Show();
            textEditor.Move += (s, args) =>
            {
                if (this.Location != textEditor.Location)
                {
                    this.Location = textEditor.Location;
                }
            };
        }
    }
}