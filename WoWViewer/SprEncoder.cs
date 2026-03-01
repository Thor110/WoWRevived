// SprEncoder.cs: Encodes raw palette-indexed pixel data into the WoW SPR format.
// Exact inverse of SprDecoder.cs — round-trip verified against original game sprites.
// Produces files that decode pixel-identically to their source data.
//
// Usage:
//   // Single-frame (e.g. a full-screen background):
//   byte[] spr = SprEncoder.Encode(indexedPixels, width, height);
//
//   // Multi-frame (e.g. a button with pressed/unpressed states):
//   byte[] spr = SprEncoder.EncodeFrames(frames, width, height);
//
// indexedPixels : row-major array of palette indices, length = width * height.
//                 Index 0 = transparent (rendered as nothing by the game engine).
//
// See SprDecoder.cs for the full format specification.

namespace WoWViewer
{
    public static class SprEncoder
    {
        // ── Public API ───────────────────────────────────────────────────────────

        /// <summary>
        /// Encode a single-frame sprite from a flat row-major palette-index array.
        /// </summary>
        public static byte[] Encode(byte[] indexedPixels, int width, int height)
        {
            var rows = BuildRleRows(indexedPixels, width, height);
            return EncodeSingleFrame(rows, width, height);
        }

        /// <summary>
        /// Encode a multi-frame sprite (e.g. animation or button states).
        /// Each element of <paramref name="frames"/> is a flat row-major palette-index
        /// array of length width * height.  All frames must have the same dimensions.
        /// </summary>
        public static byte[] EncodeFrames(IReadOnlyList<byte[]> frames, int width, int height)
        {
            if (frames.Count == 1)
            {
                return Encode(frames[0], width, height);
            }

            var frameRle = frames.Select(f => BuildRleRows(f, width, height)).ToList();

            return EncodeMultiFrame(frameRle, width, height);
        }

        // ── Single-frame encoding ────────────────────────────────────────────────
        //
        // File layout:
        //   [0x00] uint16  width
        //   [0x02] uint16  height
        //   [0x04] uint16  tableCount = 1
        //   [0x06] uint16  rowHeaderSize = 10  (= 6 + 1*4)
        //   [0x08] height × 4 bytes  row-offset table
        //          Each 4-byte entry: [carry:1][0x00:1][low16:2]
        //          absolute_offset = carry*65536 + low16 + rowHeaderSize
        //   [0x08 + height*4]  pixel data: rows packed end-to-end
        //          Each row: [paletteIndex:1][runLength:1] pairs until width pixels consumed.

        private static byte[] EncodeSingleFrame(List<List<(byte idx, byte cnt)>> rows, int width, int height)
        {
            const int tc = 1;
            int rhs = 6 + tc * 4;   // rowHeaderSize = 10

            // Pixel data starts immediately after the row-offset table.
            int dataBase = 8 + height * 4;

            // Serialise all RLE rows and record each row's absolute file offset.
            var pixelData = new List<byte>();
            var absOffsets = new int[height];

            for (int row = 0; row < height; row++)
            {
                absOffsets[row] = dataBase + pixelData.Count;
                foreach (var (idx, cnt) in rows[row])
                {
                    pixelData.Add(idx);
                    pixelData.Add(cnt);
                }
            }

            // Build the output buffer.
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms);

            bw.Write((ushort)width);
            bw.Write((ushort)height);
            bw.Write((ushort)tc);
            bw.Write((ushort)rhs);

            // Row-offset table.
            foreach (int abs in absOffsets)
            {
                int val = abs - rhs;
                int carry = val >> 16;
                int low16 = val & 0xFFFF;
                bw.Write((byte)carry);
                bw.Write((byte)0);
                bw.Write((ushort)low16);
            }

            // Pixel data.
            bw.Write(pixelData.ToArray());

            return ms.ToArray();
        }

        // ── Multi-frame encoding ─────────────────────────────────────────────────
        //
        // File layout:
        //   [0x00] uint16  width
        //   [0x02] uint16  height
        //   [0x04] uint16  tableCount  (= number of frames)
        //   [0x06] uint16  rowHeaderSize = width  (matches original game files)
        //   [0x08] height × 4 bytes  row-offset table
        //          Row 0 low16 = baseBlock (the address of the int32 sub-offset array).
        //          All other rows are zeroed — the decoder ignores them for frames 1+.
        //   [0x08 + height*4]  baseBlock starts here:
        //          int32[tableCount]  sub-offsets from baseBlock to each frame's data block.
        //          Immediately followed by the concatenated frame data blocks.
        //
        // The decoder locates the baseBlock by scanning the row table for the first entry
        // whose low16 points to a strictly-ascending int32[tc] array whose first element
        // passes a pixel-count sanity check.  Placing the baseBlock at (8 + height*4) and
        // pointing row 0 at it satisfies this on the first scan iteration.

        private static byte[] EncodeMultiFrame(List<List<List<(byte idx, byte cnt)>>> frameRle, int width, int height)
        {
            int tc = frameRle.Count;
            int rhs = width;  // rowHeaderSize matches game convention for multi-frame

            // Serialise pixel data for every frame.
            var frameBytes = frameRle.Select(frame => SerialiseFrame(frame)).ToList();

            // baseBlock sits immediately after the row-offset table.
            int baseBlock = 8 + height * 4;

            // sub-offsets[f] = offset from baseBlock to frame f's data.
            // Frame data begins after the sub-offset array (tc × 4 bytes).
            int subsSize = tc * 4;
            var subOffsets = new int[tc];
            int cumulative = subsSize;
            for (int f = 0; f < tc; f++)
            {
                subOffsets[f] = cumulative;
                cumulative += frameBytes[f].Length;
            }

            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms);

            bw.Write((ushort)width);
            bw.Write((ushort)height);
            bw.Write((ushort)tc);
            bw.Write((ushort)rhs);

            // Row-offset table: row 0 points at baseBlock; all others zeroed.
            for (int r = 0; r < height; r++)
            {
                if (r == 0)
                {
                    bw.Write((byte)0);
                    bw.Write((byte)0);
                    bw.Write((ushort)baseBlock);
                }
                else
                {
                    bw.Write((uint)0);
                }
            }

            // Sub-offset array (int32[tc]).
            foreach (int sub in subOffsets)
            {
                bw.Write((int)sub);
            }

            // Frame pixel data (concatenated).
            foreach (var fb in frameBytes)
            {
                bw.Write(fb);
            }

            return ms.ToArray();
        }

        // ── RLE helpers ──────────────────────────────────────────────────────────

        /// <summary>
        /// Convert a flat palette-index array into per-row RLE pair lists.
        /// Runs of the same index are merged up to 255 pixels.
        /// Index 0 (transparent) is encoded the same as any other index — the
        /// decoder treats it as transparent at render time.
        /// </summary>
        private static List<List<(byte idx, byte cnt)>> BuildRleRows(byte[] pixels, int width, int height)
        {
            var rows = new List<List<(byte, byte)>>(height);

            for (int row = 0; row < height; row++)
            {
                var pairs = new List<(byte, byte)>();
                int rowStart = row * width;
                int x = 0;

                while (x < width)
                {
                    byte idx = pixels[rowStart + x];
                    int run = 1;
                    while (x + run < width
                           && pixels[rowStart + x + run] == idx
                           && run < 255)
                    {
                        run++;
                    }
                    pairs.Add((idx, (byte)run));
                    x += run;
                }

                rows.Add(pairs);
            }

            return rows;
        }

        /// <summary>
        /// Flatten a list of RLE row lists into a single byte array.
        /// </summary>
        private static byte[] SerialiseFrame(List<List<(byte idx, byte cnt)>> frameRows)
        {
            var buf = new List<byte>();
            foreach (var row in frameRows)
            {
                foreach (var (idx, cnt) in row)
                {
                    buf.Add(idx);
                    buf.Add(cnt);
                }
            }
            return buf.ToArray();
        }
    }
}