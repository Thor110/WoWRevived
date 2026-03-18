using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace WoWViewer
{
    // ── IOB Building format ───────────────────────────────────────────────────
    //
    // IOB is the building/structure format used in Dat.wow alongside WOF unit files.
    // Buildings are pre-rendered isometric sprites stored in a palette-indexed texture
    // atlas. A BSP plane array provides collision geometry.
    //
    // ── CONFIRMED FILE LAYOUT (from IDA sub_49DF40 / sub_49E1E0) ─────────────
    //
    //  HEADER (34 bytes = 0x22):
    //   [0x00]  uint16  viewing_dir_count   always 3 in tested files
    //   [0x02]  uint32  face_count          total BSP planes = total normals
    //   [0x06]  uint32  lit_tri_count       triangles processed per lighting pass
    //   [0x0A]  uint32  animated_flag       0 = static, 1 = animated (index table present)
    //   [0x0E]  uint32  half_width_scale    sprite half-width in world units
    //   [0x12]  uint32  y_offset_scale      sprite Y anchor in world units
    //   [0x16]  uint32  height_scale        sprite height × 100 in world units
    //   [0x1A]  uint32  bsp_section_size    = face_count × 6  (confirmed exact)
    //   [0x1E]  uint32  normals_end_offset  relative to 0x22:
    //                     static:   points to end of used normals = pixel data start
    //                     animated: points to start of index table (= end of normals)
    //
    //  BSP PLANES (face_count × 6 bytes, starting at 0x22):
    //    Per entry: int16 nx, int16 ny, int16 nz  (position/plane vectors)
    //
    //  NORMALS (face_count × 3 bytes, immediately after BSP planes):
    //    Per entry: int8 nx, int8 ny, int8 nz  (compressed surface normals for lighting)
    //    Static files: only lit_tri_count entries are used (rest may be padding)
    //
    //  INDEX TABLE — animated only (lit_tri_count × 18 bytes):
    //    Immediately after the normals block. Contains per-triangle lighting indices
    //    and transform data. Starts at 0x22 + [0x1E].
    //
    //  PIXEL DATA (256 pixels wide, palette-indexed, to EOF):
    //    Static:   pixel_start = 0x22 + face_count*6 + lit_tri_count*3
    //    Animated: pixel_start = 0x22 + [0x1E] + lit_tri_count*18
    //    Height = (file_size - pixel_start) / 256  (truncated; remainder bytes
    //             form an incomplete final row and are discarded).

    public class IobModel
    {
        // ── Header fields ──────────────────────────────────────────────────────
        public int ViewingDirCount { get; init; }   // [0x00] uint16 — always 3
        public int FaceCount { get; init; }   // [0x02] total BSP planes / normals
        public int LitTriCount { get; init; }   // [0x06] lit triangles per pass
        public int AnimatedFlag { get; init; }   // [0x0A] 0=static, 1=animated
        public int HalfWidthScale { get; init; }   // [0x0E]
        public int YOffsetScale { get; init; }   // [0x12]
        public int HeightScale { get; init; }   // [0x16]
        public int BspSectionSize { get; init; }   // [0x1A] = FaceCount × 6
        public int NormalsEndOffset { get; init; }   // [0x1E] relative to 0x22

        // ── Geometry ───────────────────────────────────────────────────────────
        /// <summary>Vertex positions. Stored as int16 triplets (x,y,z). Divide by 100 for world units.
        /// Used for both collision (BSP) and as the renderable mesh vertex array.</summary>
        public (short Nx, short Ny, short Nz)[] BspPlanes { get; init; } = [];

        /// <summary>Compressed surface normals for lighting; one per face; stored as int8 triplets.</summary>
        public (sbyte Nx, sbyte Ny, sbyte Nz)[] Normals { get; init; } = [];

        /// <summary>Triangle faces. Each entry is (V0,V1,V2) indices into BspPlanes (the vertex array).
        /// Present for both static and animated IOBs.</summary>
        public (int V0, int V1, int V2)[] Faces { get; init; } = [];

        // ── Sprite atlas ───────────────────────────────────────────────────────
        public byte[] TexData { get; init; } = [];
        public int TexHeight { get; init; }
    }

    public static class IobDecoder
    {
        private const int HeaderSize = 0x22;  // 34 bytes
        public const int TexWidth = 256;

        // ── Parse ─────────────────────────────────────────────────────────────

        public static IobModel Parse(byte[] data)
        {
            if (data == null || data.Length < HeaderSize)
                return new IobModel();

            // Read all header fields as uint32 (the format stores them zero-padded to dword).
            int viewingDirCount = BitConverter.ToUInt16(data, 0x00);
            int faceCount = BitConverter.ToInt32(data, 0x02);
            int litTriCount = BitConverter.ToInt32(data, 0x06);
            int animatedFlag = BitConverter.ToInt32(data, 0x0A);
            int halfWidthScale = BitConverter.ToInt32(data, 0x0E);
            int yOffsetScale = BitConverter.ToInt32(data, 0x12);
            int heightScale = BitConverter.ToInt32(data, 0x16);
            int bspSectionSize = BitConverter.ToInt32(data, 0x1A);
            int normalsEndOffset = BitConverter.ToInt32(data, 0x1E);

            // Validate: bspSectionSize must equal faceCount × 6 (confirmed invariant).
            if (faceCount <= 0 || bspSectionSize != faceCount * 6)
                return new IobModel();

            // ── BSP planes ────────────────────────────────────────────────────
            int bspStart = HeaderSize;
            int bspEnd = bspStart + bspSectionSize;  // = 0x22 + faceCount*6

            if (bspEnd > data.Length)
                return new IobModel();

            var bspPlanes = new (short Nx, short Ny, short Nz)[faceCount];
            for (int i = 0; i < faceCount; i++)
            {
                int off = bspStart + i * 6;
                bspPlanes[i] = (
                    BitConverter.ToInt16(data, off),
                    BitConverter.ToInt16(data, off + 2),
                    BitConverter.ToInt16(data, off + 4)
                );
            }

            // ── Normals ───────────────────────────────────────────────────────
            // Immediately after BSP section: normalsCount × int8[3].
            // Static  (animatedFlag == 0): normalsCount = litTriCount  (one normal per lit triangle)
            // Animated(animatedFlag != 0): normalsCount = faceCount    (full normal pool)
            // Confirmed exact for ABBEY, PARLMENT (static) and CON_MAIN, LIGHTHSE (animated).
            int normCount = (animatedFlag == 0) ? litTriCount : faceCount;
            int normStart = bspEnd;
            int normEnd = normStart + normCount * 3;

            if (normEnd > data.Length)
                return new IobModel();

            var normals = new (sbyte Nx, sbyte Ny, sbyte Nz)[normCount];
            for (int i = 0; i < normCount; i++)
            {
                int off = normStart + i * 3;
                normals[i] = ((sbyte)data[off], (sbyte)data[off + 1], (sbyte)data[off + 2]);
            }

            // ── Index table → face connectivity ───────────────────────────────
            // [0x1E] is the index table start offset (from 0x22 base) for BOTH static
            // and animated. The table contains litTriCount × 18-byte entries.
            // Vertex indices are int16 at entry offsets [8],[10],[12]
            // (ebx = entry_base+10; reads [ebx-2],[ebx],[ebx+2] per IDA sub_49DF40).
            // Pixel data immediately follows the index table.
            int idxStart = HeaderSize + normalsEndOffset;
            int pixelStart = idxStart + litTriCount * 18;

            if (idxStart < 0 || pixelStart > data.Length)
                return new IobModel();

            var faces = new (int V0, int V1, int V2)[litTriCount];
            for (int i = 0; i < litTriCount; i++)
            {
                int ebx = idxStart + 10 + i * 18;
                if (ebx + 2 >= data.Length) break;
                faces[i] = (
                    BitConverter.ToInt16(data, ebx - 2),
                    BitConverter.ToInt16(data, ebx),
                    BitConverter.ToInt16(data, ebx + 2)
                );
            }

            if (pixelStart < 0 || pixelStart > data.Length)
                return new IobModel();

            // ── Texture atlas ─────────────────────────────────────────────────
            // 256 pixels wide, runs from pixelStart to EOF.
            // Any trailing bytes < 256 form an incomplete last row and are discarded.
            int texBytes = data.Length - pixelStart;
            int texHeight = texBytes / TexWidth;
            int texSize = texHeight * TexWidth;

            var texData = new byte[texSize];
            if (texSize > 0)
                Array.Copy(data, pixelStart, texData, 0, texSize);

            return new IobModel
            {
                ViewingDirCount = viewingDirCount,
                FaceCount = faceCount,
                LitTriCount = litTriCount,
                AnimatedFlag = animatedFlag,
                HalfWidthScale = halfWidthScale,
                YOffsetScale = yOffsetScale,
                HeightScale = heightScale,
                BspSectionSize = bspSectionSize,
                NormalsEndOffset = normalsEndOffset,
                BspPlanes = bspPlanes,
                Normals = normals,
                Faces = faces,
                TexData = texData,
                TexHeight = texHeight,
            };
        }

        // ── Texture rendering ─────────────────────────────────────────────────

        /// <summary>
        /// Renders the IOB sprite atlas as a 32-bit ARGB bitmap using the
        /// supplied 768-byte palette (256 × RGB triples). Palette index 0 = transparent.
        /// Returns a TexWidth × TexHeight pixel bitmap.
        /// </summary>
        public static Bitmap RenderTextureAtlas(IobModel model, byte[] palData, byte[]? shadeData = null)
        {
            int w = TexWidth;
            int h = Math.Max(1, model.TexHeight);
            var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);

            if (palData == null || palData.Length < 3)
                return bmp;

            var pixels = new int[w * h];
            for (int i = 0; i < pixels.Length; i++)
            {
                byte palIdx = i < model.TexData.Length ? model.TexData[i] : (byte)0;
                if (palIdx == 0) { pixels[i] = 0; continue; }

                byte r, g, b;
                if (shadeData != null && shadeData.Length >= 512)
                {
                    int shaded = shadeData[palIdx];
                    r = palData[shaded * 3];
                    g = palData[shaded * 3 + 1];
                    b = palData[shaded * 3 + 2];
                }
                else
                {
                    r = palData[palIdx * 3];
                    g = palData[palIdx * 3 + 1];
                    b = palData[palIdx * 3 + 2];
                }
                pixels[i] = (255 << 24) | (r << 16) | (g << 8) | b;
            }

            var bmpData = bmp.LockBits(new Rectangle(0, 0, w, h),
                ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            Marshal.Copy(pixels, 0, bmpData.Scan0, pixels.Length);
            bmp.UnlockBits(bmpData);
            return bmp;
        }

        // ── OBJ export ────────────────────────────────────────────────────────

        /// <summary>
        /// Exports the IOB building mesh as OBJ + MTL.
        /// BSP section = vertex positions (int16 × 3).
        /// Index table = face connectivity (litTriCount × 18 bytes, vertex indices at [8],[10],[12]).
        /// Both static and animated IOBs share this layout — animatedFlag only controls
        /// whether the building geometry animates in-game, not the file structure.
        /// </summary>
        public static (string ObjText, string MtlText) ToObj(IobModel model, string mtlName, string texName)
        {
            var obj = new System.Text.StringBuilder();
            var mtl = new System.Text.StringBuilder();

            obj.AppendLine($"# IOB building — {model.FaceCount} vertices, {model.LitTriCount} faces");
            obj.AppendLine($"mtllib {mtlName}");
            obj.AppendLine($"usemtl iob_building");
            obj.AppendLine();

            // Vertex positions from BSP section (int16 × 3).
            // Same coordinate scale as WOF units — divide by 100 for world units.
            const float Scale = 100f;
            foreach (var (nx, ny, nz) in model.BspPlanes)
                obj.AppendLine(FormattableString.Invariant(
                    $"v {nx / Scale:F4} {ny / Scale:F4} {nz / Scale:F4}"));
            obj.AppendLine();

            // Per-vertex normals from the normals section (int8 × 3, normalised to ±1).
            // normals array has min(faceCount, litTriCount) entries — pad with zeros if short.
            for (int i = 0; i < model.FaceCount; i++)
            {
                if (i < model.Normals.Length)
                {
                    var (nx, ny, nz) = model.Normals[i];
                    obj.AppendLine(FormattableString.Invariant(
                        $"vn {nx / 127f:F4} {ny / 127f:F4} {nz / 127f:F4}"));
                }
                else
                    obj.AppendLine("vn 0.0000 1.0000 0.0000");
            }
            obj.AppendLine();

            // Faces — 1-based OBJ indices.
            foreach (var (v0, v1, v2) in model.Faces)
            {
                int a = v0 + 1, b = v1 + 1, c = v2 + 1;
                obj.AppendLine($"f {a}//{a} {b}//{b} {c}//{c}");
            }

            mtl.AppendLine("newmtl iob_building");
            mtl.AppendLine("Ka 0.2 0.2 0.2");
            mtl.AppendLine("Kd 0.8 0.8 0.8");
            mtl.AppendLine("Ks 0.0 0.0 0.0");
            mtl.AppendLine($"map_Kd {texName}");

            return (obj.ToString(), mtl.ToString());
        }

        // ── Palette/shader helpers ────────────────────────────────────────────

        public static string SuggestPalette(string iobName)
        {
            // Buildings share a common palette.
            // Refine with prefix matching once palette assignments are confirmed.
            return "BUILD.PAL";
        }

        public static string SuggestShader(string iobName, string palName)
        {
            string pal = Path.GetFileNameWithoutExtension(palName).ToUpperInvariant();
            return pal + ".SHH";
        }

        // ── Raw texture export ────────────────────────────────────────────────

        /// <summary>
        /// Exports the palette-indexed sprite atlas as an 8-bpp PNG for lossless
        /// round-trips. Stride padding is handled via Marshal.Copy row-by-row.
        /// </summary>
        public static void ExportTextureRaw(IobModel model, byte[] palData, string path)
        {
            int w = TexWidth;
            int h = model.TexHeight;
            if (h <= 0) return;

            var bmp = new Bitmap(w, h, PixelFormat.Format8bppIndexed);
            var palette = bmp.Palette;
            for (int i = 0; i < 256; i++)
            {
                byte r = (i * 3 + 0 < palData.Length) ? palData[i * 3] : (byte)0;
                byte g = (i * 3 + 1 < palData.Length) ? palData[i * 3 + 1] : (byte)0;
                byte b = (i * 3 + 2 < palData.Length) ? palData[i * 3 + 2] : (byte)0;
                palette.Entries[i] = Color.FromArgb(i == 0 ? 0 : 255, r, g, b);
            }
            bmp.Palette = palette;

            var bmpData = bmp.LockBits(new Rectangle(0, 0, w, h),
                ImageLockMode.WriteOnly, PixelFormat.Format8bppIndexed);
            int stride = bmpData.Stride;
            var rowBuf = new byte[stride];
            for (int row = 0; row < h; row++)
            {
                for (int col = 0; col < w; col++)
                {
                    int idx = row * w + col;
                    rowBuf[col] = idx < model.TexData.Length ? model.TexData[idx] : (byte)0;
                }
                Marshal.Copy(rowBuf, 0, bmpData.Scan0 + row * stride, stride);
            }
            bmp.UnlockBits(bmpData);
            bmp.Save(path, ImageFormat.Png);
            bmp.Dispose();
        }
    }
}