using Microsoft.Win32;
using System.Diagnostics;
using System.Security.AccessControl;
using System.Security.Principal;

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
            Process[] processes = Process.GetProcessesByName("WoWViewer");
            if (processes.Length > 1) { return; }
            EnsureRegistryPermissions();
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
        // only necessary for the SaveEditorForm which is mostly unused and incomplete but added anyway
        public static void EnsureRegistryPermissions()
        {
            WindowsPrincipal principal = new WindowsPrincipal(WindowsIdentity.GetCurrent());
            bool hasAdmin = principal.IsInRole(WindowsBuiltInRole.Administrator);

            if (hasAdmin)
            {
                // Path to the Registry Key
                string keyPath = @"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000";
                using (RegistryKey key = Registry.LocalMachine.OpenSubKey(keyPath, RegistryKeyPermissionCheck.ReadWriteSubTree, RegistryRights.ChangePermissions))
                {
                    if (key != null)
                    {
                        RegistrySecurity rs = new RegistrySecurity();
                        // Grant FullControl to the "Users" group
                        rs.AddAccessRule(new RegistryAccessRule(new SecurityIdentifier(WellKnownSidType.BuiltinUsersSid, null), RegistryRights.FullControl, AccessControlType.Allow));
                        key.SetAccessControl(rs);
                    }
                }
            }
        }
    }
}