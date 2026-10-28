using System.Media;
using System.Text;

namespace WoWViewer
{
    public partial class WoWViewer : Form
    {
        private SoundPlayer soundPlayer = null!;
        private string lastSelectedListItem = "";
        private string filePath = "";
        private string outputPath = "";
        private string magic = "";
        private List<WowFileEntry> entries = new List<WowFileEntry>();
        private WowFileEntry selected = null!;
        private Dictionary<string, Action<WowFileEntry>> handlers = null!;
        private bool cancelOpenNewFile;
        private void InitializeHandlers()
        {
            handlers = new Dictionary<string, Action<WowFileEntry>>(StringComparer.OrdinalIgnoreCase)
            {
                //DAT/Dat.wow
                { "DAT", HandleDAT },   // pseudo random dithering and division tables DITH.DAT and DIV_TAB.DAT
                { "FNT", HandleFNT },   // fonts
                { "HSH", HandleHSH },   // B16.HSH
                { "HSM", HandleHSM },   // B15.HSM
                { "INT", HandleINT },   // terrain geometry shader tables
                { "IOB", HandleIOB },   // buildings and terrain objects
                { "PAL", HandlePAL },   // palettes         // pairs with shaders
                { "RAW", HandleRAW },   // effects
                { "SHH", HandleSHH },   // shader high
                { "SHL", HandleSHL },   // shader low       // one loose B8.SHL
                { "SHM", HandleSHM },   // shader medium
                { "SPR", HandleSPR },   // sprites, only lowercase exception
                { "WOF", HandleWOF },   // units
                //MAPS/MAPS.WoW
                { "ATM", HandleATM },   // terrain textures - ATM
                { "CLS", HandleATM },   // terrain geometry - CLS
                //"SPR"                 // human and martian minimaps
            };
        }
        //DAT/Dat.wow
        private void HandleDAT(WowFileEntry entry) { MessageBox.Show("DAT files are pseudo random dithering tables."); }
        private void HandleFNT(WowFileEntry entry) { newForm(new FNTViewer(entries, listBox1.SelectedItem!.ToString()!, outputPath)); }
        private void HandleHSH(WowFileEntry entry) { MessageBox.Show("HSH file is a global high quality shader table."); }
        private void HandleHSM(WowFileEntry entry) { MessageBox.Show("HSM file is a global medium quality shader table."); }
        private void HandleINT(WowFileEntry entry) { MessageBox.Show("INT files are terrain geometry shader tables, load MAPS.WoW and this file will be loaded through that method."); }
        private void HandleIOB(WowFileEntry entry) { newForm(new WOFViewer(entries, listBox1.SelectedItem!.ToString()!, outputPath, true)); }
        private void HandlePAL(WowFileEntry entry) { MessageBox.Show("This is a palette file!"); }
        private void HandleRAW(WowFileEntry entry) { MessageBox.Show("These are visual effects files! Not decoded yet."); }
        private void HandleSHH(WowFileEntry entry) { MessageBox.Show("This is a High Quality Shader Table! 16-bit. (SHH - Shader High)"); }
        private void HandleSHL(WowFileEntry entry) { MessageBox.Show("This is a Low Quality Shader Table! 8-bit. (SHH - Shader Low)"); }
        private void HandleSHM(WowFileEntry entry) { MessageBox.Show("This is a Medium Quality Shader Table! 15-bit. (SHH - Shader Medium)"); }
        private void HandleSPR(WowFileEntry entry) { newForm(new SprViewer(entries, listBox1.SelectedItem!.ToString()!, Path.GetFileName(filePath).Contains("MAPS"), outputPath, filePath)); }
        private void HandleWOF(WowFileEntry entry) { newForm(new WOFViewer(entries, listBox1.SelectedItem!.ToString()!, outputPath)); }
        //MAPS/MAPS.WoW
        private void HandleATM(WowFileEntry entry) { newForm(new CLSViewer(entries, listBox1.SelectedItem!.ToString()!, outputPath, filePath)); }
        public WoWViewer()
        {
            InitializeComponent();
            listBox1.DrawMode = DrawMode.OwnerDrawFixed;
            listBox1.DrawItem += ListBox1_DrawItem!;
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(Label) });
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
                openFileDialog.InitialDirectory = AppDomain.CurrentDomain.BaseDirectory;
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
            folderBrowserDialog.InitialDirectory = AppDomain.CurrentDomain.BaseDirectory; // Set initial directory to the application base directory
            if (outputPath != "") { folderBrowserDialog.InitialDirectory = outputPath; } // Set initial directory to the last used output path
            if (folderBrowserDialog.ShowDialog() == DialogResult.OK)
            {
                outputPath = folderBrowserDialog.SelectedPath;
                if (!outputPath.EndsWith("\\")) { outputPath += "\\"; } // If Root Directory // Complete Directory String
                textBox2.Text = outputPath;
                if (filePath != "") { button4.Enabled = true; } // Enable extract all button
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
                // update container type label
                if (filePath.Contains("Dat")) { label2.Text = "Container Type : " + "Data"; }
                else { label2.Text = "Container Type : " + "Maps"; }
                for (int i = 0; i < fileCount; i++)
                {
                    br.ReadInt32();                     // skip 4 bytes
                    int offset = br.ReadInt32();        // file offset
                    int length = br.ReadInt32();        // file size
                    byte[] nameBytes = br.ReadBytes(12);// filename (ASCII padded)
                    br.BaseStream.Seek(20, SeekOrigin.Current); // skip 20 bytes
                    int zeroIndex = Array.IndexOf(nameBytes, (byte)0); // setup entries and listbox
                    string name = Encoding.ASCII.GetString(nameBytes, 0, zeroIndex >= 0 ? zeroIndex : nameBytes.Length);
                    listBox1.Items.Add($"{name}");      // add entry to the listbox
                    // record data for the entry
                    long store = br.BaseStream.Position; // store the current position
                    br.BaseStream.Seek(offset, SeekOrigin.Begin); // seek to the offset
                    entries.Add(new WowFileEntry { Name = name, Length = length, Offset = offset, Data = br.ReadBytes(length) });
                    br.BaseStream.Position = store; // return to the original position
                }
                button5.Visible = false;                // hide audio player play button
                button6.Visible = false;                // hide audio player stop button
                pictureBox1.Visible = false;            // hide the picture box
                label5.Visible = false;                 // hide sound length label
                button10.Visible = false;               // hide replace file button
                button11.Visible = false;               // hide save file button
                checkBox1.Visible = false;              // hide checkbox for backing up the original file ( for now )
                checkBox2.Visible = true;               // show decompression checkbox
            }
            else if (magic == "SfxL")
            {
                label2.Text = "Container Type : " + "Sound Effects Library"; // update container type label
                entries.AddRange(SfxlArchive.ReadEntries(br, fileCount)); // filenames are up to 8 bytes in these files
                foreach (var entry in entries) { listBox1.Items.Add($"{entry.Name}.WAV"); }
                button5.Visible = true;                 // show audio player play button
                button6.Visible = true;                 // show audio player stop button
                pictureBox1.Visible = true;             // show the picture box
                label5.Visible = true;                  // show sound length label
                button10.Visible = true;                // show replace file button
                button11.Visible = true;                // show save file button
                checkBox1.Visible = true;               // enable checkbox for backing up the original file
                checkBox2.Visible = false;              // hide decompression checkbox
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
            byte[] rawData = entry.Data!;
            string safeName = Path.GetFileName(entry.Name); // archive names must never be able to point outside the output folder
            using var fs = new FileStream(Path.Combine(outputPath, (asWav ? $"{safeName}.wav" : safeName)), FileMode.Create);
            if (asWav) { fs.Write(CreateWavHeader(rawData.Length, DetectSampleRate(rawData))); }
            else if (checkBox2.Checked && rawData.Length >= 16 && FfuhDecoder.IsCompressed(rawData))
            {
                rawData = FfuhDecoder.Decompress(rawData);
            }
            fs.Write(rawData, 0, rawData.Length);
        }
        // listbox selection changed
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (listBox1.SelectedIndex < 0 || listBox1.SelectedIndex >= entries.Count) { return; } // fired with -1 when the list is cleared
            selected = entries[listBox1.SelectedIndex]; // update selected entry
            if (selected.Name != lastSelectedListItem) // check selection is new
            {
                label3.Text = $"File Size : {selected.Length} bytes"; // update file size label
                if (selected.Offset == 0) { label4.Text = $"File Offset : Unknown"; } // update file offset label
                else { label4.Text = $"File Offset : {selected.Offset} bytes"; } // update file offset label
                if (magic == "SfxL") // load and display the waveform image if browsing SfxL container
                {
                    byte[] rawData = selected.Data!; // entry data is always raw PCM, edited or not
                    pictureBox1.Image = DrawWaveform(rawData, 156, 137, DetectSampleRate(rawData));
                    button5.Enabled = true; // enable play button
                    button6.Enabled = true; // enable stop button
                    button10.Enabled = true; // enable replace file button
                }
                if (button4.Enabled) { button2.Enabled = true; } // enable extract button
                lastSelectedListItem = selected.Name; // update last selected item
            }
        }
        // play sound button
        private void button5_Click(object sender, EventArgs e) { PlayRawSound(selected); }
        // play sound method
        private void PlayRawSound(WowFileEntry entry)
        {
            if (entry == null || entry.Data == null) { return; }
            byte[] rawData = entry.Data; // entry data is always raw PCM, edited or not
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
            else if (magic == "KAT!" && listBox1.SelectedItem != null)
            {
                string extension = Path.GetExtension(listBox1.SelectedItem.ToString()!).TrimStart('.');
                if (handlers.TryGetValue(extension, out var handler)) { handler.Invoke(selected); }
                else { MessageBox.Show($"There is no viewer for .{extension} files yet."); }
            }
        }
        // draw waveform
        private Bitmap DrawWaveform(byte[] pcmData, int width, int height, int sampleRate)
        {
            var bmp = new Bitmap(width, height);
            using var g = Graphics.FromImage(bmp);
            g.Clear(Color.Black);
            short[] samples = Extract16BitMonoSamples(pcmData, sampleRate);
            int samplesPerPixel = Math.Max(1, samples.Length / width);
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
        // extract 16-bit mono samples from raw PCM data
        private short[] Extract16BitMonoSamples(byte[] pcmData, int sampleRate)
        {
            int sampleCount = pcmData.Length / 2; // calculate number of samples
            short[] samples = new short[sampleCount]; // create array for samples
            Buffer.BlockCopy(pcmData, 0, samples, 0, sampleCount * 2); // build samples array (little-endian 16-bit)
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
        private void button7_Click(object sender, EventArgs e)
        {
            MessageBox.Show("The Save Editor isn't finished and I have stopped working on it for now." +
    "\n\nThis is because it will take a lot of work to perfect." +
    "\n\nThe only functionality currently there is date/time/save name/swap sides." +
    "\n\nEverything else is unfinished.");
            newForm(new SaveEditorForm());
        }
        // open map editor window
        private void button8_Click(object sender, EventArgs e) { newForm(new MapEditorForm()); }
        // open text editor window
        private void button9_Click(object sender, EventArgs e) { newForm(new TextEditorForm()); }
        // create new form method
        private void newForm(Form form)
        {
            form.StartPosition = FormStartPosition.Manual;
            form.Location = this.Location;
            form.FormClosed += (s, args) => this.Show();
            form.Move += (s, args) => { if (this.Location != form.Location) { this.Location = form.Location; } };
            form.Show();
            if (!form.IsDisposed) { this.Hide(); }
        }
        // replace selected file
        private void button10_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog openFileDialog = new OpenFileDialog())
            {
                openFileDialog.Filter = "WAV Files (*.wav)|*.wav|All Files (*.*)|*.*";
                openFileDialog.FilterIndex = 1;
                openFileDialog.RestoreDirectory = true;
                openFileDialog.Title = "Select a Container (.wav) file";
                if (openFileDialog.ShowDialog() == DialogResult.OK) { newWAV(openFileDialog.FileName); }
            }
        }
        // parse new wav file
        private void newWAV(string newFile)
        {
            int selectedIndex = listBox1.SelectedIndex;
            if (selectedIndex < 0 || selectedIndex >= entries.Count) { return; }
            using (BinaryReader br = new BinaryReader(File.OpenRead(newFile)))
            {
                // Read the RIFF header
                if (br.BaseStream.Length < 12 || Encoding.ASCII.GetString(br.ReadBytes(4)) != "RIFF")
                {
                    MessageBox.Show("File is not a valid WAV file.");
                    return;
                }
                br.ReadInt32(); // Skip file size
                if (Encoding.ASCII.GetString(br.ReadBytes(4)) != "WAVE")
                {
                    MessageBox.Show("File is not a valid WAV file.");
                    return;
                }
                bool formatChecked = false;
                int sampleRate = 0;
                // Walk the chunks, the 'fmt ' chunk must come before 'data'
                while (br.BaseStream.Position + 8 <= br.BaseStream.Length)
                {
                    string chunkID = Encoding.ASCII.GetString(br.ReadBytes(4));
                    uint chunkSize = br.ReadUInt32();
                    if (chunkID == "fmt ")
                    {
                        long chunkStart = br.BaseStream.Position;
                        short format = br.ReadInt16();
                        short channels = br.ReadInt16();
                        sampleRate = br.ReadInt32();
                        br.ReadInt32(); // byte rate
                        br.ReadInt16(); // block align
                        short bitsPerSample = br.ReadInt16();
                        // SfxL entries are raw PCM with no format information, the game expects 16-bit mono
                        if (format != 1 || channels != 1 || bitsPerSample != 16)
                        {
                            MessageBox.Show($"Unsupported WAV format ({(format == 1 ? "PCM" : $"format {format}")}, {channels} channel(s), {bitsPerSample}-bit).\n\n" +
                                "Replacement sounds must be uncompressed 16-bit mono PCM.", "Unsupported WAV", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                            return;
                        }
                        formatChecked = true;
                        br.BaseStream.Position = chunkStart + chunkSize + (chunkSize % 2);
                    }
                    else if (chunkID == "data")
                    {
                        if (!formatChecked)
                        {
                            MessageBox.Show("WAV file has no format chunk before its audio data.");
                            return;
                        }
                        if (chunkSize > int.MaxValue || chunkSize > br.BaseStream.Length - br.BaseStream.Position)
                        {
                            MessageBox.Show("Audio data chunk is larger than the file.");
                            return;
                        }
                        if (sampleRate != 22050 && sampleRate != 44100)
                        {
                            MessageBox.Show($"The sample rate is {sampleRate} Hz. The game's sounds are 22050 Hz or 44100 Hz, so this sound will play at the wrong speed.",
                                "Sample Rate Warning", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        }
                        byte[] audioData = br.ReadBytes((int)chunkSize); // stored as raw PCM, the same as unedited entries
                        entries[selectedIndex].Edited = true;
                        entries[selectedIndex].Length = audioData.Length;
                        entries[selectedIndex].Data = audioData;
                        button11.Enabled = true; // enable save button
                        pictureBox1.Image = DrawWaveform(audioData, 156, 137, DetectSampleRate(audioData)); // redraw waveform and update labels
                        label3.Text = $"File Size : {audioData.Length} bytes"; // update file size label
                        label4.Text = $"File Offset : Unknown (until saved)"; // update file offset label
                        listBox1.Invalidate(); // redraw so the entry shows as edited
                        return;
                    }
                    else { br.BaseStream.Seek(chunkSize + (chunkSize % 2), SeekOrigin.Current); } // Skip this chunk
                }
                MessageBox.Show("No audio data found in WAV file.");
            }
        }
        // save file
        private void button11_Click(object sender, EventArgs e)
        {
            byte[] archive;
            try { archive = SfxlArchive.Write(entries); } // writes from memory and updates every entry's offset and length
            catch (InvalidDataException ex)
            {
                MessageBox.Show(ex.Message, "Save Failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            button11.Enabled = false; // Disable save button
            string tempPath = Path.Combine(Path.GetDirectoryName(filePath)!, Path.GetFileNameWithoutExtension(filePath) + "_updated.wow"); // temporary file location
            File.WriteAllBytes(tempPath, archive);
            if (checkBox1.Checked) { File.Replace(tempPath, filePath, filePath + ".bak"); } // create backup file
            else { File.Replace(tempPath, filePath, null); } // replace original file
            foreach (var entry in entries) { entry.Edited = false; }
            listBox1.Invalidate();
            if (selected != null) { label4.Text = $"File Offset : {selected.Offset} bytes"; }
            MessageBox.Show("Archive updated successfully.");
        }
        // test ojd parsing
        private void button12_Click(object sender, EventArgs e) { newForm(new OJDParser()); }
        // drag and drop file onto the form
        private void WoWViewer_DragDrop(object sender, DragEventArgs e) { openFile(((string[])e.Data!.GetData(DataFormats.FileDrop)!)[0]); }
        // DragEnter event handler to allow dropping .wow files onto the form
        private void WoWViewer_DragEnter(object sender, DragEventArgs e)
        {
            if (e.Data!.GetDataPresent(DataFormats.FileDrop))
            {
                string[] files = (string[])e.Data.GetData(DataFormats.FileDrop)!;
                if (files.Length == 1 && Path.GetExtension(files[0]).ToLowerInvariant() == ".wow")
                {
                    e.Effect = DragDropEffects.Copy;
                    return;
                }
            }
            e.Effect = DragDropEffects.None;
        }
        // public open file method to allow double click to open a .wow file
        public void openFile(string file)
        {
            if (filePath != "") { UnsavedChanges(null!, "opening another file", button11); }
            if (outputPath != "") { button4.Enabled = true; }
            if (cancelOpenNewFile) { cancelOpenNewFile = false; return; } // if unsaved changes were cancelled
            filePath = file;
            textBox1.Text = filePath;
            pictureBox1.Image = null; // reset picture box
            parseFileCount();
        }
        // on close prompt
        private void WoWViewer_FormClosing(object sender, FormClosingEventArgs e) { UnsavedChanges(e, "exiting", button11); }
        // check for unsaved changes
        public void UnsavedChanges(FormClosingEventArgs e, string reason, Button button)
        { if (entries.Any(e => e.Edited)) { cancelOpenNewFile = Utilities.UnsavedChanges(reason, () => button.PerformClick(), e); } }
    }
}