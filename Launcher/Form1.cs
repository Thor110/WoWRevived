using IniParser;
using Microsoft.Win32;
using IniParser;
using System.Diagnostics;

namespace WoWLauncher
{
    public partial class Form1 : Form
    {
        private bool config;
        private RegistryKey key;// privilege issue
        //private RegistryKey key = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000", true)!;
        private ToolTip tooltip = new ToolTip();
        private Type[] excludedControlTypes = new Type[] { typeof(PictureBox), typeof(Label), typeof(Button) };
        //private static readonly IniFile MyIni = new IniFile("swkotor2.ini");
        public Form1()
        {
            InitializeComponent();
            checkBox1.Visible = false;
            checkBox2.Visible = false;
            comboBox1.Visible = false;
            label1.Visible = false;
            //InitializeRegistry();
            InitializeTooltips();
            //InitializeParser();
        }
        // This method is called when the form is loaded to initialize the registry settings
        private void InitializeRegistry()
        {
            if (key != null)
            {
                //MessageBox.Show(key.GetValue("CD Path")!.ToString());
            }
            else
            {
                MessageBox.Show("Registry key not found. Please ensure the game is installed correctly.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            key.Close();
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
        /// <summary>
        /// InitializeParser parses the relevant settings from swkotor2.ini
        /// </summary>
        /// <remarks>
        /// Only parses the relevant settings.
        /// </remarks>
        private void InitializeParser()
        {
            if (File.Exists("MARTIAN.cd"))
            {
                
            }
            else
            {
                MessageBox.Show("MARTIAN.cd not found. Please ensure the game is installed correctly.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
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
            Process.Start("WoW_patched.exe");
            //Close();
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
            Process.Start("WoW_patched.exe");
            //Close();
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
            label1.Visible = true;
        }
        /// This is the event handler for the "Exit" button
        private void button4_Click(object sender, EventArgs e)
        {
            if(config)
            {
                button1.Visible = true;
                button2.Visible = true;
                button3.Visible = true;
                config = false;
                button4.Text = "Exit";
                checkBox1.Visible = false;
                checkBox2.Visible = false;
                comboBox1.Visible = false;
                label1.Visible = false;
            }
            else { Close(); }
        }
    }
}
