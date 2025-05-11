using Microsoft.Win32;
using System.Diagnostics;

namespace WoWLauncher
{
    public partial class Form1 : Form
    {
        private bool config;
        private RegistryKey mainKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000", true)!;
        private RegistryKey screenKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\screen", true)!;
        private ToolTip tooltip = new ToolTip();
        private Type[] excludedControlTypes = new Type[] { typeof(PictureBox), typeof(Label), typeof(Button) };
        public Form1()
        {
            InitializeComponent();
            checkBox1.Visible = false;
            checkBox2.Visible = false;
            comboBox1.Visible = false;
            comboBox2.Visible = false;
            comboBox3.Visible = false;
            label1.Visible = false;
            label2.Visible = false;
            label3.Visible = false;
            comboBox1.Items.Add("English");
            comboBox1.Items.Add("French");
            comboBox1.Items.Add("German");
            comboBox1.Items.Add("Italian");
            comboBox1.Items.Add("Spanish");
            comboBox2.Items.Add("640x480");
            comboBox2.Items.Add("800x600");
            comboBox2.Items.Add("1024x768");
            comboBox2.Items.Add("1280x1024");
            comboBox3.Items.Add("30");
            comboBox3.Items.Add("60");
            comboBox3.Items.Add("120");
            comboBox3.Items.Add("240");
            InitializeRegistry();
            InitializeTooltips();
        }
        // This method is called when the form is loaded to initialize the registry settings
        private void InitializeRegistry()
        {
            if (mainKey != null)
            {
                if ((int)mainKey.GetValue("Enable Network Version")! == 1)
                {
                    checkBox1.Checked = true;
                }
                if (Convert.ToInt32(mainKey.GetValue("Full Screen")) == 1)
                {
                    checkBox2.Checked = true;
                }
                comboBox1.SelectedItem = (string)mainKey.GetValue("Language")!;
                comboBox2.SelectedItem = ((string)screenKey.GetValue("Size")!).Replace(",", "x");
                comboBox3.SelectedItem = (string)mainKey.GetValue("Game Frequency")!;
            }
            else
            {
                MessageBox.Show("Registry key not found. Please ensure the game is installed correctly.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
        /// <summary>
        /// InitializeTooltips prepares a tooltip for every control in the form.
        /// </summary>
        /// <remarks>
        /// Uses excludedControlTypes to exclude certain types of controls from displaying tooltips.
        /// </remarks>
        void InitializeTooltips()
        {
            components = new System.ComponentModel.Container();
            tooltip = new ToolTip(components);
            foreach (Control control in Controls)
            {
                if (excludedControlTypes.Contains(control.GetType()) != true)
                {
                    control.MouseEnter += new EventHandler(tooltip_MouseEnter);
                    control.MouseLeave += new EventHandler(tooltip_MouseLeave);
                }
            }
        }
        /// <summary>
        /// tooltip_MouseEnter event Handler uses the existing AccessibleDescription property as the tooltip information.
        /// </summary>
        void tooltip_MouseEnter(object? sender, EventArgs e)
        {
            Control control = (Control)sender!;
            if (control.AccessibleDescription != null) { tooltip.Show(control.AccessibleDescription.ToString(), control); }
            else { tooltip.Show("No description available.", control); }
        }
        /// <summary>
        /// tooltip_MouseLeave event Handler hides the active tooltip.
        /// </summary>
        void tooltip_MouseLeave(object? sender, EventArgs e) { tooltip.Hide((Control)sender!); }
        private void launchGame()
        {
            if (File.Exists("WoW_patched.exe"))
            {
                Process.Start("WoW_patched.exe");
            }
            else
            {
                Process.Start("WOW.exe");
            }
            Close();
        }
        // This is the event handler for the "Start Human Game" button
        private void button1_Click(object sender, EventArgs e)
        {
            if (File.Exists("MARTIAN.cd") && Directory.Exists("FMV-Human"))
            {
                File.Move("MARTIAN.cd", "MARTIAN.cd.bak");
                File.Move("human.cd.bak", "human.cd");
                Directory.Move("FMV", "FMV-Martian");
                Directory.Move("FMV-Human", "FMV");
            }
            launchGame();
        }
        // This is the event handler for the "Start Martian Game" button
        private void button2_Click(object sender, EventArgs e)
        {
            if (File.Exists("human.cd") && Directory.Exists("FMV-Martian"))
            {
                File.Move("human.cd", "human.cd.bak");
                File.Move("MARTIAN.cd.bak", "MARTIAN.cd");
                Directory.Move("FMV", "FMV-Human");
                Directory.Move("FMV-Martian", "FMV");
            }
            launchGame();
        }
        // This is the event handler for the "Configuration Settings" button
        private void button3_Click(object sender, EventArgs e)
        {
            button1.Visible = false;
            button2.Visible = false;
            button3.Visible = false;
            config = true;
            button4.Text = "Back";
            checkBox1.Visible = true;
            checkBox2.Visible = true;
            comboBox1.Visible = true;
            comboBox2.Visible = true;
            comboBox3.Visible = true;
            label1.Visible = true;
            label2.Visible = true;
            label3.Visible = true;
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
                comboBox1.Visible = false;
                comboBox2.Visible = false;
                comboBox3.Visible = false;
                label1.Visible = false;
                label2.Visible = false;
                label3.Visible = false;
            }
            else { Close(); }
        }
        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            mainKey.SetValue("Enable Network Version", checkBox1.Checked ? 1 : 0);
        }
        private void checkBox2_CheckedChanged(object sender, EventArgs e)
        {
            mainKey.SetValue("Full Screen", checkBox2.Checked ? 1 : 0, RegistryValueKind.String);
        }
        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            mainKey.SetValue("Language", comboBox1.SelectedItem!.ToString()!);
        }
        private void comboBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            screenKey.SetValue("Size", comboBox2.SelectedItem!.ToString()!.Replace("x", ","), RegistryValueKind.String);
            screenKey.SetValue("Support screen size", comboBox2.SelectedItem!.ToString()!.Replace("x", ","), RegistryValueKind.String);
        }
        private void comboBox3_SelectedIndexChanged(object sender, EventArgs e)
        {
            mainKey.SetValue("Game Frequency", comboBox3.SelectedItem!.ToString()!, RegistryValueKind.String);
        }
    }
}
