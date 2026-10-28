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
            Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
            // A .wow file opened from Explorer can be a path relative to wherever the viewer was started from,
            // so resolve it before switching to the viewer's own folder.
            args = args.Select(Path.GetFullPath).ToArray();
            // TEXT.ojd, OBJ.ojd and SaveGame\ are used by relative paths, so always work from the game folder
            // (opening a .wow file from Explorer would otherwise start the viewer in DAT\ or MAPS\).
            Directory.SetCurrentDirectory(AppContext.BaseDirectory);
            Process[] processes = Process.GetProcessesByName("WoWViewer");
            if (processes.Length > 1)
            {
                MessageBox.Show("WoWViewer is already running.", "WoWViewer", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
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