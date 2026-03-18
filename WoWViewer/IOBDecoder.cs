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
        /// <summary>BSP plane position vectors. One per face; stored as int16 triplets.</summary>
        public (short Nx, short Ny, short Nz)[] BspPlanes { get; init; } = [];

        /// <summary>Compressed surface normals for lighting; one per face; stored as int8 triplets.</summary>
        public (sbyte Nx, sbyte Ny, sbyte Nz)[] Normals { get; init; } = [];

        /// <summary>
        /// Triangle face index table. Only populated for animated IOBs
        /// (AnimatedFlag != 0). Each entry is three vertex indices into BspPlanes.
        /// </summary>
        public (int V0, int V1, int V2)[] Faces { get; init; } = [];
        /// <summary>Raw palette-indexed pixel data. Width is always 256. Height = TexData.Length / 256.</summary>
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
            // Immediately after BSP section: faceCount × int8[3].
            int normStart = bspEnd;
            int normEnd = normStart + faceCount * 3;

            if (normEnd > data.Length)
                return new IobModel();

            var normals = new (sbyte Nx, sbyte Ny, sbyte Nz)[faceCount];
            for (int i = 0; i < faceCount; i++)
            {
                int off = normStart + i * 3;
                normals[i] = ((sbyte)data[off], (sbyte)data[off + 1], (sbyte)data[off + 2]);
            }

            // ── Face index table (animated only) ─────────────────────────────
            // Entry layout (18 bytes): [0-7]=misc, [8-9]=v0, [10-11]=v1, [12-13]=v2, [14-17]=misc
            // ebx starts at idxStart+10; reads [ebx-2],[ebx+0],[ebx+2] per triangle.
            var faces = new (int V0, int V1, int V2)[animatedFlag != 0 ? litTriCount : 0];
            if (animatedFlag != 0)
            {
                int idxStart = HeaderSize + normalsEndOffset;
                for (int i = 0; i < litTriCount; i++)
                {
                    int ebx = idxStart + 10 + i * 18;
                    if (ebx + 2 >= data.Length) break;
                    int v0 = BitConverter.ToInt16(data, ebx - 2);
                    int v1 = BitConverter.ToInt16(data, ebx);
                    int v2 = BitConverter.ToInt16(data, ebx + 2);
                    faces[i] = (v0, v1, v2);
                }
            }

            // ── Pixel data start ──────────────────────────────────────────────
            // Static:   normsEndOffset points to end of used normals = pixel start.
            // Animated: normsEndOffset points to index table start; pixels follow
            //           lit_tri_count × 18 bytes later.
            int pixelStart;
            if (animatedFlag == 0)
            {
                // Static: pixel_start = 0x22 + normalsEndOffset
                // This equals normStart + litTriCount*3 (confirmed exact for all tested files).
                pixelStart = HeaderSize + normalsEndOffset;
            }
            else
            {
                // Animated: index table at 0x22 + normalsEndOffset, each entry = 18 bytes.
                int idxStart = HeaderSize + normalsEndOffset;
                pixelStart = idxStart + litTriCount * 18;
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
        /// Exports the IOB model as an OBJ + MTL string pair.
        /// Only animated IOBs (AnimatedFlag != 0) carry explicit vertex positions
        /// and a face index table; static IOBs contain only BSP plane normals and
        /// produce no mesh output (the method returns empty strings with a comment).
        /// </summary>
        public static (string ObjText, string MtlText) ToObj(IobModel model, string mtlName, string texName)
        {
            if (model.AnimatedFlag == 0 || model.FaceCount == 0)
            {
                return (
                    $"# IOB static building — no explicit vertex geometry (BSP normals only)\n",
                    $"# No material — static IOB has no mesh\n"
                );
            }

            var obj = new System.Text.StringBuilder();
            var mtl = new System.Text.StringBuilder();

            obj.AppendLine($"# IOB animated building — {model.FaceCount} vertices, {model.LitTriCount} faces");
            obj.AppendLine($"mtllib {mtlName}");
            obj.AppendLine($"usemtl iob_building");
            obj.AppendLine();

            // Vertices — BSP planes store int16 positions; scale by 1/100 to match
            // the world-unit convention used by the engine (same as WOF Scale=100).
            const float Scale = 100f;
            foreach (var (nx, ny, nz) in model.BspPlanes)
            {
                float wx = nx / Scale;
                float wy = ny / Scale;   // Y is up in world space
                float wz = nz / Scale;
                obj.AppendLine(FormattableString.Invariant($"v {wx:F4} {wy:F4} {wz:F4}"));
            }
            obj.AppendLine();

            // Normals — int8 → float, normalised to [-1, 1].
            foreach (var (nx, ny, nz) in model.Normals)
            {
                float fnx = nx / 127f;
                float fny = ny / 127f;
                float fnz = nz / 127f;
                obj.AppendLine(FormattableString.Invariant($"vn {fnx:F4} {fny:F4} {fnz:F4}"));
            }
            obj.AppendLine();

            // Faces — the index table encodes triangle vertex indices.
            // OBJ indices are 1-based; vertex and normal arrays are parallel (same index).
            foreach (var (v0, v1, v2) in model.Faces)
            {
                int a = v0 + 1, b = v1 + 1, c = v2 + 1;
                obj.AppendLine($"f {a}//{a} {b}//{b} {c}//{c}");
            }

            // MTL
            mtl.AppendLine($"newmtl iob_building");
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