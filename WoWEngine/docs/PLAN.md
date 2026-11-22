# Plan after update 136

## Where things stand (update 211) and what remains

Stages 1 to 3 are done but for the few items below. Stage 4 is half done. The game plays through: campaigns
start, load and save (the owner's 24 saves read and write back true), the war map runs, battles are fought and
settled. What follows is everything left, in the order to do it; the stages further down stay the full
inventory with the history. Tick here and there as each is done.

### A. The original, finished (parity first, as always)

- [x] **A1 Constructions and resources written whole** (189). Every new war map object is now made from
      nothing in the game's layout (`SaveObject::MakeUnit` / `MakeBuilding`), checked against every object in
      the owner's saves; nothing is copied any more. The "extra value" on some resources was the sector's
      owner, which follows the last list. Left for later: a fence's two ends are written as 0 (the war map
      can't place one); a new building has no battle place until the deployment or the computer player's site
      gives it one, as in the game.
- [ ] **A2 The players and their commands** (4.5): the local, computer and network player objects; the command
      records (0x4460A0); the commands the world handles one queue pass later (0x95, 0x419A24; was 3.7).
      In a one-player game these change nothing that is seen (the engine's orders go straight to what they
      order), so this is done with the network game, A9, which is what needs it.
- [x] **A3 The wait cursor** (closed, 190). The busy object (0x40E680) sets cursor 13 round a save (0x4042D0),
      a load (0x408B70) and the war map's reading (0x418500), and nothing is drawn in between by the game's
      own thread; it shows only because the window's thread draws the cursor when the mouse moves
      (0x42D0FC) while the game's is busy for seconds. The engine does each inside one frame, so there is no
      moment to show it in. The cursor's animation is in (177).
- [x] **A4 Stage 3's last boxes** (closed, 190). 3.2: the context buttons, Suspend / Resume and
      CommandCentreBuilt were found in or not the game's in 153 to 156; "construction's remaining fields" is
      A1's, done. 3.5: the popup's Information item and page (156, with its speech 176), the items on a unit
      in transit (157), the tool tips (154, 155). Nothing is left under either.
- [ ] **A5 Deployment's leftovers** (1.2): London's deployment spot (the lorries at ring 95's 288 degree spot in
      the original, 72 in the engine); the update order of the start lists. *Needs the owner's frame* (below).
- [ ] **A6 The provisional renderers** (2.9): the WOF triangle filler, the blitter modes, the bm shade group's
      other slots. The owner's frames match closely, so each waits for a frame that shows a difference.
- [ ] **A7 Combat checked by eye** (5.2, 5.3): quiet kills, damage rounding, the firer's sparing, retaliation,
      deaths, the pod's landing, the forfeit's return; Force Fire and the specials ending after one shot (173).
      A standing item: the owner's reports from play have found more than reading has.
- [~] **A8 The developers' switches, for the record** (4.8, 4.9; the owner: keep them, low priority). In (190):
      Charlie (`[Debug]` "Charlie": ResearchAlert, PodLanded and UndefendedSectorLost without their boxes and
      not critical); Mat as a setting as well as the cheat (`[Debug]` "Mat"); on the war map, Mat or Edit
      functionality letting any sector be selected; Mat's time of day (179). Left: on the battle map, Mat's
      enemy units picked one at a time with Shift's state (0x497680, 0x4972F0) and the reinforcement points
      made with no battle on (0x43D171, 0x43D2D2); Edit functionality's picking of scenery (0x485A1A,
      0x485DA9, 0x48A87D); Dave's debug text and the debug log.
- [ ] **A9 The network game** (4.7), last of the original: the menus' lists, the lockstep packets. The owner's
      reading of what the network build needs: its timer out, network play itself, the start files not saved
      over (already so).

### B. Modern fixes (Stage 6.1 to 6.6; on by default, each marked MODERN FIX)

- [x] **B1** (191) The two sound fixes marked where they are (the sliders saved; Master reaching the music);
      Ambient and Speech were already saved as the menu closes.
- [x] **B2** (191) The music toggle acts at once.
- [x] **B3** (191) Display Settings without the colour radios and the lights toggle; the lights always on.
      `[Modern]` "Original Display Settings" = 1 gives the original's menu.
- [x] **B4** (192) The first start takes the desktop's resolution, by the rule agreed with the owner
      (MODERN.md "Resolution").
- [x] **B5** The resolution applied (6.5), in three steps:
      - [x] (a, 192) the picture is the view size; the menus and the other 640 x 480 screens are centred in it
        over the backgrounds made for the size, never scaled; the slider acts at once; the picture is scaled
        to the screen with its shape kept;
      - [x] (b, 193-194) the maps' views at the view size: the war map's (its view, edge scrolling, panels'
        and mini-map's anchors, its background chosen by the view's width) and the battle map's (the view,
        scroll and zoom limits, edge scrolling, the minimap, popups, the end box, the group icons, the sound's
        listener and borders, 0x464B10); the menus over both centred. Not looked at yet: the Build List and
        the information page on a large battle view;
      - [x] (c, 195) the briefing, debriefing, research and CD player drawn at the top left of a large
        view, where their large backgrounds have the original picture's frames; the movies stay 640 x 480,
        scaled up to fill the screen. Putting `dat/extra` and `dat/warmap` in the game's `DAT` is with D2.
- [x] Already in: the start files never saved over (160); upgraded buildings fight as their level (186).

### C. Modern options (Stage 6.7, 6.8)

- [x] **C1** (196) A Modern Options menu (from Game Options and the war map's Campaign Options), holding what is
      in already: upgraded buildings in battle, sticky popup menus, the war map status strip, the original
      Display Settings, playing in a window, stretching the picture, the system pointer, the CD player's music
      carrying on. What follows is added to it.
- [x] **C2** (197) Playback recordings started and stopped from the Modern Options menu ("Record input", "Play a
      recording"; Escape ends a playback). Not tried by hand yet.
- [x] **C3** (198) Group keys (Ctrl + digit keeps, the digit brings back, again looks at them) and Shift adding to
      the selection in battle, `[Modern]` "Selection keys", on unless turned off.
- [x] **C6** (200) More save slots: the Load and Save menus turn pages of five, up to slot 999 (the owner's
      idea, update 199). `[Modern]` "More save slots", on unless turned off.
- [~] **C4** The battle's zoom (202) and the HUD scale (205 the battle's panels, 207 the war map's) are in:
      `+` / `-` or the wheel, `R` to put it back, zoom the land only; the panels of both maps go by the "HUD
      scale" slider in Modern Options. The window keeps its size (208). Key bindings in `[Keys]` (209). Done but for
      what the testers say about the zoomed brackets.
- [ ] **C5** The log kept for players' reports (it is: CRASH and STALL lines; nothing to do but not remove it).

### D. Files and assets (Stage 6.9, 6.11)

- [ ] **D1** Every data folder overridable by loose files (with `[Sound]` Search Libraries First), modern
      formats for them.
- [ ] **D2** The owner's fixed files (the text changes, the Cotton Factory's smokestacks, the Airship model's
      name, sector 22's hole, the restored sprite pixels) and the data folder layout (`dat/warmap`,
      `dat/extra/<res>`, `dat/extra/HD`, `dat/extra/UNITS`).
- [ ] **D3** Enhanced Assets as an option; every `.spr` re-encoded by WoWViewer's packer.
- [ ] **D4** The credits picture: the team's hidden screen kept as the last 480 rows of any edited one, added
      lines above it (the owner rates this high).

### E. Content the game names and never offers (after everything regular works)

- [ ] **E1 Matter Transfer Station** (Martian, type 122: model, counter, sound, spoken page, research strings; no
      builder; "Unused" for its research-done line). First: is it in the research tree's data, does any code
      name it. Then builders behind an option, and an effect on the economy designed with the owner.
- [ ] **E2 Railway Platform** (Human, type 59: a name, a counter frame, a page's text). Needs a model made;
      behind a testing option until then.

### F. Modes and tools (Stage 6.10)

- [ ] **F1** Campaign extras (Martian against Martian, Human against Human); auto-resolving a battle.
- [ ] **F2** Level editor; war map creator and new campaigns; save editor.

### G. Release

- [ ] **G1** The Windows-only code built and tried on Windows (the crash filter and the watchdog in main.cpp,
      the Media Foundation movies): it can't be compiled here.
- [ ] **G2** Unit tests for the decoders, the save's round trip and the war map's rules.
- [ ] **G3** The release scripts tried from a clean checkout; the docs brought level (TASKS.md is stale in
      places and should be redrawn from this list).

### Waiting on the owner

- A frame of the original: a new Human campaign's first look at London, on the discs' start files, Enhanced
  Assets off (A5).
- A frame of the original with a building that was upgraded where it stood, on the battle map (confirms 185).
- A recording made at the keyboard with `--record` and played with `--playback` (183); whether ordinary input
  still feels right since it all goes that way now.
- An engine-written save loaded in the original (178).
- `WoWEngine.log` if the crash after a battle comes back (172, 175).
- Whether main.cpp builds on Windows (G1).

**The owner's saves** (update 178): 24 saves of the original's, early to end game, some hex-edited for larger
groups, are the test set for anything that reads or writes a save. In the scratchpad they are under `saves/`;
ask the owner for the folder again in a new session (`SaveGame.zip`). The check: load each, save it at once,
and compare with the original section by section.

**Open questions to keep track of** (each has its notes in PARITY.md; don't re-trace what is ruled out there)

- **A crash after coming back from a battle** (the owner, update 172: a Martian campaign, the war map shown, a
  stall, the window gone, nothing in the log). Not reproduced in updates 172 or 175: wins, losses, the forfeit,
  the player's objects losing some or all of their units, the clock left running across the battle, twelve
  whole games on different seeds, most of it under the address and undefined-behaviour checkers. What the
  owner's log does say: it ends at the war map's return with no line after it, so before the war map's music
  step (30 updates on) had started its track, and before any hour had passed. Everything that runs in that
  second runs here without fault, so it is likely something only Windows does (the audio device, the file
  calls). The log now names a fault (172) and a hang (175: a STALL line after 5 s with what the engine was
  doing), and the music file's reader can no longer loop on a bad chunk length. Wait for the next log.
  The owner tried again (update 176) and it didn't happen; they are keeping an eye out. The log's CRASH and
  STALL lines stay in for release, by the owner's wish, for players' reports.
- **London's deployment spot** (updates 143, 144). The original puts the lorries at ring 95's 288 degree spot,
  the engine at its 72 degree one. N2 (148) didn't settle it: with the parts and segments in the grid the engine
  still takes the 72 degree spot. Worth a fresh frame from the owner on the discs' start files, a new Human
  campaign's first look at London: the Martian frame of the same day turned out to be of a saved state (update
  171), and this one may be too.
- **Lighting in London** (update 143): Big Ben and Parliament are brighter in the original because its lorries
  stand beside them with their headlights on; the engine's stand to the south. It goes with the deployment
  spot above, not a lighting fault (update 151).
- **The route save layout** against a real save of the original with units in transit (asked of the owner).

Ask the owner for a frame of the original whenever a change is visible: a fresh game, the battle opened,
nothing moved.


What is left, in stages, each stage a run of updates (a zip each). It gathers the open items of
[review/PARITY.md](review/PARITY.md) (Deferred), [TASKS.md](TASKS.md) (the `[ ]` and `[~]` items) and
[MODERN.md](MODERN.md), and the owner's notes of update 136. PARITY.md and TASKS.md stay the detailed lists; tick
the item there and here as each is done.

The rule is unchanged: the original's behaviour first, routine by routine, with its addresses. Stages 1 to 5 are
that work, and the owner's decision (after update 136) is that it all comes first: there is still a lot of it.
The last stage, Stage 6, is where the engine departs from the original on purpose: the *modern fixes* (the
original's own faults, which the owner already fixed in WoWRevived; on by default) and the modern options. Each
fix is to be marked `MODERN FIX` in the code and is listed in MODERN.md, so a later parity pass doesn't undo it.
Until Stage 6 the settings menus stay as they are; nothing there is to be changed early.

## Stage 1: the open parity items (done but for what's noted, updates 137-140)

Small and independent; most need one routine traced.

- [x] 1.0 Check the listing at WoWRevived's seven executable edits (the readme's list: the CD Player's check,
      the colour radios, the lights toggle, the .smk check, the winmm redirect, the resolution slider, the drag
      select border). `IDA-Output.txt` was made from `WoW_patched.exe`; where an edit is in it, the docs must
      describe the retail bytes, not the patched ones. The drag select border (640 to 2048) is the one that could
      have been ported as the original's.
      Do this first.
- [x] 1.1 Units on a route after a load (warmap 14; TASKS "Route progress"): the route's entry list reader (the
      route's +0x64 list, vt+8) traced to find each entry's +0x24 / +0x38 / +0x3C in the save, then saved and
      restored without planning again.
- [ ] 1.2 Deployment's leftovers (update 136's provisional points). Done in 137: what ActivateAll takes out again
      (scenery on scorched ground) and the grid 0x47FE10 searches (the scenery counts). Done in 140: a building's parts aren't in
      the grid. Done in 141: a bridge under a starting building goes too. Left:
      the start order kept for the update order (the world list is HUNI, HCON, HRES, AUNI, ACON, ARES). Done
      in 145: the order of the rand() calls inside one start. Open (PARITY.md, update 145): how the deployed
      units end up facing.
- [x] 1.3 Tree circles hit by weapons (S6's last open point).
- [x] 1.4 Muster's first step and the start order of 0x495740 (movement 20).
- [x] 1.5 The fog's timing by the global frame counter, dword_4D2558 (battle-setup 12).
- [x] 1.6 The end-of-battle counts' lists (battle-setup 18).
- [x] 1.7 The second target, +0xC8 / +0xCC (movement 19).
- [x] 1.8 The window's 32-pixel scroll border and the game's windowed flag, dword_4EA364 (battle-ui 9); the
      resolution itself is Stage 6.
- [x] 1.9 Front end: what 0x80CF answers per control; the CD Player's live time and starting track, and its
      +/- 5 s seek; Debriefing's delay from its second draw.
- [x] 1.10 The war map battle pulse's own draw counter (warmap 20).
- [x] 1.11 The latent data items (data 6, 8, 13).


## Stage 2: the battle map's missing pieces

- [x] 2.1 The route finder as a stateful object, its search spread over updates, a second group's held off 10
      updates (movement A3, 17).
- [x] 2.2 Bridges as objects that break; broken links; the ground under them redone (movement A5, 21).
- [x] 2.3 Footprints taken off and route nodes removed as objects go; crushing.
- [x] 2.4 The walker subclass: the leg animation's rate, footprints (0x48FDD0, 0x490070).
- [x] 2.5 Fliers and boats checked against their classes; the units' headlights; the scenery's and bridges' object
      scale.
- [x] 2.6 The group panel (0x483DF0 / 0x484120), the label on a unit gone under (font 0x6F, 0x4991D0), the
      combined panel (0x448500, PARITY battle-ui 7). The group keys and Shift-select aren't in the game. Left:
      cursor modes 2-7 where the game (not the editor) uses them.
- [x] 2.7 The zoomer and the battle-end box (both in). Reinforcements arriving mid-battle aren't a routine of
      the game's (update 151).
- [x] 2.8 The rest of the computer player in battle (0x80B1's threat seen, update 152; the rest was in).
- [ ] 2.9 Renderers still provisional: the WOF triangle filler, the blitter modes, the rest of the bm shade
      group's slots.
- [x] 2.10 Battle music (CD tracks 4 / 5).

## Stage 3: the war map's missing pieces

- [x] 3.1 Repair (Mobile Repair Vehicles, workshops), with the repair icon (update 153). Left: the power bar,
      what 0x8124 asks of a workshop, and a counter's pips.
- [x] 3.2 (closed, 190: see A4) Construction's remaining fields, CommandCentreBuilt, the context buttons (factory panel, Suspend /
      Resume, Retreat).
- [x] 3.3 The hourly step's fan-out, checked against 0x419E9D in update 165: the routes, the players, both
      sides' research, the pods, each sector and the resource warnings are all in, in that order. Damaged
      facilities: 155. (Work demand in the economy: not looked at again.)
- [x] 3.4 Several units sent as one route (update 166: one arrow, one interruption, one arrival); the world's
      0x802C when no routes are left has no handler (traced, nothing to do).
- [x] 3.5 (closed, 190: see A4) The popup as child windows; the sector's and objects' remaining items; the remaining tool tips.
- [x] 3.6 The battle report's own counters (update 167): 0x439C88 / 0x43A800 / 0x4212F0 traced; vehicles lost
      are every vehicle destroyed, and a destroyed vehicle leaves its war map object.
- [ ] 3.7 (with A2 / A9) Commands one queue pass later (0x95) where the order matters.

## Stage 4: the rest of the original (laid out afresh in update 177)

Stage 3's open boxes were checked in updates 174 to 176 and are stale but for 3.7, which moves here as 4.5.
A battle is never saved part way (the battle map's menu has no save, 0x49BD50), so "saving a battle" was the
wrong name: what is left of it is what the war map's save keeps of a battle's objects (4.4).

- [x] 4.1 **The settings, all of them** (closed in 181: what is left unread is never read by the game, or is
      the window's, the developers' or another stage's). The listing defines 225 (found by their constructors' section and key
      strings); 62 weren't named anywhere in the engine at update 176. By what they are:
      - *Defined and never read* (181: neither the value nor the setting's object is referenced again):
        `[Colours]` Credits Heading, Credits Detail; `[Options]` Show simple tooltips; `[Tweak]` AI Oracle;
        `[Fun]` Veh Speed, Veh Rot Speed; `[English]` Language.
      - *Done in 179*: `[Colours]` Messagebox Command (22); `[Game]` Time Of Day (the battle's light forced to
        that hour with the curtains cheat on, 0x40B030); `[Sound]` the four borders. Not missing after all:
        InsulatedHighEnergyResistantArmourMultiplier (the listing's comment cuts the name short).
      - *Done in 177*: `[Colours]` Info Page Text (15) and Messagebox Detail (21); `[Tweak]` Res Stack XO / YO
        (4 / 16).
      - *With 4.3*: `[Sound]` Search Libraries First, Unload Limit; the borders scaled by the screen's size
        (0x464B10), which only matters past 640 x 480 (Stage 6.5).
      - *No reader found in the listing* (confirmed the same way, 181): `[Tweak]` AI Hours Per Turn, IMap Iteration
        Count; `[Vehicles]` stopTurnRate.
      - *The window and DirectDraw* (Stage 6's, or nothing): `[Screen]` Use DDraw surfaces, BLIT instead of
        FLIP, Damage map VRAM, Top left, Support screen size, Support screen BPP, AllowResize, FixedAspect,
        Use Reaper; `[System]` the six memory and frame-time ones.
      - *The developers' own* (4.8, optional): `[Debug]` the twelve, `[Stats]` the eight.
      - *With their stages*: `[Network]` the three (4.7), `[Demo]` Playback Files (4.6).
- [x] 4.2 **Cursors** (closed, 190: see A3). Done in 177: the cursor's sprite animates (0x40CDC0, half a frame a tick). Left: the wait
      cursor (cursor 13) shown by the busy object (0x40E680) while a game is saved (0x4042D0), loaded
      (0x408B70) and the war map built (0x418500); the engine does these inside one frame, so it is a question
      of whether anything would be seen.
- [x] 4.3 **Sound's leftovers** (the borders are settings, 179; Minimum Audible Volume was read already.
      Looked at in 181 and left: Search Libraries First only chooses between the library and a loose file,
      and the loose file is only looked for with a developers' flag on, 0x4660D4 / 0x466910, so it goes
      with Stage 6's loose files; Unload Limit frees a sample not used for that long, 0x4666DC, which
      nothing shows): the borders as settings and by screen size (above), the library before loose
      files, the unload limit, Minimum Audible Volume.
- [x] 4.4 **War map objects written field by field** (closed in 189; see A1) (was 4.0 and 4.1; begun in 178 with the owner's 24 saves:
      a save read and written straight back now matches the game's but for the view, the message list and a
      few rounded numbers; the routes' layout is confirmed and the Damage Layer's corrected): a new object made as the game makes it
      (0x43CD10 / 0x42A030) and not copied from a saved one of its type; each battle unit's BAMO / BATP and
      class tags read and written as fields, not carried as bytes; the sector's battle and pod flags; a route's
      progress in the save (done, 178: five of the owner's saves have a route; they read as the engine wrote
      them, and the route's +0x34 is a count, not a float).
      Done in 182 for a new unit's vehicles: the type's own number of them, each at full health and with no
      battle place (what the owner's saves hold for a unit that has never fought). Done in 184: a new unit
      is written whole, from nothing (`SaveObject::MakeUnit`), so any unit type can be made whether or not
      the save has one; the layout matches every unit in the owner's saves. Left: a construction or a
      resource is still a copy of a saved one, so one whose kind and category the save lacks can't be made.
      What their records are, from the saves (185), for when they are written whole: the type, two floats,
      SELO (id, place), WMOB (sector), BMOL 1, then one battle object (type, SELO, BAMO's five values, BATP)
      and the tail: for a building, whether it has a factory, its queue, its +0x3C (100 bytes with no
      factory, 104 with an empty one, 33 more for each thing queued); a gun emplacement's has VEHI before
      that; types 451 to 456 have four more values first, two points (they run between two places); some
      resources have one more value that isn't understood (0 or 1).
- [ ] 4.5 **The players and their commands**: the local, computer and network player objects, the command
      records (0x4460A0), and the commands the world handles one queue pass later (0x95, 0x419A24; was 3.7).
- [x] 4.6 **Playback recordings** (in, update 183: `core/Playback.h`, with the switches `--record` and
      `--playback` until Stage 6 gives them a place in the modern options): the game's self-running demo. Every input message goes through one routine
      (0x42D520) that can append it, with its tick, to `Playback.1`; another (0x42D2D0, each frame) reads
      `playback.1` to `playback.N` (`[Demo]` Playback Files, 4) and posts the messages back on their ticks.
      Playback is turned on by the world's command 0x808F (0x419C22); nothing in the retail listing turns
      recording on (byte_4EA3F4 has no writer). They are raw input, not the players' command records, so
      they don't wait for 4.5. Neither switch can be reached in the retail game (update 183): the only
      sender of 0x808F is the research screen's blueprint (0x45F73B), whose click the research screen takes
      itself to say the speech again (0x45D29D) and doesn't pass on. So the system is whole but dormant.
      The owner: implement it as the original has it, with the way in under the modern options (Stage 6).
- [ ] 4.7 **The network game**: the menus' lists and the lockstep packets. The owner wants it tried, last or
      nearly last (update 178). Their reading of what the network build needs to play a campaign: its timer
      taken out, network play itself, and the start files no longer saved over (the engine already doesn't,
      docs/ODDITIES.md). The retail network code doesn't work; WoWRevived ships another build for it.
- [ ] 4.9 **The curtains cheat** (byte_4D1279, found used in 179; the engine set the flag and did nothing with
      it but the time of day): enemy units can be picked in a battle and Shift adds to the selection
      (0x497680, 0x4976F5), a sector that isn't the player's can be selected on the war map (0x41D24A,
      0x41D430), and the reinforcement points are made without a battle on (0x43D171).
- [ ] 4.8 **The developers' aids** (optional; Charlie, Dave and Edit functionality are read out in
      docs/ODDITIES.md, update 180): the debug log (`[Debug]` Log Debug Output, 0x417C30; WoWEngine.log
      stands in), the overlays (Show Radii, Show Arrival, Show Orders, Show Accelerations), `[Stats]`.

Done before this list was redrawn: speech attached to text ids (176).

## Stage 5: combat seen running

Not a block of work but a standing need: neither test battle has an enemy in range, and combat only comes by
playing or waiting until the sides meet.

- [x] 5.1 A headless script that runs a campaign's clock on the war map until a battle is announced, opens it and
      saves the screenshots and the log; kept as a recipe in HANDOFF.md. Done in update 168: a new Martian
      campaign's first pod, fought through to the Debriefing and back to the war map.
- [ ] 5.2 (begun, update 169: frames of the fight sent to the owner to say what looks wrong; no fault seen
      in them here, but there is no frame of the original's to set beside them) With it, the items only checked by reading: quiet vanishing kills, damage rounding, the firer's sparing,
      retaliation, the look round, deaths, the pod's landing, the forfeit's return.
- [ ] 5.3 The owner's reports from play, checked against the disassembly first as before.

## Stage 6: modern fixes and modern options (last, once Stages 1-5 give 1:1 parity and the whole game)

The original's faults, as the owner found them while patching the game (the readme's list), and what the engine
does today:

| | The original | The engine today | To do |
|---|---|---|---|
| 1 | The Sound Control sliders never write the registry | Each slider writes `WoWRevived.ini` as it moves (unmarked) | Keep; mark as a modern fix; Ambient and Speech, which only apply as the menu closes, to be saved then too |
| 2 | The music toggle does nothing | `[Sound\Volume] CD` is saved and read once when music starts | Make it stop and start the music at once; mark |
| 3 | Master doesn't reach the music | `ApplyVolumes` passes Master to the music (unmarked) | Keep; mark |
| 4 | The resolution slider only takes effect after a restart | `[Screen] Size` is saved and never used: the engine always draws 640 x 480 | Apply at once (see below) |
| 5 | 8 / 16-bit colour radios (8-bit doesn't work on modern systems) | Both radios are in Display Settings | Remove them, as WoWRevived does; the engine has one depth |
| 6 | "Lights" off crashes when Infiltrate makes its light | The toggle is in Display Settings; the engine doesn't crash, but the option is wrong to offer | Remove the toggle and force `[Options] "Show lights"` on at start-up, as WoWRevived does |
| 7 | (none) | (none) | On the first start (no `[Screen] Size` in the ini) take the desktop's resolution; after that the setting is the player's |

- [x] 6.1 (191) Mark 1 and 3 where they are (`FrontEnd::SoundControl`, `ApplyVolumes`), with what the original does
      (the menu's 0x4059A0 / 0x464490 path, traced so the comment can name where the write is missing).
- [x] 6.2 (191) The music toggle (2): `Music` started and stopped by the check box, and by the setting at start-up.
- [x] 6.3 (191) Display Settings without the colour radios (5) and the lights toggle (6); "Show lights" forced on when
      the settings load. FRONTEND.md's Display Settings row updated to say what the original has and what was
      taken out.
- [ ] 6.4 First-start resolution (7): the desktop's size written to `[Screen] Size` when it's absent.
- [ ] 6.5 The resolution applied (4). This is the large one, in three steps:
      a. the window and the back buffer take `[Screen] Size`, the 640 x 480 screens (menus, Briefing, research)
         centred or scaled, changed at once by the slider;
      b. the war map's and battle map's views drawn at the chosen size (the original supports 512 x 384 to
         1280 x 1024, 0x409AD0; the view rectangles, edge scrolling, minimap and panel anchors all take the size);
      c. the larger sizes WoWRevived added (to 1920 x 1080 and the monitor's own), with the launcher's per-resolution
         values (the Build List's place, the sound's inner and outer borders).
- [ ] 6.6 MODERN.md's "Modern fixes" section kept in step (it is written with this plan).

The modern options, after the fixes:

- [ ] 6.7 A "Modern Options" menu (where it hangs is to be decided), holding the `[Modern]` toggles already in
      (the war map status strip, sticky popup menus) and the ones to come.
- [ ] 6.8 MODERN.md's options: HUD scaling, windowed mode, wheel zoom, key bindings.
- [ ] 6.9 What WoWRevived fixed in the game's files (the readme's list: the text changes, the Cotton Factory's
      smokestacks, the Airship model's name, the hole in sector 22, the restored sprite pixels), offered from
      the owner's edited files.
- [ ] 6.10 Game modes, the editors, the players' ideas (MODERN.md).
- [ ] 6.11 Files (MODERN.md "Files"): every data folder overridable by loose files, modern formats for them, the
      owner's `dat` / `maps` fixes repacked by the installer, the war map background chosen by resolution.

## Tooling

- [ ] Unit tests for the decoders, the war map rules and the menus (TASKS "Tests"). They stay out of the update
      zips by the owner's convention; say if they're wanted in the repository.
- [ ] Release scripts tested from a clean checkout.
