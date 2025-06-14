using System.Diagnostics;

namespace WoWViewer
{
    internal static class Program
    {
        /// <summary>
        ///  The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main(string[] args)
        {
            // To customize application configuration such as set high DPI settings or default font,
            // see https://aka.ms/applicationconfiguration.
            Process[] processes = Process.GetProcessesByName("WoWViewer");
            if (processes.Length > 1) { return; }
            ApplicationConfiguration.Initialize();
            WoWViewer mainForm = new WoWViewer();
            if (args.Length == 1)
            {
                if (Path.GetExtension(args[0]).ToLowerInvariant() == ".wow") { mainForm.openFile(args[0]); }
                else { MessageBox.Show("Only .wow files are supported."); }
            }
            else if (args.Length > 1) { MessageBox.Show("Please open only one file at a time."); }
            Application.Run(mainForm);
        }
    }
}