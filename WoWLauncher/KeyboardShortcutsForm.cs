namespace WoWLauncher
{
    public partial class KeyboardShortcutsForm : Form
    {
        private Dictionary<string, Keybinding> keybindings = new();
        public KeyboardShortcutsForm()
        {
            InitializeComponent();
            ParseExecutable();
        }
        // parse the executables current key bindings
        private void ParseExecutable()
        {
            byte[] data = File.ReadAllBytes("WoW_patched.exe");
            // manually write out labels
            keybindings["Force Fire"] = new Keybinding
            {
                ActionName = "Force Fire",
                Offsets = new List<long> { 0x1CB65, 0x1CBAB, 0x1CC20, 0x1CCB1, 0x2CFC9 ,0x30D3C ,0x30EAD ,0x84B1C ,0x84B55 ,0x84BBD ,0x84C44 ,0x85427 ,0x96598 },
                DefaultVK = 0x11, // CTRL
                CurrentVK = data[0x1CB65],
                LinkedTextBox = textBox3,
                LinkedNewKeyButton = button3,
                LinkedResetButton = button33
            };
            // Update UI with current VK
            keybindings["Force Fire"].LinkedTextBox!.Text = ((Keys)keybindings["Force Fire"].CurrentVK).ToString();
            // Wire up dynamic KeyDown
            keybindings["Force Fire"].LinkedNewKeyButton!.KeyDown += (s, e) =>
            {
                keybindings["Force Fire"].CurrentVK = (byte)e.KeyCode;
                keybindings["Force Fire"].LinkedTextBox!.Text = e.KeyCode.ToString();
            };
            // Reset to default
            keybindings["Force Fire"].LinkedResetButton!.Click += (s, e) =>
            {
                keybindings["Force Fire"].CurrentVK = keybindings["Force Fire"].DefaultVK;
                keybindings["Force Move"].LinkedTextBox!.Text = ((Keys)keybindings["Force Fire"].DefaultVK).ToString();
            };
            // add more keysets // example
            /*keybindings["Pause Game"] = new Keybinding
            {
                ActionName = "Pause Game",
                Offset = 0x1CC20, // EXAMPLE
                DefaultVK = 0x13, // PAUSE key (as example)
            };*/
        }
        // key formula ( label, textbox, new key button, default key button )
        // label1 + textBox1 + button1 + button31 (+30)
        // label2 + textBox2 + button2 + button32 etc
        // label1 - label30 etc
        // reset keys
        private void button61_Click(object sender, EventArgs e)
        {
            foreach (var key in keybindings.Values)
            {
                key.CurrentVK = key.DefaultVK;
                key.LinkedTextBox!.Text = ((Keys)key.DefaultVK).ToString();
            }
        }
        // save keys
        private void button62_Click(object sender, EventArgs e)
        {
            var replacements = keybindings.Values.Where(k => k.IsModified).SelectMany(k => k.Offsets.Select(offset => (offset, k.CurrentVK))).ToList();
            if (replacements.Count == 0)
            {
                MessageBox.Show("No changes to apply.");
                return;
            }
            BinaryUtility.ReplaceByte(replacements, "WoW_patched.exe");
            MessageBox.Show("Shortcuts updated successfully.");
        }
        // on closing event
        private void KeyboardShortcutsForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (keybindings.Values.Any(k => k.IsModified))
            {
                var result = MessageBox.Show(
                        "You have unsaved changes. Do you want to save before exiting?",
                        "Unsaved Changes",
                        MessageBoxButtons.YesNoCancel,
                        MessageBoxIcon.Warning);
                if (result == DialogResult.Cancel) { e.Cancel = true; } // Prevent closing
                else if (result == DialogResult.Yes) { button62.PerformClick(); } // Trigger the save button
            }
        }
    }
}
