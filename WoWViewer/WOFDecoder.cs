using System.Text;

namespace WoWViewer
{
    // ── Parsed data structures ────────────────────────────────────────────────

    public class WofPiece
    {
        public string Name { get; init; } = "";
        public byte Flags { get; init; }       // rec[0x10]
        public byte VertCount { get; init; }       // rec[0x11]
        public byte FaceCount { get; init; }       // rec[0x12]
        public short PivotX { get; init; }       // rec[0x13..0x14]
        public short PivotY { get; init; }       // rec[0x15..0x16]  negated by renderer
        public short PivotZ { get; init; }       // rec[0x17..0x18]
        public int VertByteOff { get; init; }       // rec[0x19..0x1C]  byte offset from vert section base (IDA sub_49E900)
        public int FaceOffset { get; init; }       // rec[0x1C..0x1F] >> 8
        // Children stored as consecutive int32s at rec[0x21], rec[0x25], rec[0x29]...
        // terminated by -1. Max 16 entries. (IDA sub_49E900: cmp [arg_0], 10h)
        public int[] BspChildren { get; init; } = [];

        // Raw accumulated pivot (sum of stored pivot values up the BSP parent chain).
        // Set after Parse() resolves the tree. World coords: world_y = -(RawAccumY + vert_y)
        public int RawAccumX { get; set; }
        public int RawAccumY { get; set; }
        public int RawAccumZ { get; set; }

        public (short X, short Y, short Z)[] Verts { get; init; } = [];
        public WofFace[] Faces { get; init; } = [];
    }

    public class WofFace
    {
        public byte V0, V1, V2;   // piece-local vertex indices
        public byte MatId;        // material/texture ID
        public byte U0, V0uv;
        public byte U1, V1uv;
        public byte U2, V2uv;
    }

    public class WofModel
    {
        public int PieceCount { get; init; }
        public int VertOffset { get; init; }
        public int TexOffset { get; init; }
        public int MatOffset { get; init; }
        public WofPiece[] Pieces { get; init; } = [];
        public byte[] TextureData { get; init; } = [];  // 256×TexHeight bytes, stride 256
        public int TexHeight { get; init; }         // actual rows = (end_off - tex_off) / 256
        public byte[] MaterialData { get; init; } = [];  // 52 bytes, 13 × 4-byte entries
    }

    // ── Static decoder ────────────────────────────────────────────────────────

    public static class WofDecoder
    {
        private const int PieceRecordSize = 97;   // 0x61
        private const int PieceTableStart = 0x30;

        // Texture layout: 256 pixels wide × N rows, 1 byte per pixel (palette index).
        // Stride = 256. Height varies per model: (header[0x2C] - header[0x28]) / 256.
        // header[0x2C] = end_off (confirmed = tex_off + tex_size in all tested models).
        // Material table byte[0] = U offset, byte[1] = V offset into this atlas.
        public const int TexWidth = 256;

        public static WofModel Parse(byte[] data)
        {
            if (data.Length < 0x30) return Empty();

            int pieceCount = BitConverter.ToUInt16(data, 0x00);
            int vertOffset = BitConverter.ToInt32(data, 0x0C);
            int matOffset = BitConverter.ToInt32(data, 0x24);
            int texOffset = BitConverter.ToInt32(data, 0x28);
            // header[0x2C] = end of texture section (confirmed from file analysis).
            // True texture size = endOffset - texOffset; height = size / 256.
            int endOffset = BitConverter.ToInt32(data, 0x2C);
            int texSize = (endOffset > texOffset && endOffset <= data.Length)
                             ? endOffset - texOffset : 24576;
            int texHeight = texSize / TexWidth;  // always stride=256

            // Guard against corrupt / IOB files
            if (vertOffset <= 0 || vertOffset >= data.Length || pieceCount is <= 0 or > 500)
                return Empty();

            var pieces = new WofPiece[pieceCount];

            for (int p = 0; p < pieceCount; p++)
            {
                int base_ = PieceTableStart + p * PieceRecordSize;
                if (base_ + PieceRecordSize > data.Length) break;

                int nameLen = 0;
                while (nameLen < 16 && data[base_ + nameLen] != 0) nameLen++;
                string name = Encoding.ASCII.GetString(data, base_, nameLen);

                byte flags = data[base_ + 0x10];
                byte vertCount = data[base_ + 0x11];
                byte faceCount = data[base_ + 0x12];
                short pivotX = BitConverter.ToInt16(data, base_ + 0x13);
                short pivotY = BitConverter.ToInt16(data, base_ + 0x15);
                short pivotZ = BitConverter.ToInt16(data, base_ + 0x17);
                int vertByteOff = BitConverter.ToInt32(data, base_ + 0x19);
                int faceOff = BitConverter.ToInt32(data, base_ + 0x1C) >> 8;

                // Read all BSP children from rec[0x21], rec[0x25], rec[0x29]...
                // Each is a signed int32; list terminates at -1. Max 16 (IDA: cmp [arg_0], 10h).
                var bspChildren = new List<int>();
                for (int ci = 0; ci < 16; ci++)
                {
                    int coff = base_ + 0x21 + ci * 4;
                    if (coff + 4 > data.Length) break;
                    int child = BitConverter.ToInt32(data, coff);
                    if (child < 0) break;
                    bspChildren.Add(child);
                }

                // Vertices: 3 × int16 each, at vertOffset + vertByteOff + i*6
                var verts = new (short X, short Y, short Z)[vertCount];
                for (int i = 0; i < vertCount; i++)
                {
                    int off = vertOffset + vertByteOff + i * 6;
                    if (off + 6 > data.Length) break;
                    verts[i] = (
                        BitConverter.ToInt16(data, off),
                        BitConverter.ToInt16(data, off + 2),
                        BitConverter.ToInt16(data, off + 4)
                    );
                }

                // Faces: 10 bytes each — v0,v1,v2,matId, u0,v0,u1,v1,u2,v2
                var faces = new WofFace[faceCount];
                for (int i = 0; i < faceCount; i++)
                {
                    int off = faceOff + i * 10;
                    if (off + 10 > data.Length) break;
                    faces[i] = new WofFace
                    {
                        V0 = data[off],
                        V1 = data[off + 1],
                        V2 = data[off + 2],
                        MatId = data[off + 3],
                        U0 = data[off + 4],
                        V0uv = data[off + 5],
                        U1 = data[off + 6],
                        V1uv = data[off + 7],
                        U2 = data[off + 8],
                        V2uv = data[off + 9],
                    };
                }

                pieces[p] = new WofPiece
                {
                    Name = name,
                    Flags = flags,
                    VertCount = vertCount,
                    FaceCount = faceCount,
                    PivotX = pivotX,
                    PivotY = pivotY,
                    PivotZ = pivotZ,
                    VertByteOff = vertByteOff,
                    FaceOffset = faceOff,
                    BspChildren = bspChildren.ToArray(),
                    Verts = verts,
                    Faces = faces,
                };
            }

            ResolvePivots(pieces);

            // Texture: 256×texHeight bytes, stride 256. Size from header[0x2C]-header[0x28].
            byte[] texData = new byte[texSize];
            if (texOffset > 0 && texOffset + texSize <= data.Length)
                Array.Copy(data, texOffset, texData, 0, texSize);

            // Material table: 52 bytes (13 × 4 bytes)
            // byte[0] = U offset in atlas, byte[1] = V offset in atlas,
            // byte[3] bit7 = use texture flag
            byte[] matData = new byte[52];
            if (matOffset > 0 && matOffset + 52 <= data.Length)
                Array.Copy(data, matOffset, matData, 0, 52);

            return new WofModel
            {
                PieceCount = pieceCount,
                VertOffset = vertOffset,
                TexOffset = texOffset,
                MatOffset = matOffset,
                Pieces = pieces,
                TextureData = texData,
                TexHeight = texHeight,
                MaterialData = matData,
            };
        }

        private static WofModel Empty() => new WofModel
        {
            PieceCount = 0,
            Pieces = [],
            TextureData = new byte[256 * 96],
            TexHeight = 96,
            MaterialData = new byte[52]
        };

        private static void ResolvePivots(WofPiece[] pieces)
        {
            int n = pieces.Length;

            // Build parent map from the full children list
            var parentOf = new int[n];
            for (int i = 0; i < n; i++) parentOf[i] = -1;
            for (int i = 0; i < n; i++)
                foreach (int c in pieces[i].BspChildren)
                    if (c >= 0 && c < n)
                        parentOf[c] = i;

            // Accumulate raw pivot values in topological order.
            // World formula: world_y = -(RawAccumY + vert_y)
            var done = new bool[n];
            bool progress = true;
            while (progress)
            {
                progress = false;
                for (int i = 0; i < n; i++)
                {
                    if (done[i]) continue;
                    int par = parentOf[i];
                    if (par >= 0 && !done[par]) continue;

                    var p = pieces[i];
                    if (par < 0)
                    {
                        p.RawAccumX = p.PivotX;
                        p.RawAccumY = p.PivotY;
                        p.RawAccumZ = p.PivotZ;
                    }
                    else
                    {
                        var parent = pieces[par];
                        p.RawAccumX = parent.RawAccumX + p.PivotX;
                        p.RawAccumY = parent.RawAccumY + p.PivotY;
                        p.RawAccumZ = parent.RawAccumZ + p.PivotZ;
                    }
                    done[i] = true;
                    progress = true;
                }
            }
        }

        // ── OBJ export ────────────────────────────────────────────────────────

        public static (string obj, string mtl) ToObj(WofModel model, string mtlName,
            string texName = "texture_atlas.png", float scale = 100f)
        {
            var obj = new StringBuilder();
            var mtl = new StringBuilder();

            obj.AppendLine($"# WOF->OBJ  {model.PieceCount} pieces");
            obj.AppendLine($"mtllib {mtlName}");
            obj.AppendLine();
            mtl.AppendLine("# WOF material library");
            mtl.AppendLine();

            // Vertices:
            //   world_x =   (RawAccumX + vx) / scale
            //   world_y = - (RawAccumY + vy) / scale
            //   world_z =   (RawAccumZ + vz) / scale
            var vertBase = new Dictionary<string, int>();
            int globalIdx = 1;

            foreach (var piece in model.Pieces)
            {
                vertBase[piece.Name] = globalIdx;
                int ax = piece.RawAccumX, ay = piece.RawAccumY, az = piece.RawAccumZ;
                obj.AppendLine($"# {piece.Name}  ({piece.VertCount}v {piece.FaceCount}f)  rawAccum=({ax},{ay},{az})");
                foreach (var (vx, vy, vz) in piece.Verts)
                    obj.AppendLine(FormattableString.Invariant(
                        $"v {(ax + vx) / scale:F4} {-(ay + vy) / scale:F4} {(az + vz) / scale:F4}"));
                globalIdx += piece.VertCount;
            }
            obj.AppendLine();

            // UV coords — face UVs (0-127, 0-63) mapped into the 256×96 atlas
            // using per-material offsets from the material table.
            // For now output normalised UVs relative to the full 256×96 atlas.
            // Material byte[0] = U atlas offset, byte[1] = V atlas offset.
            var uvList = new List<(float u, float v)>();
            var uvIndex = new Dictionary<(byte matId, byte fu, byte fv), int>();

            foreach (var piece in model.Pieces)
                foreach (var f in piece.Faces)
                    foreach (var (fu, fv) in new[] { (f.U0, f.V0uv), (f.U1, f.V1uv), (f.U2, f.V2uv) })
                    {
                        var key = (f.MatId, fu, fv);
                        if (!uvIndex.ContainsKey(key))
                        {
                            // Apply material atlas offset
                            byte uOff = model.MaterialData.Length >= (f.MatId * 4 + 2)
                                ? model.MaterialData[f.MatId * 4] : (byte)0;
                            byte vOff = model.MaterialData.Length >= (f.MatId * 4 + 2)
                                ? model.MaterialData[f.MatId * 4 + 1] : (byte)0;
                            float nu = (uOff + fu) / (float)TexWidth;
                            float nv = (vOff + fv) / (float)(model.TexHeight > 0 ? model.TexHeight : 96);
                            uvIndex[key] = uvList.Count + 1;
                            uvList.Add((nu, nv));
                        }
                    }

            foreach (var (nu, nv) in uvList)
                obj.AppendLine(FormattableString.Invariant($"vt {nu:F4} {1f - nv:F4}"));
            obj.AppendLine();

            // Materials — all share one atlas texture, named correctly for the export
            var mids = new SortedSet<byte>();
            foreach (var piece in model.Pieces)
                foreach (var f in piece.Faces)
                    mids.Add(f.MatId);

            foreach (byte mid in mids)
            {
                mtl.AppendLine($"newmtl mat_{mid}");
                mtl.AppendLine("Ka 1.0 1.0 1.0\nKd 1.0 1.0 1.0\nKs 0.0 0.0 0.0");
                mtl.AppendLine($"map_Kd {texName}\n");
            }

            // Faces
            foreach (var piece in model.Pieces)
            {
                obj.AppendLine($"o {piece.Name}");
                int vBase = vertBase[piece.Name];
                foreach (var group in piece.Faces.GroupBy(f => f.MatId).OrderBy(g => g.Key))
                {
                    obj.AppendLine($"usemtl mat_{group.Key}");
                    foreach (var f in group)
                        obj.AppendLine(
                            $"f {vBase + f.V0}/{uvIndex[(f.MatId, f.U0, f.V0uv)]}" +
                            $" {vBase + f.V1}/{uvIndex[(f.MatId, f.U1, f.V1uv)]}" +
                            $" {vBase + f.V2}/{uvIndex[(f.MatId, f.U2, f.V2uv)]}");
                }
                obj.AppendLine();
            }

            return (obj.ToString(), mtl.ToString());
        }

        // ── Texture rendering ─────────────────────────────────────────────────

        // Render the full texture atlas at its actual height (256 × model.TexHeight).
        // palData:   decompressed .PAL bytes (first 768 = 256 × RGB, 6-bit × 4).
        // shadeData: optional 512-byte SHH level slice (256 × RGB565). Pass null for raw PAL.
        public static Bitmap RenderTextureAtlas(WofModel model, byte[] palData, byte[]? shadeData = null)
            => RenderTexture(model.TextureData, model.TexHeight, palData, shadeData);

        public static Bitmap RenderTexture(byte[] texData, int texHeight, byte[] palData, byte[]? shadeData = null)
        {
            bool useSHH = shadeData?.Length >= 512;
            var bmp = new Bitmap(TexWidth, texHeight);
            for (int y = 0; y < texHeight; y++)
                for (int x = 0; x < TexWidth; x++)
                {
                    int pos = y * TexWidth + x;
                    byte idx = pos < texData.Length ? texData[pos] : (byte)0;
                    Color c;
                    if (useSHH)
                    {
                        int rgb565 = shadeData![idx * 2] | (shadeData[idx * 2 + 1] << 8);
                        int rv = (rgb565 >> 11) & 0x1F; int r = (rv << 3) | (rv >> 2);
                        int gv = (rgb565 >> 5) & 0x3F; int g = (gv << 2) | (gv >> 4);
                        int bv = rgb565 & 0x1F; int b = (bv << 3) | (bv >> 2);
                        c = Color.FromArgb(r, g, b);
                    }
                    else
                    {
                        c = Color.FromArgb(
                            Math.Min(palData[idx * 3] * 4, 255),
                            Math.Min(palData[idx * 3 + 1] * 4, 255),
                            Math.Min(palData[idx * 3 + 2] * 4, 255));
                    }
                    bmp.SetPixel(x, y, c);
                }
            return bmp;
        }

        // ── Faction / palette detection ───────────────────────────────────────

        // Models that use MB.PAL (Martian Blue palette).
        // Determined empirically from the full 145-file WOF list.
        // Note: some martian-themed prefixes (AN_, DRONE, MED_GU, MOB_RE, SEL_PR, SH_)
        // render correctly with F7.PAL and are deliberately excluded.
        private static readonly string[] MbPalPrefixes =
        [
            "ELECTR", "FIGHT", "FLYING", "HANDLE",
            "HEAT_R", "HEAT_RT", "PROJEC", "PROJECT",
            "R_RAY", "SCANIN", "SCOU_", "SCOU_M",
            "TELEPA", "TEMPES", "BOMBAR_", "CONS_M",
            "DIGGER_", "DRONE_"
        ];

        public static bool NeedsMbPal(string fileName)
        {
            string upper = Path.GetFileNameWithoutExtension(fileName).ToUpperInvariant();
            return MbPalPrefixes.Any(p => upper.StartsWith(p, StringComparison.Ordinal));
        }

        // ── Palette / shader suggestions ──────────────────────────────────────
        //
        // ALL WOF models use palette slot 752 (0x2F0) in LoadShadeTables (IDA sub_40B6C0),
        // which loads BMHVB.SHH unconditionally for all WOF unit models.
        // The faction prefix (H/M in var_3C) is NOT inserted into the BMHV filename.
        //
        // PAL selection varies by model group:
        //   F7.PAL  — default for most WOF units (human vehicles, martian drones etc.)
        //   MB.PAL  — Martian Blue, for the models listed in MbPalPrefixes above
        //   BM.PAL  — IOB buildings
        public static string SuggestPalette(string fileName, bool isIob)
        {
            if (isIob) return "BM.PAL";
            if (NeedsMbPal(fileName)) return "MB.PAL";
            return "F7.PAL";
        }

        public static string SuggestShader(string fileName, string palName)
        {
            string stem = Path.GetFileNameWithoutExtension(palName).ToUpperInvariant();
            return stem switch
            {
                "BM" => "BMGI.SHH",   // IOB buildings
                "MW" or "MB" or "MR" => "BMMVB.SHH",  // Martian palette families
                _ => "BMHVB.SHH",  // all WOF units (slot 752)
            };
        }
    }
}