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
            Process[] processes = Process.GetProcessesByName("WoWLauncher");
            if (processes.Length > 1) { return; }
            ApplicationConfiguration.Initialize();
            string basePath = AppDomain.CurrentDomain.BaseDirectory;
            if (basePath.Contains("OneDrive"))
            {
                MessageBox.Show("Warning: The game is installed in a restricted location. Please move it to a root directory like C:\\Games\\Jeff Wayne's 'The War Of The Worlds' for best results.", "Installation Warning", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            Application.Run(new Form1());
        }
    }
}