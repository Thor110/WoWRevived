using System;
using System.Drawing;
using System.Drawing.Imaging;

namespace WoWViewer
{
    internal class FNTDecoder
    {
        public class FntModel
        {
            public int AtlasWidth;
            public int Height;
            public Glyph[] Glyphs;

            public class Glyph
            {
                public int Width;
                public byte[] Pixels; // Stored row-major, exactly Width * Height
            }
        }

        public static FntModel Parse(byte[] data)
        {
            int atlasWidth = BitConverter.ToUInt16(data, 0);
            int height = BitConverter.ToUInt16(data, 2);
            int count = BitConverter.ToUInt16(data, 4);

            int tableStart = 0x08;
            int dataStart = tableStart + (count * 4);

            var font = new FntModel
            {
                AtlasWidth = atlasWidth,
                Height = height,
                Glyphs = new FntModel.Glyph[count]
            };

            for (int i = 0; i < count; i++)
            {
                int entryOff = tableStart + (i * 4);

                // This is NOT a memory offset. It is the X-Coordinate on the giant image!
                int startX = BitConverter.ToUInt16(data, entryOff);
                int charWidth = BitConverter.ToUInt16(data, entryOff + 2);

                byte[] charPixels = new byte[charWidth * height];

                // "Cut out" the character from the giant 2D image
                for (int y = 0; y < height; y++)
                {
                    for (int x = 0; x < charWidth; x++)
                    {
                        int globalX = startX + x;
                        int globalY = y;

                        // Row-major calculation for the giant atlas
                        int fileOffset = dataStart + (globalY * atlasWidth) + globalX;

                        if (fileOffset < data.Length)
                        {
                            charPixels[y * charWidth + x] = data[fileOffset];
                        }
                    }
                }

                font.Glyphs[i] = new FntModel.Glyph { Width = charWidth, Pixels = charPixels };
            }
            return font;
        }

        public static Bitmap RenderGlyph(FntModel.Glyph glyph, int fontHeight, byte[] palData)
        {
            if (glyph.Width <= 0 || fontHeight <= 0) return new Bitmap(1, 1);

            var bmp = new Bitmap(glyph.Width, fontHeight, PixelFormat.Format32bppArgb);
            var bmpData = bmp.LockBits(new Rectangle(0, 0, glyph.Width, fontHeight), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            int[] argb = new int[glyph.Width * fontHeight];

            // Because Parse() cut the pixels out perfectly row-major, drawing is now a simple 1:1 loop
            for (int i = 0; i < glyph.Pixels.Length; i++)
            {
                byte idx = glyph.Pixels[i];
                if (idx != 0)
                {
                    int p = idx * 3;
                    // 6-bit to 8-bit VGA palette conversion
                    int r = palData[p] * 4;
                    int g = palData[p + 1] * 4;
                    int b = palData[p + 2] * 4;
                    argb[i] = (255 << 24) | (r << 16) | (g << 8) | b;
                }
            }

            System.Runtime.InteropServices.Marshal.Copy(argb, 0, bmpData.Scan0, argb.Length);
            bmp.UnlockBits(bmpData);
            return bmp;
        }

        // Your existing RenderFontAtlas method stays exactly the same
        public static Bitmap RenderFontAtlas(FntModel model, byte[] palData)
        {
            int totalWidth = 0;
            foreach (var g in model.Glyphs) totalWidth += g.Width + 2;

            int atlasWidth = Math.Min(totalWidth, 1024);
            int rows = (totalWidth / atlasWidth) + 1;
            int atlasHeight = rows * (model.Height + 4);

            Bitmap bmp = new Bitmap(atlasWidth, atlasHeight, PixelFormat.Format32bppArgb);
            using (Graphics g = Graphics.FromImage(bmp))
            {
                g.Clear(Color.Transparent);
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

                    using (Bitmap charBmp = RenderGlyph(glyph, model.Height, palData))
                    {
                        g.DrawImage(charBmp, curX, curY);
                    }
                    curX += glyph.Width + 2;
                }
            }
            return bmp;
        }
    }
}