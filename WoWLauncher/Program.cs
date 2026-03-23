using System.Diagnostics;

namespace WoWLauncher
{
    internal static class Program
    {
        // Publicly accessible dictionary for all forms
        public static Dictionary<string, string> Interface = new Dictionary<string, string>();
        public static string CurrentLanguage = "English";
        /// <summary>
        ///  The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            // To customize application configuration such as set high DPI settings or default font,
            // see https://aka.ms/applicationconfiguration.
            switch (new FileInfo("credits.png").Length)
            {
                case 252640: SetEnglish(); break;
                case 287375: SetFrench(); break;
                case 289388: SetGerman(); break;
                case 255818: SetItalian(); break;
                case 270546: SetSpanish(); break;
                default: MessageBox.Show("You altered credits.png!!! The file size is used to identify which language should be set!!!"); break;
            }
            if (Process.GetProcessesByName("WoWLauncher").Length > 1)
            {
                MessageBox.Show(Interface["launcher_running"]);
                return;
            }
            if (Process.GetProcessesByName("WoW_patched").Length > 0)
            {
                MessageBox.Show(Interface["game_running"]);
                return;
            }
            ApplicationConfiguration.Initialize();
            if (AppDomain.CurrentDomain.BaseDirectory.Contains("OneDrive"))
            {
                MessageBox.Show(Interface["one_drive"], Interface["one_drive_warning"], MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            Application.Run(new Form1());
        }
        private static void SetEnglish()
        {
            CurrentLanguage = "English";
            Interface["start_human"] = "Start Human Campaign";
            Interface["start_martian"] = "Start Martian Campaign";
            Interface["config"] = "Configuration Settings";
            Interface["launcher_running"] = "The launcher is already running!";
            Interface["game_running"] = "The game is already running, please exit before running the launcher!";
            Interface["one_drive"] = "Warning: The game is installed in a restricted location. Please move it to a root directory like C:\\Games\\Jeff Wayne's 'The War Of The Worlds' for best results.";
            Interface["one_drive_warning"] = "Installation Warning";
            Interface["fullscreen_description"] = "Enable or disable fullscreen. Note : Alt-Tabbing crashes the game when in fullscreen mode.";
            Interface["fullscreen"] = "Full Screen";
            Interface["resolution_description"] = "The resolution for the game.";
            Interface["resolution"] = "Resolution";
            Interface["difficulty_description"] = "Difficulty settings. (Medium is the default)";
            Interface["difficulty"] = "Difficulty";
            Interface["fog_description"] = "Enable or disable fog of war.";
            Interface["fog"] = "Fog of War";
            Interface["advanced"] = "Advanced Settings";
            Interface["tools"] = "Development Tools";
            Interface["keyboard"] = "Keyboard Shortcuts";
            Interface["music_playback_description"] = "Enabling this will allow the music to continue playing when the window loses focus, if not in fullscreen mode.";
            Interface["music_playback"] = "Music Playback";
            Interface["enhanced_assets_description"] = "This setting enables the enhanced user interface assets. ( This adds a little more colour depth to the interface. )";
            Interface["enhanced_assets"] = "Enhanced Assets";
            Interface["enemy_visible_description"] = "This toggles whether or not enemy forces are visible on the warmap.";
            Interface["enemy_visible"] = "Enemy Forces Visible";
            Interface["game_name"] = "Jeff Wayne's 'The War Of The Worlds'";
            Interface["dir_warning"] = "Warning: The folder {0} is Read-Only or Protected.\n\n" +
                            "The launcher may fail to save settings. Please run as Administrator, " +
                            "uncheck read-only permissions on the folder or move the game to a " +
                            "different folder (e.g., C:\\Games\\).";
            Interface["dir_warning_error"] = "Permissions Error";
            Interface["alt_tab"] = "Alt-tabbing is not supported in fullscreen mode.\n\nDo you want to restart the game?";
            Interface["alt_tab_error"] = "Alt Tab Error";
            Interface["registry_missing"] = "Registry entry missing, base registry entries recreated from scratch.\n\nPlease run the game once to create the rest of the registry entries.";
            Interface["lights"] = "The \"Show lights\" registry entry must be set to one to prevent crashes when using the Infiltration skill in-game, so it has been reset.";
            Interface["colour"] = "The \"BPP\" registry entry must be set to 16 as I have removed the 8/16 bit toggle from the games executable, surely you don't want to play it in 8 bit colour mode in the 21st century?";
            Interface["executable"] = "Executable not found, please reinstall the game and follow the instructions.";
            Interface["human_game"] = "Human game not installed, please reinstall the game and follow the instructions.";
            Interface["martian_game"] = "Martian game not installed, please reinstall the game and follow the instructions.";
            Interface["back"] = "Back";
            Interface["exit"] = "Exit";
            Interface["editor"] = "Editor not found, please reinstall the game and follow the instructions.";
            Interface["fullscreen_disable"] = "Disable Full Screen to enable this feature.";
            Interface["fullscreen_detected"] = "Full Screen Detected";
            Interface["easy"] = "Easy";
            Interface["medium"] = "Medium";
            Interface["hard"] = "Hard";
            Interface["extreme"] = "Extreme";
            Interface["custom"] = "Custom";
            //Form2.Designer.cs
            Interface["damage_reduction"] = "Adjust the 'Damage reduction divisor' registry value. Default is 500.";
            Interface["damage"] = "Damage reduction divisor :";
            Interface["return"] = "Return";
            Interface["max_units"] = "Max units in sector :";
            Interface["max_units_description"] = "Adjust the 'Max units in sector' registry value. Default is 15.";
            Interface["max_boats"] = "Max boats in sector :";
            Interface["max_boats_description"] = "Adjust the 'Max boats in sector' registry value. Default is 5.";
            Interface["martian_open_rate"] = "Martian Open Rate :";
            Interface["martian_open_rate_description"] = "Adjust the 'Martian Open Rate' registry value. Default is 10.";
            Interface["human_open_rate"] = "Human Open Rate :";
            Interface["human_open_rate_description"] = "Adjust the 'Human Open Rate' registry value. Default is 20.";
            Interface["pod_interval"] = "Pod Interval (hours) :";
            Interface["pod_interval_description"] = "Adjust the 'Pod Interval (hours)' registry value. Default is 24.";
            Interface["ai_hours"] = "AI Hours Per Turn :";
            Interface["ai_hours_description"] = "Adjust the 'AI Hours Per Turn' registry value. Default is 5.";
            Interface["restore_description"] = "Restore the settings to their default state.";
            Interface["restore"] = "Restore Default Settings";
            Interface["description"] = "These settings are all available in the registry entry for the game, these are the default settings.";
            Interface["description_suggestion"] = "I suggest leaving them as they are unless you have played the game before.";
            Interface["martian_strength"] = "AI strength table Martian multiplier :";
            Interface["martian_strength_description"] = "Adjust the 'AI strength table Martian multiplier' registry value. Default is 2.000000.";
            Interface["human_strength"] = "AI strength table Human multiplier :";
            Interface["human_strength_description"] = "Adjust the 'AI strength table Human multiplier' registry value. Default is 1.000000.";
        }
        private static void SetFrench()
        {
            CurrentLanguage = "French";
            Interface["start_human"] = "Commencer Campagne Humaine"; // Démarrer le jeu humain
            Interface["start_martian"] = "Commencer Campagne Martienne";
            Interface["config"] = "Paramètres de configuration";
            Interface["launcher_running"] = "Le lanceur est déjà en cours d'exécution!";
            Interface["game_running"] = "Le jeu est déjà en cours d'exécution, veuillez le quitter avant de lancer le lanceur!";
            Interface["one_drive"] = "Avertissement : Le jeu est installé dans un emplacement restreint. Pour un fonctionnement optimal, veuillez le déplacer vers un répertoire racine tel que C:\\Games\\Jeff Wayne's 'The War Of The Worlds'.";
            Interface["one_drive_warning"] = "Installation Warning";
            Interface["fullscreen_description"] = "Enable or disable fullscreen. Note : Alt-Tabbing crashes the game when in fullscreen mode.";
            Interface["fullscreen"] = "Full Screen";
            Interface["resolution_description"] = "The resolution for the game.";
            Interface["resolution"] = "Resolution";
            Interface["difficulty_description"] = "Difficulty settings. (Medium is the default)";
            Interface["difficulty"] = "Difficulty";
            Interface["fog_description"] = "Enable or disable fog of war.";
            Interface["fog"] = "Fog of War";
            Interface["advanced"] = "Advanced Settings";
            Interface["tools"] = "Development Tools";
            Interface["keyboard"] = "Keyboard Shortcuts";
            Interface["music_playback_description"] = "Enabling this will allow the music to continue playing when the window loses focus, if not in fullscreen mode.";
            Interface["music_playback"] = "Music Playback";
            Interface["enhanced_assets_description"] = "This setting enables the enhanced user interface assets. ( This adds a little more colour depth to the interface. )";
            Interface["enhanced_assets"] = "Enhanced Assets";
            Interface["enemy_visible_description"] = "This toggles whether or not enemy forces are visible on the warmap.";
            Interface["enemy_visible"] = "Enemy Forces Visible";
            Interface["game_name"] = "Jeff Wayne's 'The War Of The Worlds'";
            Interface["dir_warning"] = "Warning: The folder {0} is Read-Only or Protected.\n\n" +
                            "The launcher may fail to save settings. Please run as Administrator, " +
                            "uncheck read-only permissions on the folder or move the game to a " +
                            "different folder (e.g., C:\\Games\\).";
            Interface["dir_warning_error"] = "Permissions Error";
            Interface["alt_tab"] = "Alt-tabbing is not supported in fullscreen mode.\n\nDo you want to restart the game?";
            Interface["alt_tab_error"] = "Alt Tab Error";
            Interface["registry_missing"] = "Registry entry missing, base registry entries recreated from scratch.\n\nPlease run the game once to create the rest of the registry entries.";
            Interface["lights"] = "The \"Show lights\" registry entry must be set to one to prevent crashes when using the Infiltration skill in-game, so it has been reset.";
            Interface["colour"] = "The \"BPP\" registry entry must be set to 16 as I have removed the 8/16 bit toggle from the games executable, surely you don't want to play it in 8 bit colour mode in the 21st century?";
            Interface["executable"] = "Executable not found, please reinstall the game and follow the instructions.";
            Interface["human_game"] = "Human game not installed, please reinstall the game and follow the instructions.";
            Interface["martian_game"] = "Martian game not installed, please reinstall the game and follow the instructions.";
            Interface["back"] = "Back";
            Interface["exit"] = "Quitter";
            Interface["editor"] = "Editor not found, please reinstall the game and follow the instructions.";
            Interface["fullscreen_disable"] = "Disable Full Screen to enable this feature.";
            Interface["fullscreen_detected"] = "Full Screen Detected";
            Interface["easy"] = "Easy";
            Interface["medium"] = "Medium";
            Interface["hard"] = "Hard";
            Interface["extreme"] = "Extreme";
            Interface["custom"] = "Custom";
            //Form2.Designer.cs
            Interface["damage_reduction"] = "Adjust the 'Damage reduction divisor' registry value. Default is 500.";
            Interface["damage"] = "Damage reduction divisor :";
            Interface["return"] = "Return";
            Interface["max_units"] = "Max units in sector :";
            Interface["max_units_description"] = "Adjust the 'Max units in sector' registry value. Default is 15.";
            Interface["max_boats"] = "Max boats in sector :";
            Interface["max_boats_description"] = "Adjust the 'Max boats in sector' registry value. Default is 5.";
            Interface["martian_open_rate"] = "Martian Open Rate :";
            Interface["martian_open_rate_description"] = "Adjust the 'Martian Open Rate' registry value. Default is 10.";
            Interface["human_open_rate"] = "Human Open Rate :";
            Interface["human_open_rate_description"] = "Adjust the 'Human Open Rate' registry value. Default is 20.";
            Interface["pod_interval"] = "Pod Interval (hours) :";
            Interface["pod_interval_description"] = "Adjust the 'Pod Interval (hours)' registry value. Default is 24.";
            Interface["ai_hours"] = "AI Hours Per Turn :";
            Interface["ai_hours_description"] = "Adjust the 'AI Hours Per Turn' registry value. Default is 5.";
            Interface["restore_description"] = "Restore the settings to their default state.";
            Interface["restore"] = "Restore Default Settings";
            Interface["description"] = "These settings are all available in the registry entry for the game, these are the default settings.";
            Interface["description_suggestion"] = "I suggest leaving them as they are unless you have played the game before.";
            Interface["martian_strength"] = "AI strength table Martian multiplier :";
            Interface["martian_strength_description"] = "Adjust the 'AI strength table Martian multiplier' registry value. Default is 2.000000.";
            Interface["human_strength"] = "AI strength table Human multiplier :";
            Interface["human_strength_description"] = "Adjust the 'AI strength table Human multiplier' registry value. Default is 1.000000.";
        }
        private static void SetGerman()
        {
            CurrentLanguage = "German";
            Interface["start_human"] = "Start Human Campaign";
            Interface["start_martian"] = "Start Martian Campaign";
            Interface["config"] = "Configuration Settings";
            Interface["launcher_running"] = "The launcher is already running!";
            Interface["game_running"] = "The game is already running, please exit before running the launcher!";
            Interface["one_drive"] = "Warning: The game is installed in a restricted location. Please move it to a root directory like C:\\Games\\Jeff Wayne's 'The War Of The Worlds' for best results.";
            Interface["one_drive_warning"] = "Installation Warning";
            Interface["fullscreen_description"] = "Enable or disable fullscreen. Note : Alt-Tabbing crashes the game when in fullscreen mode.";
            Interface["fullscreen"] = "Full Screen";
            Interface["resolution_description"] = "The resolution for the game.";
            Interface["resolution"] = "Resolution";
            Interface["difficulty_description"] = "Difficulty settings. (Medium is the default)";
            Interface["difficulty"] = "Difficulty";
            Interface["fog_description"] = "Enable or disable fog of war.";
            Interface["fog"] = "Fog of War";
            Interface["advanced"] = "Advanced Settings";
            Interface["tools"] = "Development Tools";
            Interface["keyboard"] = "Keyboard Shortcuts";
            Interface["music_playback_description"] = "Enabling this will allow the music to continue playing when the window loses focus, if not in fullscreen mode.";
            Interface["music_playback"] = "Music Playback";
            Interface["enhanced_assets_description"] = "This setting enables the enhanced user interface assets. ( This adds a little more colour depth to the interface. )";
            Interface["enhanced_assets"] = "Enhanced Assets";
            Interface["enemy_visible_description"] = "This toggles whether or not enemy forces are visible on the warmap.";
            Interface["enemy_visible"] = "Enemy Forces Visible";
            Interface["game_name"] = "Jeff Wayne's 'The War Of The Worlds'";
            Interface["dir_warning"] = "Warning: The folder {0} is Read-Only or Protected.\n\n" +
                            "The launcher may fail to save settings. Please run as Administrator, " +
                            "uncheck read-only permissions on the folder or move the game to a " +
                            "different folder (e.g., C:\\Games\\).";
            Interface["dir_warning_error"] = "Permissions Error";
            Interface["alt_tab"] = "Alt-tabbing is not supported in fullscreen mode.\n\nDo you want to restart the game?";
            Interface["alt_tab_error"] = "Alt Tab Error";
            Interface["registry_missing"] = "Registry entry missing, base registry entries recreated from scratch.\n\nPlease run the game once to create the rest of the registry entries.";
            Interface["lights"] = "The \"Show lights\" registry entry must be set to one to prevent crashes when using the Infiltration skill in-game, so it has been reset.";
            Interface["colour"] = "The \"BPP\" registry entry must be set to 16 as I have removed the 8/16 bit toggle from the games executable, surely you don't want to play it in 8 bit colour mode in the 21st century?";
            Interface["executable"] = "Executable not found, please reinstall the game and follow the instructions.";
            Interface["human_game"] = "Human game not installed, please reinstall the game and follow the instructions.";
            Interface["martian_game"] = "Martian game not installed, please reinstall the game and follow the instructions.";
            Interface["back"] = "Back";
            Interface["exit"] = "Exit";
            Interface["editor"] = "Editor not found, please reinstall the game and follow the instructions.";
            Interface["fullscreen_disable"] = "Disable Full Screen to enable this feature.";
            Interface["fullscreen_detected"] = "Full Screen Detected";
            Interface["easy"] = "Easy";
            Interface["medium"] = "Medium";
            Interface["hard"] = "Hard";
            Interface["extreme"] = "Extreme";
            Interface["custom"] = "Custom";
            //Form2.Designer.cs
            Interface["damage_reduction"] = "Adjust the 'Damage reduction divisor' registry value. Default is 500.";
            Interface["damage"] = "Damage reduction divisor :";
            Interface["return"] = "Return";
            Interface["max_units"] = "Max units in sector :";
            Interface["max_units_description"] = "Adjust the 'Max units in sector' registry value. Default is 15.";
            Interface["max_boats"] = "Max boats in sector :";
            Interface["max_boats_description"] = "Adjust the 'Max boats in sector' registry value. Default is 5.";
            Interface["martian_open_rate"] = "Martian Open Rate :";
            Interface["martian_open_rate_description"] = "Adjust the 'Martian Open Rate' registry value. Default is 10.";
            Interface["human_open_rate"] = "Human Open Rate :";
            Interface["human_open_rate_description"] = "Adjust the 'Human Open Rate' registry value. Default is 20.";
            Interface["pod_interval"] = "Pod Interval (hours) :";
            Interface["pod_interval_description"] = "Adjust the 'Pod Interval (hours)' registry value. Default is 24.";
            Interface["ai_hours"] = "AI Hours Per Turn :";
            Interface["ai_hours_description"] = "Adjust the 'AI Hours Per Turn' registry value. Default is 5.";
            Interface["restore_description"] = "Restore the settings to their default state.";
            Interface["restore"] = "Restore Default Settings";
            Interface["description"] = "These settings are all available in the registry entry for the game, these are the default settings.";
            Interface["description_suggestion"] = "I suggest leaving them as they are unless you have played the game before.";
            Interface["martian_strength"] = "AI strength table Martian multiplier :";
            Interface["martian_strength_description"] = "Adjust the 'AI strength table Martian multiplier' registry value. Default is 2.000000.";
            Interface["human_strength"] = "AI strength table Human multiplier :";
            Interface["human_strength_description"] = "Adjust the 'AI strength table Human multiplier' registry value. Default is 1.000000.";
        }
        private static void SetItalian()
        {
            CurrentLanguage = "Italian";
            Interface["start_human"] = "Start Human Campaign";
            Interface["start_martian"] = "Start Martian Campaign";
            Interface["config"] = "Configuration Settings";
            Interface["launcher_running"] = "The launcher is already running!";
            Interface["game_running"] = "The game is already running, please exit before running the launcher!";
            Interface["one_drive"] = "Warning: The game is installed in a restricted location. Please move it to a root directory like C:\\Games\\Jeff Wayne's 'The War Of The Worlds' for best results.";
            Interface["one_drive_warning"] = "Installation Warning";
            Interface["fullscreen_description"] = "Enable or disable fullscreen. Note : Alt-Tabbing crashes the game when in fullscreen mode.";
            Interface["fullscreen"] = "Full Screen";
            Interface["resolution_description"] = "The resolution for the game.";
            Interface["resolution"] = "Resolution";
            Interface["difficulty_description"] = "Difficulty settings. (Medium is the default)";
            Interface["difficulty"] = "Difficulty";
            Interface["fog_description"] = "Enable or disable fog of war.";
            Interface["fog"] = "Fog of War";
            Interface["advanced"] = "Advanced Settings";
            Interface["tools"] = "Development Tools";
            Interface["keyboard"] = "Keyboard Shortcuts";
            Interface["music_playback_description"] = "Enabling this will allow the music to continue playing when the window loses focus, if not in fullscreen mode.";
            Interface["music_playback"] = "Music Playback";
            Interface["enhanced_assets_description"] = "This setting enables the enhanced user interface assets. ( This adds a little more colour depth to the interface. )";
            Interface["enhanced_assets"] = "Enhanced Assets";
            Interface["enemy_visible_description"] = "This toggles whether or not enemy forces are visible on the warmap.";
            Interface["enemy_visible"] = "Enemy Forces Visible";
            Interface["game_name"] = "Jeff Wayne's 'The War Of The Worlds'";
            Interface["dir_warning"] = "Warning: The folder {0} is Read-Only or Protected.\n\n" +
                            "The launcher may fail to save settings. Please run as Administrator, " +
                            "uncheck read-only permissions on the folder or move the game to a " +
                            "different folder (e.g., C:\\Games\\).";
            Interface["dir_warning_error"] = "Permissions Error";
            Interface["alt_tab"] = "Alt-tabbing is not supported in fullscreen mode.\n\nDo you want to restart the game?";
            Interface["alt_tab_error"] = "Alt Tab Error";
            Interface["registry_missing"] = "Registry entry missing, base registry entries recreated from scratch.\n\nPlease run the game once to create the rest of the registry entries.";
            Interface["lights"] = "The \"Show lights\" registry entry must be set to one to prevent crashes when using the Infiltration skill in-game, so it has been reset.";
            Interface["colour"] = "The \"BPP\" registry entry must be set to 16 as I have removed the 8/16 bit toggle from the games executable, surely you don't want to play it in 8 bit colour mode in the 21st century?";
            Interface["executable"] = "Executable not found, please reinstall the game and follow the instructions.";
            Interface["human_game"] = "Human game not installed, please reinstall the game and follow the instructions.";
            Interface["martian_game"] = "Martian game not installed, please reinstall the game and follow the instructions.";
            Interface["back"] = "Back";
            Interface["exit"] = "Exit";
            Interface["editor"] = "Editor not found, please reinstall the game and follow the instructions.";
            Interface["fullscreen_disable"] = "Disable Full Screen to enable this feature.";
            Interface["fullscreen_detected"] = "Full Screen Detected";
            Interface["easy"] = "Easy";
            Interface["medium"] = "Medium";
            Interface["hard"] = "Hard";
            Interface["extreme"] = "Extreme";
            Interface["custom"] = "Custom";
            //Form2.Designer.cs
            Interface["damage_reduction"] = "Adjust the 'Damage reduction divisor' registry value. Default is 500.";
            Interface["damage"] = "Damage reduction divisor :";
            Interface["return"] = "Return";
            Interface["max_units"] = "Max units in sector :";
            Interface["max_units_description"] = "Adjust the 'Max units in sector' registry value. Default is 15.";
            Interface["max_boats"] = "Max boats in sector :";
            Interface["max_boats_description"] = "Adjust the 'Max boats in sector' registry value. Default is 5.";
            Interface["martian_open_rate"] = "Martian Open Rate :";
            Interface["martian_open_rate_description"] = "Adjust the 'Martian Open Rate' registry value. Default is 10.";
            Interface["human_open_rate"] = "Human Open Rate :";
            Interface["human_open_rate_description"] = "Adjust the 'Human Open Rate' registry value. Default is 20.";
            Interface["pod_interval"] = "Pod Interval (hours) :";
            Interface["pod_interval_description"] = "Adjust the 'Pod Interval (hours)' registry value. Default is 24.";
            Interface["ai_hours"] = "AI Hours Per Turn :";
            Interface["ai_hours_description"] = "Adjust the 'AI Hours Per Turn' registry value. Default is 5.";
            Interface["restore_description"] = "Restore the settings to their default state.";
            Interface["restore"] = "Restore Default Settings";
            Interface["description"] = "These settings are all available in the registry entry for the game, these are the default settings.";
            Interface["description_suggestion"] = "I suggest leaving them as they are unless you have played the game before.";
            Interface["martian_strength"] = "AI strength table Martian multiplier :";
            Interface["martian_strength_description"] = "Adjust the 'AI strength table Martian multiplier' registry value. Default is 2.000000.";
            Interface["human_strength"] = "AI strength table Human multiplier :";
            Interface["human_strength_description"] = "Adjust the 'AI strength table Human multiplier' registry value. Default is 1.000000.";
        }
        private static void SetSpanish()
        {
            CurrentLanguage = "Spanish";
            Interface["start_human"] = "Start Human Campaign";
            Interface["start_martian"] = "Start Martian Campaign";
            Interface["config"] = "Configuration Settings";
            Interface["launcher_running"] = "The launcher is already running!";
            Interface["game_running"] = "The game is already running, please exit before running the launcher!";
            Interface["one_drive"] = "Warning: The game is installed in a restricted location. Please move it to a root directory like C:\\Games\\Jeff Wayne's 'The War Of The Worlds' for best results.";
            Interface["one_drive_warning"] = "Installation Warning";
            Interface["fullscreen_description"] = "Enable or disable fullscreen. Note : Alt-Tabbing crashes the game when in fullscreen mode.";
            Interface["fullscreen"] = "Full Screen";
            Interface["resolution_description"] = "The resolution for the game.";
            Interface["resolution"] = "Resolution";
            Interface["difficulty_description"] = "Difficulty settings. (Medium is the default)";
            Interface["difficulty"] = "Difficulty";
            Interface["fog_description"] = "Enable or disable fog of war.";
            Interface["fog"] = "Fog of War";
            Interface["advanced"] = "Advanced Settings";
            Interface["tools"] = "Development Tools";
            Interface["keyboard"] = "Keyboard Shortcuts";
            Interface["music_playback_description"] = "Enabling this will allow the music to continue playing when the window loses focus, if not in fullscreen mode.";
            Interface["music_playback"] = "Music Playback";
            Interface["enhanced_assets_description"] = "This setting enables the enhanced user interface assets. ( This adds a little more colour depth to the interface. )";
            Interface["enhanced_assets"] = "Enhanced Assets";
            Interface["enemy_visible_description"] = "This toggles whether or not enemy forces are visible on the warmap.";
            Interface["enemy_visible"] = "Enemy Forces Visible";
            Interface["game_name"] = "Jeff Wayne's 'The War Of The Worlds'";
            Interface["dir_warning"] = "Warning: The folder {0} is Read-Only or Protected.\n\n" +
                            "The launcher may fail to save settings. Please run as Administrator, " +
                            "uncheck read-only permissions on the folder or move the game to a " +
                            "different folder (e.g., C:\\Games\\).";
            Interface["dir_warning_error"] = "Permissions Error";
            Interface["alt_tab"] = "Alt-tabbing is not supported in fullscreen mode.\n\nDo you want to restart the game?";
            Interface["alt_tab_error"] = "Alt Tab Error";
            Interface["registry_missing"] = "Registry entry missing, base registry entries recreated from scratch.\n\nPlease run the game once to create the rest of the registry entries.";
            Interface["lights"] = "The \"Show lights\" registry entry must be set to one to prevent crashes when using the Infiltration skill in-game, so it has been reset.";
            Interface["colour"] = "The \"BPP\" registry entry must be set to 16 as I have removed the 8/16 bit toggle from the games executable, surely you don't want to play it in 8 bit colour mode in the 21st century?";
            Interface["executable"] = "Executable not found, please reinstall the game and follow the instructions.";
            Interface["human_game"] = "Human game not installed, please reinstall the game and follow the instructions.";
            Interface["martian_game"] = "Martian game not installed, please reinstall the game and follow the instructions.";
            Interface["back"] = "Back";
            Interface["exit"] = "Exit";
            Interface["editor"] = "Editor not found, please reinstall the game and follow the instructions.";
            Interface["fullscreen_disable"] = "Disable Full Screen to enable this feature.";
            Interface["fullscreen_detected"] = "Full Screen Detected";
            Interface["easy"] = "Easy";
            Interface["medium"] = "Medium";
            Interface["hard"] = "Hard";
            Interface["extreme"] = "Extreme";
            Interface["custom"] = "Custom";
            //Form2.Designer.cs
            Interface["damage_reduction"] = "Adjust the 'Damage reduction divisor' registry value. Default is 500.";
            Interface["damage"] = "Damage reduction divisor :";
            Interface["return"] = "Return";
            Interface["max_units"] = "Max units in sector :";
            Interface["max_units_description"] = "Adjust the 'Max units in sector' registry value. Default is 15.";
            Interface["max_boats"] = "Max boats in sector :";
            Interface["max_boats_description"] = "Adjust the 'Max boats in sector' registry value. Default is 5.";
            Interface["martian_open_rate"] = "Martian Open Rate :";
            Interface["martian_open_rate_description"] = "Adjust the 'Martian Open Rate' registry value. Default is 10.";
            Interface["human_open_rate"] = "Human Open Rate :";
            Interface["human_open_rate_description"] = "Adjust the 'Human Open Rate' registry value. Default is 20.";
            Interface["pod_interval"] = "Pod Interval (hours) :";
            Interface["pod_interval_description"] = "Adjust the 'Pod Interval (hours)' registry value. Default is 24.";
            Interface["ai_hours"] = "AI Hours Per Turn :";
            Interface["ai_hours_description"] = "Adjust the 'AI Hours Per Turn' registry value. Default is 5.";
            Interface["restore_description"] = "Restore the settings to their default state.";
            Interface["restore"] = "Restore Default Settings";
            Interface["description"] = "These settings are all available in the registry entry for the game, these are the default settings.";
            Interface["description_suggestion"] = "I suggest leaving them as they are unless you have played the game before.";
            Interface["martian_strength"] = "AI strength table Martian multiplier :";
            Interface["martian_strength_description"] = "Adjust the 'AI strength table Martian multiplier' registry value. Default is 2.000000.";
            Interface["human_strength"] = "AI strength table Human multiplier :";
            Interface["human_strength_description"] = "Adjust the 'AI strength table Human multiplier' registry value. Default is 1.000000.";
        }
    }
}