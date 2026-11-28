// MODERN (docs/MODERN.md "The engine's own text"): the strings the engine adds to the game (the Modern Options
// menu, its rows and tool tips, the save menus' page rows). They are TEXT.ojd strings like the game's own, with ids
// past the game's last (1332): WoWLauncher appends the install's language's to its TEXT.ojd (src/install), and the
// engine reads them from there, falling back to the English here when a TEXT.ojd doesn't have them.
//
// The game itself has room for 1333 ids and no more (0x414410), so a TEXT.ojd with these in must not be given to
// the original game: it would write past its table. The files in resources/ (WoWRevived's) are left as they are;
// only the copy in the engine's install gets them.
//
// The texts here are UTF-8; in TEXT.ojd they are Latin-1, as all of its text is (every character used is in it).
// The tool tips have no line breaks: the menu wraps them.
#pragma once
#include <cstddef>
#include <string>

namespace wow::enginetext
{
    enum Language { English, French, German, Italian, Spanish, Languages };

    enum Id
    {
        First = 1400,
        ModernOptions = 1400,
        UpgradedBuildings, StickyPopups, StatusStrip, SelectionKeys, MoreSaves, BattleZoom, EnhancedAssets,
        OriginalDisplay, Windowed, Stretch, SystemPointer, MusicPlayback, MenuFade, DiscordPresence,     // 1401-1414
        RecordInput, PlayRecording, HudScale, NextSaves, PreviousSaves,                                   // 1415-1419
        MenuScale,                                                                                        // 1420 (231)
        TipFirst = 1421,            // a row's tool tip is its id + 20
        TipOffset = 20,
        // the civil wars (docs/MODERN.md "The civil wars", update 236): the main menu's entries, and what the
        // Load menu shows after a civil war's name
        HumanCivilWar = 1450, MartianCivilWar, HumanCivilWarTag, MartianCivilWarTag,                     // 1450-1453
        // the debriefing's lines that name the other faction (0x4AB / 0x4AC), for a war within one
        EnemyEliminated, EnemyOccupying,                                                                  // 1454-1455
        // the Modern Gameplay menu (update 246): its rows and title; their tips are their ids + 20 (1476-1478)
        UnlimitedBuilding = 1456, BridgesFireBack, ModernGameplay,                                       // 1456-1458
        KeepBuildings = 1459,                                                                             // 1459 (266); its tip 1479
        // custom campaigns (update 246, docs/CUSTOM.md): the main menu's entry and its menu's heading, its rows
        CustomCampaigns = 1460, ResumeCustom, NoCustomCampaigns, NextCampaigns, PreviousCampaigns,      // 1460-1464
        SaveUnreadable,                                                                                   // 1465 (248)
        BridgesStayBroken,                                                                                // 1466 (249); its tip 1486
        // a civil war's sides (update 261): the menu's heading, then each side, whose tip (+20) says where it starts
        ChooseSide = 1467, HumanRebels, HumanLoyalists, MartianResistance, MartianImperialists,          // 1467-1471; tips 1488-1491
        // a custom campaign's briefing (update 264)
        BeginCampaign = 1472, NextPage, PreviousPage,                                                     // 1472-1474
        SkipBriefings = 1475,                                                                             // 1475 (266); its tip 1495
        CdMusicVolume = 1496,                                                                             // 1496 (268): Sound Control's slider
        Last = 1496,
    };

    struct Entry { int id; const char* text[Languages]; };

    inline constexpr Entry Table[] = {
        { ModernOptions, { "Modern Options", "Options modernes", "Moderne Optionen", "Opzioni moderne", "Opciones modernas" } },
        { UpgradedBuildings, { "Upgraded buildings in battle", "Bâtiments améliorés au combat", "Ausgebaute Gebäude im Kampf", "Edifici potenziati in battaglia", "Edificios mejorados en batalla" } },
        { StickyPopups, { "Popup menus stay open", "Menus contextuels fixes", "Kontextmenüs bleiben offen", "Menu a comparsa fissi", "Menús emergentes fijos" } },
        { StatusStrip, { "War map status strip", "Barre d'état de la carte", "Statusleiste der Karte", "Barra di stato della mappa", "Barra de estado del mapa" } },
        { SelectionKeys, { "Group keys and Shift select", "Touches de groupe et Maj", "Gruppentasten und Umschalt", "Tasti gruppo e Maiusc", "Teclas de grupo y Mayús" } },
        { MoreSaves, { "More save slots", "Plus d'emplacements", "Mehr Speicherplätze", "Più slot di salvataggio", "Más ranuras de guardado" } },
        { BattleZoom, { "Battle zoom (+, -, wheel, R)", "Zoom (+, -, molette, R)", "Zoom (+, -, Mausrad, R)", "Zoom (+, -, rotellina, R)", "Zoom (+, -, rueda, R)" } },
        { EnhancedAssets, { "Enhanced assets", "Graphismes améliorés", "Verbesserte Grafik", "Risorse migliorate", "Interfaz mejorada" } },
        { OriginalDisplay, { "Original display settings", "Affichage d'origine", "Originale Anzeigeoptionen", "Impostazioni video originali", "Ajustes de pantalla originales" } },
        { Windowed, { "Play in a window", "Jouer en fenêtre", "Im Fenster spielen", "Gioca in finestra", "Jugar en ventana" } },
        { Stretch, { "Stretch the picture", "Étirer l'image", "Bild strecken", "Allarga l'immagine", "Estirar la imagen" } },
        { SystemPointer, { "System mouse pointer", "Pointeur du système", "System-Mauszeiger", "Puntatore di sistema", "Puntero del sistema" } },
        { MusicPlayback, { "Music playback", "Lecture de musique", "Musikwiedergabe", "Riproduzione musicale", "Reproducción de música" } },
        { MenuFade, { "Fade between menus", "Fondu entre les menus", "Menüs überblenden", "Dissolvenza tra i menu", "Fundido entre menús" } },
        { DiscordPresence, { "Discord rich presence", "Présence Discord", "Discord-Status", "Stato su Discord", "Presencia en Discord" } },
        { RecordInput, { "Record input", "Enregistrer les actions", "Eingaben aufzeichnen", "Registra i comandi", "Grabar las acciones" } },
        { PlayRecording, { "Play a recording", "Lire un enregistrement", "Aufzeichnung abspielen", "Riproduci registrazione", "Reproducir grabación" } },
        { HudScale, { "HUD scale", "Taille de l'interface", "HUD-Größe", "Scala interfaccia", "Escala de la interfaz" } },
        { NextSaves, { "Next Saves", "Sauvegardes suivantes", "Nächste Spielstände", "Salvataggi successivi", "Partidas siguientes" } },
        { PreviousSaves, { "Previous Saves", "Sauvegardes précédentes", "Vorherige Spielstände", "Salvataggi precedenti", "Partidas anteriores" } },
        { MenuScale, { "Menu scale", "Taille des menus", "Menügröße", "Scala dei menu", "Escala de los menús" } },
        { HumanCivilWar, { "Human Civil War", "Guerre civile humaine", "Menschlicher Bürgerkrieg", "Guerra civile umana", "Guerra civil humana" } },
        { MartianCivilWar, { "Martian Civil War", "Guerre civile martienne", "Marsianischer Bürgerkrieg", "Guerra civile marziana", "Guerra civil marciana" } },
        { HumanCivilWarTag, { "(Human Civil War)", "(Guerre civile humaine)", "(Menschlicher Bürgerkrieg)", "(Guerra civile umana)", "(Guerra civil humana)" } },
        { MartianCivilWarTag, { "(Martian Civil War)", "(Guerre civile martienne)", "(Marsianischer Bürgerkrieg)", "(Guerra civile marziana)", "(Guerra civil marciana)" } },
        { EnemyEliminated, { "The enemy in this sector was eliminated.", "L'ennemi a été éliminé dans ce secteur.", "Der Feind in diesem Sektor wurde vernichtet.", "Il nemico in questo settore è stato eliminato.", "El enemigo de este sector ha sido eliminado." } },
        { EnemyOccupying, { "The enemy is occupying this sector.", "L'ennemi occupe ce secteur.", "Der Feind hält diesen Sektor besetzt.", "Il nemico occupa questo settore.", "El enemigo ocupa este sector." } },

        { UpgradedBuildings + TipOffset, {
            "Buildings upgraded on the war map fight as their new level in battle. The original game keeps them at the level they were built at.",
            "Les bâtiments améliorés sur la carte de guerre combattent à leur nouveau niveau. Le jeu d'origine les laisse au niveau de leur construction.",
            "Auf der Kriegskarte ausgebaute Gebäude kämpfen auf ihrer neuen Stufe. Das Originalspiel belässt sie auf der Stufe, auf der sie gebaut wurden.",
            "Gli edifici potenziati sulla mappa di guerra combattono al nuovo livello. Il gioco originale li lascia al livello a cui sono stati costruiti.",
            "Los edificios mejorados en el mapa de guerra luchan con su nuevo nivel. El juego original los deja en el nivel con el que se construyeron." } },
        { StickyPopups + TipOffset, {
            "The right button's menu stays open when the button is let go, and an item is chosen with a click. The original needs the button held.",
            "Le menu du bouton droit reste ouvert quand le bouton est relâché, et un élément se choisit d'un clic. Le jeu d'origine demande de garder le bouton enfoncé.",
            "Das Menü der rechten Maustaste bleibt offen, wenn die Taste losgelassen wird, und ein Eintrag wird per Klick gewählt. Im Original muss die Taste gehalten werden.",
            "Il menu del tasto destro resta aperto quando il tasto viene rilasciato e una voce si sceglie con un clic. Nell'originale il tasto va tenuto premuto.",
            "El menú del botón derecho sigue abierto al soltar el botón y una opción se elige con un clic. El original exige mantener el botón pulsado." } },
        { StatusStrip + TipOffset, {
            "Shows the name of the sector or button under the mouse along the bottom of the war map.",
            "Affiche en bas de la carte de guerre le nom du secteur ou du bouton sous la souris.",
            "Zeigt am unteren Rand der Kriegskarte den Namen des Sektors oder der Schaltfläche unter der Maus.",
            "Mostra in fondo alla mappa di guerra il nome del settore o del pulsante sotto il mouse.",
            "Muestra en la parte inferior del mapa de guerra el nombre del sector o del botón bajo el ratón." } },
        { SelectionKeys + TipOffset, {
            "In battle: Ctrl and a digit keeps the selection, the digit brings it back, pressed again it looks at it. Shift adds to the selection.",
            "Au combat : Ctrl et un chiffre mémorisent la sélection, le chiffre la rappelle, une seconde pression la montre. Maj ajoute à la sélection.",
            "Im Kampf: Strg und eine Ziffer merkt sich die Auswahl, die Ziffer holt sie zurück, nochmals gedrückt zeigt sie sie. Umschalt fügt zur Auswahl hinzu.",
            "In battaglia: Ctrl e una cifra memorizzano la selezione, la cifra la richiama, premuta di nuovo la mostra. Maiusc aggiunge alla selezione.",
            "En batalla: Ctrl y un número guardan la selección, el número la recupera y, pulsado otra vez, la muestra. Mayús añade a la selección." } },
        { MoreSaves + TipOffset, {
            "The Load and Save menus turn pages of five saves instead of stopping at five.",
            "Les menus de chargement et de sauvegarde tournent des pages de cinq sauvegardes au lieu de s'arrêter à cinq.",
            "Die Menüs zum Laden und Speichern blättern in Seiten zu je fünf Spielständen, statt bei fünf aufzuhören.",
            "I menu di caricamento e salvataggio sfogliano pagine di cinque salvataggi invece di fermarsi a cinque.",
            "Los menús de cargar y guardar pasan páginas de cinco partidas en lugar de detenerse en cinco." } },
        { BattleZoom + TipOffset, {
            "Zoom the battle map with + and - or the mouse wheel. R puts it back.",
            "Zoome sur la carte de combat avec + et - ou la molette. R rétablit la vue.",
            "Zoomt die Schlachtkarte mit + und - oder dem Mausrad. R stellt die Ansicht wieder her.",
            "Ingrandisce la mappa di battaglia con + e - o con la rotellina. R ripristina la vista.",
            "Amplía el mapa de batalla con + y - o con la rueda del ratón. R restablece la vista." } },
        { EnhancedAssets + TipOffset, {
            "This setting enables the enhanced user interface assets. ( This adds a little more colour depth to the interface. ) From the next start.",
            "Ce paramètre active les ressources d'interface utilisateur améliorées. (Cela ajoute un peu plus de profondeur de couleur à l'interface.) Au prochain démarrage.",
            "Diese Einstellung aktiviert die erweiterten Benutzeroberflächenelemente. (Dadurch wird die Farbtiefe der Benutzeroberfläche etwas erhöht.) Ab dem nächsten Start.",
            "Questa impostazione abilita gli elementi grafici avanzati dell'interfaccia utente. (Ciò aggiunge una maggiore profondità di colore all'interfaccia.) Dal prossimo avvio.",
            "Esta configuración habilita los recursos mejorados de la interfaz de usuario. ( Esto añade un poco más de profundidad de color a la interfaz. ) Desde el próximo inicio." } },
        { OriginalDisplay + TipOffset, {
            "Puts back the 8 bit and 16 bit colour choices and the lights switch in Display Settings. They do nothing useful on a modern system.",
            "Rétablit les choix de couleurs 8 bits et 16 bits et l'option des lumières dans les paramètres d'affichage. Ils ne servent à rien sur un système moderne.",
            "Stellt die Auswahl von 8-Bit- und 16-Bit-Farben und den Lichtschalter in den Anzeigeoptionen wieder her. Auf einem modernen System sind sie nutzlos.",
            "Ripristina le scelte di colore a 8 e 16 bit e l'opzione delle luci nelle impostazioni video. Su un sistema moderno non servono a nulla.",
            "Recupera las opciones de color de 8 y 16 bits y el interruptor de luces en los ajustes de pantalla. En un sistema moderno no sirven de nada." } },
        { Windowed + TipOffset, {
            "Play in a window instead of filling the screen. Alt and Enter also switches between the two.",
            "Joue dans une fenêtre au lieu de remplir l'écran. Alt et Entrée basculent aussi entre les deux.",
            "Spielt in einem Fenster statt im Vollbild. Alt und Eingabe wechselt ebenfalls zwischen beiden.",
            "Gioca in una finestra invece che a schermo intero. Anche Alt e Invio passano dall'una all'altro.",
            "Juega en una ventana en lugar de ocupar toda la pantalla. Alt e Intro también cambian entre ambas." } },
        { Stretch + TipOffset, {
            "Stretch the picture to fill the whole screen. Off, it keeps its shape, with bars at the sides.",
            "Étire l'image pour remplir tout l'écran. Désactivé, elle garde ses proportions, avec des bandes sur les côtés.",
            "Streckt das Bild auf den ganzen Bildschirm. Ausgeschaltet behält es seine Form, mit Balken an den Seiten.",
            "Allarga l'immagine fino a riempire lo schermo. Se disattivato, mantiene le proporzioni, con bande ai lati.",
            "Estira la imagen hasta llenar la pantalla. Desactivado, conserva su proporción, con bandas a los lados." } },
        { SystemPointer + TipOffset, {
            "Use the system's mouse pointer instead of the game's own.",
            "Utilise le pointeur de souris du système au lieu de celui du jeu.",
            "Verwendet den Mauszeiger des Systems statt des spieleigenen.",
            "Usa il puntatore del mouse di sistema invece di quello del gioco.",
            "Usa el puntero del ratón del sistema en lugar del propio del juego." } },
        { MusicPlayback + TipOffset, {
            "Enabling this will allow the music to continue playing when the window loses focus.",
            "L'activation de cette option permettra à la musique de continuer à jouer lorsque la fenêtre perd le focus.",
            "Wenn dies aktiviert ist, wird die Musik weitergespielt, wenn das Fenster den Fokus verliert.",
            "L'abilitazione di questa opzione consentirà alla musica di continuare a suonare quando la finestra perde il focus.",
            "Activar esto permitirá que la música siga sonando cuando la ventana pierda el foco." } },
        { MenuFade + TipOffset, {
            "The original game's short fade through a flat colour each time a menu changes.",
            "Le bref fondu du jeu d'origine par une couleur unie à chaque changement de menu.",
            "Die kurze Überblendung des Originalspiels über eine Farbfläche bei jedem Menüwechsel.",
            "La breve dissolvenza del gioco originale attraverso un colore uniforme a ogni cambio di menu.",
            "El breve fundido del juego original a través de un color liso cada vez que cambia el menú." } },
        { DiscordPresence + TipOffset, {
            "Shows on Discord what you are doing in the game. From the next start.",
            "Affiche sur Discord ce que vous faites dans le jeu. Au prochain démarrage.",
            "Zeigt auf Discord, was Sie gerade im Spiel tun. Ab dem nächsten Start.",
            "Mostra su Discord che cosa stai facendo nel gioco. Dal prossimo avvio.",
            "Muestra en Discord lo que haces en el juego. Desde el próximo inicio." } },
        { RecordInput + TipOffset, {
            "Records every key and mouse action from now into Playback.1 in the game folder, the original game's own demo recording. Untick to stop.",
            "Enregistre dès maintenant chaque touche et action de la souris dans Playback.1, dans le dossier du jeu : l'enregistrement de démo du jeu d'origine. Décochez pour arrêter.",
            "Zeichnet ab jetzt jede Taste und Mausaktion in Playback.1 im Spielordner auf, die Demoaufzeichnung des Originalspiels. Zum Beenden abwählen.",
            "Registra da ora ogni tasto e azione del mouse in Playback.1 nella cartella del gioco, la registrazione demo del gioco originale. Deseleziona per fermare.",
            "Graba desde ahora cada tecla y acción del ratón en Playback.1, en la carpeta del juego: la grabación de demostración del juego original. Desmarca para detenerla." } },
        { PlayRecording + TipOffset, {
            "Plays a recording back. Start it from the same place the recording was made. Escape stops it.",
            "Rejoue un enregistrement. Lancez-le de l'endroit où il a été fait. Échap l'arrête.",
            "Spielt eine Aufzeichnung ab. Starten Sie sie an derselben Stelle, an der sie aufgenommen wurde. Esc beendet sie.",
            "Riproduce una registrazione. Avviala dallo stesso punto in cui è stata fatta. Esc la ferma.",
            "Reproduce una grabación. Iníciala desde el mismo sitio en que se hizo. Esc la detiene." } },
        { HudScale + TipOffset, {
            "The size of the maps' panels: the minimap, the icons, the menus and the calendar.",
            "La taille des panneaux des cartes : la minicarte, les icônes, les menus et le calendrier.",
            "Die Größe der Kartenanzeigen: Minikarte, Symbole, Menüs und Kalender.",
            "La dimensione dei pannelli delle mappe: la minimappa, le icone, i menu e il calendario.",
            "El tamaño de los paneles de los mapas: el minimapa, los iconos, los menús y el calendario." } },
        { UnlimitedBuilding, { "Unlimited building", "Construction illimitée", "Unbegrenzter Bau", "Costruzione illimitata", "Construcción ilimitada" } },
        { BridgesFireBack, { "Full control on bridges", "Contrôle total sur les ponts", "Volle Kontrolle auf Brücken", "Controllo totale sui ponti", "Control total en los puentes" } },
        { CustomCampaigns, { "Custom Campaigns", "Campagnes personnalisées", "Eigene Feldzüge", "Campagne personalizzate", "Campañas personalizadas" } },
        { ResumeCustom, { "Resume Custom Campaign", "Reprendre une campagne personnalisée", "Eigenen Feldzug fortsetzen", "Riprendi campagna personalizzata", "Reanudar campaña personalizada" } },
        { NoCustomCampaigns, { "No custom campaigns", "Aucune campagne personnalisée", "Keine eigenen Feldzüge", "Nessuna campagna personalizzata", "No hay campañas personalizadas" } },
        { BeginCampaign, { "Begin Campaign", "Commencer la campagne", "Feldzug beginnen", "Inizia la campagna", "Comenzar la campaña" } },
        { NextPage, { "Next Page", "Page suivante", "Nächste Seite", "Pagina successiva", "Página siguiente" } },
        { PreviousPage, { "Previous Page", "Page précédente", "Vorherige Seite", "Pagina precedente", "Página anterior" } },
        { ChooseSide, { "Choose Side", "Choisir son camp", "Seite wählen", "Scegli la fazione", "Elegir bando" } },
        { HumanRebels, { "The Rebels", "Les Rebelles", "Die Rebellen", "I Ribelli", "Los Rebeldes" } },
        { HumanLoyalists, { "The Loyalists", "Les Loyalistes", "Die Loyalisten", "I Lealisti", "Los Leales" } },
        { MartianResistance, { "Martian Resistance", "Résistance martienne", "Marsianischer Widerstand", "Resistenza marziana", "Resistencia marciana" } },
        { MartianImperialists, { "Martian Imperialists", "Impérialistes martiens", "Marsianische Imperialisten", "Imperialisti marziani", "Imperialistas marcianos" } },
        { HumanRebels + TipOffset, {
            "Scotland's forces, from a new base in the Grampian Mountains, against London.",
            "Les forces de l'Écosse, depuis une nouvelle base dans les monts Grampians, contre Londres.",
            "Schottlands Truppen, von einer neuen Basis in den Grampian Mountains aus, gegen London.",
            "Le forze della Scozia, da una nuova base nei monti Grampiani, contro Londra.",
            "Las fuerzas de Escocia, desde una nueva base en los montes Grampianos, contra Londres." } },
        { HumanLoyalists + TipOffset, {
            "London's forces, holding the rest of Britain, against the rising in Scotland.",
            "Les forces de Londres, qui tiennent le reste de la Grande-Bretagne, contre le soulèvement en Écosse.",
            "Londons Truppen, die den Rest Britanniens halten, gegen den Aufstand in Schottland.",
            "Le forze di Londra, che tengono il resto della Gran Bretagna, contro la rivolta in Scozia.",
            "Las fuerzas de Londres, que dominan el resto de Gran Bretaña, contra el alzamiento en Escocia." } },
        { MartianResistance + TipOffset, {
            "The Martians who would spare the Earth, from London: their weed grows green.",
            "Les Martiens qui épargneraient la Terre, depuis Londres : leur herbe pousse verte.",
            "Die Marsianer, die die Erde verschonen wollen, von London aus: ihr Kraut wächst grün.",
            "I marziani che vorrebbero risparmiare la Terra, da Londra: la loro erba cresce verde.",
            "Los marcianos que quieren salvar la Tierra, desde Londres: su hierba crece verde." } },
        { MartianImperialists + TipOffset, {
            "The Martians who hold to the conquest, from their base in the Grampian Mountains: the red weed spreads.",
            "Les Martiens fidèles à la conquête, depuis leur base des monts Grampians : l'herbe rouge se répand.",
            "Die Marsianer, die an der Eroberung festhalten, von ihrer Basis in den Grampian Mountains aus: das rote Kraut breitet sich aus.",
            "I marziani fedeli alla conquista, dalla loro base nei monti Grampiani: l'erba rossa si diffonde.",
            "Los marcianos fieles a la conquista, desde su base en los montes Grampianos: la hierba roja se extiende." } },
        { KeepBuildings, { "Enemy bases outlast their HQ", "Les bases ennemies survivent à leur QG", "Feindliche Basen überdauern ihr HQ", "Le basi nemiche sopravvivono al loro QG", "Las bases enemigas sobreviven a su cuartel" } },
        { KeepBuildings + TipOffset, {
            "When the enemy's command building is destroyed, its other buildings and its units stay to be fought, instead of exploding.",
            "Quand le bâtiment de commandement ennemi est détruit, ses autres bâtiments et ses unités restent à combattre au lieu d'exploser.",
            "Wird das feindliche Kommandogebäude zerstört, bleiben seine anderen Gebäude und Einheiten stehen und müssen bekämpft werden, statt zu explodieren.",
            "Quando l'edificio di comando nemico è distrutto, gli altri edifici e le unità restano da combattere invece di esplodere.",
            "Cuando se destruye el edificio de mando enemigo, sus demás edificios y unidades siguen en pie para combatirlos, en vez de estallar." } },
        { CdMusicVolume, { "CD Music Volume", "Volume de la musique CD", "CD-Musiklautstärke", "Volume musica CD", "Volumen de la música del CD" } },
        { SkipBriefings, { "Skip briefings", "Passer les briefings", "Einsatzbesprechungen überspringen", "Salta i briefing", "Omitir las sesiones informativas" } },
        { SkipBriefings + TipOffset, {
            "Goes straight from the war map into a battle and straight back afterwards, without the briefing and debriefing screens.",
            "Passe directement de la carte de guerre à la bataille et revient directement ensuite, sans les écrans de briefing et de débriefing.",
            "Wechselt direkt von der Kriegskarte in die Schlacht und danach direkt zurück, ohne Einsatz- und Nachbesprechung.",
            "Passa direttamente dalla mappa di guerra alla battaglia e poi torna subito indietro, senza le schermate di briefing e debriefing.",
            "Pasa directamente del mapa de guerra a la batalla y vuelve directamente después, sin las pantallas de información previa y posterior." } },
        { BridgesStayBroken, { "Bridges stay broken", "Les ponts restent détruits", "Brücken bleiben zerstört", "I ponti restano distrutti", "Los puentes siguen destruidos" } },
        { BridgesStayBroken + TipOffset, {
            "A destroyed bridge stays down until a construction vehicle in the sector, with nothing else to build, rebuilds it.",
            "Un pont détruit le reste jusqu'à ce qu'un véhicule de construction du secteur, sans autre travail, le reconstruise.",
            "Eine zerstörte Brücke bleibt zerstört, bis ein Baufahrzeug im Sektor ohne andere Arbeit sie wieder aufbaut.",
            "Un ponte distrutto resta tale finché un veicolo da costruzione del settore, senza altro lavoro, non lo ricostruisce.",
            "Un puente destruido sigue así hasta que un vehículo de construcción del sector, sin otro trabajo, lo reconstruye." } },
        { SaveUnreadable, { "This campaign couldn't be read", "Impossible de lire cette campagne", "Dieser Feldzug konnte nicht gelesen werden", "Impossibile leggere questa campagna", "No se pudo leer esta campaña" } },
        { NextCampaigns, { "Next Campaigns", "Campagnes suivantes", "Weitere Feldzüge", "Campagne successive", "Campañas siguientes" } },
        { PreviousCampaigns, { "Previous Campaigns", "Campagnes précédentes", "Vorherige Feldzüge", "Campagne precedenti", "Campañas anteriores" } },
        { ModernGameplay, { "Modern Gameplay", "Jeu moderne", "Modernes Spiel", "Gioco moderno", "Juego moderno" } },
        { ModernGameplay + TipOffset, {
            "Changes to the rules of play: upgraded buildings in battle, unlimited building, control on bridges, bridges staying broken.",
            "Changements des règles : bâtiments améliorés au combat, construction illimitée, contrôle sur les ponts, ponts qui restent détruits.",
            "Geänderte Spielregeln: ausgebaute Gebäude im Kampf, unbegrenzter Bau, Kontrolle auf Brücken, zerstörte Brücken bleiben zerstört.",
            "Modifiche alle regole: edifici potenziati in battaglia, costruzione illimitata, controllo sui ponti, ponti che restano distrutti.",
            "Cambios en las reglas: edificios mejorados en batalla, construcción ilimitada, control en los puentes, puentes que siguen destruidos." } },
        { UnlimitedBuilding + TipOffset, {
            "Your factories and construction vehicles ignore the limits on how many of each thing a sector can hold.",
            "Vos usines et véhicules de construction ignorent la limite de chaque objet par secteur.",
            "Ihre Fabriken und Baufahrzeuge ignorieren die Höchstzahl jedes Objekts pro Sektor.",
            "Le tue fabbriche e i veicoli da costruzione ignorano il limite di ogni oggetto per settore.",
            "Tus fábricas y vehículos de construcción ignoran el límite de cada objeto por sector." } },
        { BridgesFireBack + TipOffset, {
            "Units crossing a bridge aim, return fire and take new orders, instead of waiting until they are off it.",
            "Les unités sur un pont visent, ripostent et reçoivent de nouveaux ordres sans attendre d'en être sorties.",
            "Einheiten auf einer Brücke zielen, erwidern das Feuer und nehmen neue Befehle an, statt zu warten.",
            "Le unità su un ponte mirano, rispondono al fuoco e accettano nuovi ordini senza aspettare.",
            "Las unidades sobre un puente apuntan, devuelven el fuego y aceptan órdenes nuevas sin esperar." } },
        { MenuScale + TipOffset, {
            "The size of the menus, up to what fits the screen: 200% doubles every pixel.",
            "La taille des menus, jusqu'à ce que permet l'écran : 200 % double chaque pixel.",
            "Die Größe der Menüs, bis sie den Bildschirm füllen: 200 % verdoppelt jedes Pixel.",
            "La dimensione dei menu, fin dove lo schermo lo permette: 200% raddoppia ogni pixel.",
            "El tamaño de los menús, hasta lo que cabe en la pantalla: 200 % duplica cada píxel." } },
    };

    inline const char* Find(int id, Language language = English)
    {
        for (const Entry& e : Table) if (e.id == id) return e.text[language];
        return nullptr;
    }

    // UTF-8 (the texts above) to Latin-1 (TEXT.ojd's bytes). Every character above is in Latin-1.
    inline std::string Latin1(const char* utf8)
    {
        std::string out;
        for (const unsigned char* c = reinterpret_cast<const unsigned char*>(utf8); *c; c++)
        {
            if (*c < 0x80) out += char(*c);
            else if ((*c & 0xE0) == 0xC0 && c[1]) { out += char(((*c & 0x1F) << 6) | (c[1] & 0x3F)); c++; }
            else out += '?';
        }
        return out;
    }
}
