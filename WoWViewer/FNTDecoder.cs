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

            // The flawless mathematical start of the Row-Major pixel data
            int dataStart = tableStart + (count * 4) + 2;

            var font = new FntModel
            {
                Height = height,
                Glyphs = new FntModel.Glyph[count]
            };

            // STEP 1: Pre-read all X-Coordinates so we can calculate exact pixel widths
            int[] allX = new int[count];
            for (int i = 0; i < count; i++)
            {
                allX[i] = BitConverter.ToUInt16(data, tableStart + i * 4);
            }

            // STEP 2: Extract tightly packed characters
            for (int i = 0; i < count; i++)
            {
                int startX = allX[i];

                // Calculate the exact pixel width by finding the next X-coordinate in the atlas
                int endX = atlasWidth;
                for (int j = 0; j < count; j++)
                {
                    // Find the closest X coordinate to our right
                    if (allX[j] > startX && allX[j] < endX)
                    {
                        endX = allX[j];
                    }
                }

                int pixelWidth = endX - startX;
                if (pixelWidth <= 0) pixelWidth = 1;

                // Homeless-glyph check: some characters (notably space) have their
                // atlas_x placed inside a larger glyph's slot because they carry no
                // pixels of their own. Reading those atlas bytes would show the host
                // glyph's pixels instead of transparency.
                // Detection: is [startX, startX+pixelWidth) fully inside another
                // glyph's field2 slot [xj, xj+fj), with xj strictly less than startX?
                bool isHomeless = false;
                for (int j = 0; j < count; j++)
                {
                    if (j == i) continue;
                    int xj = allX[j];
                    int fj = BitConverter.ToUInt16(data, tableStart + j * 4 + 2);
                    if (xj < startX && (startX + pixelWidth) <= (xj + fj))
                    {
                        isHomeless = true;
                        break;
                    }
                }

                byte[] charPixels = new byte[pixelWidth * height];

                if (!isHomeless)
                {
                    // Standard Row-Major Atlas extraction
                    for (int y = 0; y < height; y++)
                    {
                        for (int x = 0; x < pixelWidth; x++)
                        {
                            int fileOffset = dataStart + (y * atlasWidth) + (startX + x);

                            if (fileOffset >= 0 && fileOffset < data.Length)
                            {
                                charPixels[y * pixelWidth + x] = data[fileOffset];
                            }
                        }
                    }
                }
                // else: charPixels stays all-zero — glyph has no pixels of its own.

                font.Glyphs[i] = new FntModel.Glyph { Width = pixelWidth, Pixels = charPixels };
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