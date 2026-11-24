# Handoff: WoWEngine after update 237

A new session starts here. This file says what the project is, where things are, how it is built and tested, what
state it is in, and how the owner likes work delivered.

## What this is

WoWEngine is an SDL3 / C++20 rebuild of the engine of Jeff Wayne's 'The War of the Worlds' (Rage Software, 1998).
It is written from the original executable's disassembly. The aim is **1:1 parity with the original**: the same
rules, numbers, draw order and random-number use, routine by routine. Modern extras sit behind `[Modern]` ini options
that are off by default (docs/MODERN.md). The engine contains no game data; it runs on the original discs' files.

About 30,000 lines of C++ in `src/` and 3,800 lines of docs in `docs/`. The repository's other folders (WoWLauncher,
WoWPatches, WoWViewer, WoWArt, Shared, WoWDecomp) are the owner's related projects; work has been in `WoWEngine/`.

## What you need besides the repository

These were in the previous session's scratch space, not in the repository. Ask the owner for them:

1. **The IDA listing of the original executable**, `IDA-Output.txt` (about 52 MB, the full disassembly with IDA's
   names: `sub_XXXXXX`, `dword_XXXXXX`, `loc_...`). Every address in the docs and code comments refers to it.
   It was made from `WoW_patched.exe`, which the owner says is the retail executable but for loading `_inmm.dll`
   in place of `winmm.dll`; the copy listed is the Italian release, so its messages and characters are that
   release's.
   `docs/ida/functions.tsv` and `docs/ida/strings.tsv` are an index of it (`tools/ida_index.py` regenerates them),
   handy for finding a routine without opening the whole file.
2. **The game's files**: the copy used so far came from https://archive.org/details/wow-cd-1 (fetched at the
   previous session's request). It is an install of the original (Human and Martian discs' data: `DAT/`, `MAPS/`,
   `SFX/`, `VOX/`, `FMV/`, `OBJ.ojd`, `AI.ojd`, `SFX.ojd`, `TEXT.ojd`, `Human.000`, `Martian.000`, ...). The tests below were run on
   such a folder, with one saved game in `SaveGame/Human.001` (a Human campaign; loading it and opening London gives
   a battle with lorries by Big Ben and a bridge).
   The item's `WOW_CD1.zip` and `WOW_CD2.zip` are `.bin/.cue` images of the Italian discs (the other languages'
   releases, linked in the repository's main readme, are `.iso`). The engine's own installer makes the game
   folder from them, music included: `wowinstall-cli <game folder> "Wow Human.cue" "WoW Martian.cue"`. A folder
   The owner has also supplied the English `TEXT.ojd` (drop it over the folder's `Text.ojd`: English menus and
   messages in the screenshots). `WoWEngine/dat` and `WoWEngine/maps` in the repository are the owner's fixed
   files for this version (docs/MODERN.md, "Files"): the `dat` ones can be copied into the game's `DAT` folder
   and are picked up as loose files; the `maps` ones wait for the installer to repack them.
   A folder made this way has no `SaveGame`; use `--human --open 0x8A` in place of `--load 1` (a new Human campaign gives
   the same London battle).

## Building

```
cmake -S WoWEngine -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8
```

(or the presets in WoWEngine/README.md). SDL3 is fetched and built if it isn't installed.

## Running it headless (how everything was tested)

```
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./build/WoWEngine <game folder> [options] --ticks N --screenshot out.bmp "script"
```

- `--load 1` loads `SaveGame/Human.001`. `--martian --open 0x8A` starts a new Martian campaign. `--human` is the other
  side. `--open <command>` opens a screen by its command id (`--open 0x8117` is the Critical events menu, `0x6F` Load
  Game, `0x3EF` Display Settings).
- The script is comma-separated steps:
  - `wait:n` runs n updates.
  - `click:x:y` clicks at a point.
  - `down:x:y`, `move:x:y` and `up:x:y` drive the left button; `rdown:` and `rup:` the right.
  - `=text` types letters.
  - Key names: `enter`, `escape`, `up`, `down`, `left`, `right`, `backspace`, `home`, `comma`, `period`.
- **Without `--screenshot` the run doesn't exit.**
- The unit tests: `ctest --test-dir build` (or `./build/wowtests`), no game files needed. Add tests to `tests/` with each
  change that can be checked without the game's data; GitHub runs them on every push (README "GitHub").
- **A civil war** (update 236; docs/MODERN.md "The civil wars"), headless on an English install with no SaveGame folder:
  `--human --open 0x90 --screenshot c.bmp "wait:20,click:320:236,wait:20,click:320:170,wait:20,=rebels,wait:5,click:320:266,wait:150"`
  (the main menu's Human Civil War, the first slot, a name, Begin Campaign); `--martian` for the Martians'. It
  writes `SaveGame\HumanCW.001`. Add `8,wait:6000` to let a fortnight run (a "no research" box stops the clock).
  The start's layout is `dat\CivilWar.txt` (update 237): without `--no-loose` the engine's own copy is read, and
  the log says so ("Civil war layout: ..."); `wowdump --save` on the new file shows where everything went.
- **Textures** (update 237): `wowdump --textures <game folder> <outdir>` writes every unit's and building's
  texture by side (MODERN.md "Tools", Texture packs). The owner has a zip of its output (PNG, with a README).
- A script draws the picture before each mouse step (update 231), so menus are placed for the view size as in a
  running game: clicks at 1920 x 1080 are the picture's own pixels. `--frames <folder>` saves every update's
  picture (docs/TRAILER.md).
- Screenshots are BMP; `convert x.bmp x.png` makes them viewable.
- Two recipes used for every update:
  - **London battle:**
    `--load 1 --ticks 500 --screenshot a.bmp "click:300:250,wait:5,rdown:300:250,wait:20,click:300:292,wait:400"`
    (with no save: `--human --open 0x8A` and `wait:10,` before the first click)
  - **Martian battle:**
    `--martian --open 0x8A --ticks 500 --screenshot b.bmp "wait:10,click:280:320,wait:5,rdown:280:320,wait:20,click:280:362,wait:60"`
  - In the Martian battle, add `down:2:2,move:100:100,move:250:300,up:250:300,wait:3,click:420:420,wait:400` to
    box-select the walkers (since update 136 they deploy to the upper left of the command building, clear of it)
    and send them across it. `ROUTE FOUND` in the log is the move working; the route goes round the building.
- For a check the scripts can't reach, add temporary code in a copy of the file, rebuild, run and restore the file.
  Examples: forcing the pod with `s.pod = ... || true`, or printing a value with `SDL_Log`.
- **War map, a unit in transit saved and loaded** (needs a slot: copy `Human.000` to `SaveGame/Human.001`):
  `--human --load 1 --ticks 30 --screenshot w.bmp "wait:10,click:328:228,wait:5,move:120:270,wait:5,click:120:270,wait:10,move:450:420,5,wait:60,backspace,wait:5"`
  sends London's counter towards the next sector and runs the clock ten hours (a digit sets the speed, Backspace
  stops it; more than a day and a pod's event box takes the input). Add
  `,escape,wait:20,click:320:362,wait:30,up,wait:3,enter,wait:30` to Save & Exit (362 since the Modern Options row went in at 196; 330 opens Modern Options); `--load 1` again shows it where
  it was. Don't click a destination on another counter: that selects the counter.
- **Reference frames of the original** (the owner's, updates 143 and 144): a new Human game with London opened,
  the same with sector 28 opened, and a new Martian game's first battle; nothing moved, 640 x 480, 16-bit colour.
  PARITY.md's update 143 and 144 entries list what they showed. Sector 28 and the Martian start match; London's
  deployment spot is still open (what has been ruled out is listed there, don't repeat it).
  Ask the owner for more of these: they settle things reading can't.
- **A battle with fighting in it, start to finish (update 168; Stage 5.1).** A new Martian campaign's first pod
  lands in a Human sector (9, with the discs' start files and no change of view), and the whole loop runs
  headless: war map, the pod's box, the Briefing, the battle fought by the two sides' own behaviour, its end box,
  the Debriefing, the war map again with the losses carried. In the script the waits are the time: `--ticks`
  only adds updates after the script, before the screenshot.

      SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy WoWEngine <game> --martian --open 0x8A --ticks 30 \
        --screenshot out.bmp "wait:30,8,wait:450,click:320:445,wait:200,click:320:240,wait:6000,\
        click:298:439,wait:400,click:298:439,wait:400,click:298:439,wait:400,click:298:439,wait:400,\
        rdown:300:150,wait:25,click:300:192,wait:90,click:320:240,wait:120"

  `8` sets the clock's speed; `click:320:445` is the pod box's button on the war map (Attack);
  `click:320:240` dismisses the Briefing; the battle takes some 6,000 to 7,500 updates and differs from run to
  run (the game's table of random numbers is seeded from the clock at start-up, 0x401AE8), so the end box's
  button (`298:439`, a Martian game's) is clicked several times 400 updates apart; then the popup
  (`rdown:300:150`), its Return to Warmap (`click:300:192`), the Debriefing (stop here for its figures), and a
  click back to the war map. About two minutes of real time. Cut the script short for a frame of any stage.
  The fighting is west of where the view opens: `click:566:430` on the mini-map straight after the Briefing
  puts the view on it (the Command Post), where frames 250, 500 and 900 updates on show the three Scouts and
  the Human vehicles at it.
- The headless battle recipe: after `--open 0x8A` and the script's opening clicks the Briefing is still up for
  some updates; wait about 80 before scripting clicks on the battle itself (`wait:80,down:45:45,wait:2,up:45:45`
  clicks the first group icon). `click:x:y` is a press and release in one step.
- **Before writing that the game doesn't have something**: an object's handler hands what it doesn't take on to
  its base class's (WSelectableObject 0x440300 for every unit and building), so one routine is never the whole
  answer. Update 155 declared the popup's Information item non-existent that way; the owner's frames showed it.
  Group keys and Shift-select really aren't there (the owner confirmed). When in doubt, ask the owner.
- Neither test battle has an enemy in range. So combat, retaliation, deaths and the AI's attacks have mostly been
  checked against the disassembly, not seen running. The owner's word on it: combat needs the game played, or
  waited through, until the two sides meet; there's no shortcut in the data.

## Where the knowledge is

- `docs/PLAN.md`: the staged plan and task list for what's left. Its first section ("Where things stand") is the
  working order and the open questions being tracked. Start with it.
- `docs/MODERN.md`, "Modern fixes": the deliberate departures that are on by default (the owner's WoWRevived
  fixes). Marked `MODERN FIX` in the code; don't correct them back to the original.
- `docs/review/PARITY.md`: **the parity detail.** The parity review's three phases, each item ticked with the update that
  did it, and at the end the **Deferred** list (what's still open and why) and the architecture items' status.
- `docs/review/parity/*.md`: the eight reviewers' reports. Each finding has the original's addresses and the engine's
  file:line (line numbers as of update 126).
- `docs/BATTLEMAP.md`, `docs/WARMAP.md`, `docs/FRONTEND.md`, `docs/RESEARCH.md` and `docs/UNITS.md` describe each
  subsystem as the original does it, with addresses. *Provisional* marks anything inferred and not traced.
- `docs/ENGINE-MAP.md` maps the original executable's subsystems. `docs/survey/part01-14.md` is a survey of the whole
  exe. `docs/TASKS.md` and `docs/REVIEW.md` are the older task lists (PARITY.md takes precedence).
  `docs/review/range-*.md` are per-address-range routine reviews.
- `docs/MODERN.md`: the `[Modern]` options, the only deliberate departures.
- `docs/TRAILER.md`: the trailer (first cut at update 230) and how to cut it again with `tools/trailer`; `--frames`
  records a scripted run's every update as pictures.
- Code comments follow the same convention: each routine names the original's address it ports, and says
  *Provisional* where it guesses.

## State after update 237

- **Phase 1 (structure), Phase 2 (high-severity behaviour) and Phase 3 (the rest) of PARITY.md are done.** The
  exceptions are the items listed under "Deferred" there. Each of those needs a missing engine piece (bridges that
  break, a stateful route finder, repair, editor cursor modes) or tracing not yet done (the save's route entry
  bytes, the game's global frame counter).
- Big pieces still missing from the engine as a game (update 231; docs/PLAN.md "Where things stand" is the list
  to work from, docs/TASKS.md is older):
  - network play, with the player and command objects it needs (A9, A2)
  - breakable bridges and the route finder spread over updates (PARITY.md, Deferred)
  - parts of the computer player (TASKS.md, Battle AI)
  - the tree circles' lifetime (S6)
  - the two buildings the game names and never offers (E1, E2); the editors (F2); the civil wars' balance and the
    rest of their list (F1, MODERN.md "The civil wars")

  Repair is in (WARMAP.md "Repair"). A battle is never saved part way in the original (its menu has no save), so
  there is no battle saving to add (PLAN.md, 4.4).
- The trailer, a Shorts version and title cards were made (230, 231; TRAILER.md). A world map mod was drafted
  outside the engine (`WoWEngine-Mods/WorldMap`, its README and DESIGN.md); nothing in the engine reads it.
- Known open questions are in PARITY.md (S6's tree circles, the Deferred list) and under *Provisional* in the docs.
- Things worth a look in play, because they were only checked by reading the disassembly:
  - combat after update 132 (quiet vanishing kills, damage rounding, the firer's sparing)
  - the production icon's blink (no factory in the test saves has a queue)
  - the pod's landing in a Martian-held sector with no command building
  - the forfeit's return to the war map

## Working conventions the owner uses

- **Delivery is a zip per update, not a push.** The owner merges the zips into the repository themselves. Make
  `WoWEngine-update-N.zip` of the files changed against `origin/main`:
  - CRLF line endings
  - patches left out; since update 235 the unit tests (`WoWEngine/tests/`) and the workflow
    (`.github/workflows/wowengine.yml`) go in the zip like the rest (the owner asked for them)
  - binary files (`.ico`, `.spr`, `.dll`, `.ojd` and the like) copied as they are, not given CRLF
  - the parity frames are the same from run to run only because the random seed is the milliseconds since the
    engine started, which is the same few on an idle machine; on a busy one (just after a rebuild, or a
    restart) a run can come out with another seed and a different frame. Run it again before taking a
    difference for a fault
  - a run without `--screenshot` writes the first start's view size into the test install's ini: restore the
    ini afterwards
  - the parity checks are run with `--no-loose` (the game's files only); without it the engine's own `dat`
    and `maps` are used and the pictures differ
  - deleted files left out
  - no `<<<<<<<` markers

  - the zip is made to be extracted over the repository's main directory: its paths start `WoWEngine/`
    (`WoWEngine-update-136.zip/WoWEngine/src/...`)

  Send it over and describe what changed. No patches, no pushing.
  - The next number is **238**.
  - The start files are the same on every release's disc. The owner's testing folder had a day-old pair (the
    game's slot 0 save overwrites them, docs/ODDITIES.md): frames from updates 157 to 159 were taken from that
    state, kept in the scratch notes as the owner's pair. The test install uses the discs'.
  - A checking build finds memory faults the ordinary one hides: configure a second build folder with
    `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"` and run the recipes on it
    (about three minutes to build, runs some ten times slower).
  - The script's `click` on the speed slider (`down:590:30,wait:2,up:590:30` in a Martian game) starts the clock
    as the mouse does; a digit does it the keyboard's way.
  - The log file, with its CRASH and STALL lines, stays in for release (the owner, update 176): don't take it
    out as debugging.
  - To try a view size here: copy `WoWEngine/dat/extra` into the test install's `DAT`, add `[Screen]` `Size=1024,768`
    to its `WoWRevived.ini`, and take a screenshot of a menu (`--open 0x3EF` is Display Settings). Put both back
    after: the parity checks are at 640 x 480 with the game's own files. A screenshot run never writes the size.
  - WoWEngine.log's own lines to look for in a report from the owner: `CRASH:` (a fault, with what the
    engine was doing) and `STALL:` (the main loop stopped for 5 s). Neither can be built or run here on the
    Windows side, so a compile error in `main.cpp`'s `_WIN32` parts is possible: ask.
  - Frames from the owner: check they were taken with WoWRevived's **Enhanced Assets off** before chasing a
    difference in a sprite's pixels (update 158 lost time to this).
  - Work only in `WoWEngine/`. For a file format, `WoWViewer/` has the owner's own readers in C#.
  - Commit locally but don't push unless asked; a stop hook may ask to push, and the owner's answer has been no.
- At the start of each update, merge `origin/main` (the owner's merged copy) first.
- Each update:
  1. Implement.
  2. Build and run both test battles headless.
  3. Update the docs: tick PARITY.md with the update number, and add notes to the subsystem doc.
  4. Commit.
- Docs style: plain prose, the original's addresses in brackets, *Provisional* for anything not traced, no
  marketing. Code comments the same.
- The owner reports bugs seen in play (a black move marker, units behind bridges, the map seen loading in layers).
  Those have been the most valuable leads: check them against the disassembly first.

## Suggested next steps

1. Read PARITY.md's Deferred list and pick what the owner wants first. The larger ones are:
   - movement A3 / 17: the route finder as a stateful object that runs over several updates
   - breakable bridges (movement 21, A5)
   - units in transit after a load (warmap 14): trace the WMOB route entry bytes against 0x4257C0
2. Battle saving and repair are the biggest missing game features.
3. If the owner can play-test, ask what looks or behaves differently from the original; that has found more than
   reading has.
