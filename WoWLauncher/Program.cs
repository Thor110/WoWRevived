using System.Diagnostics;

namespace WoWLauncher
{
    internal static class Program
    {
        /// <summary>
        ///  The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            // To customize application configuration such as set high DPI settings or default font,
            // see https://aka.ms/applicationconfiguration.
            if (Process.GetProcessesByName("WoWLauncher").Length > 1)
            {
                MessageBox.Show("The launcher is already running!");
                return;
            }
            if (Process.GetProcessesByName("WoW_patched").Length > 0)
            {
                MessageBox.Show("The game is already running, please exit before running the launcher!");
                return;
            }
            ApplicationConfiguration.Initialize();
            if (AppDomain.CurrentDomain.BaseDirectory.Contains("OneDrive"))
            {
                MessageBox.Show("Warning: The game is installed in a restricted location. Please move it to a root directory like C:\\Games\\Jeff Wayne's 'The War Of The Worlds' for best results.", "Installation Warning", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            Application.Run(new Form1());
        }
    }
}