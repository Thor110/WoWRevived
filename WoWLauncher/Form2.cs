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
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(Label), typeof(Button) });
        }
        /// This method compares the registry entry with the value and sets it if they are different.
        private void registryCompare(RegistryKey key, string entry, string value) { if ((string)key.GetValue(entry)! != value) { key.SetValue(entry, value); config = true; } }
        /// This method is called when the form is loaded to initialize the registry settings
        private void InitializeRegistry()
        {
            int actualValue = Convert.ToInt32(battleKey.GetValue("Damage reduction divisor"));
            int adjustedValue = actualValue / 100;
            trackBar1.Value = adjustedValue;
            label8.Text = actualValue.ToString();
            int unitsValue = Convert.ToInt32(tweakKey.GetValue("Max units in sector"));
            trackBar2.Value = unitsValue;
            label9.Text = unitsValue.ToString();
            int boatsValue = Convert.ToInt32(tweakKey.GetValue("Max boats in sector"));
            trackBar3.Value = boatsValue;
            label10.Text = boatsValue.ToString();
            int humanOpenRate = Convert.ToInt32(researchKey.GetValue("Human Open Rate"));
            trackBar4.Value = humanOpenRate;
            label11.Text = humanOpenRate.ToString();
            int martianOpenRate = Convert.ToInt32(researchKey.GetValue("Martian Open Rate"));
            trackBar5.Value = martianOpenRate;
            label12.Text = martianOpenRate.ToString();
            int podInterval = Convert.ToInt32(tweakKey.GetValue("Pod Interval (hours)"));
            trackBar6.Value = podInterval;
            label13.Text = podInterval.ToString();
            int aiHoursPerTurn = Convert.ToInt32(tweakKey.GetValue("AI Hours Per Turn"));
            trackBar7.Value = aiHoursPerTurn;
            label14.Text = aiHoursPerTurn.ToString();
            trackBar1.ValueChanged += trackBar1_ValueChanged!;
            trackBar2.ValueChanged += trackBar2_ValueChanged!;
            trackBar3.ValueChanged += trackBar3_ValueChanged!;
            trackBar4.ValueChanged += trackBar4_ValueChanged!;
            trackBar5.ValueChanged += trackBar5_ValueChanged!;
            trackBar6.ValueChanged += trackBar6_ValueChanged!;
            trackBar7.ValueChanged += trackBar7_ValueChanged!;
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

        private void trackBar1_ValueChanged(object sender, EventArgs e)
        {
            label8.Text = trackBar1.Value.ToString();
        }
        private void trackBar2_ValueChanged(object sender, EventArgs e)
        {
            label9.Text = trackBar2.Value.ToString();
        }
        private void trackBar3_ValueChanged(object sender, EventArgs e)
        {
            label10.Text = trackBar3.Value.ToString();
        }
        private void trackBar4_ValueChanged(object sender, EventArgs e)
        {
            label11.Text = trackBar4.Value.ToString();
        }
        private void trackBar5_ValueChanged(object sender, EventArgs e)
        {
            label12.Text = trackBar5.Value.ToString();
        }
        private void trackBar6_ValueChanged(object sender, EventArgs e)
        {
            label13.Text = trackBar6.Value.ToString();
        }
        private void trackBar7_ValueChanged(object sender, EventArgs e)
        {
            label14.Text = trackBar7.Value.ToString();
        }
    }
}
