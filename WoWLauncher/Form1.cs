using Microsoft.Win32;
using System.Diagnostics;

namespace WoWLauncher
{
    public partial class Form1 : Form
    {
        private bool config;
        private RegistryKey mainKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000", true)!;
        private RegistryKey tweakKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Tweak", true)!;
        private RegistryKey screenKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Screen", true)!;
        private RegistryKey battleKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\BattleMap", true)!;
        private RegistryKey researchKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Research", true)!;
        public Form1()
        {
            InitializeComponent();
            var baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32);
            // TODO : add tweak key creation below and check what happens if it doesn't exist when altering settings etc
            if (mainKey == null) // set default registry settings which are required for the launcher, the rest are created when the game starts.
            {
                mainKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000", true)!;
                screenKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Screen", true)!;
                battleKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\BattleMap", true)!;
                researchKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Research", true)!;
                // these values are set because the launcher accesses them.
                mainKey.SetValue("Enable Network Version", 0, RegistryValueKind.DWord);
                mainKey.SetValue("Full Screen", "1");
                mainKey.SetValue("Language", "English");
                screenKey.SetValue("Size", "640,480");
                screenKey.SetValue("Support screen size", "640,480");
                mainKey.SetValue("Game Frequency", "30");
                // these values are set to default values, required for the game to run correctly.
                mainKey.SetValue("Minimum Audible Volume", "0.050000");
                mainKey.SetValue("Sleeper Enable", "0");
                mainKey.SetValue("Thread Enable", "1");
                mainKey.SetValue("Timer Enable", "0");
                // default difficulty settings
                battleKey.SetValue("EnableFogOfWar", "1");
                battleKey.SetValue("Damage reduction divisor", "500");
                // alternatively I could create whatever values we use here
                MessageBox.Show("Registry entry missing, base registry entries recreated from scratch.\n\nPlease run the game once to create the rest of the registry entries.");
            }
            registryCompare(mainKey, "CD Path", AppDomain.CurrentDomain.BaseDirectory); // update the cd path in the registry automatically.
            registryCompare(mainKey, "Install Path", AppDomain.CurrentDomain.BaseDirectory); // update the install path in the registry automatically.
            // automatically clear up the unnecessary files
            string[] deleteFiles = new string[]
            {
                "_INST32I.EX_", "_ISDEL.EXE", "_SETUP.DLL", "_sys1.cab", "_user1.cab", "Autoexec.exe", "Autorun.exe", "Autorun.inf",
                "CMS16.DLL", "cms32_95.dll", "CMS32_NT.DLL", "DATA.TAG", "data1.cab", "dsetup.dll", "dsetup16.dll", "dsetup32.dll",
                "ENGLISH.cd", "lang.dat", "layout.bin", "os.dat", "README.TXT", "SETUP.EXE", "SETUP.INI", "setup.ins", "setup.lid",
                "WoW.exe", "WOWStart.exe"
            };
            foreach (string deleteFile in deleteFiles) { if(File.Exists(deleteFile)) { File.Delete(deleteFile); } }
            // delete unnecessary directx folder and fles
            if (Directory.Exists("DIRECTX")) { Directory.Delete("DIRECTX", true); }
            // check for smackw32.dll
            if (!File.Exists("Smackw32.dll"))
            {
                if (File.Exists("WinSys\\Smackw32.dll"))
                {
                    File.Move("WinSys\\Smackw32.dll", "Smackw32.dll");
                    Directory.Delete("WinSys", true);
                }
                else
                {
                    MessageBox.Show("Smackw32.dll is missing, what did you do with it?\n\nThe game will fail to run without Smackw32.dll get it back off the disc...");
                    Close();
                }
            }
            /*
            // Dynamic language pack detection, which can only go wrong if the user goes renaming files or changing the registry.
            comboBox1.Items.Add((string)mainKey.GetValue("Language")!); // DEFAULT TEXT.OJD = Language set in Registry ( this could go haywire if the user changes the language in the registry )
            var ojdFiles = Directory.GetFiles($"{Directory.GetCurrentDirectory()}", "*.OJD", SearchOption.TopDirectoryOnly);
            foreach (string currentFile in ojdFiles)
            {
                string fileName = Path.GetFileNameWithoutExtension(currentFile);
                if (fileName.Contains("TEXT") && fileName != "TEXT") // Ignore the current file
                {
                    switch (fileName.Substring(5))
                    {
                        case "DE":
                        {
                            comboBox1.Items.Add("German");
                        }
                        break;
                        case "ES":
                        {
                            comboBox1.Items.Add("Spanish");
                        }
                        break;
                        case "FR":
                        {
                            comboBox1.Items.Add("French");
                        }
                        break;
                        case "IT":
                        {
                            comboBox1.Items.Add("Italian");
                        }
                        break;
                        case "EN":
                        {
                            comboBox1.Items.Add("English");
                        }
                        break;
                    }
                }
            }
            comboBox1.SelectedIndex = 0; // set selected index to the current language as per the registry
            */


            string[] supportedResolutions = new string[]
            {
                //"512x384         (4:3)",    // Listed in original manual, ultra-low fallback - sometimes causes DDERR_NOCOOPERATIVELEVELSET
                "640x480         (4:3)",    // Classic baseline 4:3
                "800x600         (4:3)",    // Legacy 4:3 standard
                "1024x768       (4:3)",     // XGA — very common
                "1152x864       (4:3)",     // Slightly higher 4:3 (rare)
                //"1280x720       (16:9)",  // 720p — slight stretching/whiteness issue
                "1280x768       (15:9)",    // WXGA – rare variant of 1280x800 (15:9)
                "1280x800       (16:10)",   // WXGA — early widescreen laptops (16:10)
                "1280x1024     (5:4)",      // SXGA — tall 5:4 monitor resolution
                "1360x768       (16:9)",    // 16:9 — GPU-aligned, better than 1366x768
                "1366x768       (16:9)",    // Common 16:9 laptop resolution
                // These resolutions only work on the main menu
                //"1600x900       (16:9)",    // 16:9 — upper-mid range laptop displays
                //"1600x1024     (5:4)",      // Unusual 5:4 wide — seems to pass internal checks
                //"1600x1200     (4:3)",      // UXGA — classic high-res 4:3
                //"1680x1050     (16:10)",    // WSXGA+ — widescreen 16:10, works well
            };
            foreach (string res in supportedResolutions) { comboBox2.Items.Add(res); }
            //comboBox3.Items.Add("30"); // should probably not support this option
            //comboBox3.Items.Add("60");     // game frequency is not supported
            //comboBox3.Items.Add("120");
            //comboBox3.Items.Add("240");
            comboBox4.Items.Add("Easy");
            comboBox4.Items.Add("Medium");
            comboBox4.Items.Add("Hard");
            InitializeRegistry();
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(PictureBox), typeof(Label), typeof(Button) });
        }
        /// This method compares the registry entry with the value and sets it if they are different.
        private void registryCompare(RegistryKey key, string entry, string value) { if ((string)key.GetValue(entry)! != value) { key.SetValue(entry, value); } }
        /// This method is called when the form is loaded to initialize the registry settings
        private void InitializeRegistry()
        {
            if ((int)mainKey.GetValue("Enable Network Version")! == 1) { checkBox1.Checked = true; }
            if (Convert.ToInt32(mainKey.GetValue("Full Screen")) == 1) { checkBox2.Checked = true; }
            if (Convert.ToInt32(battleKey.GetValue("EnableFogOfWar")) == 1) { checkBox3.Checked = true; }
            if (Convert.ToInt32(screenKey.GetValue("AllowResize")) == 1) { checkBox4.Checked = true; }
            foreach (string res in comboBox2.Items)
            {
                if (res.StartsWith(((string)screenKey.GetValue("Size")!).Replace(",", "x").Split(' ')[0])) // check if the resolution is supported
                {
                    comboBox2.SelectedItem = res;
                    break;
                }
            }
            //comboBox3.SelectedItem = (string)mainKey.GetValue("Game Frequency")!;     // game frequency is not supported
            // custom registry entry so it will be null once // medium by default
            switch ((string)mainKey.GetValue("Difficulty")!)
            {
                case null:
                    mainKey.SetValue("Difficulty", "Medium");
                    comboBox4.SelectedIndex = 1;
                    break;
                case "Easy":
                    comboBox4.SelectedIndex = 0;
                    break;
                case "Medium":
                    comboBox4.SelectedIndex = 1;
                    break;
                case "Hard":
                    comboBox4.SelectedIndex = 2;
                    break;
                case "Custom":
                    comboBox4.Items.Add("Custom");
                    comboBox4.SelectedIndex = 3;
                    break;
            }
            // add event handlers here for the checkboxes and comboboxes to prevent them firing when the form is loaded
            checkBox1.CheckedChanged += checkBox1_CheckedChanged!;
            checkBox2.CheckedChanged += checkBox2_CheckedChanged!;
            checkBox3.CheckedChanged += checkBox3_CheckedChanged!;
            checkBox4.CheckedChanged += checkBox4_CheckedChanged!;
            comboBox1.SelectedIndexChanged += comboBox1_SelectedIndexChanged!;
            comboBox2.SelectedIndexChanged += comboBox2_SelectedIndexChanged!;
            //comboBox3.SelectedIndexChanged += comboBox3_SelectedIndexChanged!;     // game frequency is not supported
            comboBox4.SelectedIndexChanged += comboBox4_SelectedIndexChanged!;
        }
        /// <summary>
        /// InitializeTooltips prepares a tooltip for every control in the form.
        /// </summary>
        /// <remarks>
        /// Uses excludedControlTypes to exclude certain types of controls from displaying tooltips.
        /// </remarks>
        private void launchGame()
        {
            // safety check for anyone who may decide to use the original executable and to check either exists
            if (File.Exists("WoW_patched.exe")) { Process.Start("WoW_patched.exe"); }
            else if (File.Exists("WoW.exe")) { Process.Start("WoW.exe"); }
            else { MessageBox.Show("Executable not found, please reinstall the game and follow the instructions."); }
            Close();
        }
        /// This is the event handler for the "Start Human Game" button
        private void button1_Click(object sender, EventArgs e)
        {
            if (File.Exists("MARTIAN.cd") && Directory.Exists("FMV-Human") && Directory.Exists("MusicHuman")) // only swap files if martian is enabled and human is disabled
            {
                // double check if the human game is installed // prevent exceptions if these files do not exist
                if (!File.Exists("human.cd.bak") || !Directory.Exists("FMV"))
                {
                    MessageBox.Show("Human game not installed, please reinstall the game and follow the instructions.");
                    return;
                }
                File.Move("MARTIAN.cd", "MARTIAN.cd.bak");
                File.Move("human.cd.bak", "human.cd");
                Directory.Move("FMV", "FMV-Martian");
                Directory.Move("FMV-Human", "FMV");
                Directory.Move("Music", "MusicMartian");
                Directory.Move("MusicHuman", "Music");
            }
            // research variance testing
            string openRate = "20"; // default
            switch ((string)mainKey.GetValue("Difficulty")!)
            {
                case "Easy":
                    openRate = "25";
                    break;
                case "Medium":
                    openRate = "20"; // default
                    break;
                case "Hard":
                    openRate = "15";
                    break;
                case "Custom":
                    launchGame();
                    return; // do not set the open rate if custom is selected
            }
            registryCompare(researchKey, "Human Open Rate", openRate);
            registryCompare(researchKey, "Martian Open Rate", "10"); // default
            launchGame();
        }
        /// This is the event handler for the "Start Martian Game" button
        private void button2_Click(object sender, EventArgs e)
        {
            if (File.Exists("human.cd") && Directory.Exists("FMV-Martian") && Directory.Exists("MusicMartian")) // only swap files if human is enabled and martian is disabled
            {
                // double check if the martian game is installed // prevent exceptions if these files do not exist
                if (!File.Exists("MARTIAN.cd.bak") || !Directory.Exists("FMV"))
                {
                    MessageBox.Show("Martian game not installed, please reinstall the game and follow the instructions.");
                    return;
                }
                File.Move("human.cd", "human.cd.bak");
                File.Move("MARTIAN.cd.bak", "MARTIAN.cd");
                Directory.Move("FMV", "FMV-Human");
                Directory.Move("FMV-Martian", "FMV");
                Directory.Move("Music", "MusicHuman");
                Directory.Move("MusicMartian", "Music");
            }
            // research variance testing
            string openRate = "10"; // default
            switch ((string)mainKey.GetValue("Difficulty")!)
            {
                case "Easy":
                    openRate = "15";
                    break;
                case "Medium":
                    openRate = "10"; // default
                    break;
                case "Hard":
                    openRate = "5";
                    break;
                case "Custom":
                    launchGame();
                    return; // do not set the open rate if custom is selected
            }
            registryCompare(researchKey, "Martian Open Rate", openRate);
            registryCompare(researchKey, "Human Open Rate", "20"); // default
            launchGame();
        }
        /// This is the event handler for the "Configuration Settings" button
        private void button3_Click(object sender, EventArgs e)
        {
            button1.Visible = false;
            button2.Visible = false;
            button3.Visible = false;
            config = true;
            button4.Text = "Back";
            checkBox1.Visible = true;
            checkBox2.Visible = true;
            checkBox3.Visible = true;
            checkBox4.Visible = true;
            comboBox2.Visible = true;
            //comboBox3.Visible = true;     // game frequency is not supported
            comboBox4.Visible = true;
            if (comboBox1.Items.Count > 1)
            {
                comboBox1.Visible = true;
                label1.Visible = true;
            }
            label2.Visible = true;
            //label3.Visible = true;     // game frequency is not supported
            label4.Visible = true;
            button5.Visible = true;
            button6.Visible = false;
            button7.Visible = false;
        }
        /// This is the event handler for the "Exit" button
        private void button4_Click(object sender, EventArgs e)
        {
            if (config)
            {
                button1.Visible = true;
                button2.Visible = true;
                button3.Visible = true;
                config = false;
                button4.Text = "Exit";
                checkBox1.Visible = false;
                checkBox2.Visible = false;
                checkBox3.Visible = false;
                checkBox4.Visible = false;
                comboBox1.Visible = false;
                comboBox2.Visible = false;
                //comboBox3.Visible = false;     // game frequency is not supported
                comboBox4.Visible = false;
                label1.Visible = false;
                label2.Visible = false;
                //label3.Visible = false;     // game frequency is not supported
                label4.Visible = false;
                button5.Visible = false;
                button6.Visible = true;
                button7.Visible = true;
            }
            else { Close(); }
        }
        private void checkBox1_CheckedChanged(object sender, EventArgs e) { mainKey.SetValue("Enable Network Version", checkBox1.Checked ? 1 : 0); }
        private void checkBox2_CheckedChanged(object sender, EventArgs e)
        {
            mainKey.SetValue("Full Screen", checkBox2.Checked ? "1" : "0");
            string exePath = Path.GetFullPath("WoW_patched.exe");
            RegistryKey key = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers");
            string existing = key?.GetValue(exePath) as string ?? "";
            if (checkBox2.Checked && existing.Contains("16BITCOLOR")) // Set the registry key to enable compatibility mode for 16-bit color
            {
                string updated = string.Join(" ", existing!.Split(new[] { ' ' }).Where(flag => !string.Equals(flag, "16BITCOLOR")));
                if (string.IsNullOrWhiteSpace(updated) || updated == "~") { key?.DeleteValue(exePath, false); } // because windows adds this ~
                else { key?.SetValue(exePath, updated); } // retain any other user options
            }
            else if (!checkBox2.Checked && !existing.Contains("16BITCOLOR")) // Add the compatibility mode setting if unchecked
            {
                string updated = existing + " " + "16BITCOLOR"; // ensures "16BITCOLOR" is recorded as a string literal and gets interned by the compiler
                key?.SetValue(exePath, updated);
            }
            key?.Close();
        }
        private void checkBox3_CheckedChanged(object sender, EventArgs e) { battleKey.SetValue("EnableFogOfWar", checkBox3.Checked ? "1" : "0"); }
        private void checkBox4_CheckedChanged(object sender, EventArgs e) { screenKey.SetValue("AllowResize", checkBox4.Checked ? "1" : "0"); }
        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            return; // forget language switching, save space for the end user.
            if (comboBox1.Text == (string)mainKey.GetValue("Language")!) { return; } // do nothing if the same language is selected
            switch (comboBox1.Text)
            {
                case "German":
                {
                    MessageBox.Show("TEST");
                }
                break;
                case "Spanish":
                {
                    MessageBox.Show("TEST");
                }
                break;
                case "French":
                {
                    MessageBox.Show("TEST");
                }
                break;
                case "Italian":
                {
                    MessageBox.Show("TEST");
                }
                break;
                case "English":
                {
                    MessageBox.Show("TEST");
                }
                break;
            }


            return;
            string rename = (string)mainKey.GetValue("Language")!; // Default = English
            if (comboBox1.Text == rename) { return; } // do nothing if the same language is selected
            if (File.Exists($"{comboBox1.Text.ToUpper()}.ojd") && File.Exists("TEXT.ojd")) // double check if the files exist
            {
                File.Move("TEXT.ojd", $"{rename.ToUpper()}.ojd"); // rename the current language file
                File.Move($"{comboBox1.Text.ToUpper()}.ojd", $"TEXT.ojd"); // rename the new language file
                mainKey.SetValue("Language", comboBox1.SelectedItem!.ToString()!); // update the registry
            }
            else // can only happen if the user has deleted, renamed or moved the language files, or messed with the registry.
            {
                MessageBox.Show("Language file not found, please reinstall the game and follow the instructions.");
                return; // do nothing if the files do not exist
            }
        }
        private void comboBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBox2.Text == screenKey.GetValue("Size")!.ToString()!.Replace(",", "x").Split(' ')[0]) { return; }
            string screenSize = comboBox2.SelectedItem!.ToString()!.Replace("x", ",").Split(' ')[0]; // convert to the format used in the registry
            registryCompare(screenKey, "Size", screenSize);                 // "Size" is the in-game resolution
            registryCompare(screenKey, "Support screen size", screenSize);  // "Support screen size" is the resolution used by the main menu
        }
        /*private void comboBox3_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBox3.Text == (string)mainKey.GetValue("Game Frequency")!) { return; }
            mainKey.SetValue("Game Frequency", comboBox3.SelectedItem!.ToString()!);
        }*/
        private void comboBox4_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBox4.Text == (string)mainKey.GetValue("Difficulty")!) { return; }
            switch (comboBox4.SelectedItem!.ToString())
            {
                case "Easy":
                    registryCompare(mainKey, "Difficulty", "Easy");
                    registryCompare(battleKey, "Damage reduction divisor", "400");
                    break;
                case "Medium":
                    registryCompare(mainKey, "Difficulty", "Medium");
                    registryCompare(battleKey, "Damage reduction divisor", "500");
                    break;
                case "Hard":
                    registryCompare(mainKey, "Difficulty", "Hard");
                    registryCompare(battleKey, "Damage reduction divisor", "600");
                    break;
                case "Custom":
                    return; // do nothing if custom is selected
            } // reset the custom settings to default
            registryCompare(tweakKey, "Max units in sector", "15");
            registryCompare(tweakKey, "Max boats in sector", "5");
            registryCompare(tweakKey, "Pod Interval (hours)", "24");
            registryCompare(tweakKey, "AI Hours Per Turn", "5");
            if (comboBox4.Items.Count > 2) { comboBox4.Items.Remove("Custom"); } // remove custom from the combo box
        }
        // open advanced settings
        private void button5_Click(object sender, EventArgs e) { newForm(new Form2()); }
        // open editor
        private void button6_Click(object sender, EventArgs e)
        {
            if (!File.Exists("WoWViewer.exe"))
            {
                MessageBox.Show("Editor not found, please reinstall the game and follow the instructions.");
                return;
            }
            Process.Start("WoWViewer.exe");
            Close();
        }
        // open keyboard shortcuts form
        private void button7_Click(object sender, EventArgs e) { newForm(new KeyboardShortcutsForm()); }
        // create new form method
        private void newForm(Form form)
        {
            form.StartPosition = FormStartPosition.Manual;
            form.Location = this.Location;
            form.Show();
            this.Hide();
            checkBox1.CheckedChanged -= checkBox1_CheckedChanged!;
            checkBox2.CheckedChanged -= checkBox2_CheckedChanged!;
            checkBox3.CheckedChanged -= checkBox3_CheckedChanged!;
            checkBox4.CheckedChanged -= checkBox4_CheckedChanged!;
            comboBox1.SelectedIndexChanged -= comboBox1_SelectedIndexChanged!;
            comboBox2.SelectedIndexChanged -= comboBox2_SelectedIndexChanged!;
            //comboBox3.SelectedIndexChanged -= comboBox3_SelectedIndexChanged!;     // game frequency is not supported
            comboBox4.SelectedIndexChanged -= comboBox4_SelectedIndexChanged!;
            form.FormClosed += (s, args) => this.Show();
            form.FormClosed += (s, args) => InitializeRegistry();
            form.Move += (s, args) => { if (this.Location != form.Location) { this.Location = form.Location; } };
        }
    }
}