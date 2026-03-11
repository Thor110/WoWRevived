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
        // shadeData     : optional shade table from .SHH file (16-bit colour mode).
        //                 Level 0 = 512 bytes: 256 × uint16 RGB565 values.
        //                 Each entry is the final 16-bit screen colour for that palette index.
        //                 Pass null for identity palette rendering (F1/F2 sprites etc).
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
                // Read tc frame block pointers (starting at byte 8), with carry/wrap tracking
                int[] frameBlockPtrs = new int[tableCount];
                int high = 0, prevLow = 0;
                for (int i = 0; i < tableCount; i++)
                {
                    int baseX = 8 + i * 4;
                    int carry = sprData[baseX];
                    int low16 = BitConverter.ToUInt16(sprData, baseX + 2);

                    if (i > 0 && (low16 < prevLow || (low16 == 0 && prevLow > 32768))) high++;
                    if (carry > high) high = carry;
                    prevLow = low16;

                    frameBlockPtrs[i] = high * 65536 + low16;
                }

                int outerRowTableStart = 8 + tableCount * 4;

                for (int row = 0; row < height; row++)
                {
                    int pixelAbs;
                    if (frame < tableCount - 1)
                    {
                        // Sub-table frame: relative offset from block ptr
                        int blockPtr = frameBlockPtrs[frame];
                        int relVal = BitConverter.ToInt32(sprData, blockPtr + row * 4);
                        pixelAbs = blockPtr + relVal;
                    }
                    else
                    {
                        // Last frame uses the outer row table — but it is SHIFTED BY ONE.
                        // Row 0 is stored immediately after the outer table (no table entry).
                        // Rows 1..height-1 are in outer table entries [0..height-2].
                        if (row == 0)
                        {
                            pixelAbs = outerRowTableStart + height * 4; // data right after the table
                        }
                        else
                        {
                            int low16 = BitConverter.ToUInt16(sprData, outerRowTableStart + (row - 1) * 4 + 2);
                            pixelAbs = low16 + rowHeaderSize;
                        }
                    }
                    RenderRow(sprData, palData, bmp, row, width, pixelAbs, shadeData);
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
                    if (shadeData != null && shadeData.Length >= palIndex * 2 + 2)
                    {
                        // 16-bit render path: shadeData is level 0 of the .SHH file.
                        // Each entry is a uint16 RGB565 value for that palette index.
                        int rgb565 = shadeData[palIndex * 2] | (shadeData[palIndex * 2 + 1] << 8);
                        int r = ((rgb565 >> 11) & 0x1F); r = (r << 3) | (r >> 2);
                        int g = ((rgb565 >> 5) & 0x3F); g = (g << 2) | (g >> 4);
                        int b = (rgb565 & 0x1F); b = (b << 3) | (b >> 2);
                        c = Color.FromArgb(r, g, b);
                    }
                    else
                    {
                        // No shade table: direct palette lookup (6-bit VGA × 4).
                        int palPos = palIndex * 3;
                        if (palPos + 2 < palData.Length)
                            c = Color.FromArgb(palData[palPos] * 4, palData[palPos + 1] * 4, palData[palPos + 2] * 4);
                        else
                            c = Color.Magenta;
                    }
                }

                for (int i = 0; i < count && x < width; i++, x++) { bmp.SetPixel(x, row, c); }
            }
        }
    }
}