namespace WoWViewer
{
    class WowDatFile // currently unused
    {
        public int Unknown;    // Possibly always zero
        public int Stride;     // e.g. 8, 0x0E
        public int A;          // signed
        public int B;          // signed
        public int Index;      // 0–15

        public override string ToString() => $"Index {Index:D2} - A: 0x{A:X8}, B: 0x{B:X8}";
    }
    public class WowFileEntry
    {
        public string Name { get; set; } = "";
        public int Length { get; set; }
        public int Offset { get; set; }
        public bool Edited { get; set; } = false;
        public byte[]? Data { get; set; } = null; // used for storing the file data in memory
        public int Frames { get; set; }
    }
    class WowSaveEntry
    {
        public string Name { get; set; } = "";
        public DateTime dateTime { get; set; }
        public ushort actualYear { get; set; } // used when the year is below 1753
    }
    class WowTextEntry
    {
        public string Name { get; set; } = "";
        public byte Faction { get; set; } // might not be necessary to store this value either
        public ushort Index { get; set; } // used for getting entry index when filtering by type
        public ushort? ID { get; set; } // TEXT.ojd key (2 bytes at offset+6 in each entry)
        public ushort? BmolId { get; set; } // BMOL ID from OBJ.ojd lookup, null if not a battle map object
        public string? OTypeName { get; set; } // OTYPE_ string from OBJ.ojd, used as fallback display name
        public bool Edited { get; set; } = false;
    }
    class WowTextBackup { public string Name { get; set; } = ""; }
    public class OJDEntry
    {
        public ushort Id { get; set; }
        public ushort Type { get; set; }
        public ushort Length { get; set; }
        public string Name { get; set; } = "";
        public ushort PalSlot { get; set; }  // 0 for types without a palSlot field

        public override string ToString() => $"[{Id:D5}] type={Type:D3} palSlot={PalSlot:D2} len={Length:D2} '{Name}'";
    }
    public class SprInfo
    {
        public ushort Width { get; set; }
        public ushort Height { get; set; }
        public ushort TableCount { get; set; }
        public ushort RowHeaderSize { get; set; }
        public override string ToString() => $"Size={Width}x{Height}  tableCount={TableCount}  rowHeaderSize={RowHeaderSize}";
    }
}
