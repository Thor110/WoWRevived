using System.Diagnostics;

namespace WOWViewer
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
            string processName = "WOWViewer";
            Process[] processes = Process.GetProcessesByName(processName);
            if (processes.Length > 1) { return; }
            ApplicationConfiguration.Initialize();
            Application.Run(new WOWViewer());
        }
    }
}