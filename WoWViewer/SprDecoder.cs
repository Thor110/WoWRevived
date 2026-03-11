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
        //
        // ── Single-frame (tableCount == 1) ───────────────────────────────────────
        //
        //   Bytes 8..8+height*4-1 : row offset table (height × 4 bytes)
        //   Each 4-byte entry:
        //     byte[0]    carry  (only ever advances, never regresses)
        //     byte[1]    0x00
        //     byte[2..3] uint16 LE  low 16 bits of pixel-data offset
        //   True offset  = high*65536 + low
        //   Wrap rule    : when low < prevLow (or low==0 && prevLow>32768), high++
        //   Pixel data starts at: true_offset + rowHeaderSize
        //
        // ── Multi-frame (tableCount > 1) ─────────────────────────────────────────
        //
        //   Bytes 8..8+tc*4-1         : Frame Pointer Table (tc × 4-byte entries)
        //     Each entry uses the same carry/low16/wrap encoding as single-frame rows.
        //     Decoded with the same running high/prevLow wrap rule across all tc entries.
        //     Signal : the LAST entry always has low16 == height*4.
        //              That entry identifies the "outer row table" frame (see below).
        //
        //   Bytes 8+tc*4..8+tc*4+height*4-1 : Outer Row Table (height × 4-byte entries)
        //     Same carry/low16/wrap encoding.
        //     Contains row offsets for the LAST frame (frame index tableCount-1).
        //     Layout:
        //       Entries [0..height-2]  → rows 0..height-2 (absolute offsets: low16 + rhs)
        //       Row height-1 (last)    → pixel data stored in the "gap" at
        //                                byte (8 + tc*4 + height*4), immediately after
        //                                this table, with NO entry in the table.
        //
        //   Bytes 8+tc*4+height*4..  : pixel data for last frame (row height-1 first,
        //                              then the remainder pointed to by the outer table)
        //
        //   Frames 0..tableCount-2 (sub-table frames):
        //     frameBlockPtrs[F] = decoded frame pointer for frame F.
        //     A sub-table of height int32 values sits at that file offset.
        //     Each int32 is a relative offset from frameBlockPtrs[F].
        //     Pixel data for frame F, row R  =  frameBlockPtrs[F] + subTable[R]
        //     (no +rowHeaderSize; the int32 values are self-contained.)
        //
        // ── Row data corruption / missing rows ───────────────────────────────────
        //
        //   A small number of sprites (e.g. R_FOLD.SPR) appear to contain corrupted
        //   sub-table entries in certain rows, producing out-of-range pixel offsets.
        //   RenderRow() silently skips any row whose resolved pixelAbs is out of
        //   bounds, leaving that row transparent rather than throwing.
        //
        // ── White / fully-transparent frames ─────────────────────────────────────
        //
        //   Some sprites (hu-cnt24, MA-CNT24 frames 11, 23, 25, 94, 133-146 etc.)
        //   decode to all palette-index-0 (transparent).  This is intentional: those
        //   animation slots were cut before ship and the pixel streams are genuinely
        //   empty.  They are not a decoder bug.
        //
        // ── RLE pixel stream ─────────────────────────────────────────────────────
        //
        //   [paletteIndex, runLength] pairs. Index 0 = transparent (never rendered).
        //   The last run of a row may exceed width; excess pixels are clipped.
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
        // palData    : raw bytes of the .PAL file.
        // shadeData  : optional shade table from .SHH file (16-bit colour mode).
        //              Level 0 = 512 bytes: 256 × uint16 RGB565 values.
        //              Each entry is the final 16-bit screen colour for that palette index.
        //              Pass null for identity palette rendering (F1/F2 sprites etc).
        // frame      : animation frame index, clamped to [0, tableCount-1].
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
                // ── Single-frame path ────────────────────────────────────────────
                int high = 0, prevLow = 0;
                for (int row = 0; row < height; row++)
                {
                    int entryBase = 8 + row * 4;
                    int carry = sprData[entryBase];
                    int low = BitConverter.ToUInt16(sprData, entryBase + 2);

                    if (row > 0 && (low < prevLow || (low == 0 && prevLow > 32768))) high++;
                    if (carry > high) high = carry;
                    prevLow = low;

                    RenderRow(sprData, palData, bmp, row, width, high * 65536 + low + rowHeaderSize, shadeData);
                }
            }
            else
            {
                // ── Multi-frame path ─────────────────────────────────────────────

                // 1. Decode the Frame Pointer Table (bytes 8..8+tc*4-1).
                //    Use the same running carry/wrap rule as the single-frame row table.
                int[] frameBlockPtrs = new int[tableCount];
                {
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
                }

                // Outer row table begins immediately after the Frame Pointer Table.
                int outerRowTableStart = 8 + tableCount * 4;

                // Row 0 of the last frame has no outer table entry.
                // Its pixel data sits in the "gap" immediately after the outer table.
                // Entry[N] holds row N+1. Entry[height-1] is a sentinel and is never read.
                int lastFrameFirstRowPos = outerRowTableStart + height * 4;

                for (int row = 0; row < height; row++)
                {
                    int pixelAbs = -1;

                    if (frame < tableCount - 1)
                    {
                        // Sub-table frame (frames 0..tc-2).
                        // Sub-table at frameBlockPtrs[frame] holds height int32 relative offsets.
                        // Pixel data = blockPtr + subTable[row].
                        int blockPtr = frameBlockPtrs[frame];
                        int relVal = BitConverter.ToInt32(sprData, blockPtr + row * 4);
                        pixelAbs = blockPtr + relVal;
                    }
                    else
                    {
                        // Last frame (frame tc-1) uses the outer row table.
                        // Row 0 comes from the gap (lastFrameFirstRowPos).
                        // Rows 1..height-1 come from entries [0..height-2].
                        if (row == 0)
                        {
                            pixelAbs = lastFrameFirstRowPos;
                        }
                        else
                        {
                            int low16 = BitConverter.ToUInt16(sprData, outerRowTableStart + (row - 1) * 4 + 2);
                            pixelAbs = low16 + rowHeaderSize;
                        }
                    }

                    // Guard against corrupt or out-of-range offsets (e.g. R_FOLD.SPR).
                    if (pixelAbs < 0 || pixelAbs + 1 >= sprData.Length)
                        continue;

                    RenderRow(sprData, palData, bmp, row, width, pixelAbs, shadeData);
                }
            }

            return bmp;
        }

        // ── Internal helpers ─────────────────────────────────────────────────────

        // The shade table (bytes 768-66303 of a PAL file) is separate from the .SHL remap
        // tables and is only used for runtime transparency blending in the engine.

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
                else if (shadeData != null && shadeData.Length >= palIndex * 2 + 2)
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
                    c = (palPos + 2 < palData.Length)
                        ? Color.FromArgb(palData[palPos] * 4, palData[palPos + 1] * 4, palData[palPos + 2] * 4)
                        : Color.Magenta;
                }

                for (int i = 0; i < count && x < width; i++, x++)
                    bmp.SetPixel(x, row, c);
            }
        }
    }
}