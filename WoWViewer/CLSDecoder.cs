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
        //   [80 .. 80+VertCount-1]  uint8 height array, row-major
        //   [80+VertCount ..]       triangle strip indices (uint16, 0xFFFF restart)

        private const int HeaderSize = 68;
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
    }
}