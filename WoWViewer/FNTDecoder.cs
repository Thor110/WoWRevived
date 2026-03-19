using System.Drawing;
using System.Drawing.Imaging;

namespace WoWViewer
{
    internal class FNTDecoder
    {
        public class FntModel
        {
            public int Height;
            public Glyph[] Glyphs;

            public class Glyph
            {
                public int Width;
                public byte[] Pixels; // Length = Width * Height
            }

            public static FntModel Parse(byte[] data)
            {
                int height = BitConverter.ToUInt16(data, 2);
                int count = BitConverter.ToUInt16(data, 4);
                int tableStart = 0x08;
                int dataStart = tableStart + (count * 4);

                // Calculate the same stride used in the renderer
                int stride = 16;
                if (height > 16) stride = 32;
                if (height > 32) stride = 64;

                var font = new FntModel { Height = height, Glyphs = new FntModel.Glyph[count] };

                for (int i = 0; i < count; i++)
                {
                    int entryOff = tableStart + (i * 4);
                    int relPtr = BitConverter.ToUInt16(data, entryOff);
                    int width = BitConverter.ToUInt16(data, entryOff + 2);

                    // THE FIX: Each glyph occupies (Width * Stride) bytes in the file
                    byte[] pixels = new byte[width * stride];
                    if (dataStart + relPtr + pixels.Length <= data.Length)
                    {
                        Array.Copy(data, dataStart + relPtr, pixels, 0, pixels.Length);
                    }

                    font.Glyphs[i] = new FntModel.Glyph { Width = width, Pixels = pixels };
                }
                return font;
            }
        }

        public static Bitmap RenderFontAtlas(FntModel model, byte[] palData)
        {
            // Calculate total width to fit characters in a row
            int totalWidth = 0;
            foreach (var g in model.Glyphs) totalWidth += g.Width + 2; // +2 for spacing

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

        public static Bitmap RenderGlyph(FntModel.Glyph glyph, int fontHeight, byte[] palData)
        {
            int stride = 16;
            if (fontHeight > 16) stride = 32;
            if (fontHeight > 32) stride = 64;

            var bmp = new Bitmap(glyph.Width, fontHeight, PixelFormat.Format32bppArgb);
            var bmpData = bmp.LockBits(new Rectangle(0, 0, glyph.Width, fontHeight), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);

            int[] argb = new int[glyph.Width * fontHeight];

            for (int x = 0; x < glyph.Width; x++)
            {
                for (int y = 0; y < fontHeight; y++)
                {
                    // Use 'stride' to find the start of the next column in the file data
                    int fileOffset = (x * stride) + y;
                    if (fileOffset >= glyph.Pixels.Length) break;

                    byte idx = glyph.Pixels[fileOffset];

                    if (idx != 0)
                    {
                        int p = idx * 3;
                        // Rage 6-bit palette conversion (vga * 4)
                        int r = palData[p] * 4;
                        int g = palData[p + 1] * 4;
                        int b = palData[p + 2] * 4;

                        // Map to the correct (x,y) position in our horizontal row array
                        argb[y * glyph.Width + x] = (255 << 24) | (r << 16) | (g << 8) | b;
                    }
                }
            }

            System.Runtime.InteropServices.Marshal.Copy(argb, 0, bmpData.Scan0, argb.Length);
            bmp.UnlockBits(bmpData);
            return bmp;
        }
    }
}