using System.Drawing.Imaging;
using System.Text;

namespace WoWViewer
{
    public partial class WOFConverter : Form
    {
        private List<WowFileEntry> entries;
        private string selectedEntry;
        private string outputPath = "";
        private string lastSelectedEntry = "";
        private int currentRenderedEntry = -1;
        private string lastSelectedPalette = "";
        private byte[] rawData = [];
        private byte[] palData = [];
        private byte[]? shadeData;   // active SHH level-0 slice (512 bytes), null = raw PAL
        private bool modelType;    // false = WOF units, true = IOB buildings
        private int currentFrame;

        private WofModel? currentModel;

        public WOFConverter(List<WowFileEntry> entryList, string entryName, string output, bool model = false)
        {
            InitializeComponent();
            entries = entryList;
            selectedEntry = entryName;
            modelType = model;
            if (output != "")
            {
                outputPath = output;
                textBox1.Text = outputPath;
                button2.Enabled = button3.Enabled = button5.Enabled = true;
            }
            PopulateList();
        }

        // ── Population ────────────────────────────────────────────────────────

        private void PopulateList()
        {
            string ext = modelType ? ".IOB" : ".WOF";
            foreach (var entry in entries
                .Where(e => e.Name.EndsWith(ext, StringComparison.OrdinalIgnoreCase))
                .ToList())
            {
                entry.Data = File.Exists($"DAT\\{entry.Name}")
                    ? File.ReadAllBytes($"DAT\\{entry.Name}")
                    : FfuhDecoder.Decompress(entry.Data!);
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

        // ── Auto-select helpers ───────────────────────────────────────────────

        private void TryAutoSelectPalette(string entryName)
        {
            string suggested = WofDecoder.SuggestPalette(entryName, modelType);
            int idx = listBox2.FindStringExact(suggested);
            if (idx >= 0 && idx != listBox2.SelectedIndex)
                listBox2.SelectedIndex = idx;
        }

        private void TryAutoSelectShader(string palName)
        {
            string suggested = WofDecoder.SuggestShader(selectedEntry, palName);
            int idx = listBox3.FindStringExact(suggested);
            if (idx >= 0 && idx != listBox3.SelectedIndex)
                listBox3.SelectedIndex = idx;
        }

        // ── Rendering ────────────────────────────────────────────────────────

        private void RenderCurrent()
        {
            if (currentRenderedEntry != listBox1.SelectedIndex)
            {
                currentModel = WofDecoder.Parse(rawData);
                currentRenderedEntry = listBox1.SelectedIndex;
                label2.Text =
                        $"{currentModel.PieceCount} pieces  " +
                        $"{currentModel.Pieces.Sum(p => (int)p.VertCount)} verts  " +
                        $"{currentModel.Pieces.Sum(p => (int)p.FaceCount)} faces";
            }

            byte[]? shdSlice = checkBox1.Checked ? shadeData : null;
            // Render atlas at actual size, scale up for visibility (width ×3, height ×3)
            var bmp = WofDecoder.RenderTextureAtlas(currentModel!, palData, shdSlice);
            int scaledW = WofDecoder.TexWidth * 3;
            int scaledH = currentModel!.TexHeight * 3;
            var scaled = new Bitmap(scaledW, scaledH);
            using (var g = Graphics.FromImage(scaled))
            {
                g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
                g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.Half;
                g.DrawImage(bmp, 0, 0, scaledW, scaledH);
            }
            pictureBox1.Image = scaled;
            bmp.Dispose();
        }

        // ── Event handlers ────────────────────────────────────────────────────

        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            string name = listBox1.SelectedItem!.ToString()!;
            if (name == lastSelectedEntry) return;
            selectedEntry = name;
            lastSelectedEntry = name;
            rawData = entries.First(en =>
                en.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data!;
            currentRenderedEntry = -1;
            TryAutoSelectPalette(selectedEntry);
            RenderCurrent();
        }

        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            string palName = listBox2.SelectedItem!.ToString()!;
            if (palName == lastSelectedPalette) return;
            lastSelectedPalette = palName;
            palData = entries.First(en =>
                en.Name.Equals(palName, StringComparison.OrdinalIgnoreCase)).Data!;
            TryAutoSelectShader(palName);
            RenderCurrent();
        }

        private void listBox3_SelectedIndexChanged(object sender, EventArgs e)
        {
            var entry = entries.FirstOrDefault(en =>
                en.Name.Equals(listBox3.SelectedItem!.ToString()!, StringComparison.OrdinalIgnoreCase));
            if (entry?.Data == null) return;
            // SHH: 1-byte count + count × 512 bytes. Level 0 = bytes [1..512].
            shadeData = entry.Data.Length >= 513 ? entry.Data[1..513] : null;
            if (checkBox1.Checked) RenderCurrent();
        }

        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            if (checkBox1.Checked && listBox3.SelectedItem != null)
            {
                var entry = entries.FirstOrDefault(en =>
                    en.Name.Equals(listBox3.SelectedItem.ToString()!, StringComparison.OrdinalIgnoreCase));
                shadeData = entry?.Data?.Length >= 513 ? entry.Data[1..513] : null;
            }
            RenderCurrent();
        }
        // set output directory
        private void button4_Click(object sender, EventArgs e)
        {
            using var fbd = new FolderBrowserDialog
            {
                InitialDirectory = outputPath != "" ? outputPath : AppDomain.CurrentDomain.BaseDirectory
            };
            if (fbd.ShowDialog() != DialogResult.OK) return;
            outputPath = fbd.SelectedPath;
            if (!outputPath.EndsWith("\\")) outputPath += "\\";
            textBox1.Text = outputPath;
            button2.Enabled = button3.Enabled = button5.Enabled = true;
        }

        // Export current model as OBJ + MTL + texture atlas PNG
        private void button2_Click(object sender, EventArgs e)
        {
            string baseName = Path.GetFileNameWithoutExtension(selectedEntry);
            string mtlName = baseName + ".mtl";
            string texName = baseName + "_tex.png";
            var (objText, mtlText) = WofDecoder.ToObj(currentModel!, mtlName, texName);
            File.WriteAllText(Path.Combine(outputPath, baseName + ".obj"), objText);
            File.WriteAllText(Path.Combine(outputPath, mtlName), mtlText);
            // Display PNG (RGBA, for viewing/editing)
            ExportAtlas(currentModel, palData, checkBox1.Checked ? shadeData : null,
                Path.Combine(outputPath, baseName + "_tex.png"));
            // Raw-index PNG (palette-indexed 8bpp) — used by the encoder for lossless round-trips
            WofDecoder.ExportTextureRaw(currentModel!, palData,
                Path.Combine(outputPath, baseName + "_raw.png"));
            MessageBox.Show($"Exported {baseName}.obj, {mtlName}, {baseName}_tex.png, {baseName}_raw.png");
        }

        // Export all models
        private void button3_Click(object sender, EventArgs e)
        {
            string ext = modelType ? ".IOB" : ".WOF";
            int count = 0;
            foreach (var entry in entries.Where(en =>
                en.Name.EndsWith(ext, StringComparison.OrdinalIgnoreCase)))
            {
                try
                {
                    var model = WofDecoder.Parse(entry.Data!);
                    string base_ = Path.GetFileNameWithoutExtension(entry.Name);
                    string mtlN = base_ + ".mtl";
                    string texN = base_ + "_tex.png";

                    string palName = WofDecoder.SuggestPalette(entry.Name, modelType);
                    var palEntry = entries.FirstOrDefault(en =>
                        en.Name.Equals(palName, StringComparison.OrdinalIgnoreCase));
                    byte[] pal = palEntry?.Data ?? palData;

                    string shdName = WofDecoder.SuggestShader(entry.Name, palName);
                    var shdEntry = entries.FirstOrDefault(en =>
                        en.Name.Equals(shdName, StringComparison.OrdinalIgnoreCase));
                    byte[]? shd = shdEntry?.Data?.Length >= 513 ? shdEntry.Data[1..513] : null;

                    var (objText, mtlText) = WofDecoder.ToObj(model, mtlN, texN);
                    File.WriteAllText(Path.Combine(outputPath, base_ + ".obj"), objText);
                    File.WriteAllText(Path.Combine(outputPath, mtlN), mtlText);
                    ExportAtlas(model, pal, shd, Path.Combine(outputPath, base_ + "_tex.png"));
                    WofDecoder.ExportTextureRaw(model, pal, Path.Combine(outputPath, base_ + "_raw.png"));
                    count++;
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"WOF export failed for {entry.Name}: {ex.Message}");
                }
            }
            MessageBox.Show($"Exported {count} {ext} models to {outputPath}");
        }

        // Import: select OBJ → encode back to WOF and save to DAT folder
        private void button1_Click(object sender, EventArgs e)
        {
            using var ofd = new OpenFileDialog
            {
                Filter = "OBJ Model|*.obj",
                Title = "Select OBJ to encode as WOF"
            };
            if (ofd.ShowDialog() != DialogResult.OK) return;

            string objPath = ofd.FileName;
            string dir = Path.GetDirectoryName(objPath)!;
            string baseName = Path.GetFileNameWithoutExtension(objPath);
            string mtlPath = Path.Combine(dir, baseName + ".mtl");
            string texPath = Path.Combine(dir, baseName + "_tex.png");
            // Prefer the palette-indexed raw PNG for lossless round-trips.
            // If the user has replaced the texture, they can delete _raw.png to force requantisation.
            string rawPath = Path.Combine(dir, baseName + "_raw.png");
            string encodeTexPath = File.Exists(rawPath) ? rawPath : texPath;

            if (!File.Exists(mtlPath))
            {
                MessageBox.Show($"Cannot find {baseName}.mtl next to the OBJ.", "Missing MTL");
                return;
            }
            if (!File.Exists(texPath))
            {
                MessageBox.Show($"Cannot find {baseName}_tex.png next to the OBJ.", "Missing Texture");
                return;
            }
            if (palData == null || palData.Length < 768)
            {
                MessageBox.Show("Select a palette file first.", "No Palette");
                return;
            }

            try
            {
                string objText = File.ReadAllText(objPath);
                string mtlText = File.ReadAllText(mtlPath);
                byte[] texPng = File.ReadAllBytes(encodeTexPath);

                // Pass original WOF data so animation frames are preserved
                byte[]? origWof = rawData?.Length > 0 ? rawData : null;

                byte[] encoded = WofEncoder.Encode(objText, mtlText, texPng, palData, origWof);

                string outPath = Path.Combine("DAT", selectedEntry);
                if (File.Exists(outPath) &&
                    MessageBox.Show($"'{outPath}' exists — overwrite?", "Overwrite",
                        MessageBoxButtons.YesNo) == DialogResult.No)
                    return;

                Directory.CreateDirectory("DAT");
                File.WriteAllBytes(outPath, encoded);

                // Update in-memory entry so the viewer reflects the change immediately
                entries.First(en =>
                    en.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data = encoded;
                rawData = encoded;
                currentRenderedEntry = -1;
                RenderCurrent();

                MessageBox.Show($"Encoded and saved to {outPath}");
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Encoding failed: {ex.Message}", "Error");
            }
        }

        private static void ExportAtlas(WofModel model, byte[] pal, byte[]? shd, string path)
        {
            var bmp = WofDecoder.RenderTextureAtlas(model, pal, shd);
            bmp.Save(path, ImageFormat.Png);
            bmp.Dispose();
        }

        // Export palette
        private void button5_Click(object sender, EventArgs e)
        {
            if (palData == null || palData.Length < 768) return;
            byte[] trimmed = new byte[768];
            Array.Copy(palData, 0, trimmed, 0, 768);
            File.WriteAllBytes(
                outputPath + Path.GetFileNameWithoutExtension(selectedEntry) +
                (checkBox1.Checked ? "_SHADED.PAL" : ".PAL"), trimmed);
            MessageBox.Show("Palette exported.");
        }
    }
}