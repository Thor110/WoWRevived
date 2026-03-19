using System;
using System.Drawing.Imaging;
using static WoWViewer.FNTDecoder;

namespace WoWViewer
{
    internal class FNTEncoder
    {
        public static byte[] Encode(FNTDecoder.FntModel font, byte[] originalData)
        {
            // 1. Clone the entire original array. This is much safer than building from scratch
            // because it perfectly preserves all headers, table data, and unused atlas pixels.
            byte[] data = (byte[])originalData.Clone();

            int count = font.Glyphs.Length;
            int tableStart = 0x08;
            int dataStart = tableStart + (count * 4) + 2;

            // 2. Read the StartX positions directly from the cloned table
            int[] allX = new int[count];
            for (int i = 0; i < count; i++)
            {
                allX[i] = BitConverter.ToUInt16(data, tableStart + i * 4);
            }

            // 3. Stamp the new pixels back into the giant Atlas
            for (int i = 0; i < count; i++)
            {
                var glyph = font.Glyphs[i];
                int startX = allX[i];

                for (int y = 0; y < font.Height; y++)
                {
                    for (int x = 0; x < glyph.Width; x++)
                    {
                        int fileOffset = dataStart + (y * font.AtlasWidth) + (startX + x);

                        if (fileOffset >= 0 && fileOffset < data.Length)
                        {
                            data[fileOffset] = glyph.Pixels[y * glyph.Width + x];
                        }
                    }
                }
            }

            return data;
        }
    }
}