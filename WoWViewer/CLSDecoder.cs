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

        // ── Terrain material definitions ──────────────────────────────────────
        // 9 materials derived from height/usage analysis across all 30 maps.
        // 121 tile IDs in 8 base-type groups of 16; Type 1 (water) split into 3.
        private static readonly (string Name, int R, int G, int B, int IdFrom, int IdTo)[] TerrainMaterials =
        {
            ("DeepWater",    0x1A, 0x2A, 0x6A,   1,   1),
            ("ShallowWater", 0x3A, 0x6A, 0x9A,   2,   5),
            ("Coastal",      0x5A, 0x7A, 0x8A,   6,  16),
            ("Beach",        0xC8, 0xB5, 0x60,  17,  32),
            ("Grass",        0x4A, 0x7A, 0x3A,  33,  48),
            ("Road",         0x7A, 0x6A, 0x5A,  49,  64),
            ("Rock",         0x8A, 0x7A, 0x6A,  65,  80),
            ("Highland",     0x9A, 0x8A, 0x7A,  81,  96),
            ("Mountain",     0xAA, 0xAA, 0xAA,  97, 112),
            ("Peak",         0xEE, 0xEE, 0xFF, 113, 128),
        };

        // ── OBJ Export ────────────────────────────────────────────────────────
        /// <summary>
        /// Exports a CLSModel to a Wavefront OBJ + MTL file pair.
        /// World coordinates use the engine's exact scale (confirmed from IDA sub_47B900):
        ///   world_X = col * 256
        ///   world_Y = (height_byte * HeightScale) >> 16
        ///   world_Z = row * 256
        /// Faces are grouped into 9 named terrain materials.
        /// </summary>
        public static void ExportObj(CLSModel model, string objPath)
        {
            string mtlPath = Path.ChangeExtension(objPath, ".mtl");
            string mtlName = Path.GetFileName(mtlPath);
            string baseName = Path.GetFileNameWithoutExtension(objPath);

            ExportMtl(mtlPath);

            using var sw = new System.IO.StreamWriter(objPath, false, System.Text.Encoding.ASCII);

            sw.WriteLine($"# WoWRevived terrain export — {baseName}");
            sw.WriteLine($"# Grid: {model.GridW}×{model.GridH}  Verts: {model.VertCount}  Tris: {model.TriCount}");
            sw.WriteLine($"# World scale: X/Z cell=256  Y=(byte*{model.HeightScale})>>16  YBase={model.YBase}");
            sw.WriteLine($"mtllib {mtlName}");
            sw.WriteLine($"o {baseName}");
            sw.WriteLine();

            // Vertices — engine world-space coordinates
            for (int row = 0; row < model.GridH; row++)
                for (int col = 0; col < model.GridW; col++)
                {
                    int worldX = col * 256;
                    int worldY = (int)((model.Heights[row * model.GridW + col] * (long)model.HeightScale) >> 16);
                    int worldZ = row * 256;
                    sw.WriteLine($"v {worldX} {worldY} {worldZ}");
                }

            sw.WriteLine();

            // Faces grouped by terrain material — CCW winding from above
            // Quad (row,col): Tri1=TL,BL,TR  Tri2=TR,BL,BR  (1-based OBJ indices)
            if (model.Tiles != null)
            {
                foreach (var (matName, _, _, _, idFrom, idTo) in TerrainMaterials)
                {
                    bool headerWritten = false;
                    for (int row = 0; row < model.TileH; row++)
                    {
                        for (int col = 0; col < model.TileW; col++)
                        {
                            byte tileId = model.Tiles[row * model.TileW + col];
                            if (tileId < idFrom || tileId > idTo) continue;

                            if (!headerWritten)
                            {
                                sw.WriteLine($"usemtl {matName}");
                                sw.WriteLine($"g {baseName}_{matName}");
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
        }

        private static void ExportMtl(string mtlPath)
        {
            using var sw = new System.IO.StreamWriter(mtlPath, false, System.Text.Encoding.ASCII);
            sw.WriteLine("# WoWRevived terrain materials");
            sw.WriteLine("# 9 types: DeepWater ShallowWater Coastal Beach Grass Road Rock Highland Mountain Peak");
            sw.WriteLine();
            foreach (var (name, r, g, b, _, _) in TerrainMaterials)
            {
                sw.WriteLine($"newmtl {name}");
                sw.WriteLine($"Kd {r / 255.0:F4} {g / 255.0:F4} {b / 255.0:F4}");
                sw.WriteLine($"Ka 0.1000 0.1000 0.1000");
                sw.WriteLine($"Ks 0.0000 0.0000 0.0000");
                sw.WriteLine();
            }
        }
    }
}