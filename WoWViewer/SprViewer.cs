using System.Drawing.Imaging;
using System.Text;

namespace WoWViewer
{
    public partial class SprViewer : Form
    {
        private List<WowFileEntry> entries;
        private string selectedEntry; // selected SPR file on enter
        private string lastSelectedEntry;
        private int currentRenderedEntry;
        private string lastSelectedPalette;
        private byte[] rawData;
        private byte[] palData;
        private string outputPath = "";
        private bool isMaps; // is file MAPS.WoW
        private List<WowFileEntry> palettes = new List<WowFileEntry>(); // necessary if MAPS.WoW is loaded
        private string baseFolder;
        private int currentFrame;

        public SprViewer(List<WowFileEntry> entryList, string entryName, bool maps)
        {
            InitializeComponent();
            entries = entryList;
            selectedEntry = entryName;
            isMaps = maps;
            if (maps)
            {
                if (!File.Exists("DAT\\Dat.wow")) // check for dat file...
                {
                    MessageBox.Show("Where is DAT\\Dat.wow? Palette data is stored there.");
                    this.Load += (s, e) => this.Close();
                    return;
                }
                baseFolder = "MAPS";
                PopulatePalettes();
            }
            else { baseFolder = "DAT"; }
            PopulateList();
        }
        // read MAPS\\MAPS.WoW -> extract palette data from DAT\\Dat.wow
        private void PopulatePalettes()
        {
            using var br = new BinaryReader(File.OpenRead("DAT\\Dat.wow"));
            br.ReadInt32();
            int fileCount = br.ReadInt32();
            for (int i = 0; i < fileCount; i++) // near duplicate code from Form1.cs could be cleaned up later
            {
                br.ReadInt32();                     // skip 4 bytes
                int offset = br.ReadInt32();        // file offset
                int length = br.ReadInt32();        // file size
                byte[] nameBytes = br.ReadBytes(12);// filename (ASCII padded)
                br.BaseStream.Seek(20, SeekOrigin.Current); // skip 20 bytes
                int zeroIndex = Array.IndexOf(nameBytes, (byte)0); // setup entries and listbox
                string name = Encoding.ASCII.GetString(nameBytes, 0, zeroIndex >= 0 ? zeroIndex : nameBytes.Length);
                if (name.EndsWith(".PAL"))
                {
                    long store = br.BaseStream.Position; // store the current position
                    br.BaseStream.Seek(offset, SeekOrigin.Begin); // seek to the offset
                    palettes.Add(new WowFileEntry { Name = name, Length = length, Offset = offset, Data = FfuhDecoder.Decompress(br.ReadBytes(length)) });
                    br.BaseStream.Position = store; // return to the original position
                }
            }
        }
        private void PopulateList()
        {
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".SPR", StringComparison.OrdinalIgnoreCase)).ToList())
            {
                if (File.Exists($"{baseFolder}\\{entry.Name}")) // check if an overridden file already exists
                {
                    entry.Data = File.ReadAllBytes($"{baseFolder}\\{entry.Name}"); // update entries Data accordingly
                }
                else
                {
                    entry.Data = FfuhDecoder.Decompress(entry.Data!); // decompress here so I keep the ability to extract compressed assets
                }
                listBox1.Items.Add(entry.Name); // populate listbox
            }
            foreach (var entry in (isMaps ? palettes : entries).Where(e => e.Name.EndsWith(".PAL")).ToList()) { listBox2.Items.Add(entry.Name); }
            listBox1.SelectedIndex = listBox1.FindStringExact(selectedEntry); // set selectedEntry
            listBox2.SelectedIndex = 0; // set to first palette file
        }
        // SPR list box index changed
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            string sprName = listBox1.SelectedItem!.ToString()!;
            if (sprName == lastSelectedEntry) { return; } // prevent selecting the same entry
            selectedEntry = sprName; // update selected entry
            lastSelectedEntry = sprName; // update last selected entry
            rawData = entries.First(e => e.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase))!.Data!; // find selected entry data
            //
            RenderCurrent();
        }
        // PAL list box index changed
        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            string palName = listBox2.SelectedItem!.ToString()!;
            if (palName == lastSelectedPalette) { return; } // prevent selecting the same entry
            lastSelectedPalette = palName; // update last selected entry
            palData = (isMaps ? palettes : entries).First(e => e.Name.Equals(palName, StringComparison.OrdinalIgnoreCase)).Data!;
            numericUpDown1.Maximum = SprDecoder.PaletteCount(palData) - 1;
            numericUpDown1.Value = 0;
            //
            // palette limits
            // numericUpDown1 = raw byte offset, step 3 or 768
            numericUpDown1.Maximum = palData.Length - 768;
            numericUpDown1.Increment = 3; // step by one RGB entry at a time for fine tuning
            //
            RenderCurrent();
        }
        // render selected image with selected palette data
        private void RenderCurrent()
        {
            if (palData == null) { return; } // returns on first run when listBox1_SelectedIndexChanged then fires on listBox2_SelectedIndexChanged
            label1.Text = SprDecoder.ReadInfo(rawData).ToString();
            if (currentRenderedEntry != listBox1.SelectedIndex) // prevent repopulating when changing combo box, just rerender selected frame.
            {
                comboBox1.SelectedIndexChanged -= comboBox1_SelectedIndexChanged;
                //MessageBox.Show("TEST");
                comboBox1.Items.Clear(); // not ideal as the combo box gets repopulated when changing
                currentRenderedEntry = listBox1.SelectedIndex;
                if (SprDecoder.ReadInfo(rawData).TableCount != 1)
                {
                    for (int i = 0; i < SprDecoder.ReadInfo(rawData).TableCount; i++)
                    {
                        comboBox1.Items.Add($"{selectedEntry}_frame_{i:D2}");
                    }
                    currentFrame = 0;
                    comboBox1.Enabled = true;
                }
                else
                {
                    currentFrame = -1;
                    comboBox1.Enabled = false;
                    comboBox1.Text = $"{selectedEntry}";
                }
                comboBox1.SelectedIndex = currentFrame;
                comboBox1.SelectedIndexChanged += comboBox1_SelectedIndexChanged;
            }
            pictureBox1.Image = SprDecoder.Render(rawData, palData, SprDecoder.PaletteOffset((int)numericUpDown1.Value), checkBox1.Checked, currentFrame);
        }
        // palette changer
        private void numericUpDown1_ValueChanged(object sender, EventArgs e) => RenderCurrent();
        // replace selected asset by importing - future
        private void button1_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog ofd = new OpenFileDialog())
            {
                ofd.Filter = "Bitmap Files|*.bmp";
                ofd.Title = "Select 16-Color BMP for Replacement";
                if (ofd.ShowDialog() == DialogResult.OK)
                {
                    Bitmap bmp = new Bitmap(ofd.FileName);
                    if (bmp.PixelFormat != PixelFormat.Format4bppIndexed) // Validation: Ensure it's actually 4-bit (16 colors)
                    {
                        MessageBox.Show("Error: File must be a 16-color (4-bit) Indexed Bitmap.");
                        return;
                    }
                    string fullPath = Path.Combine(baseFolder, Path.GetFileNameWithoutExtension(selectedEntry));
                    if (File.Exists(fullPath) && MessageBox.Show($"File '{fullPath}' already exists, overwrite file?", "File Overwrite", MessageBoxButtons.YesNo) == DialogResult.No) { return; }
                    // save to fullPath
                    // convert to .spr format before displaying
                    pictureBox1.Image = bmp; // set picture box to the new image
                }
            }
        }
        // export selected button
        private void button2_Click(object sender, EventArgs e)
        {
            string fileNameOnly = Path.GetFileNameWithoutExtension(selectedEntry);
            pictureBox1.Image.Save(Path.Combine(outputPath, fileNameOnly + ".png"), ImageFormat.Png);
            MessageBox.Show($"{fileNameOnly}.png was exported successfully.");
        }
        // export all button
        private void button3_Click(object sender, EventArgs e)
        {
            // loop through render data export
            foreach (WowFileEntry entry in entries.Where(e => e.Name.EndsWith(".SPR", StringComparison.OrdinalIgnoreCase)).ToList())
            {
                int paletteOffset = SprDecoder.PaletteOffset((int)numericUpDown1.Value); // get palette offset // TODO: Update with correct palette data
                using (Bitmap renderedImage = SprDecoder.Render(entry.Data!, palData, paletteOffset)) // create rendered image
                {
                    renderedImage.Save(Path.Combine(outputPath, Path.GetFileNameWithoutExtension(entry.Name) + ".png"), ImageFormat.Png); // set file path and save rendered image
                }
            }
            MessageBox.Show("All .spr files exported successfully.");
        }
        // set output directory
        private void button4_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog folderBrowserDialog = new FolderBrowserDialog();
            folderBrowserDialog.InitialDirectory = AppDomain.CurrentDomain.BaseDirectory; // Set initial directory to the application base directory
            if (outputPath != "") { folderBrowserDialog.InitialDirectory = outputPath; } // Set initial directory to the last used output path
            if (folderBrowserDialog.ShowDialog() == DialogResult.OK)
            {
                outputPath = folderBrowserDialog.SelectedPath;
                if (!outputPath.EndsWith("\\")) { outputPath += "\\"; } // If Root Directory // Complete Directory String
                textBox1.Text = outputPath;
                button2.Enabled = true;
                button3.Enabled = true;
            }
        }
        // greyscale checkbox changed
        private void checkBox1_CheckedChanged(object sender, EventArgs e) => RenderCurrent();
        // frame selection combobox
        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            if(comboBox1.SelectedIndex == currentFrame) { return; }
            currentFrame = comboBox1.SelectedIndex;
            RenderCurrent();
        }
    }
}
