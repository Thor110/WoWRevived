using System.Drawing.Imaging;
using System.Text;

namespace WoWViewer
{
    public partial class SprViewer : Form
    {
        private List<WowFileEntry> entries;
        private string selectedEntry; // selected SPR file on enter
        private string lastSelectedEntry;
        private byte[] rawData;
        private byte[] palData;
        private string outputPath = "";
        private bool isMaps; // is file MAPS.WoW
        public static int PaletteOffset(int paletteIndex) => 768 + paletteIndex * 768;

        public SprViewer(List<WowFileEntry> entryList, string entryName, bool maps)
        {
            InitializeComponent();
            entries = entryList;
            selectedEntry = entryName;
            isMaps = maps;

            if(isMaps)
            {
                // load dat file...
                if (!File.Exists("DAT\\Dat.wow"))
                {
                    MessageBox.Show("Where is DAT\\Dat.wow ");
                }
            }



            PopulateList();
        }
        private void PopulateList()
        {
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".SPR", StringComparison.OrdinalIgnoreCase)).ToList()) { listBox1.Items.Add(entry.Name); }
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".PAL", StringComparison.OrdinalIgnoreCase)).ToList()) { listBox2.Items.Add(entry.Name); }
            listBox1.SelectedIndex = listBox1.FindStringExact(selectedEntry); // set selectedEntry
            listBox2.SelectedIndex = 0; // set to first palette file
        }
        // SPR list box index changed
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (listBox1.SelectedItem!.ToString()! == lastSelectedEntry) { return; } // prevent selecting the same entry
            selectedEntry = listBox1.SelectedItem!.ToString()!; // update selected entry
            lastSelectedEntry = selectedEntry; // update last selected entry
            rawData = entries.First(e => e.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase))!.Data!; // find selected entry data

            //
            RenderCurrent();
        }
        // PAL list box index changed
        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            string palName = listBox2.SelectedItem!.ToString()!;
            palData = entries.First(e => e.Name.Equals(palName, StringComparison.OrdinalIgnoreCase)).Data!;
            numericUpDown1.Maximum = SprDecoder.PaletteCount(palData) - 1;
            numericUpDown1.Value = 0;
            RenderCurrent();
        }
        // render selected image with selected palette data
        private void RenderCurrent()
        {
            if (rawData == null || palData == null) { return; } // returns on first run when listBox1_SelectedIndexChanged
            int paletteOffset = SprDecoder.PaletteOffset((int)numericUpDown1.Value);
            pictureBox1.Image = SprDecoder.Render(rawData, palData, paletteOffset);
            label1.Text = SprDecoder.ReadInfo(rawData).ToString();
        }
        // palette changer
        private void numericUpDown1_ValueChanged(object sender, EventArgs e)
        {
            // numericUpDown1 = raw byte offset, step 3 or 768
            numericUpDown1.Maximum = palData.Length - 768;
            numericUpDown1.Increment = 3; // step by one RGB entry at a time for fine tuning
            RenderCurrent();
        }
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
                    // Validation: Ensure it's actually 4-bit (16 colors)
                    if (bmp.PixelFormat != PixelFormat.Format4bppIndexed)
                    {
                        MessageBox.Show("Error: File must be a 16-color (4-bit) Indexed Bitmap.");
                        return;
                    }
                    // convert to .spr format before displaying
                    pictureBox1.Image = bmp; // set picture box to the new image

                    string fullPath = Path.Combine(isMaps ? "MAPS" : "DAT", Path.GetFileNameWithoutExtension(selectedEntry));
                    // save to fullPath
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
                byte[] rawData = entries.First(e => e.Name.Equals(entry.Name, StringComparison.OrdinalIgnoreCase))!.Data!; // get decompressed data // inline later
                string fileNameOnly = Path.GetFileNameWithoutExtension(entry.Name); // get filename without extension
                string fullPath = Path.Combine(outputPath, fileNameOnly + ".png"); // set file path
                int paletteOffset = SprDecoder.PaletteOffset((int)numericUpDown1.Value); // get palette offset // TODO: Update with correct palette data
                Bitmap renderedImage = SprDecoder.Render(rawData, palData, paletteOffset); // create rendered image
                renderedImage.Save(fullPath, ImageFormat.Png); // save rendered image
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
    }
}
