// SprEncoder.cs: Encodes raw palette-indexed pixel data into the WoW SPR format.
// Exact inverse of SprDecoder.cs — round-trip verified byte-for-byte against original
// game sprites (hu_buton.spr, HWAITCUR.SPR, hu-cnt48.spr).
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
        //          Each entry: [carry:1][0x00:1][low16:2]
        //          The carry field is written ONE ROW LATE (lagged): each row's entry
        //          stores the carry that the PREVIOUS row's absolute offset required.
        //          The decoder's wrap-detection handles both early and lagged carry
        //          correctly, but lagging is required for byte-for-byte file identity.
        //   [0x08 + height*4]  2-byte fence sentinel: [last_row_carry, 0x00]
        //                      The original encoder wrote height+1 row entries; the
        //                      last (fence) entry's carry+zero bytes land here.
        //                      The engine uses these 2 bytes to locate pixel data start.
        //   [0x08 + height*4 + 2]  pixel data: rows packed end-to-end.
        //          Each row: [paletteIndex:1][runLength:1] pairs until width pixels consumed.

        private static byte[] EncodeSingleFrame(List<List<(byte idx, byte cnt)>> rows, int width, int height)
        {
            const int tc = 1;
            int rhs = 6 + tc * 4;   // rowHeaderSize = 10

            // Pixel data starts 2 bytes after the row-offset table end.
            // Those 2 bytes are the fence sentinel [last_carry, 0x00].
            int dataBase = 8 + height * 4 + 2;

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

            // Row-offset table with lagged carry.
            // Each row's entry stores the carry from the PREVIOUS row's absolute offset,
            // matching the original encoder's behaviour exactly.
            int prevCarry = 0;
            foreach (int abs in absOffsets)
            {
                int val = abs - rhs;
                int carry = val >> 16;
                int low16 = val & 0xFFFF;
                bw.Write((byte)prevCarry);   // carry lags by one row
                bw.Write((byte)0);
                bw.Write((ushort)low16);
                prevCarry = carry;
            }

            // Fence sentinel: carry+zero of the (height+1)th entry.
            // prevCarry now holds the carry of the last real row.
            bw.Write((byte)prevCarry);
            bw.Write((byte)0);

            // Pixel data.
            bw.Write(pixelData.ToArray());

            return ms.ToArray();
        }

        // ── Multi-frame encoding ─────────────────────────────────────────────────
        //
        // File layout (byte-for-byte verified against original game sprites):
        //
        //   [0x00] uint16  width
        //   [0x02] uint16  height
        //   [0x04] uint16  tableCount  (tc)
        //   [0x06] uint16  rowHeaderSize  = 6 + tc*4  (= rhs)
        //
        //   [0x08..0x08+tc*4-1]  Frame Pointer Table (tc × 4-byte entries)
        //     Entries 0..tc-2: absolute address of each sub-table frame block.
        //       carry byte = running 'high' value BEFORE this entry's wrap check.
        //       low16      = address & 0xFFFF.
        //       Wrap rule (as single-frame): when low16 < prevLow, high++.
        //     Entry tc-1 (sentinel): carry = current high, low16 = height*4.
        //       Detected by decoder via low16 == height*4; abs value not used.
        //
        //   [0x08+tc*4 .. 0x08+tc*4+height*4-1]  Outer Row Table (height × 4-byte entries)
        //     Entries 0..height-2: entry[N] → absolute offset of last-frame row N+1.
        //       Encoded as: low16 = (abs - rhs), carry = running high.
        //     Entry height-1: sentinel (never used as a pixel pointer).
        //       Its 4 bytes [0x00, 0x00, pair0_idx, pair0_cnt] simultaneously serve
        //       as the start of the last-frame row-0 pixel stream:
        //         [0x00, 0x00] = no-op RLE pair (transparent, 0 pixels).
        //         [pair0_idx, pair0_cnt] = first real RLE pair of row 0.
        //
        //   [gapPos = 0x08+tc*4+(height-1)*4 ..]
        //     Last-frame row 0 pixel data starts here (the sentinel entry slot).
        //     Followed immediately by rows 1..height-1 packed sequentially.
        //     entry[N] (N=0..height-2) points to the start of row N+1.
        //
        //   [last_frame_end ..]
        //     Sub-table frames 0..tc-2, each laid out as:
        //       int32[height]  relative offsets from block start to each row.
        //       (rels[0] = height*4 always — pixel data starts after the table.)
        //       Pixel data for all height rows, packed end-to-end.

        private static byte[] EncodeMultiFrame(List<List<List<(byte idx, byte cnt)>>> frameRle, int width, int height)
        {
            int tc = frameRle.Count;
            int rhs = 6 + tc * 4;   // rowHeaderSize

            int outerRowTableStart = 8 + tc * 4;
            int gapPos = outerRowTableStart + (height - 1) * 4;   // sentinel slot = row 0 start

            // ── Last-frame layout ────────────────────────────────────────────────
            // gap (at gapPos) = row 0 of the last frame.
            // The sentinel entry's 4 bytes are [0x00, 0x00, pair0_idx, pair0_cnt]:
            //   carry=0, pad=0 from the table entry structure, then the first RLE pair
            //   of row 0. The decoder reads gapPos and sees a [0,0] no-op then the
            //   first real pair. The remaining pairs of row 0 follow at gapPos+4.
            // Rows 1..height-1 follow sequentially; entry[N] → row N+1 (N=0..h-2).
            var lastRle = frameRle[tc - 1];

            byte sentPair0Idx = lastRle[0][0].idx;
            byte sentPair0Cnt = lastRle[0][0].cnt;
            byte[] gapRow0Tail = SerialiseRow(lastRle[0].Skip(1));   // row 0, pair 1 onward

            // Absolute positions of last-frame rows 1..height-1.
            var lastRowPositions = new int[height - 1];   // index N → row N+1
            int pos = gapPos + 4 + gapRow0Tail.Length;
            for (int r = 1; r < height; r++)
            {
                lastRowPositions[r - 1] = pos;
                pos += SerialiseRow(lastRle[r]).Length;
            }
            int lastFrameEnd = pos;

            // ── Sub-table frame layout ───────────────────────────────────────────
            var subTableAddrs = new int[tc - 1];
            var subTables = new int[tc - 1][];
            var frameBufs = new byte[tc - 1][];

            int cur = lastFrameEnd;
            for (int f = 0; f < tc - 1; f++)
            {
                subTableAddrs[f] = cur;
                int pixStart = cur + height * 4;
                var rels = new int[height];
                int p = pixStart;
                for (int r = 0; r < height; r++)
                {
                    rels[r] = p - cur;
                    p += SerialiseRow(frameRle[f][r]).Length;
                }
                subTables[f] = rels;
                frameBufs[f] = SerialiseRows(frameRle[f]);
                cur = p;
            }

            // ── Frame Pointer Table carry-encoding ───────────────────────────────
            var fpEntries = new (byte carry, ushort low16)[tc];
            {
                int high = 0, prevLow = 0;
                for (int f = 0; f < tc - 1; f++)
                {
                    int absAddr = subTableAddrs[f];
                    byte carry = (byte)high;
                    ushort low16 = (ushort)(absAddr & 0xFFFF);
                    if (f > 0 && (low16 < prevLow || (low16 == 0 && prevLow > 32768))) high++;
                    prevLow = low16;
                    fpEntries[f] = (carry, low16);
                }
                fpEntries[tc - 1] = ((byte)high, (ushort)(height * 4));   // sentinel
            }

            // ── Outer Row Table carry-encoding ───────────────────────────────────
            // entry[N] → row N+1  (N = 0..height-2)
            var outerEntries = new (byte carry, ushort low16)[height - 1];
            {
                int high = 0, prevLow = 0;
                for (int n = 0; n < height - 1; n++)
                {
                    int absOff = lastRowPositions[n];   // position of row n+1
                    byte carry = (byte)high;
                    ushort low16 = (ushort)((absOff - rhs) & 0xFFFF);
                    if (n > 0 && (low16 < prevLow || (low16 == 0 && prevLow > 32768))) high++;
                    prevLow = low16;
                    outerEntries[n] = (carry, low16);
                }
            }

            // ── Assemble ─────────────────────────────────────────────────────────
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms);

            bw.Write((ushort)width);
            bw.Write((ushort)height);
            bw.Write((ushort)tc);
            bw.Write((ushort)rhs);

            // Frame Pointer Table
            foreach (var (carry, low16) in fpEntries)
            { bw.Write(carry); bw.Write((byte)0); bw.Write(low16); }

            // Outer Row Table entries 0..height-2
            foreach (var (carry, low16) in outerEntries)
            { bw.Write(carry); bw.Write((byte)0); bw.Write(low16); }

            // Sentinel entry at gapPos — also the start of last-frame row 0.
            bw.Write((byte)0); bw.Write((byte)0);
            bw.Write(sentPair0Idx); bw.Write(sentPair0Cnt);

            // Rest of last-frame row 0, then rows 1..height-1
            bw.Write(gapRow0Tail);
            for (int r = 1; r < height; r++)
                bw.Write(SerialiseRow(lastRle[r]));

            // Sub-table frames 0..tc-2
            for (int f = 0; f < tc - 1; f++)
            {
                foreach (int rel in subTables[f]) bw.Write(rel);
                bw.Write(frameBufs[f]);
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
        /// Flatten one RLE row into a byte array: [idx, cnt, idx, cnt, ...].
        /// </summary>
        private static byte[] SerialiseRow(IEnumerable<(byte idx, byte cnt)> row)
        {
            var buf = new List<byte>();
            foreach (var (idx, cnt) in row) { buf.Add(idx); buf.Add(cnt); }
            return buf.ToArray();
        }

        /// <summary>
        /// Flatten a list of RLE rows into a single contiguous byte array.
        /// </summary>
        private static byte[] SerialiseRows(IEnumerable<IEnumerable<(byte idx, byte cnt)>> rows)
        {
            var buf = new List<byte>();
            foreach (var row in rows)
                foreach (var (idx, cnt) in row) { buf.Add(idx); buf.Add(cnt); }
            return buf.ToArray();
        }
    }
}