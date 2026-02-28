// SprDecoder.cs: Reverse-engineered from WoW.exe via IDA Pro.
// Traced to the Huffman loop at 0x415A80 and sprite blitting routines.
// OBJ.ojd palette mapping and PAL file structure reverse-engineered with Claude.ai (Anthropic)
// for the "WoWRevived" Project.

namespace WoWViewer
{
    public static class SprDecoder
    {
        // ── SPR format (uncompressed / post-FFUH decompression) ──────────────────
        //
        //   0x00  uint16  width
        //   0x02  uint16  height
        //   0x04  uint16  tableCount     (1 = single frame, >1 = animated)
        //   0x06  uint16  rowHeaderSize  = 6 + tableCount*4
        //                                 tc=1→10  tc=2→14  tc=4→22
        //   0x08  height × 4 bytes: row offset table
        //
        // --- Single-frame (tableCount == 1) ---
        // Each 4-byte entry:
        //   byte[0]    carry  (only ever advances, never regresses)
        //   byte[1]    0x00
        //   byte[2..3] uint16 LE  low 16 bits of absolute row-data offset
        // True offset = high*65536 + low
        // Wrap: when low < prevLow (or low==0 && prevLow>32768), high++
        // Pixel data starts at: absolute_offset + rowHeaderSize
        //
        // --- Multi-frame (tableCount > 1) ---
        // Row table entries = 16-bit sub-offsets relative to a "base block".
        // No carry/wrap applies (files fit within 64 KB).
        // Full-block rows (value >= baseBlock): begin with tableCount×int32 per-frame offsets.
        // Body rows (value < baseBlock): frame-0 offset relative to base block;
        //   frame N = baseBlock + bodyOffset + (baseFrameSubs[N] - baseFrameSubs[0])
        //
        // RLE pixel stream: [paletteIndex, runLength] pairs. Index 0 = transparent.
        //
        // ── PAL file structure ───────────────────────────────────────────────────
        //
        //   Bytes     0–767  : 256-colour VGA palette.
        //                      3 bytes per entry (R, G, B), range 0–63 (6-bit VGA).
        //                      *** Multiply each channel by 4 to get true 8-bit RGB. ***
        //                      Colour index 0 is treated as transparent; never rendered.
        //   Bytes 768–66303  : 256×256 = 65 536 byte colour-blend / translucency table.
        //                      Used at runtime for transparent effects (smoke, glass…).
        //                      NOT needed for static sprite viewing.
        //   Total = 66 304 bytes (all 16 PAL files are exactly this size).
        //
        //   BUGS FIXED in this revision:
        //   (a) PaletteOffset previously returned 768 + n*768, skipping past the actual
        //       palette entirely and reading from the blend table → wrong colours.
        //   (b) RGB values were never scaled ×4, making everything ~1/4 correct brightness.
        //
        // ── SPR → PAL file mapping (from OBJ.ojd palSlot field) ─────────────────
        //
        // palSlot order (0-indexed, by order of appearance in OBJ.ojd):
        //   0=HW  1=MW  2=HB  3=MB  4=HR  5=MR  6=BM
        //   7=F1  8=F2  9=F3  10=F4 11=F5 12=F6 13=F7  14=SE  15=CD
        // Note: F1.PAL and SE.PAL have identical main palettes (differ in blend table only).
        // Call GetPaletteForSpr() to look up the PAL file for any sprite.

        public static SprInfo ReadInfo(byte[] data)
        {
            return new SprInfo
            {
                Width         = BitConverter.ToUInt16(data, 0),
                Height        = BitConverter.ToUInt16(data, 2),
                TableCount    = BitConverter.ToUInt16(data, 4),
                RowHeaderSize = BitConverter.ToUInt16(data, 6)
            };
        }

        // Render a sprite frame.
        // palData       : raw bytes of the .PAL file.
        // paletteOffset : byte offset into palData for the start of the colour table.
        //                 For normal (unshaded) rendering always pass 0.
        // greyscale     : if true, renders palette index as grey value (ignores palData).
        // frame         : animation frame, clamped to [0, tableCount-1].
        public static Bitmap Render(byte[] sprData, byte[] palData, int paletteOffset = 0, int frame = 0)
        {
            ushort width         = BitConverter.ToUInt16(sprData, 0);
            ushort height        = BitConverter.ToUInt16(sprData, 2);
            ushort tableCount    = BitConverter.ToUInt16(sprData, 4);
            ushort rowHeaderSize = BitConverter.ToUInt16(sprData, 6);

            var bmp = new Bitmap(width, height, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            frame = Math.Max(0, Math.Min(frame, tableCount - 1));

            if (tableCount == 1)
            {
                int high = 0, prevLow = 0;
                for (int row = 0; row < height; row++)
                {
                    int entryBase = 8 + row * 4;
                    int carry     = sprData[entryBase];
                    int low       = BitConverter.ToUInt16(sprData, entryBase + 2);

                    if (row > 0 && (low < prevLow || (low == 0 && prevLow > 32768))) high++;
                    if (carry > high) high = carry;
                    prevLow = low;

                    RenderRow(sprData, palData, bmp, row, width, high * 65536 + low + rowHeaderSize, paletteOffset);
                }
            }
            else
            {
                int   baseBlock     = -1;
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
                            baseBlock = low; baseFrameSubs = subs;
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
                        int[] subs = new int[tableCount];
                        for (int f = 0; f < tableCount; f++)
                            subs[f] = BitConverter.ToInt32(sprData, low + f * 4);

                        dataPos = (subs[0] > 0 && subs[0] < 0x8000 && IsSorted(subs))
                            ? low + subs[frame]             // full-block row
                            : baseBlock + low + frameDelta; // body-offset row
                    }
                    else
                    {
                        dataPos = baseBlock + low + frameDelta;
                    }

                    if (dataPos >= 0 && dataPos < sprData.Length)
                        RenderRow(sprData, palData, bmp, row, width, dataPos, paletteOffset);
                }
            }

            return bmp;
        }

        // ── Palette helpers ──────────────────────────────────────────────────────

        // For static rendering always use paletteOffset = 0.
        // The main 256-colour palette occupies bytes 0–767 of every PAL file.
        // The blend/shade table (bytes 768–66303) is only needed for runtime transparency.
        // This method is kept for future use if shade-level rendering is implemented.
        public static int ShadeTableOffset(int shadeLevel) => 768 + Math.Clamp(shadeLevel, 0, 255) * 256;

        // ── Internal helpers ─────────────────────────────────────────────────────

        private static void RenderRow(byte[] sprData, byte[] palData, Bitmap bmp, int row, int width, int dataPos, int paletteOffset)
        {
            int x = 0;
            while (x < width && dataPos + 1 < sprData.Length)
            {
                byte palIndex = sprData[dataPos];
                byte count    = sprData[dataPos + 1];
                dataPos += 2;

                Color c;
                if (palIndex == 0)
                {
                    c = Color.Transparent;
                }
                else
                {
                    int palPos = paletteOffset + palIndex * 3;
                    if (palPos + 2 < palData.Length)
                    {
                        // VGA 6-bit palette values: multiply by 4 for 8-bit RGB.
                        int r = Math.Min(255, palData[palPos]     * 4);
                        int g = Math.Min(255, palData[palPos + 1] * 4);
                        int b = Math.Min(255, palData[palPos + 2] * 4);
                        c = Color.FromArgb(r, g, b);
                    }
                    else
                    {
                        c = Color.Magenta; // palette out-of-range marker
                    }
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
                x   += data[pos + 1];
                pos += 2;
            }
            return x;
        }
    }

    public class SprInfo
    {
        public ushort Width         { get; set; }
        public ushort Height        { get; set; }
        public ushort TableCount    { get; set; }
        public ushort RowHeaderSize { get; set; }
        public override string ToString() => $"Size={Width}x{Height}  tableCount={TableCount}  rowHeaderSize={RowHeaderSize}";
    }
}