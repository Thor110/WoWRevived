namespace WoWViewer
{
    public static class SprDecoder
    {
        // SPR format (uncompressed, or post-FFUH decompression):
        //   0x00  uint16  width
        //   0x02  uint16  height
        //   0x04  uint16  tableCount
        //   0x06  uint16  rowHeaderSize (always 10)
        //   0x08  height * 4 bytes: row offset table
        //         each entry is 4 bytes, the file offset is a uint16 LE stored in bytes [2..3]
        //   per row at its offset:
        //         10 byte row header (skipped)
        //         RLE pairs: [paletteIndex, count] until row width is filled

        public static SprInfo ReadInfo(byte[] data)
        {
            ushort width = BitConverter.ToUInt16(data, 0);
            ushort height = BitConverter.ToUInt16(data, 2);
            ushort tableCount = BitConverter.ToUInt16(data, 4);
            ushort rowHeaderSize = BitConverter.ToUInt16(data, 6);
            return new SprInfo { Width = width, Height = height, TableCount = tableCount, RowHeaderSize = rowHeaderSize };
        }

        // Render SPR data using a flat RGB palette (byte[] of R,G,B triplets, 256 entries = 768 bytes)
        // paletteOffset: byte offset into palData to start reading the 768-byte palette from
        // transparentIndex: palette index to treat as transparent (default 0)
        public static Bitmap Render(byte[] sprData, byte[] palData, int paletteOffset = 0, int transparentIndex = 0)
        {
            ushort width = BitConverter.ToUInt16(sprData, 0);
            ushort height = BitConverter.ToUInt16(sprData, 2);
            ushort rowHeaderSize = BitConverter.ToUInt16(sprData, 6); // always 10

            var bmp = new Bitmap(width, height, System.Drawing.Imaging.PixelFormat.Format32bppArgb);

            for (int row = 0; row < height; row++)
            {
                // Row offset entry: 4 bytes, uint16 LE at bytes [2..3]
                int entryBase = 8 + row * 4;
                int rowOffset = BitConverter.ToUInt16(sprData, entryBase + 2);

                // Skip the row header
                int dataPos = rowOffset + rowHeaderSize;

                // Decode RLE pairs across the row
                int x = 0;
                while (x < width && dataPos + 1 < sprData.Length)
                {
                    byte palIndex = sprData[dataPos];
                    byte count = sprData[dataPos + 1];
                    dataPos += 2;

                    for (int i = 0; i < count && x < width; i++, x++)
                    {
                        Color c;
                        if (palIndex == transparentIndex)
                        {
                            c = Color.Transparent;
                        }
                        else
                        {
                            int palPos = paletteOffset + palIndex * 3;
                            if (palPos + 2 < palData.Length)
                            {
                                c = Color.FromArgb(palData[palPos], palData[palPos + 1], palData[palPos + 2]);
                            }
                            else
                            {
                                c = Color.Magenta; // out of range palette entry - visible error indicator
                            }
                        }
                        bmp.SetPixel(x, row, c);
                    }
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