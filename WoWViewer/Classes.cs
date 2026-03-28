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

    // ── Save Editor Form ────────────────────────────────────────────────────────
    // ── Data classes ────────────────────────────────────────────────────────────

    public class SeloData
    {
        public uint Seq { get; set; }
        public uint A { get; set; }
        public uint B { get; set; }
    }

    public class BamoData
    {
        public uint Unk { get; set; }
        public float X { get; set; }
        public float Z { get; set; }
        public uint Unk2 { get; set; }
        public int HP { get; set; }
    }

    public class UnitPart
    {
        public int VehicleId { get; set; }  // VEHI wmob_id (0 for lead part)
        public byte[] BatpPayload { get; set; } = Array.Empty<byte>(); // raw BATP bytes before VEHI (lead part only stores post-BAMO BATP)
        public SeloData Selo { get; set; } = new();
        public BamoData Bamo { get; set; } = new();
        // convenience
        public float X => Bamo.X;
        public float Z => Bamo.Z;
        public int HP => Bamo.HP;
        public uint SeloSeq => Selo.Seq;
    }

    public class UnitGroup
    {
        public int BmolId { get; set; }
        public string Name { get; set; } = "";
        // Group-level SELO (before WMOB)
        public SeloData GroupSelo { get; set; } = new();
        public int WmobSector { get; set; }
        // VEHU next wmob_id (links to next group or terminator)
        public int VehuNextWmob { get; set; }
        public List<UnitPart> Parts { get; set; } = new();
        // Convenience — lead part values
        public int PartCount => Parts.Count;
        public float X => Parts.Count > 0 ? Parts[0].X : 0f;
        public float Z => Parts.Count > 0 ? Parts[0].Z : 0f;
        public int HP => Parts.Count > 0 ? Parts[0].HP : 0;
    }

    public class BuildingEntry
    {
        public int BmolId { get; set; }
        public string Name { get; set; } = "";
        // Preamble (war map lookahead)
        public float Progress1 { get; set; }
        public float Progress2 { get; set; }
        public float Progress => Math.Min(Progress1, Progress2);
        // Per-building SELO (group level)
        public SeloData Selo1 { get; set; } = new();
        public int WmobSector { get; set; }
        // BMOL inside entry (battle map)
        public int BmolCount { get; set; }
        public int BmolIdInner { get; set; }
        // Per-building SELO (battle map)
        public SeloData Selo2 { get; set; } = new();
        public BamoData Bamo { get; set; } = new();
        public byte[] BatpBytes { get; set; } = Array.Empty<byte>(); // 8 zero bytes for buildings
                                                                     // Convenience
        public float X => Bamo.X;
        public float Z => Bamo.Z;
        public int HP => Bamo.HP;
    }

    public class SectorData
    {
        // SECTHUNIWMOB header
        public int UnitCount { get; set; }
        public int FirstWmob { get; set; }
        public List<UnitGroup> Units { get; set; } = new();
        public List<BuildingEntry> Buildings { get; set; } = new();
        // Tail: ACON, HRES, ARES, DMGL, FFUH — stored raw
        public byte[] TailBytes { get; set; } = Array.Empty<byte>();
    }
}