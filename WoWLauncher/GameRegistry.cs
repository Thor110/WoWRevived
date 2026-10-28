using Microsoft.Win32;
using System.Globalization;
using WoWRevived.Shared;
using System.Security;
using System.Security.AccessControl;
using System.Security.Principal;

namespace WoWLauncher
{
    /// <summary>
    /// The game's settings used to live in the registry (32-bit view, HKLM\SOFTWARE\WOW6432Node\... on 64-bit Windows).
    /// They are now kept in WoWRevived.ini (see Shared/GameSettings.cs); this class only imports the old values once
    /// and tidies up after older versions.
    /// </summary>
    internal static class GameRegistry
    {
        public const string RootPath = @"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'";
        public const string VersionPath = RootPath + @"\1.00.000";

        private static RegistryKey BaseKey() => RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32);

        // values that are never copied: the paths are always the game's own folder now, and "Use INI" would switch
        // some of the game's settings to its own unfinished INI mode
        private static readonly string[] SkippedValues = { "CD Path", "Install Path", "Use INI", "Autoexec install path" };

        /// <summary>
        /// Copies every value under the old registry key into the settings. When the game ran without administrator
        /// rights Windows stored its writes in the per-user VirtualStore instead, and those are the values it last
        /// saw, so they are applied on top. Returns true if anything was found.
        /// </summary>
        public static bool ImportInto(GameSettings settings)
        {
            bool found = false;
            try
            {
                using (var machine = BaseKey()) { found |= Copy(machine.OpenSubKey(VersionPath), settings, ""); }
                foreach (RegistryView view in new[] { RegistryView.Registry64, RegistryView.Registry32 })
                {
                    using var user = RegistryKey.OpenBaseKey(RegistryHive.CurrentUser, view);
                    found |= Copy(user.OpenSubKey(@"Software\Classes\VirtualStore\MACHINE\SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000"), settings, "");
                    found |= Copy(user.OpenSubKey(@"Software\Classes\VirtualStore\MACHINE\" + VersionPath), settings, "");
                }
            }
            catch (Exception ex) when (ex is UnauthorizedAccessException || ex is SecurityException || ex is IOException) { } // keep whatever was copied
            return found;
        }

        private static bool Copy(RegistryKey? key, GameSettings settings, string section)
        {
            if (key == null) { return false; }
            using (key)
            {
                string target = section.Length == 0 ? GameSettings.RootSection : section;
                foreach (string name in key.GetValueNames())
                {
                    if (name.Length == 0 || (section.Length == 0 && SkippedValues.Contains(name, StringComparer.OrdinalIgnoreCase))) { continue; }
                    string? text = key.GetValue(name, null, RegistryValueOptions.DoNotExpandEnvironmentNames) switch
                    {
                        int number => number.ToString(CultureInfo.InvariantCulture),
                        long number => number.ToString(CultureInfo.InvariantCulture),
                        string value => value,
                        string[] lines => string.Join(" ", lines),
                        _ => null, // binary values aren't used by the game
                    };
                    if (text != null && !text.Contains('\n') && !text.Contains('\r')) { settings.Set(target, name, text); }
                }
                foreach (string subKey in key.GetSubKeyNames())
                {
                    Copy(key.OpenSubKey(subKey), settings, section.Length == 0 ? subKey : section + "\\" + subKey);
                }
            }
            return true;
        }

        public static bool IsElevated()
        {
            using var identity = WindowsIdentity.GetCurrent();
            return new WindowsPrincipal(identity).IsInRole(WindowsBuiltInRole.Administrator);
        }

        /// <summary>
        /// Older WoWViewer versions gave the Users group full control of the game's HKLM key whenever they ran
        /// elevated. That lets any program on the machine rewrite the key, so remove that explicit rule again;
        /// the key then falls back to the permissions it inherits from HKLM\SOFTWARE. Requires elevation.
        /// </summary>
        public static void RemoveLegacyUsersFullControl()
        {
            if (!IsElevated()) { return; }
            try
            {
                using var baseKey = BaseKey();
                using var key = baseKey.OpenSubKey(VersionPath, RegistryKeyPermissionCheck.ReadWriteSubTree, RegistryRights.ReadPermissions | RegistryRights.ChangePermissions);
                if (key == null) { return; }
                RegistrySecurity security = key.GetAccessControl();
                var rule = new RegistryAccessRule(new SecurityIdentifier(WellKnownSidType.BuiltinUsersSid, null), RegistryRights.FullControl, AccessControlType.Allow);
                bool present = security.GetAccessRules(true, false, typeof(SecurityIdentifier)).Cast<RegistryAccessRule>()
                    .Any(r => r.IdentityReference.Equals(rule.IdentityReference) && r.RegistryRights == RegistryRights.FullControl &&
                              r.AccessControlType == AccessControlType.Allow && r.InheritanceFlags == InheritanceFlags.None);
                if (!present) { return; }
                security.RemoveAccessRuleSpecific(rule);
                key.SetAccessControl(security);
            }
            catch (Exception ex) when (ex is UnauthorizedAccessException || ex is SecurityException || ex is IOException) { } // best effort, not critical
        }

    }
}
