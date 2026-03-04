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
        // The first "full-block" row in the row table (identified by its low16 pointing to a
        // sorted array of tableCount int32 sub-offsets) anchors the layout:
        //   baseBlock   = that row's low16 value
        //   frameSubs[] = the tableCount int32s read at baseBlock
        //
        // Each frame's pixel data is a CONTIGUOUS block of rows packed sequentially.
        // Frame N starts at: baseBlock + frameSubs[N]
        // Rows are decoded by walking forward through the block, consuming RLE pairs until
        // width pixels are filled, then advancing the pointer for the next row.
        // The per-row low16 values in the outer row table are only used for frame 0 indexing
        // and are not consulted when rendering frames 1+.
        //
        // RLE pixel stream: [paletteIndex, runLength] pairs. Index 0 = transparent.
        // The last run of a row may extend past width; excess pixels are simply clipped.
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
                Width = BitConverter.ToUInt16(data, 0),
                Height = BitConverter.ToUInt16(data, 2),
                TableCount = BitConverter.ToUInt16(data, 4),
                RowHeaderSize = BitConverter.ToUInt16(data, 6)
            };
        }

        // Render a sprite frame.
        // palData       : raw bytes of the .PAL file.
        // shadeData     : optional 256-byte remap table (level 0 from .SHL file).
        //                 Each byte remaps a palette index before colour lookup.
        //                 Pass null for identity (F1/F2 sprites need no remapping).
        // frame         : animation frame, clamped to [0, tableCount-1].
        public static Bitmap Render(byte[] sprData, byte[] palData, byte[]? shadeData = null, int frame = 0)
        {
            ushort width = BitConverter.ToUInt16(sprData, 0);
            ushort height = BitConverter.ToUInt16(sprData, 2);
            ushort tableCount = BitConverter.ToUInt16(sprData, 4);
            ushort rowHeaderSize = BitConverter.ToUInt16(sprData, 6);

            var bmp = new Bitmap(width, height, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            frame = Math.Max(0, Math.Min(frame, tableCount - 1));

            if (tableCount == 1)
            {
                int high = 0, prevLow = 0;
                for (int row = 0; row < height; row++)
                {
                    int entryBase = 8 + row * 4;
                    int carry = sprData[entryBase];
                    int low = BitConverter.ToUInt16(sprData, entryBase + 2);

                    if (row > 0 && (low < prevLow || (low == 0 && prevLow > 32768))) { high++; }
                    if (carry > high) { high = carry; }
                    prevLow = low;

                    RenderRow(sprData, palData, bmp, row, width, high * 65536 + low + rowHeaderSize, shadeData);
                }
            }
            else
            {
                int baseBlock = -1;
                int[] frameStarts = Array.Empty<int>();

                for (int r = 0; r < height && baseBlock < 0; r++)
                {
                    int low = BitConverter.ToUInt16(sprData, 8 + r * 4 + 2); // +2 makes no difference here?
                    if (low + tableCount * 4 >= sprData.Length) { continue; }

                    int[] subs = new int[tableCount];
                    for (int f = 0; f < tableCount; f++) { subs[f] = BitConverter.ToInt32(sprData, low + f * 4); }

                    if (subs[0] > 0 && subs[0] < 0x8000 && IsSorted(subs))
                    {
                        int testOff = low + subs[0];
                        if (testOff < sprData.Length && CountPixels(sprData, testOff, width) >= width)
                        {
                            baseBlock = low;
                            frameStarts = new int[tableCount];
                            for (int f = 0; f < tableCount; f++) { frameStarts[f] = low + subs[f]; }
                        }
                    }
                }

                if (baseBlock < 0) { return bmp; } // makes no difference

                // Each frame is a contiguous block of rows; walk forward consuming RLE pairs.

                int dataPos = frameStarts[frame];
                if (tableCount > 1) { dataPos = frameStarts[frame]; } // makes no difference

                for (int row = 0; row < height; row++)
                {
                    RenderRow(sprData, palData, bmp, row, width, dataPos, shadeData);
                    // Advance past this row's RLE data
                    int x = 0;
                    while (x < width && dataPos + 1 < sprData.Length)
                    {
                        x += sprData[dataPos + 1];
                        dataPos += 2;
                    }
                }
            }

            return bmp;
        }

        // ── Palette helpers ──────────────────────────────────────────────────────

        // The shade table (bytes 768-66303 of a PAL file) is separate from the .SHL remap tables
        // and is only used for runtime transparency blending in the engine, not for sprite rendering.

        // ── Internal helpers ─────────────────────────────────────────────────────

        private static void RenderRow(byte[] sprData, byte[] palData, Bitmap bmp, int row, int width, int dataPos, byte[]? shadeData)
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
                else
                {
                    // Apply shade remap if present: palIndex -> remapped index,
                    // then look up colour in the main 256-colour VGA palette (6-bit, x4).
                    int palPos = ((shadeData != null) ? shadeData[palIndex] : palIndex) * 3;
                    if (palPos + 2 < palData.Length)
                    {
                        // VGA 6-bit palette values: multiply by 4 for 8-bit RGB.
                        // Pack to 565
                        /* // exact colour match to screenshot for cd_sep1........ not for anything else
                        int r = Math.Min(255, (palData[palPos] * 255 / 63) + 7);   // Boost Red
                        int g = Math.Min(255, (palData[palPos + 1] * 255 / 63) + 1); // Slight Green
                        int b = Math.Max(0, (palData[palPos + 2] * 255 / 63) - 6);  // Drop Blue
                        c = Color.FromArgb(r, g, b);
                        */
                        c = Color.FromArgb(palData[palPos] * 4, palData[palPos + 1] * 4, palData[palPos + 2] * 4);
                    }
                    else
                    {
                        c = Color.Magenta; // palette out-of-range marker
                    }
                }

                for (int i = 0; i < count && x < width; i++, x++) { bmp.SetPixel(x, row, c); }
            }
        }

        private static bool IsSorted(int[] arr)
        {
            for (int i = 1; i < arr.Length; i++) { if (arr[i] <= arr[i - 1]) { return false; } }
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
}