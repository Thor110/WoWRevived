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
        private RegistryKey debugKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Debug", true)!;
        public Form2()
        {
            InitializeComponent();
            ApplyLocalization();
            RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32);
            InitializeRegistry();
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(Label), typeof(Button) });
        }
        private void ApplyLocalization()
        {
            trackBar1.AccessibleDescription = Program.Interface["damage_reduction"];
            label1.Text = Program.Interface["damage"];
            button1.Text = Program.Interface["return"];
            label2.Text = Program.Interface["max_units"];
            trackBar2.AccessibleDescription = Program.Interface["max_units_description"];
            label3.Text = Program.Interface["max_boats"];
            trackBar3.AccessibleDescription = Program.Interface["max_boats_description"];
            label4.Text = Program.Interface["martian_open_rate"];
            trackBar4.AccessibleDescription = Program.Interface["martian_open_rate_description"];
            label5.Text = Program.Interface["human_open_rate"];
            trackBar5.AccessibleDescription = Program.Interface["human_open_rate_description"];
            label6.Text = Program.Interface["pod_interval"];
            trackBar6.AccessibleDescription = Program.Interface["pod_interval_description"];
            label7.Text = Program.Interface["ai_hours"];
            trackBar7.AccessibleDescription = Program.Interface["ai_hours_description"];
            button2.AccessibleDescription = Program.Interface["restore_description"];
            button2.Text = Program.Interface["restore"];
            label15.Text = Program.Interface["description"];
            label16.Text = Program.Interface["description_suggestion"];
            label19.Text = Program.Interface["martian_strength"];
            trackBar8.AccessibleDescription = Program.Interface["martian_strength_description"];
            label20.Text = Program.Interface["human_strength"];
            trackBar9.AccessibleDescription = Program.Interface["human_strength_description"];
            //
            Text = Program.Interface["advanced"];
            // language specific interface nudges
            if (Program.CurrentLanguage == "French")
            {

            }
            else if (Program.CurrentLanguage == "German")
            {

            }
            else if (Program.CurrentLanguage == "Italian")
            {

            }
            else if (Program.CurrentLanguage == "Spanish")
            {

            }
            // English - Default
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
            // new settings
            double humanMultiplier = Convert.ToDouble(tweakKey.GetValue("AI strength table Human multiplier"));
            trackBar9.Value = (int)humanMultiplier * 100;
            label18.Text = humanMultiplier.ToString("F6");
            double martianMultiplier = Convert.ToDouble(tweakKey.GetValue("AI strength table Martian multiplier"));
            trackBar8.Value = (int)martianMultiplier * 100;
            label17.Text = martianMultiplier.ToString("F6");
            trackBar1.ValueChanged += trackBar1_ValueChanged!;
            trackBar2.ValueChanged += trackBar2_ValueChanged!;
            trackBar3.ValueChanged += trackBar3_ValueChanged!;
            trackBar4.ValueChanged += trackBar4_ValueChanged!;
            trackBar5.ValueChanged += trackBar5_ValueChanged!;
            trackBar6.ValueChanged += trackBar6_ValueChanged!;
            trackBar7.ValueChanged += trackBar7_ValueChanged!;
            trackBar8.ValueChanged += trackBar8_ValueChanged!;
            trackBar9.ValueChanged += trackBar9_ValueChanged!;
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
            // new settings
            registryCompare(tweakKey, "AI strength table Human multiplier", ((double)trackBar9.Value / 100).ToString("F6"));
            registryCompare(tweakKey, "AI strength table Martian multiplier", ((double)trackBar8.Value / 100).ToString("F6"));
            if (config) { registryCompare(mainKey, "Difficulty", "Custom"); }
            this.Close();
        }
        // update labels when track Sbar values change
        private void trackBar1_ValueChanged(object sender, EventArgs e) { label8.Text = trackBar1.Value.ToString(); }   // Damage Reduction Divisor
        private void trackBar2_ValueChanged(object sender, EventArgs e) { label9.Text = trackBar2.Value.ToString(); }   // Max Units In Sector
        private void trackBar3_ValueChanged(object sender, EventArgs e) { label10.Text = trackBar3.Value.ToString(); }  // Max Boats In Sector
        private void trackBar5_ValueChanged(object sender, EventArgs e) { label11.Text = trackBar5.Value.ToString(); }  // Human Open Rate
        private void trackBar4_ValueChanged(object sender, EventArgs e) { label12.Text = trackBar4.Value.ToString(); }  // Martian Open Rate
        private void trackBar6_ValueChanged(object sender, EventArgs e) { label13.Text = trackBar6.Value.ToString(); }  // Pod Interval (hours)
        private void trackBar7_ValueChanged(object sender, EventArgs e) { label14.Text = trackBar7.Value.ToString(); }  // AI Hours Per Turn
        // AI strength table Human multiplier
        private void trackBar9_ValueChanged(object sender, EventArgs e) { label18.Text = ((double)trackBar9.Value / 100).ToString("F6"); }
        // AI strength table Martian multiplier
        private void trackBar8_ValueChanged(object sender, EventArgs e) { label17.Text = ((double)trackBar8.Value / 100).ToString("F6"); }
        // default
        private void button2_Click(object sender, EventArgs e)
        {
            registryCompare(battleKey, "Damage reduction divisor", "500");
            trackBar1.Value = 5;
            registryCompare(tweakKey, "Max units in sector", "15");
            trackBar2.Value = 15;
            registryCompare(tweakKey, "Max boats in sector", "5");
            trackBar3.Value = 5;
            registryCompare(researchKey, "Human Open Rate", "20");
            trackBar4.Value = 20;
            registryCompare(researchKey, "Martian Open Rate", "10");
            trackBar5.Value = 10;
            registryCompare(tweakKey, "Pod Interval (hours)", "24");
            trackBar6.Value = 24;
            registryCompare(tweakKey, "AI Hours Per Turn", "5");
            trackBar7.Value = 5;
            registryCompare(mainKey, "Difficulty", "Medium");
            registryCompare(battleKey, "EnableFogOfWar", "1");
            registryCompare(debugKey, "Enemy Visible", "0");
            // new settings
            registryCompare(tweakKey, "AI Aggression Value", "0.500000");                   // not added to menu
            registryCompare(tweakKey, "AI Invasion Threshold PC", "150.000000");            // not added to menu
            registryCompare(tweakKey, "AI strength table Human multiplier", "1.000000");
            trackBar9.Value = 100;
            registryCompare(tweakKey, "AI strength table Martian multiplier", "2.000000");
            trackBar8.Value = 200;
            config = false;
        }
    }
}
