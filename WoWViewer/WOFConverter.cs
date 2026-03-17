using System.Text;

namespace WoWViewer
{
    public partial class WOFConverter : Form
    {
        private List<WowFileEntry> entries;
        private string selectedEntry;
        private string outputPath = "";
        private string lastSelectedEntry;
        private int currentRenderedEntry;
        private string lastSelectedPalette;
        private byte[] rawData;
        private byte[] palData;
        private byte[]? shadeData;   // active shade remap table (level 0, 256 bytes), null = identity
        private bool modelType;
        private int currentFrame;
        public WOFConverter(List<WowFileEntry> entryList, string entryName, string output, bool model = false)
        {
            InitializeComponent();
            entries = entryList;
            selectedEntry = entryName;
            if (output != "")
            {
                outputPath = output;
                textBox1.Text = outputPath;
                button2.Enabled = true;
                button3.Enabled = true;
                button5.Enabled = true;
            }
            modelType = model;
            PopulateList();
        }
        // populate wof and pal lists
        private void PopulateList()
        {
            foreach (var entry in entries.Where(e => e.Name.EndsWith(modelType ? ".IOB" : ".WOF", StringComparison.OrdinalIgnoreCase)).ToList())
            {
                entry.Data = File.Exists($"DAT\\{entry.Name}") ? File.ReadAllBytes($"DAT\\{entry.Name}") : FfuhDecoder.Decompress(entry.Data!);
                listBox1.Items.Add(entry.Name);
            }
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".PAL", StringComparison.OrdinalIgnoreCase)))
            {
                entry.Data = FfuhDecoder.Decompress(entry.Data!);
                listBox2.Items.Add(entry.Name);
            }
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".SHH", StringComparison.OrdinalIgnoreCase)))
            {
                entry.Data = FfuhDecoder.Decompress(entry.Data!);
                listBox3.Items.Add(entry.Name);
            }
            listBox1.SelectedIndex = listBox1.FindStringExact(selectedEntry);
        }
        // export palette
        private void button5_Click(object sender, EventArgs e)
        {
            byte[] trimmedPalette = new byte[768];
            Array.Copy(palData, 0, trimmedPalette, 0, 768);
            File.WriteAllBytes(outputPath + Path.GetFileNameWithoutExtension(selectedEntry) + (checkBox1.Checked ? "_SHADED.PAL" : ".PAL"), trimmedPalette);
            MessageBox.Show("Shader Mapped Palette Exported");
        }
        // export model
        private void button2_Click(object sender, EventArgs e)
        {

        }
        // export all models
        private void button3_Click(object sender, EventArgs e)
        {

        }
        // output path
        private void button4_Click(object sender, EventArgs e)
        {
            using var fbd = new FolderBrowserDialog { InitialDirectory = outputPath != "" ? outputPath : AppDomain.CurrentDomain.BaseDirectory };
            if (fbd.ShowDialog() != DialogResult.OK) return;
            outputPath = fbd.SelectedPath;
            if (!outputPath.EndsWith("\\")) outputPath += "\\";
            textBox1.Text = outputPath;
            button2.Enabled = true;
            button3.Enabled = true;
            button5.Enabled = true;
        }
        // model selection
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            string sprName = listBox1.SelectedItem!.ToString()!;
            if (sprName == lastSelectedEntry) { return; }
            selectedEntry = sprName;
            lastSelectedEntry = sprName;
            rawData = entries.First(e => e.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data!;
            //TryAutoSelectPalette(selectedEntry);
            //TryAutoSelectShader(selectedEntry);
            RenderCurrent();
        }
        // palette selection
        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            string palName = listBox2.SelectedItem!.ToString()!;
            if (palName == lastSelectedPalette) { return; }
            lastSelectedPalette = palName;
            palData = entries.First(e => e.Name.Equals(palName, StringComparison.OrdinalIgnoreCase)).Data!;
            // PAL structure: bytes 0–767 = main 256-colour VGA palette (6-bit, ×4 = 8-bit).
            // Bytes 768–66303 = colour-blend / translucency table (not used for static viewing).
            // numericUpDown1 is exposed as a "shade level" control (0 = normal brightness).
            // Range 0–255 corresponds to rows in the shade table; 0 is full brightness.
            RenderCurrent();
        }
        // shader listbox
        private void listBox3_SelectedIndexChanged(object sender, EventArgs e)
        {
            shadeData = entries.FirstOrDefault(e => e.Name.Equals(listBox3.SelectedItem!.ToString()!, StringComparison.OrdinalIgnoreCase))!.Data![1..513];
            RenderCurrent();
        }
        // disable shader data
        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            shadeData = checkBox1.Checked ? entries.FirstOrDefault(e => e.Name.Equals(listBox3.SelectedItem!.ToString()!, StringComparison.OrdinalIgnoreCase))!.Data![1..513] : null;
            RenderCurrent();
        }
        // render selected model texture with selected palette
        private void RenderCurrent()
        {
            if (palData == null) { return; } // TODO: Palette detection
            if (currentRenderedEntry != listBox1.SelectedIndex) // repopulate comboBox1 if new entry is selected
            {
                comboBox1.SelectedIndexChanged -= comboBox1_SelectedIndexChanged!;
                comboBox1.Items.Clear();
                currentRenderedEntry = listBox1.SelectedIndex;
                int frameCount = 1; // TODO: update once sub textures are detected
                string name = Path.GetFileNameWithoutExtension(selectedEntry);
                if (frameCount != 1)
                {
                    for (int i = 0; i < frameCount; i++) { comboBox1.Items.Add($"{name}_frame_{i:D2}"); }
                    comboBox1.Enabled = true;   // enable frames combo box
                    button6.Enabled = true;     // enable replace all frames button
                    button1.Enabled = false;    // disable single frame replace button??
                    comboBox1.SelectedIndex = 0;
                }
                else
                {
                    comboBox1.Enabled = false;  // disable frames combo box
                    button6.Enabled = false;    // disable replace all frames button
                    button1.Enabled = true;     // enable single frame replace button??
                    comboBox1.Text = name;      // update combobox with entry name
                    comboBox1.SelectedIndex = -1;
                }
                currentFrame = 0; // reset currently selected frame
                comboBox1.SelectedIndexChanged += comboBox1_SelectedIndexChanged!;
            }
            //pictureBox1.Image = SprDecoder.Render(rawData, palData, shadeData: shadeData, frame: currentFrame);
        }
        // replace texture
        private void button1_Click(object sender, EventArgs e)
        {
            using var ofd = new OpenFileDialog { Filter = "PNG Image|*.png", Title = "Select a replacement to encode" };
            if (ofd.ShowDialog() != DialogResult.OK) { return; }
            var bmp = new Bitmap(ofd.FileName);
            //byte[] encoded = SprEncoder.Encode(QuantiseToPalette(bmp, palData, checkBox1.Checked ? shadeData : null), bmp.Width, bmp.Height);
            //string outPath = Path.Combine("DAT", selectedEntry);
            //if (File.Exists(outPath) && MessageBox.Show($"'{outPath}' exists, overwrite?", "Overwrite", MessageBoxButtons.YesNo) == DialogResult.No) { return; }
            //File.WriteAllBytes(outPath, encoded);
            //entries.First(e => e.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data = encoded;
            //rawData = encoded;
            RenderCurrent();
            MessageBox.Show("Encoded and saved.");
        }
        // replace all textures
        private void button6_Click(object sender, EventArgs e)
        {

        }
        // select sub texture
        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBox1.SelectedIndex == currentFrame) { return; }
            currentFrame = comboBox1.SelectedIndex;
            RenderCurrent();
        }
    }
}
