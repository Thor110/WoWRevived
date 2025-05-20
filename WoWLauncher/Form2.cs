using Microsoft.Win32;

namespace WoWLauncher
{
    public partial class Form2 : Form
    {
        private bool config;
        private RegistryKey mainKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000", true)!;
        private RegistryKey tweakKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Tweak", true)!;
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
        private void registryCompare(RegistryKey key, string entry, string value) { if ((string)key.GetValue(entry)! != value) { key.SetValue(entry, value); config = true; } }
        /// This method is called when the form is loaded to initialize the registry settings
        private void InitializeRegistry()
        {
            trackBar1.Value = Convert.ToInt32(battleKey.GetValue("Damage reduction divisor")) / 100;
            trackBar2.Value = Convert.ToInt32(tweakKey.GetValue("Max units in sector"));
            trackBar3.Value = Convert.ToInt32(tweakKey.GetValue("Max boats in sector"));
            trackBar4.Value = Convert.ToInt32(researchKey.GetValue("Human Open Rate"));
            trackBar5.Value = Convert.ToInt32(researchKey.GetValue("Martian Open Rate"));
            trackBar6.Value = Convert.ToInt32(tweakKey.GetValue("Pod Interval (hours)"));
            trackBar7.Value = Convert.ToInt32(tweakKey.GetValue("AI Hours Per Turn"));
        }
        // return button saves the settings and closes the form
        private void button1_Click(object sender, EventArgs e)
        {
            registryCompare(battleKey, "Damage reduction divisor", (trackBar1.Value * 100).ToString());
            registryCompare(tweakKey, "Max units in sector", trackBar2.Value.ToString());
            registryCompare(tweakKey, "Max boats in sector", trackBar3.Value.ToString());
            registryCompare(researchKey, "Human Open Rate", trackBar4.Value.ToString());
            registryCompare(researchKey, "Martian Open Rate", trackBar5.Value.ToString());
            registryCompare(tweakKey, "Pod Interval (hours)", trackBar6.Value.ToString());
            registryCompare(tweakKey, "AI Hours Per Turn", trackBar7.Value.ToString());
            if (config) { registryCompare(mainKey, "Difficulty", "Custom"); }
            this.Close();
        }
    }
}
