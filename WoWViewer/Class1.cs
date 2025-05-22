namespace WOWViewer
{
    class WowFileEntry
    {
        public string Name { get; set; } = String.Empty;
        public int Length { get; set; }
        public int Offset { get; set; }
    }
    class WowSaveEntry
    {
        public string Name { get; set; } = String.Empty;
        public DateTime dateTime { get; set; }
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
