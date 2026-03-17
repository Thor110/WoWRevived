namespace WoWViewer
{
    // =========================================================================
    // CLSModel  –  decoded terrain data
    // =========================================================================
    public class CLSModel
    {
        // Header fields (confirmed from binary analysis)
        public int GridW { get; set; }   // vertex columns (251)
        public int GridH { get; set; }   // vertex rows    (251)
        public int VertCount { get; set; }   // GridW × GridH = 63001
        public int TriCount { get; set; }   // 250×250×2 = 125000

        // Height array: uint8[VertCount], row-major (row 0 = top), 0 = water
        public byte[] Heights { get; set; } = [];

        // Tile type map from ATM: uint8[TileH × TileW], 1=water, 2+=terrain types
        public byte[]? Tiles { get; set; }
        public int TileW { get; set; }       // 250
        public int TileH { get; set; }       // 250

        // Triangle strip indices (uint16[], 0xFFFF = strip restart)
        // For future OBJ/3D export — not needed for 2D rendering
        public ushort[]? StripIndices { get; set; }

        // World-space scale factors (from CLS header, used for OBJ export/import)
        // world_Y = (height_byte * HeightScale) >> 16
        // world_X = col * 256,  world_Z = row * 256
        public uint HeightScale { get; set; }   // [0x3C] — varies per map
        public int YBase { get; set; }   // [0x18] + [0x30] — vertical datum offset
    }

    // =========================================================================
    // CLSDecoder  –  parses raw CLS + ATM bytes into a CLSModel
    // =========================================================================
    public static class CLSDecoder
    {
        // CLS file layout (confirmed):
        //   [0x00] uint32  GridW      = 251
        //   [0x04] uint32  GridH      = 251
        //   [0x08] uint32  VertCount  = 63001  (GridW × GridH)
        //   [0x0C] uint32  TriCount   = 125000 (250×250×2)
        //   [0x10..0x4F]  additional metadata (section offsets — not needed for height decode)
        //   [68 .. 68+VertCount-1]  uint8 height array, row-major
        //   [68+VertCount ..]       triangle strip indices (uint16, 0xFFFF restart)

        private const int HeaderSize = 68; // corrected header size
        private const ushort StripRestart = 0xFFFF;

        public static CLSModel Decode(byte[] cls, byte[]? atm)
        {
            var model = new CLSModel();

            // ── CLS ───────────────────────────────────────────────────────────
            if (cls != null && cls.Length >= HeaderSize)
            {
                model.GridW = (int)BitConverter.ToUInt32(cls, 0x00);
                model.GridH = (int)BitConverter.ToUInt32(cls, 0x04);
                model.VertCount = (int)BitConverter.ToUInt32(cls, 0x08);
                model.TriCount = (int)BitConverter.ToUInt32(cls, 0x0C);

                // World-space scale factors (from IDA sub_47B900 / sub_47BD80)
                // world_Y = (height_byte * HeightScale) >> 16
                // world_X = col * 256,  world_Z = row * 256
                model.HeightScale = BitConverter.ToUInt32(cls, 0x3C);
                model.YBase = (int)BitConverter.ToUInt32(cls, 0x18)
                                  + (int)BitConverter.ToUInt32(cls, 0x30);

                // Sanity: VertCount should equal GridW × GridH
                int expected = model.GridW * model.GridH;
                if (model.VertCount != expected && expected > 0)
                    model.VertCount = expected;

                // Height array immediately follows the 80-byte header
                int heightsStart = HeaderSize;
                int heightsEnd = heightsStart + model.VertCount;

                if (heightsEnd <= cls.Length)
                {
                    model.Heights = new byte[model.VertCount];
                    Array.Copy(cls, heightsStart, model.Heights, 0, model.VertCount);
                }

                // Triangle strip indices follow the height array
                int stripStart = heightsEnd;
                if (stripStart < cls.Length)
                {
                    var strips = new List<ushort>();
                    int pos = stripStart;
                    while (pos + 1 < cls.Length)
                    {
                        ushort v = BitConverter.ToUInt16(cls, pos);
                        pos += 2;
                        // Stop at clearly invalid data (values > VertCount that aren't restarts)
                        // Allow a short run to get past any transitional data
                        if (v != StripRestart && v >= model.VertCount && strips.Count > 500)
                            break;
                        strips.Add(v);
                    }
                    model.StripIndices = strips.ToArray();
                }
            }

            // ── ATM ───────────────────────────────────────────────────────────
            if (atm != null && atm.Length > 0)
            {
                // ATM dimensions are always (GridW-1) × (GridH-1) — confirmed across all 30 maps.
                // Do NOT infer from ATM byte count: non-square maps (e.g. 176×301) have
                // TileW=175, TileH=300 which sqrt() cannot recover.
                model.TileW = model.GridW - 1;
                model.TileH = model.GridH - 1;
                model.Tiles = atm;
            }

            return model;
        }

        // ── OBJ Export ────────────────────────────────────────────────────────
        /// <summary>
        /// Exports a CLSModel to a Wavefront OBJ + MTL file pair.
        /// World coordinates use the engine's exact scale (confirmed from IDA sub_47B900):
        ///   world_X = col * 256
        ///   world_Y = (height_byte * HeightScale) >> 16
        ///   world_Z = row * 256
        /// Faces are grouped by tile type for material assignment.
        /// </summary>
        public static void ExportObj(CLSModel model, string objPath)
        {
            string mtlPath = Path.ChangeExtension(objPath, ".mtl");
            string mtlName = Path.GetFileName(mtlPath);
            string baseName = Path.GetFileNameWithoutExtension(objPath);

            var usedTiles = new SortedSet<byte>();
            if (model.Tiles != null)
                foreach (byte t in model.Tiles) usedTiles.Add(t);

            ExportMtl(usedTiles, mtlPath);

            using var sw = new System.IO.StreamWriter(objPath, false, System.Text.Encoding.ASCII);

            sw.WriteLine($"# WoWRevived terrain export — {baseName}");
            sw.WriteLine($"# Grid: {model.GridW}×{model.GridH}  Verts: {model.VertCount}  Tris: {model.TriCount}");
            sw.WriteLine($"# World scale: X/Z cell=256  Y=(byte*{model.HeightScale})>>16  YBase={model.YBase}");
            sw.WriteLine($"mtllib {mtlName}");
            sw.WriteLine($"o {baseName}");
            sw.WriteLine();

            // Vertices — engine world-space coordinates
            // world_X = col * 256
            // world_Y = (height_byte * HeightScale) >> 16
            // world_Z = row * 256
            for (int row = 0; row < model.GridH; row++)
                for (int col = 0; col < model.GridW; col++)
                {
                    int worldX = col * 256;
                    int worldY = (int)((model.Heights[row * model.GridW + col] * (long)model.HeightScale) >> 16);
                    int worldZ = row * 256;
                    sw.WriteLine($"v {worldX} {worldY} {worldZ}");
                }

            sw.WriteLine();

            // Faces grouped by tile type — CCW winding from above
            // Quad (row, col) corners (1-based OBJ indices):
            //   TL = row*GridW + col + 1
            //   TR = row*GridW + (col+1) + 1
            //   BL = (row+1)*GridW + col + 1
            //   BR = (row+1)*GridW + (col+1) + 1
            // Tri1: TL, BL, TR   Tri2: TR, BL, BR
            foreach (byte tileId in usedTiles)
            {
                bool headerWritten = false;
                for (int row = 0; row < model.TileH; row++)
                {
                    for (int col = 0; col < model.TileW; col++)
                    {
                        if (model.Tiles![row * model.TileW + col] != tileId) continue;

                        if (!headerWritten)
                        {
                            sw.WriteLine($"usemtl tile_{tileId}");
                            sw.WriteLine($"g {baseName}_tile_{tileId}");
                            headerWritten = true;
                        }

                        int tl = row * model.GridW + col + 1;
                        int tr = row * model.GridW + (col + 1) + 1;
                        int bl = (row + 1) * model.GridW + col + 1;
                        int br = (row + 1) * model.GridW + (col + 1) + 1;

                        sw.WriteLine($"f {tl} {bl} {tr}");
                        sw.WriteLine($"f {tr} {bl} {br}");
                    }
                }
            }
        }

        private static void ExportMtl(SortedSet<byte> tileIds, string mtlPath)
        {
            using var sw = new System.IO.StreamWriter(mtlPath, false, System.Text.Encoding.ASCII);
            sw.WriteLine("# WoWRevived terrain materials");
            sw.WriteLine();

            foreach (byte id in tileIds)
            {
                double r, g, b;
                if (id == 1)
                {
                    r = 0x1A / 255.0; g = 0x3A / 255.0; b = 0x6A / 255.0;
                }
                else
                {
                    double hue = ((id - 2) / 120.0) % 1.0;
                    (r, g, b) = HsvToRgb(hue, 0.72, 0.85);
                }
                sw.WriteLine($"newmtl tile_{id}");
                sw.WriteLine($"Kd {r:F4} {g:F4} {b:F4}");
                sw.WriteLine($"Ka 0.1000 0.1000 0.1000");
                sw.WriteLine($"Ks 0.0000 0.0000 0.0000");
                sw.WriteLine();
            }
        }

        private static (double r, double g, double b) HsvToRgb(double h, double s, double v)
        {
            int hi = (int)(h * 6) % 6;
            double f = h * 6 - Math.Floor(h * 6);
            double p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
            return hi switch
            {
                0 => (v, t, p),
                1 => (q, v, p),
                2 => (p, v, t),
                3 => (p, q, v),
                4 => (t, p, v),
                _ => (v, p, q)
            };
        }
    }
}