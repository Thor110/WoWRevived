using Microsoft.Win32;

namespace WoWLauncher
{
    public partial class Form2 : Form
    {
        private RegistryKey mainKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000", true)!;
        private RegistryKey screenKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Screen", true)!;
        private RegistryKey battleKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\BattleMap", true)!;
        private RegistryKey researchKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Research", true)!;
        public Form2()
        {
            InitializeComponent();
            var baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32);
            InitializeRegistry();
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(PictureBox), typeof(Label), typeof(Button) });
        }
        /// This method compares the registry entry with the value and sets it if they are different.
        private void registryCompare(RegistryKey key, string entry, string value) { if ((string)key.GetValue(entry)! != value) { key.SetValue(entry, value); } }
        /// This method is called when the form is loaded to initialize the registry settings
        private void InitializeRegistry()
        {
            trackBar1.Value = Convert.ToInt32(battleKey.GetValue("Damage reduction divisor")) / 100;

            // add event handlers
            trackBar1.ValueChanged += trackBar1_ValueChanged!;
        }
        // set custom difficulty
        private void customDifficulty() { registryCompare(mainKey, "Difficulty", "Custom"); }
        // return button
        private void button1_Click(object sender, EventArgs e) { this.Close(); } // not really necessary?
        // Damage reduction divisor track bar value changed event
        private void trackBar1_ValueChanged(object sender, EventArgs e)
        {
            registryCompare(battleKey, "Damage reduction divisor", (trackBar1.Value * 100).ToString());
            customDifficulty();
        }
    }
}
