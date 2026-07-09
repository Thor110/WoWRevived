# WoW Revived

The definitive preservation and revival project for the classic Rage Software game Jeff Wayne's 'The War Of The Worlds' released in 1998.

The game is usually installed to "C:\Program Files (x86)\Jeff Wayne's 'The War Of The Worlds'" but you may install it elsewhere if you prefer, for example, into a dedicated folder for modding, preservation, or portable use.

**Download and mount the disc images or insert the discs one by one.** https://archive.org/details/jeffwaynewowgame_202110

**IMPORTANT : Do NOT use the installer, just follow the instructions below.**

**IMPORTANT : Do NOT launch WoW_patched.exe or WoW_network.exe directly unless you have run the launcher successfully at least once!**


- 1 : Copy the Human disc contents to your installation folder.
- 2 : Copy the contents of the Martian disc to your installation folder ( when prompted, choose *not* to replace files, or do, it doesn't matter anymore, but fewer writes are better for SSD longevity )
- 3 : Download the latest version from the Releases page, extract the contents and place the files in your install folder.
- 4 : Right-click "WoWLauncher.exe" and choose **Send to > Desktop (create shortcut)**, then launch it and enjoy the game!

LANGUAGES : Language packs are now stand-alone releases that ship with the relevant files required to convert any language installation to any other language

ADDITIONAL : The launcher will automatically request administrator permissions to ensure registry settings are correctly applied.

NOTE : If you choose to make a language patch using the text editor, feel free to send the "TEXT.ojd" file my way and I will add it to the releases.

WANTED : Regional Manuals : We are looking for high-quality scans of the original physical German (DE) and Spanish (ES) manuals. If you own a physical copy and can provide a scan, please join the Discord or open an Issue!

# To-Do List

The only remaining things on the to-do list are not important for releases.

- Find remaining shader tables for the few remaining sprites that haven't been located in the sprite viewer.
- Correct terrain preview colouring in the terrain viewer.
- Fix remaining bugs in the building viewer.
- Finish the save editor and map editor.

# Road Map

The road map for this project.
- [✅ 0 : Main Game Functionality](#main-game-functionality) ( 100% Complete )
	- Iron changed to say Steel in multiple entries in the TEXT.ojd file.
	- Leading space added to some text to improve appearance in the Critical Event Options menu.
	- Missing entry created for the Credits button in the options menu.
	- Build List screen location is now dynamically set as per the resolution setting chosen in the launcher.
	- Missing pixels have been restored for some building sprites.
	- Violent stretching of the Human Airship unit has been fixed, it seems this was fixed in the original game files but hadn't been renamed from the cancelled ZEPPLIN3.WOF to AIRSHIP3.WOF in the game files.
	- Alt-tabbing now works as cnc-ddraw is working with the latest version of the smacker wrapper.
	- Custom networking executable included and accessible via the launcher when networking is enabled.
	- A hole in Sector 22 South Wales terrain geometry has been patched.
	- Pressing escape to enter or exit the menu no longer stops the music from playing like it did in the original game.
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
	- Advanced registry settings options, now including the ability to adjust the turret build limit.
	- Automated registry entry type switching between the network and vanilla versions of the game executable. ( the network executable has some entry types set to dword instead of string, this causes the vanilla executable to start up at 320x240 )
	- Custom keyboard shortcut settings. (WIP - cancelled for now)
	- Automatic regeneration of base registry settings, making the installation portable.
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
	- .WoW archives can be extracted.
	- Waveform preview, play and replace sound files from .WoW archives.
	- Decompressing files optional but enabled by default.
- [✅ 3 : Sprite Viewer](#sprite-viewer) ( 99% Complete )
	- Sprite viewer can extract and replace game sprites.
	- Export original palette and palette with the shader tables applied. ( this feature is currently partially broken and hidden )
	- Preview with the original palette or with the shader tables applied.
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
	- Includes code for hijacking the credits sequence so that it doesn't crash and creating an overlay to display the credits sequence.
- [❌ 13 : Custom Extended Backgrounds](#custom-extended-backgrounds) ( 100% Complete )
	- Upscaled splash screens and main menu backgrounds for resolutions above 640x480.
	- Resolution agnostic custom backgrounds have been made but have yet to be finished.
	- Extended the width of the war map background, this allows higher resolutions to work.
	- Martian war map now shows the coast of Europe in the bottom right corner.
- [✅ 14 : Enhanced Original Assets](#enhanced-original-assets) ( 100% Complete )
	- Reworked many assets in the game to improve their appearance.
		- Martian & Human Unit Icons are now the same for both factions, which makes it easier to tell when using the infiltration skill.
		- Music Track Artwork is now brighter.
		- Reworked 129/445 .spr files in total which includes the vast majority of the user interface, building unit icons, warmap unit icons and battlemap unit icons.
			- This count doesn't include the menu backgrounds and legal screens, which is another 9 files.
			- The extended warmap background includes another 4 files.
			- Full count 142/445 .spr files.
			- Remaining sprite files include mouse cursors, visual effects and parts of menus where there is very little space to add any detail.
- ❌ 15 : Decomp/Recomp ( Started - 1% Complete )
	- Begun mapping out virtual key addresses for use in the launchers custom keyboard shortcut settings. ( "WoWRevived\WoWDecomp\ida-map.txt" )

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