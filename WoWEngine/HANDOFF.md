# Handoff: WoWEngine after update 135

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
   `docs/ida/functions.tsv` and `docs/ida/strings.tsv` are an index of it (`tools/ida_index.py` regenerates them),
   handy for finding a routine without opening the whole file.
2. **The game's files**: an install of the original (Human and Martian discs' data: `DAT/`, `MAPS/`, `SFX/`, `VOX/`,
   `FMV/`, `OBJ.ojd`, `AI.ojd`, `SFX.ojd`, `TEXT.ojd`, `Human.000`, `Martian.000`, ...). The tests below were run on
   such a folder, with one saved game in `SaveGame/Human.001` (a Human campaign; loading it and opening London gives
   a battle with lorries by Big Ben and a bridge).

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
- Screenshots are BMP; `convert x.bmp x.png` makes them viewable.
- Two recipes used for every update:
  - **London battle:**
    `--load 1 --ticks 500 --screenshot a.bmp "click:300:250,wait:5,rdown:300:250,wait:20,click:300:292,wait:400"`
  - **Martian battle:**
    `--martian --open 0x8A --ticks 500 --screenshot b.bmp "wait:10,click:280:320,wait:5,rdown:280:320,wait:20,click:280:362,wait:60"`
  - In the Martian battle, add `down:100:60,move:300:260,move:480:460,up:480:460,wait:3,click:400:120,wait:300` to
    box-select the walkers and send them off. `ROUTE FOUND` in the log is the move working.
- For a check the scripts can't reach, add temporary code in a copy of the file, rebuild, run and restore the file.
  Examples: forcing the pod with `s.pod = ... || true`, or printing a value with `SDL_Log`.
- Neither test battle has an enemy in range. So combat, retaliation, deaths and the AI's attacks have mostly been
  checked against the disassembly, not seen running.

## Where the knowledge is

- `docs/review/PARITY.md`: **start here.** The parity review's three phases, each item ticked with the update that
  did it, and at the end the **Deferred** list (what's still open and why) and the architecture items' status.
- `docs/review/parity/*.md`: the eight reviewers' reports. Each finding has the original's addresses and the engine's
  file:line (line numbers as of update 126).
- `docs/BATTLEMAP.md`, `docs/WARMAP.md`, `docs/FRONTEND.md`, `docs/RESEARCH.md` and `docs/UNITS.md` describe each
  subsystem as the original does it, with addresses. *Provisional* marks anything inferred and not traced.
- `docs/ENGINE-MAP.md` maps the original executable's subsystems. `docs/survey/part01-14.md` is a survey of the whole
  exe. `docs/TASKS.md` and `docs/REVIEW.md` are the older task lists (PARITY.md takes precedence).
  `docs/review/range-*.md` are per-address-range routine reviews.
- `docs/MODERN.md`: the `[Modern]` options, the only deliberate departures.
- Code comments follow the same convention: each routine names the original's address it ports, and says
  *Provisional* where it guesses.

## State after update 135

- **Phase 1 (structure), Phase 2 (high-severity behaviour) and Phase 3 (the rest) of PARITY.md are done.** The
  exceptions are the items listed under "Deferred" there. Each of those needs a missing engine piece (bridges that
  break, a stateful route finder, repair, editor cursor modes) or tracing not yet done (the save's route entry
  bytes, the game's global frame counter).
- Big pieces still missing from the engine as a game:
  - battle saving
  - repair
  - breakable bridges
  - network play
  - parts of the computer player
  - the tree circles' lifetime (S6)

  docs/TASKS.md has the longer list.
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
  - patches, tests and workflows left out
  - deleted files left out
  - no `<<<<<<<` markers

  Send it over and describe what changed.
  - The next number is **136**.
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
