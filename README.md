# WoW Revived

A revival project for the classic RAGE game Jeff Wayne's 'The War Of The Worlds'

The game is usually installed to "C:\Program Files (x86)\Jeff Wayne's 'The War Of The Worlds'", but you may install it elsewhere if you prefer — for example, into a dedicated folder for modding, preservation, or portable use.

- 1 : Copy the Human disc contents to your installation folder.
- 2 : Rename the file "Human.cd" to "Human.cd.bak"
- 3 : Rename the folder "FMV" to "FMV-Human"
- 4 : Copy the Martian disc to your installation folder ( when prompted, choose *not* to replace files — or do, it doesn't matter anymore, but fewer writes are better for SSD longevity )
- 5 : Download and extract "wowpatch.zip" from : https://www.old-games.ru/forum/threads/patchi-vozvraschajuschie-cd-audio-the-patchs-to-restore-cdda-playback.51778/#post-877625
- 6 : Download the launcher from the Releases page and and place it in your install folder.
- 7 : Right-click the launcher and choose **Send to > Desktop (create shortcut)**, then launch it and enjoy the game!

The launcher will automatically request administrator permissions to ensure registry settings are correctly applied.

If anyone really wants to save themselves 15.2mb's of space or wants to save writes that badly, check "unnecessary-files.txt" for a list of files that do not need to be copied from the discs, there are also four files not needed from "wowpatch.zip" which are the four "winmm_driver9x" files.

Language options are currently hidden, as no confirmed non-English PC versions of the game have been found. If you happen to own or know of a different language release, please let us know as support can easily be added if needed.

# Road Map

The road map for this project.
- [✅ 1 : Custom Launcher](#custom-launcher)(#custom-launcher)
- [✅ 2 : File Extractor](#file-extractor)(#file-extractor) ( Fully Functional )
	- .WoW archives can be extracted
	- .ojd files can be parsed, but there is still more decoding to do.
- ❌ 3 : Save Editor(#save-editor) ( Partially Implemented )
	- Save Name, Time & Date editing functionality implemented, along with Swap Sides and Delete Save buttons.
	- Sector & Area names loaded dynamically from TEXT.ojd
- ✅ 4 : Text Editor(#text-editor) ( Fully Functional, edit and save references in the TEXT.ojd file, along with properly updating their headers. )
- ❌ 5 : Map Editor ( Basic Parsing Implemented )
	- Basic parsing of .nsb filetypes.
- ❌ 6 : No-CD Music Fix
- ❌ 7 : Decomp/Recomp
- ❌ 8 : Remake

This might not all happen but we wanted to create a more accessible guide for running the game on modern systems, while the information exists much of it is scattered across the internet.

Discord server : https://discord.gg/bwG6Z3RK8b

# Screenshots

Screenshots of the current progress for the toolkit.

## Custom Launcher
WoWLauncher
![Launcher](images/launcher.png)

## File Extractor
WoWViewer - File Extractor
![File Extractor](images/file-extractor.png)

## Save Editor
WoWViewer - Save Editor
![Save Editor](images/save-editor.png)

## Text Editor
WoWViewer - Text Editor
![Text Editor](images/text-editor.png)
