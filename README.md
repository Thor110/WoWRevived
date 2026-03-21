# WoW Revived

A revival project for the classic RAGE game Jeff Wayne's 'The War Of The Worlds'

The game is usually installed to "C:\Program Files (x86)\Jeff Wayne's 'The War Of The Worlds'" but you may install it elsewhere if you prefer, for example, into a dedicated folder for modding, preservation, or portable use.

**IMPORTANT : Do NOT use the installer, just follow the instructions below.**

- 1 : Copy the Human disc contents to your installation folder.
- 2 : Rename the file "Human.cd" to "Human.cd.bak"
- 3 : Rename the folder "FMV" to "FMV-Human"
- 4 : Copy the Martian disc to your installation folder ( when prompted, choose *not* to replace files — or do, it doesn't matter anymore, but fewer writes are better for SSD longevity )
- 5 : Download the latest version from the Releases page, extract the contents and place the files in your install folder.
- 6 : Right-click the launcher and choose **Send to > Desktop (create shortcut)**, then launch it and enjoy the game!

LANGUAGES : Language packs are now stand-alone releases.

COMPATIBILITY : Windows 11 users might have to adjust the properties for "WoW_patched.exe" by setting compatibility to Windows XP Service Pack 3 as well as setting the colour depth to 16 bit.

IE : Right click "wow_patched.exe" and go to the compatibility tab, then adjust the settings accordingly.

ADDITIONAL : The launcher will automatically request administrator permissions to ensure registry settings are correctly applied.

If you choose to make a language patch using the text editor, feel free to send the "TEXT.ojd" file my way and I will add it to the releases.

WANTED : Regional Manuals : We are looking for high-quality scans of the original physical German (DE) and Spanish (ES) manuals. If you own a physical copy and can provide a scan, please join the Discord or open an Issue!

# Road Map

The road map for this project.
- [✅ 0 : Main Game Functionality](#main-game-functionality) ( 100% Complete )
	- Iron changed to say Steel in multiple entries in the TEXT.ojd file.
	- Leading space added to some text to improve appearance in the Critical Event Options menu.
	- Missing entry created for the Credits button in the options menu.
	- Build List screen location is now dynamically set as per the resolution setting chosen in the launcher.
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
	- Features I am considering adjusting:
		- 1\. Heat-ray construction limit of 12, I may add this limit to the launcher when I locate the relevant values.
- [✅ 1 : Custom Launcher](#custom-launcher) ( 95% Complete )
	- Quick restart after alt-tab error exit of the game.
	- Advanced registry settings options.
	- Custom keyboard shortcut settings. (WIP - not enabled currently)
	- Automatic regeneration of base registry settings, making the installation portable.
	- Automatic cleanup of unnecessary files and Smackw32.dll moved to main directory.
	- Custom pre-set difficulty settings. (Easy, Medium[default], Hard)
	- Dynamic asset swapping pipeline with custom assets curated for each supported resolution.
	- New working resolutions added.
		- 1280x768 &nbsp;&nbsp;(15:9)
		- 1280x1024	(5:4)
		- 1360x768 &nbsp;&nbsp;(16:9)
		- 1366x768 &nbsp;&nbsp;(16:9)
		- 1600x900 &nbsp;&nbsp;(16:9)
		- 1600x1024	(5:4)
		- 1680x1050	(16:10)
		- 1920x1080	(16:9)
- [✅ 2 : File Extractor](#file-extractor) ( 100% Complete )
	- .WoW archives can be extracted.
	- Waveform preview, play and replace sound files from .WoW archives.
	- Decompressing files optional but enabled by default.
- [✅ 3 : Sprite Viewer](#sprite-viewer) ( 99% Complete )
	- Sprite viewer can extract and replace game sprites.
	- Export original palette and palette with the shader tables applied.
	- Preview with the original palette or with the shader tables applied.
	- Replace single frame and multi-frame sprites with custom ones.
	- Quantisation of imported images will reduce them to the closest equivalent colour found in the palette through the associated shader table.
	- Known "bugs" with the Sprite Viewer.
		- 1\. Shader tables are not applied to all sprites yet, only a very small number of shader tables are left to be identified.
- [✅ 4 : Unit Viewer](#unit-viewer) ( 100% Complete )
	- Unit viewer can export .WOF models and their embedded textures.
	- Ability to import custom models.
	- Preview the units texture.
- [✅ 5 : Terrain Viewer](#terrain-viewer) ( 95% Complete )
	- Terrain viewer can export .CLS geometry and their tile indexed texture map.
	- Ability to import custom geometry.
	- Preview the terrains tilemap.
- [✅ 6 : Building Viewer](#building-viewer) ( 50% Complete )
	- Building viewer can export .IOB geometry.
- [✅ 7 : Font Viewer](#font-viewer) ( 50% Complete )
	- Font viewer can export .FNT files to .png images.
- [❌ 8 : Save Editor](#save-editor) ( Partially Implemented - 10% Complete )
	- Save Name, Time & Date editing functionality implemented, along with Swap Sides and Delete Save buttons.
	- Override standard limit of 1753 as the minimum date and set the year manually to as low as year zero.
	- Sector & Area names loaded dynamically from TEXT.ojd to support the regional releases.
- [✅ 9 : Text Editor](#text-editor) ( 100% Complete )
	- All 1397 strings are editable. ( Default game has 1396, new string added for the missing Credits button text, original files also supported )
	- UTF-8 + ISO-8859-1 encoding supported.
	- File is recompiled from scratch based on modifications.
	- Import & Export as .txt file.
	- Undo changes to current string.
	- Edited strings highlighted.
	- Rich text editing with newline support (\n to \r\n handling when loading and the opposite when saving )
- ❌ 10 : Map Editor ( Basic Parsing Implemented - 1% Complete )
	- Basic parsing of .nsb filetypes.
	- .ojd files can be parsed, but there is still more decoding to do.
- ✅ 11 : No-CD Music Fix ( 99% Complete )
	- Uses a winmm shim that allows for localised audio playback.
	- Added functionality to allow music playback to continue when not in fullscreen and the window loses focus.
	- Permanence added to the "In-Game Music Enabled/Disabled" option through the winmm shim.
	- Known "bugs" which I consider non-issues for the time-being.
		- 1\. The counter increases indefinitely.
		- 2\. Doesn't progress to the next track when a track finishes in the CD player menu.
- [✅ 12 : Video Playback Intercept](#video-playback-intercept) ( 100% Complete )
	- Custom Smackw32.dll that plays upscaled videos at the games resolution.
	- Simple 1920x1080 upscales of all the original videos which get scaled to the games resolution.
	- Alt-Tab detection when in fullscreen used to close the game automatically to save the user time encountering an error.
		- This is because alt-tabbing is broken in the game by default.
	- Includes code for hijacking the credits sequence so that it doesn't crash and creating an overlay to display the credits sequence.
- [❌ 13 : Custom Extended Backgrounds](#custom-extended-backgrounds) ( 90% Complete )
	- Resolution agnostic custom backgrounds have been made but have yet to be finished.
	- Extended width of the war map, this allows higher resolutions to work.
- [✅ 14 : Enhanced Original Assets](#enhanced-original-assets) ( 90% Complete )
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

## Custom Launcher

A custom launcher that makes it easy to access both campaigns as well as adjust settings.

![Launcher](images/launcher.png)

Custom Keyboard Shortcuts - Work In Progress - Not Currently Enabled

<div align="center">
  <img src="images/keycodes.png">
</div>

Advanced Settings

<div align="center">
  <img src="images/advanced-settings.png">
</div>

## File Extractor

A custom file extractor for extracting files from the .WoW archive format the game uses.

<div align="center">
  <img src="images/file-extractor.gif" alt="File Extractor">
</div>

## Sprite Viewer

A work in progress sprite viewer.

<div align="center">
  <img src="images/sprite-viewer.png" alt="Sprite Viewer">
</div>

## Unit Viewer

A work in progress unit viewer.

<div align="center">
  <img src="images/unit-viewer.png" alt="Unit Viewer">
</div>

## Terrain Viewer

A work in progress terrain viewer.

<div align="center">
  <img src="images/terrain-viewer.png" alt="Terrain Viewer">
</div>

## Building Viewer

A work in progress building viewer, this uses the same window as the unit viewer.

<div align="center">
  <img src="images/building-viewer.png" alt="Building Viewer">
</div>

## Font Viewer

A work in progress font viewer.

<div align="center">
  <img src="images/font-viewer.png" alt="Font Viewer">
</div>

## Save Editor

A work in progress save editor.

![Save Editor](images/save-editor.png)

## Text Editor

A text editor that makes it easy to adjust any entry in the TEXT.ojd file.

![Text Editor](images/text-editor.png)

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

These are a work in-progress, AI (Gemini) was used to generate portions of the extended backgrounds, but they have undergone extensive editing.

There is still a lot of cleanup left to do before these images are viable for use, but I have done my best to try and ensure that they are resolution agnostic and people using different resolutions won't miss out on much.

NOTE : The "random" colours in the background is just my template that showcases all of the space that each available resolution would see.

CD Player Menu

<div align="center">
  <img src="images/custom-cd-player.png" alt="CD Player Menu">
</div>

Human Briefing Menu

<div align="center">
  <img src="images/custom-human-briefing.png" alt="Human Briefing Menu">
</div>

Human Research Menu

<div align="center">
  <img src="images/custom-human-research.png" alt="Human Research Menu">
</div>

Martian Briefing Menu

<div align="center">
  <img src="images/custom-martian-briefing.png" alt="Martian Briefing Menu">
</div>

Martian Research Menu

<div align="center">
  <img src="images/custom-martian-research.png" alt="Martian Research Menu">
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

## Credit

Thanks to old-games.ru and the author for the original executable patch and winmm.dll shim which I used as a reference for my own fixes : https://www.old-games.ru/forum/threads/patchi-vozvraschajuschie-cd-audio-the-patchs-to-restore-cdda-playback.51778/#post-877625

English manual sourced from : https://oldgamesdownload.com/wp-content/uploads/Jeff_Waynes_The_War_of_the_Worlds_Manual_Win_EN_OldGamesDownload.com_.pdf

Thanks to Dan Redfield for the Credits patch to the TEXT.ojd file which I then used as reference to patch the other languages versions of that file.

Thanks to RetroKet for setting up and managing the Discord server as well as encouragement and helping identify some bytes in the .WoW archive format.

Thanks to yereverluvinunclebert for creating the front of the filing cabinet drawer, which will be used in a future version for the expanded backgrounds : https://www.deviantart.com/yereverluvinuncleber/gallery

Thanks to Z-Fighter for help testing the project and providing a never-ending supply of screenshots for reference which have saved me a lot of time not needing to fire the game up to see how something should look, as well as for finding a crash in the original game that would occur when using infiltration if lights are disabled in the options.

Thanks to the Discord community for supporting the project and helping out with testing.