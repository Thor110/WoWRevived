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
    //   [0x18]  uint32  h18        (= 0)      — padding
    //   [0x1C]  uint32  h1C        (= 2)      — metadata
    //   [0x20]  uint32  h20        (= 171)    — metadata
    //   [0x24]  uint32  h24        (= 1310982)— metadata
    //   [0x28]  uint32  h28        (= 927004) — metadata
    //   [0x2C]  uint32  h2C        (= 76947)  — metadata / strip section offset
    //   [0x30]  uint32  h30        (= 0)      — padding
    //   [0x34]  uint32  h34        (= 248)    — metadata
    //   [0x38]  uint32  h38        (= 63001)  — = VertCount again
    //   [0x3C]  uint32  h3C        (= 313001) — metadata
    //   [0x40]  uint32  h40        (= 465253) — metadata
    //   [0x44..0x4F]    zeros      — padding to 80-byte header boundary
    //
    //   [80 .. 80+VertCount-1]  uint8[] heights (row-major, 251 cols × 251 rows)
    //   [80+VertCount ..]       uint16[] triangle strip indices with 0xFFFF restart
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

            // Overwrite just the height array at offset 80
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

        // ── Import heights from OBJ ───────────────────────────────────────────
        /// <summary>
        /// Reads vertex Y values from a Wavefront OBJ exported by CLSDecoder.ExportObj
        /// and converts them back to height bytes using the inverse of the engine formula:
        ///   height_byte = (world_Y * 65536) / HeightScale
        /// X and Z are used to recover the grid position (col = X/256, row = Z/256).
        /// </summary>
        public static void ImportHeightsFromObj(CLSModel model, string objPath)
        {
            if (model.HeightScale == 0)
                throw new InvalidDataException("Model has no HeightScale — was it decoded from a valid CLS?");

            var newHeights = new byte[model.VertCount];
            int found = 0;

            foreach (string raw in File.ReadLines(objPath))
            {
                string line = raw.Trim();
                if (!line.StartsWith("v ")) continue;

                string[] parts = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length < 4) continue;

                if (!float.TryParse(parts[1], System.Globalization.NumberStyles.Float,
                        System.Globalization.CultureInfo.InvariantCulture, out float fx)) continue;
                if (!float.TryParse(parts[2], System.Globalization.NumberStyles.Float,
                        System.Globalization.CultureInfo.InvariantCulture, out float fy)) continue;
                if (!float.TryParse(parts[3], System.Globalization.NumberStyles.Float,
                        System.Globalization.CultureInfo.InvariantCulture, out float fz)) continue;

                // Invert world-space coords back to grid position and height byte
                int col = (int)Math.Round(fx / 256.0);
                int row = (int)Math.Round(fz / 256.0);
                int heightByte = Math.Clamp(
                    (int)Math.Round((double)fy * 65536.0 / model.HeightScale),
                    0, 255);

                if (col < 0 || col >= model.GridW || row < 0 || row >= model.GridH) continue;

                newHeights[row * model.GridW + col] = (byte)heightByte;
                found++;
            }

            if (found != model.VertCount)
                throw new InvalidDataException(
                    $"OBJ has {found} vertices but model expects {model.VertCount}. " +
                    "Make sure this OBJ was exported from the same map.");

            model.Heights = newHeights;
        }

        // ── Import tile map from PNG ──────────────────────────────────────────
        /// <summary>
        /// Placeholder for importing a recoloured tile-map PNG back to ATM tile indices.
        /// Not yet implemented — requires the false-colour → tile-ID reverse map.
        /// </summary>
        public static void ImportTilesFromPng(CLSModel model, string pngPath)
        {
            throw new NotImplementedException("Tile map import from PNG is not yet implemented.");
        }
    }
}