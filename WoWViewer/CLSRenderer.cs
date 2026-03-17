using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace WoWViewer
{
    public static class CLSRenderer
    {
        // ── Tile-type colour table ─────────────────────────────────────────────
        private static readonly int[] TileArgb = BuildTileArgb();

        private static int[] BuildTileArgb()
        {
            var c = new int[256];
            c[0] = unchecked((int)0xFF000000);
            c[1] = unchecked((int)0xFF1A3A6A);  // water
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

        // ── Shared pixel-write helper (no unsafe needed) ──────────────────────
        private static Bitmap WriteBitmap(int w, int h, int[] pixels)
        {
            var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
            var bd = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            Marshal.Copy(pixels, 0, bd.Scan0, pixels.Length);
            bmp.UnlockBits(bd);
            return bmp;
        }

        // ── Tile map ──────────────────────────────────────────────────────────
        public static Bitmap RenderTileMap(CLSModel model)
        {
            if (model.Tiles == null || model.TileW == 0) return new Bitmap(1, 1);
            int w = model.GridW, h = model.GridH, n = w * h;
            var px = new int[n];
            for (int i = 0; i < n; i++)
            {
                int row = Math.Min(i / w, model.TileH - 1);
                int col = Math.Min(i % w, model.TileW - 1);
                px[i] = TileArgb[model.Tiles[row * model.TileW + col]];
            }
            return WriteBitmap(w, h, px);
        }

        // ── Heightmap ─────────────────────────────────────────────────────────
        public static Bitmap RenderHeightmap(CLSModel model)
        {
            if (model.Heights.Length == 0) return new Bitmap(1, 1);
            int w = model.GridW, h = model.GridH, n = w * h;
            byte maxH = model.Heights.Max();
            if (maxH == 0) maxH = 1;
            var px = new int[n];
            for (int i = 0; i < n; i++)
            {
                byte raw = model.Heights[i];
                if (raw == 0) { px[i] = unchecked((int)0xFF1A2A3A); continue; }
                byte level = (byte)(60 + raw * 195 / maxH);
                px[i] = unchecked((int)(0xFF000000u | ((uint)level << 16) | ((uint)level << 8) | level));
            }
            return WriteBitmap(w, h, px);
        }

        // ── Composite (tile colour × height brightness) ───────────────────────
        public static Bitmap RenderComposite(CLSModel model)
        {
            if (model.Heights.Length == 0) return RenderTileMap(model);
            int w = model.GridW, h = model.GridH, n = w * h;
            byte maxH = model.Heights.Max();
            if (maxH == 0) maxH = 1;
            var px = new int[n];
            for (int i = 0; i < n; i++)
            {
                int row = i / w, col = i % w;
                byte hgt = model.Heights[i];

                int baseArgb = TileArgb[1]; // default water
                if (model.Tiles != null && row < model.TileH && col < model.TileW)
                    baseArgb = TileArgb[model.Tiles[row * model.TileW + col]];

                float bright = hgt == 0 ? 0.25f : 0.35f + hgt / (float)maxH * 0.65f;
                int r = Math.Min(255, (int)(((baseArgb >> 16) & 0xFF) * bright));
                int g = Math.Min(255, (int)(((baseArgb >> 8) & 0xFF) * bright));
                int b = Math.Min(255, (int)(((baseArgb) & 0xFF) * bright));
                px[i] = unchecked((int)(0xFF000000u | ((uint)r << 16) | ((uint)g << 8) | (uint)b));
            }
            return WriteBitmap(w, h, px);
        }
    }
}