# WoW Engine

An SDL3 / C++20 rebuild of the engine from Jeff Wayne's 'The War Of The Worlds' (Rage Software, 1998), for Windows, macOS and Linux.

The engine contains no game data. It runs on the files from the original Human and Martian discs, which the installer copies (see below).

## Building

Needs CMake 3.20+ and a C++20 compiler (Visual Studio 2022, Xcode 15 / Apple Clang 15, GCC 11+, Clang 14+).

**Visual Studio:** open the `WoWEngine` folder (File, Open, Folder), pick `x64-Debug` or `x64-Release` from the configuration list and Build All. They build into `out\build\x64-Debug` and `out\build\x64-Release`.

**Command line** (the same presets; `debug` and `release` on Linux and macOS):

```
cmake --preset x64-Release          (or x64-Debug, debug, release)
cmake --build --preset x64-Release
```

If SDL3 isn't installed it is downloaded and built automatically (`WOW_FETCH_SDL`, on by default).
To use an installed copy, point `CMAKE_PREFIX_PATH` at it.

**Version:** set it in one place, `project(WoWEngine VERSION 0.0.1 ...)` at the top of `CMakeLists.txt`. The build turns it into `Version.h` (`WOWENGINE_VERSION`), which the window title uses, and the release script reads it from there too.

**Release:** `tools\make_release.bat` (double-click it) runs `tools/make_release.py`, which takes the version from `CMakeLists.txt` and the programs from `out\build\x64-Release`, and makes `out\WoWEngine-v<version>\` and `out\WoWEngine-v<version>.zip`. If the Release build isn't there it builds it first, which needs CMake and the compiler on the PATH (the "x64 Native Tools Command Prompt"); otherwise build `x64-Release` in Visual Studio and run it again. Options: `--build` to rebuild first, `--debug` to package the Debug build, `--seven-zip` to also make a `.7z`, `--build-dir` to package another folder.

Targets:

| Target | What it is |
|---|---|
| `WoWEngine` | The game. Pass the game folder as the first argument, or put the engine in the game folder. `--open 0x6E` starts on the screen a menu command opens. `--ticks N` runs N game updates before a screenshot. `--screenshot file.bmp [keys]` presses the listed keys (up, down, left, right, enter, escape), saves the frame and exits. |
| `wowdump` | Lists and extracts the KAT! and SfxL archives. `--check <folder>` reports missing game files, `--sprite` saves a sprite as a BMP, `--text` prints TEXT.ojd and `--obj` prints the OBJ.ojd graphics table. |
| `wowcore` | The file formats and game data code, with no SDL dependency, so tools can use it on their own. |
| `WoWLauncher` | The one program players start, laid out like WoWRevived's WoWLauncher. With the game installed it starts a Human or Martian game; without it, it's the installer: it finds the discs in the drives (or disc images you pick), copies the game and rips the music. See [Installing](#installing). |
| `wowinstall-cli` | The same installer from the command line. |

## Layout

```
src/core/       file formats and game data (no SDL)
  Bytes.h         bounds-checked little-endian reader
  Archive.*       KAT! and SfxL archives
  Ffuh.*          FFUH Huffman decompression
  GameFiles.*     locating the game files, case-insensitive paths for Linux and macOS
  Palette.*       .PAL palettes and blend tables
  Sprite.*        .SPR sprites (single and animated)
  TextOjd.*       TEXT.ojd strings, read the way the game does (only the side being played)
  ObjOjd.*        OBJ.ojd graphics table (GType id to file, palette slot, hotspot)
  Font.*          .FNT fonts;  Charset.h  the game's Latin-1 to font table
  ShadeTable.*    .SHH shade tables;  ShadeGroups.h  which tables each palette group loads into which slot
  Vfs.*           the game's file lookup rules: archives, loose file overrides
  Bmp.*           BMP output for screenshots and wowdump
src/frontend/   Resources (graphics, strings, shade tables), Canvas (drawing), Screen (controls), Menu, CdPlayer,
                FrontEnd, Settings (WoWRevived.ini)
src/install/    the installer: Disc (folders, .iso, .bin/.cue), DriveAudio (raw CD audio), Iso9660, Installer
src/platform/   SDL3 code: window and true colour frame buffer (Video), music (Music); later sound effects and movies
src/main.cpp    start-up and the fixed-rate game loop
tools/          command line tools
docs/           ENGINE-MAP.md (the exe mapped by subsystem), FRONTEND.md (how the menus work), IDA index
```

## How it works

- **Frame buffer.** The frame buffer is true colour, because each sprite is drawn through its own palette, so several palettes are on screen at once. `Video::Present` scales it to the window, letterboxed.
- **Game loop.** Updates run at a fixed 30 per second, the game's default "Game Frequency". Drawing runs as fast as vsync allows.
- **Settings.** Settings are read from and saved to the same `WoWRevived.ini` the launcher and patches use, through the shared `IniStore.h`, with the game's own section and value names. Existing installs carry straight over. The registry is never used.
- **File names.** Every game path goes through `GameFiles::Resolve`, which matches names without regard to case. The discs mix cases (`DAT\Dat.wow`, `MAPS\MAPS.WoW`, `MARTIAN.cd`), and Linux and macOS file systems are case-sensitive.

## Road map

1. **Foundations** (started): CMake and SDL3, the archives, FFUH, game file lookup, window and frame buffer.
2. **Formats** (in progress): archives, FFUH, palettes, SPR, FNT, shade tables, TEXT.ojd and the OBJ.ojd graphics table are done, and all but the first three are read the way the game reads them. All of these have been checked against the English discs. Still to do: the unit-specific shade groups, the rest of OBJ.ojd (unit data), AI.ojd, SFX.ojd and sound libraries, CLS, WOF/IOB and maps.
3. **Front end** (started): rebuilt so far are the menu framework, Main Menu, Game Options, Display Settings, Sound Control, Critical event options, Load and Save Game, Credits, Quit, and the CD Player with music (see `docs/FRONTEND.md`). They match screenshots of the original for both sides. Still to do: the campaign and network menus, Smacker movies (FMV), the cursor, tooltips and sound effects.
4. **Audio**: SfxL sound effects, positional ambient sound, and the music played from the ripped `Music-Human` / `Music-Martian` WAV tracks.
5. **War map**: the strategic layer, covering sectors, construction, research, AI turns and pods.
6. **Battle map**: terrain, the 3D units (WOF/IOB with the shade tables), buildings, pathfinding, combat and fog of war.
7. **Saves**: loading and writing the original save format, so old saves keep working.
8. **Network**: multiplayer, based on the custom WoW_network.exe build.
9. **Release**: packaging for all three platforms, then the Sieve of Potentia pass.

Each stage is checked against the original game running side by side, address by address, using `docs/ENGINE-MAP.md` and the IDA notes in `WoWDecomp/`.

## Installing

Run `WoWLauncher`. It's both the launcher and the installer, laid out like WoWRevived's WoWLauncher (the same picture, with the buttons underneath).

**When the game isn't installed** in its folder, it opens on the install screen, and installs into its own folder unless you choose another with **Choose Install Folder...**. Put either disc in, or both, and it finds them within a couple of seconds; **Install The War Of The Worlds** copies the first disc and rips its music, then waits for the other one, carrying on by itself as soon as it's inserted (**Skip the Other Disc** finishes with what it has). **Use Disc Images...** takes `.iso` or `.cue` files instead of drives. The music is always ripped. When a disc source has none (an `.iso` image holds only the data track, not the CD audio), the disc line says so before you start, and when the install finishes a message lists each problem, why it happened and what it means: no Human or Martian music in the game, and to use the disc or a `.bin/.cue` image to get it.

**When the game is installed**, it opens on the launcher screen: **Start Human Game** and **Start Martian Game** set the side marker (`Human.cd` or `MARTIAN.cd`, as WoWRevived's launcher does) and start WoWEngine; **Install from Discs...** goes back to the install screen. If a side's music is missing it says so under the buttons. WoWEngine also offers to run WoWLauncher if it starts without the game files.

The command line version:

```
wowinstall-cli --drives                                     list the optical drives and the discs in them
wowinstall-cli [<game folder>] [<disc>] [<disc>] [--martian] [--no-music]
```

A `<disc>` can be any of:
- a drive or mounted disc (`D:\`, `/media/cdrom`, `/dev/sr0`);
- a folder holding a copy of a disc;
- an `.iso` image;
- a `.cue` sheet, with its `.bin` files.

Give it both discs, in either order, or no disc at all to use whatever is in the drives. It works out which is which. Where both discs have a file, the Human disc's copy is kept, so the order makes no difference. Then it copies the game files, rips the music from tracks 2 to 5, writes `language.txt` and leaves the side marker (`Human.cd`, or `MARTIAN.cd` with `--martian`). Finally it checks that every file the engine needs is there. It skips everything WoWLauncher's clean-up deletes, including the original `FMV\*.smk` movies.

How it handles the different situations:
- **Both discs in two drives:** both are found and copied without asking.
- **The same disc in two drives, or given twice:** it's copied once.
- **One drive:** after the first disc, it asks for the other one in the same drive. It also checks the other drives in case the disc went into one of those, and says so if the same disc is put back in.
- **Discs from two different language releases:** the install goes ahead with a warning, and the text comes from the Human disc.

Music only comes from a disc in a drive or from a `.bin/.cue` image, because `.iso` images hold only the data track. Reading the music straight from a drive works on Windows and Linux. macOS doesn't allow raw CD reads, so there it has to come from a `.bin/.cue` image.

Both installers are front ends to the `wowinstall` library (`src/install`), which has no SDL dependency. WoWLauncher (`src/launcher`) draws its own Windows-style controls so it looks the same on every platform; its font is DejaVu Sans baked into `src/launcher/UiFont.h` by `tools/bake_font.py`, and its picture is `resources/launcher.bmp`, copied next to the program when it's built.

