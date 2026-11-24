<div align="center">
  <img src="images/discord/banner.jpg" alt="WoW Revived">
</div>

# WoW Revived

The definitive preservation patch and revival project for the classic Rage Software game Jeff Wayne's 'The War Of The Worlds' released in 1998.

**NEW : WoWEngine** - the game's engine rebuilt from the ground up in modern C++ and SDL3, natively for Windows, macOS and Linux. It runs on the original disc files, aims to play exactly like the original, and includes all of the features and file fixes made for WoWRevived, plus a Modern Options menu with battle zoom, HUD scaling, group keys, more save slots and more. See [Decomp/Recomp - WoWEngine](#decomprecomp---wowengine) below.

<table>
  <tr>
    <td align="center"><img src="images/wowengine/engine-warmap-640.png" alt="War Map - 640x480"><br>Original : 640x480</td>
    <td align="center"><img src="images/wowengine/engine-warmap-1080.jpg" alt="War Map - 1920x1080"><br>WoWEngine : 1920x1080</td>
  </tr>
</table>

**OLD : WoWRevived for the original executable**

The game is usually installed to "C:\Program Files (x86)\Jeff Wayne's 'The War Of The Worlds'", but a folder you can write to without administrator rights, such as "C:\Games\Jeff Wayne's 'The War Of The Worlds'", is recommended. The game's settings are saved next to it in WoWRevived.ini, so the folder can be moved or copied for modding, preservation, or portable use.

**Download and mount the disc images or insert the discs one by one.** https://archive.org/details/jeffwaynewowgame_202110

**To mount the disc images, you may need Daemon Tools Lite.** https://www.daemon-tools.cc/eng/products/dtlite

**IMPORTANT : Do NOT use the installer, just follow the instructions below.**

**IMPORTANT : Do NOT launch WoW_patched.exe or WoW_network.exe directly!**

- 1 : Copy the Human disc contents to your installation folder.
- 2 : Copy the contents of the Martian disc to your installation folder ( when prompted, choose *not* to replace files, or do, it doesn't matter anymore, but fewer writes are better for SSD longevity )
- 3 : Download the latest version from the Releases page, extract the contents and place the files in your install folder making sure to replace existing files.
- 4 : Right-click "WoWLauncher.exe" and choose **Send to > Desktop (create shortcut)**, then launch it and enjoy the game!

**If you need a visual guide for installation, watch the YouTube video.** https://www.youtube.com/watch?v=tDKG9-QHSSY

LANGUAGES : Language packs are now stand-alone releases that ship with the relevant files required to convert any language installation to any other language

SETTINGS : The game's settings are kept in WoWRevived.ini in the install folder instead of the registry, so the installation is fully portable and the launcher no longer needs administrator permissions. Settings from an older version are imported from the registry automatically the first time the launcher runs. ( On Windows 11 the launcher only asks for administrator permission once, to enable DirectPlay. )

NOTE : If you choose to make a language patch using the text editor, feel free to send the "TEXT.ojd" file my way and I will add it to the releases.

WANTED : Regional Manuals : We are looking for high-quality scans of the original physical German (DE) and Spanish (ES) manuals. If you own a physical copy and can provide a scan, please join the Discord or open an Issue!

# To-Do List

Current progress is about as far as the project is likely to go, I spent a lot of time on this project trying to make sure the game runs perfectly on modern systems and fixing bugs in the original game where I could.

However I am still going to keep track of any issues with the project here, for myself or future developers to take note of.

Main features

- In the Spanish release, the Scout and Fighting machine voice lines are in Italian for some reason.
- Edinburgh and Southampton have issues spawning Ironclad vessels when entering the battlemap due to not having enough space for a full compliment of 5 sea units.
	- In order to do this, the terrain import and export system needs revising.
- Scrolling to the very edge of some maps crashes due to a fault in the BLIT routine.
- In the top left of the War Map when in construction view (F2), an unused empty/template building background is displayed alongside a number, the number varies, this can be seen in both the Human and the Martian campaign.
	- I believe this is some type of sentinel unit that represents the entire faction though I am not certain yet.

Toolkit features

- Revise terrain import and export pipeline in order to plan for fixing coastal maps sea unit spawn zones.
- Find remaining shader tables for the few remaining sprites that haven't been located in the sprite viewer.
- Correct terrain preview colouring in the terrain viewer.
- Fix remaining bugs in the building viewer.
- Finish the save editor and map editor.

Potentially cancelled features

- Add "Now Playing : Track Name" message pumped to the in-game message log.
	- Potentially simple but means tracking down exactly how it is utilised because the last 10 message log entries are baked into save files, probably just string length would be a factor.
- In-Game UI colour picker accessibility options.
	- Requires tracking palette indexes referenced within the executable, which are then mapped to palettes through the colour entry value in the game settings via OBJ.ojd and shader tables by the executable to arrive at a colour.
	- Colour range would be entirely limited to all of the colours across all of the palettes as changing colours within palettes could impact sprite rendering.
	- After tracking them all down, the launcher would have to patch the executable with the new palette index and where applicable update the game settings ( WoWRevived.ini ) with the new object index.
- Zoom feature implementation.

# Road Map

The road map for this project.
- [✅ 0 : Main Game Functionality](#main-game-functionality) ( 100% Complete )
	- Iron changed to say Steel in multiple entries in the TEXT.ojd file.
	- Leading space added to some text to improve appearance in the Critical Event Options menu.
	- Missing entry created for the Credits button in the options menu.
	- Build List screen location is now dynamically set as per the resolution setting chosen in the launcher.
	- Missing pixels have been restored for some building sprites.
	- Cotton Factory COTTFACT.IOB rendering problem with the smokestacks has been fixed.
	- Violent stretching of the Human Airship unit has been fixed, it seems this was fixed in the original game files but hadn't been renamed from the cancelled ZEPPLIN3.WOF to AIRSHIP3.WOF in the game files.
	- Alt-tabbing now works as cnc-ddraw is working with the latest version of the smacker wrapper.
	- Custom networking executable included and accessible via the launcher when networking is enabled.
		- WoW_network.exe is not Rage's retail executable: it is a custom build with a working network stack, passed on by someone who was in contact with one of the developers. Its memory layout is completely different from the retail build, so the patch DLLs verify every address against the running build before touching it.
	- A hole in Sector 22 South Wales terrain geometry has been patched.
	- Entering or exiting the menu no longer stops the music from playing like it did in the original game.
	- Discord rich presence features implemented with multiple states. ( Main Menu, CD Player Track Name & Image, Human Campaign, Martian Campaign & Network Campaign )
		- These also work in the network executable, where multiplayer games show the side being played.
	- Game executables no longer requires administrator privileges to launch.
	- Various executable edits:
		- 1\. 1 byte to skip the "NO CD INSERTED" message in the CD Player menu.
		- 2\. 2 bytes to skip creation of the 8/16 bit colour toggles in the "Display Settings" menu.
			- this is because the 8 bit colour mode doesn't work on modern systems.
		- 3\. 2 bytes to skip creation of the "Lights" toggle in the "Display Settings" menu.
			- this prevents a crash from occuring when using the infiltration skill in-game.
		- 4\. 1 byte to skip checking for the original .smk movie files to save storage space, this patch also allows my movies to play through the wrapper.
		- 5\. 9 bytes to redirect to the winmm shim to allow localised playback of the games music.
		- 6\. 2 bytes to skip creation of the resolution slider which doesn't change the resolution without restarting the game.
		- 7\. 12 bytes changed to adjust the drag select border size from 640x640 to 2048x2048 in the battle map.
	- Languages supported:
		- 1\. English
		- 2\. French
		- 3\. German
		- 4\. Italian
		- 5\. Spanish
	- Known "bugs" which I consider non-issues for the time-being.
		- Occasional crashes on startup or during gameplay which are caused by a bug in the windows compatibility layer. ( specifically apphelp.dll )
- [✅ 1 : Custom Launcher](#custom-launcher) ( 100% Complete )
	- Advanced settings options, now including the ability to adjust the turret build limit.
	- Portable settings: every registry call the game makes is redirected to WoWRevived.ini in the install folder, which both the vanilla and network executables can read. ( previously the registry entry types had to be switched between the two, as the network executable stores some entries as dwords, which caused the vanilla executable to start up at 320x240 )
	- Custom keyboard shortcut settings. (WIP - cancelled for now)
	- Automatic regeneration of the base settings, and a one-time import of settings from older registry based installs.
	- Automatic cleanup of unnecessary files and Smackw32.dll moved to main directory.
	- Custom pre-set difficulty settings. (Easy, Medium[default], Hard, Extreme)
	- Dynamic asset swapping pipeline with custom assets curated for each supported resolution.
	- Automatic Windows 11 DirectPlay setup.
	- Automatic Compatibility settings applied.
	- Inner and outer, regular and ambient sound borders now updated through the launcher according to the users chosen resolution.
	- Resolution list is filtered to only show resolutions supported by the monitor.
	- Optional accessibility option enhanced minimaps toggle, doubling the size of the battlemap minimaps. (3D View)
	- New native working resolutions added.
		- 1280x768 &nbsp;&nbsp;&nbsp;(15:9)
		- 1280x1024	&nbsp;&nbsp;(5:4)
		- 1360x768 &nbsp;&nbsp;&nbsp;(16:9)
		- 1366x768 &nbsp;&nbsp;&nbsp;(16:9)
		- 1600x900 &nbsp;&nbsp;&nbsp;(16:9)
		- 1600x1024	&nbsp;&nbsp;(5:4)
		- 1680x1050	&nbsp;&nbsp;(16:10)
		- 1920x1080	&nbsp;&nbsp;(16:9)
	- Overclocked resolutions added. ( These resolutions are scaled from the nearest matching aspect ratio and resolution. )
		- Every resolution the monitor reports is supported. ( Overclocked resolutions are only available when using fullscreen mode. )
- [✅ 2 : File Extractor](#file-extractor) ( 100% Complete )
	- .WoW archives can be previewed and their contents extracted.
	- Waveform preview, play and replace sound files from .WoW archives.
	- Decompressing files optional but is enabled by default.
- [✅ 3 : Sprite Viewer](#sprite-viewer) ( 99% Complete )
	- Sprite viewer can extract and replace game sprites.
	- Export original palette and palette with the shader tables applied. ( this feature is currently partially broken and hidden )
	- Preview with the original palette or with the shader tables applied. ( shader tables applied is how it looks in-game )
	- Replace single frame and multi-frame sprites with custom ones.
	- Quantisation of imported images will reduce them to the closest equivalent colour found in the palette through the associated shader table.
	- Known "bugs" with the Sprite Viewer. ( The remaining 1% )
		- 1\. Shader tables are not applied to all sprites yet, only a very small number of shader tables are left to be identified.
- [✅ 4 : Unit Viewer](#unit-viewer) ( 100% Complete )
	- Unit viewer can export .WOF models and their embedded textures.
	- Ability to import custom models.
	- Preview the units texture.
- [✅ 5 : Terrain Viewer](#terrain-viewer) ( 95% Complete )
	- Terrain viewer can export .CLS geometry and their tile indexed texture map.
	- Ability to import custom geometry.
	- Preview the terrains tilemap.
	- Known "bugs" with the Terrain Viewer:
		- 1\. Not heavily tested beyond export and import.
		- 2\. Colour preview isn't quite right.
- [✅ 6 : Building Viewer](#building-viewer) ( 90% Complete )
	- Building viewer can export .IOB BSP geometry.
	- Ability to export building sprites.
	- Import currently only allows for replacing existing pixels in a sprite due to run-length encoding.
	- Known "bugs" with the Building Viewer. ( The remaining 10% )
		- Importing has a bug that shifts some sprites.
- [✅ 7 : Font Viewer](#font-viewer) ( 100% Complete )
	- Font viewer can export .FNT files to .png images.
	- Ability to import edited fonts.
- [❌ 8 : Save Editor](#save-editor) ( 25% Complete )
	- Save Name, Time & Date editing functionality implemented, along with Swap Sides and Delete Save buttons.
	- Override standard limit of 1753 as the minimum date and set the year manually to as low as year zero.
	- Sector & Area names loaded dynamically from TEXT.ojd to support the regional releases.
	- Building, unit group and unit parsing implemented.
- [✅ 9 : Text Editor](#text-editor) ( 100% Complete )
	- All 1397 strings are editable. ( Default game has 1396, new string added for the missing Credits button text, original files also supported )
	- UTF-8 + ISO-8859-1 encoding supported.
	- File is recompiled from scratch based on modifications.
	- Import & Export as .txt file.
	- Undo changes to current string.
	- Edited strings highlighted.
	- Saving keeps the previous file as TEXT.ojd.bak, and strings containing characters the game can't display are reported instead of being saved.
	- Rich text editing with newline support (\n to \r\n handling when loading and the opposite when saving )
- [❌ 10 : Map Editor](#map-editor) ( 10% Complete )
	- Basic parsing of .nsb levels.
	- .ojd files can be parsed, but there is still more decoding to do.
- ✅ 11 : No-CD Music Fix ( 100% Complete )
	- Custom winmm shim that allows for localised audio playback.
	- Added functionality to allow music playback to continue when the window loses focus or is minimised.
	- Persistence added to the "In-Game Music Enabled/Disabled" and sound options through the winmm shim.
	- Master volume now applies to music playback.
- [✅ 12 : Video Playback Intercept](#video-playback-intercept) ( 100% Complete )
	- Custom Smackw32.dll that plays upscaled videos at the games resolution.
	- Simple 1920x1080 upscales of all the original videos which get scaled to the games resolution.
	- Includes code for hijacking the credits sequence so that it doesn't crash and creating a custom overlay to display the credits sequence as intended.
- [✅ 13 : Custom Extended Backgrounds](#custom-extended-backgrounds) ( 100% Complete )
	- Upscaled splash screens and main menu backgrounds for resolutions above 640x480.
	- Resolution agnostic custom backgrounds made for specific menus. ( CD Player, Human Briefing, Human Research, Martian Briefing & Martian Research )
	- Extended the width of the war map background, this allows higher resolutions to work. ( specifically 1920x1080 )
	- Martian warmap now shows the coast of Europe in the bottom right corner. ( The human warmap does not which I believe suits the lore with it being styled like an old paper map )
- [✅ 14 : Enhanced Original Assets](#enhanced-original-assets) ( 100% Complete )
	- Reworked many assets in the game to improve their appearance.
		- Martian & Human Unit Icons are now the same for both factions, which makes it easier to tell when using the infiltration skill.
		- Music Track Artwork is now brighter.
		- Reworked 129/445 .spr files in total which includes the vast majority of the user interface, building unit icons, warmap unit icons and battlemap unit icons.
			- This count doesn't include the menu backgrounds and legal screens, which is another 9 files.
			- The extended warmap background includes another 4 files.
			- Full count 142/445 .spr files.
			- Remaining sprite files include mouse cursors, visual effects and parts of menus where there is very little space to add any detail.
- [✅ 15 : Decomp/Recomp](#decomprecomp---wowengine) ( 90% Complete )
	- WoWEngine built.
		- An SDL3 / C++20 rebuild of the game's engine, written routine by routine from the disassembly of the original executable. ( about 34,000 lines of code )
		- Runs natively on Windows, macOS and Linux as a 64-bit program, no compatibility layers, DirectDraw wrappers or DirectPlay setup required.
		- Contains no game data, it runs on the files from the original Human and Martian discs.
		- Aims for 1:1 parity with the original: the same rules, numbers, draw order and random number use.
		- Its own installer and launcher, WoWLauncher, copies the game from the discs, mounted disc images (.iso, .bin/.cue) or folders, and rips the music, on all three platforms.
	- All of the WoWRevived features and file fixes included.
		- The updated TEXT.ojd for all five languages, the fixed Cotton Factory, Airship and Sector 22 files and the restored building pixels.
		- Extended backgrounds for every supported resolution and the extended war map, picked automatically for the resolution in use, no file swapping required.
		- Enhanced Assets as an option.
		- WoWRevived's 1080p upscales of the movies are played when they are installed, otherwise the original Smacker movies play through the engine's own decoder.
		- Volume sliders that are saved, master volume applied to the music, music that keeps playing when the window loses focus. ( optional )
		- No 8/16 bit colour toggles and no "Lights" toggle, so infiltration can't crash the game.
		- Discord rich presence.
	- Modern fixes, on by default.
		- The resolution is applied at once from the Display Settings menu, no restart required, and the first start uses the desktop's resolution.
		- Starting a new campaign never overwrites the start files.
		- Upgraded buildings fight at their upgraded level in battle.
		- Loose map files in the MAPS folder are used, like loose files in the DAT folder.
	- Modern options implemented.
		- A Modern Options menu, opened from Game Options and the war map's Campaign Options.
		- Battle zoom. ( +, -, mouse wheel, R to reset )
		- HUD scale from 50% to 300% for the war map and battle map panels and their tool tips.
		- Menu scale for the menus, up to 225% at 1920x1080 (as large as the resolution allows).
		- Group keys ( Ctrl + digit to keep, digit to select ) and Shift to add to the selection in battle.
		- More save slots, in pages of five, up to 999.
		- Key bindings for both maps in WoWRevived.ini.
		- Popup menus that stay open, a war map status strip, windowed mode, picture stretching and the system mouse pointer.
		- Input recording and playback.
	- Civil wars ( work in progress ): a Human Civil War and a Martian Civil War, each faction against itself, with no pods.
		- A log file that records crashes and stalls, to make bug reports easier.
	- Still to do: network play and parts of the computer player.

This might not all happen but I wanted to create a more accessible guide for running the game on modern systems, while the information exists much of it is scattered across the internet.

Discord server : https://discord.gg/bwG6Z3RK8b

## Main Game Functionality

Some quality of life changes have been made to the game, from incorrect text entries to the removal of unnecessary buttons and more.

Critical Event Options - Edited

This was fixed by adding two leading spaces to the TEXT.ojd file for these entries.

<div align="center">
  <img src="images/menu-critical-edited.png" alt="Critical Event Options - Edited">
</div>

Critical Event Options - Unedited

<div align="center">
  <img src="images/menu-critical-unedited.png" alt="Critical Event Options - Unedited">
</div>

Display Settings - Edited

This was fixed by patching the executable to jump past the instructions to create these buttons.

The resolution slider was also removed because this is now handled by the launcher and the original functionality required restarting the game.

<div align="center">
  <img src="images/menu-settings-edited.png" alt="Display Settings - Edited">
</div>

Display Settings - Unedited

<div align="center">
  <img src="images/menu-settings-unedited.png" alt="Display Settings - Unedited">
</div>

Unit Border Selection - Changed from 640x640 to 2048x2048

<div align="center">
  <img src="images/custom-border-select.png" alt="Unit Selection Border">
</div>

The render order for the building file COTTFACT.IOB has been fixed which previously caused the frontmost smokestack to get rendered behind the main building.

<div align="center">
  <img src="images/building-factory-smokestack-ordering.png" alt="Smokestack Render Order">
</div>

Some missing pixels have been restored for building sprites, seen in the following previews.

<div align="center">
  <img src="images/building-martian-pixels-restored.png" alt="Missing Pixels Restored">
</div>

Violent stretching of the Human Airship unit has been fixed, it seems this was fixed in the original game files but hadn't been renamed from the cancelled ZEPPLIN3.WOF to AIRSHIP3.WOF in the game files.

Screenshot taken from a video posted by Sulus in the Discord server.

<div align="center">
  <img src="images/airship-stretching-fixed.png" alt="Violent Airship Model Stretching">
</div>

Sector 22 South Wales Terrain Geometry Hole

<div align="center">
  <img src="images/south-wales-terrain.png" alt="Sector 22 South Wales Terrain Geometry Hole">
</div>

Sector 22 South Wales Terrain Geometry Hole Fixed

<div align="center">
  <img src="images/south-wales-terrain-fixed.png" alt="Sector 22 South Wales Terrain Geometry Hole Fixed">
</div>

Discord Rich Presence

<div align="center">
  <img src="images/discord-rich-presence.png" alt="Discord Rich Presence">
</div>

## Custom Launcher

A custom launcher that makes it easy to access both campaigns as well as adjust settings.

![Launcher](images/launcher.png)

I tried to match the original launcher as best as possible, I was even able to find a high quality version of the launcher image embedded in the games installer.

<div align="center">
  <img src="images/launcher-original.png">
</div>

Custom Keyboard Shortcuts - Work In Progress - Not Currently Enabled

<div align="center">
  <img src="images/keycodes.png">
</div>

Advanced Settings

<div align="center">
  <img src="images/advanced-settings.png">
</div>

Larger Minimaps Accessibility Option - Larger Minimaps

<div align="center">
  <img src="images/minimaps-larger.png" alt="Larger Minimaps">
</div>

Larger Minimaps Accessibility Option - Regular Minimaps

<div align="center">
  <img src="images/minimaps-regular.png" alt="Regular Minimaps">
</div>

## File Extractor

A custom file extractor for extracting files from the .WoW archive format the game uses.

<div align="center">
  <img src="images/file-extractor.gif" alt="File Extractor">
</div>

## Sprite Viewer

A fully functional sprite viewer.

<div align="center">
  <img src="images/sprite-viewer.png" alt="Sprite Viewer">
</div>

## Unit Viewer

A fully functional unit viewer.

<div align="center">
  <img src="images/unit-viewer.png" alt="Unit Viewer">
</div>

Using this part of the application you can extract the 3D models for the units.

<div align="center">
  <img src="images/unit-viewer-zepp-extracted.png" alt="Zeppelin - Extracted - Viewed in Blender">
</div>

This is the Zeppelin unit as seen in-game, the screenshot is courtesy of Z-Fighter from the WoWRevived Discord server.

<div align="center">
  <img src="images/unit-viewer-zepp-in-game.png" alt="Zeppelin - In-Game">
</div>

## Terrain Viewer

A work in progress terrain viewer.

<div align="center">
  <img src="images/terrain-viewer.png" alt="Terrain Viewer">
</div>

## Building Viewer

A fully functional building viewer, this uses the same window as the unit viewer.

<div align="center">
  <img src="images/building-viewer.png" alt="Building Viewer">
</div>

## Font Viewer

A fully functional font viewer.

<div align="center">
  <img src="images/font-viewer.png" alt="Font Viewer">
</div>

## Save Editor

A work in progress save editor.

![Save Editor](images/save-editor.png)

## Text Editor

A text editor that makes it easy to adjust any entry in the TEXT.ojd file.

![Text Editor](images/text-editor.png)

## Map Editor

A work in progress "Map Editor" that currently just parses the .nsb level data.

<div align="center">
  <img src="images/map-editor.png" alt="Map Editor">
</div>

## Video Playback Intercept

A custom Smackw32.dll that intercepts the original smacker calls and replaces them with a more modern system for playing the newly upscaled videos.

![Video Playback Intercept](images/wrapper.png)

## Custom Extended Backgrounds

Upscaled menu sprites for all supported resolutions which are swapped around when changing the resolution in the launcher.

Human Background Menu

<div align="center">
  <img src="images/menu-background-human.png" alt="Human Background Menu">
</div>

Martian Background Menu

<div align="center">
  <img src="images/menu-background-martian.png" alt="Martian Background Menu">
</div>

Original Backround Menu

All of the original menu backgrounds sat in the corner at 640x480 no matter the resolution you were using.

<div align="center">
  <img src="images/menu-background-original.png" alt="Menu Background Original">
</div>

## Martian War Map

Though this could use a little cleanup, I added the coast of Europe to the Martian view of the war map.

<div align="center">
  <img src="images/custom-martian-warmap.png" alt="Martian War Map">
</div>

Similarly I expanded the Human view of the war map, I chose not to add the edge of Europe, partially because making it look right would be difficult, but also because the style is that of a paper map of the British Isles, which in real life likely wouldn't include Europe in the games timeline and setting.

## Expanded Backgrounds

These are a work in-progress, AI (Gemini) was used to generate portions of the extended backgrounds, but they have undergone extensive editing.

There is still a lot of cleanup left to do before these images are viable for use, but I have done my best to try and ensure that they are resolution agnostic and people using different resolutions won't miss out on much.

CD Player Menu - Expanded - Finished

<div align="center">
  <img src="images/custom-cd-player.png" alt="CD Player Menu">
</div>

CD Player Menu - Original

<div align="center">
  <img src="images/custom-cd-player-original.png" alt="CD Player Menu - Original">
</div>

Human Briefing Menu - Expanded - Finished

<div align="center">
  <img src="images/custom-human-briefing.png" alt="Human Briefing Menu">
</div>

Human Briefing Menu - Original

<div align="center">
  <img src="images/custom-human-briefing-original.png" alt="Human Briefing Menu - Original">
</div>

Human Research Menu - Expanded - Finished

<div align="center">
  <img src="images/custom-human-research.png" alt="Human Research Menu">
</div>

Human Research Menu - Original

<div align="center">
  <img src="images/custom-human-research-original.png" alt="Human Research Menu - Original">
</div>

Martian Briefing Menu - Expanded - Finished

<div align="center">
  <img src="images/custom-martian-briefing.png" alt="Martian Briefing Menu">
</div>

Martian Briefing Menu - Original

<div align="center">
  <img src="images/custom-martian-briefing-original.png" alt="Martian Briefing Menu - Original">
</div>

Martian Research Menu - Expanded - Finished

<div align="center">
  <img src="images/custom-martian-research.png" alt="Martian Research Menu">
</div>

Martian Research Menu - Original

<div align="center">
  <img src="images/custom-martian-research-original.png" alt="Martian Research Menu - Original">
</div>

## Enhanced Original Assets

These are the human battle map unit icons.

<div align="center">
  <img src="images/human-comparison.gif" alt="Human Battle Map Unit Icons">
</div>

These are the martian battle map unit icons.

<div align="center">
  <img src="images/martian-comparison.gif" alt="Martian Battle Map Unit Icons">
</div>

Many more assets have been reworked, most of which is barely noticeable but in general adds a little more colour depth to the interface.

Please note that these enhanced assets are disabled by default and can be enabled in the launcher.

## Decomp/Recomp - WoWEngine

WoWEngine is a rebuild of the game's engine in C++20 with SDL3, written routine by routine from the disassembly of the original executable. The aim is 1:1 parity with the original game: the same rules, the same numbers, the same draw order and the same use of random numbers. Anything that changes how the game plays sits behind an option in the Modern Options menu, the only changes made without asking are fixes for faults in the original.

It runs natively on Windows, macOS and Linux as a 64-bit program, so there is no need for compatibility settings, a DirectDraw wrapper or DirectPlay. Like the patched game, it keeps its settings in WoWRevived.ini in the game's folder, so existing installations carry straight over.

The engine contains no game data, it runs on the files from the original Human and Martian discs.

The source code and its documentation are in the [WoWEngine](WoWEngine) folder, see [WoWEngine/README.md](WoWEngine/README.md) for how to build it.

### Installing

WoWEngine comes with its own installer and launcher, WoWLauncher, laid out like the WoWRevived launcher. ( This is not the original game's installer, which should still not be used for the patched game. )

- Insert either disc, or both, and it finds them within a couple of seconds, or choose **Use Disc Images...** to install from .iso or .bin/.cue images instead.
- It copies the game, rips the music from the discs and copies in the updated TEXT.ojd for the language of your discs.
- With the game installed, it opens on the launcher screen with **Start Human Game** and **Start Martian Game**.

The music can only be ripped from a disc or a .bin/.cue image, as .iso images only hold the data track. Reading the music straight from a drive works on Windows and Linux, on macOS it has to come from a .bin/.cue image.

### 640x480 vs 1920x1080

The original game drew everything at 640x480. WoWEngine draws the war map and battle map at the full resolution chosen, and the resolution is applied the moment it is changed in the Display Settings menu.

War Map

<table>
  <tr>
    <td align="center"><img src="images/wowengine/engine-warmap-640.png" alt="War Map - 640x480"><br>640x480</td>
    <td align="center"><img src="images/wowengine/engine-warmap-1080.jpg" alt="War Map - 1920x1080"><br>1920x1080</td>
  </tr>
</table>

Battle Map

<table>
  <tr>
    <td align="center"><img src="images/wowengine/engine-london-640.png" alt="Battle Map - 640x480"><br>640x480</td>
    <td align="center"><img src="images/wowengine/engine-london-1080.jpg" alt="Battle Map - 1920x1080"><br>1920x1080</td>
  </tr>
</table>

### Everything from WoWRevived

All of the features and file fixes made especially for WoWRevived are included:

- The updated TEXT.ojd files for English, French, German, Italian and Spanish.
- The fixed game files: the Cotton Factory smokestacks, the Airship model, the hole in Sector 22 South Wales and the restored building pixels.
- The extended backgrounds for every supported resolution and the extended war map. The engine picks the right ones for the resolution in use, so no files are swapped around.
- Enhanced Assets, as an option in the Modern Options menu.
- WoWRevived's 1080p upscales of the movies are played when they are installed, otherwise the original Smacker movies play through the engine's own decoder.
- Volume sliders that are actually saved, the master volume applied to the music and the music playback option that keeps the music playing when the window loses focus.
- The Display Settings menu without the broken 8/16 bit colour toggles and the "Lights" toggle that could crash the game when using infiltration.
- Discord rich presence, with the same states as WoWRevived.

<table>
  <tr>
    <td align="center"><img src="images/wowengine/engine-research-1080.jpg" alt="Human Research - 1920x1080"><br>Human Research - 1920x1080</td>
    <td align="center"><img src="images/wowengine/engine-cd-player-1080.jpg" alt="CD Player - 1920x1080"><br>CD Player - 1920x1080</td>
  </tr>
</table>

### Modern Fixes

These are fixes for faults in the original game and are on by default:

- The resolution is applied at once, without restarting the game, and the first start uses the desktop's resolution.
- Starting a new campaign never overwrites the Human.000 and Martian.000 start files, which the original game could do.
- Upgraded buildings fight at their upgraded level, in the original game a Level 3 Vehicle Factory still fought as a Level 1 on the battle map. ( can be turned off )
- Loose map files placed in the MAPS folder are used, as loose files in the DAT folder already were.

### Modern Options

The Modern Options menu is opened from Game Options in the main menu, or from Campaign Options on the war map.

<div align="center">
  <img src="images/wowengine/engine-modern-options.png" alt="Modern Options">
</div>

| Option | Default | What it does |
|---|---|---|
| Upgraded buildings in battle | On | Upgraded buildings fight at their upgraded level |
| Popup menus stay open | Off | The right-click popup menus stay open after the button is let go, items are clicked |
| War map status strip | Off | A strip at the bottom left of the war map naming the sector or button under the mouse |
| Group keys and Shift select | On | Ctrl + a digit keeps the selection, the digit selects it again, pressed twice it moves the view there, Shift adds to the selection |
| More save slots | On | The Load and Save menus have pages of five, up to slot 999 |
| Battle zoom | On | +, - or the mouse wheel to zoom the battle map, R to reset |
| HUD scale | 100% | The size of the war map and battle map panels and their tool tips, from 50% to 300% |
| Menu scale | 100% | The size of the menus, up to 225% at 1920x1080 (the largest that fits the resolution) |
| Fade between menus | Off | The original game's fade between menus |
| Enhanced assets | Off | WoWRevived's Enhanced Assets, switched on or off without a restart; also reads a texture pack from the TEXTURES folder (see WoWEngine/docs/TEXTURES.md) |
| Original display settings | Off | The original Display Settings menu |
| Play in a window | Off | Windowed mode, the window's size is remembered |
| Stretch the picture | Off | Fill the screen instead of keeping the picture's shape |
| System mouse pointer | Off | The system's mouse pointer instead of the game's |
| Music playback | Off | Keep the music playing when the window loses focus |
| Discord rich presence | On | Discord rich presence |
| Record input / Play a recording | | Record a game's input and play it back |

The keys for both maps can also be changed in WoWRevived.ini under `[Keys]`.

Battle Zoom

<div align="center">
  <img src="images/wowengine/engine-battle-zoom.jpg" alt="Battle Zoom">
</div>

HUD Scale - 200% on the war map at 1920x1080

<div align="center">
  <img src="images/wowengine/engine-hud-scale.jpg" alt="HUD Scale">
</div>

More Save Slots

<div align="center">
  <img src="images/wowengine/engine-more-save-slots.png" alt="More Save Slots">
</div>

A Martian pod landing in a Human sector at 1920x1080

<div align="center">
  <img src="images/wowengine/engine-martian-landing.jpg" alt="Martian Pod Landing">
</div>

### Civil Wars ( work in progress )

Two new campaigns, started from the main menu under Start New Campaign: **Human Civil War** when playing as the Humans, **Martian Civil War** when playing as the Martians.

- The Human Civil War sets the Humans against each other, the player's side starting from Scotland against London.
- The Martian Civil War is set after the Humans have lost the Earth, the player's Martians starting from London against Scotland.
- There are no pods in either, the computer player plays the other side with its own faction's rules, and the other side's units are shown in different colours on the battle map.
- Each side has its own unit table, so the other side's units take the enemy armour settings as in the regular game.
- There are no ending movies; a civil war goes straight to the credits.
- Civil war campaigns are saved separately ( HumanCW.001, MartianCW.001 ) so the original game never sees them, and they are listed in the same Load menu with "(Human Civil War)" or "(Martian Civil War)" after their names.

The starting layouts are a first draft and the two sides still need evening out. They are read from DAT\CivilWar.txt when a civil war begins, so which sectors each side starts with can be changed there and tried with a new campaign, no rebuild needed.

### Bug Reports

WoWEngine writes a log, WoWEngine.log, next to WoWEngine.exe, with a CRASH line if the game crashes and a STALL line if it stops responding. The log from the previous run is kept as WoWEngine.previous.log, so please include both when reporting a problem on the Discord or in an Issue.

### Still To Do

- Saving during a battle.
- Repairing units and buildings.
- Breakable bridges.
- Network play.
- Parts of the computer player.

## Credit

Thanks to angrylion for the original executable patch and winmm.dll shim which I used as a reference for my own fixes : https://www.old-games.ru/forum/threads/patchi-vozvraschajuschie-cd-audio-the-patchs-to-restore-cdda-playback.51778/#post-877625

English manual sourced from : https://oldgamesdownload.com/wp-content/uploads/Jeff_Waynes_The_War_of_the_Worlds_Manual_Win_EN_OldGamesDownload.com_.pdf

Thanks to Dan Redfield for the Credits patch to the TEXT.ojd file which I then used as reference to patch the other languages versions of that file.

Thanks to the uploader of the regional releases which were useful for constructing the language patches.

Italian : https://archive.org/details/cd-wow-98-it

Spanish : https://archive.org/details/cd-wow-98-es

German : https://archive.org/details/cd-wow-98-de

French : https://archive.org/details/cd-wow-98-fr

My language patches do not require the specific regional release and instead ship with the relevant files required to convert any language installation to any other language.

Thanks to RetroKet for setting up and managing the Discord server as well as encouragement and helping identify some bytes in the .WoW archive format.

Thanks to yereverluvinunclebert for creating the front of the filing cabinet drawer, which will be used in a future version for the expanded backgrounds : https://www.deviantart.com/yereverluvinuncleber/gallery

Thanks to Z-Fighter for help testing the project and providing a never-ending supply of screenshots for reference which have saved me a lot of time not needing to fire the game up to see how something should look, as well as for finding a crash in the original game that would occur when using infiltration if lights are disabled in the options.

Thanks to Sulus for posting a video of the violent geometry tearing on the airship model when it turns around.

Thanks to FunkyFr3sh for cnc-ddraw, which provides the DirectDraw translation layer for modern systems. See ddraw-license.txt for the full MIT license.

Thanks to the Discord community for supporting the project and helping out with testing.
