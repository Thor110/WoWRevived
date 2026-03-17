using System.Drawing.Imaging;
using System.Text;

namespace WoWViewer
{
    public partial class CLSViewer : Form
    {
        private List<WowFileEntry> entries;
        private string selectedEntry;
        private string outputPath = "";
        private string lastSelectedEntry = "";
        private int currentRenderedEntry = -1;
        private string lastSelectedPalette = "";
        private byte[] clsData = [];
        private byte[] atmData = [];
        private byte[] palData = [];
        private byte[]? shadeData;   // active SHH level-0 slice (512 bytes), null = raw PAL
        private int currentFrame;
        private List<WowFileEntry> palettes = new List<WowFileEntry>();
        // ── View mode ─────────────────────────────────────────────────────────
        private enum ViewMode { TileMap, Heightmap, Composite }
        private ViewMode currentView = ViewMode.Composite;
        public CLSViewer(List<WowFileEntry> entryList, string entryName, string output)
        {
            InitializeComponent();
            entries = entryList;
            if (entryName.EndsWith(".ATM")) { entryName = Path.ChangeExtension(entryName, ".CLS"); }
            selectedEntry = entryName;
            if (output != "")
            {
                outputPath = output;
                textBox1.Text = outputPath;
                button2.Enabled = button3.Enabled = button5.Enabled = true;
            }
            PopulatePalettes();
            PopulateList();
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
                if (name.EndsWith(".PAL") || name.StartsWith("LAND")) // .PAL // LAND INT/SHH/SHL/SHM into palettes
                {
                    long store = br.BaseStream.Position;
                    br.BaseStream.Seek(offset, SeekOrigin.Begin);
                    palettes.Add(new WowFileEntry { Name = name, Length = length, Offset = offset, Data = br.ReadBytes(length) });
                    br.BaseStream.Position = store;
                }
            }
        }
        private void PopulateList()
        {
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".CLS")).ToList())
            {
                entry.Data = FfuhDecoder.Decompress(entry.Data!);
                listBox1.Items.Add(entry.Name);
            }
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".ATM")).ToList())
            {
                entry.Data = FfuhDecoder.Decompress(entry.Data!);
            }
            foreach (var entry in palettes.Where(e => e.Name.EndsWith(".PAL")))
            {
                entry.Data = FfuhDecoder.Decompress(entry.Data!);
                listBox2.Items.Add(entry.Name);
            }
            foreach (var entry in palettes.Where(e => e.Name.EndsWith(".SHH")))
            {
                entry.Data = FfuhDecoder.Decompress(entry.Data!);
                listBox3.Items.Add(entry.Name);
            }
            listBox1.SelectedIndex = listBox1.FindStringExact(selectedEntry);
        }
        // ── Load matching ATM whenever a CLS is selected ──────────────────────
        private void LoadMatchingAtm(string clsName)
        {
            string atmName = Path.ChangeExtension(clsName, ".ATM");
            string diskPath = Path.Combine("MAPS", atmName);
            WowFileEntry atmEntry = entries.FirstOrDefault(e => e.Name.Equals(atmName))!;
            atmData = File.Exists(diskPath) ? File.ReadAllBytes(diskPath) : atmEntry.Data!;
        }
        private void TryAutoSelectShader()
        {
            if (listBox3.Items.Count == 0 || listBox3.SelectedIndex >= 0) return;
            for (int i = 0; i < listBox3.Items.Count; i++)
            {
                if (listBox3.Items[i].ToString()!.StartsWith("LANDR0"))
                {
                    listBox3.SelectedIndex = i;
                    return;
                }
            }
            listBox3.SelectedIndex = 0;
        }
        // ── Auto-select palette / shader (once only) ──────────────────────────
        private void TryAutoSelectPalette()
        {
            if (listBox2.Items.Count == 0 || listBox2.SelectedIndex >= 0) return;
            string[] preferred = { "CDSEPIA.PAL", "CD1.PAL", "CDNORM.PAL" };
            foreach (string p in preferred)
            {
                int idx = listBox2.FindStringExact(p);
                if (idx >= 0) { listBox2.SelectedIndex = idx; return; }
            }
            listBox2.SelectedIndex = 0;
        }
        // set output path button
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
        // export all button
        private void button3_Click(object sender, EventArgs e)
        {
            int count = 0;
            foreach (string clsName in listBox1.Items)
            {
                WowFileEntry clsEntry = entries.FirstOrDefault(en => en.Name.Equals(clsName))!;
                if (clsEntry?.Data == null) continue;

                string atmName = Path.ChangeExtension(clsName, ".ATM");
                WowFileEntry atmEntry = entries.FirstOrDefault(en => en.Name.Equals(atmName))!;

                CLSModel model = CLSDecoder.Decode(clsEntry.Data, atmEntry!.Data);
                string bname = Path.GetFileNameWithoutExtension(clsName);

                using var tileBmp = CLSRenderer.RenderTileMap(model);
                tileBmp.Save(Path.Combine(outputPath, $"{bname}_tilemap.png"), ImageFormat.Png);

                using var hBmp = CLSRenderer.RenderHeightmap(model);
                hBmp.Save(Path.Combine(outputPath, $"{bname}_heightmap.png"), ImageFormat.Png);

                count++;
            }
            MessageBox.Show($"Exported {count} terrain pairs.");
        }
        // export selected button
        private void button2_Click(object sender, EventArgs e)
        {
            string baseName = Path.GetFileNameWithoutExtension(selectedEntry);
            var model = CLSDecoder.Decode(clsData, atmData);

            using var tileBmp = CLSRenderer.RenderTileMap(model);
            tileBmp.Save(Path.Combine(outputPath, $"{baseName}_tilemap.png"), ImageFormat.Png);

            using var hBmp = CLSRenderer.RenderHeightmap(model);
            hBmp.Save(Path.Combine(outputPath, $"{baseName}_heightmap.png"), ImageFormat.Png);

            using var rawBmp = CLSRenderer.RenderHeightmapRaw(model);
            rawBmp.Save(Path.Combine(outputPath, $"{baseName}_heights_raw.png"), ImageFormat.Png);

            MessageBox.Show($"Exported:\n  {baseName}_tilemap.png\n  {baseName}_heightmap.png\n  {baseName}_heights_raw.png");
        }
        // replace selected button
        private void button1_Click(object sender, EventArgs e)
        {
            MessageBox.Show("Import not yet implemented.");
        }
        // render selected sprite with selected palette
        private void RenderCurrent()
        {
            if (clsData.Length == 0) return;

            var model = CLSDecoder.Decode(clsData, atmData);
            UpdateInfoLabel(model);

            Bitmap bmp = currentView switch
            {
                ViewMode.TileMap => CLSRenderer.RenderTileMap(model),
                ViewMode.Heightmap => CLSRenderer.RenderHeightmap(model),
                _ => CLSRenderer.RenderComposite(model)
            };

            var old = pictureBox1.Image;
            pictureBox1.Image = bmp;
            old?.Dispose();
        }

        private void UpdateInfoLabel(CLSModel model)
        {
            label2.Text =
                $"Grid: {model.GridW}×{model.GridH}  " +
                $"Verts: {model.VertCount:N0}  Tris: {model.TriCount:N0}\r\n" +
                $"ATM tiles: {model.Tiles?.Length ?? 0:N0}  " +
                $"Max height: {(model.Heights.Length > 0 ? model.Heights.Max() : 0)}";
        }
        // cls listbox
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            string clsName = listBox1.SelectedItem!.ToString()!;
            if (clsName == lastSelectedEntry) { return; }
            selectedEntry = clsName;
            lastSelectedEntry = clsName;
            clsData = entries.First(e => e.Name.Equals(selectedEntry)).Data!;
            LoadMatchingAtm(clsName);
            TryAutoSelectPalette();
            TryAutoSelectShader();
            RenderCurrent();
        }
        // pal listbox
        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            string palName = listBox2.SelectedItem!.ToString()!;
            if (palName == lastSelectedPalette) { return; }
            lastSelectedPalette = palName;
            palData = palettes.First(e => e.Name.Equals(palName)).Data!;
            RenderCurrent();
        }
        // shh listbox
        private void listBox3_SelectedIndexChanged(object sender, EventArgs e)
        {
            shadeData = palettes.FirstOrDefault(e => e.Name.Equals(listBox3.SelectedItem!.ToString()!))!.Data![1..513];
            RenderCurrent();
        }
        // export palette button
        private void button5_Click(object sender, EventArgs e)
        {
            byte[] trimmedPalette = new byte[768];
            Array.Copy(palData, 0, trimmedPalette, 0, 768);
            File.WriteAllBytes(outputPath + Path.GetFileNameWithoutExtension(selectedEntry) + (checkBox1.Checked ? "_SHADED.PAL" : ".PAL"), trimmedPalette);
            MessageBox.Show("Shader Mapped Palette Exported");
        }
        // shader tables enabled/disabled
        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            shadeData = checkBox1.Checked ? palettes.FirstOrDefault(e => e.Name.Equals(listBox3.SelectedItem!.ToString()!))!.Data![1..513] : null;
            RenderCurrent();
        }
    }
}