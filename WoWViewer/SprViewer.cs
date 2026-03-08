using System.Drawing.Imaging;
using System.Text;

namespace WoWViewer
{
    public partial class SprViewer : Form
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
        private string baseFolder;
        private int currentFrame;
        private Dictionary<string, string> _sprToPal = new();
        private Dictionary<string, string> _sprToShader = new();
        private byte[]? shadeData;   // active shade remap table (level 0, 256 bytes), null = identity
        private static readonly string?[] PalSlots =
        {
            // Palette slot index from OBJ.ojd extra word field.
            // Confirmed via IDA analysis of sub_40B6C0 (LoadShadeTables) and sub_40B520 (slot registration).
            /*  0 */ "HW.PAL",      // UNUSED?
            /*  1 */ "BM.PAL",      // VERIFIED (bld_mark.spr, stlamp.spr) - IDA: BMMB shader
            /*  2 */ "HB.PAL",      // WOF files
            /*  3 */ "BM.PAL",      // WOF files
            /*  4 */ "BM.PAL",      // IOB files
            /*  5 */ "HW.PAL",      // VERIFIED (selcurs.spr) - runtime context slot, world default
            /*  6 */ "HW.PAL",      // VERIFIED (HWM.SPR) - IDA: %sW%sW world shader
            /*  7 */ "HR.PAL",      // VERIFIED (RES_LAMP.SPR, RES_EXIT.SPR) - IDA: %sR%sI radar
            /*  8 */ "HB.PAL",      // VERIFIED (HU_BRIEF.SPR) - IDA: %sB%sI  H/M prefix resolved from sprite filename at runtime
            /*  9 */ "F3.PAL",      // VERIFIED (ragelogo.spr) - runtime context slot, F3 default
            /* 10 */ "BM.PAL",      // VERIFIED (bomb.spr, ripple.spr) - IDA: BMMV shader
            /* 11 */ "BM.PAL",      // UNVERIFIED (mush64.spr, basicm64.spr) - IDA: LANDR shader
            /* 12 */ "F1.PAL",      // VERIFIED (LEGAL.spr, TITLEFMV.spr) - IDA: F1%sI shader
            /* 13 */ "F2.PAL",      // VERIFIED (SEPIATIT.spr)
            /* 14 */ "F3.PAL",      // VERIFIED (CREDITS.spr, topfade.spr)
            /* 15 */ "F4.PAL",      // VERIFIED (gtlogo.spr lowercase)
            /* 16 */ "F5.PAL",      // VERIFIED (legal1.spr, legal2.spr)
            /* 17 */ "F6.PAL",      // VERIFIED (humanbd.spr)
            /* 18 */ "F7.PAL",      // VERIFIED (martbd.spr)
            /* 19 */ "SE.PAL",      // VERIFIED (gtlogo.spr uppercase) - SE.PAL == F1.PAL main palette
            /* 20 */ "BM.PAL",      // UNVERIFIED (TWINK1.SPR, W_FROTH.SPR) - IDA: %sMINIB shader
            /* 21 */ "F1.PAL",      // UNUSED?
            /* 22 */ "CD.PAL",      // VERIFIED (CD_SEP1.spr)
            /* 23 */ "CD.PAL",      // VERIFIED (cd_BD1.spr)
            /* 24 */ "CD.PAL",      // VERIFIED (cd_BD2.spr)
            /* 25 */ "CD.PAL",      // VERIFIED (cd_BD3.spr)
            /* 26 */ "CD.PAL",      // VERIFIED (cd_BD4.spr)
            /* 27 */ "CD.PAL",      // VERIFIED (cd_BD5.spr)
            /* 28 */ "CD.PAL",      // VERIFIED (cd_BD6.spr)
            /* 29 */ "CD.PAL",      // VERIFIED (cd_BD7.spr)
        };

        // Rule confirmed from IDA: .text:0040C452 push offset aDatF1f1S ; "dat\\F1F1.%s"
        // Strip ".PAL", double the two-letter prefix -> shader stem.
        // BM.PAL has no BMBM; use BMGI (general illumination) as closest equivalent.
        // CD sprites (slots 22-29) use individually named shaders via SpriteShaderOverrides.
        // HMIN/MMIN map tiles use no shader (fallback at 0x4A954C is 4 null bytes).
        // CD.PAL player UI buttons use no shader (no entry here, no SpriteShaderOverride).
        private static readonly Dictionary<string, string> PalToShaderStem = new(StringComparer.OrdinalIgnoreCase)
        {
            { "HW.PAL", "HWHW" }, { "MW.PAL", "MWMW" }, { "HB.PAL", "HBHB" },
            { "MB.PAL", "MBMB" }, { "HR.PAL", "HRHR" }, { "MR.PAL", "MRMR" },
            { "BM.PAL", "BMGI" }, // no BMBM exists; BMGI = general illumination
            { "SE.PAL", "SESE" },
            { "F1.PAL", "F1F1" }, { "F2.PAL", "F2F2" }, { "F3.PAL", "F3F3" },
            { "F4.PAL", "F4F4" }, { "F5.PAL", "F5F5" }, { "F6.PAL", "F6F6" },
            { "F7.PAL", "F7F7" },
        };

        // Per-sprite shader overrides. Takes priority over PalToShaderStem.
        // Used for CD FMV backdrop sprites which have individually named shade tables.
        private static readonly Dictionary<string, string> SpriteShaderOverrides = new(StringComparer.OrdinalIgnoreCase)
        {
            { "CD_SEP1.SPR", "CDSEPIA" },
            { "CD_BD1.SPR",  "CD1"  }, { "CD_BD2.SPR", "CD2" },
            { "CD_BD3.SPR",  "CD3"  }, { "CD_BD4.SPR", "CD4" },
            { "CD_BD5.SPR",  "CD5"  }, { "CD_BD6.SPR", "CD6" },
            { "CD_BD7.SPR",  "CD7"  },
        };

        // Per-sprite palette overrides. Takes priority over PalSlots.
        // Used for sprites whose OBJ.ojd slot is a shared runtime-overwritten context
        // that doesn't reflect the palette they actually render against in the game.
        private static readonly Dictionary<string, string> SpritePalOverrides = new(StringComparer.OrdinalIgnoreCase)
        {
            // CD player UI control buttons are in slot 9 (shared F3/context slot) but
            // are rendered against CD.PAL at runtime when the CD player screen is active.
            // These have no SpriteShaderOverride so they get no shader (identity), which
            // is correct: the fallback shader path at 0x4A954C for this context is null.
            { "CDNEXTBT.SPR",  "CD.PAL" }, { "CDPREVBT.SPR",  "CD.PAL" },
            { "CDPLAYBT.SPR",  "CD.PAL" }, { "CDSTOPBT.SPR",  "CD.PAL" },
            { "CDPAUSBT.SPR",  "CD.PAL" }, { "CDFFWDBT.SPR",  "CD.PAL" },
            { "CDRWEDBT.SPR",  "CD.PAL" }, { "CDBTALPH.SPR",  "CD.PAL" },
            { "CD_HL.SPR",     "CD.PAL" }, { "CDHEAD.SPR",    "CD.PAL" },
            { "CDTTRALP.SPR",  "CD.PAL" }, { "CD_TRALP.SPR",  "CD.PAL" },
            { "CDBTRALP.SPR",  "CD.PAL" }, { "CD_DTALP.SPR",  "CD.PAL" },
            { "CDTMALPH.SPR",  "CD.PAL" },
        };
        public SprViewer(List<WowFileEntry> entryList, string entryName, bool maps, string output)
        {
            InitializeComponent();
            entries = entryList;
            selectedEntry = entryName;
            isMaps = maps;
            if (output != "")
            {
                outputPath = output;
                textBox1.Text = outputPath;
                button2.Enabled = true;
                button3.Enabled = true;
                button5.Enabled = true;
            }
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
            BuildSprShaderMap();
            PopulateList();
        }
        // build spr palette map from OBJ.ojd file
        private void BuildSprPalMap()
        {
            if (!isMaps)
            {
                // DAT sprites: palette assignments sourced from OBJ.ojd.
                if (!File.Exists("OBJ.ojd")) { MessageBox.Show("OBJ.ojd file is missing."); return; }
                foreach (var entry in OJDParser.ParseOJDFile())
                {
                    if (!entry.Name.EndsWith(".spr", StringComparison.OrdinalIgnoreCase)) { continue; }
                    string key = Path.GetFileName(entry.Name).ToUpperInvariant();
                    if (_sprToPal.ContainsKey(key)) { continue; } // first occurrence wins

                    // Per-sprite override takes priority over the slot table.
                    if (SpritePalOverrides.TryGetValue(key, out string? overridePal))
                    {
                        _sprToPal[key] = overridePal;
                    }
                    else if (entry.PalSlot < PalSlots.Length && PalSlots[entry.PalSlot] != null)
                    {
                        string pal = PalSlots[entry.PalSlot]!;
                        // IDA: %sB%sI / %sW%sW / %sR%sI — the H/M prefix is resolved from the
                        // sprite filename at runtime, not stored in the slot. If the sprite starts
                        // with M and the slot-assigned PAL starts with H, swap H→M (and vice versa).
                        char sprPrefix = key[0];
                        char palPrefix = pal[0];
                        if ((sprPrefix == 'M' && palPrefix == 'H') || (sprPrefix == 'H' && palPrefix == 'M'))
                            pal = sprPrefix + pal[1..];
                        _sprToPal[key] = pal;
                    }
                }
                return;
            }

            // Maps sprites: not in OBJ.ojd, assigned by filename prefix.
            // sub_4A1F90 confirmed: HMIN* uses HW.PAL context, MMIN* uses MW.PAL context.
            // Fallback shader at 0x4A954C is 4 null bytes -> no shader for map tiles.
            // Other maps sprites (if any) are left unmapped for manual palette selection.
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".SPR", StringComparison.OrdinalIgnoreCase)))
            {
                string key = Path.GetFileName(entry.Name).ToUpperInvariant();
                if (_sprToPal.ContainsKey(key)) { continue; }
                if (key.StartsWith("HMIN")) { _sprToPal[key] = "HW.PAL"; }
                else if (key.StartsWith("MMIN")) { _sprToPal[key] = "MW.PAL"; }
            }
        }
        // Build sprite -> shader filename map.
        // For most sprites: PAL prefix doubled (e.g. MR.PAL -> MRMR.SHL).
        // CD sprites: per-sprite named shader from SpriteShaderOverrides.
        private void BuildSprShaderMap()
        {
            foreach (var kvp in _sprToPal)
            {
                string sprKey = kvp.Key;    // e.g. "HUMANBD.SPR"
                string palName = kvp.Value; // e.g. "MR.PAL"
                if (SpriteShaderOverrides.TryGetValue(sprKey, out string? stem) || PalToShaderStem.TryGetValue(palName, out stem))
                {
                    _sprToShader[sprKey] = stem + ".SHH";
                }
            }
        }// Select the shader for the current sprite from the already-loaded entries.
        private void TryAutoSelectShader()
        {
            if (!checkBox1.Checked) { shadeData = null; return; }
            string key = Path.GetFileName(selectedEntry).ToUpperInvariant();
            if (!_sprToShader.TryGetValue(key, out string? shaderName)) { shadeData = null; return; }
            listBox3.SelectedIndex = listBox3.FindStringExact(shaderName);
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
                if (name.EndsWith(".PAL") || name.EndsWith(".SHH"))
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
            foreach (var entry in (isMaps ? palettes : entries).Where(e => e.Name.EndsWith(".SHH", StringComparison.OrdinalIgnoreCase)))
            {
                entry.Data = FfuhDecoder.Decompress(entry.Data!);
                listBox3.Items.Add(entry.Name);
            }
            listBox1.SelectedIndex = listBox1.FindStringExact(selectedEntry);
        }
        // Auto-select the PAL file that OBJ.ojd says this SPR should use.
        private void TryAutoSelectPalette(string entry)
        {
            string key = entry.ToUpperInvariant();
            if (!_sprToPal.TryGetValue(key, out string? correctPal)) { return; }
            for (int i = 0; i < listBox2.Items.Count; i++)
            {
                if (string.Equals(listBox2.Items[i]?.ToString(), correctPal, StringComparison.OrdinalIgnoreCase))
                {
                    if (i != listBox2.SelectedIndex) { listBox2.SelectedIndex = i; }
                    return;
                }
            }
        }
        // sprite selection
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            string sprName = listBox1.SelectedItem!.ToString()!;
            if (sprName == lastSelectedEntry) { return; }
            selectedEntry = sprName;
            lastSelectedEntry = sprName;
            rawData = entries.First(e => e.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data!;
            TryAutoSelectPalette(selectedEntry);
            TryAutoSelectShader();
            RenderCurrent();
        }
        // palette selection
        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            string palName = listBox2.SelectedItem!.ToString()!;
            if (palName == lastSelectedPalette) { return; }
            lastSelectedPalette = palName;
            palData = (isMaps ? palettes : entries).First(e => e.Name.Equals(palName, StringComparison.OrdinalIgnoreCase)).Data!;
            // PAL structure: bytes 0–767 = main 256-colour VGA palette (6-bit, ×4 = 8-bit).
            // Bytes 768–66303 = colour-blend / translucency table (not used for static viewing).
            // numericUpDown1 is exposed as a "shade level" control (0 = normal brightness).
            // Range 0–255 corresponds to rows in the shade table; 0 is full brightness.
            RenderCurrent();
        }
        // render selected sprite with selected palette
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
            pictureBox1.Image = SprDecoder.Render(rawData, palData, shadeData: shadeData, frame: currentFrame);
        }
        // replace selected sprite
        private void button1_Click(object sender, EventArgs e)
        {
            using var ofd = new OpenFileDialog { Filter = "PNG Image|*.png", Title = "Select a replacement to encode" };
            if (ofd.ShowDialog() != DialogResult.OK) { return; }
            var bmp = new Bitmap(ofd.FileName);
            byte[] indices = QuantiseToPalette(bmp, palData, (checkBox1.Checked) ? shadeData : null);
            byte[] encoded = SprEncoder.Encode(indices, bmp.Width, bmp.Height);
            string outPath = Path.Combine(baseFolder, selectedEntry);
            if (File.Exists(outPath) && MessageBox.Show($"'{outPath}' exists, overwrite?", "Overwrite", MessageBoxButtons.YesNo) == DialogResult.No) { return; }
            File.WriteAllBytes(outPath, encoded);
            (isMaps ? palettes : entries).First(e => e.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase)).Data = encoded;
            rawData = encoded;
            RenderCurrent();
            MessageBox.Show("Encoded and saved.");
        }
        // Quantise each pixel of bmp to the nearest available palette index.
        //
        // When shadeData is null (no shader active):
        //   Search palette entries 1-255 directly. Store the winning index i.
        //   Round-trip: stored i -> pal[i] -> displayed colour.
        //
        // When shadeData is active (512 bytes = 256 × uint16 RGB565 from .SHH level 0):
        //   The engine displays shadeData[i] (as RGB565) for stored index i.
        //   Search over all i, evaluate the displayed colour from the SHH table,
        //   find the nearest match, and store i.
        //
        // Index 0 is always transparent and is never stored for opaque pixels.
        private static byte[] QuantiseToPalette(Bitmap bmp, byte[] palData, byte[]? shadeData)
        {
            int w = bmp.Width, h = bmp.Height;
            byte[] indices = new byte[w * h];
            bool useSHH = shadeData != null && shadeData.Length >= 512;
            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    Color c = bmp.GetPixel(x, y);
                    if (c.A < 128) { indices[y * w + x] = 0; continue; }
                    byte best = 1;
                    int bestErr = int.MaxValue;
                    for (int i = 1; i < 256; i++)
                    {
                        int r, g, b;
                        if (useSHH)
                        {
                            int rgb565 = shadeData![i * 2] | (shadeData[i * 2 + 1] << 8);
                            int rv = ((rgb565 >> 11) & 0x1F); r = (rv << 3) | (rv >> 2);
                            int gv = ((rgb565 >> 5) & 0x3F); g = (gv << 2) | (gv >> 4);
                            int bv = (rgb565 & 0x1F); b = (bv << 3) | (bv >> 2);
                        }
                        else
                        {
                            r = palData[i * 3] * 4;
                            g = palData[i * 3 + 1] * 4;
                            b = palData[i * 3 + 2] * 4;
                        }
                        int err = (c.R - r) * (c.R - r)
                                + (c.G - g) * (c.G - g)
                                + (c.B - b) * (c.B - b);
                        if (err < bestErr) { bestErr = err; best = (byte)i; }
                    }
                    indices[y * w + x] = best;
                }
            }
            return indices;
        }
        // export selected sprite/frame
        private void button2_Click(object sender, EventArgs e)
        {
            string name = (SprDecoder.ReadInfo(rawData).TableCount != 1) ? comboBox1.Text : Path.GetFileNameWithoutExtension(selectedEntry);
            pictureBox1.Image.Save(Path.Combine(outputPath, name + ".png"), ImageFormat.Png);
            MessageBox.Show($"{name}.png exported.");
        }
        // export all sprites
        private void button3_Click(object sender, EventArgs e)
        {
            foreach (WowFileEntry entry in entries.Where(e => e.Name.EndsWith(".SPR", StringComparison.OrdinalIgnoreCase)))
            {
                // Use the correct PAL for this sprite if known; otherwise fall back to selected PAL.
                int frameCount = SprDecoder.ReadInfo(entry.Data!).TableCount;
                string fileName = Path.GetFileNameWithoutExtension(entry.Name);
                TryAutoSelectPalette(entry.Name);
                TryAutoSelectShader();
                if (frameCount != 1)
                {
                    for (int i = 0; i < frameCount; i++)
                    {
                        Bitmap frame = SprDecoder.Render(entry.Data!, palData, shadeData: shadeData, frame: i);
                        frame.Save(Path.Combine(outputPath, $"{fileName}_frame_{i:D2}.png"), ImageFormat.Png);
                        frame.Dispose();
                    }
                }
                else
                {
                    Bitmap img = SprDecoder.Render(entry.Data!, palData, shadeData: shadeData, frame: 0);
                    img.Save(Path.Combine(outputPath, fileName + ".png"), ImageFormat.Png);
                    img.Dispose();
                }
            }
            MessageBox.Show("All .spr files exported.");
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
        // frame change
        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBox1.SelectedIndex == currentFrame) { return; }
            currentFrame = comboBox1.SelectedIndex;
            RenderCurrent();
        }
        // export shader mapped palette
        private void button5_Click(object sender, EventArgs e)
        {
            byte[] trimmedPalette = new byte[768];
            Array.Copy(palData, 0, trimmedPalette, 0, 768);
            if (checkBox1.Checked) // TODO : apply relevant shader mapping
            {
                //remap trimmedPalette etc
                File.WriteAllBytes(outputPath + Path.GetFileNameWithoutExtension(selectedEntry) + "_SHADED.PAL", trimmedPalette);
            }
            else { File.WriteAllBytes(outputPath + Path.GetFileNameWithoutExtension(selectedEntry) + ".PAL", trimmedPalette); }
            MessageBox.Show("Shader Mapped Palette Exported");
        }
        // disable shader data
        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            shadeData = null;
            RenderCurrent();
        }
        // shader listbox
        private void listBox3_SelectedIndexChanged(object sender, EventArgs e)
        {
            byte[] raw = (isMaps ? palettes : entries).FirstOrDefault(e => e.Name.Equals(listBox3.SelectedItem!.ToString()!, StringComparison.OrdinalIgnoreCase))!.Data!;
            shadeData = (raw.Length >= 513) ? raw[1..513] : null;
            RenderCurrent();
        }
    }
}