// MODERN (update 248, docs/MODERN.md "The documented WoWRevived.ini"): every setting the engine reads from
// WoWRevived.ini, with its default and what it does, so the file can document itself. The launcher (and wowdump
// --default-ini, for the release's WoWRevived.default.ini) adds to the ini each setting's description and, where
// it isn't set, its default as a commented example ("; Name=default"), which does nothing until it's uncommented
// or the game sets it (IniStore keeps comments, and a value set takes its example's place). The settings the
// original game reads that the engine doesn't (the platform's, the network game's, the developers' logging:
// docs/review/SETTINGS.md) aren't listed: the engine ignores them.
//
// The list was made from what the engine reads (WOWENGINE_SETTINGS_LOG, frontend/Settings.cpp, and the reads
// outside it), and the defaults are the engine's, which are the original's (docs/survey/part14.md section 4) where
// the original has the setting. Kept in the order the file is written: the engine's own sections, then the
// player's, then the original's tuning.
#pragma once
#include <string>
#include <vector>

namespace wow::settingstable
{
    struct SectionInfo { const char* name; const char* description; };
    struct Setting { const char* section; const char* name; const char* example; const char* description; };

    // a description's lines are split at '\n'
    inline constexpr SectionInfo Sections[] = {
        { "Modern", "WoWEngine's own options (the Modern Options and Modern Gameplay menus). 1 on, 0 off." },
        { "WoW Engine", "WoWEngine's window and language." },
        { "Keys", "Key bindings: a key's name as SDL names it (Up, Down, F1, Space, Backspace, R, ...).\nEmpty or left out: the game's own key." },
        { "Main", "The game's main key (the registry's 1.00.000)." },
        { "Screen", "The screen and the tool tips (the delays are in updates, 30 a second)." },
        { "Options", "Display Settings' boxes. 1 on, 0 off." },
        { "Sound\\Volume", "Sound Control's volumes, 0.0 to 1.0, and the CD music." },
        { "Sound", "Where sounds can be heard, and the sound files." },
        { "Event", "Critical event options: which events stop the war map's clock. 1 on, 0 off." },
        { "Colours", "Text colours: an index into the screen's palette." },
        { "Game", "The battle map's light." },
        { "Briefing", "" },
        { "Debriefing", "" },
        { "BuildList", "Where the Build List opens on the war map, in pixels." },
        { "Research", "" },
        { "BattleMap", "The battle map's fog of war and damage." },
        { "Demo", "" },
        { "System", "" },
        { "Debug", "The developers' switches. Leave them off to play." },
        { "Tweak", "The original's tuning values: the economy, the computer player, the sides' modifiers and more.\nChanging them changes the game; the defaults are the original's." },
        { "Vehicles", "The original's tuning for how vehicles steer and keep apart on the battle map." },
    };

    inline constexpr Setting Settings[] = {
        // [Modern]
        { "Modern", "Upgraded buildings in battle", "1", "Buildings upgraded on the war map fight at their new level in battle (the original keeps the level they were built at)." },
        { "Modern", "Unlimited building", "0", "Your factories and construction vehicles ignore the per-sector limits (OBJ.ojd), except one command building per sector." },
        { "Modern", "Bridges stay broken", "0", "A destroyed bridge stays down until a construction vehicle in its sector, with nothing else to build,\nrebuilds it (4 days' work); kept beside the save as <save>.bridges. The original builds every bridge again at\neach battle." },
        { "Modern", "Bridge control", "0", "Units crossing a bridge aim, return fire and take new orders (the original makes them wait until they're\noff it). Before update 249 this was \"Bridges fire back\", which is still read when this isn't set." },
        { "Modern", "Sticky popup menus", "0", "Popup menus stay open once the right button is let go." },
        { "Modern", "War map status strip", "0", "A strip on the war map with the sector under the pointer." },
        { "Modern", "Selection keys", "1", "In battle: Ctrl + a digit keeps a selection, the digit brings it back, Shift adds to a selection." },
        { "Modern", "More save slots", "1", "The Load and Save menus go past the game's five slots, in pages of five, up to 999." },
        { "Modern", "Battle zoom", "1", "Zoom on the battle map: +, -, the mouse wheel, R to reset." },
        { "Modern", "HUD scale", "100", "The size of the maps' panels, in percent: 50 to 300." },
        { "Modern", "Menu scale", "100", "The size of the menus, in percent: 100 up to what the screen holds." },
        { "Modern", "Enhanced assets", "0", "WoWRevived's enhanced sprites (DAT\\extra\\HD), texture packs (TEXTURES) and PNG sprite overrides\n(README-OVERRIDE.txt, README-TEXTURE-PACKS.txt)." },
        { "Modern", "Original Display Settings", "0", "Display Settings with the original's 8 / 16-bit and Lights boxes (Lights can crash infiltration)." },
        { "Modern", "Menu fade", "0", "The original's fade to black between menus." },
        { "Modern", "Discord rich presence", "1", "Show what you're playing on Discord." },
        // [WoW Engine]
        { "WoW Engine", "Windowed", "0", "Play in a window (Alt + Enter switches)." },
        { "WoW Engine", "Stretch", "0", "Stretch the picture to fill the screen instead of keeping its shape." },
        { "WoW Engine", "System Cursor", "0", "Use the system's mouse pointer instead of the game's." },
        { "WoW Engine", "Window Size", "1280,720", "The window's size, kept as it's resized (width,height). Left out: the screen size, as far as the desktop allows." },
        { "WoW Engine", "Language", "English", "The install's language (WoWLauncher sets it from the discs)." },
        { "WoW Engine", "CD Player Music Continues", "0", "The CD player's track goes on playing after its menu is left." },
        // [Keys]
        { "Keys", "War map scroll up", "Up", "" }, { "Keys", "War map scroll down", "Down", "" },
        { "Keys", "War map scroll left", "Left", "" }, { "Keys", "War map scroll right", "Right", "" },
        { "Keys", "War map top", "PageUp", "" }, { "Keys", "War map bottom", "PageDown", "" },
        { "Keys", "War map centre on sector", "Home", "" }, { "Keys", "War map unit view", "F1", "" },
        { "Keys", "War map construction view", "F2", "" }, { "Keys", "War map resource view 1", "F3", "" },
        { "Keys", "War map resource view 2", "F4", "" }, { "Keys", "War map resource view 3", "F5", "" },
        { "Keys", "War map next view", "Space", "" }, { "Keys", "War map research", "R", "" },
        { "Keys", "War map stop time", "Backspace", "" }, { "Keys", "War map message history", "M", "" },
        { "Keys", "War map panel", "Tab", "" },
        { "Keys", "Battle scroll up", "Up", "" }, { "Keys", "Battle scroll down", "Down", "" },
        { "Keys", "Battle scroll left", "Left", "" }, { "Keys", "Battle scroll right", "Right", "" },
        { "Keys", "Battle top", "PageUp", "" }, { "Keys", "Battle bottom", "PageDown", "" },
        { "Keys", "Battle centre on selection", "Home", "" }, { "Keys", "Battle previous unit", ",", "" },
        { "Keys", "Battle next unit", ".", "" }, { "Keys", "Battle halt", "Backspace", "" },
        { "Keys", "Battle minimap", "Tab", "" }, { "Keys", "Battle zoom in", "=", "" },
        { "Keys", "Battle zoom out", "-", "" }, { "Keys", "Battle zoom reset", "R", "" },
        // [Main]
        { "Main", "Game Frequency", "30", "Updates a second. The game is made for 30." },
        { "Main", "Enable Network Version", "0", "The network version's menus (network play isn't in the engine yet)." },
        { "Main", "Skip", "0", "Start straight at Start New Campaign." },
        // [Screen]
        { "Screen", "Size", "1920,1080", "The screen size (width,height), from Display Settings. Left out: the desktop's resolution on the first start." },
        { "Screen", "BPP", "16", "The original's colour depth box (only with Original Display Settings; the engine always draws in full colour)." },
        { "Screen", "Scroll Speed", "40", "How fast the maps scroll." },
        { "Screen", "Tool Tip Enable", "1", "Tool tips. 1 on, 0 off." },
        { "Screen", "Tool Tip Delay", "70", "How long the pointer rests before a tool tip shows." },
        { "Screen", "Quick Tool Tip Delay", "15", "The same, when a tool tip was shown just before." },
        { "Screen", "Slow Tool Tip Delay", "150", "The delay for the slower tool tips." },
        { "Screen", "Tool Tip Offset", "16", "How far from the pointer a tool tip shows, in pixels." },
        // [Options]
        { "Options", "Show Smoke & particles", "1", "" },
        { "Options", "Show gouraud shading", "1", "" },
        { "Options", "Show vehicle shadows", "1", "" },
        { "Options", "Show lights", "1", "" },
        // [Sound\Volume]
        { "Sound\\Volume", "Master", "1.000000", "" },
        { "Sound\\Volume", "Ambient", "1.000000", "" },
        { "Sound\\Volume", "Engine", "1.000000", "" },
        { "Sound\\Volume", "Explosions", "1.000000", "" },
        { "Sound\\Volume", "Weapons", "1.000000", "" },
        { "Sound\\Volume", "Special", "1.000000", "" },
        { "Sound\\Volume", "Speech", "1.000000", "" },
        { "Sound\\Volume", "CD", "1", "The CD music. 1 on, 0 off." },
        { "Sound\\Volume", "CD-Focus", "0", "Music playback: the music goes on while the window is out of focus or minimised." },
        // [Sound]
        { "Sound", "Minimum Audible Volume", "0.050000", "Sounds quieter than this aren't played." },
        { "Sound", "Inner border", "-320,-200,320,200", "Where a sound is heard at full volume, around the middle of the view (left,top,right,bottom at 640 x 480;\nscaled to the view). Left out: these." },
        { "Sound", "Outer border", "-640,-400,640,400", "Where a sound fades to nothing." },
        { "Sound", "Inner Ambient border", "-640,-400,640,400", "The same for background sounds." },
        { "Sound", "Outer Ambient border", "-960,-600,960,600", "" },
        { "Sound", "Search Libraries First", "0", "1: the game's sound libraries before loose sound files (README-SOUNDS.txt), which are then only used for\nsounds the libraries don't have." },
        // [Event]
        { "Event", "Command Centre Built", "0", "" },
        { "Event", "Construction Completed", "0", "" },
        { "Event", "Constructor Idle", "1", "" },
        { "Event", "Facility Idle", "1", "" },
        { "Event", "Research Completed", "1", "" },
        { "Event", "Resource Alert", "1", "" },
        { "Event", "Resource Warning", "0", "" },
        { "Event", "Travel Interrupted", "1", "" },
        { "Event", "Unit Arrived At Destination", "0", "" },
        { "Event", "Unit Manufactured", "0", "" },
        // [Colours]
        { "Colours", "Menu Heading", "7", "" }, { "Colours", "Menu Items", "4", "" }, { "Colours", "Menu Static Text", "4", "" },
        { "Colours", "Menu Glow First", "4", "" }, { "Colours", "Menu Glow Last", "9", "" }, { "Colours", "Message Text", "19", "" },
        { "Colours", "Messagebox Command", "22", "" }, { "Colours", "Messagebox Detail", "21", "" }, { "Colours", "Info Page Text", "15", "" },
        { "Colours", "Briefing Heading", "17", "" }, { "Colours", "Briefing Detail", "18", "" }, { "Colours", "Briefing Sector Name", "17", "" },
        { "Colours", "Research Current", "25", "" }, { "Colours", "Research Normal", "26", "" }, { "Colours", "Research New", "27", "" },
        { "Colours", "Research Heading", "17", "" }, { "Colours", "Research Proposal", "26", "" }, { "Colours", "Research Detail", "26", "" },
        { "Colours", "Research Specification", "26", "" }, { "Colours", "Research Enhancement", "26", "" },
        // [Game]
        { "Game", "Time Of Day", "0", "With [Debug] Mat on, this hour (1 to 23) lights every battle map; 0: the war map's clock." },
        { "Game", "ISO Brightness", "0.500000", "How brightly lights light the buildings and scenery." },
        { "Game", "WOF Brightness", "2.500000", "How brightly lights light the units." },
        // the rest
        { "Briefing", "Delay", "30", "How long the briefing shows before the battle map, in updates (30 a second)." },
        { "Debriefing", "Delay", "300", "How long the debriefing shows before the war map, in updates." },
        { "BuildList", "Top", "70", "" },
        { "BuildList", "Left", "0", "" },
        { "Research", "Human Open Rate", "20", "How fast a research plan opens in a Human game." },
        { "Research", "Martian Open Rate", "10", "The same in a Martian game." },
        { "BattleMap", "EnableFogOfWar", "1", "" },
        { "BattleMap", "Fog darkness", "0.700000", "" },
        { "BattleMap", "Fog return rate", "5", "" },
        { "BattleMap", "Fog update freq", "15", "" },
        { "BattleMap", "Fog smooth index", "128", "" },
        { "BattleMap", "Fog vert dropoff", "3", "" },
        { "BattleMap", "Fog damage delta", "0", "" },
        { "BattleMap", "Fog vert damage radius", "16", "" },
        { "BattleMap", "Damage reduction divisor", "500", "How fast a sector's scorch marks fade: 1000 / this off each point at each battle." },
        { "Demo", "Playback Files", "4", "How many input recordings are kept (playback.1 to playback.4, used in turn)." },
        { "System", "ZMapRes", "400", "The size of the battle map's depth grid's cells." },
        { "Debug", "Load scenery", "1", "Load each battle map's scenery." },
        { "Debug", "Dave", "0", "Developers' notes in WoWEngine.log (each sound's name as it's first played, and more)." },
        { "Debug", "Mat", "0", "A developers' switch: [Game] Time Of Day, and the other side's units can be ordered." },
        { "Debug", "Charlie", "0", "A developers' switch on the war map." },
        { "Debug", "Edit functionality", "0", "The developers' war map editing." },
        // [Tweak]
        { "Tweak", "Max units in sector", "15", "How many of a side's units a sector holds." },
        { "Tweak", "Max boats in sector", "5", "How many boats a sector holds." },
        { "Tweak", "Pod Interval (hours)", "24", "Hours between the Martians' pods." },
        { "Tweak", "HumanFriend Armour Modifier", "1.800000", "" }, { "Tweak", "HumanEnemy Armour Modifier", "1.800000", "" },
        { "Tweak", "MartianFriend Armour Modifier", "1.800000", "" }, { "Tweak", "MartianEnemy Armour Modifier", "1.800000", "" },
        { "Tweak", "HumanFriend Manufacture Modifier", "1.000000", "" }, { "Tweak", "HumanEnemy Manufacture Modifier", "0.850000", "" },
        { "Tweak", "MartianFriend Manufacture Modifier", "1.000000", "" }, { "Tweak", "MartianEnemy Manufacture Modifier", "0.850000", "" },
        { "Tweak", "HumanFriend Research Modifier", "0.750000", "" }, { "Tweak", "HumanEnemy Research Modifier", "0.750000", "" },
        { "Tweak", "MartianFriend Research Modifier", "0.750000", "" }, { "Tweak", "MartianEnemy Research Modifier", "0.750000", "" },
        { "Tweak", "Scenery Armour Modifier", "1.200000", "" },
        { "Tweak", "Resource Buffer", "30.000000", "" },
        { "Tweak", "Resource Drop Off Rate", "3.000000", "" },
        { "Tweak", "Distance Crippler VX", "0.500000", "" },
        { "Tweak", "Distance Crippler VY", "0.500000", "" },
        { "Tweak", "AI Aggression Value", "0.500000", "The computer player's war map tuning (the AI values below)." },
        { "Tweak", "AI Build Resfac Eagerness", "50.000000", "" }, { "Tweak", "AI Eff Threshold", "0.600000", "" },
        { "Tweak", "AI Frontline Attractor", "2.000000", "" }, { "Tweak", "AI Frontline Attractor for Defences", "2.000000", "" },
        { "Tweak", "AI Invader NFriend Mul", "3000.000000", "" }, { "Tweak", "AI Invasion Threshold PC", "150.000000", "" },
        { "Tweak", "AI Minimum Rr", "8.000000", "" }, { "Tweak", "AI Move Threshold", "1.050000", "" },
        { "Tweak", "AI Movement Temperature", "0.250000", "" }, { "Tweak", "AI Outnumber Force", "1.100000", "" },
        { "Tweak", "AI Panic Build Threshold", "500.000000", "" }, { "Tweak", "AI Retain Force", "0.200000", "" },
        { "Tweak", "AI safe sector repeller Mul", "2.000000", "" },
        { "Tweak", "AI strength table Human multiplier", "1.000000", "" }, { "Tweak", "AI strength table Martian multiplier", "2.000000", "" },
        { "Tweak", "VisRadMul", "1.500000", "How far units see, as a multiple of their vision." },
        { "Tweak", "Audible radius multiplier", "1.000000", "" },
        { "Tweak", "Quick Cancel Timeout", "5", "How long the right button is held for the popup menu's quick cancel, in updates." },
        { "Tweak", "No-build border width (vertices)", "16", "How far from the battle map's edge nothing can be built." },
        { "Tweak", "Warmap Zoomer Speed", "1.500000", "" },
        { "Tweak", "Battlemap Zoomer Speed", "2.500000", "" },
        { "Tweak", "Clunting Speed", "7.000000", "How fast the war map's counters slide into place." },
        { "Tweak", "WM clunting res", "36", "" }, { "Tweak", "WM clunting adj", "4", "" },
        { "Tweak", "Route clunting width", "4", "" }, { "Tweak", "Route clunting spacing", "4", "" },
        { "Tweak", "Res Stack XO", "4", "" }, { "Tweak", "Res Stack YO", "16", "" },
        { "Tweak", "Text Line Spacing", "4", "" },
        { "Tweak", "Water twinkles per frame", "20", "" },
        // [Vehicles]
        { "Vehicles", "maxAcc", "1.000000", "" },
        { "Vehicles", "arrivalAccMax", "0.200000", "" }, { "Vehicles", "arrivalAccWeight", "0.330000", "" }, { "Vehicles", "arrivalRadius", "1.500000", "" },
        { "Vehicles", "attractAccMax", "0.100000", "" }, { "Vehicles", "attractAccWeight", "0.001000", "" },
        { "Vehicles", "attributeAccMax", "0.200000", "" }, { "Vehicles", "attributeAccWeight", "0.300000", "" },
        { "Vehicles", "avoidanceAccMax", "0.100000", "" }, { "Vehicles", "avoidanceAccWeight", "0.150000", "" },
        { "Vehicles", "brakingAccMax", "0.200000", "" }, { "Vehicles", "brakingAccWeight", "0.100000", "" },
        { "Vehicles", "separationAccMax", "0.100000", "" }, { "Vehicles", "separationAccWeight", "0.200000", "" },
        { "Vehicles", "targAccMax", "0.150000", "" }, { "Vehicles", "targAccWeight", "0.003000", "" },
        { "Vehicles", "velMatchAccMax", "0.030000", "" }, { "Vehicles", "velMatchAccWeight", "0.080000", "" },
        { "Vehicles", "collisionCheckDist", "0.500000", "" },
        { "Vehicles", "speedCostDivisor", "25.000000", "" },
        { "Vehicles", "startTurnRate", "12", "" },
        { "Vehicles", "turnAnimRate", "0.020000", "" },
    };

    inline std::vector<std::string> Lines(const char* text)
    {
        std::vector<std::string> out;
        std::string t = text;
        if (t.empty()) return out;
        size_t start = 0;
        for (size_t nl; (nl = t.find('\n', start)) != std::string::npos; start = nl + 1) out.push_back(t.substr(start, nl - start));
        out.push_back(t.substr(start));
        return out;
    }

    // Every setting documented in an ini (IniStore, WoWPatches/Common: its Document, which adds nothing twice).
    // True if anything was added (then the caller saves it).
    template <class Ini> bool Document(Ini& ini)
    {
        bool changed = false;
        for (const Setting& s : Settings)
        {
            std::vector<std::string> sectionComment;
            for (const SectionInfo& info : Sections)
                if (std::string(info.name) == s.section) sectionComment = Lines(info.description);
            changed |= ini.Document(s.section, sectionComment, s.name, s.example, Lines(s.description));
        }
        return changed;
    }
}
