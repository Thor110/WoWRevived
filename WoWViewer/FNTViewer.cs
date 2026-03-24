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

        // ── Population ────────────────────────────────────────────────────────

        private void PopulateList()
        {
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".FNT", StringComparison.OrdinalIgnoreCase)).ToList())
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

        // ── Event handlers ────────────────────────────────────────────────────

        // font listbox
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            string sprName = listBox1.SelectedItem!.ToString()!;
            if (sprName == lastSelectedEntry) return;
            selectedEntry = sprName;
            lastSelectedEntry = sprName;
            rawData = entries.First(e => e.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data!;
            RenderCurrent();
        }

        // palette listbox
        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            string palName = listBox2.SelectedItem!.ToString()!;
            if (palName == lastSelectedPalette) return;
            lastSelectedPalette = palName;
            palData = entries.First(e => e.Name.Equals(palName, StringComparison.OrdinalIgnoreCase)).Data!;
            RenderCurrent();
        }

        // shader listbox
        private void listBox3_SelectedIndexChanged(object sender, EventArgs e)
        {
            shadeData = entries.FirstOrDefault(e => e.Name.Equals(listBox3.SelectedItem!.ToString()!,
                StringComparison.OrdinalIgnoreCase))!.Data![1..513];
            RenderCurrent();
        }

        // shader checkbox
        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            shadeData = checkBox1.Checked
                ? (isMaps ? palettes : entries).FirstOrDefault(e => e.Name.Equals(
                    listBox3.SelectedItem!.ToString()!, StringComparison.OrdinalIgnoreCase))!.Data![1..513]
                : null;
            RenderCurrent();
        }

        // ── Rendering ────────────────────────────────────────────────────────

        private void RenderCurrent()
        {
            if (palData == null) palData = GenerateFontPalette();

            var fontModel = FNTDecoder.Parse(rawData);
            var atlas = FNTDecoder.RenderFontAtlas(fontModel, palData);

            pictureBox1.Image?.Dispose();
            pictureBox1.Image = atlas;
        }

        // ── Import ────────────────────────────────────────────────────────────

        private void button1_Click(object sender, EventArgs e)
        {
            using var ofd = new OpenFileDialog { Filter = "PNG Image|*.png", Title = "Select a replacement to encode" };
            if (ofd.ShowDialog() != DialogResult.OK) return;

            using var bmp = new Bitmap(ofd.FileName);

            var model = FNTDecoder.Parse(rawData);

            // Pre-cache palette colours for reverse matching.
            Color[] paletteColors = new Color[256];
            for (int i = 0; i < 256; i++)
            {
                int r = palData[i * 3] * 4;
                int g = palData[i * 3 + 1] * 4;
                int b = palData[i * 3 + 2] * 4;
                paletteColors[i] = Color.FromArgb(r, g, b);
            }

            var bmpData = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height),
                              ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            int[] argbPixels = new int[bmp.Width * bmp.Height];
            System.Runtime.InteropServices.Marshal.Copy(bmpData.Scan0, argbPixels, 0, argbPixels.Length);
            bmp.UnlockBits(bmpData);

            int totalWidth = 0;
            foreach (var g in model.Glyphs) totalWidth += g.Width + 2;
            int exportAtlasWidth = Math.Min(totalWidth, 1024);

            int curX = 2, curY = 2;

            for (int i = 0; i < model.Glyphs.Length; i++)
            {
                var glyph = model.Glyphs[i];
                if (glyph.Width <= 0) continue;

                if (curX + glyph.Width > exportAtlasWidth) { curX = 2; curY += model.Height + 4; }

                byte[] newPixels = new byte[glyph.Width * model.Height];

                for (int y = 0; y < model.Height; y++)
                {
                    for (int x = 0; x < glyph.Width; x++)
                    {
                        int bmpX = curX + x, bmpY = curY + y;
                        if (bmpX >= bmp.Width || bmpY >= bmp.Height) continue;

                        int argb = argbPixels[bmpY * bmp.Width + bmpX];
                        int alpha = (argb >> 24) & 0xFF;

                        if (alpha < 128) { newPixels[y * glyph.Width + x] = 0; continue; }

                        int pr = (argb >> 16) & 0xFF;
                        int pg = (argb >> 8) & 0xFF;
                        int pb = argb & 0xFF;

                        int bestIndex = 1, minDiff = int.MaxValue;
                        for (int c = 1; c < 256; c++)
                        {
                            Color palC = paletteColors[c];
                            int dr = pr - palC.R, dg = pg - palC.G, db = pb - palC.B;
                            int diff = dr * dr + dg * dg + db * db;
                            if (diff < minDiff) { minDiff = diff; bestIndex = c; }
                        }
                        newPixels[y * glyph.Width + x] = (byte)bestIndex;
                    }
                }

                glyph.Pixels = newPixels;
                curX += glyph.Width + 2;
            }

            byte[] encodedData = FNTEncoder.Encode(model, rawData);

            string savePath = Path.Combine("DAT", selectedEntry);
            Directory.CreateDirectory("DAT");
            File.WriteAllBytes(savePath, encodedData);

            rawData = encodedData;
            entries.First(ent => ent.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data = rawData;

            RenderCurrent();
            MessageBox.Show($"{selectedEntry} successfully imported, encoded, and saved to the DAT folder!", "Import Complete");
        }

        // ── Export ────────────────────────────────────────────────────────────

        // export selected
        private void button2_Click(object sender, EventArgs e)
        {
            string name = Path.GetFileNameWithoutExtension(selectedEntry);
            pictureBox1.Image.Save(Path.Combine(outputPath, name + ".png"), ImageFormat.Png);
            MessageBox.Show($"{name}.png exported.");
        }

        // export all — one pass per FNT entry, no duplicate writes
        private void button3_Click(object sender, EventArgs e)
        {
            byte[] pal = palData ?? GenerateFontPalette();
            foreach (WowFileEntry entry in entries.Where(e => e.Name.EndsWith(".FNT", StringComparison.OrdinalIgnoreCase)))
            {
                using Bitmap font = FNTDecoder.RenderFontAtlas(FNTDecoder.Parse(entry.Data!), pal);
                font.Save(Path.Combine(outputPath, Path.GetFileNameWithoutExtension(entry.Name) + ".png"), ImageFormat.Png);
            }
            MessageBox.Show("All font files exported as .png files.");
        }

        // export palette
        private void button5_Click(object sender, EventArgs e)
        {
            byte[] trimmedPalette = new byte[768];
            Array.Copy(palData, 0, trimmedPalette, 0, 768);
            File.WriteAllBytes(outputPath + Path.GetFileNameWithoutExtension(selectedEntry) +
                (checkBox1.Checked ? "_SHADED.PAL" : ".PAL"), trimmedPalette);
            MessageBox.Show("Shader Mapped Palette Exported");
        }

        // set output path
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

        // reset to generated grayscale palette
        private void button6_Click(object sender, EventArgs e)
        {
            listBox2.SelectedIndexChanged -= listBox2_SelectedIndexChanged!;
            listBox2.SelectedIndex = -1;
            listBox2.SelectedIndexChanged += listBox2_SelectedIndexChanged!;
            lastSelectedPalette = "";
            palData = GenerateFontPalette();
            RenderCurrent();
        }

        // ── Helpers ───────────────────────────────────────────────────────────

        private byte[] GenerateFontPalette()
        {
            byte[] pal = new byte[768];
            for (int i = 1; i < 256; i++)
            {
                int val = i;
                if (val > 63) val = 63;
                pal[i * 3] = (byte)val;
                pal[i * 3 + 1] = (byte)val;
                pal[i * 3 + 2] = (byte)(i % 64);
            }
            return pal;
        }
    }
}