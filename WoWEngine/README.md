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
| `WoWEngine` | The game. Pass the game folder as the first argument, or put the engine in the game folder. |
| `wowdump` | Lists and extracts the KAT! and SfxL archives. `--check <folder>` reports missing game files, `--sprite` saves a sprite as a BMP, and `--text` prints TEXT.ojd. |
| `wowcore` | The file formats and game data code, with no SDL dependency, so tools can use it on their own. |

## Layout

```
src/core/       file formats and game data (no SDL)
  Bytes.h         bounds-checked little-endian reader
  Archive.*       KAT! and SfxL archives
  Ffuh.*          FFUH Huffman decompression
  GameFiles.*     locating the game files, case-insensitive paths for Linux and macOS
  Palette.*       .PAL palettes and blend tables
  Sprite.*        .SPR sprites (single and animated)
  TextOjd.*       TEXT.ojd strings
src/platform/   SDL3 code (window, frame buffer, later input, audio, video playback)
src/main.cpp    start-up and the fixed-rate game loop
tools/          command line tools
docs/           ENGINE-MAP.md (the original exe mapped by subsystem) and the IDA function / string index
```

## How it works

- **Frame buffer.** The engine draws into an 8-bit paletted frame buffer, as the original did. `Video::Present` converts it through the palette and scales it to the window, letterboxed.
- **Game loop.** Updates run at a fixed 30 per second, the game's default "Game Frequency". Drawing runs as fast as vsync allows.
- **Settings.** Settings will come from the same `WoWRevived.ini` the launcher and patches already use. Existing installs and saves should then carry straight over.
- **File names.** Every game path goes through `GameFiles::Resolve`, which matches names without regard to case. The discs mix cases (`DAT\Dat.wow`, `MAPS\MAPS.WoW`, `MARTIAN.cd`), and Linux and macOS file systems are case-sensitive.

## Road map

1. **Foundations** (started): CMake and SDL3, the archives, FFUH, game file lookup, window and frame buffer.
2. **Formats** (in progress): palettes, SPR and TEXT.ojd are done. Still to port from WoWViewer: CLS, FNT, WOF/IOB, the other .ojd files, the shade tables (SHH/SHL/SHM) and maps.
3. **Front end**: main menu, options, CD Player, credits, Smacker movies (FMV), cursor and fonts.
4. **Audio**: SfxL sound effects, positional ambient sound, and the music played from the ripped `Music-Human` / `Music-Martian` WAV tracks.
5. **War map**: the strategic layer, covering sectors, construction, research, AI turns and pods.
6. **Battle map**: terrain, the 3D units (WOF/IOB with the shade tables), buildings, pathfinding, combat and fog of war.
7. **Saves**: loading and writing the original save format, so old saves keep working.
8. **Network**: multiplayer, based on the custom WoW_network.exe build.
9. **Release**: packaging for all three platforms, then the Sieve of Potentia pass.

Each stage is checked against the original game running side by side, address by address, using `docs/ENGINE-MAP.md` and the IDA notes in `WoWDecomp/`.

## Installer plan

A separate `WoWInstall` tool will set up a game folder from the original discs, or from disc images.

1. **Find the discs.** It checks every optical drive and any `.cue` / `.iso` image the user picks. The Human disc is identified by `Human.cd` and the Martian disc by `MARTIAN.cd`, the same markers the launcher uses. It asks for whichever disc is still missing.
2. **Copy the data.** It copies the data track from both discs. It skips the files the launcher already deletes: the setup files and cabs, `WoW.exe`, `WOWStart.exe`, `DIRECTX\`, `WinSys\` and the `.smk` movies that are replaced. Where both discs have the same file, it reads the Human disc's copy only once.
3. **Rip the music.** It rips CD audio tracks 2 to 5 from each disc as 16-bit PCM WAV files. They go to `Music-Human\02 Track02.wav` and `Music-Martian\02 Track02.wav` and so on, the names the _inmm wrapper already reads. How it reads the tracks depends on the platform:
   - **Windows:** `IOCTL_CDROM_RAW_READ`
   - **Linux:** `CDROMREADAUDIO`
   - **macOS:** the mounted audio CD exposes each track as an AIFF file, which only needs converting.
   - **BIN/CUE images:** for images such as the archive.org ones, the tracks are read straight from the raw 2352-byte sectors. This works the same on every platform.
4. **Check the copy.** It checks every copied file against known sizes and hashes for each language release, then writes `language.txt` and a fresh `WoWRevived.ini`.
