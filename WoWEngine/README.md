# WoW Engine

An SDL3 / C++20 rebuild of the engine from Jeff Wayne's 'The War Of The Worlds' (Rage Software, 1998), for Windows, macOS and Linux.

The engine contains no game data. It runs on the files from the original Human and Martian discs, which the installer copies (see below).

## Building

Needs CMake 3.20+ and a C++20 compiler (Visual Studio 2022, Xcode 15 / Apple Clang 15, GCC 11+, Clang 14+).

```
cmake -S WoWEngine -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

If SDL3 isn't installed it is downloaded and built automatically (`WOW_FETCH_SDL`, on by default).
To use an installed copy, point `CMAKE_PREFIX_PATH` at it.

Targets:

| Target | What it is |
|---|---|
| `WoWEngine` | The game. Pass the game folder as the first argument, or put the engine in the game folder. `--open 0x6E` starts on the screen a menu command opens. `--ticks N` runs N game updates before a screenshot. `--screenshot file.bmp [keys]` presses the listed keys (up, down, left, right, enter, escape), saves the frame and exits. |
| `wowdump` | Lists and extracts the KAT! and SfxL archives. `--check <folder>` reports missing game files, `--sprite` saves a sprite as a BMP, `--text` prints TEXT.ojd and `--obj` prints the OBJ.ojd graphics table. |
| `wowcore` | The file formats and game data code, with no SDL dependency, so tools can use it on their own. |
| `WoWInstall` | Sets up a game folder from the two discs or their images, and rips the music. See [Installing](#installing). |

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

```
WoWInstall --drives                                     list the optical drives and the discs in them
WoWInstall <game folder> [<disc>] [<disc>] [--martian] [--no-music]
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

The installer is the `wowinstall` library (`src/install`). It has no SDL dependency, so a graphical installer or a rewritten launcher can use it directly.

