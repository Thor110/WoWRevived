namespace WoWViewer
{
    public static class SprDecoder
    {
        public static Bitmap LoadSprAsGrayscale(string filePath)
        {
            using var br = new BinaryReader(File.OpenRead(filePath));
            // Read width and height
            ushort width = br.ReadUInt16();   // 0x00
            ushort height = br.ReadUInt16();  // 0x02
            ushort frameCount = br.ReadUInt16(); // 0x04 — possibly

            List<int> frameOffsets = new();
            for (int i = 0; i < 256; i++) // Max frames or bytes
            {
                int offset = br.ReadUInt16();
                if (offset == 0) break;
                frameOffsets.Add(offset);
            }
            br.BaseStream.Seek(frameOffsets[0], SeekOrigin.Begin);
            byte[] pixels = new byte[width * height];
            int pixelIndex = 0;

            while (pixelIndex < pixels.Length && br.BaseStream.Position < br.BaseStream.Length)
            {
                byte count = br.ReadByte();
                byte value = br.ReadByte();
                for (int i = 0; i < count && pixelIndex < pixels.Length; i++)
                {
                    pixels[pixelIndex++] = value;
                }
            }
            var bmp = new Bitmap(width, height);
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    byte val = pixels[y * width + x];
                    Color c = Color.FromArgb(val, val, val);
                    bmp.SetPixel(x, y, c);
                }
            }

            return bmp;
        }
    }

}
