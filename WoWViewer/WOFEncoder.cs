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
        ///
        /// ROUND-TRIP CONSTRAINTS (when originalWof != null):
        ///   - Piece count must match original exactly.
        ///   - Per-piece vert counts must match original exactly.
        ///   - If either constraint is violated, encoding is aborted with an exception.
        ///   - Pivots, BSP children, flags and animation blob are all copied verbatim from original.
        ///   - Only face data, vertex positions within each piece, and the texture atlas are replaced.
        /// </summary>
        public static byte[] Encode(
            string objText,
            string mtlText,
            byte[] texturePng,       // RGBA PNG, 256 wide
            byte[] palData,          // PAL file for colour quantisation
            byte[]? originalWof = null)
        {
            // ── 0. Parse original WOF for reference data ──────────────────────
            WofModel? orig = null;
            if (originalWof != null && originalWof.Length >= 0x30)
                orig = WofDecoder.Parse(originalWof);

            // ── 1. Parse OBJ ─────────────────────────────────────────────────
            var (pieces, globalVts, globalUvs) = ParseObj(objText);
            if (pieces.Count == 0)
                throw new InvalidDataException("OBJ contains no objects.");

            int pc = pieces.Count;

            // ── 1a. Round-trip validation ─────────────────────────────────────
            // We do NOT validate vert counts here — the OBJ deduplication means a piece
            // can have fewer OBJ verts than the original WOF (orphan/duplicate positions
            // in the original are collapsed). Vert count is reconciled in step 3 below.
            if (orig != null && orig.PieceCount != pc)
                throw new InvalidDataException(
                    $"Round-trip error: OBJ has {pc} objects but original WOF has {orig.PieceCount} pieces. " +
                    "Piece count must match exactly for animated models.");

            // ── 2. Build material table from MTL + actual UV ranges in OBJ ─────
            var (texBytes, texHeight) = QuantiseTexture(texturePng, palData);
            var matTable = RecoverMaterialTable(mtlText, pieces, texHeight);

            // ── 3. Build face and vertex binary data ──────────────────────────
            var faceBlocks = new List<byte[]>(pc);   // per-piece face bytes
            var vertBlocks = new List<byte[]>(pc);   // per-piece vert bytes (int16 x3)

            for (int p = 0; p < pc; p++)
            {
                var piece = pieces[p];

                // Pivot: for round-trip use original pivot values (they're relative to BSP
                // parent chain and govern where animation data places the piece).
                // For new models (no original) compute centroid as before.
                short px, py, pz;
                if (orig != null)
                {
                    px = orig.Pieces[p].PivotX;
                    py = orig.Pieces[p].PivotY;
                    pz = orig.Pieces[p].PivotZ;
                }
                else
                {
                    float cx = piece.Verts.Average(v => v.x);
                    float cy = piece.Verts.Average(v => v.y);
                    float cz = piece.Verts.Average(v => v.z);
                    px = Clamp16(cx * Scale);
                    py = Clamp16(-cy * Scale);
                    pz = Clamp16(cz * Scale);
                }

                // For round-trip: accumulate pivots up parent chain to get world offset
                // so we can express OBJ world-space verts back as raw game-space values.
                int accumX = px, accumY = py, accumZ = pz;
                if (orig != null)
                {
                    accumX = orig.Pieces[p].RawAccumX;
                    accumY = orig.Pieces[p].RawAccumY;
                    accumZ = orig.Pieces[p].RawAccumZ;
                }

                // Verts: OBJ world-space → raw game-space
                // world_x = (accumX + vx) / 100  →  vx = world_x * 100 - accumX
                // world_y = -(accumY + vy) / 100  →  vy = -world_y * 100 - accumY
                //
                // Round-trip vert count reconciliation:
                //   The OBJ may have fewer verts than the original because unreferenced
                //   (orphan) verts in the WOF are not emitted by the exporter, and shared
                //   verts are collapsed to a single OBJ `v` line.
                //   Strategy: fill slots 0..ObjVertCount-1 from OBJ, then fill any remaining
                //   slots up to origVertCount from the original WOF raw vert data.
                //   This preserves the original vert indices that the animation blob references.
                int origVertCount = orig?.Pieces[p].VertCount ?? piece.Verts.Count;
                int finalVertCount = (orig != null) ? origVertCount : piece.Verts.Count;

                var vb = new byte[finalVertCount * 6];
                // Fill from OBJ (up to however many the OBJ has, capped at finalVertCount)
                int objVertCount = Math.Min(piece.Verts.Count, finalVertCount);
                for (int i = 0; i < objVertCount; i++)
                {
                    var (wx, wy, wz) = piece.Verts[i];
                    short rvx = Clamp16(wx * Scale - accumX);
                    short rvy = Clamp16(-wy * Scale - accumY);
                    short rvz = Clamp16(wz * Scale - accumZ);
                    int off = i * 6;
                    WriteInt16LE(vb, off, rvx);
                    WriteInt16LE(vb, off + 2, rvy);
                    WriteInt16LE(vb, off + 4, rvz);
                }
                // Pad any remaining slots from original vert data (orphan verts)
                if (orig != null && objVertCount < finalVertCount)
                {
                    var op = orig.Pieces[p];
                    for (int i = objVertCount; i < finalVertCount; i++)
                    {
                        int srcOff = orig.VertOffset + op.VertByteOff + i * 6;
                        int dstOff = i * 6;
                        if (srcOff + 6 <= originalWof!.Length)
                            Array.Copy(originalWof, srcOff, vb, dstOff, 6);
                        // else leave as zero — shouldn't happen for valid files
                    }
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
                    // For round-trips: read uOff/vOff directly from the original material table.
                    // RecoverMaterialTable derives offsets from min/max UV values in the OBJ,
                    // which gives uOff+minFaceU (not uOff) — always wrong unless minFaceU==0.
                    int uOff, vOff;
                    if (orig != null && orig.MaterialData.Length >= (f.mat * 4 + 2))
                    {
                        uOff = orig.MaterialData[f.mat * 4];
                        vOff = orig.MaterialData[f.mat * 4 + 1];
                    }
                    else
                    {
                        var (ru, rv, _) = matTable.TryGetValue(f.mat, out var mt) ? mt : (0, 0, false);
                        uOff = ru; vOff = rv;
                    }
                    fb[off + 4] = UvToByte(f.u0, WofDecoder.TexWidth, uOff);
                    fb[off + 5] = UvToByte(1f - f.v0uv, texHeight, vOff);
                    fb[off + 6] = UvToByte(f.u1, WofDecoder.TexWidth, uOff);
                    fb[off + 7] = UvToByte(1f - f.v1uv, texHeight, vOff);
                    fb[off + 8] = UvToByte(f.u2, WofDecoder.TexWidth, uOff);
                    fb[off + 9] = UvToByte(1f - f.v2uv, texHeight, vOff);
                }
                faceBlocks.Add(fb);
            }

            // ── 4. Animation blob ─────────────────────────────────────────────
            // For round-trip: preserve verbatim. For new models: empty (static).
            byte[] animBlob = orig?.AnimBlob ?? [];

            // ── 5. Compute section offsets ────────────────────────────────────
            int faceSecSize = faceBlocks.Sum(b => b.Length);
            int baseVertSize = vertBlocks.Sum(b => b.Length);

            // Material count: use original's actual count (from toff-moff), not the hardcoded 13.
            // Original may have 12 entries; writing 13 shifts tex_off and breaks the file.
            int matCount = orig != null
                ? (orig.TexOffset - orig.MatOffset) / MaterialStride
                : MaterialCount;
            // Clamp to [1..13] for safety
            matCount = Math.Clamp(matCount, 1, MaterialCount);

            int pieceTblOff = HeaderSize;
            int faceSecOff = pieceTblOff + pc * PieceRecSize;
            int vertSecOff = faceSecOff + faceSecSize;
            int h20 = vertSecOff + baseVertSize;        // end of base-frame vert data
            int matOff = h20 + animBlob.Length;         // animation blob sits between h20 and mat_off
            int texOff = matOff + matCount * MaterialStride;
            int endOff = texOff + texBytes.Length;

            int totalFaces = pieces.Sum(p => p.Faces.Count);
            // totalVerts must reflect reconciled vert counts (original counts for round-trip,
            // OBJ vert counts for new models), not just what the OBJ parser saw.
            int totalVerts = orig != null
                ? orig.Pieces.Sum(p => (int)p.VertCount)
                : pieces.Sum(p => p.Verts.Count);

            // ── 6. Assemble output buffer ─────────────────────────────────────
            var buf = new byte[endOff];

            // Header — copy animation-related fields from original if available
            WriteUInt16LE(buf, 0x00, (ushort)pc);
            WriteUInt16LE(buf, 0x02, 0);
            WriteUInt32LE(buf, 0x04, (uint)totalFaces);
            WriteUInt32LE(buf, 0x08, (uint)totalVerts);
            WriteUInt32LE(buf, 0x0C, (uint)vertSecOff);
            WriteUInt32LE(buf, 0x10, orig?.H10 ?? 0u);   // animation frame count — preserve
            WriteUInt32LE(buf, 0x14, orig?.H14 ?? 0u);   // unknown — preserve
            WriteUInt32LE(buf, 0x18, orig?.H18 ?? 0u);   // animated piece count — preserve
            WriteUInt32LE(buf, 0x1C, orig?.H1C ?? 0u);   // animation count — preserve
            WriteUInt32LE(buf, 0x20, (uint)h20);          // end of base-frame vert data
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

                // For round-trip: copy pivot, flags, BSP children from original.
                // For new models: compute from geometry (already done above).
                short px, py, pz;
                byte flags;
                int[] bspChildren;
                if (orig != null)
                {
                    var op = orig.Pieces[p];
                    px = op.PivotX; py = op.PivotY; pz = op.PivotZ;
                    flags = op.Flags;
                    bspChildren = op.BspChildren;
                }
                else
                {
                    // Pivot was computed in step 3 but not stored — recompute from verts.
                    float cx = piece.Verts.Average(v => v.x);
                    float cy = piece.Verts.Average(v => v.y);
                    float cz = piece.Verts.Average(v => v.z);
                    px = Clamp16(cx * Scale);
                    py = Clamp16(-cy * Scale);
                    pz = Clamp16(cz * Scale);
                    flags = 0;
                    bspChildren = [];
                }

                // Name (16 bytes, null-padded)
                var nameBytes = Encoding.ASCII.GetBytes(piece.Name.Length > 15
                    ? piece.Name[..15] : piece.Name);
                Array.Copy(nameBytes, 0, buf, recOff, nameBytes.Length);

                buf[recOff + 0x10] = flags;
                buf[recOff + 0x11] = (byte)(orig != null ? orig.Pieces[p].VertCount : piece.Verts.Count);
                buf[recOff + 0x12] = (byte)piece.Faces.Count;
                WriteInt16LE(buf, recOff + 0x13, px);
                WriteInt16LE(buf, recOff + 0x15, py);
                WriteInt16LE(buf, recOff + 0x17, pz);
                WriteInt32LE(buf, recOff + 0x19, curVertOff);
                WriteUInt32LE(buf, recOff + 0x1C, (uint)(curFaceOff << 8));
                buf[recOff + 0x20] = 0;

                // BSP children (up to 16 int32s starting at 0x21, terminated by -1)
                for (int ci = 0; ci < 16; ci++)
                {
                    int child = ci < bspChildren.Length ? bspChildren[ci] : -1;
                    WriteInt32LE(buf, recOff + 0x21 + ci * 4, child);
                }

                curFaceOff += faceBlocks[p].Length;
                curVertOff += vertBlocks[p].Length;
            }

            // Face data
            int pos = faceSecOff;
            foreach (var fb in faceBlocks) { Array.Copy(fb, 0, buf, pos, fb.Length); pos += fb.Length; }

            // Base-frame vertex data
            pos = vertSecOff;
            foreach (var vb in vertBlocks) { Array.Copy(vb, 0, buf, pos, vb.Length); pos += vb.Length; }

            // Animation blob (verbatim from original, or empty for new models)
            if (animBlob.Length > 0)
                Array.Copy(animBlob, 0, buf, h20, animBlob.Length);

            // Material table
            pos = matOff;
            if (orig?.MaterialData.Length >= matCount * MaterialStride)
            {
                // Round-trip: copy original material table verbatim — byte-perfect.
                // This preserves uOff, vOff, use_tex flags, and the unused byte[2].
                // useTex cannot be reliably recovered from the OBJ/MTL because our exporter
                // writes map_Kd for ALL materials (they all share the atlas PNG).
                Array.Copy(orig.MaterialData, 0, buf, pos, matCount * MaterialStride);
            }
            else
            {
                // New model: write matCount entries from recovered table
                for (int m = 0; m < matCount; m++)
                {
                    if (matTable.TryGetValue(m, out var mt))
                    {
                        buf[pos] = (byte)mt.uOff;
                        buf[pos + 1] = (byte)mt.vOff;
                        buf[pos + 2] = 0;
                        buf[pos + 3] = mt.useTex ? (byte)0x80 : (byte)0x00;
                    }
                    pos += MaterialStride;
                }
            }

            // Texture
            Array.Copy(texBytes, 0, buf, texOff, texBytes.Length);

            // ── Skinning table ────────────────────────────────────────────────
            // Appended verbatim after end_off (outside the header-described region).
            // The engine reads it at data+end_off immediately after loading.
            // Format (fully decoded):
            //   [0x00]          uint32 = 0                sentinel
            //   [0x04]          animPc × uint32           absolute file ptrs, one per animated piece
            //   [0x04+animPc*4] animPc variable blocks    one per animated piece
            //
            // Per-block: [N] then N records of (weight, shade_id_0..shade_id_{weight-1}, vert_idx)
            //   weight=1 for flat shading (all verts → shade_group 0).
            //
            // Strategy:
            //   - If vc unchanged for a piece: copy original block verbatim (preserves smooth shading).
            //   - If vc changed (or no original): generate a flat-shading block (weight=1, shade=0).
            //   - The pointer array is always rebuilt with the correct absolute offsets.
            byte[] skinningTable = BuildSkinningTable(buf.Length, pieces, orig);
            if (skinningTable.Length > 0)
            {
                var withSkin = new byte[buf.Length + skinningTable.Length];
                Array.Copy(buf, withSkin, buf.Length);
                Array.Copy(skinningTable, 0, withSkin, buf.Length, skinningTable.Length);
                return withSkin;
            }

            return buf;
        }

        // ── Skinning table builder ────────────────────────────────────────────

        /// <summary>
        /// Build the skinning table that is appended after end_off.
        ///
        /// Format:
        ///   uint32 = 0                               sentinel
        ///   animPc × uint32                          absolute file pointers
        ///   animPc × variable block                  per animated piece
        ///
        /// Per-block:
        ///   byte  N            = number of vert records (= vc for flat encoding)
        ///   N × record:
        ///     byte  weight     = number of shade groups influencing this vert (always 1 here)
        ///     byte  shade_id   = shade group id (0 = flat)
        ///     byte  vert_idx   = local piece vert index
        ///
        /// If the original WOF has a skinning table and the vert count for a piece is
        /// unchanged, the original block is preserved verbatim (smooth shading).
        /// Otherwise a flat-shading block is generated.
        /// </summary>
        private static byte[] BuildSkinningTable(int fileBase, List<ObjPiece> pieces, WofModel? orig)
        {
            // Identify animated pieces (flags & 0x20) from orig if available,
            // otherwise treat all pieces as animated (safe fallback).
            // We need flags and orig vc to decide whether to copy or regenerate.
            int pc = pieces.Count;

            // Build a parallel list of (flags, origVc, origBlock?) for each piece
            var pieceInfos = new List<(byte flags, int origVc, byte[]? origBlock)>(pc);
            for (int p = 0; p < pc; p++)
            {
                byte flags = 0;
                int origVc = pieces[p].Verts.Count;
                byte[]? origBlock = null;

                if (orig != null && p < orig.PieceCount)
                {
                    flags = orig.Pieces[p].Flags;
                    origVc = orig.Pieces[p].VertCount;
                }
                pieceInfos.Add((flags, origVc, origBlock));
            }

            // Parse the original skinning table blocks if we have one
            Dictionary<int, byte[]> origBlocks = [];   // piece_index → raw block bytes
            if (orig?.SkinningTable.Length > 4)
            {
                var st = orig.SkinningTable;
                // st[0..3] = uint32 sentinel (0)
                // count the pointers: they all point into st itself
                int animPcOrig = 0;
                int stBase = orig.TexOffset + (orig.TexHeight * WofDecoder.TexWidth); // = end_off
                for (int i = 0; i < 64 && 4 + (i + 1) * 4 <= st.Length; i++)
                {
                    uint ptr = BitConverter.ToUInt32(st, 4 + i * 4);
                    if (ptr < stBase || ptr >= stBase + st.Length) break;
                    animPcOrig++;
                }

                // Now extract each block (span between consecutive pointers)
                int animIdx = 0;
                for (int p = 0; p < orig.PieceCount && animIdx < animPcOrig; p++)
                {
                    if ((orig.Pieces[p].Flags & 0x20) == 0) continue;

                    int ptrOff = 4 + animIdx * 4;
                    uint ptr = BitConverter.ToUInt32(st, ptrOff);
                    uint nextPtr = (animIdx + 1 < animPcOrig)
                        ? BitConverter.ToUInt32(st, ptrOff + 4)
                        : (uint)(stBase + st.Length);

                    int blockOff = (int)(ptr - stBase);
                    int blockEnd = (int)(nextPtr - stBase);
                    if (blockOff >= 0 && blockEnd <= st.Length && blockEnd > blockOff)
                    {
                        origBlocks[p] = st[blockOff..blockEnd];
                    }
                    animIdx++;
                }
            }

            // Collect animated pieces and build their blocks
            var animPieces = new List<int>();   // piece indices that are animated
            var blocks = new List<byte[]>();

            for (int p = 0; p < pc; p++)
            {
                var (flags, origVc, _) = pieceInfos[p];
                bool isAnimated = orig != null
                    ? (orig.Pieces[p].Flags & 0x20) != 0
                    : true;  // if no original, treat all as animated (safe)
                if (!isAnimated) continue;

                int newVc = p < pieces.Count ? pieces[p].Verts.Count : origVc;
                // Use reconciled vc (orig takes priority for round-trips)
                int finalVc = orig != null ? origVc : newVc;

                byte[] block;
                if (origBlocks.TryGetValue(p, out var ob) &&
                    ob.Length == 1 + ob[0] + finalVc * 2)
                {
                    // Vert count unchanged — copy original block verbatim
                    block = ob;
                }
                else
                {
                    // Vert count changed or no original — generate flat-shading block:
                    // N=finalVc, each vert: (weight=1, shade=0, vert_idx)
                    block = GenerateFlatSkinningBlock(finalVc);
                }

                animPieces.Add(p);
                blocks.Add(block);
            }

            if (animPieces.Count == 0) return [];

            // Compute absolute file pointers for each block
            // Layout in the skinning table:
            //   [0]              uint32 sentinel = 0
            //   [4..4+apc*4-1]   apc × uint32 pointers
            //   [4+apc*4...]     blocks sequentially
            int apc = animPieces.Count;
            int headerSize = 4 + apc * 4;   // sentinel + pointer array
            int tableBase = fileBase;       // absolute position of skinning table in file

            int[] blockOffsets = new int[apc];
            int cursor = headerSize;
            for (int i = 0; i < apc; i++)
            {
                blockOffsets[i] = cursor;
                cursor += blocks[i].Length;
            }
            int totalSize = cursor;

            var table = new byte[totalSize];
            // Sentinel
            // (already zero)

            // Pointer array
            for (int i = 0; i < apc; i++)
            {
                uint absPtr = (uint)(tableBase + blockOffsets[i]);
                table[4 + i * 4] = (byte)absPtr;
                table[4 + i * 4 + 1] = (byte)(absPtr >> 8);
                table[4 + i * 4 + 2] = (byte)(absPtr >> 16);
                table[4 + i * 4 + 3] = (byte)(absPtr >> 24);
            }

            // Blocks
            for (int i = 0; i < apc; i++)
                Array.Copy(blocks[i], 0, table, blockOffsets[i], blocks[i].Length);

            return table;
        }

        /// <summary>
        /// Generate a flat-shading skinning block for a piece with vc vertices.
        /// All verts → shade_group 0, weight 1.
        /// Format: [N=vc] + [1, 0, 0, 1, 0, 1, ..., 1, 0, vc-1]
        /// </summary>
        private static byte[] GenerateFlatSkinningBlock(int vc)
        {
            var block = new byte[1 + vc * 3];
            block[0] = (byte)vc;
            for (int v = 0; v < vc; v++)
            {
                block[1 + v * 3] = 1;       // weight = 1
                block[1 + v * 3 + 1] = 0;       // shade_group = 0
                block[1 + v * 3 + 2] = (byte)v; // local vert index
            }
            return block;
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
            int pieceVertBase = 0;  // global vert index (0-based) of this piece's vert[0]

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
                    // Record how many global verts existed before this piece — that's its base.
                    // All `v` lines for this piece immediately follow the `o` line in our export,
                    // so globalVerts.Count at this point is the 0-based global index of vert[0].
                    pieceVertBase = globalVerts.Count;
                    cur = new ObjPiece { Name = line[2..].Trim() };
                    pieces.Add(cur);
                    curMat = 0;
                }
                else if (line.StartsWith("usemtl "))
                {
                    var matName = line[7..].Trim();
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

                    // Convert global vert index → piece-local index by subtracting the piece's
                    // vert base. This preserves the exact original vert ordering (critical for
                    // animation: the anim blob references verts by their original local index).
                    // The WOF exporter writes each piece's verts sequentially to the OBJ in
                    // order vert[0]..vert[N-1], so global[pieceVertBase + k] == local[k].
                    int[] li = new int[3];
                    for (int i = 0; i < 3; i++)
                        li[i] = vi[i] - pieceVertBase;

                    // Populate piece.Verts in canonical order on first pass through faces.
                    // We need all slots 0..max(li) to exist. Fill any gaps with the actual
                    // global vert data (orphan verts will be filled by padding from orig later).
                    int maxLocal = Math.Max(li[0], Math.Max(li[1], li[2]));
                    while (cur.Verts.Count <= maxLocal)
                    {
                        int gIdx = pieceVertBase + cur.Verts.Count;
                        cur.Verts.Add(gIdx < globalVerts.Count ? globalVerts[gIdx] : (0f, 0f, 0f));
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
        // u/v offsets are recovered from the UV range seen per material across all faces.
        //
        // OBJ vt convention (as written by WofDecoder.ToObj):
        //   vt_u = (matUOff + faceU) / TexWidth          → min vt_u = matUOff / TexWidth
        //   vt_v = 1 - (matVOff + faceV) / texHeight     → max vt_v = 1 - matVOff / texHeight
        //
        // Recovery:
        //   matUOff = round( min(vt_u)        * TexWidth  )
        //   matVOff = round( (1 - max(vt_v))  * texHeight )
        //
        // Note: the old code used min(normV) for vOff which gives vOff+maxFaceV (wrong).
        private static Dictionary<int, (int uOff, int vOff, bool useTex)>
            RecoverMaterialTable(string mtlText, List<ObjPiece> pieces, int texHeight)
        {
            // Step 1: which materials have map_Kd (textured)?
            // NOTE: our exporter writes map_Kd for ALL materials (they all share the atlas PNG).
            // use_tex flag cannot be reliably recovered from the MTL alone — it must come from
            // the original WOF's material table (handled by the caller for round-trips).
            // This function marks everything with map_Kd as use_tex=True; the caller overrides.
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

            // Step 2: find min vt_u and max vt_v per material across all faces
            var minU = new Dictionary<int, float>();
            var maxV = new Dictionary<int, float>();
            foreach (var piece in pieces)
                foreach (var f in piece.Faces)
                    foreach (var (nu, nv) in new[] { (f.u0, f.v0uv), (f.u1, f.v1uv), (f.u2, f.v2uv) })
                    {
                        if (!minU.ContainsKey(f.mat) || nu < minU[f.mat]) minU[f.mat] = nu;
                        if (!maxV.ContainsKey(f.mat) || nv > maxV[f.mat]) maxV[f.mat] = nv;
                    }

            var result = new Dictionary<int, (int uOff, int vOff, bool useTex)>();
            var allMats = usesTexture.Concat(minU.Keys).Concat(maxV.Keys).Distinct();
            foreach (int m in allMats)
            {
                float nu = minU.TryGetValue(m, out float u) ? u : 0f;
                float nv = maxV.TryGetValue(m, out float v) ? v : 1f;
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

            // Palette-indexed PNG (Format8bppIndexed / PixelFormat with indexed flag):
            // raw index bytes are preserved — read them directly without re-quantising.
            // This is the lossless round-trip path, used when the atlas was exported with
            // WofDecoder.ExportTextureRaw (the _raw.png file written alongside the display PNG).
            if (bmp.PixelFormat == System.Drawing.Imaging.PixelFormat.Format8bppIndexed)
            {
                var result = new byte[w * h];
                var bmpData = bmp.LockBits(
                    new System.Drawing.Rectangle(0, 0, w, h),
                    System.Drawing.Imaging.ImageLockMode.ReadOnly,
                    System.Drawing.Imaging.PixelFormat.Format8bppIndexed);
                try
                {
                    for (int y = 0; y < h; y++)
                        System.Runtime.InteropServices.Marshal.Copy(
                            bmpData.Scan0 + y * bmpData.Stride,
                            result, y * w, w);
                }
                finally { bmp.UnlockBits(bmpData); }
                return (result, h);
            }

            // RGBA PNG: nearest-neighbour quantise against the PAL.
            // Used when the user has painted a new texture atlas.
            var palRgb = new (int r, int g, int b)[256];
            for (int i = 0; i < 256; i++)
                palRgb[i] = (Math.Min(palData[i * 3] * 4, 255),
                             Math.Min(palData[i * 3 + 1] * 4, 255),
                             Math.Min(palData[i * 3 + 2] * 4, 255));

            var rgba = new byte[w * h];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    var px = bmp.GetPixel(x, y);
                    if (px.A < 128) { rgba[y * w + x] = 0; continue; }
                    int best = 1, bestDist = int.MaxValue;
                    for (int i = 1; i < 256; i++)
                    {
                        var (pr, pg, pb) = palRgb[i];
                        int d = (px.R - pr) * (px.R - pr) + (px.G - pg) * (px.G - pg) + (px.B - pb) * (px.B - pb);
                        if (d < bestDist) { bestDist = d; best = i; if (d == 0) break; }
                    }
                    rgba[y * w + x] = (byte)best;
                }
            return (rgba, h);
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