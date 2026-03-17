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
            // Terrain type colour table — derived from height/usage analysis of all 30 maps.
            // 121 tile IDs are organised in 8 base-type groups of 16 (IDs 1–128, 121 used).
            // Type 1 (IDs 1–16) is split into three water sub-types by variant offset.
            //   ID 1       = Deep Water    (100% height=0, open sea)
            //   IDs 2–5    = Shallow Water (avg height 6, tripod-wading depth)
            //   IDs 6–16   = Coastal       (transition to beach)
            //   IDs 17–32  = Beach
            //   IDs 33–48  = Grass
            //   IDs 49–64  = Road
            //   IDs 65–80  = Rock
            //   IDs 81–96  = Highland
            //   IDs 97–112 = Mountain
            //   IDs 113–128= Peak
            var c = new int[256];
            c[0] = unchecked((int)0xFF000000); // unused

            void Fill(int from, int to, int r, int g, int b)
            {
                int argb = unchecked((int)(0xFF000000u | ((uint)r << 16) | ((uint)g << 8) | (uint)b));
                for (int i = from; i <= Math.Min(to, 255); i++) c[i] = argb;
            }

            Fill(1, 1, 0x1A, 0x2A, 0x6A); // Deep Water
            Fill(2, 5, 0x3A, 0x6A, 0x9A); // Shallow Water
            Fill(6, 16, 0x5A, 0x7A, 0x8A); // Coastal
            Fill(17, 32, 0xC8, 0xB5, 0x60); // Beach
            Fill(33, 48, 0x4A, 0x7A, 0x3A); // Grass
            Fill(49, 64, 0x7A, 0x6A, 0x5A); // Road
            Fill(65, 80, 0x8A, 0x7A, 0x6A); // Rock
            Fill(81, 96, 0x9A, 0x8A, 0x7A); // Highland
            Fill(97, 112, 0xAA, 0xAA, 0xAA); // Mountain
            Fill(113, 128, 0xEE, 0xEE, 0xFF); // Peak
            // Any IDs above 128 get a visible magenta so they stand out if encountered
            Fill(129, 255, 0xFF, 0x00, 0xFF);

            return c;
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