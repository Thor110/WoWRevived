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
        //   0x04  uint16  tableCount
        //   0x06  uint16  rowHeaderSize (always 10)
        //   0x08  height * 4 bytes: row offset table
        //
        // Each 4-byte row offset entry:
        //   bytes[0]    high byte (carry) - written to the entry AFTER a uint16 overflow occurs
        //   bytes[1]    always 0x00
        //   bytes[2..3] uint16 LE - low 16 bits of the row offset
        //
        // True offset = high * 65536 + uint16(bytes[2..3])
        // Wrap detection: when low < prev_low, increment high for the current row.
        // The carry byte in bytes[0] is written one entry late so we also apply it when nonzero.
        //
        // At each row offset:
        //   10-byte row header (skipped)
        //   RLE pixel data: pairs of [paletteIndex, runLength]

        public static SprInfo ReadInfo(byte[] data)
        {
            ushort width = BitConverter.ToUInt16(data, 0);
            ushort height = BitConverter.ToUInt16(data, 2);
            ushort tableCount = BitConverter.ToUInt16(data, 4);
            ushort rowHeaderSize = BitConverter.ToUInt16(data, 6);
            return new SprInfo { Width = width, Height = height, TableCount = tableCount, RowHeaderSize = rowHeaderSize };
        }

        // Render SPR data using a flat RGB palette (byte[] of R,G,B triplets)
        // paletteOffset: byte offset into palData to start reading from
        // transparentIndex: palette index to treat as transparent (default 0)
        public static Bitmap Render(byte[] sprData, byte[] palData, int paletteOffset = 0, int transparentIndex = 0)
        {
            ushort width = BitConverter.ToUInt16(sprData, 0);
            ushort height = BitConverter.ToUInt16(sprData, 2);

            var bmp = new Bitmap(width, height, System.Drawing.Imaging.PixelFormat.Format32bppArgb);

            int high = 0;
            int prevLow = 0;

            for (int row = 0; row < height; row++)
            {
                int entryBase = 8 + row * 4;
                int carry = sprData[entryBase];                             // high byte (one entry late)
                int low = BitConverter.ToUInt16(sprData, entryBase + 2); // low 16 bits

                if (low < prevLow && row > 0) high++;   // wrap detected: increment for this row
                if (carry > 0) high = carry; // carry byte from previous overflow
                prevLow = low;

                int rowOffset = high * 65536 + low;
                int dataPos = rowOffset + 10; // skip 10-byte row header

                int x = 0;
                while (x < width && dataPos + 1 < sprData.Length)
                {
                    byte palIndex = sprData[dataPos];
                    byte count = sprData[dataPos + 1];
                    dataPos += 2;

                    Color c;
                    if (palIndex == transparentIndex)
                    {
                        c = Color.Transparent;
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

            return bmp;
        }

        // Returns how many 768-byte palettes fit in the PAL data
        public static int PaletteCount(byte[] palData) => palData.Length / 768;

        // Returns the byte offset for palette index n
        public static int PaletteOffset(int paletteIndex) => paletteIndex * 768;
    }

    public class SprInfo
    {
        public ushort Width { get; set; }
        public ushort Height { get; set; }
        public ushort TableCount { get; set; }
        public ushort RowHeaderSize { get; set; }
        public override string ToString() => $"{Width}x{Height}  tableCount={TableCount}  rowHeaderSize={RowHeaderSize}";
    }
}