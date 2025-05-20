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
                MessageBox.Show("Registry entry missing, base registry entries recreated from scratch.\nPlease run the game once to create the rest of the registry entries.");
            }
            registryCompare(mainKey, "CD Path", Directory.GetCurrentDirectory().ToString()); // update the cd path in the registry automatically.
            registryCompare(mainKey, "Install Path", Directory.GetCurrentDirectory().ToString()); // update the install path in the registry automatically.
            // Dynamic language pack detection
            comboBox1.Items.Add((string)mainKey.GetValue("Language")!); // DEFAULT TEXT.OJD = Language set in Registry ( this could go haywire if the user changes the language in the registry )
            var ojdFiles = Directory.GetFiles($"{Directory.GetCurrentDirectory()}", "*.OJD", SearchOption.TopDirectoryOnly);
            foreach (string currentFile in ojdFiles)
            {
                string fileName = Path.GetFileNameWithoutExtension(currentFile);
                if (fileName != "TEXT" && fileName != "AI" && fileName != "OBJ" && fileName != "SFX") // Ignore the default files
                {
                    comboBox1.Items.Add(fileName.Substring(0, 1).ToUpper() + fileName.Substring(1).ToLower());
                }
            }
            comboBox1.SelectedIndex = 0; // default to the current language
            comboBox2.Items.Add("640x480");
            comboBox2.Items.Add("800x600");
            comboBox2.Items.Add("1024x768");
            comboBox2.Items.Add("1280x1024");
            comboBox3.Items.Add("30");
            comboBox3.Items.Add("60");
            comboBox3.Items.Add("120");
            comboBox3.Items.Add("240");
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
            comboBox2.SelectedItem = ((string)screenKey.GetValue("Size")!).Replace(",", "x");
            comboBox3.SelectedItem = (string)mainKey.GetValue("Game Frequency")!;
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
            comboBox3.SelectedIndexChanged += comboBox3_SelectedIndexChanged!;
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
            else { MessageBox.Show("Executable not found, please reinstall the and follow the instructions."); }
            Close();
        }
        /// This is the event handler for the "Start Human Game" button
        private void button1_Click(object sender, EventArgs e)
        {
            if (File.Exists("MARTIAN.cd") && Directory.Exists("FMV-Human")) // only swap files if martian is enabled and human is disabled
            {
                // double check if the human game is installed // prevent exceptions if these files do not exist
                if (!File.Exists("human.cd.bak") || !Directory.Exists("FMV"))
                {
                    MessageBox.Show("Human game not installed! Please install the Human game first.");
                    return;
                }
                File.Move("MARTIAN.cd", "MARTIAN.cd.bak");
                File.Move("human.cd.bak", "human.cd");
                Directory.Move("FMV", "FMV-Martian");
                Directory.Move("FMV-Human", "FMV");
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
            if (File.Exists("human.cd") && Directory.Exists("FMV-Martian")) // only swap files if human is enabled and martian is disabled
            {
                // double check if the martian game is installed // prevent exceptions if these files do not exist
                if (!File.Exists("MARTIAN.cd.bak") || !Directory.Exists("FMV"))
                {
                    MessageBox.Show("Martian game not installed! Please install the Martian game first.");
                    return;
                }
                File.Move("human.cd", "human.cd.bak");
                File.Move("MARTIAN.cd.bak", "MARTIAN.cd");
                Directory.Move("FMV", "FMV-Human");
                Directory.Move("FMV-Martian", "FMV");
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
            comboBox2.Visible = true;
            comboBox3.Visible = true;
            comboBox4.Visible = true;
            if (comboBox1.Items.Count > 1)
            {
                comboBox1.Visible = true;
                label1.Visible = true;
            }
            label2.Visible = true;
            label3.Visible = true;
            label4.Visible = true;
            button5.Visible = true;
            button6.Visible = false;
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
                comboBox1.Visible = false;
                comboBox2.Visible = false;
                comboBox3.Visible = false;
                comboBox4.Visible = false;
                label1.Visible = false;
                label2.Visible = false;
                label3.Visible = false;
                label4.Visible = false;
                button5.Visible = false;
                button6.Visible = true;
            }
            else { Close(); }
        }
        private void checkBox1_CheckedChanged(object sender, EventArgs e) { mainKey.SetValue("Enable Network Version", checkBox1.Checked ? 1 : 0); }
        private void checkBox2_CheckedChanged(object sender, EventArgs e) { mainKey.SetValue("Full Screen", checkBox2.Checked ? "1" : "0"); }
        private void checkBox3_CheckedChanged(object sender, EventArgs e) { battleKey.SetValue("EnableFogOfWar", checkBox3.Checked ? "1" : "0"); }
        private void checkBox4_CheckedChanged(object sender, EventArgs e) { screenKey.SetValue("AllowResize", checkBox4.Checked ? "1" : "0"); }
        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            string rename = (string)mainKey.GetValue("Language")!; // Default = English
            if (comboBox1.Text == rename) { return; } // do nothing if the same language is selected
            File.Move("TEXT.ojd", $"{rename.ToUpper()}.ojd"); // rename the current language file
            File.Move($"{comboBox1.Text.ToUpper()}.ojd", $"TEXT.ojd"); // rename the new language file
            mainKey.SetValue("Language", comboBox1.SelectedItem!.ToString()!); // update the registry
        }
        private void comboBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBox2.Text == screenKey.GetValue("Size")!.ToString()!.Replace(",", "x")) { return; }
            string screenSize = comboBox2.SelectedItem!.ToString()!.Replace("x", ",");
            registryCompare(screenKey, "Size", screenSize);
            registryCompare(screenKey, "Support screen size", screenSize);
        }
        private void comboBox3_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBox3.Text == (string)mainKey.GetValue("Game Frequency")!) { return; }
            mainKey.SetValue("Game Frequency", comboBox3.SelectedItem!.ToString()!);
        }
        private void comboBox4_SelectedIndexChanged(object sender, EventArgs e)
        {
            if(comboBox4.Text == (string)mainKey.GetValue("Difficulty")!) { return; }
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
        private void button5_Click(object sender, EventArgs e)
        {
            Form advanced = new Form2();
            advanced.StartPosition = FormStartPosition.Manual;
            advanced.Location = this.Location;
            advanced.Show();
            this.Hide();
            checkBox1.CheckedChanged -= checkBox1_CheckedChanged!;
            checkBox2.CheckedChanged -= checkBox2_CheckedChanged!;
            checkBox3.CheckedChanged -= checkBox3_CheckedChanged!;
            checkBox4.CheckedChanged -= checkBox4_CheckedChanged!;
            comboBox1.SelectedIndexChanged -= comboBox1_SelectedIndexChanged!;
            comboBox2.SelectedIndexChanged -= comboBox2_SelectedIndexChanged!;
            comboBox3.SelectedIndexChanged -= comboBox3_SelectedIndexChanged!;
            comboBox4.SelectedIndexChanged -= comboBox4_SelectedIndexChanged!;
            advanced.FormClosed += (s, args) => this.Show();
            advanced.FormClosed += (s, args) => InitializeRegistry();
        }
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
    }
}