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
        public static Bitmap Export8bppAtlas(FntModel model, byte[] palData)
        {
            int totalWidth = 0;
            foreach (var g in model.Glyphs) totalWidth += g.Width + 2;
            int atlasWidth = Math.Min(totalWidth, 1024);
            int rows = (totalWidth / atlasWidth) + 1;
            int atlasHeight = rows * (model.Height + 4);

            // Create an explicitly 8-bit indexed bitmap
            Bitmap bmp = new Bitmap(atlasWidth, atlasHeight, PixelFormat.Format8bppIndexed);

            // 1. Inject our palette into the PNG header
            ColorPalette bmpPal = bmp.Palette;
            for (int i = 0; i < 256; i++)
            {
                int r = palData[i * 3] * 4;
                int g = palData[i * 3 + 1] * 4;
                int b = palData[i * 3 + 2] * 4;
                // Index 0 is transparent
                bmpPal.Entries[i] = i == 0 ? Color.Transparent : Color.FromArgb(255, r, g, b);
            }
            bmp.Palette = bmpPal;

            // 2. Lock the bits and write the raw 0-255 indices directly!
            var bmpData = bmp.LockBits(new Rectangle(0, 0, atlasWidth, atlasHeight), ImageLockMode.WriteOnly, PixelFormat.Format8bppIndexed);
            byte[] rawImageBytes = new byte[atlasWidth * atlasHeight];

            int curX = 2;
            int curY = 2;

            foreach (var glyph in model.Glyphs)
            {
                if (glyph.Width <= 0) continue;
                if (curX + glyph.Width > atlasWidth)
                {
                    curX = 2;
                    curY += model.Height + 4;
                }

                // Copy the glyph's raw byte indices into the giant 8-bit image array
                for (int y = 0; y < model.Height; y++)
                {
                    for (int x = 0; x < glyph.Width; x++)
                    {
                        rawImageBytes[(curY + y) * atlasWidth + (curX + x)] = glyph.Pixels[y * glyph.Width + x];
                    }
                }
                curX += glyph.Width + 2;
            }

            // Copy our constructed byte array into the Bitmap's memory
            System.Runtime.InteropServices.Marshal.Copy(rawImageBytes, 0, bmpData.Scan0, rawImageBytes.Length);
            bmp.UnlockBits(bmpData);

            return bmp;
        }
    }
}