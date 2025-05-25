namespace WoWLauncher
{
    public partial class KeyboardShortcutsForm : Form
    {
        private Dictionary<string, Keybinding> keybindings = new();
        public KeyboardShortcutsForm()
        {
            InitializeComponent();
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, typeof(Button));
            ParseExecutable();
        }
        // parse the executables current key bindings
        // key formula ( label, textbox, new key button, default key button )
        // label1 + textBox1 + button1 + button31 (+30)
        // label2 + textBox2 + button2 + button32 etc
        // label1 - label30 etc label 31-33 are different but all values are set already
        private void ParseExecutable()
        {
            byte[] data = File.ReadAllBytes("WoW_patched.exe");
            // TODO : manually write out label tooltip descriptions
                                                    // FUNCTION             // Int64 Value          // Key Name // Offset
            AddKeybinding(data[0x4D800], // ESCAPE
                keyName: "In-game Menu",
                offsets: new List<long> { 0x4D800,  // SKIPS MOVIES         // 20989574264068970    // ESCAPE // 4D800
                    0x4D619,                        // UNKNOWN              // 8466983863921351530  // ESCAPE // 4D619
                    0x4D66D                         // UNKNOWN              // 8466983863921351530  // ESCAPE // 4D66D
                },
                defaultVK: 0x1B,
                linkedTextBox: textBox1,
                linkedNewKeyButton: button1,
                linkedResetButton: button31);
            /*AddKeybinding(data[], // TAB
                keyName: "Hide Mini Map",
                offsets: new List<long> {  },
                defaultVK: 0x09,
                linkedTextBox: textBox2,
                linkedNewKeyButton: button2,
                linkedResetButton: button32);*/
            AddKeybinding(data[0x96BAC], // CONTROL // CURSOR KEY UP EVENT NOT FOUND WITHIN THESE VALUES
                keyName: "Force Fire",
                offsets: new List<long> { 0x96BAC,  // UNKNOWN              // 25290642783474026    // CTRL // 96BAC
                    0x96AB8,                        // UNKNOWN              // 8394926269816312170  // CTRL // 96AB8
                    0x8507D,                        // FORCE FIRE COMMAND   // 18388800105288700266 // CTRL // 8507D
                    0x1CB65,                        // UNKNOWN              // 20989574264066410    // CTRL // 1CB65
                    0x1CBAB,                        // UNKNOWN              // 20989574264066410    // CTRL // 1CBAB
                    0x1CC20,                        // UNKNOWN              // 20989574264066410    // CTRL // 1CC20
                    0x1CCB1,                        // UNKNOWN              // 20989574264066410    // CTRL // 1CCB1
                    0x2CFC9,                        // UNKNOWN              // 20989574264066410    // CTRL // 2CFC9
                    0x30D3C,                        // UNKNOWN              // 20989574264066410    // CTRL // 30D3C
                    0x30EAD,                        // UNKNOWN              // 20989574264066410    // CTRL // 30EAD
                    0x84B1C,                        // UNKNOWN              // 20989574264066410    // CTRL // 84B1C
                    0x84B55,                        // UNKNOWN              // 20989574264066410    // CTRL // 84B55
                    0x84BBD,                        // UNKNOWN              // 20989574264066410    // CTRL // 84BBD
                    0x84C44,                        // UNKNOWN              // 20989574264066410    // CTRL // 84C44
                    0x85427,                        // CURSOR KEY DOWN      // 20989574264066410    // CTRL // 85427
                    0x96598                         // UNKNOWN              // 20989574264066410    // CTRL // 96598
                },
                defaultVK: 0x11,
                linkedTextBox: textBox3,
                linkedNewKeyButton: button3,
                linkedResetButton: button33);
            /*AddKeybinding(data[], // HOME
                keyName: "Centers On Selection",
                offsets: new List<long> {  },
                defaultVK: 0x24,
                linkedTextBox: textBox4,
                linkedNewKeyButton: button4,
                linkedResetButton: button34);*/
            /*AddKeybinding(data[], // PAGEUP
                keyName: "Fast Scroll Up",
                offsets: new List<long> {  },
                defaultVK: 0x21,
                linkedTextBox: textBox5,
                linkedNewKeyButton: button5,
                linkedResetButton: button35);*/
            /*AddKeybinding(data[], // PAGEDN
                keyName: "Fast Scroll Down",
                offsets: new List<long> {  },
                defaultVK: 0x22,
                linkedTextBox: textBox6,
                linkedNewKeyButton: button6,
                linkedResetButton: button36);*/
            /*AddKeybinding(data[], // UP
                keyName: "Scroll Up",
                offsets: new List<long> {  },
                defaultVK: 0x26,
                linkedTextBox: textBox7,
                linkedNewKeyButton: button7,
                linkedResetButton: button37);*/
            /*AddKeybinding(data[], // DOWN
                keyName: "Scroll Down",
                offsets: new List<long> {  },
                defaultVK: 0x28,
                linkedTextBox: textBox8,
                linkedNewKeyButton: button8,
                linkedResetButton: button38);*/
            /*AddKeybinding(data[], // LEFT
                keyName: "Scroll Left",
                offsets: new List<long> {  },
                defaultVK: 0x25,
                linkedTextBox: textBox9,
                linkedNewKeyButton: button9,
                linkedResetButton: button39);*/
            /*AddKeybinding(data[], // RIGHT
                keyName: "Scroll Right",
                offsets: new List<long> {  },
                defaultVK: 0x27,
                linkedTextBox: textBox10,
                linkedNewKeyButton: button10,
                linkedResetButton: button40);*/
            // FIRST TEN END NAMED WITH VKS
            // SECOND TEN NAMED WITH VKS
            /*AddKeybinding(data[], // <
                keyName: "Previous Unit",
                offsets: new List<long> {  },
                defaultVK: 0xBC,
                linkedTextBox: textBox11,
                linkedNewKeyButton: button11,
                linkedResetButton: button41);*/
            /*AddKeybinding(data[], // >
                keyName: "Next Unit",
                offsets: new List<long> {  },
                defaultVK: 0xBE,
                linkedTextBox: textBox12,
                linkedNewKeyButton: button12,
                linkedResetButton: button42);*/
            /*AddKeybinding(data[], // F1
                keyName: "Unit View",
                offsets: new List<long> {  },
                defaultVK: 0x70,
                linkedTextBox: textBox13,
                linkedNewKeyButton: button13,
                linkedResetButton: button43);*/
            /*AddKeybinding(data[], // F2
                keyName: "Manufacture View",
                offsets: new List<long> {  },
                defaultVK: 0x71,
                linkedTextBox: textBox14,
                linkedNewKeyButton: button14,
                linkedResetButton: button44);*/
            /*AddKeybinding(data[], // F3
                keyName: "Resource View 1",
                offsets: new List<long> {  },
                defaultVK: 0x72,
                linkedTextBox: textBox15,
                linkedNewKeyButton: button15,
                linkedResetButton: button45);*/
            /*AddKeybinding(data[], // F4
                keyName: "Resource View 2",
                offsets: new List<long> {  },
                defaultVK: 0x73,
                linkedTextBox: textBox16,
                linkedNewKeyButton: button16,
                linkedResetButton: button46);*/
            /*AddKeybinding(data[], // F5
                keyName: "Resource View 3",
                offsets: new List<long> {  },
                defaultVK: 0x74,
                linkedTextBox: textBox17,
                linkedNewKeyButton: button17,
                linkedResetButton: button47);*/
            /*AddKeybinding(data[], // M
                keyName: "Last Five Messages",
                offsets: new List<long> {  },
                defaultVK: 0x4D,
                linkedTextBox: textBox18,
                linkedNewKeyButton: button18,
                linkedResetButton: button48);*/
            /*AddKeybinding(data[], // R
                keyName: "Research Menu",
                offsets: new List<long> {  },
                defaultVK: 0x52,
                linkedTextBox: textBox19,
                linkedNewKeyButton: button19,
                linkedResetButton: button49);*/
            /*AddKeybinding(data[], // BACKSPACE
                keyName: "Stop Time",
                offsets: new List<long> {  },
                defaultVK: 0x08,
                linkedTextBox: textBox20,
                linkedNewKeyButton: button20,
                linkedResetButton: button50);*/
            // LAST TEN NAMED WITH VKS
            /*AddKeybinding(data[], // 1
                keyName: "Speed 1",
                offsets: new List<long> {  },
                defaultVK: 0x31,
                linkedTextBox: textBox21,
                linkedNewKeyButton: button21,
                linkedResetButton: button51);*/
            /*AddKeybinding(data[], // 2
                keyName: "Speed 2",
                offsets: new List<long> {  },
                defaultVK: 0x32,
                linkedTextBox: textBox22,
                linkedNewKeyButton: button22,
                linkedResetButton: button52);*/
            /*AddKeybinding(data[], // 3
                keyName: "Speed 3",
                offsets: new List<long> {  },
                defaultVK: 0x33,
                linkedTextBox: textBox23,
                linkedNewKeyButton: button23,
                linkedResetButton: button53);*/
            /*AddKeybinding(data[], // 4
                keyName: "Speed 4",
                offsets: new List<long> {  },
                defaultVK: 0x34,
                linkedTextBox: textBox24,
                linkedNewKeyButton: button24,
                linkedResetButton: button54);*/
            /*AddKeybinding(data[], // 5
                keyName: "Speed 5",
                offsets: new List<long> {  },
                defaultVK: 0x35,
                linkedTextBox: textBox25,
                linkedNewKeyButton: button25,
                linkedResetButton: button55);*/
            /*AddKeybinding(data[], // 6
                keyName: "Speed 6",
                offsets: new List<long> {  },
                defaultVK: 0x36,
                linkedTextBox: textBox26,
                linkedNewKeyButton: button26,
                linkedResetButton: button56);*/
            /*AddKeybinding(data[], // 7
                keyName: "Speed 7",
                offsets: new List<long> {  },
                defaultVK: 0x37,
                linkedTextBox: textBox27,
                linkedNewKeyButton: button27,
                linkedResetButton: button57);*/
            /*AddKeybinding(data[], // 8
                keyName: "Speed 8",
                offsets: new List<long> {  },
                defaultVK: 0x38,
                linkedTextBox: textBox28,
                linkedNewKeyButton: button28,
                linkedResetButton: button58);*/
            /*AddKeybinding(data[], // 9
                keyName: "Speed 9",
                offsets: new List<long> {  },
                defaultVK: 0x39,
                linkedTextBox: textBox29,
                linkedNewKeyButton: button29,
                linkedResetButton: button59);*/
            /*AddKeybinding(data[], // 0
                keyName: "Speed 0",
                offsets: new List<long> {  },
                defaultVK: 0x30,
                linkedTextBox: textBox30,
                linkedNewKeyButton: button30,
                linkedResetButton: button60);*/
            // LAST THREE NAMED WITH VKS
            /*AddKeybinding(data[], // SPACE
               keyName: "Cycle Resource View",
               offsets: new List<long> {  },
               defaultVK: 0x20,
               linkedTextBox: textBox33,
               linkedNewKeyButton: button67,
               linkedResetButton: button68);*/
            AddKeybinding(data[0x32D25], // LEFT CLICK
                keyName: "Left Click",
                offsets: new List<long> { 0x32D25,  // UNKNOWN              // 1117109272035918186  // LEFTMOUSE // 32D25
                    0x32D4E                         // UNKNOWN              // 8466983863904567658  // LEFTMOUSE // 32D4E
                },
                defaultVK: 0x01,
                linkedTextBox: textBox31,
                linkedNewKeyButton: button63,
                linkedResetButton: button64);
            AddKeybinding(data[0x32D32], // RIGHT CLICK
                keyName: "Right Click",
                offsets: new List<long> { 0x32D32,  // UNKNOWN              // 1117109272035918442  // RIGHTMOUSE // 32D32
                    0x32D57                         // UNKNOWN              // 8394926269866639978  // RIGHTMOUSE // 32D57
                },
                defaultVK: 0x02,
                linkedTextBox: textBox32,
                linkedNewKeyButton: button65,
                linkedResetButton: button66);
            // UNKNOWN SECTION START
            /*AddKeybinding(data[], // SHIFT // Unknown functionality with 11 references?
               keyName: "UNKNOWN",
               offsets: new List<long> { 0x2B2F9,   // UNKNOWN              // 20989574264066154    // SHIFT // 2B2F9
                   0x33391,                         // UNKNOWN              // 20989574264066154    // SHIFT // 33391
                   0x96B3A,                         // UNKNOWN              // 9444022862802260074  // SHIFT // 96B3A
                   0x7C9B8,                         // UNKNOWN              // 16723553203190960234 // SHIFT // 7C9B8
                   0x3B7A6,                         // UNKNOWN              // 13967350231206662250 // SHIFT // 3B7A6
                   0x2B709,                         // UNKNOWN              // 13966787281119023210 // SHIFT // 2B709
                   0x2B6DC,                         // UNKNOWN              // 8466983863854239850  // SHIFT // 2B6DC
                   0x2B678,                         // UNKNOWN              // 16723553203039965290 // SHIFT // 2B678
                   0x2B628,                         // UNKNOWN              // 13967913181059420266 // SHIFT // 2B628
                   0x2B5DA,                         // UNKNOWN              // 9444022862869368938  // SHIFT // 2B5DA
                   0x2B591                          // UNKNOWN              // 8394926269883420778  // SHIFT // 2B591
               },
               defaultVK: 0x10,
               linkedTextBox: textBox1, // no textbox yet
               linkedNewKeyButton: button1, // no buttons yet
               linkedResetButton: button31);*/ // no buttons yet
            // UNKNOWN SECTION END
            foreach (var (keyName, binding) in keybindings) { checkState(keyName, binding.CurrentVK); } // update state from load
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
            byte data,
            string keyName,
            List<long> offsets,
            byte defaultVK,
            TextBox linkedTextBox,
            Button linkedNewKeyButton,
            Button linkedResetButton)
        {
            keybindings[keyName] = new Keybinding
            {
                Offsets = offsets,
                DefaultVK = defaultVK,
                CurrentVK = data, // Just use the first offset as representative
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
            // Lost focus reset text
            keybindings[keyName].LinkedNewKeyButton!.LostFocus += (s, e) =>
            {
                if(keybindings[keyName].ListeningForInput)
                {
                    keybindings[keyName].LinkedNewKeyButton!.Text = "New Key";
                    keybindings[keyName].LinkedTextBox!.Text = ((Keys)keybindings[keyName].CurrentVK).ToString();
                }
            };
            // Button click event
            keybindings[keyName].LinkedNewKeyButton!.Click += (s, e) =>
            {
                keybindings[keyName].ListeningForInput = true;
                keybindings[keyName].LinkedNewKeyButton!.Text = "Press Key!";
                keybindings[keyName].LinkedTextBox!.Text = "Press a key...";
                keybindings[keyName].LinkedNewKeyButton!.Focus();
            };
            // Wire up dynamic KeyDown
            keybindings[keyName].LinkedNewKeyButton!.KeyDown += (s, e) =>
            {
                keybindings[keyName].ListeningForInput = false; // stop listening for input
                keybindings[keyName].LinkedNewKeyButton!.Text = "New Key"; // reset button text
                byte newVK = (byte)e.KeyCode; // get keycode
                if (!IsValidVirtualKey(newVK))
                {
                    ping(keyName);
                    return;
                }
                checkState(keyName, newVK); // Check and update state
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
                return $"Keybind '{keyName}' is set to an invalid key ({binding.CurrentVK}). It won't be saved.";
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
        public static bool IsValidVirtualKey(byte vk) { return (vk >= 1 && vk <= 254); }
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
        // import keybinds
        private void button69_Click(object sender, EventArgs e)
        {
            string filePath;
            using (OpenFileDialog openFileDialog = new OpenFileDialog())
            {
                openFileDialog.InitialDirectory = Directory.GetCurrentDirectory();
                openFileDialog.Filter = "Keybinding Files (*.key)|*.key";
                openFileDialog.FilterIndex = 1;
                openFileDialog.RestoreDirectory = true;
                openFileDialog.Title = "Import keybinding presets.";
                if (openFileDialog.ShowDialog() != DialogResult.OK) { return; }
                else { filePath = openFileDialog.FileName; }
            }
            byte[] loaded = File.ReadAllBytes(filePath);
            if (loaded.Length != keybindings.Count)
            {
                MessageBox.Show("Invalid keybindings file. Import aborted.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            int i = 0;
            foreach (var (keyName, binding) in keybindings) { checkState(keyName, loaded[i++]); } // check and update state
        }
        public void checkState(string keyName, byte newVK)
        {
            if (newVK == keybindings[keyName].DefaultVK) { keybindings[keyName].LinkedResetButton!.Enabled = false; }
            else { keybindings[keyName].LinkedResetButton!.Enabled = true; }
            keybindings[keyName].CurrentVK = newVK;
            keybindings[keyName].LinkedTextBox!.Text = ((Keys)newVK).ToString();
            if (keybindings.Values.Any(k => k.IsModified))
            {
                button61.Enabled = true; // Enable reset button if any key is modified
                button62.Enabled = true; // Enable save button if any key is modified
            }
            else
            {
                button61.Enabled = false; // Enable reset button if any key is modified
                button62.Enabled = false; // Disable save button if no keys are modified
            }
        }
        // export keybindings
        private void button70_Click(object sender, EventArgs e)
        {
            using (SaveFileDialog sfd = new SaveFileDialog())
            {
                sfd.Title = "Export Keybindings";
                sfd.Filter = "Keybinding Files (*.key)|*.key|All Files (*.*)|*.*";
                sfd.DefaultExt = "key";
                sfd.FileName = "bindings.key";
                if (sfd.ShowDialog() == DialogResult.OK)
                {
                    File.WriteAllBytes(sfd.FileName, keybindings.Values.Select(k => k.CurrentVK).ToArray());
                    MessageBox.Show("Keybindings exported successfully.", "Export Complete", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
            }
        }
    }
}
