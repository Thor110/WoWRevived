using Microsoft.Win32;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace WoWLauncher
{
    public partial class Form1 : Form
    {
        private bool config; // are settings open or not
        private int resolution; // temp resolution combobox index for swapping files in future versions
        private List<string> keptResolutions = new List<string>(); // keep listed resolutions for future versions
        private RegistryKey mainKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000", true)!;
        private RegistryKey tweakKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Tweak", true)!;
        private RegistryKey screenKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Screen", true)!;
        private RegistryKey battleKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\BattleMap", true)!;
        private RegistryKey researchKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Research", true)!;
        private RegistryKey optionsKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Options", true)!;
        private RegistryKey buildListKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\BuildList", true)!;
        private RegistryKey debugKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Debug", true)!;
        private RegistryKey soundKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Sound", true)!;
        private RegistryKey volumeKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Sound\Volume", true)!;
        [StructLayout(LayoutKind.Sequential)]
        public struct DEVMODE
        {
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
            public string dmDeviceName;
            public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra;
            public int dmFields;
            public int dmPositionX, dmPositionY;
            public int dmDisplayOrientation, dmDisplayFixedOutput;
            public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
            public string dmFormName;
            public short dmLogPixels;
            public int dmBitsPerPel, dmPelsWidth, dmPelsHeight;
            public int dmDisplayFlags, dmDisplayFrequency;
        }

        [DllImport("user32.dll")]
        static extern bool EnumDisplaySettings(string deviceName, int modeNum, ref DEVMODE devMode);

        public static List<string> GetSupportedResolutions()
        {
            var resolutions = new HashSet<string>();
            DEVMODE devMode = new DEVMODE();
            devMode.dmSize = (short)Marshal.SizeOf(devMode);
            int i = 0;
            while (EnumDisplaySettings(null!, i++, ref devMode)) { resolutions.Add($"{devMode.dmPelsWidth}x{devMode.dmPelsHeight}"); }
            return resolutions.OrderBy(r => int.Parse(r.Split('x')[0])).ToList();
        }
        public static bool IsWindows11()
        {
            var os = Environment.OSVersion;
            // Both Win 10 and 11 are major version 10
            // Windows 11 is Build 22000 or higher
            return os.Platform == PlatformID.Win32NT && os.Version.Build >= 22000;
        }
        public Form1()
        {
            InitializeComponent();
            ApplyLocalization();
            if (!Utilities.IsDirectoryWritable(AppDomain.CurrentDomain.BaseDirectory))
            {
                string parentDir = Directory.GetParent(AppDomain.CurrentDomain.BaseDirectory)!.FullName;
                string localizedMessage = string.Format(Program.Interface["dir_warning"], parentDir);
                MessageBox.Show(localizedMessage, Program.Interface["dir_warning_error"], MessageBoxButtons.OK, MessageBoxIcon.Error);
                Environment.Exit(0);
            }
            // TODO : add tweak key creation below and check what happens if it doesn't exist when altering settings etc
            // TODO : or just repopulate every registry entry
            RegistryKey baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32);
            if (mainKey == null) // set default registry settings which are required for the launcher, the rest are created when the game starts.
            {
                mainKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000", true)!;
                tweakKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Tweak", true)!;
                screenKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Screen", true)!;
                battleKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\BattleMap", true)!;
                researchKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Research", true)!;
                optionsKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Options", true)!;
                buildListKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\BuildList", true)!;
                debugKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Debug", true)!;
                soundKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Sound", true)!;
                volumeKey = baseKey.CreateSubKey(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Sound\Volume", true);
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
                mainKey.SetValue("Difficulty", "Medium");
                tweakKey.SetValue("AI Aggression Value", "0.500000");
                tweakKey.SetValue("AI Invasion Threshold PC", "150.000000");
                tweakKey.SetValue("Max units in sector", "15");
                tweakKey.SetValue("Pod Interval (hours)", "24");
                tweakKey.SetValue("AI Hours Per Turn", "5");
                tweakKey.SetValue("Max boats in sector", "5");
                tweakKey.SetValue("AI strength table Human multiplier", "1.000000");
                tweakKey.SetValue("AI strength table Martian multiplier", "2.000000");
                battleKey.SetValue("EnableFogOfWar", "1");
                battleKey.SetValue("Damage reduction divisor", "500");
                researchKey.SetValue("Human Open Rate", "20");
                researchKey.SetValue("Martian Open Rate", "10");
                optionsKey.SetValue("Show lights", 1, RegistryValueKind.DWord);
                screenKey.SetValue("BPP", 16, RegistryValueKind.DWord);
                buildListKey.SetValue("Top", "70");
                debugKey.SetValue("Enemy Visible", "0");
                soundKey.SetValue("Inner Ambient border", "-640,-400");
                soundKey.SetValue("Inner border", "-320,-200");
                soundKey.SetValue("Outer Ambient border", "-960,-600");
                soundKey.SetValue("Outer border", "-640,-400");
                volumeKey.SetValue("CD-Focus", 0, RegistryValueKind.DWord);
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
            foreach (string deleteFile in deleteFiles) { if (File.Exists(deleteFile)) { File.Delete(deleteFile); } }
            // delete unnecessary directx folder and fles
            if (Directory.Exists("DIRECTX")) { Directory.Delete("DIRECTX", true); }
            if (Directory.Exists("WinSys")) { Directory.Delete("WinSys", true); }
            // delete old .smk movie files
            string[] folders = { "FMV", "FMV-Human" };
            foreach (string file in folders.Where(Directory.Exists).SelectMany(f => Directory.EnumerateFiles(f, "*.smk", SearchOption.TopDirectoryOnly))) { File.Delete(file); }
            string[] supportedResolutions = new string[]
            {
                //"512x384         (4:3)",    // Listed in original manual, ultra-low fallback - sometimes causes DDERR_NOCOOPERATIVELEVELSET
                "640x480         (4:3)",    // Classic baseline 4:3                         // Exists In-Game
                "800x600         (4:3)",    // Legacy 4:3 standard                          // Exists In-Game
                "1024x768       (4:3)",     // XGA — very common                            // Exists In-Game
                "1152x864       (4:3)",     // Slightly higher 4:3 (rare)                   // Exists In-Game
                //"1280x720       (16:9)",  // 720p — slight stretching/whiteness issue
                "1280x768       (15:9)",    // WXGA – rare variant of 1280x800 (15:9)
                "1280x800       (16:10)",   // WXGA — early widescreen laptops (16:10)      // Exists In-Game
                "1280x1024     (5:4)",      // SXGA — tall 5:4 monitor resolution
                "1360x768       (16:9)",    // 16:9 — GPU-aligned, better than 1366x768
                "1366x768       (16:9)",    // Common 16:9 laptop resolution
                // These resolutions only work on the main menu - newly expanded warmap allows these resolutions to work
                "1600x900       (16:9)",    // 16:9 — upper-mid range laptop displays
                "1600x1024     (5:4)",      // Unusual 5:4 wide — seems to pass internal checks
                //"1600x1200     (4:3)",      // UXGA — classic high-res 4:3                // DDERR_NOCOOPERATIVELEVELSET
                "1680x1050     (16:10)",    // WSXGA+ — widescreen 16:10, works well
                "1920x1080     (16:9)"      // 1080p
            };
            List<string> supported = GetSupportedResolutions();
            List<string> matchedResolutions = supportedResolutions.Where(sr => supported.Any(r => sr.Contains(r))).ToList();
            foreach (string resolution in matchedResolutions) { comboBox2.Items.Add(resolution); keptResolutions.Add(resolution.Split(' ')[0]); } // list and keep only supported resolutions for later use
            comboBox4.Items.Add(Program.Interface["easy"]);
            comboBox4.Items.Add(Program.Interface["medium"]);
            comboBox4.Items.Add(Program.Interface["hard"]);
            comboBox4.Items.Add(Program.Interface["extreme"]);
            string exePath = Path.GetFullPath("WoW_patched.exe");
            RegistryKey key = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers");
            string current = key?.GetValue(exePath) as string ?? "";
            if (!current.Contains("16BITCOLOR") || !current.Contains("WINXPSP3") && IsWindows11())
            {
                var flags = current.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries).ToList();
                if (!flags.Contains("~")) flags.Insert(0, "~");
                if (!flags.Contains("WINXPSP3") && IsWindows11()) flags.Add("WINXPSP3"); // only for W11
                if (!flags.Contains("16BITCOLOR")) flags.Add("16BITCOLOR");
                key?.SetValue(exePath, string.Join(" ", flags));
            }
            key?.Close();
            if (IsWindows11()) { EnsureDirectPlay(); }
            InitializeRegistry();
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(PictureBox), typeof(Label), typeof(Button) });
        }
        // Helper to enable DirectPlay silently
        private void EnsureDirectPlay()
        {
            using (RegistryKey? dpKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Microsoft\DirectPlay", false))
            {
                object? value = dpKey?.GetValue("Installed");
                if (value == null || (int)value != 1)
                {
                    // Trigger DISM
                    ProcessStartInfo psi = new ProcessStartInfo
                    {
                        FileName = "dism.exe",
                        Arguments = "/online /enable-feature /featurename:LegacyComponents /featurename:DirectPlay /all /norestart",
                        UseShellExecute = true,
                        WindowStyle = ProcessWindowStyle.Hidden,
                        Verb = "runas"
                    };

                    try
                    {
                        using (Process? proc = Process.Start(psi))
                        {
                            proc?.WaitForExit();
                            // --- THE FIX: Manually set the registry flag so the loop breaks ---
                            RegistryKey baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32);
                            RegistryKey newKey = baseKey.CreateSubKey(@"SOFTWARE\Microsoft\DirectPlay", true);
                            newKey.SetValue("Installed", 1, RegistryValueKind.DWord);
                            newKey?.Close();
                            // Notify the user after a successful DISM run
                            MessageBox.Show(Program.Interface["directplay_installed"],
                                            Program.Interface["directplay_title"],
                                            MessageBoxButtons.OK,
                                            MessageBoxIcon.Information);
                        }
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show("DirectPlay Automation Failed: " + ex.Message);
                    }
                }
            }
        }
        private void ApplyLocalization()
        {
            label2.TextAlign = ContentAlignment.MiddleRight;
            label4.TextAlign = ContentAlignment.MiddleRight;
            // commented are unused currently
            button1.Text = Program.Interface["start_human"];
            button2.Text = Program.Interface["start_martian"];
            button3.Text = Program.Interface["config"];
            button4.Text = Program.Interface["exit"];
            //checkBox1.AccessibleDescription = "Enable or disable multiplayer. ( This option messes with single player resume and save campaign options, careful! )";
            //checkBox1.Text = "Enable Network Version";
            checkBox2.AccessibleDescription = Program.Interface["fullscreen_description"];
            checkBox2.Text = Program.Interface["fullscreen"];
            comboBox2.AccessibleDescription = Program.Interface["resolution_description"];
            label2.Text = Program.Interface["resolution"];
            comboBox4.AccessibleDescription = Program.Interface["difficulty_description"];
            label4.Text = Program.Interface["difficulty"];
            checkBox3.AccessibleDescription = Program.Interface["fog_description"];
            checkBox3.Text = Program.Interface["fog"];
            button5.Text = Program.Interface["advanced"];
            button6.Text = Program.Interface["tools"];
            //button7.Text = Program.Interface["keyboard"];
            button8.Text = Program.Interface["instruction_button"];
            if(Program.CurrentLanguage == "German" || Program.CurrentLanguage == "Spanish") { button8.Enabled = false; }
            checkBox5.AccessibleDescription = Program.Interface["music_playback_description"];
            checkBox5.Text = Program.Interface["music_playback"];
            checkBox6.AccessibleDescription = Program.Interface["enhanced_assets_description"];
            checkBox6.Text = Program.Interface["enhanced_assets"];
            checkBox7.AccessibleDescription = Program.Interface["enemy_visible_description"];
            checkBox7.Text = Program.Interface["enemy_visible"];
            //
            Text = Program.Interface["game_name"];
        }
        /// This method compares the registry entry with the value and sets it if they are different.
        private void registryCompare(RegistryKey key, string entry, string value) { if ((string)key.GetValue(entry)! != value) { key.SetValue(entry, value); } }
        /// This method is called when the form is loaded to initialize the registry settings
        private void InitializeRegistry()
        {
            if ((int)mainKey.GetValue("Enable Network Version")! == 1) { checkBox1.Checked = true; }
            if (Convert.ToInt32(mainKey.GetValue("Full Screen")) == 1) { checkBox2.Checked = true; }
            checkBox3.Checked = Convert.ToInt32(battleKey.GetValue("EnableFogOfWar")) == 1; // for restore default settings
            checkBox5.Checked = (int)volumeKey.GetValue("CD-Focus")! == 1; //music playback when out of focus
            if (File.Exists("DAT\\cd_bd1.spr")) { checkBox6.Checked = true; } // enhanced assets enabled
            checkBox7.Checked = Convert.ToInt32(debugKey.GetValue("Enemy Visible")) == 1; // for restore default settings
            foreach (string res in comboBox2.Items)
            {
                if (res.StartsWith(((string)screenKey.GetValue("Size")!).Replace(",", "x").Split(' ')[0])) // set combobox to the registry resolution
                {
                    comboBox2.SelectedItem = res;
                    resolution = comboBox2.SelectedIndex;
                    break;
                }
            }
            // custom registry entry so it will be null once // medium by default
            if (comboBox4.Items.Count > 3) { comboBox4.Items.Remove(Program.Interface["custom"]); } // remove custom from the combo box on return
            switch ((string)mainKey.GetValue("Difficulty")!)
            {
                case "Easy": comboBox4.SelectedIndex = 0; break;
                case "Medium": comboBox4.SelectedIndex = 1; break;
                case "Hard": comboBox4.SelectedIndex = 2; break;
                case "Extreme": comboBox4.SelectedIndex = 3; break;
                case "Custom":
                    comboBox4.Items.Add(Program.Interface["custom"]);
                    comboBox4.SelectedIndex = 4;
                    break;
            }
            // force registry settings for removed options to prevent user meddling
            if (Convert.ToInt32(optionsKey.GetValue("Show lights")) != 1)
            {
                optionsKey.SetValue("Show lights", 1, RegistryValueKind.DWord);
                MessageBox.Show(Program.Interface["lights"]);
            }
            if (Convert.ToInt32(screenKey.GetValue("BPP")) != 16)
            {
                screenKey.SetValue("BPP", 16, RegistryValueKind.DWord);
                MessageBox.Show(Program.Interface["colour"]);
            }
            // add event handlers here for the checkboxes and comboboxes to prevent them firing when the form is loaded
            checkBox1.CheckedChanged += checkBox1_CheckedChanged!;
            checkBox2.CheckedChanged += checkBox2_CheckedChanged!;
            checkBox3.CheckedChanged += checkBox3_CheckedChanged!;
            comboBox2.SelectedIndexChanged += comboBox2_SelectedIndexChanged!;
            comboBox4.SelectedIndexChanged += comboBox4_SelectedIndexChanged!;
            checkBox5.CheckedChanged += checkBox5_CheckedChanged!;
            checkBox6.CheckedChanged += checkBox6_CheckedChanged!;
            checkBox7.CheckedChanged += checkBox7_CheckedChanged!;
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
            if (File.Exists("WoW_patched.exe"))
            {
                // TODO: run as administrator for future networked version
                Process proc = new Process();
                proc.StartInfo.FileName = "WoW_patched.exe";
                proc.StartInfo.UseShellExecute = true;
                proc.StartInfo.Verb = "runas";
                proc.Start();
            }
            else { MessageBox.Show(Program.Interface["executable"]); }
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
                    MessageBox.Show(Program.Interface["human_game"]);
                    return;
                }
                File.Move("FMV\\RAGELOGO.MP4", "FMV-Human\\RAGELOGO.MP4");
                File.Move("FMV\\TITLE.MP4", "FMV-Human\\TITLE.MP4");
                File.Move("MARTIAN.cd", "MARTIAN.cd.bak");
                File.Move("human.cd.bak", "human.cd");
                Directory.Move("FMV", "FMV-Martian");
                Directory.Move("FMV-Human", "FMV");
                Directory.Move("Music", "MusicMartian");
                Directory.Move("MusicHuman", "Music");
            }
            // difficulty variance testing
            switch ((string)mainKey.GetValue("Difficulty")!)
            {
                case "Easy":
                    registryCompare(tweakKey, "AI strength table Human multiplier", "1.200000");
                    registryCompare(tweakKey, "AI strength table Martian multiplier", "1.800000");
                    break;
                case "Medium":
                    registryCompare(tweakKey, "AI strength table Human multiplier", "1.000000");
                    registryCompare(tweakKey, "AI strength table Martian multiplier", "2.000000");
                    break;
                case "Hard":
                    registryCompare(tweakKey, "AI strength table Human multiplier", "0.900000");
                    registryCompare(tweakKey, "AI strength table Martian multiplier", "2.100000");
                    break;
                case "Extreme":
                    registryCompare(tweakKey, "AI strength table Human multiplier", "0.800000");
                    registryCompare(tweakKey, "AI strength table Martian multiplier", "2.200000");
                    break;
            }
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
                    MessageBox.Show(Program.Interface["martian_game"]);
                    return;
                }
                File.Move("FMV\\RAGELOGO.MP4", "FMV-Martian\\RAGELOGO.MP4");
                File.Move("FMV\\TITLE.MP4", "FMV-Martian\\TITLE.MP4");
                File.Move("human.cd", "human.cd.bak");
                File.Move("MARTIAN.cd.bak", "MARTIAN.cd");
                Directory.Move("FMV", "FMV-Human");
                Directory.Move("FMV-Martian", "FMV");
                Directory.Move("Music", "MusicHuman");
                Directory.Move("MusicMartian", "Music");
            }
            // difficulty variance testing
            switch ((string)mainKey.GetValue("Difficulty")!)
            {
                case "Easy":
                    registryCompare(tweakKey, "AI strength table Human multiplier", "0.800000");
                    registryCompare(tweakKey, "AI strength table Martian multiplier", "2.200000");
                    break;
                case "Medium":
                    registryCompare(tweakKey, "AI strength table Human multiplier", "1.000000");
                    registryCompare(tweakKey, "AI strength table Martian multiplier", "2.000000");
                    break;
                case "Hard":
                    registryCompare(tweakKey, "AI strength table Human multiplier", "1.100000");
                    registryCompare(tweakKey, "AI strength table Martian multiplier", "1.900000");
                    break;
                case "Extreme":
                    registryCompare(tweakKey, "AI strength table Human multiplier", "1.200000");
                    registryCompare(tweakKey, "AI strength table Martian multiplier", "1.800000");
                    break;
            }
            launchGame();
        }
        /// This is the event handler for the "Configuration Settings" button
        private void button3_Click(object sender, EventArgs e)
        {
            button1.Visible = false;
            button2.Visible = false;
            button3.Visible = false;
            config = true;
            button4.Text = Program.Interface["return"];
            //checkBox1.Visible = true;     // network enabled
            checkBox2.Visible = true;
            checkBox3.Visible = true;
            checkBox5.Visible = true;
            checkBox6.Visible = true;
            checkBox7.Visible = true;
            comboBox2.Visible = true;
            comboBox4.Visible = true;
            label2.Visible = true;
            label4.Visible = true;
            button5.Visible = true;
            button6.Visible = false;
            //button7.Visible = false;      // keyboard shortcut remapper cancelled
            button8.Visible = false;
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
                button4.Text = Program.Interface["exit"];
                //checkBox1.Visible = false;    // network enabled
                checkBox2.Visible = false;
                checkBox3.Visible = false;
                checkBox5.Visible = false;
                checkBox6.Visible = false;
                checkBox7.Visible = false;
                comboBox2.Visible = false;
                comboBox4.Visible = false;
                label2.Visible = false;
                label4.Visible = false;
                button5.Visible = false;
                button6.Visible = true;
                //button7.Visible = true;   // keyboard shortcut remapper cancelled
                button8.Visible = true;
            }
            else { Close(); }
        }
        private void checkBox1_CheckedChanged(object sender, EventArgs e) { mainKey.SetValue("Enable Network Version", checkBox1.Checked ? 1 : 0); }
        private void checkBox2_CheckedChanged(object sender, EventArgs e)
        {
            if(checkBox2.Checked)
            {
                File.Move("ddraw-off.dll", "ddraw.dll");
                mainKey.SetValue("Full Screen", "1");
            }
            else
            {
                File.Move("ddraw.dll", "ddraw-off.dll");
                mainKey.SetValue("Full Screen", "0");
            }
        }
        private void checkBox3_CheckedChanged(object sender, EventArgs e) { battleKey.SetValue("EnableFogOfWar", checkBox3.Checked ? "1" : "0"); }
        // resolution combobox
        private void comboBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (resolution == comboBox2.SelectedIndex) { return; }
            string screenSize = comboBox2.SelectedItem!.ToString()!.Replace("x", ",").Split(' ')[0]; // convert to the format used in the registry
            registryCompare(screenKey, "Size", screenSize);                 // "Size" is the in-game resolution
            registryCompare(screenKey, "Support screen size", screenSize);  // "Support screen size" is the resolution used by the main menu
            registryCompare(buildListKey, "Top", ((Int32.Parse(screenSize.Split(',')[1]) - 340) / 2).ToString()); // build list resolution adjustment
            string soundOne = screenSize.Split(",")[0];                 // x string
            string soundTwo = screenSize.Split(",")[1];                 // y string
            int soundThree = Int32.Parse(screenSize.Split(',')[0]);     // x int
            int soundFour = Int32.Parse(screenSize.Split(',')[1]);      // y int
            string reusedSound = "-" + soundOne + ",-" + (soundFour - 80).ToString();
            registryCompare(soundKey, "Inner Ambient border", reusedSound);
            registryCompare(soundKey, "Inner border", "-" + (soundThree / 2).ToString() + ",-" + (soundFour / 2 - 40).ToString());
            registryCompare(soundKey, "Outer Ambient border", "-" + (soundThree + 320).ToString() + ",-" + (soundFour + 120).ToString());
            registryCompare(soundKey, "Outer border", reusedSound);
            // future prep for moving resolution specific assets back and forth
            string[] moveFiles = new string[] { "humanbd.spr", "legal1.spr", "legal2.spr", "martbd.spr" };
            foreach (string file in moveFiles)
            {
                File.Move("DAT\\" + file, $"DAT-EXTRA\\{keptResolutions[resolution]}\\" + file); // move from DAT to storage
                File.Move($"DAT-EXTRA\\{keptResolutions[comboBox2.SelectedIndex]}\\" + file, "DAT\\" + file); // move from storage to DAT
            }
            // larger resolution warmap files
            string[] resolutionFiles = new string[] { "HWM.SPR", "HWMHI.SPR", "MWM.SPR", "MWMHI.SPR" };
            if (screenSize.Split(",")[0] == "1600" || screenSize.Split(",")[0] == "1680" || screenSize.Split(",")[0] == "1920")
            {
                if (File.Exists("DAT\\MWM.SPR") && !File.Exists("DAT\\NORM-MWM.SPR")) // high resolutions check
                {
                    File.Move("DAT\\MWM.SPR", $"DAT\\NORM-MWM.SPR"); // just for MWM - EUROPE
                    foreach (string file in resolutionFiles)
                    {
                        if (File.Exists("DAT\\TEMP-" + file))
                        {
                            File.Move("DAT\\TEMP-" + file, $"DAT\\" + file);
                        }
                    }
                }
            }
            else
            {
                if (File.Exists("DAT\\NORM-MWM.SPR") && !File.Exists("DAT\\TEMP-MWM.SPR")) // low resolutions check
                {
                    foreach (string file in resolutionFiles)
                    {
                        if (File.Exists("DAT\\" + file))
                        {
                            File.Move("DAT\\" + file, $"DAT\\TEMP-" + file);
                        }
                    }
                    File.Move("DAT\\NORM-MWM.SPR", $"DAT\\MWM.SPR"); // just for MWM - EUROPE
                }
            }
            resolution = comboBox2.SelectedIndex; // update resolution tracker
        }
        private void comboBox4_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBox4.Text == (string)mainKey.GetValue("Difficulty")!) { return; }
            switch (comboBox4.SelectedIndex)
            {
                case 0:
                    registryCompare(mainKey, "Difficulty", "Easy");
                    registryCompare(battleKey, "Damage reduction divisor", "400");
                    // new settings
                    registryCompare(tweakKey, "AI Aggression Value", "0.400000");
                    registryCompare(tweakKey, "AI Invasion Threshold PC", "200.000000");
                    registryCompare(tweakKey, "Max units in sector", "15");
                    // custom settings reset
                    registryCompare(researchKey, "Human Open Rate", "20");
                    registryCompare(researchKey, "Martian Open Rate", "10");
                    registryCompare(tweakKey, "Pod Interval (hours)", "24");
                    registryCompare(tweakKey, "AI Hours Per Turn", "5");
                    registryCompare(tweakKey, "Max boats in sector", "5");
                    break;
                case 1:
                    registryCompare(mainKey, "Difficulty", "Medium");
                    registryCompare(battleKey, "Damage reduction divisor", "500");
                    // new settings
                    registryCompare(tweakKey, "AI Aggression Value", "0.500000");
                    registryCompare(tweakKey, "AI Invasion Threshold PC", "150.000000");
                    registryCompare(tweakKey, "Max units in sector", "15");
                    // custom settings reset
                    registryCompare(researchKey, "Human Open Rate", "20");
                    registryCompare(researchKey, "Martian Open Rate", "10");
                    registryCompare(tweakKey, "Pod Interval (hours)", "24");
                    registryCompare(tweakKey, "AI Hours Per Turn", "5");
                    registryCompare(tweakKey, "Max boats in sector", "5");
                    break;
                case 2:
                    registryCompare(mainKey, "Difficulty", "Hard");
                    registryCompare(battleKey, "Damage reduction divisor", "600");
                    // new settings
                    registryCompare(tweakKey, "AI Aggression Value", "0.900000");
                    registryCompare(tweakKey, "AI Invasion Threshold PC", "100.000000");
                    registryCompare(tweakKey, "Max units in sector", "25");
                    // custom settings reset
                    registryCompare(researchKey, "Human Open Rate", "20");
                    registryCompare(researchKey, "Martian Open Rate", "10");
                    registryCompare(tweakKey, "Pod Interval (hours)", "24");
                    registryCompare(tweakKey, "AI Hours Per Turn", "5");
                    registryCompare(tweakKey, "Max boats in sector", "5");
                    break;
                case 3:
                    registryCompare(mainKey, "Difficulty", "Extreme");
                    registryCompare(battleKey, "Damage reduction divisor", "700");
                    // new settings
                    registryCompare(tweakKey, "AI Aggression Value", "1.000000");
                    registryCompare(tweakKey, "AI Invasion Threshold PC", "90.000000");
                    registryCompare(tweakKey, "Max units in sector", "30");
                    // custom settings reset
                    registryCompare(researchKey, "Human Open Rate", "20");
                    registryCompare(researchKey, "Martian Open Rate", "10");
                    registryCompare(tweakKey, "Pod Interval (hours)", "24");
                    registryCompare(tweakKey, "AI Hours Per Turn", "5");
                    registryCompare(tweakKey, "Max boats in sector", "5");
                    break;
                case 4:
                    return; // do nothing if custom is selected
            }
            if (comboBox4.Items.Count > 3) { comboBox4.Items.Remove(Program.Interface["custom"]); } // remove custom from the combo box
        }
        // open advanced settings
        private void button5_Click(object sender, EventArgs e) { newForm(new Form2()); }
        // open editor
        private void button6_Click(object sender, EventArgs e)
        {
            if (!File.Exists("WoWViewer.exe"))
            {
                MessageBox.Show(Program.Interface["editor"]);
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
            //checkBox4.CheckedChanged -= checkBox4_CheckedChanged!;
            checkBox5.CheckedChanged -= checkBox5_CheckedChanged!;
            checkBox6.CheckedChanged -= checkBox6_CheckedChanged!;
            checkBox7.CheckedChanged -= checkBox7_CheckedChanged!;
            //comboBox1.SelectedIndexChanged -= comboBox1_SelectedIndexChanged!;    // language settings not supported
            comboBox2.SelectedIndexChanged -= comboBox2_SelectedIndexChanged!;
            //comboBox3.SelectedIndexChanged -= comboBox3_SelectedIndexChanged!;     // game frequency is not supported
            comboBox4.SelectedIndexChanged -= comboBox4_SelectedIndexChanged!;
            form.FormClosed += (s, args) => this.Show();
            form.FormClosed += (s, args) => InitializeRegistry();
            form.Move += (s, args) => { if (this.Location != form.Location) { this.Location = form.Location; } };
        }
        // music focus checkbox
        private void checkBox5_CheckedChanged(object sender, EventArgs e) { volumeKey.SetValue("CD-Focus", checkBox5.Checked ? 1 : 0, RegistryValueKind.DWord); }
        // enable or disable hd upscale
        private string[] upscaledFiles = new string[] {
            "cd_bd1.spr", "cd_bd2.spr", "cd_bd3.spr", "cd_bd4.spr", "cd_bd5.spr", "cd_bd6.spr", "cd_bd7.spr",
            "HBBdown.spr", "HBBup.spr", "HBTdown.spr", "HBTup.spr", "HBuildB1.spr", "hexitarr.spr", "Hmessage.spr",
            "hmframe.spr", "hu-cnt24.spr", "hu-cnt32.spr", "hu-cnt48.spr", "hu_buton.spr", "hu_calen.spr",
            "hu_event.spr", "hu_speed.spr", "hu_twirl.spr", "MA-CNT24.SPR", "MA-CNT32.SPR", "MA-CNT48.SPR", "MAN-BUT.SPR",
            "MA_BUTON.SPR", "MA_CALEN.SPR", "MA_EVENT.SPR", "ma_goo.spr", "MA_SPEED.SPR", "MA_WORLD.SPR", "MBBDOWN.SPR",
            "MBBUP.SPR", "MBTDOWN.SPR", "MBTUP.SPR", "MBUILDB1.SPR", "MBUILDB2.SPR", "MCOG.SPR", "mexitarr.spr", "MFACT.SPR",
            "MMFRAME.SPR", "MRESRCHB.SPR", "mr_exit.spr", "MR_TAB.SPR", "MUNITS.SPR",
            "RBHAbndn.spr", "RBHAggro.spr", "RBHattk.spr", "RBHBsrk.spr", "RBHbuild.spr", "RBHChkn.spr", "RBHcncl.spr", "RBHcnclM.spr",
            "RBHCtrlB.spr", "RBHFrze.spr", "RBHgo.spr", "RBHInfl.spr", "RBHinfo.spr", "RBHPulse.spr", "RBHrsrch.spr", "RBHRtlt.spr",
            "RBHrtrt.spr", "RBHrturn.spr", "RBHScare.spr", "RBHstop.spr", "RBHsusp.spr", "RBHtnnl.spr", "RBHvwmap.spr", "RBHXplsv.spr",
            "RBMAbndn.spr", "RBMAggro.spr", "RBMATTK.SPR", "RBMBDUST.SPR", "RBMBsrk.spr", "RBMBUILD.SPR", "RBMChkn.spr", "RBMCNCL.SPR",
            "RBMcnclM.spr", "RBMCtrlB.spr", "RBMFrze.spr", "RBMgo.spr", "RBMHALT.SPR", "RBMinflt.spr", "RBMINFO.SPR", "RBMPulse.spr",
            "RBMRtlt.spr", "RBMRTRT.SPR", "RBMRTURN.SPR", "RBMSBOMB.SPR", "RBMscan.spr", "RBMScare.spr", "RBMstop.spr", "RBMsusp.spr",
            "RBMVWMAP.SPR", "RES-BUT.SPR", "rese-but.spr", "RP_Bio.spr", "RP_Blcks.spr", "RP_bm.spr", "RP_Cnstr.spr", "RP_Comms.spr",
            "RP_cons.spr", "RP_Coppr.spr", "RP_DM.spr", "RP_DR.spr", "RP_el.spr", "RP_EWP.spr", "RP_Farm.spr", "RP_FLM.spr", "RP_fm.spr",
            "RP_HElmn.spr", "RP_hm.spr", "RP_HRT.spr", "RP_HX.spr", "RP_mods.spr", "RP_Obs.spr", "RP_Power.spr", "RP_Proj.spr", "RP_Rapid.spr",
            "RP_Repr.spr", "RP_scn.spr", "RP_sm.spr", "RP_Susp.spr", "RP_T.spr", "RP_TC.spr", "RP_xt.spr", "UNIT-BUT.SPR",
        };
        private void checkBox6_CheckedChanged(object sender, EventArgs e)
        {
            if (checkBox6.Checked) { foreach (string file in upscaledFiles) { File.Move($"DAT-EXTRA\\HD\\{file}", $"DAT\\{file}"); } }
            else { foreach (string file in upscaledFiles) { File.Move($"DAT\\{file}", $"DAT-EXTRA\\HD\\{file}"); } }
        }
        // Debug "Enemy Visible" value determines if enemy units are visible on the warmap
        private void checkBox7_CheckedChanged(object sender, EventArgs e) { registryCompare(debugKey, "Enemy Visible", checkBox7.Checked ? "1" : "0"); }
        // open game manual
        private void button8_Click(object sender, EventArgs e)
        {
            MessageBox.Show(Program.Interface["instruction_readme"]);
            ProcessStartInfo psi = new ProcessStartInfo { FileName = Program.Interface["instruction_location"], UseShellExecute = true };
            Process.Start(psi);
        }
    }
}