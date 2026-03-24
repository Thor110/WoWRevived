using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

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
    //   [0x0E]  uint32  half_width_scale    sprite canvas pixel width (full, despite name)
    //   [0x12]  uint32  y_offset_scale      sprite Y anchor in world units
    //   [0x16]  uint32  height_scale        sprite canvas pixel height
    //   [0x1A]  uint32  bsp_section_size    = face_count × 6  (confirmed exact)
    //   [0x1E]  uint32  normals_end_offset  offset from 0x22 to index table start
    //                     (= end of normals section for both static and animated)
    //
    //  BSP PLANES (face_count × 6 bytes, starting at 0x22):
    //    Per entry: int16 nx, int16 ny, int16 nz  (world-unit vertex positions)
    //
    //  NORMALS (after BSP):
    //    Static  (animatedFlag == 0): litTriCount × int8[3]
    //    Animated(animatedFlag != 0): faceCount   × int8[3]
    //
    //  INDEX TABLE (litTriCount × 18 bytes, at 0x22 + normalsEndOffset):
    //    [+0]     byte   w_byte       patch width hint (& 0x7F); also used as row count
    //                                 when h_byte bit7 is SET (mode B)
    //    [+1]     byte   h_byte       patch height / row count:
    //                                   bit7=0 → h_byte IS the row count (mode A)
    //                                   bit7=1 → w_byte is the row count (mode B)
    //    [+2]     byte   colour_mode  base colour index for the patch
    //    [+3]     byte   render_mode  selects pixel-writer function
    //    [+4..5]  int16  screen_X     pixel offset from building anchor (X)
    //    [+6..7]  int16  screen_Y     pixel offset from building anchor (Y)
    //    [+8..9]  uint16 norm_v0      normal index for lighting vertex 0
    //    [+10..11]uint16 norm_v1      normal index for lighting vertex 1
    //    [+12..13]uint16 norm_v2      normal index for lighting vertex 2
    //    [+14..17]uint32 patch_offset offset from file[0x22] to start of patch RLE data
    //            (high word non-zero for large files; previously misread as uint16)
    //
    //  PATCH RLE DATA (from index table end to EOF):
    //    The patch byte stream begins immediately at patch_offset — NO separate header.
    //    w_byte / h_byte / colour_mode live in the index table entry (confirmed: IDA
    //    sub_4579A4 reads them from [esi+0/1/2] = the index table entry pointer, while
    //    the pixel writer sub_45702D reads its data pointer 4CA0D0 = file[0x22] +
    //    patch_offset directly with no skip).
    //
    //    Per row: [skip_byte][run_byte] then either colour bytes or nothing:
    //      skip_byte                  = pixel offset from patch's screen_X anchor
    //      run_byte == 0x00           → end-of-patch sentinel (with skip_byte == 0x00)
    //                                   OR transparent row when skip_byte != 0x00
    //      run_byte & 0x80 == 0, > 0 → LITERAL: run_byte colour bytes follow
    //      run_byte & 0x80 != 0       → RLE: (run_byte & 0x7F) copies of one colour byte
    //
    //    Termination: [0x00][0x00] sentinel pair OR patch boundary (next patch's offset),
    //    whichever comes first. Both must be honoured. Verified byte-perfect across all
    //    patches in CP_BUNK.IOB — consumed bytes match inter-patch gaps exactly.
    //
    //    Pixel Y = screenY + rowIndex  (no +1 offset; game's Y increment is cosmetic)

    public record class IobModel
    {
        // ── Header fields ──────────────────────────────────────────────────────
        public int ViewingDirCount { get; init; }   // [0x00] uint16 — always 3
        public int FaceCount { get; init; }   // [0x02] total BSP planes / normals
        public int LitTriCount { get; init; }   // [0x06] lit triangles per pass
        public int AnimatedFlag { get; init; }   // [0x0A] 0=static, 1=animated
        public int HalfWidthScale { get; init; }   // [0x0E] canvas pixel width
        public int YOffsetScale { get; init; }   // [0x12]
        public int HeightScale { get; init; }   // [0x16] canvas pixel height
        public int BspSectionSize { get; init; }   // [0x1A] = FaceCount × 6
        public int NormalsEndOffset { get; init; }   // [0x1E] relative to 0x22

        // ── Geometry ───────────────────────────────────────────────────────────
        /// <summary>Vertex positions. int16 triplets (x,y,z) in world units.</summary>
        public (short Nx, short Ny, short Nz)[] BspPlanes { get; init; } = [];

        /// <summary>Compressed surface normals for lighting; int8 triplets, stored inward.</summary>
        public (sbyte Nx, sbyte Ny, sbyte Nz)[] Normals { get; init; } = [];

        /// <summary>Triangle faces — (V0,V1,V2) indices into BspPlanes.</summary>
        public (int V0, int V1, int V2)[] Faces { get; init; } = [];

        /// <summary>Per-patch screen layout from the index table.</summary>
        public PatchInfo[] Patches { get; init; } = [];

        // ── Sprite atlas ───────────────────────────────────────────────────────
        /// <summary>
        /// Raw bytes from file[0x22] to EOF. PatchOffset values from the index table
        /// index directly into this array (they are relative to file[0x22]).
        /// </summary>
        public byte[] TexData { get; init; } = [];
        public int TexHeight { get; init; }
        public int PatchDataBase { get; init; }  // always 0; kept for clarity
    }

    public record PatchInfo(
        int ScreenX, int ScreenY,
        int WByte, int HByte,   // raw index-table [+0] and [+1] (width/height hints)
        int ColourMode,           // index-table [+2]
        int RenderMode,           // index-table [+3]
        int NormV0, int NormV1, int NormV2,
        int PatchOffset);

    public static class IobDecoder
    {
        private const int HeaderSize = 0x22;  // 34 bytes
        public const int TexWidth = 256;

        // ── Parse ─────────────────────────────────────────────────────────────

        public static IobModel Parse(byte[] data)
        {
            if (data == null || data.Length < HeaderSize)
                return new IobModel();

            int viewingDirCount = BitConverter.ToUInt16(data, 0x00);
            int faceCount = BitConverter.ToInt32(data, 0x02);
            int litTriCount = BitConverter.ToInt32(data, 0x06);
            int animatedFlag = BitConverter.ToInt32(data, 0x0A);
            int halfWidthScale = BitConverter.ToInt32(data, 0x0E);
            int yOffsetScale = BitConverter.ToInt32(data, 0x12);
            int heightScale = BitConverter.ToInt32(data, 0x16);
            int bspSectionSize = BitConverter.ToInt32(data, 0x1A);
            int normalsEndOffset = BitConverter.ToInt32(data, 0x1E);

            // bspSectionSize must equal faceCount × 6 (confirmed invariant).
            if (faceCount <= 0 || bspSectionSize != faceCount * 6)
                return new IobModel();

            // ── BSP planes ────────────────────────────────────────────────────
            int bspStart = HeaderSize;
            int bspEnd = bspStart + bspSectionSize;

            if (bspEnd > data.Length)
                return new IobModel();

            var bspPlanes = new (short Nx, short Ny, short Nz)[faceCount];
            for (int i = 0; i < faceCount; i++)
            {
                int off = bspStart + i * 6;
                bspPlanes[i] = (
                    BitConverter.ToInt16(data, off),
                    BitConverter.ToInt16(data, off + 2),
                    BitConverter.ToInt16(data, off + 4));
            }

            // ── Normals ───────────────────────────────────────────────────────
            // Static  (animatedFlag == 0): litTriCount normals
            // Animated(animatedFlag != 0): faceCount   normals
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

            // ── Index table ───────────────────────────────────────────────────
            // Starts at 0x22 + normalsEndOffset for BOTH static and animated.
            // normalsEndOffset [0x1E] always points to the index table start
            // (= end of normals). Patch byte data follows immediately after.
            int idxStart = HeaderSize + normalsEndOffset;
            int pixelStart = idxStart + litTriCount * 18;

            if (idxStart < 0 || pixelStart > data.Length)
                return new IobModel();

            var faces = new (int V0, int V1, int V2)[litTriCount];
            var patches = new PatchInfo[litTriCount];

            for (int i = 0; i < litTriCount; i++)
            {
                int off = idxStart + i * 18;
                if (off + 16 > data.Length) break;

                // Index table layout (confirmed from IDA sub_4579A4 / sub_457C60):
                //   [+0] w_byte      — width hint (used as row count when h_byte bit7 set)
                //   [+1] h_byte      — height hint (used as row count when bit7 clear)
                //   [+2] colour_mode — base colour index
                //   [+3] render_mode — pixel-writer selector
                //   [+4..5]  screen_X (int16)
                //   [+6..7]  screen_Y (int16)
                //   [+8..9]  norm_v0 (uint16)
                //   [+10..11]norm_v1 (uint16)
                //   [+12..13]norm_v2 (uint16)
                //   [+14..17]patch_offset (uint32) — offset from file[0x22]
                //            The high word [+16..17] is non-zero for large IOB files
                //            (e.g. POD.IOB). Previously misread as uint16 + zero pad.
                int wByte = data[off];
                int hByte = data[off + 1];
                int colourMode = data[off + 2];
                int renderMode = data[off + 3];
                int screenX = BitConverter.ToInt16(data, off + 4);
                int screenY = BitConverter.ToInt16(data, off + 6);
                int v0 = BitConverter.ToUInt16(data, off + 8);
                int v1 = BitConverter.ToUInt16(data, off + 10);
                int v2 = BitConverter.ToUInt16(data, off + 12);
                int patchOff = (int)BitConverter.ToUInt32(data, off + 14);

                faces[i] = (v0, v1, v2);
                patches[i] = new PatchInfo(
                    screenX, screenY,
                    wByte, hByte,
                    colourMode, renderMode,
                    v0, v1, v2,
                    patchOff);
            }

            // ── Face winding correction ────────────────────────────────────────
            // Stored normals are INWARD (negated in sub_49DF40 before lighting dot).
            // Correct each face so cross(e1,e2) · outward_normal > 0.
            // Only meaningful for animated buildings with real 3D BSP positions.
            if (animatedFlag != 0)
            {
                for (int i = 0; i < faces.Length; i++)
                {
                    var (v0, v1, v2) = faces[i];
                    if (v0 >= bspPlanes.Length || v1 >= bspPlanes.Length || v2 >= bspPlanes.Length) continue;
                    if (v0 >= normals.Length) continue;

                    var (px0, py0, pz0) = bspPlanes[v0];
                    var (px1, py1, pz1) = bspPlanes[v1];
                    var (px2, py2, pz2) = bspPlanes[v2];

                    float e1x = px1 - px0, e1y = py1 - py0, e1z = pz1 - pz0;
                    float e2x = px2 - px0, e2y = py2 - py0, e2z = pz2 - pz0;

                    float cx = e1y * e2z - e1z * e2y;
                    float cy = e1z * e2x - e1x * e2z;
                    float cz = e1x * e2y - e1y * e2x;

                    if (cx * cx + cy * cy + cz * cz < 1e-10f) continue;

                    var (nx, ny, nz) = normals[v0];
                    // outward = -stored normal
                    if (cx * (-nx) + cy * (-ny) + cz * (-nz) < 0f)
                        faces[i] = (v0, v2, v1);
                }
            }

            // TexData: all bytes from file[0x22] to EOF.
            // PatchOffset values index directly into this array.
            int texBytes = Math.Max(0, data.Length - HeaderSize);
            int texHeight = texBytes / TexWidth;

            var texData = new byte[texBytes];
            if (texBytes > 0)
                Array.Copy(data, HeaderSize, texData, 0, texBytes);

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
                Patches = patches,
                TexData = texData,
                TexHeight = texHeight,
                PatchDataBase = 0,
            };
        }

        // ── Texture rendering ─────────────────────────────────────────────────

        /// <summary>
        /// Renders the IOB building sprite onto a HalfWidthScale × HeightScale canvas.
        ///
        /// Patch RLE format (confirmed byte-perfect from CP_BUNK.IOB analysis):
        ///
        ///   The stream starts at file[0x22 + patchOffset] with NO header bytes to skip.
        ///   w_byte / h_byte / colourMode live in the index table entry, not the stream.
        ///
        ///   Per row: [skip_byte][run_byte] then zero or more colour bytes:
        ///     skip == 0 &amp;&amp; run == 0  → end-of-patch sentinel, stop decoding this patch
        ///     run &amp; 0x80 == 0, > 0   → LITERAL: run colour bytes follow
        ///     run &amp; 0x80 != 0        → RLE: (run &amp; 0x7F) copies of the next colour byte
        ///     run == 0 (skip != 0)   → transparent row, no colour bytes
        ///
        ///   Termination: sentinel [0x00][0x00] OR patch boundary, whichever first.
        ///   Pixel X = screenX + skip_byte,  Pixel Y = screenY + rowIndex
        /// </summary>
        public static Bitmap RenderTextureAtlas(IobModel model, byte[] palData, byte[]? shadeData = null)
        {
            int W = Math.Max(1, model.HalfWidthScale);
            // HeightScale can be 0 for some IOB files (e.g. RF_DOORS). Derive from
            // patch extents so we still produce a valid bitmap in those cases.
            int H = model.HeightScale > 0
                ? model.HeightScale
                : model.Patches.Length > 0
                    ? model.Patches.Max(p => p.ScreenY + p.HByte) + 1
                    : 1;
            H = Math.Max(1, H);
            var canvas = new byte[W * H];   // palette indices; 0 = transparent

            DecodePatchesToCanvas(model, canvas, W, H);

            // Palette → ARGB
            var pixels = new int[W * H];
            for (int i = 0; i < canvas.Length; i++)
            {
                byte idx = canvas[i];
                if (idx == 0) { pixels[i] = 0; continue; }

                int r, g, b;
                if (shadeData != null && shadeData.Length >= idx * 2 + 2)
                {
                    // SHH level-0 slice: 256 × RGB565
                    int rgb565 = shadeData[idx * 2] | (shadeData[idx * 2 + 1] << 8);
                    int rv = (rgb565 >> 11) & 0x1F; r = (rv << 3) | (rv >> 2);
                    int gv = (rgb565 >> 5) & 0x3F; g = (gv << 2) | (gv >> 4);
                    int bv = rgb565 & 0x1F; b = (bv << 3) | (bv >> 2);
                }
                else
                {
                    // PAL: 6-bit RGB → 8-bit
                    r = palData.Length > idx * 3 + 2 ? palData[idx * 3] : 0;
                    g = palData.Length > idx * 3 + 2 ? palData[idx * 3 + 1] : 0;
                    b = palData.Length > idx * 3 + 2 ? palData[idx * 3 + 2] : 0;
                    r = Math.Min(r * 4, 255);
                    g = Math.Min(g * 4, 255);
                    b = Math.Min(b * 4, 255);
                }
                pixels[i] = (255 << 24) | (r << 16) | (g << 8) | b;
            }

            var bmp = new Bitmap(W, H, PixelFormat.Format32bppArgb);
            var bmpData = bmp.LockBits(new Rectangle(0, 0, W, H),
                              ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            Marshal.Copy(pixels, 0, bmpData.Scan0, pixels.Length);
            bmp.UnlockBits(bmpData);
            return bmp;
        }

        /// <summary>
        /// Decodes all RLE/literal patches from model.TexData onto a palette-index canvas (W×H).
        /// </summary>
        private static void DecodePatchesToCanvas(IobModel model, byte[] canvas, int W, int H)
        {
            byte[] raw = model.TexData;

            // Build a sorted list of patch offsets so we know each patch's byte boundary.
            // A patch ends at either its [0x00][0x00] sentinel or the next patch's offset,
            // whichever comes first.
            var sortedOffsets = model.Patches
                .Select(p => p.PatchOffset)
                .Distinct()
                .OrderBy(o => o)
                .ToList();
            sortedOffsets.Add(raw.Length);  // sentinel for the last patch

            for (int i = 0; i < model.Patches.Length; i++)
            {
                var patch = model.Patches[i];

                // PatchOffset is relative to file[0x22] = start of TexData.
                int patchStart = patch.PatchOffset - model.PatchDataBase;
                if (patchStart < 0 || patchStart >= raw.Length) continue;

                // Determine the hard boundary for this patch (next distinct patch offset).
                int nextIdx = sortedOffsets.BinarySearch(patch.PatchOffset);
                int patchEnd = (nextIdx + 1 < sortedOffsets.Count)
                                 ? sortedOffsets[nextIdx + 1]
                                 : raw.Length;

                // screen_X values are direct canvas X coordinates (originX = 0).
                int baseX = patch.ScreenX;
                int baseY = patch.ScreenY;
                int pos = patchStart;
                int row = 0;

                while (pos + 1 < patchEnd)
                {
                    int skip = raw[pos++];
                    int runByte = raw[pos++];

                    // [0x00][0x00] = end-of-patch sentinel.
                    if (skip == 0 && runByte == 0) break;

                    int py = baseY + row;
                    int cx = baseX + skip;
                    int count = runByte & 0x7F;

                    if ((runByte & 0x80) != 0)
                    {
                        // RLE: (runByte & 0x7F) copies of the next colour byte.
                        if (pos >= patchEnd) break;
                        byte colour = raw[pos++];
                        for (int k = 0; k < count; k++)
                        {
                            int fx = cx + k;
                            if ((uint)py < (uint)H && (uint)fx < (uint)W)
                                canvas[py * W + fx] = colour;
                        }
                    }
                    else if (count > 0)
                    {
                        // Literal: `count` individual colour bytes.
                        for (int k = 0; k < count; k++, pos++)
                        {
                            if (pos >= patchEnd) break;
                            int fx = cx + k;
                            if ((uint)py < (uint)H && (uint)fx < (uint)W)
                                canvas[py * W + fx] = raw[pos];
                        }
                    }
                    // count == 0 (run_byte == 0x00, skip != 0): transparent row, nothing written.

                    row++;
                }
            }
        }

        // ── OBJ export ────────────────────────────────────────────────────────

        /// <summary>
        /// Exports the IOB building as a Wavefront OBJ mesh.
        ///
        /// FULL-XYZ mode (e.g. CON_MAIN, LONDST):
        ///   BSP planes have real XYZ positions — exported verbatim.
        ///   Detected by non-trivial X or Z variation across planes.
        ///
        /// NORMAL-DERIVED mode (e.g. LIGHTHSE):
        ///   All BSP planes have X=Z=0 (radial/cylindrical building).
        ///   XZ positions are reconstructed from surface normals:
        ///     vx = (n.Nx / 127) × HalfWidthScale
        ///     vz = (n.Nz / 127) × HalfWidthScale
        ///   (pixel-to-world scale not yet calibrated; ~12.3 px/unit estimated)
        /// </summary>
        public static (string obj, string mtl) ToObj(IobModel model, string mtlName, string texName)
        {
            var obj = new StringBuilder();
            var mtl = new StringBuilder();

            obj.AppendLine($"mtllib {mtlName}");
            obj.AppendLine("usemtl iob_building");

            bool isRadial = true;
            foreach (var p in model.BspPlanes)
                if (p.Nx != 0 || p.Nz != 0) { isRadial = false; break; }

            for (int i = 0; i < model.FaceCount; i++)
            {
                var p = model.BspPlanes[i];
                float vx, vy, vz;

                if (isRadial && i < model.Normals.Length)
                {
                    var n = model.Normals[i];
                    vx = (n.Nx / 127f) * model.HalfWidthScale;
                    vz = (n.Nz / 127f) * model.HalfWidthScale;
                    vy = (p.Ny * model.HeightScale) + ((n.Ny / 127f) * model.HeightScale) + model.YOffsetScale;
                }
                else
                {
                    vx = p.Nx;
                    vy = p.Ny;
                    vz = p.Nz;
                }
                obj.AppendLine(FormattableString.Invariant($"v {vx:F4} {vy:F4} {vz:F4}"));
            }

            for (int i = 0; i < model.FaceCount && i < model.Normals.Length; i++)
            {
                var n = model.Normals[i];
                // Negate stored (inward) normals to produce outward normals for Blender.
                // Radial buildings already face inward by construction so negate those too.
                obj.AppendLine(FormattableString.Invariant(
                    $"vn {-n.Nx / 127f:F4} {-n.Ny / 127f:F4} {-n.Nz / 127f:F4}"));
            }

            foreach (var f in model.Faces)
            {
                // V1/V2 swap corrects CW→CCW winding for solid shading in Blender.
                obj.AppendLine($"f {f.V0 + 1}//{f.V0 + 1} {f.V2 + 1}//{f.V2 + 1} {f.V1 + 1}//{f.V1 + 1}");
            }

            mtl.AppendLine("newmtl iob_building");
            mtl.AppendLine($"map_Kd {texName}");

            return (obj.ToString(), mtl.ToString());
        }

        // ── Palette / shader suggestions ──────────────────────────────────────
        //
        // All IOB buildings share BM.PAL (confirmed from IDA).
        //
        // The game selects shaders at session start via a single global faction flag
        // (byte_4B84C4 in sub_40B6C0): non-zero = Human → BMHBB.SHH, zero = Martian →
        // BMMBB.SHH. There is no per-filename logic in the game itself.
        //
        // For the viewer we use a manual dictionary so you can add/correct entries as
        // you encounter them. The default fallback is BMHBB.SHH (human buildings).
        // RED.IOB uses HWHW.SHH (confirmed).
        // Add entries here in the form: { "FILENAME_WITHOUT_EXTENSION", "SHADER.SHH" }

        private static readonly Dictionary<string, string> ShaderMap =
            new(StringComparer.OrdinalIgnoreCase)
        {
            // ── Human buildings ────────────────────────────────────── BMHBB.SHH (default)
            // (no entry needed — covered by fallback)

            // ── Martian buildings ──────────────────────────────────── BMMBB.SHH
            { "POD",       "BMMBB.SHH" },

            // ── Special / override ─────────────────────────────────────────────
            { "RED",       "HWHW.SHH"  },
        };

        public static string SuggestPalette(string iobName) => "BM.PAL";

        public static string SuggestShader(string iobName, string palName)
        {
            string key = Path.GetFileNameWithoutExtension(iobName);
            return ShaderMap.TryGetValue(key, out string? shd) ? shd : "BMHBB.SHH";
        }
    }
}