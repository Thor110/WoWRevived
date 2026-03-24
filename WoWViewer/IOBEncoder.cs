using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace WoWViewer
{
    // ── IOB Encoder ───────────────────────────────────────────────────────────
    //
    // Encodes an IobModel back to the IOB wire format, and supports two types of
    // edit operation:
    //
    //   FromObj        — replace BSP collision geometry from a Wavefront OBJ.
    //   ReplaceTexture — substitute sprite pixel colours from a PNG.
    //   Encode         — serialise an IobModel to raw bytes (used after either op).
    //
    // ── File layout ──────────────────────────────────────────────────────────
    //
    //   HEADER         34 bytes (0x22)
    //   BSP PLANES     faceCount × 6 bytes  (int16 x/y/z per entry)
    //   NORMALS        normCount × 3 bytes  (int8  x/y/z per entry)
    //     static  (animatedFlag == 0): normCount = litTriCount
    //     animated(animatedFlag != 0): normCount = faceCount
    //   INDEX TABLE    litTriCount × 18 bytes  (always present, both modes)
    //   PATCH DATA     variable, to EOF
    //
    // ── Header encoding ──────────────────────────────────────────────────────
    //
    //   [0x00] uint16  viewingDirCount (bare 2-byte word)
    //   [0x02] uint32  faceCount       (value fits in low uint16, high word = 0)
    //   [0x06] uint32  litTriCount
    //   [0x0A] uint32  animatedFlag
    //   [0x0E] uint32  halfWidthScale
    //   [0x12] uint32  yOffsetScale
    //   [0x16] uint32  heightScale
    //   [0x1A] uint32  bspSectionSize  = faceCount × 6
    //   [0x1E] uint32  normalsEndOffset = bspSectionSize + normCount×3
    //                    (= offset from 0x22 to index table start)
    //
    // Confirmed from real file headers: all fields from [0x02] onward store their
    // value in the low 2 bytes with the high 2 bytes zero-padded.

    public static class IobEncoder
    {
        private const int HeaderSize = 0x22;

        // ── Encode ────────────────────────────────────────────────────────────

        /// <summary>
        /// Serialises <paramref name="model"/> to IOB wire format.
        /// Round-trips byte-perfectly when the model was produced by IobDecoder.Parse.
        /// </summary>
        public static byte[] Encode(IobModel model)
        {
            using var ms = new MemoryStream(HeaderSize + model.TexData.Length);
            using var w = new BinaryWriter(ms);

            int faceCount = model.FaceCount;
            int litTri = model.LitTriCount;
            int anim = model.AnimatedFlag;
            int normCount = (anim == 0) ? litTri : faceCount;
            int bspSize = faceCount * 6;
            int normSize = normCount * 3;
            int normEndOff = bspSize + normSize;   // [0x1E]

            // Header
            w.Write((ushort)model.ViewingDirCount);
            WriteU16Padded(w, faceCount);
            WriteU16Padded(w, litTri);
            WriteU16Padded(w, anim);
            WriteU16Padded(w, model.HalfWidthScale);
            WriteU16Padded(w, model.YOffsetScale);
            WriteU16Padded(w, model.HeightScale);
            WriteU16Padded(w, bspSize);
            WriteU16Padded(w, normEndOff);

            // Data section — TexData stores everything from file[0x22] onward:
            // BSP planes, normals, index table, and patch data, in order.
            if (model.TexData.Length > 0)
                w.Write(model.TexData);

            w.Flush();
            return ms.ToArray();
        }

        // Writes value as a uint16 zero-padded to a uint32 slot (confirmed format).
        private static void WriteU16Padded(BinaryWriter w, int value)
        {
            w.Write((ushort)value);
            w.Write((ushort)0);
        }

        // ── OBJ import ────────────────────────────────────────────────────────
        //
        // Replaces BSP collision geometry from a Wavefront OBJ while keeping the
        // index table and patch sprite data completely untouched.
        //
        // OBJ structure (as exported by IobDecoder.ToObj):
        //   faceCount  "v x y z"   lines — vertex positions in raw world units
        //   faceCount  "vn x y z"  lines — OUTWARD normals (stored inward in IOB)
        //   litTriCount "f v//vn …" lines — face connectivity
        //
        // BSP values ARE raw world units (int16). There is no ×100 conversion.
        // This was confirmed by round-trip testing: exporting raw values and
        // re-importing without any scale factor produces byte-identical BSP sections.
        //
        // Vertex count MUST equal model.FaceCount exactly — the index table v0/v1/v2
        // fields reference normals[0..faceCount-1] and cannot be changed without
        // rewriting all patch offsets.

        /// <summary>
        /// Parses an OBJ file and returns a new IobModel with updated BSP planes and
        /// surface normals, while preserving the index table and all patch data verbatim.
        /// </summary>
        /// <exception cref="InvalidDataException">
        /// Thrown when the OBJ vertex count does not match the model's FaceCount.
        /// </exception>
        public static IobModel FromObj(string objText, IobModel original)
        {
            var verts = new List<(float x, float y, float z)>();
            var normals = new List<(float x, float y, float z)>();
            var faces = new List<(int v0, int v1, int v2)>();

            foreach (var rawLine in objText.Split('\n'))
            {
                var line = rawLine.Trim();
                if (line.StartsWith("vn "))
                {
                    var p = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                    if (p.Length >= 4 &&
                        float.TryParse(p[1], System.Globalization.NumberStyles.Float,
                            System.Globalization.CultureInfo.InvariantCulture, out float nx) &&
                        float.TryParse(p[2], System.Globalization.NumberStyles.Float,
                            System.Globalization.CultureInfo.InvariantCulture, out float ny) &&
                        float.TryParse(p[3], System.Globalization.NumberStyles.Float,
                            System.Globalization.CultureInfo.InvariantCulture, out float nz))
                        normals.Add((nx, ny, nz));
                }
                else if (line.StartsWith("v "))
                {
                    var p = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                    if (p.Length >= 4 &&
                        float.TryParse(p[1], System.Globalization.NumberStyles.Float,
                            System.Globalization.CultureInfo.InvariantCulture, out float x) &&
                        float.TryParse(p[2], System.Globalization.NumberStyles.Float,
                            System.Globalization.CultureInfo.InvariantCulture, out float y) &&
                        float.TryParse(p[3], System.Globalization.NumberStyles.Float,
                            System.Globalization.CultureInfo.InvariantCulture, out float z))
                        verts.Add((x, y, z));
                }
                else if (line.StartsWith("f "))
                {
                    var p = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                    if (p.Length >= 4)
                    {
                        static int ParseIdx(string tok)
                        {
                            var s = tok.Split('/')[0];
                            return int.TryParse(s, out int i) ? i - 1 : 0;
                        }
                        faces.Add((ParseIdx(p[1]), ParseIdx(p[2]), ParseIdx(p[3])));
                    }
                }
            }

            int f02 = original.FaceCount;
            if (verts.Count != f02)
                throw new InvalidDataException(
                    $"OBJ has {verts.Count} vertices but the IOB requires exactly {f02}. " +
                    "Export the current model first, edit it, then re-import.");

            // ── New BSP planes ────────────────────────────────────────────────
            // BSP values are raw world units stored as int16 — no scale factor.
            var newBsp = new (short Nx, short Ny, short Nz)[f02];
            for (int i = 0; i < f02; i++)
            {
                var (x, y, z) = verts[i];
                newBsp[i] = (
                    (short)Math.Clamp((int)Math.Round(x), short.MinValue, short.MaxValue),
                    (short)Math.Clamp((int)Math.Round(y), short.MinValue, short.MaxValue),
                    (short)Math.Clamp((int)Math.Round(z), short.MinValue, short.MaxValue)
                );
            }

            // ── New normals ───────────────────────────────────────────────────
            // OBJ exports OUTWARD normals; IOB stores INWARD — negate on import.
            int normCount = (original.AnimatedFlag == 0) ? original.LitTriCount : f02;
            var newNormals = new (sbyte Nx, sbyte Ny, sbyte Nz)[normCount];
            for (int i = 0; i < normCount && i < normals.Count; i++)
            {
                var (nx, ny, nz) = normals[i];
                newNormals[i] = (
                    (sbyte)Math.Clamp((int)Math.Round(-nx * 127f), -127, 127),
                    (sbyte)Math.Clamp((int)Math.Round(-ny * 127f), -127, 127),
                    (sbyte)Math.Clamp((int)Math.Round(-nz * 127f), -127, 127)
                );
            }

            // ── Splice new BSP+normals into TexData ───────────────────────────
            // TexData = [BSP][normals][index table][patches]
            // We replace only the BSP and normals sections; everything after stays.
            int bspSize = f02 * 6;
            int normSize = normCount * 3;
            int suffixStart = bspSize + normSize;

            if (suffixStart > original.TexData.Length)
                throw new InvalidDataException("TexData shorter than expected BSP+normals section.");

            var newTexData = new byte[original.TexData.Length];

            // Write BSP
            for (int i = 0; i < f02; i++)
            {
                int off = i * 6;
                var (nx, ny, nz) = newBsp[i];
                BitConverter.GetBytes(nx).CopyTo(newTexData, off);
                BitConverter.GetBytes(ny).CopyTo(newTexData, off + 2);
                BitConverter.GetBytes(nz).CopyTo(newTexData, off + 4);
            }

            // Write normals
            for (int i = 0; i < normCount; i++)
            {
                int off = bspSize + i * 3;
                newTexData[off] = (byte)newNormals[i].Nx;
                newTexData[off + 1] = (byte)newNormals[i].Ny;
                newTexData[off + 2] = (byte)newNormals[i].Nz;
            }

            // Copy index table + patches verbatim
            Array.Copy(original.TexData, suffixStart,
                       newTexData, suffixStart,
                       original.TexData.Length - suffixStart);

            var newFaces = faces.Count == original.LitTriCount
                ? faces.Select(f => (f.v0, f.v1, f.v2)).ToArray()
                : original.Faces;

            return original with
            {
                BspPlanes = newBsp,
                Normals = newNormals,
                Faces = newFaces,
                TexData = newTexData,
            };
        }

        // ── Texture replacement ───────────────────────────────────────────────
        //
        // Strategy: IN-PLACE colour substitution.
        //   Walk every patch byte stream using the same decoder logic as
        //   IobDecoder.DecodePatchesToCanvas, but instead of writing to a canvas,
        //   overwrite each colour byte in the stream with the corresponding pixel
        //   from the new canvas. Structural bytes (skip, run) are never touched,
        //   so the file size stays identical and no offset fixup is needed.
        //
        // Patch format (confirmed — see IobDecoder for full notes):
        //   No separate header in the byte stream. Stream starts at patchOffset.
        //   Termination: [0x00][0x00] sentinel OR next patch's offset boundary.
        //   Per row: [skip][run][colour bytes]
        //     run & 0x80 != 0  → RLE:     one colour byte  (replace it)
        //     run & 0x80 == 0,
        //       run > 0        → LITERAL: `run` colour bytes (replace each)
        //     run == 0         → transparent, no colour bytes
        //
        // Patch offset field in index table is uint32 [+14..17], not uint16.

        /// <summary>
        /// Returns a new IobModel with sprite colours replaced from a PNG.
        /// The PNG must be HalfWidthScale × HeightScale pixels (or the derived
        /// height for files where HeightScale == 0).
        /// </summary>
        public static IobModel ReplaceTexture(IobModel model, byte[] pngBytes, byte[] palData,
                                               byte[]? shadeData = null)
        {
            int W = Math.Max(1, model.HalfWidthScale);
            int H = model.HeightScale > 0
                ? model.HeightScale
                : model.Patches.Length > 0
                    ? model.Patches.Max(p => p.ScreenY + p.HByte) + 1
                    : 1;
            H = Math.Max(1, H);

            // When shadeData is provided the exported PNG was rendered with the shader
            // applied, so we must reverse-map shaded colours back to raw palette indices
            // using the SHH table — exactly as WofEncoder.QuantiseTexture does.
            byte[] canvas = QuantisePng(pngBytes, W, H, palData, shadeData);
            byte[] newTex = (byte[])model.TexData.Clone();

            // Build sorted list of distinct patch offsets for boundary computation.
            // Mirrors the logic in IobDecoder.DecodePatchesToCanvas.
            var sortedOffsets = model.Patches
                .Select(p => p.PatchOffset)
                .Distinct()
                .OrderBy(o => o)
                .ToList();
            sortedOffsets.Add(newTex.Length);

            var seen = new HashSet<int>();

            for (int i = 0; i < model.Patches.Length; i++)
            {
                var patch = model.Patches[i];
                if (!seen.Add(patch.PatchOffset)) continue;   // skip duplicates

                int patchStart = patch.PatchOffset;
                if (patchStart < 0 || patchStart >= newTex.Length) continue;

                int nextIdx = sortedOffsets.BinarySearch(patch.PatchOffset);
                int patchEnd = (nextIdx + 1 < sortedOffsets.Count)
                                ? sortedOffsets[nextIdx + 1]
                                : newTex.Length;

                SubstituteColours(
                    newTex, patchStart, patchEnd,
                    canvas, W, H,
                    patch.ScreenX,   // originX = 0, confirmed
                    patch.ScreenY);
            }

            return model with { TexData = newTex };
        }

        /// <summary>
        /// Walks a patch byte stream in [patchStart, patchEnd) and overwrites each
        /// colour byte with the corresponding pixel from <paramref name="canvas"/>.
        /// Mirrors IobDecoder.DecodePatchesToCanvas exactly — same termination logic,
        /// same RLE/literal branching, same coordinate arithmetic.
        /// </summary>
        private static void SubstituteColours(
            byte[] tex, int patchStart, int patchEnd,
            byte[] canvas, int W, int H,
            int baseX, int baseY)
        {
            int pos = patchStart;
            int row = 0;

            while (pos + 1 < patchEnd)
            {
                int skip = tex[pos++];
                int runByte = tex[pos++];

                // [0x00][0x00] = end-of-patch sentinel.
                if (skip == 0 && runByte == 0) break;

                int py = baseY + row;
                int cx = baseX + skip;
                int count = runByte & 0x7F;

                if ((runByte & 0x80) != 0)
                {
                    // RLE: one colour byte covers `count` pixels at cx..cx+count-1.
                    // All those pixels have the same colour, so we sample the first
                    // in-bounds position and write it back.
                    if (pos >= patchEnd) break;
                    if ((uint)py < (uint)H)
                    {
                        // Find the first in-bounds X position in this run.
                        for (int k = 0; k < count; k++)
                        {
                            int fx = cx + k;
                            if ((uint)fx < (uint)W)
                            {
                                byte newColour = canvas[py * W + fx];
                                if (newColour != 0)
                                    tex[pos] = newColour;
                                break;  // one colour byte covers the whole run
                            }
                        }
                    }
                    pos++;  // advance past the single colour byte
                }
                else if (count > 0)
                {
                    // Literal: `count` individual colour bytes, one per pixel.
                    for (int k = 0; k < count; k++, pos++)
                    {
                        if (pos >= patchEnd) break;
                        int fx = cx + k;
                        if ((uint)py < (uint)H && (uint)fx < (uint)W)
                        {
                            byte newColour = canvas[py * W + fx];
                            if (newColour != 0)
                                tex[pos] = newColour;
                        }
                    }
                }
                // count == 0 (transparent row): no colour bytes, nothing to substitute.

                row++;
            }
        }

        // ── PNG → palette index canvas ────────────────────────────────────────

        /// <summary>
        /// Decodes a PNG to a palette-index canvas of size <paramref name="expectedW"/> ×
        /// <paramref name="expectedH"/>. 8-bpp indexed PNGs are read losslessly; true-colour
        /// PNGs are nearest-colour matched against <paramref name="palData"/>.
        /// </summary>
        private static byte[] QuantisePng(byte[] pngBytes, int expectedW, int expectedH,
                                           byte[] palData, byte[]? shadeData = null)
        {
            using var ms = new MemoryStream(pngBytes);
            using var bmp = new Bitmap(ms);

            if (bmp.Width != expectedW || bmp.Height != expectedH)
                throw new InvalidDataException(
                    $"PNG size {bmp.Width}×{bmp.Height} does not match expected " +
                    $"{expectedW}×{expectedH}. Export the current building first " +
                    "to get the correct canvas dimensions.");

            byte[] result = new byte[expectedW * expectedH];

            // Fast path: palette-indexed PNG — read raw indices directly.
            // Only valid when no shader was applied during export (shadeData == null),
            // because a shaded export produces true-colour ARGB, not indexed.
            if (bmp.PixelFormat == PixelFormat.Format8bppIndexed && shadeData == null)
            {
                var bd = bmp.LockBits(new Rectangle(0, 0, expectedW, expectedH),
                             ImageLockMode.ReadOnly, PixelFormat.Format8bppIndexed);
                for (int y = 0; y < expectedH; y++)
                    Marshal.Copy(bd.Scan0 + y * bd.Stride, result, y * expectedW, expectedW);
                bmp.UnlockBits(bd);
                return result;
            }

            // Build colour lookup table.
            // When shadeData is provided: each entry is the shaded RGB565 colour for that
            // palette index — this is what the renderer wrote into the PNG.
            // When shadeData is null: use raw PAL RGB (6-bit × 4 → 8-bit).
            var palRgb = new (int r, int g, int b)[256];
            if (shadeData != null && shadeData.Length >= 512)
            {
                // SHH level-0 slice: 256 × RGB565, little-endian.
                for (int i = 0; i < 256; i++)
                {
                    int rgb565 = shadeData[i * 2] | (shadeData[i * 2 + 1] << 8);
                    int rv = (rgb565 >> 11) & 0x1F; palRgb[i].r = (rv << 3) | (rv >> 2);
                    int gv = (rgb565 >> 5) & 0x3F; palRgb[i].g = (gv << 2) | (gv >> 4);
                    int bv = rgb565 & 0x1F; palRgb[i].b = (bv << 3) | (bv >> 2);
                }
            }
            else
            {
                // Raw PAL: 6-bit RGB values scaled to 8-bit.
                for (int i = 0; i < 256 && i * 3 + 2 < palData.Length; i++)
                    palRgb[i] = (Math.Min(palData[i * 3] * 4, 255),
                                 Math.Min(palData[i * 3 + 1] * 4, 255),
                                 Math.Min(palData[i * 3 + 2] * 4, 255));
            }

            using var argbBmp = bmp.Clone(new Rectangle(0, 0, expectedW, expectedH),
                                          PixelFormat.Format32bppArgb);
            var bd2 = argbBmp.LockBits(new Rectangle(0, 0, expectedW, expectedH),
                           ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var argb = new byte[expectedH * bd2.Stride];
            Marshal.Copy(bd2.Scan0, argb, 0, argb.Length);
            argbBmp.UnlockBits(bd2);

            for (int y = 0; y < expectedH; y++)
                for (int x = 0; x < expectedW; x++)
                {
                    int off = y * bd2.Stride + x * 4;
                    byte b = argb[off], g = argb[off + 1], r = argb[off + 2], a = argb[off + 3];
                    if (a < 128) { result[y * expectedW + x] = 0; continue; }

                    int best = 1, bestDist = int.MaxValue;
                    for (int p = 1; p < 256; p++)
                    {
                        var (pr, pg, pb) = palRgb[p];
                        int d = (r - pr) * (r - pr) + (g - pg) * (g - pg) + (b - pb) * (b - pb);
                        if (d < bestDist) { bestDist = d; best = p; if (d == 0) break; }
                    }
                    result[y * expectedW + x] = (byte)best;
                }

            return result;
        }
    }
}