namespace WoWLauncher
{
    public class Keybinding
    {
        public List<long> Offsets { get; set; } = new();
        public byte DefaultVK { get; set; }
        public byte CurrentVK { get; set; }

        public TextBox? LinkedTextBox { get; set; }
        public Button? LinkedNewKeyButton { get; set; }
        public Button? LinkedResetButton { get; set; }

        public bool IsModified => CurrentVK != DefaultVK;
        public bool ListeningForInput { get; set; }
    }
}