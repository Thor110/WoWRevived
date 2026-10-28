using System.Text;

namespace WoWViewer
{
    /// <summary>
    /// Reads and writes "SfxL" sound effect libraries.
    /// Layout: "SfxL", int32 count, then per entry an 8 byte padded name, int32 length and int32 offset,
    /// followed by the raw 16-bit mono PCM data for every entry.
    /// </summary>
    public static class SfxlArchive
    {
        public const string Magic = "SfxL";
        private const int NameLength = 8;

        public static List<WowFileEntry> Read(byte[] file)
        {
            using var br = new BinaryReader(new MemoryStream(file));
            if (Encoding.ASCII.GetString(br.ReadBytes(4)) != Magic) { throw new InvalidDataException("Not a SfxL archive."); }
            return ReadEntries(br, br.ReadInt32());
        }

        // reads the entry table and data, br must be positioned just after the file count
        public static List<WowFileEntry> ReadEntries(BinaryReader br, int fileCount)
        {
            var entries = new List<WowFileEntry>(fileCount);
            for (int i = 0; i < fileCount; i++)
            {
                string name = Encoding.ASCII.GetString(br.ReadBytes(NameLength)).TrimEnd('\0');
                int length = br.ReadInt32();
                int offset = br.ReadInt32();
                long store = br.BaseStream.Position;
                br.BaseStream.Seek(offset, SeekOrigin.Begin);
                entries.Add(new WowFileEntry { Name = name, Length = length, Offset = offset, Data = br.ReadBytes(length) });
                br.BaseStream.Position = store;
            }
            return entries;
        }

        /// <summary>
        /// Builds a complete archive from the in-memory entry data and updates each entry's
        /// Offset and Length to match the new layout, so a second save stays consistent.
        /// </summary>
        public static byte[] Write(IList<WowFileEntry> entries)
        {
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms);
            bw.Write(Encoding.ASCII.GetBytes(Magic));
            bw.Write(entries.Count);
            int dataOffset = 8 + entries.Count * (NameLength + 8);
            foreach (var entry in entries)
            {
                byte[] data = entry.Data ?? throw new InvalidDataException($"Entry '{entry.Name}' has no data loaded.");
                byte[] name = new byte[NameLength];
                byte[] nameBytes = Encoding.ASCII.GetBytes(entry.Name);
                Array.Copy(nameBytes, name, Math.Min(NameLength, nameBytes.Length));
                bw.Write(name);
                bw.Write(data.Length);
                bw.Write(dataOffset);
                entry.Offset = dataOffset;
                entry.Length = data.Length;
                dataOffset += data.Length;
            }
            foreach (var entry in entries) { bw.Write(entry.Data!); }
            bw.Flush();
            return ms.ToArray();
        }
    }
}
