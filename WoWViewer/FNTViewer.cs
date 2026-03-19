using System.Drawing.Imaging;

namespace WoWViewer
{
    public partial class FNTViewer : Form
    {
        private List<WowFileEntry> entries;
        private string selectedEntry;
        private string lastSelectedEntry;
        private int currentRenderedEntry;
        private string lastSelectedPalette;
        private byte[] rawData;
        private byte[] palData;
        private string outputPath = "";
        private bool isMaps;
        private List<WowFileEntry> palettes = new List<WowFileEntry>();
        private byte[]? shadeData;   // active shade remap table (level 0, 256 bytes), null = identity
        public FNTViewer(List<WowFileEntry> entryList, string entryName, string output)
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
            PopulateList();
        }
        // populate spr and pal lists
        private void PopulateList()
        {
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".FNT", StringComparison.OrdinalIgnoreCase)).ToList())
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
        // font listbox
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            string sprName = listBox1.SelectedItem!.ToString()!;
            if (sprName == lastSelectedEntry) { return; }
            selectedEntry = sprName;
            lastSelectedEntry = sprName;
            rawData = entries.First(e => e.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data!;
            RenderCurrent();
        }
        // palette listbox
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
        // shader tables listbox
        private void listBox3_SelectedIndexChanged(object sender, EventArgs e)
        {
            shadeData = entries.FirstOrDefault(e => e.Name.Equals(listBox3.SelectedItem!.ToString()!, StringComparison.OrdinalIgnoreCase))!.Data![1..513];
            RenderCurrent();
        }
        // render selected sprite with selected palette
        private void RenderCurrent()
        {
            if (rawData == null || palData == null) return;

            var fontModel = FNTDecoder.Parse(rawData);
            var atlas = FNTDecoder.RenderFontAtlas(fontModel, palData);

            // Assuming your PictureBox is named pictureBox1
            pictureBox1.Image?.Dispose();
            pictureBox1.Image = atlas;
        }
        // export shader mapped palette
        private void button5_Click(object sender, EventArgs e)
        {
            byte[] trimmedPalette = new byte[768];
            Array.Copy(palData, 0, trimmedPalette, 0, 768);
            File.WriteAllBytes(outputPath + Path.GetFileNameWithoutExtension(selectedEntry) + (checkBox1.Checked ? "_SHADED.PAL" : ".PAL"), trimmedPalette);
            MessageBox.Show("Shader Mapped Palette Exported");
        }
        // disable shader data
        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            shadeData = checkBox1.Checked ? (isMaps ? palettes : entries).FirstOrDefault(e => e.Name.Equals(listBox3.SelectedItem!.ToString()!, StringComparison.OrdinalIgnoreCase))!.Data![1..513] : null;
            RenderCurrent();
        }
        // export selected
        private void button2_Click(object sender, EventArgs e)
        {
            string name = Path.GetFileNameWithoutExtension(selectedEntry);
            pictureBox1.Image.Save(Path.Combine(outputPath, name + ".png"), ImageFormat.Png);
            MessageBox.Show($"{name}.png exported.");
        }
        // replace
        private void button1_Click(object sender, EventArgs e)
        {
            MessageBox.Show("Replace font logic not setup yet!");
        }
        // export all
        private void button3_Click(object sender, EventArgs e)
        {
            Bitmap font;
            foreach (WowFileEntry entry in entries.Where(e => e.Name.EndsWith(".FNT", StringComparison.OrdinalIgnoreCase)))
            {
                for (int i = 0; i < listBox1.Items.Count; i++)
                {
                    font = FNTDecoder.RenderFontAtlas(FNTDecoder.Parse(entry.Data!), palData);
                    font.Save(Path.Combine(outputPath, Path.GetFileNameWithoutExtension(entry.Name) + ".png"), ImageFormat.Png);
                    font.Dispose();
                }
            }
            MessageBox.Show("All font files exported as .png files.");
        }
        // set output path
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
    }
}
