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
            int dataStart = tableStart + (count * 4) + 2;

            var font = new FntModel
            {
                AtlasWidth = atlasWidth,
                Height = height,
                Glyphs = new FntModel.Glyph[count]
            };

            // STEP 1: Pre-read all atlas X-coordinates.
            int[] allX = new int[count];
            for (int i = 0; i < count; i++)
                allX[i] = BitConverter.ToUInt16(data, tableStart + i * 4);

            // STEP 2: Extract each glyph.
            //
            // Pixel width = distance to the nearest atlas_x to the right (old_w).
            // This is the correct extraction width; field2 (the second uint16 per table
            // entry) is the text-advance/cursor-spacing width used by the game's text
            // renderer and is unrelated to pixel extraction.
            //
            // Homeless-glyph check: some characters (e.g. space) have their atlas_x
            // placed inside a larger glyph's field2 slot because they have no pixels
            // of their own and the atlas packer reused that space. Reading those atlas
            // bytes would show the host glyph's pixels instead of transparency.
            // Detection: the glyph's old_w slot [startX, startX+pixelWidth) lies
            // entirely inside another glyph j's field2 slot [xj, xj+fj), where
            // xj < startX (strict, to avoid misidentifying same-x entries like
            // TOOLTIP's space and '#' which both start at x=10).
            // Homeless glyphs are returned as all-transparent (all-zero pixels).
            for (int i = 0; i < count; i++)
            {
                int startX = allX[i];

                // Find nearest atlas_x to the right → pixel extraction width.
                int endX = atlasWidth;
                for (int j = 0; j < count; j++)
                {
                    if (allX[j] > startX && allX[j] < endX)
                        endX = allX[j];
                }

                int pixelWidth = endX - startX;
                if (pixelWidth <= 0) pixelWidth = 1;

                // Homeless check: is [startX, startX+pixelWidth) fully inside
                // another glyph's [xj, xj+field2j)?
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
                    for (int y = 0; y < height; y++)
                    {
                        for (int x = 0; x < pixelWidth; x++)
                        {
                            int fileOffset = dataStart + (y * atlasWidth) + (startX + x);
                            if (fileOffset >= 0 && fileOffset < data.Length)
                                charPixels[y * pixelWidth + x] = data[fileOffset];
                        }
                    }
                }
                // else: charPixels stays all-zero (transparent).

                font.Glyphs[i] = new FntModel.Glyph { Width = pixelWidth, Pixels = charPixels };
            }
            return font;
        }

        public static Bitmap RenderGlyph(FntModel.Glyph glyph, int fontHeight, byte[] palData)
        {
            if (glyph.Width <= 0 || fontHeight <= 0) return new Bitmap(1, 1);

            var bmp = new Bitmap(glyph.Width, fontHeight, PixelFormat.Format32bppArgb);
            var bmpData = bmp.LockBits(new Rectangle(0, 0, glyph.Width, fontHeight),
                              ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            int[] argb = new int[glyph.Width * fontHeight];

            for (int i = 0; i < glyph.Pixels.Length; i++)
            {
                byte idx = glyph.Pixels[i];
                if (idx != 0)
                {
                    int p = idx * 3;
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
                int curX = 2, curY = 2;

                foreach (var glyph in model.Glyphs)
                {
                    if (glyph.Width <= 0) continue;
                    if (curX + glyph.Width > atlasWidth) { curX = 2; curY += model.Height + 4; }

                    using (Bitmap charBmp = RenderGlyph(glyph, model.Height, palData))
                        g.DrawImage(charBmp, curX, curY);

                    curX += glyph.Width + 2;
                }
            }
            return bmp;
        }
    }
}