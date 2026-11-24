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

**Version:** set it in one place, `project(WoWEngine VERSION 0.2.5 ...)` at the top of `CMakeLists.txt`. The build turns it into `Version.h` (`WOWENGINE_VERSION`), which the window title uses, and the release script reads it from there too.

**Release:** `tools\make_release.bat` (double-click it) runs `tools/make_release.py`, which takes the version from `CMakeLists.txt` and the programs from `out\build\x64-Release`, and makes `out\WoWEngine-v<version>\` and `out\WoWEngine-v<version>.zip`. If the Release build isn't there it builds it first, which needs CMake and the compiler on the PATH (the "x64 Native Tools Command Prompt"); otherwise build `x64-Release` in Visual Studio and run it again. Options: `--build` to rebuild first, `--debug` to package the Debug build, `--seven-zip` to also make a `.7z`, `--build-dir` to package another folder.

**Tried from a clean checkout** (update 234, on Linux): a fresh clone, `python3 tools/make_release.py --build`, and
nothing else, configures, builds and packages the release (the programs, SDL3, the five TEXT.ojd, DAT and MAPS with
the fixed files and the backgrounds, the licence and this readme). The release, unpacked into an empty folder with
the game's files added and the build folder out of the way, starts and uses its own files. That check found the
Linux programs looking for SDL3 only in the folder they were built in; since 234 they look beside themselves
first (`CMakeLists.txt`, BUILD_RPATH). Not tried here: Windows (the owner builds and runs each update there) and
macOS, where the programs are built as `.app` bundles that the script doesn't yet look inside.

**Unit tests** (`tests/`, update 235): the file formats (FFUH both ways, sprites, palettes, shade tables,
TEXT.ojd, the byte reader), the game's random numbers (MSVC's `rand()` from `srand(1)`), the save written, read
back and written again byte for byte (with new units, buildings and a Damage Layer), the war map's tables and
rules (waypoints, borders, the calendar, arrival labels, route search) and WoWRevived.ini. They make their own
data, so no game files are needed. Built with the rest (`wowtests`; `-DWOW_BUILD_TESTS=OFF` leaves them out),
run with `ctest --test-dir <build folder>`, or `wowtests <part of a name>` for some of them. Not in a release.

**GitHub** (`.github/workflows/wowengine.yml`, update 235): every push that touches `WoWEngine/` builds the
Release configuration with the tools on Windows, Linux and macOS, runs the unit tests, and on Windows and Linux
packages the release, kept as the run's artifact (Actions tab, the run, "Artifacts"). A newer push to the same
branch cancels the run still going for it and starts again. It can also be started by hand from the Actions tab.

Targets:

| Target | What it is |
|---|---|
| `WoWEngine` | The game. Pass the game folder as the first argument, or put the engine in the game folder. `--open 0x6E` starts on the screen a menu command opens. `--ticks N` runs N game updates before a screenshot. `--screenshot file.bmp [keys]` presses the listed keys (up, down, left, right, enter, escape, backspace, or =letters, which are also typed into a text field), saves the frame and exits. `--frames <folder>` with `--screenshot` saves every update's picture as numbered BMPs, for videos (docs/TRAILER.md). |
| `wowdump` | *Developer tool, not built unless `-DWOW_BUILD_TOOLS=ON`, not in a release.* Lists and extracts the KAT! and SfxL archives. `--check <folder>` reports missing game files, `--sprite` saves a sprite as a BMP, `--text` prints TEXT.ojd, `--obj` the OBJ.ojd graphics table, `--sfx` the SFX.ojd sound table, `--ai` AI.ojd, `--units` each unit type's counter frame, movement class and war map speed, `--arrows` the route arrow styles, `--smk` decodes a Smacker movie to BMPs and a WAV, `--save` a save's name, date, sections and fences' ends, `--addfence` and `--addunit` add a fence or a unit to a save for testing, `--models` reads and summarises every WOF, IOB and CLS in an archive, `--iob` saves a building's picture as a BMP, `--textures` writes every unit's and building's texture, sorted by side, as 8-bit BMPs, `--texcheck` checks a texture pack reads back as the game's, and `--png` reads a PNG as the engine does. |
| `wowcore` | The file formats and game data code, with no SDL dependency, so tools can use it on their own. |
| `WoWLauncher` | The one program players start, laid out like WoWRevived's WoWLauncher. With the game installed it starts a Human or Martian game; without it, it's the installer: it finds the discs in the drives (or disc images you pick), copies the game and rips the music. See [Installing](#installing). |
| `wowinstall-cli` | *Developer tool, built with `-DWOW_BUILD_TOOLS=ON` only.* The same installer from the command line, printing each step. |

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
  SoundTable.*    SFX.ojd (the sound table) and the three sound libraries the samples come from
  AiTable.*       AI.ojd (entries and each side's rule lists)
  Models.*        WOF units, IOB buildings (with their pre-rendered pictures) and CLS/ATM terrain
  SaveGame.*      save slots, and the start of a save (name, war map, game time, date)
  Vfs.*           the game's file lookup rules: archives, loose file overrides
  Bmp.*           BMP output for screenshots and wowdump
src/frontend/   Resources (graphics, strings, shade tables), Canvas (drawing), Screen (controls), Menu, CdPlayer,
                FrontEnd, Settings (WoWRevived.ini)
src/install/    the installer: Disc (folders, .iso, .bin/.cue), DriveAudio (raw CD audio), Iso9660, Installer
src/platform/   SDL3 code: window and true colour frame buffer (Video), music (Music), sound effects mixed in software
                (Sound), the .mp4 movies on Windows, Discord, starting the other program (Launch.h)
src/warmap/      the war map: WarMapData.h (sector table and borders from the exe), WarMap (the screen), production,
                the economy, the computer player
src/battlemap/  the battle map (its landscape is core/Landscape): the screen and its objects, driving and route
                finding, combat, weapons and effects, fog, reinforcements, the computer player's battles
src/launcher/   WoWLauncher: the installer and launcher, with its own Windows-style controls (Ui)
src/main.cpp    start-up and the fixed-rate game loop
tools/          command line tools
docs/           ENGINE-MAP.md (the exe mapped by subsystem), FRONTEND.md (how the menus work), WARMAP.md, TEXTURES.md
                (where a texture pack's files go), IDA index
```

## How it works

- **Frame buffer.** The frame buffer is true colour, because each sprite is drawn through its own palette, so several palettes are on screen at once. `Video::Present` scales it to the window, letterboxed.
- **Game loop.** Updates run at a fixed 30 per second, the game's default "Game Frequency". Drawing runs as fast as vsync allows.
- **Settings.** Settings are read from and saved to the same `WoWRevived.ini` the launcher and patches use, through the shared `IniStore.h`, with the game's own section and value names. Existing installs carry straight over. The registry is never used.
  The engine's own options go in a `[WoW Engine]` section: `"System Cursor" = 1` shows the system's mouse cursor instead of the game's, and `"CD Player Music Continues" = 1` keeps the CD Player's music playing after you leave it, as the original did.
- **File names.** Every game path goes through `GameFiles::Resolve`, which matches names without regard to case. The discs mix cases (`DAT\Dat.wow`, `MAPS\MAPS.WoW`, `MARTIAN.cd`), and Linux and macOS file systems are case-sensitive.

## Road map

Where the engine stands (update 234; `docs/PLAN.md` is the full list, and what to work on next):

1. **Foundations, formats, the front end, audio**: done. Every file format the game reads is read as the game
   reads it; every menu is rebuilt; sound effects, positional sound, music and the movies play.
2. **War map**: done. Units, routes, construction, production, research, repair, the economy, the Martian pods, the
   messages, the computer player, saving and loading (old saves load and write back exactly).
3. **Battle map**: done but for the tracing still open. Terrain, units, buildings, scenery, driving and route
   finding, combat, specials, the computer player's battles, reinforcements, the battle's end and its losses taken
   back to the war map. Still open: parts of the
   computer player's battle planning; combat is to be checked by eye in play.
4. **Modern fixes and options**: done. The Modern Options menu (docs/MODERN.md): battle zoom, HUD and menu scale,
   group keys, more save slots, key bindings, windowed play, Enhanced Assets, Discord, recordings and more, with
   every WoWRevived fix and file built in.
5. **Network play**: not started (the last of the original; with the player and command objects it needs).
6. **Modes and tools**: the civil wars (Human against Human, Martian against Martian) are in as a first draft
   (update 236; docs/MODERN.md "The civil wars"); auto-resolve and the editors not started.
7. **Release**: `tools/make_release.py` packages it (below); unit tests to come.

Each part is checked against the original game, address by address, using `docs/ENGINE-MAP.md` and the IDA notes in `WoWDecomp/`.

## Installing

Run `WoWLauncher`. It's both the launcher and the installer, laid out like WoWRevived's WoWLauncher (the same picture, with the buttons underneath).

**When the game isn't installed** in its folder, it opens on the install screen, and installs into its own folder unless you choose another with **Choose Install Folder...**. Put either disc in, or both, and it finds them within a couple of seconds; **Install The War Of The Worlds** copies the first disc and rips its music, then waits for the other one, carrying on by itself as soon as it's inserted (**Skip the Other Disc** finishes with what it has). **Use Disc Images...** takes `.iso` or `.cue` files instead of drives. The music is always ripped. When a disc source has none (an `.iso` image holds only the data track, not the CD audio), the disc line says so before you start, and when the install finishes a message lists each problem, why it happened and what it means: no Human or Martian music in the game, and to use the disc or a `.bin/.cue` image to get it.

**When the game is installed**, it opens on the launcher screen: **Start Human Game** and **Start Martian Game** start WoWEngine with `--human` or `--martian` (the install doesn't copy the discs' `Human.cd` / `MARTIAN.cd` side markers; with neither flag WoWEngine plays the Humans). If a side's music is missing it says so under the buttons. WoWEngine also offers to run WoWLauncher if it starts without the game files. To install again, start WoWLauncher with `--install`.

The command line version:

```
wowinstall-cli --drives                                     list the optical drives and the discs in them
wowinstall-cli [<game folder>] [<disc>] [<disc>] [--no-music]
```

A `<disc>` can be any of:
- a drive or mounted disc (`D:\`, `/media/cdrom`, `/dev/sr0`);
- a folder holding a copy of a disc;
- an `.iso` image;
- a `.cue` sheet, with its `.bin` files.

Give it both discs, in either order, or no disc at all to use whatever is in the drives. It works out which is which. Where both discs have a file, the Human disc's copy is kept, so the order makes no difference. Then it copies the game files, rips the music from tracks 2 to 5, writes the language into `WoWRevived.ini` (`[WoW Engine]` "Language"). Finally it checks that every file the engine needs is there. It skips everything WoWLauncher's clean-up deletes. The movies go where WoWRevived's wrapper looks for them: `TITLE` and `RAGELOGO` in `FMV`, and each disc's own `INTRO`, `VICTORY` and `DEFEAT` in `FMV-Human` or `FMV-Martian`, so both sides keep theirs. WoWRevived's `.mp4` versions can go next to them and are played first.

How it handles the different situations:
- **Both discs in two drives:** both are found and copied without asking.
- **The same disc in two drives, or given twice:** it's copied once.
- **One drive:** after the first disc, it asks for the other one in the same drive. It also checks the other drives in case the disc went into one of those, and says so if the same disc is put back in.
- **Discs from two different language releases:** the install goes ahead with a warning, and the text comes from the Human disc.

Music only comes from a disc in a drive or from a `.bin/.cue` image, because `.iso` images hold only the data track. Reading the music straight from a drive works on Windows and Linux. macOS doesn't allow raw CD reads, so there it has to come from a `.bin/.cue` image.

Both installers are front ends to the `wowinstall` library (`src/install`), which has no SDL dependency. WoWLauncher (`src/launcher`) draws its own Windows-style controls so it looks the same on every platform; its font is DejaVu Sans baked into `src/launcher/UiFont.h` by `tools/bake_font.py`, and its picture is `resources/launcher.bmp`, copied next to the program when it's built.

