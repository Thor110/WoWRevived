namespace WoWLauncher
{
    public class Keybinding
    {
        public string ActionName { get; set; } = string.Empty;
        public List<long> Offsets { get; set; } = new();
        public byte DefaultVK { get; set; }
        public byte CurrentVK { get; set; }

        public TextBox? LinkedTextBox { get; set; }
        public Button? LinkedNewKeyButton { get; set; }
        public Button? LinkedResetButton { get; set; }

        public bool IsModified => CurrentVK != DefaultVK;
    }
}