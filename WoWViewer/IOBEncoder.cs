using System.IO;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace WoWViewer
{
    // ── IOB Encoder ───────────────────────────────────────────────────────────
    //
    // Produces a byte-perfect re-encoding of an IobModel back into the IOB wire
    // format.  All section sizes and offsets are recomputed from scratch so that
    // round-tripping through Parse → Encode yields an identical file.
    //
    // File layout written:
    //
    //   HEADER  (34 bytes = 0x22)
    //   BSP PLANES   faceCount × 6 bytes   (int16 × 3 per entry)
    //   NORMALS      normCount × 3 bytes   (int8  × 3 per entry)
    //     static:  normCount = litTriCount  (no index table follows)
    //     animated: normCount = faceCount   (index table follows)
    //   INDEX TABLE  animated only: litTriCount × 18 bytes
    //   PIXEL DATA   texData bytes (all, including any trailing partial row)
    //
    // Header field encoding: every field is a zero-padded uint16 stored as a
    // 4-byte little-endian dword — i.e. value fits in low 2 bytes, high 2 = 0.
    // Exception: [0x00] is a bare uint16 (2 bytes), consistent with the game.
    //
    // [0x1E] normalsEndOffset = offset from 0x22 to the end of the normals block
    //   static:   = faceCount*6 + litTriCount*3       (= bspSize + normSize)
    //   animated: = faceCount*6 + faceCount*3          (= bspSize + normSize)
    // This equals (pixelStart - 0x22) for static, and (indexTableStart - 0x22)
    // for animated — consistent with how sub_49DF40 uses the field at runtime.

    public static class IobEncoder
    {
        /// <summary>
        /// Encodes the supplied IobModel to a byte array in IOB wire format.
        /// The result is byte-identical to the original file when the model was
        /// produced by IobDecoder.Parse (round-trip fidelity).
        ///
        /// IobModel.TexData stores all file bytes from offset 0x22 onwards, so
        /// encoding is simply: 34-byte header + TexData.
        /// The BSP planes, normals, index table and patch data are all embedded
        /// in TexData exactly as they appear in the file.
        /// </summary>
        public static byte[] Encode(IobModel model)
        {
            using var ms = new MemoryStream(0x22 + model.TexData.Length);
            using var w = new BinaryWriter(ms);

            // ── Header (34 bytes = 0x22) ──────────────────────────────────────
            // Every field is a zero-padded uint16 stored as a 4-byte LE dword,
            // except [0x00] which is a bare uint16.
            int faceCount = model.FaceCount;
            int litTri = model.LitTriCount;
            int anim = model.AnimatedFlag;
            int normCount = (anim == 0) ? litTri : faceCount;
            int bspSize = faceCount * 6;
            int normSize = normCount * 3;
            int normEndOff = bspSize + normSize;   // = [0x1E]

            w.Write((ushort)model.ViewingDirCount);
            WriteU16Padded(w, faceCount);
            WriteU16Padded(w, litTri);
            WriteU16Padded(w, anim);
            WriteU16Padded(w, model.HalfWidthScale);
            WriteU16Padded(w, model.YOffsetScale);
            WriteU16Padded(w, model.HeightScale);
            WriteU16Padded(w, bspSize);
            WriteU16Padded(w, normEndOff);

            // ── Data section (BSP + normals + index table + patches) ──────────
            // TexData = file[0x22..EOF], so writing it verbatim reconstructs the file.
            if (model.TexData.Length > 0)
                w.Write(model.TexData);

            w.Flush();
            return ms.ToArray();
        }

        // Writes a value as a zero-padded uint16 in a uint32 slot (little-endian).
        private static void WriteU16Padded(BinaryWriter w, int value)
        {
            w.Write((ushort)value);
            w.Write((ushort)0);
        }

        // ── OBJ import ────────────────────────────────────────────────────────
        //
        // Parses an OBJ file and produces a new IobModel with the updated BSP
        // vertex positions and surface normals, while keeping all sprite patch
        // data intact.
        //
        // IOB OBJ structure (as written by IobDecoder.ToObj):
        //   f02  "v x y z"   lines — vertex positions (world units; ×100 → int16)
        //   f02  "vn x y z"  lines — outward normals  (negated → inward int8)
        //   f06  "f v//vn …" lines — face connectivity (must match index table)
        //
        // Constraints:
        //   • Vertex count MUST equal model.FaceCount (f02).  The index table
        //     v0/v1/v2 fields reference normals[0..f02-1] and cannot be changed
        //     without rewriting the index table (which would break all patch offsets).
        //   • Face count should equal model.LitTriCount (f06); a mismatch is a
        //     warning but not a hard error — the index table is unchanged.
        //
        // The rebuilt TexData is:
        //   [new BSP bytes] + [new normal bytes] + [original TexData suffix]
        // where suffix = TexData[bspSize + normSize ..] = index table + patches.

        /// <summary>
        /// Parses an OBJ string and returns a new IobModel with updated BSP planes
        /// and surface normals taken from the OBJ, while preserving all sprite patch
        /// data from the original <paramref name="original"/> model unchanged.
        /// </summary>
        /// <param name="objText">Contents of the .obj file.</param>
        /// <param name="original">The model to update — provides FaceCount, AnimatedFlag,
        /// and the TexData suffix (index table + patches) that must be preserved.</param>
        /// <exception cref="InvalidDataException">Thrown if the OBJ vertex count does not
        /// match the original model's FaceCount.</exception>
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
                            // "v//vn" or "v/vt/vn" or just "v"
                            var s = tok.Split('/')[0];
                            return int.TryParse(s, out int i) ? i - 1 : 0; // 0-based
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

            // ── Build new BSP planes (int16 × 3) ─────────────────────────────
            // OBJ is in world units; BSP stores centiunits (world × 100, clamped to int16).
            var newBsp = new (short Nx, short Ny, short Nz)[f02];
            for (int i = 0; i < f02; i++)
            {
                var (x, y, z) = verts[i];
                newBsp[i] = (
                    (short)Math.Clamp((int)Math.Round(x * 100f), short.MinValue, short.MaxValue),
                    (short)Math.Clamp((int)Math.Round(y * 100f), short.MinValue, short.MaxValue),
                    (short)Math.Clamp((int)Math.Round(z * 100f), short.MinValue, short.MaxValue)
                );
            }

            // ── Build new normals (int8 × 3) ──────────────────────────────────
            // OBJ exports OUTWARD normals (negated stored values).
            // IOB stores INWARD normals, so we negate the OBJ vn back.
            int normCount = (original.AnimatedFlag == 0) ? original.LitTriCount : f02;
            var newNormals = new (sbyte Nx, sbyte Ny, sbyte Nz)[normCount];
            for (int i = 0; i < normCount; i++)
            {
                if (i < normals.Count)
                {
                    var (nx, ny, nz) = normals[i];
                    newNormals[i] = (
                        (sbyte)Math.Clamp((int)Math.Round(-nx * 127f), -127, 127),
                        (sbyte)Math.Clamp((int)Math.Round(-ny * 127f), -127, 127),
                        (sbyte)Math.Clamp((int)Math.Round(-nz * 127f), -127, 127)
                    );
                }
                // else: leave as zero (padding vert)
            }

            // ── Splice new BSP+normals into TexData ───────────────────────────
            // TexData layout: [BSP (f02×6)] [normals (normCount×3)] [index table + patches]
            int bspSize = f02 * 6;
            int normSize = normCount * 3;
            int suffixStart = bspSize + normSize;   // start of index table in TexData

            if (suffixStart > original.TexData.Length)
                throw new InvalidDataException("TexData is shorter than expected BSP+normals section.");

            var newTexData = new byte[original.TexData.Length];  // same total size

            // Write new BSP
            for (int i = 0; i < f02; i++)
            {
                int off = i * 6;
                var (nx, ny, nz) = newBsp[i];
                BitConverter.GetBytes(nx).CopyTo(newTexData, off);
                BitConverter.GetBytes(ny).CopyTo(newTexData, off + 2);
                BitConverter.GetBytes(nz).CopyTo(newTexData, off + 4);
            }

            // Write new normals
            for (int i = 0; i < normCount; i++)
            {
                int off = bspSize + i * 3;
                newTexData[off] = (byte)newNormals[i].Nx;
                newTexData[off + 1] = (byte)newNormals[i].Ny;
                newTexData[off + 2] = (byte)newNormals[i].Nz;
            }

            // Copy preserved suffix (index table + patches) verbatim
            Array.Copy(original.TexData, suffixStart, newTexData, suffixStart,
                       original.TexData.Length - suffixStart);

            // ── Build updated model ───────────────────────────────────────────
            var newFaceArr = faces.Count == original.LitTriCount
                ? faces.Select(f => (f.v0, f.v1, f.v2)).ToArray()
                : original.Faces;

            return original with
            {
                BspPlanes = newBsp,
                Normals = newNormals,
                Faces = newFaceArr,
                TexData = newTexData,
            };
        }
    }
}