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
            if (palData == null) palData = GenerateDefaultFontPalette();

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
            using var ofd = new OpenFileDialog { Filter = "PNG Image|*.png", Title = "Select a replacement to encode" };
            if (ofd.ShowDialog() != DialogResult.OK) { return; }

            using var bmp = new Bitmap(ofd.FileName);

            // 1. Decode current raw data to get the layout structure
            var model = FNTDecoder.Parse(rawData);

            // 2. Pre-cache the palette for fast RGB-to-Index reverse matching
            Color[] paletteColors = new Color[256];
            for (int i = 0; i < 256; i++)
            {
                int r = palData[i * 3] * 4;
                int g = palData[i * 3 + 1] * 4;
                int b = palData[i * 3 + 2] * 4;
                paletteColors[i] = Color.FromArgb(r, g, b);
            }

            // Lock bitmap for fast pixel reading
            var bmpData = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            int[] argbPixels = new int[bmp.Width * bmp.Height];
            System.Runtime.InteropServices.Marshal.Copy(bmpData.Scan0, argbPixels, 0, argbPixels.Length);
            bmp.UnlockBits(bmpData);

            int totalWidth = 0;
            foreach (var g in model.Glyphs) totalWidth += g.Width + 2;
            int exportAtlasWidth = Math.Min(totalWidth, 1024);

            int curX = 2;
            int curY = 2;

            // 3. Slice the glyphs out of the imported PNG
            for (int i = 0; i < model.Glyphs.Length; i++)
            {
                var glyph = model.Glyphs[i];
                if (glyph.Width <= 0) continue;

                if (curX + glyph.Width > exportAtlasWidth)
                {
                    curX = 2;
                    curY += model.Height + 4;
                }

                byte[] newPixels = new byte[glyph.Width * model.Height];

                for (int y = 0; y < model.Height; y++)
                {
                    for (int x = 0; x < glyph.Width; x++)
                    {
                        int bmpX = curX + x;
                        int bmpY = curY + y;

                        if (bmpX >= bmp.Width || bmpY >= bmp.Height) continue;

                        int argb = argbPixels[bmpY * bmp.Width + bmpX];
                        int alpha = (argb >> 24) & 0xFF;

                        // Treat anything mostly transparent as Index 0
                        if (alpha < 128)
                        {
                            newPixels[y * glyph.Width + x] = 0;
                        }
                        else
                        {
                            int pr = (argb >> 16) & 0xFF;
                            int pg = (argb >> 8) & 0xFF;
                            int pb = argb & 0xFF;

                            int bestIndex = 1;
                            int minDiff = int.MaxValue;

                            for (int c = 1; c < 256; c++) // Skip index 0 (transparency)
                            {
                                Color palC = paletteColors[c];
                                int dr = pr - palC.R;
                                int dg = pg - palC.G;
                                int db = pb - palC.B;
                                int diff = (dr * dr) + (dg * dg) + (db * db);

                                if (diff < minDiff)
                                {
                                    minDiff = diff;
                                    bestIndex = c;
                                }
                            }

                            newPixels[y * glyph.Width + x] = (byte)bestIndex;
                        }
                    }
                }

                glyph.Pixels = newPixels;
                curX += glyph.Width + 2;
            }

            // 4. Encode the updated model back into raw .FNT bytes
            byte[] encodedData = FNTEncoder.Encode(model, rawData);

            // 5. Save the new FNT file to the DAT directory
            string savePath = Path.Combine("DAT", selectedEntry);

            // Ensure the DAT directory exists just in case
            Directory.CreateDirectory("DAT");
            System.IO.File.WriteAllBytes(savePath, encodedData);

            // 6. Overwrite the viewer's current state and re-render
            rawData = encodedData;
            entries.First(ent => ent.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data = rawData;

            RenderCurrent();
            MessageBox.Show($"{selectedEntry} successfully imported, encoded, and saved to the DAT folder!", "Import Complete");
        }
        private byte[] GenerateDefaultFontPalette()
        {
            byte[] pal = new byte[768]; // 256 colors * 3 bytes (RGB)

            // Index 0 is transparent (0,0,0)

            // Create a smooth grayscale gradient for indices 1 through 31
            for (int i = 1; i < 32; i++)
            {
                // Scale 1-31 up to 0-255 for the RGB values
                byte intensity = (byte)(i * 8);
                if (intensity > 255) intensity = 255;

                // Set R, G, and B to the same value for grayscale (divided by 4 to match VGA 6-bit shift)
                pal[i * 3] = (byte)(intensity / 4);
                pal[i * 3 + 1] = (byte)(intensity / 4);
                pal[i * 3 + 2] = (byte)(intensity / 4);
            }
            return pal;
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
