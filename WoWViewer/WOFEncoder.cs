using System.Text;

namespace WoWViewer
{
    // ── OBJ intermediate structures ──────────────────────────────────────────

    internal class ObjPiece
    {
        public string Name { get; set; } = "";
        public int MatId { get; set; }        // dominant mat (first usemtl seen)
        public List<(float x, float y, float z)> Verts { get; } = [];
        public List<(int v0, int v1, int v2, int mat, float u0, float v0uv, float u1, float v1uv, float u2, float v2uv)> Faces { get; } = [];
    }

    // ── Encoder ──────────────────────────────────────────────────────────────

    public static class WofEncoder
    {
        private const int HeaderSize = 0x30;
        private const int PieceRecSize = 97;
        private const int MaterialCount = 13;
        private const int MaterialStride = 4;
        private const float Scale = 100f;

        // ── Public entry point ────────────────────────────────────────────────

        /// <summary>
        /// Encode a WOF file from OBJ + texture PNG data.
        /// If originalWof is provided, animation frames and header fields
        /// that we cannot reconstruct are preserved from the original.
        /// </summary>
        public static byte[] Encode(
            string objText,
            string mtlText,
            byte[] texturePng,       // RGBA PNG, 256 wide
            byte[] palData,          // PAL file for colour quantisation
            byte[]? originalWof = null)
        {
            // ── 1. Parse OBJ ─────────────────────────────────────────────────
            var (pieces, globalVts, globalUvs) = ParseObj(objText);
            if (pieces.Count == 0)
                throw new InvalidDataException("OBJ contains no objects.");

            // ── 2. Build material table from MTL + actual UV ranges in OBJ ─────
            var (texBytes, texHeight) = QuantiseTexture(texturePng, palData);
            var matTable = RecoverMaterialTable(mtlText, pieces, texHeight);

            // ── 3. Build face and vertex binary data ──────────────────────────
            int pc = pieces.Count;
            var faceBlocks = new List<byte[]>(pc);   // per-piece face bytes
            var vertBlocks = new List<byte[]>(pc);   // per-piece vert bytes (int16 x3)
            var pivots = new List<(short x, short y, short z)>(pc);

            foreach (var piece in pieces)
            {
                // Centroid as pivot (world space -> game space)
                float cx = piece.Verts.Average(v => v.x);
                float cy = piece.Verts.Average(v => v.y);
                float cz = piece.Verts.Average(v => v.z);

                // Game pivot: x=cx*scale, y=-cy*scale (undo Y-negate), z=cz*scale
                short px = Clamp16(cx * Scale);
                short py = Clamp16(-cy * Scale);
                short pz = Clamp16(cz * Scale);
                pivots.Add((px, py, pz));

                // Verts relative to pivot (undo world transform)
                var vb = new byte[piece.Verts.Count * 6];
                for (int i = 0; i < piece.Verts.Count; i++)
                {
                    var (vx, vy, vz) = piece.Verts[i];
                    short rvx = Clamp16(vx * Scale - px);
                    short rvy = Clamp16(-vy * Scale - py);  // undo Y-negate
                    short rvz = Clamp16(vz * Scale - pz);
                    int off = i * 6;
                    WriteInt16LE(vb, off, rvx);
                    WriteInt16LE(vb, off + 2, rvy);
                    WriteInt16LE(vb, off + 4, rvz);
                }
                vertBlocks.Add(vb);

                // Faces: v0,v1,v2,matId, u0,v0,u1,v1,u2,v2
                var fb = new byte[piece.Faces.Count * 10];
                for (int i = 0; i < piece.Faces.Count; i++)
                {
                    var f = piece.Faces[i];
                    int off = i * 10;
                    fb[off] = (byte)f.v0;
                    fb[off + 1] = (byte)f.v1;
                    fb[off + 2] = (byte)f.v2;
                    fb[off + 3] = (byte)f.mat;

                    // Un-normalise UVs: face_u = (norm_u * texWidth) - mat_uOff
                    var (uOff, vOff, _) = matTable.TryGetValue(f.mat, out var mt) ? mt : (0, 0, false);
                    fb[off + 4] = UvToByte(f.u0, WofDecoder.TexWidth, uOff);
                    fb[off + 5] = UvToByte(1f - f.v0uv, texHeight, vOff);
                    fb[off + 6] = UvToByte(f.u1, WofDecoder.TexWidth, uOff);
                    fb[off + 7] = UvToByte(1f - f.v1uv, texHeight, vOff);
                    fb[off + 8] = UvToByte(f.u2, WofDecoder.TexWidth, uOff);
                    fb[off + 9] = UvToByte(1f - f.v2uv, texHeight, vOff);
                }
                faceBlocks.Add(fb);
            }

            // ── 5. Compute section offsets ────────────────────────────────────
            int faceSecSize = faceBlocks.Sum(b => b.Length);
            int vertSecSize = vertBlocks.Sum(b => b.Length);

            int pieceTblOff = HeaderSize;
            int faceSecOff = pieceTblOff + pc * PieceRecSize;
            int vertSecOff = faceSecOff + faceSecSize;
            int matOff = vertSecOff + vertSecSize;
            int texOff = matOff + MaterialCount * MaterialStride;
            int endOff = texOff + texBytes.Length;

            int totalFaces = pieces.Sum(p => p.Faces.Count);
            int totalVerts = pieces.Sum(p => p.Verts.Count);

            // ── 6. Assemble output buffer ─────────────────────────────────────
            var buf = new byte[endOff];

            // Header
            WriteUInt16LE(buf, 0x00, (ushort)pc);
            WriteUInt16LE(buf, 0x02, 0);
            WriteUInt32LE(buf, 0x04, (uint)totalFaces);
            WriteUInt32LE(buf, 0x08, (uint)totalVerts);
            WriteUInt32LE(buf, 0x0C, (uint)vertSecOff);
            WriteUInt32LE(buf, 0x10, 1);               // 1 anim frame (static)
            // [0x14] h14 - copy from original if available, else 0
            WriteUInt32LE(buf, 0x14, originalWof?.Length >= 0x18
                ? BitConverter.ToUInt32(originalWof, 0x14) : 0u);
            WriteUInt32LE(buf, 0x18, (uint)pc);
            WriteUInt32LE(buf, 0x1C, 0);
            WriteUInt32LE(buf, 0x20, (uint)(vertSecOff + vertSecSize));  // = matOff
            WriteUInt32LE(buf, 0x24, (uint)matOff);
            WriteUInt32LE(buf, 0x28, (uint)texOff);
            WriteUInt32LE(buf, 0x2C, (uint)endOff);

            // Piece records
            int curFaceOff = faceSecOff;
            int curVertOff = 0;  // byte offset within vert section
            for (int p = 0; p < pc; p++)
            {
                int recOff = pieceTblOff + p * PieceRecSize;
                var piece = pieces[p];
                var (px, py, pz) = pivots[p];

                // Name (16 bytes, null-padded)
                var nameBytes = Encoding.ASCII.GetBytes(piece.Name.Length > 15
                    ? piece.Name[..15] : piece.Name);
                Array.Copy(nameBytes, 0, buf, recOff, nameBytes.Length);

                buf[recOff + 0x10] = 0;                             // flags
                buf[recOff + 0x11] = (byte)piece.Verts.Count;
                buf[recOff + 0x12] = (byte)piece.Faces.Count;
                WriteInt16LE(buf, recOff + 0x13, px);
                WriteInt16LE(buf, recOff + 0x15, py);
                WriteInt16LE(buf, recOff + 0x17, pz);
                WriteInt32LE(buf, recOff + 0x19, curVertOff);
                WriteUInt32LE(buf, recOff + 0x1C, (uint)(curFaceOff << 8));

                // BSP children: all -1 (static, no tree)
                for (int ci = 0; ci < 16; ci++)
                    WriteInt32LE(buf, recOff + 0x21 + ci * 4, -1);

                // Fill remaining bytes to 0xFF (rec is 97 bytes, used up to 0x61=97)
                // 0x21 + 16*4 = 0x61 = exactly 97, nothing to pad

                curFaceOff += faceBlocks[p].Length;
                curVertOff += vertBlocks[p].Length;
            }

            // Face data
            int pos = faceSecOff;
            foreach (var fb in faceBlocks) { Array.Copy(fb, 0, buf, pos, fb.Length); pos += fb.Length; }

            // Vertex data
            pos = vertSecOff;
            foreach (var vb in vertBlocks) { Array.Copy(vb, 0, buf, pos, vb.Length); pos += vb.Length; }

            // Material table (13 * 4 bytes)
            pos = matOff;
            for (int m = 0; m < MaterialCount; m++)
            {
                if (matTable.TryGetValue(m, out var mt))
                {
                    buf[pos] = (byte)mt.uOff;
                    buf[pos + 1] = (byte)mt.vOff;
                    buf[pos + 2] = 0;
                    buf[pos + 3] = mt.useTex ? (byte)0x80 : (byte)0x00;
                }
                // else: 4 zero bytes (already zeroed)
                pos += 4;
            }

            // Texture
            Array.Copy(texBytes, 0, buf, texOff, texBytes.Length);

            // ── 7. If original provided, splice back animation vert frames ────
            if (originalWof != null)
                SpliceAnimationData(buf, originalWof, vertSecOff, vertSecSize, pc);

            return buf;
        }

        // ── OBJ parser ────────────────────────────────────────────────────────

        private static (List<ObjPiece> pieces,
                        List<(float x, float y, float z)> verts,
                        List<(float u, float v)> uvs)
            ParseObj(string text)
        {
            var globalVerts = new List<(float x, float y, float z)>();
            var globalUvs = new List<(float u, float v)>();
            var pieces = new List<ObjPiece>();
            ObjPiece? cur = null;
            int curMat = 0;

            // Track per-piece local vertex indices (OBJ uses global 1-based indices)
            var globalToLocal = new Dictionary<int, int>();

            foreach (var rawLine in text.Split('\n'))
            {
                var line = rawLine.Trim();
                if (line.StartsWith("v ") && !line.StartsWith("vt") && !line.StartsWith("vn"))
                {
                    var p = ParseFloats(line[2..]);
                    globalVerts.Add((p[0], p[1], p[2]));
                }
                else if (line.StartsWith("vt "))
                {
                    var p = ParseFloats(line[3..]);
                    globalUvs.Add((p[0], p.Length > 1 ? p[1] : 0f));
                }
                else if (line.StartsWith("o "))
                {
                    cur = new ObjPiece { Name = line[2..].Trim() };
                    pieces.Add(cur);
                    globalToLocal.Clear();
                    curMat = 0;
                }
                else if (line.StartsWith("usemtl "))
                {
                    var matName = line[7..].Trim();
                    // mat_N -> N
                    if (matName.StartsWith("mat_") && int.TryParse(matName[4..], out int mid))
                        curMat = mid;
                }
                else if (line.StartsWith("f ") && cur != null)
                {
                    var tokens = line[2..].Trim().Split(' ', StringSplitOptions.RemoveEmptyEntries);
                    if (tokens.Length < 3) continue;

                    int[] vi = new int[3];
                    int[] uvi = new int[3];
                    for (int i = 0; i < 3; i++)
                    {
                        var parts = tokens[i].Split('/');
                        vi[i] = int.Parse(parts[0]) - 1;  // 0-based global
                        uvi[i] = parts.Length > 1 && int.TryParse(parts[1], out int u) ? u - 1 : 0;
                    }

                    // Map global vert indices to local piece-relative indices
                    int[] li = new int[3];
                    for (int i = 0; i < 3; i++)
                    {
                        if (!globalToLocal.TryGetValue(vi[i], out int loc))
                        {
                            loc = cur.Verts.Count;
                            globalToLocal[vi[i]] = loc;
                            var gv = globalVerts[vi[i]];
                            cur.Verts.Add(gv);
                        }
                        li[i] = loc;
                    }

                    var (u0, vv0) = uvi[0] < globalUvs.Count ? globalUvs[uvi[0]] : (0f, 0f);
                    var (u1, vv1) = uvi[1] < globalUvs.Count ? globalUvs[uvi[1]] : (0f, 0f);
                    var (u2, vv2) = uvi[2] < globalUvs.Count ? globalUvs[uvi[2]] : (0f, 0f);

                    cur.Faces.Add((li[0], li[1], li[2], curMat,
                                   u0, vv0, u1, vv1, u2, vv2));
                }
            }

            return (pieces, globalVerts, globalUvs);
        }

        // ── MTL parser + UV offset recovery ──────────────────────────────────

        // Parse MTL for useTex flag per material.
        // u/v offsets are recovered from the minimum normalised UV seen per material
        // across all faces in the OBJ — min(atlas_u) = mat_uOff, min(atlas_v) = mat_vOff.
        private static Dictionary<int, (int uOff, int vOff, bool useTex)>
            RecoverMaterialTable(string mtlText, List<ObjPiece> pieces, int texHeight)
        {
            // Step 1: which materials have map_Kd (textured)?
            var usesTexture = new HashSet<int>();
            int curMtl = -1;
            foreach (var rawLine in mtlText.Split('\n'))
            {
                var line = rawLine.Trim();
                if (line.StartsWith("newmtl mat_") && int.TryParse(line[11..], out int mid))
                    curMtl = mid;
                else if (line.StartsWith("map_Kd") && curMtl >= 0)
                    usesTexture.Add(curMtl);
            }

            // Step 2: find min normalised UV per material across all faces
            // OBJ vt: u is 0-1 across TexWidth, v is (1-v) flipped for top-down atlas
            // atlas_u = norm_u * TexWidth,  atlas_v = (1 - norm_v) * texHeight
            var minU = new Dictionary<int, float>();
            var minV = new Dictionary<int, float>();
            foreach (var piece in pieces)
                foreach (var f in piece.Faces)
                {
                    foreach (var (nu, nv) in new[] {
                    (f.u0, f.v0uv), (f.u1, f.v1uv), (f.u2, f.v2uv) })
                    {
                        if (!minU.ContainsKey(f.mat) || nu < minU[f.mat]) minU[f.mat] = nu;
                        if (!minV.ContainsKey(f.mat) || nv < minV[f.mat]) minV[f.mat] = nv;
                    }
                }

            var result = new Dictionary<int, (int uOff, int vOff, bool useTex)>();
            var allMats = usesTexture.Concat(minU.Keys).Concat(minV.Keys).Distinct();
            foreach (int m in allMats)
            {
                // Convert min normalised UV back to pixel offset in atlas
                float nu = minU.TryGetValue(m, out float u) ? u : 0f;
                float nv = minV.TryGetValue(m, out float v) ? v : 0f;
                // vt in OBJ is already (1 - atlas_v/texHeight) so atlas_v = (1-nv)*texHeight
                int uOff = Math.Clamp((int)MathF.Round(nu * WofDecoder.TexWidth), 0, 255);
                int vOff = Math.Clamp((int)MathF.Round((1f - nv) * texHeight), 0, 255);
                // Snap to nearest multiple of 4 (material offsets are always 4-aligned)
                uOff = (uOff / 4) * 4;
                vOff = (vOff / 4) * 4;
                result[m] = (uOff, vOff, usesTexture.Contains(m));
            }
            return result;
        }

        // ── Texture quantisation ──────────────────────────────────────────────

        private static (byte[] data, int height) QuantiseTexture(byte[] pngBytes, byte[] palData)
        {
            using var ms = new System.IO.MemoryStream(pngBytes);
            using var bmp = new System.Drawing.Bitmap(ms);

            int w = bmp.Width, h = bmp.Height;
            if (w != WofDecoder.TexWidth)
                throw new InvalidDataException($"Texture must be {WofDecoder.TexWidth}px wide, got {w}.");

            // Build palette lookup: each PAL entry is RGB (6-bit * 4 = 0-252)
            // We convert to full 8-bit (multiply by 4 then clamp) for matching
            var palRgb = new (int r, int g, int b)[256];
            for (int i = 0; i < 256; i++)
                palRgb[i] = (Math.Min(palData[i * 3] * 4, 255),
                             Math.Min(palData[i * 3 + 1] * 4, 255),
                             Math.Min(palData[i * 3 + 2] * 4, 255));

            var result = new byte[w * h];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    var px = bmp.GetPixel(x, y);
                    if (px.A < 128) { result[y * w + x] = 0; continue; }  // transparent → index 0

                    // Find nearest palette entry (squared Euclidean distance)
                    int best = 1, bestDist = int.MaxValue;
                    for (int i = 1; i < 256; i++)
                    {
                        var (pr, pg, pb) = palRgb[i];
                        int d = (px.R - pr) * (px.R - pr) + (px.G - pg) * (px.G - pg) + (px.B - pb) * (px.B - pb);
                        if (d < bestDist) { bestDist = d; best = i; if (d == 0) break; }
                    }
                    result[y * w + x] = (byte)best;
                }

            return (result, h);
        }

        // ── Animation splice ──────────────────────────────────────────────────

        // Copy extra animation vertex frames from original WOF into our re-encoded buffer.
        // TODO: for full animation support, grow newBuf and splice anim frames here.
        private static void SpliceAnimationData(byte[] newBuf, byte[] orig,
            int vertSecOff, int baseVertSize, int pc)
        {
            if (orig.Length < 0x18) return;
            int origVertOff = (int)BitConverter.ToUInt32(orig, 0x0C);
            int origMatOff = (int)BitConverter.ToUInt32(orig, 0x24);
            int origAnimSize = origMatOff - origVertOff - baseVertSize;
            if (origAnimSize <= 0) return;
            _ = newBuf; _ = vertSecOff; _ = pc;  // suppress unused warnings until implemented
        }

        // ── UV helper ─────────────────────────────────────────────────────────

        // Convert normalised UV back to byte offset within material region.
        // normUv = (matOffset + byteOffset) / dimension  ->  byteOffset = normUv*dim - matOffset
        private static byte UvToByte(float normUv, int dimension, int matOffset)
        {
            int pixel = (int)MathF.Round(normUv * dimension) - matOffset;
            return (byte)Math.Clamp(pixel, 0, 255);
        }

        // ── Binary helpers ────────────────────────────────────────────────────

        private static void WriteUInt16LE(byte[] b, int off, ushort v)
        { b[off] = (byte)v; b[off + 1] = (byte)(v >> 8); }

        private static void WriteInt16LE(byte[] b, int off, short v)
        { b[off] = (byte)v; b[off + 1] = (byte)(v >> 8); }

        private static void WriteInt32LE(byte[] b, int off, int v)
        { b[off] = (byte)v; b[off + 1] = (byte)(v >> 8); b[off + 2] = (byte)(v >> 16); b[off + 3] = (byte)(v >> 24); }

        private static void WriteUInt32LE(byte[] b, int off, uint v)
        { b[off] = (byte)v; b[off + 1] = (byte)(v >> 8); b[off + 2] = (byte)(v >> 16); b[off + 3] = (byte)(v >> 24); }

        private static short Clamp16(float v)
            => (short)Math.Clamp((int)MathF.Round(v), short.MinValue, short.MaxValue);

        private static float[] ParseFloats(string s)
            => s.Trim().Split(' ', StringSplitOptions.RemoveEmptyEntries)
                .Select(t => float.Parse(t, System.Globalization.CultureInfo.InvariantCulture))
                .ToArray();
    }
}