namespace WoWLauncher
{
    public partial class KeyboardShortcutsForm : Form
    {
        private Dictionary<string, Keybinding> keybindings = new();
        public KeyboardShortcutsForm()
        {
            InitializeComponent();
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, typeof(Button) );
            ParseExecutable();
        }
        // parse the executables current key bindings
        // key formula ( label, textbox, new key button, default key button )
        // label1 + textBox1 + button1 + button31 (+30)
        // label2 + textBox2 + button2 + button32 etc
        // label1 - label30 etc
        private void ParseExecutable()
        {
            byte[] data = File.ReadAllBytes("WoW_patched.exe");
            // manually write out label tooltip descriptions
            AddKeybinding(data, // CONTROL
                keyName: "Force Fire",
                actionName: "Force Fire",
                offsets: new List<long> { 0x96BAC, 0x96AB8, 0x8507D, 0x1CB65, 0x1CBAB, 0x1CC20, 0x1CCB1, 0x2CFC9, 0x30D3C, 0x30EAD, 0x84B1C, 0x84B55, 0x84BBD, 0x84C44, 0x85427, 0x96598 },
                defaultVK: 0x11,
                linkedTextBox: textBox3,
                linkedNewKeyButton: button3,
                linkedResetButton: button33);
            AddKeybinding(data, // ESCAPE
                keyName: "In-game Menu",
                actionName: "In-game Menu",
                offsets: new List<long> { 0x4D800, 0x4D619, 0x4D66D },
                defaultVK: 0x1B,
                linkedTextBox: textBox1,
                linkedNewKeyButton: button1,
                linkedResetButton: button31);

            /*AddKeybinding(data, // SHIFT
               keyName: "UNKNOWN",
               actionName: "UNKNOWN",
               offsets: new List<long> { 0x2B2F9, 0x33391, 0x96B3A, 0x7C9B8, 0x3B7A6, 0x2B709, 0x2B6DC, 0x2B678, 0x2B628, 0x2B5DA, 0x2B591 },
               defaultVK: 0x10,
               linkedTextBox: textBox1,
               linkedNewKeyButton: button1,
               linkedResetButton: button31);*/

            AddKeybinding(data, // LEFT CLICK
                keyName: "Left Click",
                actionName: "Left Click",
                offsets: new List<long> { 0x32D25, 0x32D4E },
                defaultVK: 0x01,
                linkedTextBox: textBox31,
                linkedNewKeyButton: button63,
                linkedResetButton: button64);
            AddKeybinding(data, // RIGHT CLICK
                keyName: "Right Click",
                actionName: "Right Click",
                offsets: new List<long> { 0x32D32, 0x32D57 },
                defaultVK: 0x02,
                linkedTextBox: textBox32,
                linkedNewKeyButton: button65,
                linkedResetButton: button66);
        }
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
        private void AddKeybinding(
            byte[] data,
            string keyName,
            string actionName,
            List<long> offsets,
            byte defaultVK,
            TextBox linkedTextBox,
            Button linkedNewKeyButton,
            Button linkedResetButton)
        {
            keybindings[keyName] = new Keybinding
            {
                ActionName = actionName,
                Offsets = offsets,
                DefaultVK = defaultVK,
                CurrentVK = data[(int)offsets[0]], // Just use the first offset as representative
                LinkedTextBox = linkedTextBox,
                LinkedNewKeyButton = linkedNewKeyButton,
                LinkedResetButton = linkedResetButton
            };
            updateUI(keyName);
        }
        public void updateUI(string keyName)
        {
            // Update UI with current VK
            keybindings[keyName].LinkedTextBox!.Text = ((Keys)keybindings[keyName].CurrentVK).ToString();
            // Wire up dynamic KeyDown
            keybindings[keyName].LinkedNewKeyButton!.KeyDown += (s, e) =>
            {
                byte newVK = (byte)e.KeyCode;
                if (!IsValidVirtualKey(newVK))
                {
                    ping(keyName);
                    return;
                }
                keybindings[keyName].CurrentVK = newVK;
                keybindings[keyName].LinkedTextBox!.Text = e.KeyCode.ToString();
            };
            // Reset to default
            keybindings[keyName].LinkedResetButton!.Click += (s, e) =>
            {
                keybindings[keyName].CurrentVK = keybindings[keyName].DefaultVK;
                keybindings[keyName].LinkedTextBox!.Text = ((Keys)keybindings[keyName].DefaultVK).ToString();
            };
        }
        public void ping(string keyName) { MessageBox.Show(FormatInvalidKeyMessage(keyName), "Invalid Key", MessageBoxButtons.OK, MessageBoxIcon.Warning); }   
        public string FormatInvalidKeyMessage(string keyName)
        {
            if (keybindings.TryGetValue(keyName, out var binding))
            {
                return $"Keybind '{binding.ActionName}' is set to an invalid key ({binding.CurrentVK}). It won't be saved.";
            }
            return $"Keybind '{keyName}' is not found or is invalid.";
        }
        // check if the key is safe to use
        bool IsSafeKey(byte vk)
        {
            return (vk >= 0x08 && vk <= 0xFE) &&
                   vk != 0x7F && // DEL, rarely usable
                   vk != 0x40;   // '@' or other non-functional VKs
        }
        public static bool IsValidVirtualKey(byte vk) { return (vk >= 1 && vk <= 127); }
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
