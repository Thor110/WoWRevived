namespace WoWViewer
{
    // =========================================================================
    // CLSEncoder  –  re-encodes a CLSModel back to CLS + ATM bytes
    // =========================================================================
    // CLS layout (confirmed from binary analysis of LAND01.CLS):
    //
    //   [0x00]  uint32  GridW      (= 251)
    //   [0x04]  uint32  GridH      (= 251)
    //   [0x08]  uint32  VertCount  (= GridW × GridH = 63001)
    //   [0x0C]  uint32  TriCount   (= 250×250×2 = 125000)
    //   [0x10]  uint32  h10        (= 5001)   — metadata, copy verbatim
    //   [0x14]  uint32  h14        (= 3536)   — metadata, copy verbatim
    //   [0x18]  uint32  h18        — metadata, copy verbatim
    //   [0x1C]  uint32  h1C        — metadata, copy verbatim
    //   [0x20]  uint32  h20        — metadata, copy verbatim
    //   [0x24]  uint32  h24        — metadata, copy verbatim
    //   [0x28]  uint32  h28        — metadata, copy verbatim
    //   [0x2C]  uint32  h2C        — metadata / strip section offset
    //   [0x30]  uint32  h30        — metadata, copy verbatim
    //   [0x34]  uint32  h34        — metadata, copy verbatim
    //   [0x38]  uint32  h38        — = VertCount again
    //   [0x3C]  uint32  h3C        — metadata, copy verbatim
    //   [0x40]  uint32  h40        — metadata, copy verbatim
    //                              ↑ header ends here at byte 68 (17 × uint32)
    //
    //   [68 .. 68+VertCount-1]  uint8[] heights (row-major, GridW cols × GridH rows)
    //   [68+VertCount ..]       uint16[] triangle strip indices with 0xFFFF restart
    //                           (rest of file — copy verbatim when only editing heights)
    //
    // ATM layout (confirmed):
    //   [0 .. 62499]  uint8[250×250] tile type indices, row-major
    //                  1 = water, 2–121 = terrain tile types
    //
    // -------------------------------------------------------------------------
    // CURRENT STATUS:
    //   Height-only edits (same grid, same strip indices) are achievable now.
    //   Full re-triangulation (changing grid dimensions) requires regenerating
    //   the strip section — not yet implemented.
    // =========================================================================

    public static class CLSEncoder
    {
        // ── Encode (height-edit round-trip) ───────────────────────────────────
        /// <summary>
        /// Encodes a CLSModel back to CLS bytes.
        /// The original CLS bytes MUST be supplied so that the header metadata and
        /// triangle strip section can be copied verbatim.
        /// Only the height array is rebuilt from model.Heights.
        /// </summary>
        public static byte[] EncodeCls(CLSModel model, byte[] originalCls)
        {
            if (originalCls == null || originalCls.Length < 80)
                throw new ArgumentException("Original CLS data required for encoding.");

            if (model.Heights.Length != model.VertCount)
                throw new ArgumentException($"Heights array length {model.Heights.Length} != VertCount {model.VertCount}");

            // Output is exactly the same size as the original (heights are same count, same uint8)
            byte[] output = new byte[originalCls.Length];

            // Copy the entire original file first (preserves header + strip section verbatim)
            Array.Copy(originalCls, output, originalCls.Length);

            // Overwrite just the height array at offset 68
            int heightsStart = 68;
            Array.Copy(model.Heights, 0, output, heightsStart, model.VertCount);

            return output;
        }

        // ── Encode ATM ────────────────────────────────────────────────────────
        /// <summary>
        /// Encodes tile type indices back to ATM bytes.
        /// ATM is always 250×250 = 62500 raw bytes, no header.
        /// </summary>
        public static byte[] EncodeAtm(CLSModel model)
        {
            int tileCount = model.TileW * model.TileH;
            if (model.Tiles == null || model.Tiles.Length != tileCount)
                throw new ArgumentException($"ATM tile data must be exactly {tileCount} bytes ({model.TileW}×{model.TileH}).");

            byte[] output = new byte[tileCount];
            Array.Copy(model.Tiles, output, tileCount);
            return output;
        }

        // ── Import heights from PNG ───────────────────────────────────────────
        /// <summary>
        /// Reads a grayscale PNG (exported by RenderHeightmapRaw) back into the model's
        /// Heights array. The PNG must be 251×251 pixels.
        /// </summary>
        public static void ImportHeightsFromPng(CLSModel model, string pngPath)
        {
            using var bmp = new System.Drawing.Bitmap(pngPath);
            if (bmp.Width != model.GridW || bmp.Height != model.GridH)
                throw new ArgumentException(
                    $"PNG size {bmp.Width}×{bmp.Height} does not match grid {model.GridW}×{model.GridH}.");

            var heights = new byte[model.VertCount];
            for (int row = 0; row < model.GridH; row++)
                for (int col = 0; col < model.GridW; col++)
                    heights[row * model.GridW + col] = bmp.GetPixel(col, row).R; // R=G=B for grayscale

            model.Heights = heights;
        }

        // ── Import tile map from PNG ──────────────────────────────────────────
        /// <summary>
        /// Placeholder for importing a recoloured tile-map PNG back to ATM tile indices.
        /// Since the tile-map uses false colours, a lookup table approach will be needed.
        /// Not yet implemented — requires the false-colour → tile-ID reverse map.
        /// </summary>
        public static void ImportTilesFromPng(CLSModel model, string pngPath)
        {
            // TODO: Implement reverse colour lookup
            // The CLSRenderer colour table is deterministic (HSV hue ramp),
            // so the reverse mapping is: for each pixel colour, find closest entry in TileArgb.
            throw new NotImplementedException("Tile map import from PNG is not yet implemented.");
        }
    }
}