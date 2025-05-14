using System.Text;
using System.Media;
using System.IO;
using System.Windows.Forms;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace WOWViewer
{
    public partial class WOWViewer : Form
    {
        private SoundPlayer soundPlayer = null!;
        private ToolTip tooltip = new ToolTip();
        private Type[] excludedControlTypes = new Type[] { typeof(ListBox), typeof(Label) };
        private string lastSelectedListItem = string.Empty;
        private string filePath = string.Empty;
        private string outputPath = string.Empty;
        private string magic = string.Empty;
        private int fileCount = 0;
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
            InitializeTooltips();
            InitializeHandlers();
        }
        /// <summary>
        /// InitializeTooltips prepares a tooltip for every control in the form.
        /// </summary>
        /// <remarks>
        /// Uses excludedControlTypes to exclude certain types of controls from displaying tooltips.
        /// </remarks>
        void InitializeTooltips()
        {
            components = new System.ComponentModel.Container();
            tooltip = new ToolTip(components);
            foreach (Control control in Controls)
            {
                if (excludedControlTypes.Contains(control.GetType()) != true)
                {
                    control.MouseEnter += new EventHandler(tooltip_MouseEnter);
                    control.MouseLeave += new EventHandler(tooltip_MouseLeave);
                }
            }
        }
        /// <summary>
        /// tooltip_MouseEnter event Handler uses the existing AccessibleDescription property as the tooltip information.
        /// </summary>
        void tooltip_MouseEnter(object? sender, EventArgs e)
        {
            Control control = (Control)sender!;
            if (control.AccessibleDescription != null) { tooltip.Show(control.AccessibleDescription.ToString(), control); }
            else { tooltip.Show("No description available.", control); }
        }
        /// <summary>
        /// tooltip_MouseLeave event Handler hides the active tooltip.
        /// </summary>
        void tooltip_MouseLeave(object? sender, EventArgs e) { tooltip.Hide((Control)sender!); }
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
        private void button2_Click(object sender, EventArgs e)
        {
            if (listBox1.SelectedIndex == -1)
            {
                MessageBox.Show("Please select a file from the list.");
                return;
            }
            bool isWav;
            if (magic == "KAT!") { isWav = false; }
            else if (magic == "SfxL") { isWav = true; }
            else { return; }
            ExtractToFile(selected, isWav);
            selected.ToString();
            MessageBox.Show($"Extracted: {listBox1.SelectedItem!.ToString()}");
        }
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
                button2.Enabled = true; // Enable extract button
                button4.Enabled = true; // Enable extract all button
            }
        }
        // extract all files
        private void button4_Click(object sender, EventArgs e)
        {
            bool isWav;
            if (magic == "KAT!") { isWav = false; }
            else if (magic == "SfxL") { isWav = true; }
            else { return; }
            foreach (var entry in entries) { ExtractToFile(entry, isWav); }
            MessageBox.Show("All files extracted successfully.");
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
                byte[] wavHeader = CreateWavHeader(entry.Length);
                fs.Write(wavHeader, 0, wavHeader.Length);
            }
            //if entry.Name ends with extension .RAW, .SHH, .SHL, .SHM, .SPR, .WOF etc add relevant header when they are determined and implemented.
            fs.Write(rawData, 0, rawData.Length);
        }
        // listbox selection changed
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            selected = entries[listBox1.SelectedIndex];
            // check selection is new
            if (selected.Name != lastSelectedListItem)
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
                    byte[] wavHeader = CreateWavHeader(selected.Length);

                    using var ms = new MemoryStream();
                    ms.Write(wavHeader, 0, wavHeader.Length);
                    ms.Write(rawData, 0, rawData.Length);
                    ms.Position = 0;

                    byte[] fullWav = ms.ToArray();
                    pictureBox1.Image = DrawWaveform(fullWav); // update the waveform

                    lastSelectedListItem = selected.Name;
                }
                else if (magic == "KAT!")
                {
                    string ext = listBox1.SelectedItem!.ToString()!.Split('.')[1];
                    if (handlers.ContainsKey(ext))
                    {
                        handlers[ext].Invoke(selected);
                    }
                    lastSelectedListItem = selected.Name;
                }
            }
            
        }
        // play sound button
        private void button5_Click(object sender, EventArgs e)
        {
            if (selected == null)
            {
                MessageBox.Show("Please select a file from the list to play.");
                return;
            }
            else
            {
                PlayRawSound(selected);
            }

        }
        // play sound method
        private void PlayRawSound(WowFileEntry entry)
        {
            using var br = new BinaryReader(File.OpenRead(filePath));
            br.BaseStream.Seek(entry.Offset, SeekOrigin.Begin);
            byte[] rawData = br.ReadBytes(entry.Length);

            byte[] wavHeader = CreateWavHeader(entry.Length);
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
        private void button6_Click(object sender, EventArgs e)
        {
            soundPlayer?.Stop();
        }
        // double click to play sound
        private void listBox1_MouseDoubleClick(object sender, MouseEventArgs e)
        {
            if (magic == "SfxL")
            {
                PlayRawSound(selected);
            }
        }
        // draw waveform
        private Bitmap DrawWaveform(byte[] wavData, int width = 156, int height = 137)
        {
            var bmp = new Bitmap(width, height);
            using var g = Graphics.FromImage(bmp);
            g.Clear(Color.Black);

            short[] samples = Extract16BitMonoSamples(wavData);
            int samplesPerPixel = samples.Length / width;

            for (int x = 0; x < width; x++)
            {
                int start = x * samplesPerPixel;
                short max = 0;
                for (int i = 0; i < samplesPerPixel && (start + i) < samples.Length; i++)
                {
                    short val = Math.Abs(samples[start + i]);
                    if (val > max) max = val;
                }

                float normalized = max / (float)short.MaxValue;
                int y = (int)(normalized * height / 2);
                g.DrawLine(Pens.LimeGreen, x, height / 2 - y, x, height / 2 + y);
            }

            return bmp;
        }
        // extract 16-bit mono samples from WAV data
        private short[] Extract16BitMonoSamples(byte[] wavData)
        {
            using var ms = new MemoryStream(wavData);
            using var br = new BinaryReader(ms);

            br.BaseStream.Seek(44, SeekOrigin.Begin); // skip WAV header
            int sampleCount = (wavData.Length - 44) / 2;
            short[] samples = new short[sampleCount];

            for (int i = 0; i < sampleCount; i++)
            {
                samples[i] = br.ReadInt16();
            }
            // calculate sound length
            double duration = sampleCount / (double)22050; // seconds
            int minutes = (int)duration / 60;
            int seconds = (int)duration % 60;
            if (duration > 60)
            {
                label5.Text = $"Sound Length : {minutes:D2}:{seconds:D2}";
            }
            else
            {
                label5.Text = $"Sound Length : {duration:F2} seconds"; // update sound length label
            }
            return samples;
        }
    }
}