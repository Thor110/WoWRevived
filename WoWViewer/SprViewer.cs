using System.Drawing.Imaging;
using System.Text;

namespace WoWViewer
{
    public partial class SprViewer : Form
    {
        private List<WowFileEntry> entries;
        private string selectedEntry;
        private string lastSelectedEntry;
        private int currentRenderedEntry = -1;
        private string lastSelectedPalette;
        private byte[] rawData;
        private byte[] palData;
        private string outputPath = "";
        private bool isMaps;
        private List<WowFileEntry> palettes = new List<WowFileEntry>();
        private string baseFolder;
        private int currentFrame;
        // Add at the top of SprViewer fields:
        private Dictionary<string, string> _sprToPal = new();

        // The 16-slot PAL mapping with confirmed corrections (slot 12=F1, 13=F2):
        private static readonly string[] PalSlots =
        {
            "HW.PAL","MW.PAL","HB.PAL","MB.PAL","HR.PAL","MR.PAL","BM.PAL","F1.PAL",
            "F2.PAL","F3.PAL","F4.PAL","F5.PAL","F6.PAL","F7.PAL","SE.PAL","CD.PAL"
        };
        public SprViewer(List<WowFileEntry> entryList, string entryName, bool maps)
        {
            InitializeComponent();
            entries = entryList;
            selectedEntry = entryName;
            isMaps = maps;
            if (maps)
            {
                if (!File.Exists("DAT\\Dat.wow"))
                {
                    MessageBox.Show("Where is DAT\\Dat.wow? Palette data is stored there.");
                    this.Load += (s, e) => this.Close();
                    return;
                }
                baseFolder = "MAPS";
                PopulatePalettes();
            }
            else { baseFolder = "DAT"; }
            BuildSprPalMap();
            PopulateList();
        }
        // build spr palette map from OBJ.ojd file
        private void BuildSprPalMap()
        {
            if (!File.Exists("OBJ.ojd")) { MessageBox.Show("OBJ.ojd file is missing."); return; }
            foreach (var entry in OJDParser.ParseOjdFile("OBJ.ojd"))
            {
                if (!entry.Name.EndsWith(".spr", StringComparison.OrdinalIgnoreCase)) { continue; }
                string key = Path.GetFileName(entry.Name).ToUpperInvariant();
                if (_sprToPal.ContainsKey(key)) { continue; } // first occurrence wins
                if (entry.PalSlot < PalSlots.Length) { _sprToPal[key] = PalSlots[entry.PalSlot]; }
            }
        }
        // populate palettes from DAT\\Dat.wow when reading MAPS.WoW
        private void PopulatePalettes()
        {
            using var br = new BinaryReader(File.OpenRead("DAT\\Dat.wow"));
            br.ReadInt32();
            int fileCount = br.ReadInt32();
            for (int i = 0; i < fileCount; i++)
            {
                br.ReadInt32();
                int offset = br.ReadInt32();
                int length = br.ReadInt32();
                byte[] nameBytes = br.ReadBytes(12);
                br.BaseStream.Seek(20, SeekOrigin.Current);
                int zeroIndex = Array.IndexOf(nameBytes, (byte)0);
                string name = Encoding.ASCII.GetString(nameBytes, 0, zeroIndex >= 0 ? zeroIndex : nameBytes.Length);
                if (name.EndsWith(".PAL"))
                {
                    long store = br.BaseStream.Position;
                    br.BaseStream.Seek(offset, SeekOrigin.Begin);
                    palettes.Add(new WowFileEntry { Name = name, Length = length, Offset = offset, Data = br.ReadBytes(length) });
                    br.BaseStream.Position = store;
                }
            }
        }
        // populate spr and pal lists
        private void PopulateList()
        {
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".SPR", StringComparison.OrdinalIgnoreCase)).ToList())
            {
                entry.Data = File.Exists($"{baseFolder}\\{entry.Name}") ? File.ReadAllBytes($"{baseFolder}\\{entry.Name}") : FfuhDecoder.Decompress(entry.Data!);
                listBox1.Items.Add(entry.Name);
            }
            foreach (var entry in (isMaps ? palettes : entries).Where(e => e.Name.EndsWith(".PAL", StringComparison.OrdinalIgnoreCase)))
            {
                entry.Data = FfuhDecoder.Decompress(entry.Data!);
                listBox2.Items.Add(entry.Name);
            }
            listBox1.SelectedIndex = listBox1.FindStringExact(selectedEntry);
            listBox2.SelectedIndex = 0;
        }
        // Auto-select the PAL file that OBJ.ojd says this SPR should use.
        private void TryAutoSelectPalette()
        {
            string key = selectedEntry.ToUpperInvariant();
            if (!_sprToPal.TryGetValue(key, out string? correctPal)) return;
            for (int i = 0; i < listBox2.Items.Count; i++)
            {
                if (string.Equals(listBox2.Items[i]?.ToString(), correctPal, StringComparison.OrdinalIgnoreCase))
                {
                    if (i != listBox2.SelectedIndex) listBox2.SelectedIndex = i;
                    return;
                }
            }
        }

        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            string sprName = listBox1.SelectedItem!.ToString()!;
            if (sprName == lastSelectedEntry) return;
            selectedEntry    = sprName;
            lastSelectedEntry = sprName;
            rawData = entries.First(e => e.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data!;
            TryAutoSelectPalette();
            RenderCurrent();
        }

        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            string palName = listBox2.SelectedItem!.ToString()!;
            if (palName == lastSelectedPalette) return;
            lastSelectedPalette = palName;
            

            palData = (isMaps ? palettes : entries).First(e => e.Name.Equals(palName, StringComparison.OrdinalIgnoreCase)).Data!;


            // PAL structure: bytes 0–767 = main 256-colour VGA palette (6-bit, ×4 = 8-bit).
            // Bytes 768–66303 = colour-blend / translucency table (not used for static viewing).
            // numericUpDown1 is exposed as a "shade level" control (0 = normal brightness).
            // Range 0–255 corresponds to rows in the shade table; 0 is full brightness.

            RenderCurrent();
        }

        private void RenderCurrent()
        {
            if (palData == null) { return; }
            label2.Text = SprDecoder.ReadInfo(rawData).ToString();

            if (currentRenderedEntry != listBox1.SelectedIndex)
            {
                comboBox1.SelectedIndexChanged -= comboBox1_SelectedIndexChanged!;
                comboBox1.Items.Clear();
                currentRenderedEntry = listBox1.SelectedIndex;
                int frameCount = SprDecoder.ReadInfo(rawData).TableCount;
                if (frameCount != 1)
                {
                    for (int i = 0; i < frameCount; i++) { comboBox1.Items.Add($"{selectedEntry}_frame_{i:D2}"); }
                    currentFrame = 0;
                    comboBox1.Enabled = true;
                    comboBox1.SelectedIndex = 0;
                }
                else
                {
                    currentFrame = 0;
                    comboBox1.Enabled = false;
                    comboBox1.Text = selectedEntry;
                    comboBox1.SelectedIndex = -1;
                }
                comboBox1.SelectedIndexChanged += comboBox1_SelectedIndexChanged!;
            }

            // paletteOffset = 0 → use the main palette at the start of the PAL file.
            // If shade-level rendering were ever needed:
            //   int shadeOffset = SprDecoder.ShadeTableOffset((int)numericUpDown1.Value);
            //   then look up shaded index before applying palette.
            // For now always render at full brightness (paletteOffset = 0).
            pictureBox1.Image = SprDecoder.Render(rawData, palData, paletteOffset: 0, frame: currentFrame);
        }

        private void numericUpDown1_ValueChanged(object sender, EventArgs e) => RenderCurrent();

        private void button1_Click(object sender, EventArgs e)
        {
            using var ofd = new OpenFileDialog { Filter = "Bitmap Files|*.bmp", Title = "Select 16-Color BMP for Replacement" };
            if (ofd.ShowDialog() != DialogResult.OK) return;
            var bmp = new Bitmap(ofd.FileName);
            if (bmp.PixelFormat != PixelFormat.Format4bppIndexed)
            { MessageBox.Show("Error: File must be a 16-color (4-bit) Indexed Bitmap."); return; }
            string fullPath = Path.Combine(baseFolder, Path.GetFileNameWithoutExtension(selectedEntry));
            if (File.Exists(fullPath) && MessageBox.Show($"'{fullPath}' exists, overwrite?", "Overwrite", MessageBoxButtons.YesNo) == DialogResult.No) return;
            pictureBox1.Image = bmp;
        }

        private void button2_Click(object sender, EventArgs e)
        {
            string name = Path.GetFileNameWithoutExtension(selectedEntry);
            pictureBox1.Image.Save(Path.Combine(outputPath, name + ".png"), ImageFormat.Png);
            MessageBox.Show($"{name}.png exported.");
        }

        private void button3_Click(object sender, EventArgs e)
        {
            foreach (WowFileEntry entry in entries.Where(e => e.Name.EndsWith(".SPR", StringComparison.OrdinalIgnoreCase)))
            {
                // Use the correct PAL for this sprite if known; otherwise fall back to selected PAL.
                int frameCount = SprDecoder.ReadInfo(entry.Data!).TableCount;
                string fileName = Path.GetFileNameWithoutExtension(entry.Name);

                if (frameCount != 1)
                {
                    for (int i = 0; i < frameCount; i++)
                    {
                        Bitmap frame = SprDecoder.Render(entry.Data!, palData, 0, i);
                        frame.Save(Path.Combine(outputPath, $"{fileName}_frame_{i:D2}.png"), ImageFormat.Png);
                        frame.Dispose();
                    }
                }
                else
                {
                    Bitmap img = SprDecoder.Render(entry.Data!, palData, 0, 0);
                    img.Save(Path.Combine(outputPath, fileName + ".png"), ImageFormat.Png);
                    img.Dispose();
                }
            }
            MessageBox.Show("All .spr files exported.");
        }

        private void button4_Click(object sender, EventArgs e)
        {
            using var fbd = new FolderBrowserDialog { InitialDirectory = outputPath != "" ? outputPath : AppDomain.CurrentDomain.BaseDirectory };
            if (fbd.ShowDialog() != DialogResult.OK) return;
            outputPath = fbd.SelectedPath;
            if (!outputPath.EndsWith("\\")) outputPath += "\\";
            textBox1.Text = outputPath;
            button2.Enabled = true;
            button3.Enabled = true;
        }

        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBox1.SelectedIndex < 0 || comboBox1.SelectedIndex == currentFrame) return;
            currentFrame = comboBox1.SelectedIndex;
            RenderCurrent();
        }
    }
}