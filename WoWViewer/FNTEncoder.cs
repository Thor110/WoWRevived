namespace WoWViewer
{
    internal class FNTEncoder
    {
        public static byte[] Encode(FNTDecoder.FntModel font, byte[] originalData)
        {
            // Clone the original so all headers, table entries, and any untouched atlas
            // pixels are preserved exactly.
            byte[] data = (byte[])originalData.Clone();

            int count = font.Glyphs.Length;
            int tableStart = 0x08;
            int dataStart = tableStart + (count * 4) + 2;

            // Read atlas X-positions from the cloned table.
            int[] allX = new int[count];
            for (int i = 0; i < count; i++)
                allX[i] = BitConverter.ToUInt16(data, tableStart + i * 4);

            // Read field2 values (text-advance widths, also used as the glyph's own
            // atlas slot width for clamping the write region).
            int[] allW = new int[count];
            for (int i = 0; i < count; i++)
                allW[i] = BitConverter.ToUInt16(data, tableStart + i * 4 + 2);

            // Pre-compute homeless flags.
            // A homeless glyph (e.g. space) has its atlas_x inside another glyph's
            // field2 slot — it owns no pixels there. Writing zeros would corrupt the
            // host glyph's data. The clone already preserves those bytes correctly,
            // so homeless glyphs are skipped entirely.
            bool[] homeless = new bool[count];
            for (int i = 0; i < count; i++)
            {
                int xi = allX[i], wi = font.Glyphs[i].Width;
                for (int j = 0; j < count; j++)
                {
                    if (j == i) continue;
                    if (allX[j] < xi && (xi + wi) <= (allX[j] + allW[j]))
                    {
                        homeless[i] = true;
                        break;
                    }
                }
            }

            // Stamp new pixels back into the atlas.
            //
            // Write width = min(glyph.Width, field2).
            //   glyph.Width is the extraction width (old_w = next_atlas_x - atlas_x).
            //   For some glyphs old_w > field2, meaning the extraction slot extends into
            //   the next glyph's territory. Clamping to field2 prevents overwriting the
            //   neighbouring glyph's pixels. Columns beyond glyph.Width (when field2 >
            //   old_w) already contain correct data from the clone and are left alone.
            for (int i = 0; i < count; i++)
            {
                if (homeless[i]) continue;

                var glyph = font.Glyphs[i];
                int startX = allX[i];
                int writeWidth = Math.Min(glyph.Width, allW[i]);

                for (int y = 0; y < font.Height; y++)
                {
                    for (int x = 0; x < writeWidth; x++)
                    {
                        int fileOffset = dataStart + (y * font.AtlasWidth) + (startX + x);
                        if (fileOffset >= 0 && fileOffset < data.Length)
                            data[fileOffset] = glyph.Pixels[y * glyph.Width + x];
                    }
                }
            }

            return data;
        }
    }
}