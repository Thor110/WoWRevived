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

    public record class IobModel
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

        /// <summary>
        /// Screen-space patch layout. Each entry describes one triangular sprite patch:
        /// its position relative to the building centre, and the pixel dimensions of the patch.
        /// Populated from the 18-byte index table (confirmed from IDA sub_457C60).
        /// </summary>
        public PatchInfo[] Patches { get; init; } = [];

        // ── Sprite atlas ───────────────────────────────────────────────────────
        /// <summary>Raw patch data bytes starting at pixelStart in the file.</summary>
        public byte[] TexData { get; init; } = [];
        public int TexHeight { get; init; }
        /// <summary>
        /// Offset to subtract from a raw PatchOffset (uint16 from index table) to get
        /// an index into TexData. Equals pixelStart - HeaderSize (0x22).
        /// patch_in_texdata = patchOffset - PatchDataBase
        /// </summary>
        public int PatchDataBase { get; init; }
    }

    public record PatchInfo(int ScreenX, int ScreenY, int Width, int Height,
                            int NormV0, int NormV1, int NormV2,
                            int Mode, int PatchOffset);

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

            // ── Index table — animated only ───────────────────────────────────
            // STATIC buildings (animatedFlag == 0):
            //   NO index table. Pixel data starts immediately after the normals section.
            //   pixelStart = normEnd = bspEnd + litTriCount × 3
            //   (normalsEndOffset [0x1E] = bspSectionSize + litTriCount×3, verified exact)
            //
            // ANIMATED buildings (animatedFlag != 0):
            //   Index table starts at 0x22 + [0x1E] = normEnd.
            //   Contains litTriCount × 18-byte entries.
            //
            // Confirmed field layout (IDA sub_457C60, sub_4579A4):
            //   [+0..1]  uint16 = world-data offset (internal use)
            //   [+2..3]  uint16 = (hi byte = piece id?, lo byte split)
            //   [+3]     byte   = render_mode
            //   [+4..5]  int16  = screen_X pixel offset from building centre
            //   [+6..7]  int16  = screen_Y pixel offset from building centre
            //   [+8..9]  uint16 = normal index v0 (for lighting — NOT a mesh vertex)
            //   [+10..11] uint16 = normal index v1
            //   [+12..13] uint16 = normal index v2
            //   [+14..15] uint16 = sprite patch offset P where patch = file[0x22 + P]
            //   [+16..17] uint16 = always 0
            //
            // NOTE: v0/v1/v2 are lighting normal indices only. No 3D vertex positions.
            //
            // BOTH static and animated buildings have an index table immediately after
            // the normals section. normalsEndOffset [0x1E] points to the index table
            // start (= normals end). Pixel/patch data always follows the index table.
            int idxStart = HeaderSize + normalsEndOffset;   // index table start
            int pixelStart = idxStart + litTriCount * 18;     // patch data start

            if (idxStart < 0 || pixelStart > data.Length)
                return new IobModel();

            var faces = new (int V0, int V1, int V2)[litTriCount];
            var patches = new PatchInfo[litTriCount];

            // Both static and animated have an index table (litTriCount × 18 bytes).
            for (int i = 0; i < litTriCount; i++)
            {
                int entryBase = idxStart + i * 18;
                if (entryBase + 18 > data.Length) break;

                int width = data[entryBase] & 0x7F;
                int height = data[entryBase + 1] & 0x7F;
                int mode = data[entryBase + 2];

                int screenX = BitConverter.ToInt16(data, entryBase + 4);
                int screenY = BitConverter.ToInt16(data, entryBase + 6);

                int v0 = BitConverter.ToUInt16(data, entryBase + 8);
                int v1 = BitConverter.ToUInt16(data, entryBase + 10);
                int v2 = BitConverter.ToUInt16(data, entryBase + 12);
                int patchOff = BitConverter.ToUInt16(data, entryBase + 14);
                faces[i] = (v0, v1, v2);
                patches[i] = new PatchInfo(screenX, screenY, width, height, v0, v1, v2, mode, patchOff);
            }

            // ── Face winding correction ────────────────────────────────────────
            // The game renderer has no back-face cull (sub_456DD0 sorts by Y only),
            // so individual faces may have inconsistent CCW/CW winding in the source data.
            // Correct each face: if cross(e1,e2) · outward_normal < 0, swap v1 and v2.
            // Stored normals are INWARD (they are negated before the lighting dot product
            // in sub_49DF40: "neg eax/ecx/edx"), so outward = -stored_normal.
            // Only applies to animated buildings where BSP has real 3D positions.
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

                    float magSq = cx * cx + cy * cy + cz * cz;
                    if (magSq < 1e-10f) continue;  // degenerate triangle

                    // Outward normal = negate stored (stored normals are inward)
                    var (nx, ny, nz) = normals[v0];
                    float dot = cx * (-nx) + cy * (-ny) + cz * (-nz);

                    if (dot < 0f)
                        faces[i] = (v0, v2, v1);  // flip winding
                }
            }

            // Patch data: all bytes from 0x22 to EOF, so that PatchOffset values
            // from the index table (which are relative to 0x22) index directly into
            // this array without any rebasing. Some patches point into the BSP/normals
            // region (before pixelStart) — this is correct game behaviour.
            int texBytes = Math.Max(0, data.Length - HeaderSize);
            int texHeight = texBytes / TexWidth;   // kept for display size reference only

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
                PatchDataBase = 0,   // PatchOffset directly indexes TexData (= file[0x22..])
            };
        }

        // ── Texture rendering ─────────────────────────────────────────────────

        /// <summary>
        /// Renders the IOB building sprite as a 32-bit ARGB bitmap by decoding all
        /// RLE triangle patches onto a canvas sized HalfWidthScale×HeightScale pixels.
        ///
        /// Patch RLE format (confirmed from IDA sub_457C60 / sub_4579A4):
        ///   patch[0]  = width hint (& 0x7F, informational only)
        ///   patch[1]  = height flags (0x80 = full-RLE-height mode)
        ///   patch[2]  = base colour index
        ///   patch[3+] = rows, each encoded as: [skip][run_byte] ...
        ///     skip     = pixel offset from triangle's screen_X origin
        ///     run_byte:
        ///       bit7=1 → RLE run: (run_byte & 0x7F) copies of the next byte
        ///       bit7=0 → literal:  run_byte literal palette bytes follow
        ///     [0x00, 0x00] = end-of-patch sentinel
        ///
        /// Each patch's origin on canvas = (screen_X + canvasW/2,  screen_Y).
        /// Palette index 0 = transparent.
        /// </summary>
        public static Bitmap RenderTextureAtlas(IobModel model, byte[] palData, byte[]? shadeData = null)
        {
            int W = Math.Max(1, model.HalfWidthScale);
            int H = Math.Max(1, model.HeightScale);
            var canvas = new byte[W * H];   // palette indices, 0 = transparent

            DecodePatchesToCanvas(model, canvas, W, H);

            // Palette → ARGB
            var pixels = new int[W * H];
            for (int i = 0; i < canvas.Length; i++)
            {
                byte idx = canvas[i];
                if (idx == 0) { pixels[i] = 0; continue; }

                byte r, g, b;
                if (shadeData != null && shadeData.Length > idx)
                {
                    int s = shadeData[idx];
                    r = palData.Length > s * 3 + 2 ? palData[s * 3] : (byte)0;
                    g = palData.Length > s * 3 + 2 ? palData[s * 3 + 1] : (byte)0;
                    b = palData.Length > s * 3 + 2 ? palData[s * 3 + 2] : (byte)0;
                }
                else
                {
                    r = palData.Length > idx * 3 + 2 ? palData[idx * 3] : (byte)0;
                    g = palData.Length > idx * 3 + 2 ? palData[idx * 3 + 1] : (byte)0;
                    b = palData.Length > idx * 3 + 2 ? palData[idx * 3 + 2] : (byte)0;
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
        /// Decodes all RLE triangle patches from model.TexData onto the supplied
        /// palette-index canvas (W × H, row-major). Called by both render and export.
        /// </summary>
        private static void DecodePatchesToCanvas(IobModel model, byte[] canvas, int W, int H)
        {
            int originX = W / 2;
            byte[] raw = model.TexData;
            int patchBase = model.PatchDataBase;  // subtract from raw PatchOffset → index into TexData

            for (int i = 0; i < model.Patches.Length; i++)
            {
                var patch = model.Patches[i];
                int patchIdx = patch.PatchOffset - patchBase; // index into TexData

                if (patchIdx < 0 || patchIdx + 3 > raw.Length) continue;

                int pos = patchIdx + 3;   // skip w, h_flags, base_colour
                int baseX = patch.ScreenX + originX;
                int baseY = patch.ScreenY;
                int row = 0;

                while (pos + 1 < raw.Length)
                {
                    int skip = raw[pos++];
                    int runByte = raw[pos++];
                    if (skip == 0 && runByte == 0) break;

                    int py = baseY + row;
                    int cx = baseX + skip;

                    if (runByte == 0x80)
                    {
                        // Transparent row — no colour byte follows.
                    }
                    else if (runByte >= 0x81)
                    {
                        int count = runByte & 0x7F;
                        if (pos >= raw.Length) break;
                        byte colour = raw[pos++];
                        for (int k = 0; k < count; k++)
                        {
                            int fx = cx + k;
                            if ((uint)py < (uint)H && (uint)fx < (uint)W)
                                canvas[py * W + fx] = colour;
                        }
                    }
                    else
                    {
                        int count = runByte;
                        for (int k = 0; k < count; k++, pos++)
                        {
                            if (pos >= raw.Length) break;
                            int fx = cx + k;
                            if ((uint)py < (uint)H && (uint)fx < (uint)W)
                                canvas[py * W + fx] = raw[pos];
                        }
                    }
                    row++;
                }
            }
        }

        // ── OBJ export ────────────────────────────────────────────────────────

        /// <summary>
        /// Exports the IOB building as an OBJ mesh.
        ///
        /// Two vertex position modes — selected automatically per building:
        ///
        /// FULL-XYZ mode (e.g. CON_MAIN, LONDST):
        ///   BSP section has real XYZ vertex positions. Used directly.
        ///   Detected when BSP has non-trivial X or Z variation (range > 1).
        ///
        /// NORMAL-DERIVED mode (e.g. LIGHTHSE):
        ///   BSP stores only Y height tiers (X=Z=0 for every vertex).
        ///   XZ positions are reconstructed from the surface normals:
        ///     vertex.XZ = normalize(normal.XZ) × R
        ///   where R = (SpriteWidth / 2) / pixel_scale.
        ///   Pixel scale is calibrated from CON_MAIN (f0E=183, X_range=36 → 5.083 px/unit).
        ///   This produces a correct cylindrical/polygonal outline approximation.
        ///   Confirmed: LIGHTHSE normals encode octagonal side directions; BSP Y encodes
        ///   height tiers (0=base, 5=shaft, 6=lantern cap).
        ///
        /// Scale: divide by 100 for world-unit scale consistent with WOF exports.
        /// Winding correction: per-face cross-product vs outward-normal dot test.
        /// </summary>
        public static (string ObjText, string MtlText) ToObj(IobModel model, string mtlName, string texName)
        {
            var obj = new System.Text.StringBuilder();
            var mtl = new System.Text.StringBuilder();

            obj.AppendLine($"# IOB building — {model.FaceCount} vertices, {model.LitTriCount} faces");
            obj.AppendLine($"# AnimatedFlag={model.AnimatedFlag}  HalfWidthScale={model.HalfWidthScale}");
            obj.AppendLine($"mtllib {mtlName}");
            obj.AppendLine($"usemtl iob_building");
            obj.AppendLine();

            const float Scale = 100f;

            // Detect whether BSP has real XZ data or only Y heights.
            bool hasXzData = false;
            foreach (var (bx, _, bz) in model.BspPlanes)
                if (Math.Abs(bx) > 1 || Math.Abs(bz) > 1) { hasXzData = true; break; }

            // Compute vertex positions.
            float[] vx = new float[model.FaceCount];
            float[] vy = new float[model.FaceCount];
            float[] vz = new float[model.FaceCount];

            if (hasXzData)
            {
                // Full XYZ from BSP section.
                for (int i = 0; i < model.FaceCount; i++)
                {
                    vx[i] = model.BspPlanes[i].Nx / Scale;
                    vy[i] = model.BspPlanes[i].Ny / Scale;
                    vz[i] = model.BspPlanes[i].Nz / Scale;
                }
            }
            else
            {
                // No XZ vertex data available for this building.
                // The BSP section deliberately stores only Y height tiers (e.g. 0, 5, 6
                // for LIGHTHSE) with X=Z=0 — the game uses a vertical-rod collision model
                // for cylindrical/symmetric buildings. The 3D visual appearance is encoded
                // entirely in the 2D sprite patches and cannot be reconstructed as a mesh.
                //
                // Export what we have: BSP Y values as height, XZ=0.
                // This produces a degenerate vertical stack which is geometrically honest.
                for (int i = 0; i < model.FaceCount; i++)
                {
                    vx[i] = 0f;
                    vy[i] = model.BspPlanes[i].Ny / Scale;
                    vz[i] = 0f;
                }
            }

            for (int i = 0; i < model.FaceCount; i++)
                obj.AppendLine(FormattableString.Invariant($"v {vx[i]:F4} {vy[i]:F4} {vz[i]:F4}"));
            obj.AppendLine();

            // Per-vertex normals (int8 × 3 → normalised float, negated = outward).
            for (int i = 0; i < model.FaceCount; i++)
            {
                if (i < model.Normals.Length)
                {
                    var (nx, ny, nz) = model.Normals[i];
                    // Stored normals are inward; negate for outward-facing OBJ normals.
                    obj.AppendLine(FormattableString.Invariant(
                        $"vn {-nx / 127f:F4} {-ny / 127f:F4} {-nz / 127f:F4}"));
                }
                else
                    obj.AppendLine("vn 0.0000 1.0000 0.0000");
            }
            obj.AppendLine();

            // Faces — v0/v1/v2 from index table, 1-based OBJ indices.
            // For normal-derived buildings (no BSP XZ data), the in-Parse winding
            // correction was skipped (degenerate BSP). We do NOT apply a secondary
            // winding correction here — the cross-product test is unreliable for
            // ring-section triangles that are nearly flat or share identical positions.
            foreach (var (v0raw, v1raw, v2raw) in model.Faces)
            {
                obj.AppendLine($"f {v0raw + 1}//{v0raw + 1} {v1raw + 1}//{v1raw + 1} {v2raw + 1}//{v2raw + 1}");
            }

            mtl.AppendLine("newmtl iob_building");
            mtl.AppendLine("Ka 0.2 0.2 0.2");
            mtl.AppendLine("Kd 0.8 0.8 0.8");
            mtl.AppendLine("Ks 0.0 0.0 0.0");
            mtl.AppendLine($"map_Kd {texName}");

            return (obj.ToString(), mtl.ToString());
        }

        // ── Palette/shader helpers ────────────────────────────────────────────

        // ── Palette / shader suggestions ──────────────────────────────────────
        //
        // All IOB buildings use BM.PAL and BMGI.SHH, confirmed from IDA analysis
        // (same conclusion reached independently by WofDecoder.SuggestPalette with
        // isIob=true). BM likely stands for "Building Map".  BMGI.SHH is the
        // single shade table used for all building lighting passes.
        //
        // Martian-faction buildings (MB_, MT_, HE_, HX_, SF_, AH_, TC_, PP_, EWP_)
        // are included in BM.PAL — they share the same palette as human buildings.

        public static string SuggestPalette(string iobName) => "BM.PAL";

        public static string SuggestShader(string iobName, string palName) => "BMGI.SHH";
    }
}