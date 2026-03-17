using System.Drawing.Imaging;

namespace WoWViewer
{
    // =========================================================================
    // CLSRenderer  –  creates Bitmaps from a CLSModel for display and export
    // =========================================================================
    public static class CLSRenderer
    {
        // ── Tile-type colour table ─────────────────────────────────────────────
        // Index 0 = unused (black), index 1 = water (deep blue), 2–255 = HSV hue ramp
        private static readonly int[] TileArgb = BuildTileArgb();

        private static int[] BuildTileArgb()
        {
            var c = new int[256];
            c[0] = unchecked((int)0xFF000000);              // black  (unused)
            c[1] = unchecked((int)0xFF1A3A6A);              // water
            for (int i = 2; i < 256; i++)
            {
                double hue = ((i - 2) / 120.0) % 1.0;
                var (r, g, b) = HsvToRgb(hue, 0.72, 0.85);
                c[i] = unchecked((int)(0xFF000000u | ((uint)r << 16) | ((uint)g << 8) | (uint)b));
            }
            return c;
        }

        private static (int r, int g, int b) HsvToRgb(double h, double s, double v)
        {
            int hi = (int)(h * 6) % 6;
            double f = h * 6 - Math.Floor(h * 6);
            double p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
            var (rd, gd, bd) = hi switch
            {
                0 => (v, t, p),
                1 => (q, v, p),
                2 => (p, v, t),
                3 => (p, q, v),
                4 => (t, p, v),
                _ => (v, p, q)
            };
            return ((int)(rd * 255), (int)(gd * 255), (int)(bd * 255));
        }

        // ── Tile map (false-colour per tile ID) ───────────────────────────────
        public static Bitmap RenderTileMap(CLSModel model)
        {
            if (model.Tiles == null || model.TileW == 0 || model.TileH == 0)
                return MakePlaceholder();

            int w = model.TileW, h = model.TileH;
            var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
            WriteToBitmap(bmp, w, h, i => TileArgb[model.Tiles[i]]);
            return bmp;
        }

        // ── Heightmap (contrast-stretched grayscale, water tinted blue) ───────
        public static Bitmap RenderHeightmap(CLSModel model)
        {
            if (model.Heights.Length == 0) return MakePlaceholder();

            int w = model.GridW, h = model.GridH;
            byte maxH = model.Heights.Max() is byte m && m > 0 ? m : (byte)1;

            var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
            WriteToBitmap(bmp, w, h, i =>
            {
                if (i >= model.Heights.Length) return unchecked((int)0xFF101010);
                byte raw = model.Heights[i];
                if (raw == 0) return unchecked((int)0xFF1A2A3A);   // water
                byte level = (byte)(60 + raw * 195 / maxH);
                return unchecked((int)(0xFF000000u | ((uint)level << 16) | ((uint)level << 8) | level));
            });
            return bmp;
        }

        // ── Raw heightmap (true uint8 values, no stretch — for export/import) ─
        public static Bitmap RenderHeightmapRaw(CLSModel model)
        {
            if (model.Heights.Length == 0) return MakePlaceholder();
            int w = model.GridW, h = model.GridH;
            var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
            WriteToBitmap(bmp, w, h, i =>
            {
                byte v = i < model.Heights.Length ? model.Heights[i] : (byte)0;
                return unchecked((int)(0xFF000000u | ((uint)v << 16) | ((uint)v << 8) | v));
            });
            return bmp;
        }

        // ── Composite (tile colour × height brightness) ───────────────────────
        public static Bitmap RenderComposite(CLSModel model)
        {
            if (model.Heights.Length == 0) return RenderTileMap(model);

            int w = model.GridW, h = model.GridH;
            byte maxH = model.Heights.Max() is byte m && m > 0 ? m : (byte)1;

            var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
            WriteToBitmap(bmp, w, h, vi =>
            {
                int row = vi / w, col = vi % w;
                byte hgt = vi < model.Heights.Length ? model.Heights[vi] : (byte)0;

                // Tile colour: vertex (row,col) is at the corner of tile (row,col)
                int baseArgb = TileArgb[1]; // default to water
                if (model.Tiles != null && row < model.TileH && col < model.TileW)
                {
                    int ti = row * model.TileW + col;
                    if (ti < model.Tiles.Length)
                        baseArgb = TileArgb[model.Tiles[ti]];
                }

                float bright = hgt == 0 ? 0.25f : 0.35f + hgt / (float)maxH * 0.65f;
                int br_ = (baseArgb >> 16) & 0xFF;
                int bg = (baseArgb >> 8) & 0xFF;
                int bb = (baseArgb) & 0xFF;
                int r = Math.Min(255, (int)(br_ * bright));
                int g = Math.Min(255, (int)(bg * bright));
                int b = Math.Min(255, (int)(bb * bright));
                return unchecked((int)(0xFF000000u | ((uint)r << 16) | ((uint)g << 8) | (uint)b));
            });
            return bmp;
        }

        // ── Shared fast pixel-write helper ────────────────────────────────────
        private static unsafe void WriteToBitmap(Bitmap bmp, int w, int h, Func<int, int> pixelFunc)
        {
            var bd = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            try
            {
                int* ptr = (int*)bd.Scan0;
                int size = w * h;
                for (int i = 0; i < size; i++)
                    ptr[i] = pixelFunc(i);
            }
            finally
            {
                bmp.UnlockBits(bd);
            }
        }

        private static Bitmap MakePlaceholder()
        {
            var bmp = new Bitmap(1, 1, PixelFormat.Format32bppArgb);
            bmp.SetPixel(0, 0, Color.FromArgb(30, 30, 30));
            return bmp;
        }
    }
}