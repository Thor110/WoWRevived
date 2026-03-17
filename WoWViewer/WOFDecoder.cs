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
        public int FaceOffset { get; init; }       // rec[0x1C..0x1F] >> 8
        public (int X, int Y, int Z)[] Verts { get; init; } = [];  // world-space
        public WofFace[] Faces { get; init; } = [];
    }

    public class WofFace
    {
        public byte V0, V1, V2;     // piece-local vertex indices
        public byte MatId;          // material/texture ID  (byte[3])
        public byte U0, V0uv;       // UV for vertex 0
        public byte U1, V1uv;       // UV for vertex 1
        public byte U2, V2uv;       // UV for vertex 2
    }

    public class WofModel
    {
        public int PieceCount { get; init; }
        public int VertOffset { get; init; }  // raw byte offset to vertex data
        public int TexOffset { get; init; }  // raw byte offset to 24576-byte texture
        public int MatOffset { get; init; }  // raw byte offset to 52-byte material table
        public WofPiece[] Pieces { get; init; } = [];
        public byte[] TextureData { get; init; } = [];  // 24576 bytes, 128×64 palette-indexed
        public byte[] MaterialData { get; init; } = [];  // 52 bytes, 13 × 4-byte entries
    }

    // ── Static decoder ────────────────────────────────────────────────────────

    public static class WofDecoder
    {
        private const int PieceRecordSize = 97;   // 0x61
        private const int PieceTableStart = 0x30;

        // Parse a decompressed WOF file.
        public static WofModel Parse(byte[] data)
        {
            int pieceCount = BitConverter.ToUInt16(data, 0x00);
            int vertOffset = BitConverter.ToInt32(data, 0x0C);
            int matOffset = BitConverter.ToInt32(data, 0x24);
            int texOffset = BitConverter.ToInt32(data, 0x28);

            var pieces = new WofPiece[pieceCount];
            int cumVerts = 0;

            for (int p = 0; p < pieceCount; p++)
            {
                int base_ = PieceTableStart + p * PieceRecordSize;

                // Name: 16 bytes null-padded
                int nameLen = 0;
                while (nameLen < 16 && data[base_ + nameLen] != 0) nameLen++;
                string name = Encoding.ASCII.GetString(data, base_, nameLen);

                byte flags = data[base_ + 0x10];
                byte vertCount = data[base_ + 0x11];
                byte faceCount = data[base_ + 0x12];
                short pivotX = BitConverter.ToInt16(data, base_ + 0x13);
                short pivotY = BitConverter.ToInt16(data, base_ + 0x15);
                short pivotZ = BitConverter.ToInt16(data, base_ + 0x17);
                int faceOffRaw = BitConverter.ToInt32(data, base_ + 0x1C);
                int faceOff = faceOffRaw >> 8;

                // Vertices: 3 × int16 each, cumulative across all pieces
                var verts = new (int X, int Y, int Z)[vertCount];
                for (int i = 0; i < vertCount; i++)
                {
                    int off = vertOffset + (cumVerts + i) * 6;
                    short vx = BitConverter.ToInt16(data, off);
                    short vy = BitConverter.ToInt16(data, off + 2);
                    short vz = BitConverter.ToInt16(data, off + 4);
                    // Apply pivot + negate Y (confirmed from IDA sub_49E690 at 0x49E7BD)
                    verts[i] = (pivotX + vx, -(pivotY + vy), pivotZ + vz);
                }

                // Faces: 10 bytes each — v0,v1,v2,matId, u0,v0,u1,v1,u2,v2
                var faces = new WofFace[faceCount];
                for (int i = 0; i < faceCount; i++)
                {
                    int off = faceOff + i * 10;
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
                    FaceOffset = faceOff,
                    Verts = verts,
                    Faces = faces,
                };
                cumVerts += vertCount;
            }

            // Texture: 24576 bytes at texOffset (128×64 palette-indexed)
            byte[] texData = new byte[24576];
            if (texOffset > 0 && texOffset + 24576 <= data.Length)
                Array.Copy(data, texOffset, texData, 0, 24576);

            // Material table: 52 bytes (13 × 4 bytes) at matOffset
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
                MaterialData = matData,
            };
        }

        // ── OBJ export ──────────────────────────────────────────────────────

        // Export to Wavefront OBJ + MTL.  Returns the OBJ text.
        // mtlPath: relative path for the mtllib reference (e.g. "model.mtl").
        // scale:   divide game units by this to get sensible metre-scale (~100).
        public static (string obj, string mtl) ToObj(WofModel model, string mtlName, float scale = 100f)
        {
            var obj = new StringBuilder();
            var mtl = new StringBuilder();

            obj.AppendLine($"# WOF->OBJ  {model.PieceCount} pieces");
            obj.AppendLine($"mtllib {mtlName}");
            obj.AppendLine();

            mtl.AppendLine("# WOF material library");
            mtl.AppendLine();

            // Write all vertices (global 1-based index)
            var vertBase = new Dictionary<string, int>(); // piece name -> 1-based global start
            int globalIdx = 1;

            foreach (var piece in model.Pieces)
            {
                vertBase[piece.Name] = globalIdx;
                obj.AppendLine($"# {piece.Name}  ({piece.VertCount}v {piece.FaceCount}f)  pivot=({piece.PivotX},{piece.PivotY},{piece.PivotZ})");
                foreach (var (x, y, z) in piece.Verts)
                    obj.AppendLine(FormattableString.Invariant($"v {x / scale:F4} {y / scale:F4} {z / scale:F4}"));
                globalIdx += piece.VertCount;
            }

            obj.AppendLine();

            // Write UV coords and faces grouped by material
            // Collect unique (u,v) pairs across all pieces for OBJ vt block
            var uvList = new List<(byte u, byte v)>();
            var uvIndex = new Dictionary<(byte, byte), int>(); // -> 1-based

            // Pre-pass to gather all UVs
            foreach (var piece in model.Pieces)
            {
                foreach (var f in piece.Faces)
                {
                    foreach (var uv in new[] { (f.U0, f.V0uv), (f.U1, f.V1uv), (f.U2, f.V2uv) })
                    {
                        if (!uvIndex.ContainsKey(uv))
                        {
                            uvIndex[uv] = uvList.Count + 1;
                            uvList.Add(uv);
                        }
                    }
                }
            }

            foreach (var (u, v) in uvList)
                obj.AppendLine(FormattableString.Invariant($"vt {u / 127f:F4} {v / 63f:F4}"));

            obj.AppendLine();

            // Collect unique material IDs for MTL
            var materialIds = new SortedSet<byte>();
            foreach (var piece in model.Pieces)
                foreach (var f in piece.Faces)
                    materialIds.Add(f.MatId);

            foreach (byte mid in materialIds)
            {
                mtl.AppendLine($"newmtl mat_{mid}");
                mtl.AppendLine("Ka 1.0 1.0 1.0");
                mtl.AppendLine("Kd 1.0 1.0 1.0");
                mtl.AppendLine("Ks 0.0 0.0 0.0");
                mtl.AppendLine($"map_Kd texture_mat{mid}.png");
                mtl.AppendLine();
            }

            // Write faces, grouped by piece then by material
            foreach (var piece in model.Pieces)
            {
                obj.AppendLine($"o {piece.Name}");
                int vBase = vertBase[piece.Name];

                // Group faces by material
                var byMat = piece.Faces
                    .GroupBy(f => f.MatId)
                    .OrderBy(g => g.Key);

                foreach (var group in byMat)
                {
                    obj.AppendLine($"usemtl mat_{group.Key}");
                    foreach (var f in group)
                    {
                        int a = vBase + f.V0;
                        int b = vBase + f.V1;
                        int c = vBase + f.V2;
                        int ta = uvIndex[(f.U0, f.V0uv)];
                        int tb = uvIndex[(f.U1, f.V1uv)];
                        int tc = uvIndex[(f.U2, f.V2uv)];
                        obj.AppendLine($"f {a}/{ta} {b}/{tb} {c}/{tc}");
                    }
                }
                obj.AppendLine();
            }

            return (obj.ToString(), mtl.ToString());
        }

        // ── Texture export ───────────────────────────────────────────────────

        // Render the embedded 128×64 texture atlas to a Bitmap using a PAL file.
        // palData: decompressed .PAL bytes.  First 768 bytes = 256 × RGB (6-bit, ×4).
        // shadeData: optional 512-byte SHH level-0 slice (256 × RGB565).
        //            Pass null to use the PAL directly.
        public static Bitmap RenderTexture(byte[] texData, byte[] palData, byte[]? shadeData = null)
        {
            const int W = 128, H = 64;
            var bmp = new Bitmap(W, H);
            bool useSHH = shadeData != null && shadeData.Length >= 512;

            for (int y = 0; y < H; y++)
            {
                for (int x = 0; x < W; x++)
                {
                    byte idx = texData[y * W + x];
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
                        int r = palData[idx * 3] * 4;
                        int g = palData[idx * 3 + 1] * 4;
                        int b = palData[idx * 3 + 2] * 4;
                        c = Color.FromArgb(Math.Min(r, 255), Math.Min(g, 255), Math.Min(b, 255));
                    }
                    bmp.SetPixel(x, y, c);
                }
            }
            return bmp;
        }

        // Export per-material texture crops.
        // The texture atlas is 128×64.  UV coords address it directly (U: 0-127, V: 0-63).
        // This method renders the full atlas once and returns it; the caller can crop as needed.
        public static Bitmap RenderTextureAtlas(WofModel model, byte[] palData, byte[]? shadeData = null)
            => RenderTexture(model.TextureData, palData, shadeData);

        // Auto-select PAL for WOF/IOB models.
        // WOF (unit) models use slot 2 -> HB.PAL based on PalSlots in SprViewer,
        // but the IDA analysis of LoadShadeTables shows F7GI is loaded for WOF rendering.
        // F7.PAL is the best general-purpose choice for textured WOF units.
        public static string SuggestPalette(string fileName, bool isIob)
        {
            string upper = Path.GetFileNameWithoutExtension(fileName).ToUpperInvariant();

            if (isIob)
            {
                // IOB = static building/object.  BM.PAL is the general map-object palette.
                return "BM.PAL";
            }

            // WOF unit models:
            // Human units start with H or AA (Anti-Aircraft), AR (Artillery), etc.
            // Martian units start with M or ZEP (Zeppelin), etc.
            // F7.PAL is the neutral terrain palette used for WOF rendering per IDA analysis.
            return "F7.PAL";
        }

        public static string SuggestShader(string palName)
        {
            // Mirror PalToShaderStem from SprViewer but use GI variant for WOF models
            // since they render with global illumination rather than per-terrain shading.
            string stem = Path.GetFileNameWithoutExtension(palName).ToUpperInvariant();
            return stem switch
            {
                "F1" or "F2" or "F3" or "F4" or "F5" or "F6" or "F7" => stem + "GI.SHH",
                "BM" => "BMGI.SHH",
                "SE" => "SEGI.SHH",
                "HW" => "HWGI.SHH",
                "HB" => "HBGI.SHH",
                "HR" => "HRGI.SHH",
                "MW" => "MWGI.SHH",
                "MB" => "MBGI.SHH",
                "MR" => "MRGI.SHH",
                _ => "F7GI.SHH",
            };
        }
    }
}