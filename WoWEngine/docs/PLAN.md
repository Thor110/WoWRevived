# Plan after update 136

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

## Stage 1: the open parity items (next)

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
      (scenery on scorched ground) and the grid 0x47FE10 searches (the scenery counts). Left: a building's parts in
      the grid; the bridges and the pod in a building's scenery clearing;
      the order of the rand() calls inside one start; the start order kept for the update order (the world
      list is HUNI, HCON, HRES, AUNI, ACON, ARES).
- [ ] 1.3 Tree circles hit by weapons (S6's last open point).
- [x] 1.4 Muster's first step and the start order of 0x495740 (movement 20).
- [x] 1.5 The fog's timing by the global frame counter, dword_4D2558 (battle-setup 12).
- [ ] 1.6 The end-of-battle counts' lists (battle-setup 18).
- [ ] 1.7 The second target, +0xC8 / +0xCC (movement 19).
- [ ] 1.8 The window's 32-pixel scroll border and the game's windowed flag, dword_4EA364 (battle-ui 9); the
      resolution itself is Stage 6.
- [ ] 1.9 Front end: what 0x80CF answers per control; the CD Player's live time and starting track, and its
      +/- 5 s seek; Debriefing's delay from its second draw.
- [ ] 1.10 The war map battle pulse's own draw counter (warmap 20).
- [x] 1.11 The latent data items (data 6, 8, 13).
- [ ] 1.12 New war map objects made as the game makes them (0x43CD10 / 0x42A030), not copied from a saved one of
      the type.

## Stage 2: the battle map's missing pieces

- [ ] 2.1 The route finder as a stateful object, its search spread over updates, a second group's held off 10
      updates (movement A3, 17).
- [ ] 2.2 Bridges as objects that break; broken links; the ground under them redone (movement A5, 21).
- [ ] 2.3 Footprints taken off and route nodes removed as objects go; crushing.
- [ ] 2.4 The walker subclass: the leg animation's rate, footprints (0x48FDD0, 0x490070).
- [ ] 2.5 Fliers and boats checked against their classes; the units' headlights; the scenery's and bridges' object
      scale.
- [ ] 2.6 The order panel (0x484A30), the group keys, the label 0x6F, the combined panel (0x448500), the cursor's
      vt+0x6C; cursor modes 2-7 where the game (not the editor) uses them.
- [ ] 2.7 Reinforcements arriving during a battle; the zoomer, the battle info and the battle-end dialogs.
- [ ] 2.8 The rest of the computer player in battle.
- [ ] 2.9 Renderers still provisional: the WOF triangle filler, the blitter modes, the rest of the bm shade
      group's slots.
- [ ] 2.10 Battle music (CD tracks 4 / 5).

## Stage 3: the war map's missing pieces

- [ ] 3.1 Repair (Mobile Repair Vehicles, workshops), with the repair icon and the power bar (warmap 2).
- [ ] 3.2 Construction's remaining fields, CommandCentreBuilt, the context buttons (factory panel, Suspend /
      Resume, Retreat).
- [ ] 3.3 The hourly step's remaining fan-out; damaged facilities and work demand in the economy.
- [ ] 3.4 Several units sent as one route; the world's 0x802C when no routes are left.
- [ ] 3.5 The popup as child windows; the sector's and objects' remaining items; the remaining tool tips.
- [ ] 3.6 The battle report's own counters in place of the auto-resolve's.
- [ ] 3.7 Commands one queue pass later (0x95) where the order matters.

## Stage 4: saving a battle, and the rest of the original

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

## Tooling

- [ ] Unit tests for the decoders, the war map rules and the menus (TASKS "Tests"). They stay out of the update
      zips by the owner's convention; say if they're wanted in the repository.
- [ ] Release scripts tested from a clean checkout.
