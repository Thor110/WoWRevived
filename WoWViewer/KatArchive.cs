using System.Text;

namespace WoWViewer
{
    /// <summary>
    /// Writes "KAT!" archives (MAPS.WoW, the .CLS/.ATM archives and friends).
    /// Layout: "KAT!", int32 count, then a 44 byte directory entry per file:
    /// skip(4) + offset(4) + length(4) + name(12, 0xCD filled, null terminated) + 20 bytes of 0xCD,
    /// then a 0xCD gap and the file data.
    /// </summary>
    public static class KatArchive
    {
        private const int EntrySize = 44;
        private const int NameLength = 12;
        private const byte Fill = 0xCD;

        /// <summary>
        /// Rewrites the archive at path with the given entries. The original skip(4) values and the gap between the
        /// directory and the first file are kept. The old file is kept as path.bak.
        /// </summary>
        public static void Write(string path, IList<WowFileEntry> entries)
        {
            if (string.IsNullOrEmpty(path) || !File.Exists(path))
                throw new FileNotFoundException("Archive path not set or file not found.", path);

            int[] skipValues;
            int postDirGap;
            using (var br = new BinaryReader(File.OpenRead(path)))
            {
                br.ReadInt32();                          // magic
                int fileCount = br.ReadInt32();
                skipValues = new int[fileCount];
                int firstDataOffset = int.MaxValue;
                for (int i = 0; i < fileCount; i++)
                {
                    skipValues[i] = br.ReadInt32();
                    int entryOffset = br.ReadInt32();
                    br.ReadInt32();                      // length
                    br.BaseStream.Seek(NameLength + 20, SeekOrigin.Current);
                    if (entryOffset > 0 && entryOffset < firstDataOffset)
                        firstDataOffset = entryOffset;
                }
                int dirEnd = 8 + fileCount * EntrySize;
                postDirGap = firstDataOffset != int.MaxValue ? Math.Max(0, firstDataOffset - dirEnd) : 0;
            }

            byte[] pad20 = Enumerable.Repeat(Fill, 20).ToArray();
            var offsets = new int[entries.Count];
            var lengths = new int[entries.Count];

            string tmp = path + ".tmp";
            try
            {
                using (var bw = new BinaryWriter(File.Create(tmp)))
                {
                    bw.Write(Encoding.ASCII.GetBytes("KAT!"));
                    bw.Write(entries.Count);

                    // directory placeholder, filled in once the offsets are known
                    long dirStart = bw.BaseStream.Position;
                    bw.Write(new byte[entries.Count * EntrySize]);
                    bw.Write(Enumerable.Repeat(Fill, postDirGap).ToArray());

                    for (int i = 0; i < entries.Count; i++)
                    {
                        byte[] data = entries[i].Data ?? Array.Empty<byte>();
                        offsets[i] = (int)bw.BaseStream.Position;
                        lengths[i] = data.Length;
                        bw.Write(data);
                    }

                    bw.BaseStream.Seek(dirStart, SeekOrigin.Begin);
                    for (int i = 0; i < entries.Count; i++)
                    {
                        bw.Write(i < skipValues.Length ? skipValues[i] : 0);
                        bw.Write(offsets[i]);
                        bw.Write(lengths[i]);
                        bw.Write(MakeNameField(entries[i].Name));
                        bw.Write(pad20);
                    }
                }
                File.Replace(tmp, path, path + ".bak");
            }
            catch
            {
                try { File.Delete(tmp); } catch (IOException) { } catch (UnauthorizedAccessException) { }
                throw;
            }

            // only once the archive is safely written
            for (int i = 0; i < entries.Count; i++)
            {
                entries[i].Offset = offsets[i];
                entries[i].Length = lengths[i];
                entries[i].Edited = false;
            }
        }

        // 12 bytes of 0xCD with the name and its terminating null written over the start (11 characters at most)
        private static byte[] MakeNameField(string name)
        {
            var field = Enumerable.Repeat(Fill, NameLength).ToArray();
            byte[] nb = Encoding.ASCII.GetBytes(name);
            int copy = Math.Min(nb.Length, NameLength - 1);
            Array.Copy(nb, field, copy);
            field[copy] = 0;
            return field;
        }
    }
}
