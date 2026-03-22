using System.Drawing.Imaging;

namespace WoWViewer
{
    public partial class WOFViewer : Form
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
        private bool modelType;      // false = WOF units, true = IOB buildings
        private int currentFrame;

        private WofModel? currentModel;      // non-null when modelType == false
        private IobModel? currentIobModel;   // non-null when modelType == true

        public WOFViewer(List<WowFileEntry> entryList, string entryName, string output, bool model = false)
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
            string suggested = modelType
                ? IobDecoder.SuggestPalette(entryName)
                : WofDecoder.SuggestPalette(entryName, modelType);
            int idx = listBox2.FindStringExact(suggested);
            if (idx >= 0 && idx != listBox2.SelectedIndex)
                listBox2.SelectedIndex = idx;
        }

        private void TryAutoSelectShader(string palName)
        {
            string suggested = modelType
                ? IobDecoder.SuggestShader(selectedEntry, palName)
                : WofDecoder.SuggestShader(selectedEntry, palName);
            int idx = listBox3.FindStringExact(suggested);
            if (idx >= 0 && idx != listBox3.SelectedIndex)
                listBox3.SelectedIndex = idx;
        }

        // ── Rendering ────────────────────────────────────────────────────────

        private void RenderCurrent()
        {
            byte[]? shdSlice = checkBox1.Checked ? shadeData : null;
            if (modelType)
                RenderCurrentIob(shdSlice);
            else
                RenderCurrentWof(shdSlice);
        }

        private void RenderCurrentWof(byte[]? shdSlice)
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

            var bmp = WofDecoder.RenderTextureAtlas(currentModel!, palData, shdSlice);
            DisplayScaled(bmp, WofDecoder.TexWidth * 3, currentModel!.TexHeight * 3);
        }

        private void RenderCurrentIob(byte[]? shdSlice)
        {
            if (currentRenderedEntry != listBox1.SelectedIndex)
            {
                currentIobModel = IobDecoder.Parse(rawData);
                currentRenderedEntry = listBox1.SelectedIndex;
                label2.Text =
                        $"IOB building  " +
                        $"{currentIobModel.FaceCount} BSP planes  " +
                        $"{currentIobModel.LitTriCount} triangles  " +
                        $"{currentIobModel.HalfWidthScale}×{currentIobModel.HeightScale}px  " +
                        $"anim={currentIobModel.AnimatedFlag}";
            }

            if (currentIobModel == null || currentIobModel.Patches.Length == 0)
            {
                label2.Text = "IOB building — no patch data";
                return;
            }

            var bmp = IobDecoder.RenderTextureAtlas(currentIobModel, palData, shdSlice);
            DisplayScaled(bmp, currentIobModel.HalfWidthScale * 3, currentIobModel.HeightScale * 3);
        }

        private void DisplayScaled(Bitmap bmp, int scaledW, int scaledH)
        {
            var scaled = new Bitmap(scaledW, scaledH);
            using (var g = Graphics.FromImage(scaled))
            {
                g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
                g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.Half;
                g.DrawImage(bmp, 0, 0, scaledW, scaledH);
            }
            var old = pictureBox1.Image;
            pictureBox1.Image = scaled;
            old?.Dispose();
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

        // Export current model
        private void button2_Click(object sender, EventArgs e)
        {
            if (modelType)
                ExportCurrentIob();
            else
                ExportCurrentWof();
        }

        private void ExportCurrentWof()
        {
            string baseName = Path.GetFileNameWithoutExtension(selectedEntry);
            string mtlName = baseName + ".mtl";
            string texName = baseName + "_tex.png";
            var (objText, mtlText) = WofDecoder.ToObj(currentModel!, mtlName, texName);
            File.WriteAllText(Path.Combine(outputPath, baseName + ".obj"), objText);
            File.WriteAllText(Path.Combine(outputPath, mtlName), mtlText);
            ExportAtlasWof(currentModel!, palData, shadeData, Path.Combine(outputPath, baseName + "_tex.png"));
            MessageBox.Show($"Exported {baseName}.obj, {mtlName}, {baseName}_tex.png");
        }

        private void ExportCurrentIob()
        {
            if (currentIobModel == null) return;
            string baseName = Path.GetFileNameWithoutExtension(selectedEntry);
            string mtlName = baseName + ".mtl";
            string texName = baseName + "_tex.png";

            var (objText, mtlText) = IobDecoder.ToObj(currentIobModel, mtlName, texName);
            File.WriteAllText(Path.Combine(outputPath, baseName + ".obj"), objText);
            File.WriteAllText(Path.Combine(outputPath, mtlName), mtlText);

            ExportAtlasIob(currentIobModel, palData, null,  // shader not baked into export
                Path.Combine(outputPath, texName));

            MessageBox.Show($"Exported {baseName}.obj ({currentIobModel.FaceCount} verts, {currentIobModel.LitTriCount} faces), {mtlName}, {texName}");
        }

        // Export all models
        private void button3_Click(object sender, EventArgs e)
        {
            if (modelType)
                ExportAllIob();
            else
                ExportAllWof();
        }

        private void ExportAllWof()
        {
            int count = 0;
            foreach (var entry in entries.Where(en => en.Name.EndsWith(".WOF", StringComparison.OrdinalIgnoreCase)))
            {
                try
                {
                    var model = WofDecoder.Parse(entry.Data!);
                    string baseName = Path.GetFileNameWithoutExtension(entry.Name);
                    string mtlN = baseName + ".mtl";
                    string texN = baseName + "_tex.png";

                    string palName = WofDecoder.SuggestPalette(entry.Name, false);
                    var palEntry = entries.FirstOrDefault(en =>
                        en.Name.Equals(palName, StringComparison.OrdinalIgnoreCase));
                    byte[] pal = palEntry?.Data ?? palData;

                    string shdName = WofDecoder.SuggestShader(entry.Name, palName);
                    var shdEntry = entries.FirstOrDefault(en =>
                        en.Name.Equals(shdName, StringComparison.OrdinalIgnoreCase));
                    byte[]? shd = shdEntry?.Data?.Length >= 513 ? shdEntry.Data[1..513] : null;

                    var (objText, mtlText) = WofDecoder.ToObj(model, mtlN, texN);
                    File.WriteAllText(Path.Combine(outputPath, baseName + ".obj"), objText);
                    File.WriteAllText(Path.Combine(outputPath, mtlN), mtlText);
                    ExportAtlasWof(model, pal, shd, Path.Combine(outputPath, baseName + "_tex.png"));
                    count++;
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"WOF export failed for {entry.Name}: {ex.Message}");
                }
            }
            MessageBox.Show($"Exported {count} WOF models to {outputPath}");
        }

        private void ExportAllIob()
        {
            int count = 0;
            foreach (var entry in entries.Where(en =>
                en.Name.EndsWith(".IOB", StringComparison.OrdinalIgnoreCase)))
            {
                try
                {
                    var model = IobDecoder.Parse(entry.Data!);
                    if (model.FaceCount == 0) continue;

                    string base_ = Path.GetFileNameWithoutExtension(entry.Name);
                    string mtlN = base_ + ".mtl";
                    string texN = base_ + "_tex.png";

                    string palName = IobDecoder.SuggestPalette(entry.Name);
                    var palEntry = entries.FirstOrDefault(en =>
                        en.Name.Equals(palName, StringComparison.OrdinalIgnoreCase));
                    byte[] pal = palEntry?.Data ?? palData;

                    string shdName = IobDecoder.SuggestShader(entry.Name, palName);
                    var shdEntry = entries.FirstOrDefault(en =>
                        en.Name.Equals(shdName, StringComparison.OrdinalIgnoreCase));
                    byte[]? shd = shdEntry?.Data?.Length >= 513 ? shdEntry.Data[1..513] : null;

                    var (objText, mtlText) = IobDecoder.ToObj(model, mtlN, texN);
                    File.WriteAllText(Path.Combine(outputPath, base_ + ".obj"), objText);
                    File.WriteAllText(Path.Combine(outputPath, mtlN), mtlText);
                    ExportAtlasIob(model, pal, null, Path.Combine(outputPath, texN));
                    count++;
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"IOB export failed for {entry.Name}: {ex.Message}");
                }
            }
            MessageBox.Show($"Exported {count} IOB buildings to {outputPath}");
        }

        // Import/Replace — OBJ → WOF  or  raw PNG → IOB
        private void button1_Click(object sender, EventArgs e)
        {
            if (modelType)
                ReplaceIob();
            else
                ReplaceWof();
        }

        // ── IOB replace ───────────────────────────────────────────────────────
        // IOB buildings consist of:
        //   • BSP planes  — 3D collision geometry (vertex positions)
        //   • Normals     — surface normals for lighting
        //   • Index table — per-triangle screen positions and normal indices (fixed)
        //   • Patch data  — compressed sprite pixels                         (fixed)
        //
        // Importing an OBJ replaces only the BSP planes and normals.  The sprite
        // appearance (index table + patches) is unchanged — the game renders IOB
        // buildings as pre-rendered sprites; the 3D mesh is collision/lighting only.
        //
        // The imported OBJ MUST have exactly the same number of vertices as the
        // original model (model.FaceCount), because the index table v0/v1/v2 fields
        // reference normals[0..FaceCount-1] and cannot be changed without rewriting
        // all patch offsets.
        //
        // Workflow:
        //   1. Export the current IOB (button2) — writes <n>.obj + <n>_tex.png.
        //   2. Edit the OBJ in your modelling tool (move/rotate/scale geometry).
        //      Do NOT add or remove vertices — only reposition existing ones.
        //   3. Click Replace, select the edited .obj.
        private void ReplaceIob()
        {
            if (currentIobModel == null)
            {
                MessageBox.Show("Load an IOB file first.", "No model");
                return;
            }

            // IOB import: load an OBJ for geometry and the matching _tex.png for texture,
            // exactly like WOF. Both are applied in one operation.
            using var ofd = new OpenFileDialog
            {
                Filter = "OBJ Model|*.obj",
                Title = "Select OBJ to import (matching _tex.png will be loaded automatically)"
            };
            if (ofd.ShowDialog() != DialogResult.OK) return;

            string objPath = ofd.FileName;
            string dir = Path.GetDirectoryName(objPath)!;
            string baseName = Path.GetFileNameWithoutExtension(objPath);
            string texPath = Path.Combine(dir, baseName + "_tex.png");

            if (!File.Exists(texPath))
            {
                MessageBox.Show($"Cannot find {baseName}_tex.png next to the OBJ.\n" +
                                "Export the current IOB first to generate the texture file.",
                                "Missing Texture");
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
                byte[] pngBytes = File.ReadAllBytes(texPath);

                // Apply geometry update then texture update.
                var newModel = IobEncoder.FromObj(objText, currentIobModel!);
                newModel = IobEncoder.ReplaceTexture(newModel, pngBytes, palData);

                SaveIob(newModel,
                    $"{newModel.FaceCount} BSP verts, {newModel.LitTriCount} triangles, texture replaced");
            }
            catch (Exception ex)
            {
                MessageBox.Show($"IOB import failed: {ex.Message}", "Error");
            }
        }

        // Shared save + refresh for IOB import operations.
        private void SaveIob(IobModel newModel, string detail)
        {
            byte[] encoded = IobEncoder.Encode(newModel);
            string outPath = Path.Combine("DAT", selectedEntry);

            if (File.Exists(outPath) &&
                MessageBox.Show($"'{outPath}' exists — overwrite?", "Overwrite",
                    MessageBoxButtons.YesNo) == DialogResult.No)
                return;

            Directory.CreateDirectory("DAT");
            File.WriteAllBytes(outPath, encoded);

            entries.First(en =>
                en.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data = encoded;
            rawData = encoded;
            currentIobModel = newModel;
            currentRenderedEntry = -1;
            RenderCurrent();

            MessageBox.Show($"Saved to {outPath}  ({detail})");
        }


        // ── WOF replace ───────────────────────────────────────────────────────
        private void ReplaceWof()
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

            try
            {
                string objText = File.ReadAllText(objPath);
                string mtlText = File.ReadAllText(mtlPath);
                byte[] texPng = File.ReadAllBytes(texPath);
                byte[]? origWof = rawData?.Length > 0 ? rawData : null;

                byte[] encoded = WofEncoder.Encode(objText, mtlText, texPng, palData, origWof);

                string outPath = Path.Combine("DAT", selectedEntry);
                if (File.Exists(outPath) &&
                    MessageBox.Show($"'{outPath}' exists — overwrite?", "Overwrite",
                        MessageBoxButtons.YesNo) == DialogResult.No)
                    return;

                Directory.CreateDirectory("DAT");
                File.WriteAllBytes(outPath, encoded);

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

        // ── Atlas export helpers ──────────────────────────────────────────────

        private static void ExportAtlasWof(WofModel model, byte[] pal, byte[]? shd, string path)
        {
            var bmp = WofDecoder.RenderTextureAtlas(model, pal, shd);
            bmp.Save(path, ImageFormat.Png);
            bmp.Dispose();
        }

        private static void ExportAtlasIob(IobModel model, byte[] pal, byte[]? shd, string path)
        {
            var bmp = IobDecoder.RenderTextureAtlas(model, pal, shd);
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