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
            ExportAtlas(currentModel, palData, checkBox1.Checked ? shadeData : null,
                Path.Combine(outputPath, baseName + "_tex.png"));
            MessageBox.Show($"Exported {baseName}.obj, {mtlName}, {baseName}_tex.png");
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
                    count++;
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"WOF export failed for {entry.Name}: {ex.Message}");
                }
            }
            MessageBox.Show($"Exported {count} {ext} models to {outputPath}");
        }

        // replace model button obj + mtl + png
        private void button1_Click(object sender, EventArgs e)
        {
            using var ofd = new OpenFileDialog { Filter = "PNG Image|*.png", Title = "Select a replacement to encode" };
            if (ofd.ShowDialog() != DialogResult.OK) { return; }
            var bmp = new Bitmap(ofd.FileName);
            //byte[] indices = QuantiseToPalette(bmp, palData, (checkBox1.Checked) ? shadeData : null);
            // TODO : remove checkBox1?
            byte[] encoded = null!; //TODO
            string outPath = Path.Combine("DAT", selectedEntry);
            if (File.Exists(outPath) && MessageBox.Show($"'{outPath}' exists, overwrite?", "Overwrite", MessageBoxButtons.YesNo) == DialogResult.No) { return; }
            File.WriteAllBytes(outPath, encoded);
            entries.First(e => e.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data = encoded;
            rawData = encoded;
            RenderCurrent();
            MessageBox.Show("Encoded and saved.");
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