namespace WoWViewer
{
    class WowDatFile
    {
        public int Unknown { get; set; }     // Often 0
        public int Length { get; set; }       // Usually 0x28 (40 decimal)
        public int Field { get; set; }       // Always 1?
        public uint Index { get; set; }        // 0–15
        public int A { get; set; }
        public int B { get; set; }
        public int Type { get; set; }

        public override string ToString()
        {
            return $"Index {Index:D2} - A: 0x{A:X8}, B: 0x{B:X8}";
        }
    }
    class WowHuffmanContext
    {
        public int BufferSize;      // From ebx
        public int Flags;           // (arg_8 & 0xFF) | 0x08
        public int ID;              // From arg_0
        public byte[]? Buffer;      // Allocated memory
        public string? SourceName;  // 9-char name from Source
        public byte ExtraFlag;      // Always 0
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
}
