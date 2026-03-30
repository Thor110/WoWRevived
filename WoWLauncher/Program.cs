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
            Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
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
                            "uncheck read-only permissions on the folder or move the game to a different folder (e.g., C:\\Games\\).";
            Interface["dir_warning_error"] = "Permissions Error";
            Interface["alt_tab"] = "Alt-tabbing is not supported in fullscreen mode.\n\nDo you want to restart the game?";
            Interface["alt_tab_error"] = "Alt Tab Error";
            Interface["registry_missing"] = "Registry entry missing, base registry entries recreated from scratch.\n\nPlease run the game once to create the rest of the registry entries.";
            Interface["lights"] = "The \"Show lights\" registry entry must be set to one to prevent crashes when using the Infiltration skill in-game, so it has been reset.";
            Interface["colour"] = "The \"BPP\" registry entry must be set to 16 as I have removed the 8/16 bit toggle from the games executable, surely you don't want to play it in 8 bit colour mode in the 21st century?";
            Interface["executable"] = "Executable not found, please reinstall the game and follow the instructions.";
            Interface["human_game"] = "Human game not installed, please reinstall the game and follow the instructions.";
            Interface["martian_game"] = "Martian game not installed, please reinstall the game and follow the instructions.";
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
            Interface["martian_open_rate_description"] = "Adjust the 'Martian Open Rate' registry value. Default is 10. (Research speed)";
            Interface["human_open_rate"] = "Human Open Rate :";
            Interface["human_open_rate_description"] = "Adjust the 'Human Open Rate' registry value. Default is 20. (Research speed)";
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
            Interface["turret_description"] = "Adjust the turret build limit.";
            Interface["turret_label"] = "Turret Build Limit :";
            // new values
            Interface["directplay_installed"] = "DirectPlay has been enabled to resolve compatibility issues.";
            Interface["directplay_title"] = "System Update";
            Interface["instruction_button"] = "Instruction Manual";
            Interface["instruction_readme"] = "You should also read the original readme file as there are notes in there not in the games manual.";
            Interface["instruction_location"] = "wotw-man-english-v1.pdf";

        }
        private static void SetFrench()
        {
            CurrentLanguage = "French";
            Interface["start_human"] = "Commencer Campagne Humaine";
            Interface["start_martian"] = "Commencer Campagne Martienne";
            Interface["config"] = "Paramètres de configuration";
            Interface["launcher_running"] = "Le lanceur est déjà en cours d'exécution!";
            Interface["game_running"] = "Le jeu est déjà en cours d'exécution, veuillez le quitter avant de lancer le lanceur!";
            Interface["one_drive"] = "Avertissement : Le jeu est installé dans un emplacement restreint. Pour un fonctionnement optimal, veuillez le déplacer vers un répertoire racine tel que C:\\Jeux\\Jeff Wayne's 'La Guerre des mondes'.";
            Interface["one_drive_warning"] = "Avertissement d'installation";
            Interface["fullscreen_description"] = "Le passage d'une application à l'autre (Alt-Tab) fait planter le jeu en mode plein écran.";
            Interface["fullscreen"] = "Plein écran";
            Interface["resolution_description"] = "La résolution du jeu.";
            Interface["resolution"] = "Résolution";
            Interface["difficulty_description"] = "Niveaux de difficulté. (Moyen est le niveau par défaut)";
            Interface["difficulty"] = "Difficulté";
            Interface["fog_description"] = "Activer ou désactiver le brouillard de guerre.";
            Interface["fog"] = "Brouillard de guerre";
            Interface["advanced"] = "Paramètres avancés";
            Interface["tools"] = "Outils de développement";
            Interface["keyboard"] = "Raccourcis clavier";
            Interface["music_playback_description"] = "L'activation de cette option permettra à la musique de continuer à jouer lorsque la fenêtre perd le focus, sauf en mode plein écran.";
            Interface["music_playback"] = "Lecture de musique";
            Interface["enhanced_assets_description"] = "Ce paramètre active les ressources d'interface utilisateur améliorées. (Cela ajoute un peu plus de profondeur de couleur à l'interface.)";
            Interface["enhanced_assets"] = "Graphismes améliorés";
            Interface["enemy_visible_description"] = "Cette option permet d'afficher ou non les forces ennemies sur la carte de guerre.";
            Interface["enemy_visible"] = "Forces ennemies visibles";
            Interface["game_name"] = "Jeff Wayne's 'La Guerre des mondes'";
            Interface["dir_warning"] = "Avertissement : Le dossier {0} est en lecture seule ou protégé.\n\n" +
                            "Le lanceur peut ne pas enregistrer les paramètres. Veuillez l'exécuter en tant qu'administrateur, " +
                            "Décochez les autorisations de lecture seule sur le dossier ou déplacez le jeu vers un autre dossier (e.g., C:\\Jeux\\).";
            Interface["dir_warning_error"] = "Erreur d'autorisation";
            Interface["alt_tab"] = "Le passage d'une application à l'autre (Alt+Tab) n'est pas pris en charge en mode plein écran.\n\nVoulez-vous redémarrer le jeu?";
            Interface["alt_tab_error"] = "Erreur Alt Tab";
            Interface["registry_missing"] = "Entrée de registre manquante, les entrées de registre de base ont été recréées à partir de zéro.\n\nVeuillez exécuter le jeu une fois pour créer le reste des entrées de registre.";
            Interface["lights"] = "L'entrée de registre \"Show lights\" doit être définie sur un pour éviter les plantages lors de l'utilisation de la compétence Infiltration en jeu ; elle a donc été réinitialisée.";
            Interface["colour"] = "L'entrée de registre \"BPP\" doit être définie sur 16 car j'ai supprimé l'option de basculement 8/16 bits de l'exécutable du jeu ; vous ne voudriez tout de même pas y jouer en mode couleur 8 bits au XXIe siècle?";
            Interface["executable"] = "Fichier exécutable introuvable, veuillez réinstaller le jeu et suivre les instructions.";
            Interface["human_game"] = "Le jeu Human n'est pas installé, veuillez le réinstaller et suivre les instructions.";
            Interface["martian_game"] = "Le jeu Martian n'est pas installé, veuillez le réinstaller et suivre les instructions.";
            Interface["exit"] = "Quitter";
            Interface["editor"] = "Éditeur introuvable, veuillez réinstaller le jeu et suivre les instructions.";
            Interface["fullscreen_disable"] = "Désactivez le mode plein écran pour activer cette fonctionnalité.";
            Interface["fullscreen_detected"] = "Écran plein détecté";
            Interface["easy"] = "Facile";
            Interface["medium"] = "Moyen";
            Interface["hard"] = "Difficile";
            Interface["extreme"] = "Extrême";
            Interface["custom"] = "Personnalisé";
            //Form2.Designer.cs
            Interface["damage_reduction"] = "Modifiez la valeur du registre 'Damage reduction divisor'. La valeur par défaut est 500.";
            Interface["damage"] = "Diviseur de réduction des dommages :";
            Interface["return"] = "Retour";
            Interface["max_units"] = "Nombre maximal d'unités dans le secteur :";
            Interface["max_units_description"] = "Modifiez la valeur du registre 'Max units in sector'. La valeur par défaut est 15.";
            Interface["max_boats"] = "Nombre maximal de bateaux dans le secteur :";
            Interface["max_boats_description"] = "Modifiez la valeur du registre 'Max boats in sector'. La valeur par défaut est 5.";
            Interface["martian_open_rate"] = "Taux d'ouverture martien :";
            Interface["martian_open_rate_description"] = "Modifiez la valeur du registre 'Martian Open Rate'. La valeur par défaut est 10. (Vitesse de recherche)";
            Interface["human_open_rate"] = "Taux d'ouverture humain :";
            Interface["human_open_rate_description"] = "Modifiez la valeur du registre 'Human Open Rate'. La valeur par défaut est 20. (Vitesse de recherche)";
            Interface["pod_interval"] = "Intervalle entre les capsules (heures) :";
            Interface["pod_interval_description"] = "Modifiez la valeur du registre 'Pod Interval (hours)'. La valeur par défaut est de 24.";
            Interface["ai_hours"] = "Durée de l'IA par tour :";
            Interface["ai_hours_description"] = "Modifiez la valeur du registre 'AI Hours Per Turn'. La valeur par défaut est 5.";
            Interface["restore_description"] = "Rétablir les paramètres par défaut.";
            Interface["restore"] = "Paramètres par défaut";
            Interface["description"] = "Ces paramètres sont tous disponibles dans l'entrée de registre du jeu ; ce sont les paramètres par défaut.";
            Interface["description_suggestion"] = "Je vous suggère de les laisser tels quels, sauf si vous avez déjà joué au jeu.";
            Interface["martian_strength"] = "Tableau de puissance de l'IA Multiplicateur martien :";
            Interface["martian_strength_description"] = "Modifiez la valeur du registre 'AI strength table Martian multiplier'. La valeur par défaut est 2,000000.";
            Interface["human_strength"] = "Tableau de puissance de l'IA Multiplicateur humain :";
            Interface["human_strength_description"] = "Modifiez la valeur du registre 'AI strength table Human multiplier'. La valeur par défaut est 1,000000.";
            Interface["turret_description"] = "Ajuster la limite de construction de la tourette.";
            Interface["turret_label"] = "Limite de construction de la tourette :";
            // new values
            Interface["directplay_installed"] = "DirectPlay a été activé pour résoudre des problèmes de compatibilité.";
            Interface["directplay_title"] = "Mise à jour du système";
            Interface["instruction_button"] = "Manuel d'Instruction";
            Interface["instruction_readme"] = "Vous devriez également lire le fichier Lisez-moi d'origine, car il contient des notes ne figurant pas dans le manuel du jeu.";
            Interface["instruction_location"] = "wotw-man-french-v1.pdf";
        }
        private static void SetGerman()
        {
            CurrentLanguage = "German";
            Interface["start_human"] = "Menschliche Kampagne starten";
            Interface["start_martian"] = "Marsianer-Kampagne starten";
            Interface["config"] = "Konfigurationseinstellungen";
            Interface["launcher_running"] = "Der Launcher läuft bereits!";
            Interface["game_running"] = "Das Spiel läuft bereits, bitte beenden Sie es, bevor Sie den Launcher starten!";
            Interface["one_drive"] = "Warnung: Das Spiel ist in einem eingeschränkten Verzeichnis installiert. Bitte verschieben Sie es in ein Hauptverzeichnis wie C:\\Spiele\\Jeff Wayne's 'Der Krieg der Welten' um optimale Ergebnisse zu erzielen.";
            Interface["one_drive_warning"] = "Installationswarnung";
            Interface["fullscreen_description"] = "Vollbildmodus aktivieren oder deaktivieren. Hinweis: Das Wechseln zwischen Fenstern (Alt+Tab) führt im Vollbildmodus zum Absturz des Spiels.";
            Interface["fullscreen"] = "Vollbild";
            Interface["resolution_description"] = "Die Auflösung des Spiels.";
            Interface["resolution"] = "Auflösung";
            Interface["difficulty_description"] = "Schwierigkeitseinstellungen. (Mittel ist die Standardeinstellung)";
            Interface["difficulty"] = "Schwierigkeit";
            Interface["fog_description"] = "Nebel des Krieges aktivieren oder deaktivieren.";
            Interface["fog"] = "Nebel des Krieges";
            Interface["advanced"] = "Erweiterte Einstellungen";
            Interface["tools"] = "Entwicklungstools";
            Interface["keyboard"] = "Tastaturkürzel";
            Interface["music_playback_description"] = "Wenn Sie diese Option aktivieren, wird die Musikwiedergabe fortgesetzt, wenn das Fenster den Fokus verliert, es sei denn, Sie befinden sich im Vollbildmodus.";
            Interface["music_playback"] = "Musikwiedergabe";
            Interface["enhanced_assets_description"] = "Diese Einstellung aktiviert die erweiterten Benutzeroberflächenelemente. (Dadurch wird die Farbtiefe der Benutzeroberfläche etwas erhöht.)";
            Interface["enhanced_assets"] = "Verbesserte Grafik";
            Interface["enemy_visible_description"] = "Hiermit wird umgeschaltet, ob feindliche Streitkräfte auf der Kriegskarte sichtbar sind oder nicht.";
            Interface["enemy_visible"] = "Sichtbare feindliche Streitkräfte";
            Interface["game_name"] = "Jeff Wayne's 'Der Krieg der Welten'";
            Interface["dir_warning"] = "Warnung: Der Ordner {0} ist schreibgeschützt oder geschützt.\n\n" +
                            "\r\nDer Launcher speichert die Einstellungen möglicherweise nicht. Bitte führen Sie ihn als Administrator aus " +
                            "Deaktivieren Sie die Schreibschutzberechtigungen für den Ordner oder verschieben Sie das Spiel in einen anderen Ordner (e.g., C:\\Spiele\\).";
            Interface["dir_warning_error"] = "Berechtigungsfehler";
            Interface["alt_tab"] = "Alt+Tab wird im Vollbildmodus nicht unterstützt.\n\nMöchten Sie das Spiel neu starten?";
            Interface["alt_tab_error"] = "Alt-Tab-Fehler";
            Interface["registry_missing"] = "Der Registrierungseintrag fehlte, die Basisregistrierungseinträge wurden von Grund auf neu erstellt.\n\nBitte starten Sie das Spiel einmal, um die restlichen Registrierungseinträge zu erstellen.";
            Interface["lights"] = "Der Registry-Eintrag \"Show lights\" muss auf eins gesetzt werden, um Abstürze bei der Verwendung der Infiltrationsfähigkeit im Spiel zu verhindern. Daher wurde er zurückgesetzt.";
            Interface["colour"] = "Der Registrierungseintrag \"BPP\" muss auf 16 gesetzt werden, da ich die 8/16-Bit-Umschaltung aus der ausführbaren Datei des Spiels entfernt habe. Sie wollen es im 21. Jahrhundert doch nicht im 8-Bit-Farbmodus spielen?";
            Interface["executable"] = "Die ausführbare Datei wurde nicht gefunden. Bitte installieren Sie das Spiel neu und folgen Sie den Anweisungen.";
            Interface["human_game"] = "Das Spiel \"Human\" ist nicht installiert. Bitte installieren Sie das Spiel neu und folgen Sie den Anweisungen.";
            Interface["martian_game"] = "Das Spiel \"Martian\" ist nicht installiert. Bitte installieren Sie das Spiel neu und folgen Sie den Anweisungen.";
            Interface["exit"] = "Beenden";
            Interface["editor"] = "Der Editor wurde nicht gefunden. Bitte installieren Sie das Spiel neu und folgen Sie den Anweisungen.";
            Interface["fullscreen_disable"] = "Deaktivieren Sie den Vollbildmodus, um diese Funktion zu aktivieren.";
            Interface["fullscreen_detected"] = "Vollbildmodus erkannt";
            Interface["easy"] = "Einfach";
            Interface["medium"] = "Medium";
            Interface["hard"] = "Schwer";
            Interface["extreme"] = "Extrem";
            Interface["custom"] = "Benutzerdefiniert";
            //Form2.Designer.cs
            Interface["damage_reduction"] = "Passen Sie den Registrierungswert für den 'Damage reduction divisor' an. Der Standardwert ist 500.";
            Interface["damage"] = "Schadensreduzierungsdivisor:";
            Interface["return"] = "Zurück";
            Interface["max_units"] = "Maximale Einheiten im Sektor:";
            Interface["max_units_description"] = "Passen Sie den Registrierungswert 'Max units in sector' an. Der Standardwert ist 15.";
            Interface["max_boats"] = "Maximale Anzahl Boote im Sektor:";
            Interface["max_boats_description"] = "Passen Sie den Registrierungswert'Max boats in sector' an. Der Standardwert ist 5.";
            Interface["martian_open_rate"] = "Marsianische Öffnungsrate:";
            Interface["martian_open_rate_description"] = "Passen Sie den Registrierungswert 'Martian Open Rate' an. Der Standardwert ist 10 (Forschungsgeschwindigkeit).";
            Interface["human_open_rate"] = "Menschliche Öffnungsrate:";
            Interface["human_open_rate_description"] = "Passen Sie den Registrierungswert für die 'Human Open Rate' an. Der Standardwert ist 20 (Forschungsgeschwindigkeit).";
            Interface["pod_interval"] = "Pod-Intervall (Stunden):";
            Interface["pod_interval_description"] = "Passen Sie den Registrierungswert 'Pod Interval (hours)' an. Der Standardwert ist 24.";
            Interface["ai_hours"] = "KI-Stunden pro Runde:";
            Interface["ai_hours_description"] = "Passen Sie den Registrierungswert 'AI Hours Per Turn' an. Der Standardwert ist 5.";
            Interface["restore_description"] = "Die Einstellungen werden auf den Standardzustand zurückgesetzt.";
            Interface["restore"] = "Standardwerte";
            Interface["description"] = "Diese Einstellungen sind alle im Registrierungseintrag für das Spiel verfügbar; dies sind die Standardeinstellungen.";
            Interface["description_suggestion"] = "Ich schlage vor, die Einstellungen so zu belassen, es sei denn, Sie haben das Spiel schon einmal gespielt.";
            Interface["martian_strength"] = "KI-Stärketabelle Mars-Multiplikator:";
            Interface["martian_strength_description"] = "Passen Sie den Registrierungswert 'AI strength table Martian multiplier' an. Der Standardwert ist 2,000000.";
            Interface["human_strength"] = "KI-Stärketabelle Menschlicher Multiplikator:";
            Interface["human_strength_description"] = "Passen Sie den Registrierungswert 'AI strength table Human multiplier' an. Der Standardwert ist 1,000000.";
            Interface["turret_description"] = "Passe das Turmbaulimit an.";
            Interface["turret_label"] = "Turmbaulimit :";
            // new values
            Interface["directplay_installed"] = "DirectPlay wurde aktiviert, um Kompatibilitätsprobleme zu beheben.";
            Interface["directplay_title"] = "Systemaktualisierung";
            Interface["instruction_button"] = "Bedienungsanleitung";
            Interface["instruction_readme"] = "Sie sollten auch die ursprüngliche Readme-Datei lesen, da diese Hinweise enthält, die nicht im Handbuch des Spiels stehen.";
            Interface["instruction_location"] = "";
        }
        private static void SetItalian()
        {
            CurrentLanguage = "Italian";
            Interface["start_human"] = "Avvia la campagna umana";
            Interface["start_martian"] = "Inizia la campagna marziana";
            Interface["config"] = "Impostazioni di configurazione";
            Interface["launcher_running"] = "Il launcher è già in esecuzione!";
            Interface["game_running"] = "Il gioco è già in esecuzione, esci prima di avviare il launcher!";
            Interface["one_drive"] = "Attenzione: il gioco è installato in una posizione con restrizioni. Per ottenere risultati ottimali, spostalo in una directory principale come C:\\Giochi\\Jeff Wayne's 'La guerra dei mondi'.";
            Interface["one_drive_warning"] = "Avviso di installazione";
            Interface["fullscreen_description"] = "Attiva o disattiva la modalità a schermo intero. Nota: la combinazione di tasti Alt+Tab causa l'arresto anomalo del gioco in modalità a schermo intero.";
            Interface["fullscreen"] = "A schermo intero";
            Interface["resolution_description"] = "La risoluzione del gioco.";
            Interface["resolution"] = "Risoluzione";
            Interface["difficulty_description"] = "Impostazioni di difficoltà. (Media è l'impostazione predefinita)";
            Interface["difficulty"] = "Difficoltà";
            Interface["fog_description"] = "Attiva o disattiva la nebbia di guerra.";
            Interface["fog"] = "Nebbia di guerra";
            Interface["advanced"] = "Impostazioni avanzate";
            Interface["tools"] = "Strumenti di sviluppo";
            Interface["keyboard"] = "Scorciatoie da tastiera";
            Interface["music_playback_description"] = "Attivando questa opzione, la musica continuerà a essere riprodotta anche quando la finestra perde il focus, a meno che non sia in modalità a schermo intero.";
            Interface["music_playback"] = "Riproduzione musicale";
            Interface["enhanced_assets_description"] = "Questa impostazione abilita gli elementi grafici avanzati dell'interfaccia utente. (Ciò aggiunge una maggiore profondità di colore all'interfaccia.)";
            Interface["enhanced_assets"] = "Risorse migliorate";
            Interface["enemy_visible_description"] = "Questa opzione attiva o disattiva la visualizzazione delle forze nemiche sulla mappa di guerra.";
            Interface["enemy_visible"] = "Forze nemiche visibili";
            Interface["game_name"] = "Jeff Wayne's 'La guerra dei mondi'";
            Interface["dir_warning"] = "Attenzione: la cartella {0} è di sola lettura o protetta.\n\n" +
                            "Il programma di avvio potrebbe non salvare le impostazioni. Eseguilo come amministratore, " +
                            "Deseleziona le autorizzazioni di sola lettura per la cartella oppure sposta il gioco in una cartella diversa (e.g., C:\\Giochi\\).";
            Interface["dir_warning_error"] = "Errore di autorizzazioni";
            Interface["alt_tab"] = "La combinazione di tasti Alt+Tab non è supportata in modalità a schermo intero.\n\nVuoi riavviare il gioco?";
            Interface["alt_tab_error"] = "Errore Alt-Tab";
            Interface["registry_missing"] = "Voce del registro mancante, le voci del registro di base sono state ricreate da zero.\n\nEsegui il gioco una volta per creare le restanti voci del registro.";
            Interface["lights"] = "La voce del registro di sistema \"Show lights\" deve essere impostata su uno per evitare arresti anomali quando si utilizza l'abilità Infiltrazione nel gioco, quindi è stata reimpostata.";
            Interface["colour"] = "La voce di registro \"BPP\" deve essere impostata su 16 poiché ho rimosso l'opzione per passare da 8 a 16 bit dall'eseguibile del gioco; sicuramente non vorrai giocarci in modalità colore a 8 bit nel XXI secolo, vero?";
            Interface["executable"] = "File eseguibile non trovato. Reinstallare il gioco e seguire le istruzioni.";
            Interface["human_game"] = "Il gioco Human non è installato, si prega di reinstallarlo e seguire le istruzioni.";
            Interface["martian_game"] = "Il gioco Martian non è installato, si prega di reinstallarlo e seguire le istruzioni.";
            Interface["exit"] = "Esci";
            Interface["editor"] = "Editor non trovato, reinstalla il gioco e segui le istruzioni.";
            Interface["fullscreen_disable"] = "Disattiva la modalità a schermo intero per abilitare questa funzione.";
            Interface["fullscreen_detected"] = "Schermo intero rilevato";
            Interface["easy"] = "Facile";
            Interface["medium"] = "Medio";
            Interface["hard"] = "Difficile";
            Interface["extreme"] = "Estremo";
            Interface["custom"] = "Personalizzata";
            //Form2.Designer.cs
            Interface["damage_reduction"] = "Modificare il valore del registro di sistema 'Damage reduction divisor'. Il valore predefinito è 500.";
            Interface["damage"] = "Divisore di riduzione del danno :";
            Interface["return"] = "Ritorno";
            Interface["max_units"] = "Numero massimo di unità nel settore :";
            Interface["max_units_description"] = "Modifica il valore del registro 'Max units in sector'. Il valore predefinito è 15.";
            Interface["max_boats"] = "Numero massimo di imbarcazioni nel settore :";
            Interface["max_boats_description"] = "Regola il valore di registro 'Max boats in sector'. Il valore predefinito è 5.";
            Interface["martian_open_rate"] = "Tasso di apertura marziano :";
            Interface["martian_open_rate_description"] = "Regola il valore del registro 'Martian Open Rate'. Il valore predefinito è 10. (Velocità di ricerca)";
            Interface["human_open_rate"] = "Tasso di apertura umano :";
            Interface["human_open_rate_description"] = "Regola il valore del registro di sistema 'Human Open Rate'. Il valore predefinito è 20. (Velocità di ricerca)";
            Interface["pod_interval"] = "Intervallo tra i pod (ore) :";
            Interface["pod_interval_description"] = "Modifica il valore del registro di sistema 'Pod Interval (hours)'. Il valore predefinito è 24.";
            Interface["ai_hours"] = "Ore di IA per turno :";
            Interface["ai_hours_description"] = "Modifica il valore del registro di sistema 'AI Hours Per Turn'. Il valore predefinito è 5.";
            Interface["restore_description"] = "Ripristina le impostazioni ai valori predefiniti.";
            Interface["restore"] = "Ripristina le impostazioni predefinite";
            Interface["description"] = "Tutte queste impostazioni sono disponibili nella voce di registro relativa al gioco; queste sono le impostazioni predefinite.";
            Interface["description_suggestion"] = "Suggerisco di lasciarli così come sono, a meno che tu non abbia già giocato a questo gioco.";
            Interface["martian_strength"] = "Tabella di forza dell'IA Moltiplicatore marziano :";
            Interface["martian_strength_description"] = "Modifica il valore del registro 'AI strength table Martian multiplier'. Il valore predefinito è 2.000000.";
            Interface["human_strength"] = "Tabella di forza dell'IA Moltiplicatore umano :";
            Interface["human_strength_description"] = "Modifica il valore del registro di sistema 'AI strength table Human multiplier'. Il valore predefinito è 1,000000.";
            Interface["turret_description"] = "Regola il limite di costruzione della torretta.";
            Interface["turret_label"] = "Limite di costruzione della torretta :";
            // new values
            Interface["directplay_installed"] = "DirectPlay è stato abilitato per risolvere problemi di compatibilità.";
            Interface["directplay_title"] = "Aggiornamento di sistema";
            Interface["instruction_button"] = "Manuale di Istruzioni";
            Interface["instruction_readme"] = "Si consiglia di leggere anche il file readme originale, poiché contiene note non presenti nel manuale del gioco.";
            Interface["instruction_location"] = "wotw-man-italian-v1.pdf";
        }
        private static void SetSpanish()
        {
            CurrentLanguage = "Spanish";
            Interface["start_human"] = "Iniciar campaña humana";
            Interface["start_martian"] = "Iniciar campaña marciana";
            Interface["config"] = "Ajustes de configuración";
            Interface["launcher_running"] = "¡El lanzador ya está en funcionamiento!";
            Interface["game_running"] = "El juego ya está en ejecución, ¡ciérrelo antes de ejecutar el lanzador!";
            Interface["one_drive"] = "Advertencia: El juego está instalado en una ubicación restringida. Para obtener mejores resultados, muévalo a un directorio raíz como C:\\Juegos\\Jeff Wayne's 'La Guerra de los Mundos'.";
            Interface["one_drive_warning"] = "Advertencia de instalación";
            Interface["fullscreen_description"] = "Activa o desactiva la pantalla completa. Nota: Al usar Alt+Tab, el juego se bloquea en modo de pantalla completa.";
            Interface["fullscreen"] = "Pantalla completa";
            Interface["resolution_description"] = "La resolución del juego.";
            Interface["resolution"] = "Resolución";
            Interface["difficulty_description"] = "Ajustes de dificultad. (Medio es el predeterminado)";
            Interface["difficulty"] = "Dificultad";
            Interface["fog_description"] = "Activar o desactivar la niebla de guerra.";
            Interface["fog"] = "Niebla de guerra";
            Interface["advanced"] = "Configuración avanzada";
            Interface["tools"] = "Herramientas de desarrollo";
            Interface["keyboard"] = "Atajos de teclado";
            Interface["music_playback_description"] = "Al habilitar esta opción, la música podrá seguir reproduciéndose cuando la ventana pierda el foco, siempre que no esté en modo de pantalla completa.";
            Interface["music_playback"] = "Reproducción de música";
            Interface["enhanced_assets_description"] = "Esta configuración habilita los recursos mejorados de la interfaz de usuario. ( Esto añade un poco más de profundidad de color a la interfaz. )";
            Interface["enhanced_assets"] = "Interfaz mejorada";
            Interface["enemy_visible_description"] = "Esto activa o desactiva la visibilidad de las fuerzas enemigas en el mapa de guerra.";
            Interface["enemy_visible"] = "Fuerzas enemigas visibles";
            Interface["game_name"] = "Jeff Wayne's 'La Guerra de los Mundos'";
            Interface["dir_warning"] = "Advertencia: La carpeta {0} es de solo lectura o está protegida.\n\n" +
                            "Es posible que el lanzador no guarde la configuración. Ejecútelo como administrador, " +
                            "Desactive los permisos de solo lectura en la carpeta o mueva el juego a una carpeta diferente (por ejemplo, C:\\Juegos\\).";
            Interface["dir_warning_error"] = "Error de permisos";
            Interface["alt_tab"] = "El cambio de ventana (Alt+Tab) no es compatible en el modo de pantalla completa.\n\n¿Quieres reiniciar el juego?";
            Interface["alt_tab_error"] = "Error de Alt-Tab";
            Interface["registry_missing"] = "Falta una entrada en el registro; las entradas base del registro se han recreado desde cero.\n\nEjecute el juego una vez para crear el resto de las entradas del registro.";
            Interface["lights"] = "La entrada de registro \"Show lights\" debe estar configurada en uno para evitar bloqueos al usar la habilidad Infiltración en el juego, por lo que se ha restablecido.";
            Interface["colour"] = "La entrada de registro \"BPP\" debe estar configurada en 16, ya que he eliminado la opción de alternar entre 8 y 16 bits del ejecutable del juego. ¿Seguro que no quieres jugarlo en modo de color de 8 bits en el siglo XXI?";
            Interface["executable"] = "No se encontró el archivo ejecutable. Por favor, reinstale el juego y siga las instrucciones.";
            Interface["human_game"] = "El juego no está instalado. Por favor, reinstálelo y siga las instrucciones.";
            Interface["martian_game"] = "El juego Martian no está instalado. Por favor, reinstálelo y siga las instrucciones.";
            Interface["exit"] = "Salir";
            Interface["editor"] = "Editor no encontrado. Por favor, reinstale el juego y siga las instrucciones.";
            Interface["fullscreen_disable"] = "Desactive la pantalla completa para habilitar esta función.";
            Interface["fullscreen_detected"] = "Pantalla completa detectada";
            Interface["easy"] = "Fácil";
            Interface["medium"] = "Medio";
            Interface["hard"] = "Difícil";
            Interface["extreme"] = "Extremo";
            Interface["custom"] = "Personalizada";
            //Form2.Designer.cs
            Interface["damage_reduction"] = "Ajuste el valor del registro 'Damage reduction divisor'. El valor predeterminado es 500.";
            Interface["damage"] = "Divisor de reducción de daño :";
            Interface["return"] = "Volver";
            Interface["max_units"] = "Unidades máximas en el sector :";
            Interface["max_units_description"] = "Ajuste el valor del registro 'Max units in sector'. El valor predeterminado es 15.";
            Interface["max_boats"] = "Máximo de barcos en el sector :";
            Interface["max_boats_description"] = "Ajuste el valor del registro 'Max boats in sector'. El valor predeterminado es 5.";
            Interface["martian_open_rate"] = "Tasa de apertura marciana :";
            Interface["martian_open_rate_description"] = "Ajusta el valor del registro 'Martian Open Rate'. El valor predeterminado es 10. (Velocidad de investigación)";
            Interface["human_open_rate"] = "Tasa de apertura humana :";
            Interface["human_open_rate_description"] = "Ajusta el valor del registro 'Human Open Rate'. El valor predeterminado es 20. (Velocidad de investigación)";
            Interface["pod_interval"] = "Intervalo entre cápsulas (horas) :";
            Interface["pod_interval_description"] = "Ajuste el valor del registro 'Pod Interval (hours)'. El valor predeterminado es 24.";
            Interface["ai_hours"] = "Horas de IA por turno :";
            Interface["ai_hours_description"] = "Ajusta el valor del registro 'AI Hours Per Turn'. El valor predeterminado es 5.";
            Interface["restore_description"] = "Restaura la configuración a su estado predeterminado.";
            Interface["restore"] = "Restaurar la configuración predeterminada";
            Interface["description"] = "Estas opciones de configuración están disponibles en la entrada del registro del juego; se trata de la configuración predeterminada.";
            Interface["description_suggestion"] = "Sugiero dejarlos como están a menos que ya hayas jugado al juego anteriormente.";
            Interface["martian_strength"] = "Tabla de fuerza de IA Multiplicador marciano :";
            Interface["martian_strength_description"] = "Ajusta el valor del registro 'AI strength table Martian multiplier'. El valor predeterminado es 2.000000.";
            Interface["human_strength"] = "Tabla de fuerza de la IA Multiplicador humano :";
            Interface["human_strength_description"] = "Ajuste el valor del registro 'AI strength table Human multiplier'. El valor predeterminado es 1,000000.";
            Interface["turret_description"] = "Ajusta el límite de construcción de la torreta.";
            Interface["turret_label"] = "Límite de construcción de la torreta :";
            // new values
            Interface["directplay_installed"] = "DirectPlay ha sido habilitado para resolver problemas de compatibilidad.";
            Interface["directplay_title"] = "Actualización del sistema";
            Interface["instruction_button"] = "Manual de Instrucciones";
            Interface["instruction_readme"] = "También debería leer el archivo readme original, ya que contiene notas que no aparecen en el manual del juego.";
            Interface["instruction_location"] = "";
        }
    }
}