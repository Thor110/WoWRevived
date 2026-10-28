using IniParser;
using Microsoft.Win32;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace WoWLauncher
{
    public partial class Form1 : Form
    {
        private IniFile iniFile = new IniFile("ddraw.ini");
        private string[] supportedResolutions = new string[]
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
        private bool config; // are settings open or not
        private int resolution; // temp resolution combobox index for swapping files in future versions
        private int resolutionStored; // for overclocked resolution checks......
        private List<string> keptResolutions = new List<string>(); // keep listed resolutions for future versions
        private RegistryKey mainKey = GameRegistry.Open()!;
        private RegistryKey tweakKey = GameRegistry.Open(@"Tweak")!;
        private RegistryKey screenKey = GameRegistry.Open(@"Screen")!;
        private RegistryKey battleKey = GameRegistry.Open(@"BattleMap")!;
        private RegistryKey researchKey = GameRegistry.Open(@"Research")!;
        private RegistryKey optionsKey = GameRegistry.Open(@"Options")!;
        private RegistryKey buildListKey = GameRegistry.Open(@"BuildList")!;
        private RegistryKey debugKey = GameRegistry.Open(@"Debug")!;
        private RegistryKey soundKey = GameRegistry.Open(@"Sound")!;
        private RegistryKey volumeKey = GameRegistry.Open(@"Sound\Volume")!;
        private RegistryKey systemKey = GameRegistry.Open(@"System")!;
        private RegistryKey networkKey = GameRegistry.Open(@"Network")!;
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
            if (File.Exists("Human.cd") && File.Exists("MARTIAN.cd"))
            {
                File.Delete("Human.cd");
                DeleteRegistryFolder(@"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'");
                mainKey = null!;
            }
            // TODO : add tweak key creation below and check what happens if it doesn't exist when altering settings etc
            // TODO : or just repopulate every registry entry
            if (mainKey == null) // set default registry settings which are required for the launcher, the rest are created when the game starts.
            {
                mainKey = GameRegistry.Create();
                tweakKey = GameRegistry.Create(@"Tweak");
                screenKey = GameRegistry.Create(@"Screen");
                battleKey = GameRegistry.Create(@"BattleMap");
                researchKey = GameRegistry.Create(@"Research");
                optionsKey = GameRegistry.Create(@"Options");
                buildListKey = GameRegistry.Create(@"BuildList");
                debugKey = GameRegistry.Create(@"Debug");
                soundKey = GameRegistry.Create(@"Sound");
                volumeKey = GameRegistry.Create(@"Sound\Volume");
                systemKey = GameRegistry.Create(@"System");
                networkKey = GameRegistry.Create(@"Network");
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
                // memory limits
                systemKey.SetValue("Game Memory", "32");//
                systemKey.SetValue("Game Memory Handles", "32767");
                systemKey.SetValue("Renderer Memory", "32");
                systemKey.SetValue("Renderer Memory Handles", "32767");
                // custom cd focus registry entry
                volumeKey.SetValue("CD-Focus", 0, RegistryValueKind.DWord);
            }
            // the main key can exist without some of its subkeys (e.g. Sound\Volume before the game first exits), create any that are missing
            tweakKey ??= GameRegistry.Create("Tweak");
            screenKey ??= GameRegistry.Create("Screen");
            battleKey ??= GameRegistry.Create("BattleMap");
            researchKey ??= GameRegistry.Create("Research");
            optionsKey ??= GameRegistry.Create("Options");
            buildListKey ??= GameRegistry.Create("BuildList");
            debugKey ??= GameRegistry.Create("Debug");
            soundKey ??= GameRegistry.Create("Sound");
            volumeKey ??= GameRegistry.Create(@"Sound\Volume");
            systemKey ??= GameRegistry.Create("System");
            networkKey ??= GameRegistry.Create("Network");
            GameRegistry.RemoveLegacyUsersFullControl(); // undo the permissions older WoWViewer versions granted
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
            if (File.Exists("Human.cd") && File.Exists("MARTIAN.cd")) { File.Delete("Human.cd"); }
            // delete unnecessary directx folder and fles
            if (Directory.Exists("DIRECTX")) { Directory.Delete("DIRECTX", true); }
            if (Directory.Exists("WinSys")) { Directory.Delete("WinSys", true); }
            // delete old .smk movie files
            string[] folders = { "FMV" };
            foreach (string file in folders.Where(Directory.Exists).SelectMany(f => Directory.EnumerateFiles(f, "*.smk", SearchOption.TopDirectoryOnly))) { File.Delete(file); }
            List<string> supported = GetSupportedResolutions();
            List<string> matchedResolutions = supportedResolutions.Where(sr => supported.Any(r => sr.Contains(r))).ToList();
            foreach (string resolution in matchedResolutions) { comboBox2.Items.Add(resolution); keptResolutions.Add(resolution.Split(' ')[0]); } // list and keep only supported resolutions for later use
            // overclocked resolutions
            List<string> dynamicOverclocked = supported.Where(osRes => !supportedResolutions.Any(safeRes => safeRes.StartsWith(osRes))).ToList();
            foreach (string rawRes in dynamicOverclocked)
            {
                // Filter to only show resolutions actually "Above 1080p" to keep the list clean
                var dims = rawRes.Split('x');
                if (int.TryParse(dims[0], out int w) && int.TryParse(dims[1], out int h))
                {
                    if (w > 1920 || h > 1080)
                    {
                        // Calculate Aspect Ratio dynamically for the label
                        string ratio = GetAspectRatio(w, h);
                        string space = "    ";
                        if (rawRes.Length > 8)
                        {
                            int length = rawRes.Length - 8;
                            for (int i = 0; i < length; i++)
                            {
                                space += " ";
                            }
                        }
                        string entry = $"{rawRes}{space}({ratio})";
                        comboBox2.Items.Add(entry);
                    }
                }
            }
            // normal code
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
            RepairAssetLayout();
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(PictureBox), typeof(Label), typeof(Button) });
        }
        // The Euclidean Algorithm to find the Greatest Common Divisor
        private int GetGCD(int a, int b)
        {
            while (b != 0)
            {
                int temp = b;
                b = a % b;
                a = temp;
            }
            return a;
        }

        private string GetAspectRatio(int width, int height)
        {
            int gcd = GetGCD(width, height);
            int ratioW = width / gcd;
            int ratioH = height / gcd;

            // Special case: 8:5 is commonly referred to as 16:10 in marketing
            if (ratioW == 8 && ratioH == 5) return "16:10";

            return $"{ratioW}:{ratioH}";
        }
        // Helper to enable DirectPlay silently
        private void EnsureDirectPlay()
        {
            using (RegistryKey baseView = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32))
            using (RegistryKey? dpKey = baseView.OpenSubKey(@"SOFTWARE\Microsoft\DirectPlay", false))
            {
                object? value = dpKey?.GetValue("Installed");
                if (value == null || Convert.ToInt32(value) != 1)
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
                            using RegistryKey newKey = baseView.CreateSubKey(@"SOFTWARE\Microsoft\DirectPlay", true);
                            newKey.SetValue("Installed", 1, RegistryValueKind.DWord);
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
            if (Program.CurrentLanguage == "German" || Program.CurrentLanguage == "Spanish") { button8.Enabled = false; }
            checkBox5.AccessibleDescription = Program.Interface["music_playback_description"];
            checkBox5.Text = Program.Interface["music_playback"];
            checkBox6.AccessibleDescription = Program.Interface["enhanced_assets_description"];
            checkBox6.Text = Program.Interface["enhanced_assets"];
            checkBox7.AccessibleDescription = Program.Interface["enemy_visible_description"];
            checkBox7.Text = Program.Interface["enemy_visible"];
            // larger minimaps
            checkBox4.AccessibleDescription = Program.Interface["larger_minimaps_tooltip"];
            checkBox4.Text = Program.Interface["larger_minimaps"];
            //
            Text = Program.Interface["game_name"];
        }
        /// This method compares the registry entry with the value and sets it if they are different.
        /// The value keeps the type it already has (the network build stores some values as DWORDs, the vanilla build as strings).
        private void registryCompare(RegistryKey key, string entry, string value)
        {
            object? current = key.GetValue(entry);
            if (current?.ToString() == value) { return; }
            if (current is int && int.TryParse(value, out int number)) { key.SetValue(entry, number, RegistryValueKind.DWord); }
            else { key.SetValue(entry, value); }
        }
        /// This method is called when the form is loaded to initialize the registry settings
        private void InitializeRegistry()
        {
            if (Convert.ToInt32(mainKey.GetValue("Enable Network Version") ?? 0) == 1) { checkBox1.Checked = true; }
            if (Convert.ToInt32(mainKey.GetValue("Full Screen")) == 1) { checkBox2.Checked = true; }
            checkBox3.Checked = Convert.ToInt32(battleKey.GetValue("EnableFogOfWar")) == 1; // for restore default settings
            checkBox5.Checked = Convert.ToInt32(volumeKey.GetValue("CD-Focus") ?? 0) == 1; //music playback when out of focus
            checkBox6.Checked = AssetLayout.IsEnhancedActive(File.Exists); // enhanced assets enabled
            checkBox7.Checked = Convert.ToInt32(debugKey.GetValue("Enemy Visible")) == 1; // for restore default settings
            // overclocked resolutions
            string realRes = (screenKey.GetValue("Size") as string ?? "640,480").Replace(",", "x").Split(' ')[0];
            string width = iniFile.Read("width", "ddraw");
            if (width != "0")
            {
                string height = iniFile.Read("height", "ddraw");
                // get overclocked resolution
                realRes = $"{width}x{height}";
            }
            foreach (string res in comboBox2.Items)
            {
                if (res.Split(' ')[0] == realRes) // set combobox to the registry resolution
                {
                    comboBox2.SelectedItem = res; // set combobox to selected resolution
                    resolutionStored = comboBox2.SelectedIndex;
                    string closestMatch = supportedResolutions.OrderBy(res => Math.Abs(int.Parse(res.Split('x')[0]) - int.Parse(realRes.Split('x')[0]))).First();
                    string screenSize = closestMatch.Split(' ')[0].Replace("x", ","); // e.g., "1920x1080"
                    resolution = resolutionHelper(screenSize, ",", "x"); // set resolution to actual resolution
                    break;
                }
            }
            // custom registry entry so it will be null once // medium by default
            if (comboBox4.Items.Count > 3) { comboBox4.Items.Remove(Program.Interface["custom"]); } // remove custom from the combo box on return
            switch ((mainKey.GetValue("Difficulty") as string))
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
            checkBox4.Checked = AssetLayout.IsLargerActive(File.Exists); // larger minimaps enabled
            // add event handlers here for the checkboxes and comboboxes to prevent them firing when the form is loaded
            checkBox1.CheckedChanged += checkBox1_CheckedChanged!;
            checkBox2.CheckedChanged += checkBox2_CheckedChanged!;
            checkBox3.CheckedChanged += checkBox3_CheckedChanged!;
            checkBox4.CheckedChanged += checkBox4_CheckedChanged!;
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
            if (File.Exists("WoW_patched.exe") && !checkBox1.Checked)
            {
                launch("WoW_patched.exe");
            }
            else if (File.Exists("WoW_network.exe") && checkBox1.Checked)
            {
                launch("WoW_network.exe");
            }
            else { MessageBox.Show(Program.Interface["executable"]); }
            Close();
            void launch(string executable)
            {
                Process.Start("explorer.exe", executable);
                /*
                // TODO: run as administrator for future networked version
                Process proc = new Process();
                proc.StartInfo.FileName = executable;
                proc.StartInfo.UseShellExecute = true;
                proc.StartInfo.Verb = "runas";
                proc.Start();
                */
            }
        }

        // function referenced from Riccardo Bassilichi : https://stackoverflow.com/questions/32250244/delete-a-registry-key-using-c-sharp
        // Deletes the real HKLM key AND its UAC-virtualized shadow copy under HKCU,
        // if one exists. A prior non-elevated write can leave settings living only
        // in the shadow copy (HKCU\...\VirtualStore\MACHINE\...) - deleting just the
        // real key leaves that shadow fully intact, and a subsequent "fresh" install
        // keeps transparently reading the old, stale values through it. Clearing
        // both closes that gap. Safe to call even if virtualization was never
        // active - the shadow path just won't exist, and DeleteSubKeyTree with
        // throwOnMissingSubKey: false is a no-op in that case.
        public static void DeleteRegistryFolder(string fullPathKeyToDelete)
        {
            using (var baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32))
            {
                baseKey.DeleteSubKeyTree(fullPathKeyToDelete, throwOnMissingSubKey: false);
            }

            string virtualStorePath = @"Software\Classes\VirtualStore\MACHINE\" + fullPathKeyToDelete;
            using (var userKey = RegistryKey.OpenBaseKey(RegistryHive.CurrentUser, RegistryView.Registry32))
            {
                userKey.DeleteSubKeyTree(virtualStorePath, throwOnMissingSubKey: false);
            }
        }
        /// This is the event handler for the "Start Human Game" button
        private void button1_Click(object sender, EventArgs e)
        {
            if (File.Exists("MARTIAN.cd")) { File.Move("MARTIAN.cd", "Human.cd"); }
            // difficulty variance testing
            switch ((mainKey.GetValue("Difficulty") as string))
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
            if (File.Exists("Human.cd")) { File.Move("Human.cd", "MARTIAN.cd"); }
            // difficulty variance testing
            switch ((mainKey.GetValue("Difficulty") as string))
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
            checkBox1.Visible = true;   // network enabled
            checkBox2.Visible = true;
            checkBox3.Visible = true;
            checkBox4.Visible = true;   // larger minimaps
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
                checkBox1.Visible = false;  // network enabled
                checkBox2.Visible = false;
                checkBox3.Visible = false;
                checkBox4.Visible = false;  // larger minimaps
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
        // Values the network executable reads as DWORDs and the vanilla executable reads as strings, with their defaults.
        // The vanilla executable starts at 320x240 if it finds DWORDs here, so they are converted whenever the version changes.
        private (Func<RegistryKey> Key, string Name, int Default)[] VersionTypedValues() => new (Func<RegistryKey>, string, int)[]
        {
            (() => mainKey, "Thread Enable", 1),
            (() => mainKey, "Timer Enable", 0),
            (() => mainKey, "Sleeper Enable", 0),
            (() => mainKey, "Full Screen", 1),
            (() => battleKey, "EnableFogOfWar", 1),
            (() => debugKey, "Enemy Visible", 0),
            (() => debugKey, "Charlie", 0),
            (() => debugKey, "Dave", 0),
            (() => debugKey, "Mat", 0),
            (() => debugKey, "Edit functionality", 0),
            (() => debugKey, "Load scenery", 1),
            (() => debugKey, "Arrow scenery load fix", 1),
            (() => debugKey, "Show Accelerations", 0),
            (() => debugKey, "Show Radii", 0),
            (() => debugKey, "Show Arrival", 0),
            (() => debugKey, "Show Orders", 0),
            (() => networkKey, "Messages Guaranteed", 0),
            (() => screenKey, "AllowResize", 0),
            (() => screenKey, "FixedAspect", 1),
            (() => tweakKey, "AI Oracle", 0),
        };
        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            mainKey.SetValue("Enable Network Version", checkBox1.Checked ? 1 : 0);
            foreach (var (key, name, fallback) in VersionTypedValues())
            {
                object? raw = key().GetValue(name); // missing or unreadable values get their default instead of throwing
                if (checkBox1.Checked)
                {
                    int value = raw is int number ? number : int.TryParse(raw as string, out int parsed) ? parsed : fallback;
                    key().SetValue(name, value, RegistryValueKind.DWord);
                }
                else
                {
                    string text = raw is int number ? number.ToString() : raw as string ?? fallback.ToString();
                    key().SetValue(name, text, RegistryValueKind.String);
                }
            }
            /* // only other value that varies or is introduced, not relevant.
[HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Debug]
"Log File"="wow.log"
             */
        }
        private void checkBox2_CheckedChanged(object sender, EventArgs e)
        {
            var move = checkBox2.Checked ? ("ddraw-off.dll", "ddraw.dll") : ("ddraw.dll", "ddraw-off.dll");
            if (!MoveFilesOrRevert(new[] { move }, checkBox2, checkBox2_CheckedChanged!)) { return; }
            registryCompare(mainKey, "Full Screen", checkBox2.Checked ? "1" : "0"); // keeps the DWORD type while the network version is enabled
        }
        private void checkBox3_CheckedChanged(object sender, EventArgs e) { registryCompare(battleKey, "EnableFogOfWar", checkBox3.Checked ? "1" : "0"); }
        private int resolutionHelper(string resolution, string from, string to)
        {
            string baseResMatch = resolution.Replace(from, to); // "1920x1080"
            for (int i = 0; i < comboBox2.Items.Count; i++)
            {
                if (comboBox2.Items[i]!.ToString()!.StartsWith(baseResMatch))
                {
                    return i; // Found the asset index
                }
            }
            MessageBox.Show("RESOLUTION HELPER FAIL");
            return 0; // should never fire
        }
        // resolution combobox
        private void comboBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (resolutionStored == comboBox2.SelectedIndex) { return; }
            string screenSize = comboBox2.SelectedItem!.ToString()!.Replace("x", ",").Split(' ')[0]; // convert to the format used in the registry
            // overclocked resolutions........
            int nextResolution = comboBox2.SelectedIndex;
            // dynamic overclocked resolution
            string[] dimensions = screenSize.Split(',');
            int width = int.Parse(dimensions[0]);
            int height = int.Parse(dimensions[1]);
            bool overclocked = width > 1920 || height > 1080;
            if (overclocked)
            {
                // Find the original base resolution's index so asset swapping uses the correct DAT-EXTRA folder
                // Get the aspect ratio of the selected overclocked resolution
                string targetRatio = GetAspectRatio(width, height); // e.g., "16:10"
                // Find the closest match that has the same aspect ratio, OR fallback to closest width if none match
                string closestMatch = supportedResolutions
                    .OrderBy(res => {
                        var parts = res.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
                        string resRatio = parts.Length > 1 ? parts[1].Trim('(', ')') : "";

                        // Prioritize aspect ratio matches by giving them a massive distance reduction
                        int ratioPenalty = (resRatio == targetRatio) ? 0 : 100000;
                        int widthDiff = Math.Abs(int.Parse(parts[0].Split('x')[0]) - width);

                        return widthDiff + ratioPenalty;
                    })
                    .First();
                screenSize = closestMatch.Split(' ')[0].Replace("x", ","); // e.g., "1920x1080"
                nextResolution = resolutionHelper(screenSize, ",", "x");
            }
            // gather every file move first and make them all at once, so a missing file can't leave assets half swapped
            var moves = new List<(string From, string To)>();
            if (keptResolutions[resolution] != keptResolutions[nextResolution])
            {
                foreach (string file in new[] { "humanbd.spr", "legal1.spr", "legal2.spr", "martbd.spr" })
                {
                    moves.Add(($"DAT\\{file}", $"DAT-EXTRA\\{keptResolutions[resolution]}\\{file}")); // move from DAT to storage
                    moves.Add(($"DAT-EXTRA\\{keptResolutions[nextResolution]}\\{file}", $"DAT\\{file}")); // move from storage to DAT
                }
            }
            // larger resolution warmap files
            string baseWidth = screenSize.Split(',')[0];
            string[] resolutionFiles = new string[] { "HWM.SPR", "HWMHI.SPR", "MWM.SPR", "MWMHI.SPR" };
            if (baseWidth == "1600" || baseWidth == "1680" || baseWidth == "1920")
            {
                if (File.Exists("DAT\\MWM.SPR") && !File.Exists("DAT\\NORM-MWM.SPR")) // high resolutions check
                {
                    moves.Add(("DAT\\MWM.SPR", "DAT\\NORM-MWM.SPR")); // just for MWM - EUROPE
                    foreach (string file in resolutionFiles)
                    {
                        if (File.Exists("DAT\\TEMP-" + file)) { moves.Add(("DAT\\TEMP-" + file, "DAT\\" + file)); }
                    }
                }
            }
            else if (File.Exists("DAT\\NORM-MWM.SPR") && !File.Exists("DAT\\TEMP-MWM.SPR")) // low resolutions check
            {
                foreach (string file in resolutionFiles)
                {
                    if (File.Exists("DAT\\" + file)) { moves.Add(("DAT\\" + file, "DAT\\TEMP-" + file)); }
                }
                moves.Add(("DAT\\NORM-MWM.SPR", "DAT\\MWM.SPR")); // just for MWM - EUROPE
            }
            if (!FileMoves.TryMoveAll(moves, out string? error))
            {
                ShowMoveError(error);
                comboBox2.SelectedIndexChanged -= comboBox2_SelectedIndexChanged!;
                comboBox2.SelectedIndex = resolutionStored; // put the selection back, nothing has changed
                comboBox2.SelectedIndexChanged += comboBox2_SelectedIndexChanged!;
                return;
            }
            resolutionStored = comboBox2.SelectedIndex;
            resolution = nextResolution;
            if (overclocked)
            {
                MessageBox.Show(Program.Interface["overclocked"] + " (Above 1920x1080)", Program.Interface["config"], MessageBoxButtons.OK, MessageBoxIcon.Information);
                iniFile.Write("width", $"{dimensions[0]}", "ddraw");
                iniFile.Write("height", $"{dimensions[1]}", "ddraw");
            }
            else
            {
                iniFile.Write("width", "0", "ddraw"); // used to check if resolution is overclocked on startup
                iniFile.Write("height", "0", "ddraw");
            }
            registryCompare(screenKey, "Size", screenSize);                 // "Size" is the in-game resolution
            registryCompare(screenKey, "Support screen size", screenSize);  // "Support screen size" is the resolution used by the main menu
            registryCompare(buildListKey, "Top", ((height - 340) / 2).ToString()); // build list resolution adjustment
            //parse once
            string soundOne = screenSize.Split(",")[0];                     // x string
            string soundTwo = screenSize.Split(",")[1];                     // y string
            int soundThree = Int32.Parse(soundOne);                         // x int
            int soundFour = Int32.Parse(soundTwo);                          // y int
            string reusedSound = "-" + soundOne + ",-" + (soundFour - 80).ToString();
            registryCompare(soundKey, "Inner Ambient border", reusedSound);
            registryCompare(soundKey, "Inner border", "-" + (soundThree / 2).ToString() + ",-" + (soundFour / 2 - 40).ToString());
            registryCompare(soundKey, "Outer Ambient border", "-" + (soundThree + 320).ToString() + ",-" + (soundFour + 120).ToString());
            registryCompare(soundKey, "Outer border", reusedSound);
        }
        private void comboBox4_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBox4.Text == (mainKey.GetValue("Difficulty") as string)) { return; }
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
            checkBox4.CheckedChanged -= checkBox4_CheckedChanged!;
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
        // enable or disable the enhanced user interface assets
        private void checkBox6_CheckedChanged(object sender, EventArgs e) { ApplyAssetLayout(checkBox6, checkBox6_CheckedChanged!); }
        // Debug "Enemy Visible" value determines if enemy units are visible on the warmap
        private void checkBox7_CheckedChanged(object sender, EventArgs e) { registryCompare(debugKey, "Enemy Visible", checkBox7.Checked ? "1" : "0"); }
        // open game manual
        private void button8_Click(object sender, EventArgs e)
        {
            MessageBox.Show(Program.Interface["instruction_readme"]);
            ProcessStartInfo psi = new ProcessStartInfo { FileName = Program.Interface["instruction_location"], UseShellExecute = true };
            Process.Start(psi);
        }
        // larger minimaps checkbox
        private void checkBox4_CheckedChanged(object sender, EventArgs e) { ApplyAssetLayout(checkBox4, checkBox4_CheckedChanged!); }
        // move the enhanced asset and larger minimap files to match both checkboxes, see AssetLayout for the rules
        private void ApplyAssetLayout(CheckBox changed, EventHandler handler)
        {
            var moves = AssetLayout.PlanMoves(checkBox6.Checked, checkBox4.Checked, File.Exists);
            MoveFilesOrRevert(moves, changed, handler);
        }
        // fix installs left half swapped by older launcher versions, only if every move can be made
        private void RepairAssetLayout()
        {
            var moves = AssetLayout.PlanMoves(checkBox6.Checked, checkBox4.Checked, File.Exists);
            if (moves.Count > 0 && FileMoves.Validate(moves, File.Exists) == null) { FileMoves.TryMoveAll(moves, out _); }
        }
        // make every move or none, and untick/retick the checkbox if they couldn't be made
        private bool MoveFilesOrRevert(IReadOnlyList<(string From, string To)> moves, CheckBox checkBox, EventHandler handler)
        {
            if (FileMoves.TryMoveAll(moves, out string? error)) { return true; }
            ShowMoveError(error);
            checkBox.CheckedChanged -= handler;
            checkBox.Checked = !checkBox.Checked;
            checkBox.CheckedChanged += handler;
            return false;
        }
        private static void ShowMoveError(string? error)
        {
            MessageBox.Show("The game files could not be moved, so nothing has been changed.\n\n" + error +
                "\n\nRe-extracting the latest release over your installation should restore any missing files.",
                "File Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }
}