# The trailer

The trailer is PLAN.md's G4. A first cut was made at update 230, entirely here: the engine's own footage
recorded headless with `--frames`, WoWRevived's screenshots from the repository's `images` folder, and the cards
and captions drawn by a small compositor. The tools are in `tools/trailer`, so the trailer can be cut again as the
engine changes (below).

## What it has to do

- Show every feature the engine adds over the original game (MODERN.md), each long enough to be seen working.
- Compare the original's 640 x 480 with 1920 x 1080, side by side, on the screens where the difference is greatest.
- Say that the engine includes all of the features and file fixes made for WoWRevived, and show them.
- Mention WoWRevived's 1080p upscales of the movies, which the engine plays when they are installed.
- End with where to get it, that it needs the original discs, and the Discord.
- Be as long as it needs to be: the first cut is 6 minutes 32 seconds.

## Text on screen

Every card and caption stays up long enough to be read twice by a slow reader:

- **Duration = 1.5 s + 0.4 s per word, never less than 3 s**, rounded up to the half second, plus 0.3 s before the
  text fades in and 0.4 s after it has gone (`compose.read_time`, `Seg`). A segment given a fixed length is never
  made shorter than that.
- 0.5 s fade in and out for the text; segments fade from and to black at their ends.
- Cards are on their own, in the middle, on a dark red glow. Captions over footage sit in the lower third on a dark
  band, so the game's panels stay readable above them. Before and after pairs and the 640 x 480 / 1920 x 1080 pairs
  carry labels in the top corners.
- The typeface is IM Fell English (SIL Open Font License), a period face close to the menus' headings. The game's
  own fonts are too small and thin at 1080p.

## The cut

| Time | Part | What is in it |
|---|---|---|
| 0:00 | The original | The Rage logo movie; a card on the 1998 game; the war map and London at 640 x 480, scaled up with whole pixels and side bars as a 1080p screen shows them |
| 0:30 | WoWRevived | The launcher; the extended menu, briefing, research and CD player backgrounds (before and after wipes); the wider war map; Enhanced Assets (the icon comparisons); the fixed files (the Cotton Factory's smokestacks, the Airship, Sector 22's hole, the restored pixels); the Display Settings fix; the 1080p movies; Discord; the modding toolkit |
| 2:16 | WoWEngine | The title over the TITLE movie; written from the executable; Windows, macOS, Linux; WoWLauncher installing from a disc image and ready to start; no game data |
| 3:03 | 640 x 480 against 1920 x 1080 | Wipes between the same scripted run at both sizes: the Human war map, the Martian war map, London, the main menu; the war map at seven view sizes from 640 x 480 to 1920 x 1080 (the resolution changed without a restart); the research desks and the CD player at 1080p |
| 3:56 | Modern Options | The menu (at 640 x 480, scaled up so it can be read); battle zoom; HUD scale 100% against 200%; group keys and Shift; the sticky popup; the status strip; the save pages; cards for windowed play and key bindings, recordings, upgraded buildings and the start files |
| 5:17 | Everything from WoWRevived | Enhanced Assets as an option (off against on); the fixed files, backgrounds and text; the 1080p movies (over the original's TITLE movie); saved volumes, music playback and Discord |
| 5:49 | The war | The first Martian pod's battle, zoomed in: Fighting Machines and their heat rays against the Human vehicles; London |
| 6:07 | End | The title over the credits; "rebuilt for today"; the discs needed; the repository and the Discord |

The music is the Human disc's track 5, The Eve of the War, crossfaded into track 4 for the last part, faded out at
the end. It is the game's licensed music (Jeff Wayne's and Sony's): a video site may claim it. Replacing it is one
line in `timeline.py` (`MUSIC`).

What is shown as a card and not as footage, because a headless run can't show it: windowed play (the window is
the system's), key bindings (the ini), input recording and playback, upgraded buildings in battle, the start files.
The 1080p movie upscales are named over the original movie, as they aren't in the repository. Windows, macOS and
Linux are named; footage of each would need the owner's machines.

## How it was made

### The engine's footage

`--frames <folder>` (main.cpp, update 230) saves the picture after every update of a `--screenshot` run as numbered
BMPs, 30 to the second of game time, so a scripted run is a clip with no dropped frames. It is a developer's option
and changes nothing else.

`tools/trailer/shots.py` holds every shot as a scripted run (the same script language as `--screenshot`: waits,
clicks, keys, and mouse moves, which `glide` and `path` spread over many updates so the pointer moves smoothly).
`cap.sh` runs one with a temporary ini (the view size, any `[Modern]` setting the shot needs), puts the ini back,
brings every frame to 1920 x 1080 (smaller sizes scaled up with whole pixels, with bars) and encodes
`clips/<name>.mp4`. `stills.sh` makes the still pictures: the war map at seven view sizes, and WoWLauncher's
screens.

Notes from making them:

- The 640 x 480 side of each comparison is the engine with `--no-loose` (the game's files only), which is meant to
  match the original; real frames of the original would be better and can replace those clips.
- At 1920 x 1080, a click on the war map's pod box ("Pod landed in enemy sector ... Attack") did nothing, though
  the same click at 640 x 480 works; Enter works, and the pod shots use it. Not looked into: it may be the
  script's click rather than the game. Worth trying by hand at 1080p.
- The sticky popup is the war map's (a right click on a counter, the button let go, the popup stays); in battle a
  right click on a unit selects it.
- The pod's battle has real fighting from about 20 seconds in (the walkers' heat rays and the Human vehicles); the
  London battle has none, as HANDOFF.md says.
- The install picture is from an English disc image; on a fast disk the copy is over in a second.
- The saves on the Load menu are copies of `Human.000` with names written at 0x0C (`shots.py`, `make_saves`, only
  when the install has no SaveGame folder).

### The cut

`tools/trailer/compose.py` is the compositor: sources (a clip from a time, optionally cropped and scaled up; a still
with a slow push-in on a blurred backdrop of itself; an animated GIF; a grid of pictures; a wipe between two
sources; steps through pictures) are drawn frame by frame with PIL, captions, cards, labels and fades are laid on,
and the frames are streamed to ffmpeg. `timeline.py` is the cut itself, a list of segments; it prints each one's
start time as it renders. The music is added at the end.

### Cutting it again

Needs a Linux machine with ffmpeg, ImageMagick (`convert`) and Python 3 with Pillow, a Release build of the engine
(and a Debug build for WoWLauncher, or any build with it), and an English install (WoWLauncher or `wowinstall-cli`
from the English discs, with WoWRevived's `resources/TEXT-EN.ojd` as its TEXT.ojd and the music ripped: the
English discs' .iso images have none, so the music came from a .bin/.cue install).

```
export WOW_GAME=/path/to/game WOW_ENGINE=/path/to/build-release/WoWEngine TRAILER_DIR=/path/to/work
mkdir -p $TRAILER_DIR/fonts    # IMFeENrm28P.ttf and IMFeENsc28P.ttf from github.com/google/fonts (ofl/imfellenglish, ofl/imfellenglishsc)
python3 WoWEngine/tools/trailer/shots.py all
WoWEngine/tools/trailer/stills.sh
python3 WoWEngine/tools/trailer/timeline.py
```

The clips take a few minutes, the cut about half an hour. `timeline.py out.mp4 3 7 12` renders only those
segments, for a quick look; `review.sh <clip> [fps]` makes a contact sheet of a clip. Each run's log is in
`$TRAILER_DIR/logs`.

## Still to do

- The owner's opinion of the first cut: the order, the wording, the length, the music.
- Footage the owner could add: the original game itself (for the 640 x 480 side), the 1080p movies, the game in a
  window on Windows and macOS.
- Combat from a later campaign (more units, buildings burning), from a save where the two sides meet.
- Sound effects under the battle footage (the engine's mixer would need to write what it plays, as `--frames` does
  for the picture).
