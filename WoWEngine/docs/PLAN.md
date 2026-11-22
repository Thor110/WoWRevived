# Plan after update 136

## Where things stand (update 160) and what is next

Stage 1 is done. Stage 2 has 2.6 to 2.9 open. The owner's reference frames of the original (London, sector 28,
the Martian start) have been the best check so far: each one turned up things reading hadn't. The list below is
the working order; the stages further down stay the full inventory.

**Next, in this order**

- [x] N1 (147) The buildings' ground shadows for units: the shadow part (331) of a unit building and of each of
      its parts, as the scenery already had.
- [x] N2 (148) Objects the collision grid was missing: a building's parts and a bridge's segments, each with
      its own radius, for the deployment's check and the look ahead. Left: an explosion's victims (0x47FC50)
      still take a building as one object; the parts of scenery objects and of vehicles.
- [x] N3 (148, the mines) Ground flag 8: a built mine (0x273-0x276, 0x48CB10) marks the ground its own side
      won't cross. Left: the line-linked objects (0x48B690), which the engine doesn't have, and a mine that
      finishes building during a battle.
- [x] N4 Stage 2.6 (149, 150): the group panel top left; the name label on a unit gone under; the icons'
      cursor. Checked and not in the game: group keys, Shift adding to the selection. Left as provisional:
      the icon's lit state under the mouse and its flashing (what level 1 of its sprite looks like isn't known).
- [x] N5 (151) Objects under a light pool. Each part of a building is lit from its own point with its own
      radius, and the lit point is a quarter of the model's height up (0x41141C). The Command Post in the
      owner's sector 28 frame now measures as the original on every part checked (below).
- [x] N6 Stage 2.7, looked into in 151 and found already in or not in the game: the zoomer is in (0x446690,
      `ZoomTo`); the battle-end box is in (`EndBox`); "reinforcements arriving during a battle" has no routine
      of its own (the deployment, 0x4222C0, is only called as a battle is entered, 0x439770; what the engine
      has under that name is the retreat points, Reinforcements.cpp). Nothing left under 2.7.
- [x] N7 Stage 2.8 (152): the battle AI was already in but for one message, 0x80B1 to the AI player (a threat
      seen, 0x445D60), now ported with its sender (0x49668B).
- [ ] N8 Stage 2.9: the renderers still marked provisional (the WOF triangle filler, the blitter modes, the bm
      shade group's other slots). The owner's frames match closely, so this waits for a frame that shows a
      difference.
- [~] N9 Stage 3 (the war map's missing pieces). Done: 3.1 repair and the units' kept health (153); the
      counters' strength pips and the tool tips' task lines (154); the Resource view's tool tips, the popup
      asking the counter under the mouse, damaged facilities making nothing (155). Looked into and found in
      already, or not things the game does: 3.2's context buttons, Suspend / Resume, CommandCentreBuilt; the
      event boxes. Corrected in 156: the popup's Information item and the information page are real and are
      in now (the owner's frames). 157: the menu on a unit in transit as the owner's frame (Cancel move, no
      View map, the sector unselected, units in transit selectable). 158: the mini-map shaded by sector and
      the panel where the game puts it. 159 (the owner's Martian frame): placed counters 16 up and left, the
      view's start on the selected sector, the view button's held frame. Left: 3.3's
      hourly fan-out, 3.4 (several units as one route), 3.6 and 3.7, each to be checked against the code first:
      these lists have turned out stale more often than not.

**Open questions to keep track of** (each has its notes in PARITY.md; don't re-trace what is ruled out there)

- **How deployed units end up facing** (updates 145, 146). Measured in the owner's Martian frame: the two units
  that stand alone both face about 3400 of 4096, the engine has them near 0. The owner: units do sometimes go
  on turning after a level loads, not always; it may be the computer player's doing. Watch for it while doing
  2.8 (the battle AI) and anything that sets a stopped unit going (0x808B, 0x80C2 at a battle's start).
- **London's deployment spot** (updates 143, 144). The original puts the lorries at ring 95's 288 degree spot,
  the engine at its 72 degree one. N2 (148) didn't settle it: with the parts and segments in the grid the engine
  still takes the 72 degree spot.
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
- [ ] 3.2 Construction's remaining fields, CommandCentreBuilt, the context buttons (factory panel, Suspend /
      Resume, Retreat).
- [ ] 3.3 The hourly step's remaining fan-out; damaged facilities and work demand in the economy.
- [ ] 3.4 Several units sent as one route; the world's 0x802C when no routes are left.
- [ ] 3.5 The popup as child windows; the sector's and objects' remaining items; the remaining tool tips.
- [ ] 3.6 The battle report's own counters in place of the auto-resolve's.
- [ ] 3.7 Commands one queue pass later (0x95) where the order matters.

## Stage 4: saving a battle, and the rest of the original

- [ ] 4.0 New war map objects made as the game makes them (0x43CD10 / 0x42A030), not copied from a saved one of
      the type (was 1.12: it needs the whole object written field by field, which 4.1 brings).
- [ ] 4.1 Battle saving and loading: each object's BAMO / BATP and class tags written and read (today carried as
      blobs), the battle's own state, the sector's battle and pod flags.
- [ ] 4.2 The players: local / AI / network player objects and the command records (0x4460A0); playback
      recordings.
- [ ] 4.3 The network game: the menus' lists, the lockstep packets. The retail network code doesn't work
      (WoWRevived ships a different build for it), so what "parity" means here is the owner's call.
- [ ] 4.4 Speech attached to text ids; the `[Sound]` settings; animated cursors and the busy icon; the remaining
      settings of the ~229; the debug log.

## Stage 5: combat seen running

Not a block of work but a standing need: neither test battle has an enemy in range, and combat only comes by
playing or waiting until the sides meet.

- [ ] 5.1 A headless script that runs a campaign's clock on the war map until a battle is announced, opens it and
      saves the screenshots and the log; kept as a recipe in HANDOFF.md.
- [ ] 5.2 With it, the items only checked by reading: quiet vanishing kills, damage rounding, the firer's sparing,
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

- [ ] 6.1 Mark 1 and 3 where they are (`FrontEnd::SoundControl`, `ApplyVolumes`), with what the original does
      (the menu's 0x4059A0 / 0x464490 path, traced so the comment can name where the write is missing).
- [ ] 6.2 The music toggle (2): `Music` started and stopped by the check box, and by the setting at start-up.
- [ ] 6.3 Display Settings without the colour radios (5) and the lights toggle (6); "Show lights" forced on when
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
