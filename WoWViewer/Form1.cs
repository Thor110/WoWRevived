using System.Media;
using System.Text;

namespace WoWViewer
{
    public partial class WoWViewer : Form
    {
        private SoundPlayer soundPlayer = null!;
        private string lastSelectedListItem = string.Empty;
        private string filePath = string.Empty;
        private string outputPath = string.Empty;
        private string magic = string.Empty;
        private List<WowFileEntry> entries = new List<WowFileEntry>();
        private WowFileEntry selected = null!;
        private Dictionary<string, Action<WowFileEntry>> handlers = null!;
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
        public WoWViewer()
        {
            InitializeComponent();
            listBox1.DrawMode = DrawMode.OwnerDrawFixed;
            listBox1.DrawItem += ListBox1_DrawItem!;
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(ListBox), typeof(Label) });
            InitializeHandlers();
        }
        // for the listBox draw item event to change the color of the text if an entry is edited
        private void ListBox1_DrawItem(object sender, DrawItemEventArgs e)
        {
            if (e.Index < 0 || e.Index >= listBox1.Items.Count) { return; } // Check if index is valid before drawing
            e.DrawBackground();
            string itemText = listBox1.Items[e.Index].ToString()!;
            int entryIndex = e.Index;
            // Determine proper color
            Color textColor = (e.State & DrawItemState.Selected) == DrawItemState.Selected
                ? SystemColors.HighlightText
                : entries[entryIndex].Edited ? Color.Red
                : SystemColors.WindowText;
            // Use TextRenderer instead of Graphics.DrawString for better alignment and kerning
            TextRenderer.DrawText(e.Graphics, itemText, e.Font, e.Bounds, textColor, TextFormatFlags.VerticalCenter);
            e.DrawFocusRectangle();
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
                if (openFileDialog.ShowDialog() == DialogResult.OK) { openFile(openFileDialog.FileName); }
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
                if (listBox1.SelectedIndex != -1) { button2.Enabled = true; } // Enable extract button if a file is selected
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
            if (multiple) // extra check for reduced code duplication - inlining would result in faster execution time by a millionth of a second or so
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
            magic = new string(br.ReadChars(4));        // archive type
            if (magic != "KAT!" && magic != "SfxL") { return false; } // return false if not a valid archive type
            label3.Text = $"File Size :";               // reset labels
            label4.Text = $"File Offset :";
            int fileCount = br.ReadInt32();             // check file count
            label1.Text = "File Count : " + fileCount.ToString();
            entries.Clear();                            // clear entries and listbox
            listBox1.Items.Clear();
            if (magic == "KAT!") // read file entries based on archive type
            {
                label2.Text = "Container Type : " + "Data or Maps"; // update container type label
                for (int i = 0; i < fileCount; i++)
                {
                    br.ReadInt32();                     // skip 4 bytes
                    int offset = br.ReadInt32();        // file offset
                    int length = br.ReadInt32();        // file size
                    byte[] nameBytes = br.ReadBytes(12);// filename (ASCII padded)
                    br.BaseStream.Seek(20, SeekOrigin.Current); // skip 20 bytes
                    int zeroIndex = Array.IndexOf(nameBytes, (byte)0); // setup entries and listbox
                    string name = Encoding.ASCII.GetString(nameBytes, 0, zeroIndex >= 0 ? zeroIndex : nameBytes.Length);
                    listBox1.Items.Add($"{name}");
                    entries.Add(new WowFileEntry { Name = name, Length = length, Offset = offset });
                }
                button5.Visible = false;                // hide audio player play button
                button6.Visible = false;                // hide audio player stop button
                pictureBox1.Visible = false;            // hide the picture box
                label5.Visible = false;                 // hide sound length label
                button10.Visible = false;               // hide replace file button
                button11.Visible = false;               // hide save file button
                checkBox1.Visible = false;              // hide checkbox for backing up the original file ( for now )
            }
            else if (magic == "SfxL")
            {
                label2.Text = "Container Type : " + "Sound Effects Library"; // update container type label
                for (int i = 0; i < fileCount; i++)
                {
                    byte[] nameBytes = br.ReadBytes(8); // filename (ASCII padded) // filenames are up to 8 bytes in these file
                    int length = br.ReadInt32();        // file size
                    int offset = br.ReadInt32();        // file offset
                    // setup entries and listbox
                    string name = Encoding.ASCII.GetString(nameBytes).TrimEnd('\0');
                    listBox1.Items.Add($"{name}.WAV");
                    entries.Add(new WowFileEntry { Name = name, Length = length, Offset = offset });
                }
                button5.Visible = true;                 // show audio player play button
                button6.Visible = true;                 // show audio player stop button
                pictureBox1.Visible = true;             // show the picture box
                label5.Visible = true;                  // show sound length label
                button10.Visible = true;                // show replace file button
                button11.Visible = true;                // show save file button
                checkBox1.Visible = true;               // enable checkbox for backing up the original file
            }
            button2.Enabled = false;                    // disable extract button
            button5.Enabled = false;                    // disable play button
            button6.Enabled = false;                    // disable stop button
            button10.Enabled = false;                   // disable replace file button
            button11.Enabled = false;                   // disable save file button
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
            int sampleCount = rawData.Length / 2; // calculate sample count
            double duration22050 = sampleCount / 22050.0; // calculate duration at both sample rates
            if (rawData.Length > 100000 && duration22050 > 30) { return 44100; } // if the file would be unusually long at 22050Hz, it's probably 44100Hz
            double duration44100 = sampleCount / 44100.0; // calculate duration at both sample rates
            if (duration44100 < 10) { return 22050; } // if the file is less than 1 minute probably 22050hz
            if (duration22050 < 30 && duration44100 < 20) { return 44100; } // if the file would be unusually long at 22050Hz, it's probably 44100Hz
            return 22050; // otherwise, assume default (most are voice clips)
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
                byte[] wavHeader = CreateWavHeader(rawData.Length, DetectSampleRate(rawData));
                fs.Write(wavHeader, 0, wavHeader.Length);
            }
            //if entry.Name ends with extension .RAW, .SHH, .SHL, .SHM, .SPR, .WOF etc add relevant header when they are determined and implemented.
            fs.Write(rawData, 0, rawData.Length);
        }
        // listbox selection changed
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            selected = entries[listBox1.SelectedIndex]; // update selected entry
            if (selected.Name != lastSelectedListItem) // check selection is new
            {
                label3.Text = $"File Size : {selected.Length} bytes"; // update file size label
                if (selected.Offset == 0) { label4.Text = $"File Offset : Unknown"; } // update file offset label
                else { label4.Text = $"File Offset : {selected.Offset} bytes"; } // update file offset label
                if (magic == "SfxL") // load and display the waveform image if browsing SfxL container
                {
                    using var br = new BinaryReader(File.OpenRead(filePath));
                    br.BaseStream.Seek(selected.Offset, SeekOrigin.Begin);
                    byte[] rawData = br.ReadBytes(selected.Length);
                    int sampleRate = DetectSampleRate(rawData);
                    byte[] wavHeader = CreateWavHeader(rawData.Length, sampleRate);
                    byte[] fullWav; // if edited get data else read data
                    if (entries[listBox1.SelectedIndex].Edited) { fullWav = entries[listBox1.SelectedIndex].Data!; }
                    else
                    {
                        using var ms = new MemoryStream();
                        ms.Write(wavHeader, 0, wavHeader.Length);
                        ms.Write(rawData, 0, rawData.Length);
                        ms.Position = 0;
                        fullWav = ms.ToArray();
                    }
                    pictureBox1.Image = DrawWaveform(fullWav, 156, 137, sampleRate);
                    button5.Enabled = true; // enable play button
                    button6.Enabled = true; // enable stop button
                    button10.Enabled = true; // enable replace file button
                } // invoke extension handler for displaying different types
                else if (magic == "KAT!") { handlers[listBox1.SelectedItem!.ToString()!.Split('.')[1]].Invoke(selected); }
                if (button4.Enabled) { button2.Enabled = true; } // enable extract button
                lastSelectedListItem = selected.Name; // update last selected item
            }
        }
        // play sound button
        private void button5_Click(object sender, EventArgs e) { PlayRawSound(selected); }
        // play sound method
        private void PlayRawSound(WowFileEntry entry)
        {
            using var br = new BinaryReader(File.OpenRead(filePath));
            br.BaseStream.Seek(entry.Offset, SeekOrigin.Begin);
            byte[] rawData; // if edited get data else read data
            if (entries[listBox1.SelectedIndex].Edited) { rawData = entries[listBox1.SelectedIndex].Data!; }
            else { rawData = br.ReadBytes(entry.Length); }
            byte[] wavHeader = CreateWavHeader(rawData.Length, DetectSampleRate(rawData));
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
            catch (Exception ex) { MessageBox.Show($"Error playing sound: {ex.Message}"); }
        }
        // stop sound button
        private void button6_Click(object sender, EventArgs e) { soundPlayer?.Stop(); }
        // double click to play sound
        private void listBox1_MouseDoubleClick(object sender, MouseEventArgs e)
        {
            if (magic == "SfxL") { PlayRawSound(selected); }
            else if (magic == "KAT!") { MessageBox.Show(selected.Name); } // temporary placeholder
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
            if (duration > 60) // update sound length label
            {
                int minutes = (int)duration / 60; // only calculate minutes and seconds if necessary
                int seconds = (int)duration % 60;
                label5.Text = $"Sound Length : {minutes:D2}:{seconds:D2}";
            }
            else { label5.Text = $"Sound Length : {duration:F2} seconds"; }
            return samples;
        }
        // open save editor window
        private void button7_Click(object sender, EventArgs e) { newForm(new SaveEditorForm()); }
        // open map editor window
        private void button8_Click(object sender, EventArgs e) { newForm(new MapEditorForm()); }
        // open text editor window
        private void button9_Click(object sender, EventArgs e) { newForm(new TextEditorForm()); }
        // create new form method
        private void newForm(Form form)
        {
            form.StartPosition = FormStartPosition.Manual;
            form.Location = this.Location;
            form.Show();
            this.Hide();
            form.FormClosed += (s, args) => this.Show();
            form.Move += (s, args) => { if (this.Location != form.Location) { this.Location = form.Location; } };
        }
        // replace selected file
        private void button10_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog openFileDialog = new OpenFileDialog())
            {
                openFileDialog.InitialDirectory = "C:\\Program Files (x86)\\Jeff Wayne's 'The War Of The Worlds'";
                openFileDialog.Filter = "WAV Files (*.wav)|*.wav|All Files (*.*)|*.*";
                openFileDialog.FilterIndex = 1;
                openFileDialog.RestoreDirectory = true;
                openFileDialog.Title = "Select a Container (.wav) file";
                if (openFileDialog.ShowDialog() == DialogResult.OK)
                {
                    newWAV(openFileDialog.FileName);
                }
            }
        }
        // parse new wav file
        private void newWAV(string newFile)
        {
            using (BinaryReader br = new BinaryReader(File.OpenRead(newFile)))
            {
                // Read the RIFF header
                string riff = new string(br.ReadChars(4));
                if (riff != "RIFF")
                {
                    MessageBox.Show("File is not a valid WAV file.");
                    return;
                }
                br.ReadInt32(); // Skip file size
                string wave = new string(br.ReadChars(4));
                if (wave != "WAVE")
                {
                    MessageBox.Show("File is not a valid WAV file.");
                    return;
                }
                // Search for the 'data' chunk
                while (br.BaseStream.Position < br.BaseStream.Length)
                {
                    string chunkID = new string(br.ReadChars(4));
                    int chunkSize = br.ReadInt32();
                    if (chunkID == "data")
                    {
                        byte[] audioData = br.ReadBytes(chunkSize);
                        int selectedIndex = listBox1.SelectedIndex;
                        entries[selectedIndex].Edited = true;
                        entries[selectedIndex].Length = audioData.Length;
                        entries[selectedIndex].Offset = 0;
                        entries[selectedIndex].Data = audioData;
                        button11.Enabled = true; // enable save button
                        pictureBox1.Image = DrawWaveform(audioData, 156, 137, DetectSampleRate(audioData)); // redraw waveform and update labels
                        label3.Text = $"File Size : {selected.Length} bytes"; // update file size label
                        label4.Text = $"File Offset : Unknown"; // update file offset label
                        return;
                    }
                    else { br.BaseStream.Seek(chunkSize, SeekOrigin.Current); } // Skip this chunk
                }
                MessageBox.Show("No audio data found in WAV file.");
            }
        }
        // save file
        private void button11_Click(object sender, EventArgs e)
        {
            button11.Enabled = false; // Disable save button
            string outputPath = Path.Combine(Path.GetDirectoryName(filePath)!, Path.GetFileNameWithoutExtension(filePath) + "_updated.wow");
            using (BinaryWriter bw = new BinaryWriter(File.Create(outputPath)))
            {
                bw.Write(Encoding.ASCII.GetBytes(magic)); // Write archive header
                bw.Write(entries.Count); // could just write the original header and file count bytes as they are not changed
                // Placeholder for file entries
                long entriesStart = bw.BaseStream.Position;
                foreach (var entry in entries)
                {
                    bw.Write(Encoding.ASCII.GetBytes(entry.Name.PadRight(8, '\0')));
                    bw.Write(0); // Placeholder for length
                    bw.Write(0); // Placeholder for offset
                }
                // Write file data and update entries
                List<long> lengths = new List<long>();
                List<long> offsets = new List<long>();
                foreach (var entry in entries)
                {
                    long offset = bw.BaseStream.Position;
                    byte[]? data = entry.Edited ? entry.Data : ReadOriginalData(entry);
                    entry.Edited = false; // Reset all edited flags after saving ( overkill? )
                    bw.Write(data!);
                    lengths.Add(data!.Length);
                    offsets.Add(offset);
                }
                // Update file entries with actual lengths and offsets
                bw.BaseStream.Seek(entriesStart, SeekOrigin.Begin);
                for (int i = 0; i < entries.Count; i++)
                {
                    bw.Write(Encoding.ASCII.GetBytes(entries[i].Name.PadRight(8, '\0')));
                    bw.Write((int)lengths[i]);
                    bw.Write((int)offsets[i]);
                }
            }
            if (checkBox1.Checked) { File.Replace(outputPath, filePath, filePath + ".bak"); }
            else { File.Replace(outputPath, filePath, null); }
            MessageBox.Show("Archive updated successfully.");
        }
        private byte[] ReadOriginalData(WowFileEntry entry)
        {
            using (FileStream fs = new FileStream(filePath, FileMode.Open, FileAccess.Read))
            {
                fs.Seek(entry.Offset, SeekOrigin.Begin);
                byte[] buffer = new byte[entry.Length];
                int bytesRead = 0;
                while (bytesRead < entry.Length)
                {
                    int read = fs.Read(buffer, bytesRead, entry.Length - bytesRead);
                    if (read == 0)
                    {
                        throw new EndOfStreamException($"Unexpected end of file while reading entry '{entry.Name}'.");
                    }
                    bytesRead += read;
                }
                return buffer;
            }
        }
        // test ojd parsing
        private void button12_Click(object sender, EventArgs e) { newForm(new OJDParser()); }
        // drag and drop file onto the form
        private void WoWViewer_DragDrop(object sender, DragEventArgs e)
        {
            if (e.Data!.GetDataPresent(DataFormats.FileDrop))
            {
                string[] files = (string[])e.Data.GetData(DataFormats.FileDrop)!;
                if (files.Length != 1)
                {
                    MessageBox.Show("Please drop only one file.");
                    return;
                }
                string file = files[0];
                string extension = Path.GetExtension(file).ToLowerInvariant();
                if (extension == ".wow") { openFile(file); }
                else { MessageBox.Show("Only .wow files are supported."); }
            }
        }
        // DragEnter event handler to allow dropping .wow files onto the form
        private void WoWViewer_DragEnter(object sender, DragEventArgs e)
        {
            if (e.Data!.GetDataPresent(DataFormats.FileDrop))
            {
                string[] files = (string[])e.Data.GetData(DataFormats.FileDrop)!;
                if (files.Length == 1)
                {
                    if (Path.GetExtension(files[0]).ToLowerInvariant() == ".wow")
                    {
                        e.Effect = DragDropEffects.Copy;
                        return;
                    }
                }
            }
            e.Effect = DragDropEffects.None;
        }
        // public open file method to allow double click to open a .wow file
        public void openFile(string file)
        {
            filePath = file;
            textBox1.Text = filePath;
            parseFileCount();
        }
    }
}