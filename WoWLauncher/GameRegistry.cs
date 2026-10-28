using Microsoft.Win32;
using System.Security;
using System.Security.AccessControl;
using System.Security.Principal;

namespace WoWLauncher
{
    /// <summary>
    /// The game is 32-bit, so its settings live in the 32-bit registry view
    /// (HKLM\SOFTWARE\WOW6432Node\... on 64-bit Windows). Every launcher key goes through here so a
    /// 64-bit build of the launcher can never end up reading or writing a different copy.
    /// </summary>
    internal static class GameRegistry
    {
        public const string RootPath = @"SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'";
        public const string VersionPath = RootPath + @"\1.00.000";

        private static RegistryKey BaseKey() => RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32);

        /// <summary>Opens a writable game key, e.g. Open("Tweak"). Returns null if it doesn't exist.</summary>
        public static RegistryKey? Open(string subKey = "")
        {
            using var baseKey = BaseKey();
            return baseKey.OpenSubKey(Combine(subKey), true);
        }

        /// <summary>Opens or creates a writable game key.</summary>
        public static RegistryKey Create(string subKey = "")
        {
            using var baseKey = BaseKey();
            return baseKey.CreateSubKey(Combine(subKey), true);
        }

        /// <summary>
        /// Older WoWViewer versions gave the Users group full control of the game's HKLM key whenever they ran
        /// elevated. That lets any program on the machine rewrite the key, so remove that explicit rule again;
        /// the key then falls back to the permissions it inherits from HKLM\SOFTWARE. Requires elevation.
        /// </summary>
        public static void RemoveLegacyUsersFullControl()
        {
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

        private static string Combine(string subKey) => subKey.Length == 0 ? VersionPath : VersionPath + "\\" + subKey;
    }
}
