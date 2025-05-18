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
        public int Length { get; set; }
        public int Offset { get; set; }
        public byte Faction { get; set; }
        public ushort Index { get; set; }
        public bool Edited { get; set; } = false;
    }
}
