// SprDecoder.cs: Reverse-engineered from WoW.exe via IDA Pro.
// Traced to the Huffman loop at 0x415A80 and sprite blitting routines.
// Logic derived and verified with Claude.ai (Anthropic) for the "WoWRevived" Project.
namespace WoWViewer
{
    public static class SprDecoder
    {
        // SPR format (uncompressed, or post-FFUH decompression):
        //
        //   0x00  uint16  width
        //   0x02  uint16  height
        //   0x04  uint16  tableCount   (1 = single frame, >1 = animated/multi-frame)
        //   0x06  uint16  rowHeaderSize
        //   0x08  height * 4 bytes: row offset table
        //
        // --- Single-frame (tableCount == 1) ---
        // Each 4-byte row offset entry:
        //   bytes[0]    carry byte - written one entry late after a uint16 overflow
        //   bytes[1]    always 0x00
        //   bytes[2..3] uint16 LE - low 16 bits of the row data offset
        //
        // True offset = high * 65536 + uint16(bytes[2..3])
        // Wrap: when low < prevLow, increment high. Carry only advances high, never regresses.
        // Row data starts at offset + rowHeaderSize (the row header is skipped).
        //
        // --- Multi-frame (tableCount > 1) ---
        // Offset table entries are 16-bit sub-offsets relative to a "base block".
        // No carry/wrap logic applies (files fit within 64 KB).
        //
        // "Full block" rows: entry value >= base block offset; the block begins with
        //   tableCount x uint32 sub-offsets (one per frame), each relative to block start.
        //
        // "Body" rows: entry value < base block offset; treated as a frame-0 sub-offset
        //   relative to the base block. To get frame N, add the per-frame delta derived
        //   from the base block header.
        //
        // RLE pixel data: pairs of [paletteIndex, runLength]; index 0 = transparent.

        public static SprInfo ReadInfo(byte[] data)
        {
            ushort width = BitConverter.ToUInt16(data, 0);
            ushort height = BitConverter.ToUInt16(data, 2);
            ushort tableCount = BitConverter.ToUInt16(data, 4);
            ushort rowHeaderSize = BitConverter.ToUInt16(data, 6);
            return new SprInfo { Width = width, Height = height, TableCount = tableCount, RowHeaderSize = rowHeaderSize };
        }

        // Render SPR data using a flat RGB palette (byte[] of R,G,B triplets).
        // paletteOffset : byte offset into palData to start reading colours from
        // greyscale     : if true, renders using palette index as grey value (ignores palData)
        // frame         : which animation frame to render (0 = first/default)
        public static Bitmap Render(byte[] sprData, byte[] palData, int paletteOffset, bool greyscale = false, int frame = 0)
        {
            ushort width = BitConverter.ToUInt16(sprData, 0);
            ushort height = BitConverter.ToUInt16(sprData, 2);
            ushort tableCount = BitConverter.ToUInt16(sprData, 4);
            ushort rowHeaderSize = BitConverter.ToUInt16(sprData, 6);

            var bmp = new Bitmap(width, height, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            frame = Math.Max(0, Math.Min(frame, tableCount - 1));

            if (tableCount == 1)
            {
                // Single-frame: carry/high/low offset encoding with rowHeaderSize skip.
                int high = 0, prevLow = 0;
                for (int row = 0; row < height; row++)
                {
                    int entryBase = 8 + row * 4;
                    int carry = sprData[entryBase];
                    int low = BitConverter.ToUInt16(sprData, entryBase + 2);

                    if (row > 0 && (low < prevLow || (low == 0 && prevLow > 32768))) high++;
                    if (carry > high) high = carry; // carry only advances, never regresses
                    prevLow = low;

                    int dataPos = high * 65536 + low + rowHeaderSize;
                    RenderRow(sprData, palData, bmp, row, width, dataPos, paletteOffset, greyscale);
                }
            }
            else
            {
                // Multi-frame: find the base block (first entry that is a valid block header).
                // A valid block header has tableCount ascending uint32 sub-offsets where
                // frame-0 data at (entry + sub[0]) yields exactly width pixels.
                int baseBlock = -1;
                int[] baseFrameSubs = new int[tableCount];

                for (int r = 0; r < height && baseBlock < 0; r++)
                {
                    int low = BitConverter.ToUInt16(sprData, 8 + r * 4 + 2);
                    if (low + tableCount * 4 >= sprData.Length) continue;

                    int[] subs = new int[tableCount];
                    for (int f = 0; f < tableCount; f++)
                        subs[f] = BitConverter.ToInt32(sprData, low + f * 4);

                    if (subs[0] > 0 && subs[0] < 0x8000 && IsSorted(subs))
                    {
                        int testOff = low + subs[0];
                        if (testOff < sprData.Length && CountPixels(sprData, testOff, width) == width)
                        {
                            baseBlock = low;
                            baseFrameSubs = subs;
                        }
                    }
                }

                if (baseBlock < 0) return bmp;

                int frameDelta = baseFrameSubs[frame] - baseFrameSubs[0];

                for (int row = 0; row < height; row++)
                {
                    int low = BitConverter.ToUInt16(sprData, 8 + row * 4 + 2);
                    int dataPos;

                    if (low >= baseBlock && low + tableCount * 4 < sprData.Length)
                    {
                        // Check whether this entry is itself a full block header.
                        int[] subs = new int[tableCount];
                        for (int f = 0; f < tableCount; f++)
                            subs[f] = BitConverter.ToInt32(sprData, low + f * 4);

                        dataPos = (subs[0] > 0 && subs[0] < 0x8000 && IsSorted(subs))
                            ? low + subs[frame]
                            : baseBlock + low + frameDelta;
                    }
                    else
                    {
                        dataPos = baseBlock + low + frameDelta;
                    }

                    if (dataPos >= 0 && dataPos < sprData.Length)
                        RenderRow(sprData, palData, bmp, row, width, dataPos, paletteOffset, greyscale);
                }
            }

            return bmp;
        }

        // Returns how many 768-byte palettes fit in the PAL data
        public static int PaletteCount(byte[] palData) => palData.Length / 768;

        // Returns the byte offset for palette index n (palettes are 768 bytes each)
        public static int PaletteOffset(int paletteIndex) => 768 + paletteIndex * 768;

        // ── helpers ──────────────────────────────────────────────────────────────

        private static void RenderRow(byte[] sprData, byte[] palData, Bitmap bmp,
            int row, int width, int dataPos, int paletteOffset, bool greyscale)
        {
            int x = 0;
            while (x < width && dataPos + 1 < sprData.Length)
            {
                byte palIndex = sprData[dataPos];
                byte count = sprData[dataPos + 1];
                dataPos += 2;

                Color c;
                if (palIndex == 0)
                {
                    c = Color.Transparent;
                }
                else if (greyscale)
                {
                    c = Color.FromArgb(palIndex, palIndex, palIndex);
                }
                else
                {
                    int palPos = paletteOffset + palIndex * 3;
                    c = (palPos + 2 < palData.Length)
                        ? Color.FromArgb(palData[palPos], palData[palPos + 1], palData[palPos + 2])
                        : Color.Magenta; // out-of-range palette entry
                }

                for (int i = 0; i < count && x < width; i++, x++)
                    bmp.SetPixel(x, row, c);
            }
        }

        private static bool IsSorted(int[] arr)
        {
            for (int i = 1; i < arr.Length; i++)
                if (arr[i] <= arr[i - 1]) return false;
            return true;
        }

        private static int CountPixels(byte[] data, int pos, int width)
        {
            int x = 0;
            while (x < width && pos + 1 < data.Length)
            {
                x += data[pos + 1];
                pos += 2;
            }
            return x;
        }
    }

    public class SprInfo
    {
        public ushort Width { get; set; }
        public ushort Height { get; set; }
        public ushort TableCount { get; set; }
        public ushort RowHeaderSize { get; set; }
        public override string ToString() =>
            $"{Width}x{Height}  tableCount={TableCount}  rowHeaderSize={RowHeaderSize}";
    }
}