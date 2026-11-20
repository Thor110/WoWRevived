# WoWEngine task list

Built from a survey of the whole game executable (0x401000-0x4A8A92) in 14 pieces; the detailed reports, with every
function, structure, message and setting, are in [`docs/survey/`](survey/) (`partNN.md`; section 6 of each lists the
tasks for that piece with the addresses needed). The aim is a 1:1 match with the original first.

Everything still to do is below, grouped into milestones in the order they depend on each other: each milestone
needs the ones before it, and within one the tasks are roughly in the order to do them. `[~]` marks a task that is
partly done (what's left is said). The part number after each task points to the survey report. What's finished is
listed at the end.

## Milestone 1: a campaign you can save, load and carry on (war map, no battles yet)

The war map runs from a save but nothing is kept: these make it a real game state.

- [x] **Saves read and written exactly** (`core/SaveFile`): the war map's chunks field by field, the rest carried
      as it was; Human.000 and Martian.000 round-trip byte for byte (`wowdump --resave`) — 01, 03, 04, 08
- [~] **WMOB object model**: the war map keeps every saved object (its record, battle map units and all) and the
      counters point at them; units added on the map (pods) get a copy of a saved unit of their type with new ids,
      the lowest free (object ids, 32768). Still to come: the objects' own fields (hit points, orders, construction
      progress) read and used, which the systems that use them (Milestone 2) will bring (0x428BB0, 0x42A190) — 04, 14
- [x] **Save the war map**: WMAP, CALN / DtTm, MPAD, routes (units on their way), the pods, owners and every object
      in its sector, in 0x41AD80's order — 03, 04, 07
- [x] **New campaign in its save slot**: the start file under the campaign's name in `SaveGame\<side>.00N`
      (0x404660), played from there — 01
- [x] **Saving and loading**: Return to Main Menu and Save & Exit save to the slot (0x80B4 → 0x4042D0); a loaded
      slot plays with "Game Loaded" (0x465) in the message list — 01, 02, 03
- [x] **War map menu**: the original's items (Resume, Return to Main Menu, Critical event options, Sound Control,
      Save & Exit), the war map paused while it's open (0x49B980, 0x41CD50) — 03, 13
- [x] **"Skip" setting**: `[Main] Skip=1` goes straight to Start New Campaign (0x402A30) — 01
- [ ] **Route progress**: a unit on its way is saved with the route but sent on again from where it set out; the
      route's progress (+0x34) and the units' places along it, as the game keeps them (0x425080, 0x4257C0) — 04
- [~] **Sector flags kept**: the battle (+0x124) and pod (+0x64) flags, which live in the sector's part of the save
      the engine carries unchanged (0x43C5E0); the economy is written — 03, 04
- [ ] **Commands one queue pass later** (0x95) where the order matters (comes with the research screen, battles and
      the debriefing it defers) — 01

## Milestone 2: the war map plays like the original

- [x] **Economy**: the five phases, one an update, all five at the start (0x41A760); supply from finished
      facilities, demand from unit upkeep, spread by the sector paths, the ratios; saved with each sector — 03, 04
- [ ] **Hourly step**: the rest of 0x802D's fan-out (routes, players, both sides' research); and the economy's
      inputs still to come: damaged facilities (hp / maxHP), the work demand while repairing or building — 03, 04
- [x] **View shading**: the Construction view from the economy (table 1, levels 67-77) (0x4192F0) — 03
- [x] **Resource warnings / alerts**: thresholds 0.3 / 0.8, counters 336 / 504, events 13 / 14 with their sounds
      (0x4198C0 at 0x419EFB); the event box for critical events comes with Milestone 3 — 03
- [x] **Travel as the game does it**: formation offsets, per-hour movement along segment lengths, attacks arriving
      together (0x41FCF0), the arrow trimmed behind the units, TravelInterrupted, sea waypoints (0x425080, 0x42AFE0,
      0x4373A0); still to come: several units sent as one route, the world's 0x802C when no routes are left — 03, 04
- [x] **A\* details**: the cost as its own estimate, newest-first ties, no re-sort on improvement, the neighbour
      order, the 1000 limit (0x44EE70 / 0x44EFA0 / 0x44F120); the goal's limit counts routes heading there — 08
- [x] **Sector paths cache** (WM_COMMAND 0xB7, WSectorPath 0x4A39E0) for the economy; the AI will use it too — 14
- [x] **Counter cells**: the sector grids with their spiral place kept, shrinking by "WM clunting adj" (4) from
      "WM clunting res" (36) when a sector is full with the counters sliding at 5 pixels a tick, mode 3 / mode 2
      (with its row test), re-laid out on a view change and on arrival (0x4205F0, 0x4208B0, 0x4209E0, 0x43CB60) — 03
- [~] **Counter decorations** (0x42B850): done: the class-7 counts ("%d" / "%d/%d") at the bottom right, the
      picked counter drawn on top, own unfinished buildings at 50%; with their systems: the power bar, production
      blink, idle and repair icons, old enemy intel at 50% — 04
- [x] **Mouse**: rubber-band selection (sectors 0x8010 / 0x8011), the sector selection rules, the hover route
      preview with cursor mode 2 and the click that sends (0x41D300-0x41D590, 0x43C3F0), the quick cancel, the
      right-hold "Warmap menu" with Cancel (0x41D120); the rubber band's look isn't traced — 03
- [x] **Selection**: click, Shift-click, box select, the cursors (0x42BEB0, 0x42C160, 0x43ADA0 0x20) — 04
- [~] **Right-click popup menu** widget (0x432C60): placement, sounds 0x193 / 0x196, closing, commands; to do: rows
      of several items opening out on hover, the flip at the right edge, the hot button's own look, and the
      sector's and objects' items (0x805C: Build, Resume building tasks, Cancel Move, Information) — 05
- [x] **Keyboard / time running**: Backspace, map clicks and speed 0 turning time off and stopping the clock, the
      digits turning it on, the two sides' votes (world +0x64 / +0x68, 0x8050-0x8055, 0x41A6C0, 0x41F640); the clock
      starts stopped; the command records themselves come with the players — 03
- [x] **Calendar**: flashing while one side waits (calendar +0x38, 0x4271E0), a sector with both sides in it refusing
      a speed change (0x41AEA0 → 0x43D010) — 03, 04
- [~] **Tool tips**: done: units (0x428D70) and constructions (0x42A4B0) with their levels; to do: the task lines
      (with construction), resource facilities (0x426430 / 0x426AE0), the owner position query 0x80D0 — 04, 05
- [ ] **Repair** (MRV / workshops) and **construction** progress and completion, CommandCentreBuilt (0x4297C0,
      0x42A4B0) — 04
- [ ] **Context buttons**: factory panel, Suspend / Resume, Retreat (0x429A70) — 04
- [x] **Intel ageing and war map fog**: the other side's objects shown only with intel, ageing every hour, 50% under
      168 h, the post-battle reveal of the sector and 1-3 neighbours by research (0x421630, 0x429B80, 0x418E17;
      the reveal waits for battles, the research count for research, the network case for the network) — 03, 04
- [~] **Research button**: waits for the research data (Milestone 4); its tool tip already says nothing is being
      researched, as at the start
- [ ] **Players**: local / AI / network player objects and the command recording `{01,01,code,04}` (0x4460A0) for
      playback and the network — 03, 07

## Milestone 3: events, battles announced, the game won and lost

- [x] **InfoBox dialog** (0x432520): the critical-event box with its button, both sides' layouts — 05, 03
- [~] **Critical events**: the queue (0x41E5B0 / 0x41E730, command 0x8122), the tests and `[Event]` options, the
      handlers for events 0, 3, 11, 12, 13, 14, 17, 18, 19; the rest come with research and production — 03
- [x] **Battle detection** (0x41AB00): both sides in a sector → BattleInSector, UndefendedSectorLost (0x43C8D0,
      0x43A250) or PodLanded, the pending battle; sectors held by presence (0x43CA40) — 03
- [x] **Briefing**: the transition (0x41B210: sound 0x18E, the Insert CD check) and the screen (0x499930 /
      0x4999E0): VIEWING / ATTACKING / DEFENDING, the date, Forces Present, the mini battle map (`maps\hmin%02d.spr`),
      key or click → 0x98, `[Briefing] Delay` 30 → 0x71; palette group hb (0x40BE05) — 03, 13
- [~] **Auto-resolve** of a battle (a stand-in until the battle map exists): more men wins, the loser's objects go,
      the result announced (0x41AED0), the Debriefing's losses; to do: the battle report's own counters (0x421210,
      0x4212F0) once the battle map fills them — 03
- [x] **Debriefing** (0x49A7B0 / 0x49A820): VICTORY / DEFEAT / RETURNING TO WAR ROOMS, sounds 0x18F / 0x190 /
      0x18E, Forces lost and Enemy forces destroyed, `[Debriefing] Delay` 300 → 0x8017, and the way back to the war
      map (0x419AA2: sounds, music after 30 ticks, the announcement and the queued events) — 13, 03
- [x] **Viewing a sector** (0x805C "View map", 0x8133 / 0x8019 from the sector's own button in the Warmap menu,
      0x41D120 / 0x43B9F0): the Briefing's VIEWING case and the Debriefing's RETURNING TO WAR ROOMS without a
      battle — 06, 03
- [x] **End of game**: events 18 / 19, codes 0x80FA-0x80FD sent as 0x80FE (0x41AED0, 0x4212A0) — 03
- [x] **Game over flow**: the VICTORY / DEFEAT movie for the code, then the credits, then the main menu (0x402B80)
      — 01

## Milestone 4: research, production and the opponent

- [ ] **Research data**: the hard-coded trees (6 Human plans, 78 items and 8 triggers; 6 Martian, 84 and 6), states,
      obsolescence, triggers, the 17 `[Tweak]` multipliers, `[Research] Open Rate` (0x457F20-0x45BCB0) — 08, 09
- [ ] **Research screen**: the tree, the detail panel, scientist speech, opened by R and the Research button
      (0x45E670, 0x45D3F0) — 09
- [ ] **Durations text**: day / days / hr / hrs (0x41BDA0) and the Human / Martian date formats (0x41B440) — 03
- [ ] **What can be built**: the command building rules (0x45F970) — 09
- [ ] **Build list / production**: 32-item queue, time ×24 h, resource slowdown, repeats, events 1 / 2 / 9 / 10
      (0x45F770-0x462EE0) — 09
- [ ] **Unit table**: all 65 OBJ.ojd record types and the `[Tweak]` armour / research / manufacture modifiers
      (0x437750) — 06
- [ ] **War map AI**: the 10-state machine every game hour, its 14 `[Tweak]` values, AI.ojd's sites (0x442890,
      0x443340-) — 06, 07

## Milestone 5: the battle map

- [ ] **Terrain**: fix `core/Models` (height scale from CLS +0x2C, heights × √2) — 10
- [ ] **Landscape**: lighting, shadows, placement rules, scorch marks (0x47AF10-); shade groups hb / hr / bm and the
      time-of-day lighting (0x40B6C0, 0x40B030; fix the ShadeGroups.h comment) — 01, 10
- [ ] **Renderers**: the blitter modes, triangle spans, WOFLib, RTypeISO, the terrain view (0x44F890-0x457F14,
      0x49DA50) — 08, 13
- [ ] **Battle map screen**: keys, mouse, the minimap, fog of war, the battle map menu 0x80B8 (0x49BD50: heading
      0x3FD, Resume, Sound Control) (0x484A30, 0x4346C0) — 05, 11, 13, 14
- [ ] **Enter / leave a sector**, the battle-end dialogs, the briefing sound stopped (0x439770, 0x439ED0, 0x41B360)
      — 03, 06
- [ ] **Deployment**: ring placement, HQ and entry-edge centres, random headings, the 0x80C3 rally (0x421FE0,
      0x4222C0) — 03
- [ ] **Objects**: the factory map, units (WPuppet), vehicles with steering (`[Vehicles]`, 23 settings), tripods,
      fliers, boats, buildings, bridges, fences, mines, pods (0x446B30, 0x48A4C0-, 0x4956D0-) — 07, 12, 13
- [ ] **Unit groups, orders, stances, specials** (Black Dust ... Scan) and the order panel (0x480190, 0x47CCF0-;
      the 0x80A0 stagger 0x47CEE0) — 07, 10, 11
- [ ] **Weapons and effects**: shots, explosions, gas, area effects; smoke and particle emitters (0x4949A0,
      0x47F050) — 10, 11, 12
- [ ] **Battle AI** (0x445010) — 07
- [~] **Battle map music**: CD tracks 4 / 5 (the war map's is done) — 11
- [ ] **Positional sound**: listener at the view centre, inner / outer rects, pan 4·dx (0x464FF0) — 09

## Milestone 6: the rest of the original

- [ ] **Cheats**: youlikeit, shym, datsun, curtains, icomeback, punyhumans, atchooo (englishrose is in) and their
      flags (0x401000, 0x401320; `byte_4D1249` / `4D1261` / `4D1279`, `byte_58913C`) — 01, 03
- [ ] **Speech queue**: opcode-6 sounds one at a time, no duplicates (0x4652B0) — 09
- [ ] **Virtual voices** that resume at the right place when they get a voice back (0x4654E0) — 09
- [ ] **Speech attached to text ids** (0x414700) — 02
- [ ] **`[Sound]` settings**: Minimum Audible Volume, Search Libraries First, Unload Limit; the library-before-loose-
      file order — 09, 10
- [ ] **CD player seek** ±5 s (transport 4 / 6) (0x4081D0, 0x412660-) — 01, 02
- [ ] **Animated cursors** (the wait cursor) and the busy icon while loading (0x40CDC0, 0x44E4B0) — 01, 08
- [ ] **Display Settings** as the original: the resolution list (512×384 to 1280×1024, only available modes) and the
      colour depth (0x409AD0); the engine still draws at 640 × 480 — 01
- [ ] **The settings**: the ~229 registry / INI settings with their defaults, all read (list in part 14) — 14
- [ ] **Network game**: the network menus (0xA0 / 0xA4), the lockstep order packets and player command queue
      (0x438C90, 0x4460A0), "Messages Guaranteed" — 01, 06, 07
- [ ] **Playback recordings** (`playback.%d`, 0x42D290) — 04, 10
- [ ] **Debug log** (`[Debug] "Log Debug Output"`, 0x417C30); optional, WoWEngine.log covers it for now — 03

## Engine and tooling

- [ ] **Tests**: unit tests for the decoders (save, SMK, NV12, OBJ.ojd records), the war map rules (routes, pods with
      srand(1), shading) and the menus' input, run in CI
- [ ] **Release scripts**: `make_release.bat` and `install-wowrevived.bat` tested from a clean checkout; the
      WoWRevived version in the default movie path taken from the folder instead of fixed at v1.1.2
- [ ] **.mp4 movies**: scale large movies in the decoder (Media Foundation's advanced video processing) if a slow
      machine can't keep up; drop the per-piece sound logging once it has been confirmed on more files
- [ ] **Docs**: keep FRONTEND.md, WARMAP.md and this list up to date with each update

## Done

- Front end: legal splash screens; fades and the 5-tick command delay; menu glow; button frames; movies (Smacker and
  WoWRevived's .mp4, with the sound on its own reader, NV12 decoding and the formats taken after the first sample);
  CD player pause / play; options sliders' formula; the text field; Shift+Tab and right-button push buttons; tool
  tips (manager, BoxedText tip, front end, war map panel and calendar); Sound Control's live preview; pause on focus
  loss and the Pause key; WoWEngine.log.
- Sound: stereo samples; the volume curve, voice limits and stealing, ducking, variations in turn; the war map's
  music and clock sounds.
- War map: map, borders, ownership, panel, calendar (month names, odometer, "Advance time"), slider; routes with the
  game's A\* rules; arrows (curve, head, styles) and arrival labels; the message list and history; the original keys
  (but Backspace); quick right-click cancel; view shading (Unit and Resource views) and pulsing battle sectors;
  Martian pods; border pixels; the Research button (placeholder screen).
- Data and platform: the game's random numbers (MSVC rand(), srand(1), the 1024-value table); sector deposits from
  OBJ.ojd.
