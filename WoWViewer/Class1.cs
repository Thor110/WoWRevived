namespace WoWViewer
{
    class WowDatFile
    {
        public int Unknown;    // Possibly always zero
        public int Stride;     // e.g. 8, 0x0E
        public int A;          // signed
        public int B;          // signed
        public int Index;      // 0–15

        public override string ToString() => $"Index {Index:D2} - A: 0x{A:X8}, B: 0x{B:X8}";
    }
    class WowHuffmanContext
    {
        public int BufferSize;      // From ebx
        public int Flags;           // (arg_8 & 0xFF) | 0x08
        public int ID;              // From arg_0
        public byte[]? Buffer;      // Allocated memory
        public string? SourceName;  // 9-char name from Source
        public byte ExtraFlag;      // Always 0
        public int ComputeTableSize(ushort entryCount, int itemSize)
        {
            return ((itemSize + 4) * entryCount) + 0x14;
        }
    }
    class WowFileEntry
    {
        public string Name { get; set; } = String.Empty;
        public int Length { get; set; }
        public int Offset { get; set; }
        public bool Edited { get; set; } = false;
        public byte[]? Data { get; set; } = null; // used for storing the file data in memory
    }
    class WowSaveEntry
    {
        public string Name { get; set; } = String.Empty;
        public DateTime dateTime { get; set; }
        public ushort actualYear { get; set; } // used when the year is below 1753
    }
    class WowTextEntry
    {
        public string Name { get; set; } = String.Empty;
        public byte Faction { get; set; } // might not be necessary to store this value either
        public ushort Index { get; set; } // used for getting entry index when filtering by type
        public bool Edited { get; set; } = false;
    }
    class WowTextBackup
    {
        public string Name { get; set; } = String.Empty;
    }
    public class OjdEntry
    {
        public ushort Id { get; set; }
        public ushort Type { get; set; }
        public ushort Length { get; set; }
        public string Name { get; set; } = String.Empty;
        public override string ToString()
        {
            return $"ID: {Id:X4}, Type: {Type:X4}, Length: {Length:X4}, Path: {Name}";
        }
    }
}
