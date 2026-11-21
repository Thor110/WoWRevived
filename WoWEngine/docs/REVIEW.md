# Routine review and task list

A review of every routine in the original executable (2,790 in the IDA listing, 0x401000-0x4A4470) against the
engine, made by eight reviewers each taking an address range. Each routine was classed as **ported** (and spot-
checked), **partial** (part done, provisional, or diverging), **missing** (game logic or visible behaviour not in
the engine) or **library** (C runtime, MFC, DirectX plumbing, codecs, destructors and the like, replaced by SDL or
not needed). The full reports, with every address and the port's file and line for each finding, are in
`docs/review/range-*.md`; this page is the task list drawn from them, to be worked through top to bottom.

| Range | Routines | Ported | Partial | Missing | Library |
|---|---|---|---|---|---|
| 0x401000-0x40FFFF: WinMain, front end, shade tables, renderer | 307 | 100 | 31 | 14 | 162 |
| 0x410000-0x41FFFF: ISOLib, CD player, text, war map world | 294 | 116 | 16 | 9 | 153 |
| 0x420000-0x42FFFF: war map objects, routes, deployment, saves | 327 | 141 | 34 | 23 | 129 |
| 0x430000-0x43FFFF: UI controls, fog, battle world, sectors | 256 | 107 | 17 | 41 (22 debug) | 91 |
| 0x440000-0x44FFFF: AI players, factory, panels, network | 297 | 78 | 20 | 47 | 150 (+2 unclear) |
| 0x450000-0x47CFFF: spans, research, build lists, sound, CRT | 475 | 227 | 45 | 11 | 192 |
| 0x47D000-0x48FFFF: battle objects, groups, effects, classes | 431 | 118 | 28 | 102 | 183 |
| 0x490000-0x4A4470: vehicles, routes, bridges, renderers, UI | 403 | 187 | 24 | 70 | 122 |
| **Total** | **2,790** | **1,074** | **215** | **317** | **1,182** |

Checked and found matching (no task): the war map AI's phases and constants, the economy's five phases, pods,
resource warnings, battle detection, the event queue, the route hourly step and curves, the A* rules, counter
placement, the calendar, the end-game codes, the guns' impact tables, the explosion splash, the damage smoke,
turret turning and arcs, the ground and slope costs, the vehicles' speed and turn constants, LineCost, the bridge
segment count, the WOF scale chain, reinforcement points, entry points and the `[Tweak]` defaults.

## 1. Bugs (the engine contradicts the original)

Each is small; fix these first. Ticked: fixed (the update says which). (The address is where the original shows it; the report has the port's line.)

- [x] **Human command building outside London is 58, not 314** (0x422082, 0x42A9C9, 0x439B94: `and al,24h; add
      eax,116h` wraps). `core/Deployment.h` HumanHq; deployment and the start view never find a Command Post. Fix
      the docs too (BATTLEMAP.md).
- [x] **The battle ends too early**: each side's count adds its construction list (0x43AA59 / 0x43AAF6, sector +0x54
      / +0x58); the engine counts units only (`BattleMap.cpp` battle end).
- [x] **The battle AI's threats** are its own constructions and resources, lists 3 and 5 (0x445210:
      0x446370(2,0) / (3,0)); the engine takes lists 2 and 3 (`BattleAi.cpp` AiStart; BATTLEMAP.md too).
- [x] **AI units get stance 2 (Attack)** as they're registered (0x445500, `push 2`).
- [x] **AI attackers go back to the pool** when their target dies (task destructor 0x4451A0 -> 0x445500); the
      engine loses them for the rest of the battle.
- [x] **Groups ignore hits while moving under an order** (0x483880: bit 0 set and bit 2 clear; every move order sets
      bit 0, only 0x483B70's own moves set bit 2). `Retaliate` / `OrderMove`.
- [x] **Click-select narrows the selection**: 0x497680 always clears (packet 4) before reselecting; the engine keeps
      every group when one already selected is clicked. Shift (packet 5) adds.
- [x] **Speed control's going test** is vt+0x144 (0x4909D0: not stopped, or bits 5 / 8 or the link byte, and not
      frozen), not bit 0 alone (0x492E8E; `Driving.cpp`).
- [x] **Flat-shaded WOF faces**: the flat level comes from vertex 0 as passed, before the sort (0x456DF5);
      `WofView.cpp` takes the top vertex after sorting.
- [x] **The computer's research choice** is the side's plan picker, 0x45A400 / 0x45B4E0 (two hard-coded plans by
      rand(), falling back to 0x458A60), not 0x458A60 directly; it also shifts the rand() stream (RESEARCH.md too).
- [x] **Enemy sector shade in the Construction view** (shade and pulse update 102, borders 103) is 11, not 0 (0x41DAB6-0x41DAEF); only the Unit view darkens
      it. Battle sectors pulse only in the Unit view (0x41DD35). Borders count a sector without a finished command
      building as no one's (0x41DC1A, 0x43C990).
- [x] **Slider frames truncate** (0x4320B6 `div`): `value * 10 / max`, not rounded (`Screen.cpp`).
- [x] **Word wrap** breaks at width or more, resets the run to 0, keeps runs of spaces and sums per-character widths
      (0x416250; `Canvas.cpp`).
- [x] **CD Next / Prev wrap round and stop play** (0x413220 / 0x4134B0, caller 0x4083E4); the engine clamps 2-5.
- [x] **The war map message list** draws at `[Colours] "Message Text"` 19 with flags 1 (0x4288A3); the engine uses 0.
- [x] **The CD Player's palette group 0x2F9** also loads `{S}R{S}I` into slot 5 and `{S}RGI` into slot 9
      (0x40BCB3, 0x40C54F), and the loader frees all 33 slots first (0x40B6C0).
- [x] **Credits** (update 102, the exit's fade 103): the easter egg holds 300 ticks, not 600 (0x40577C); waits for the music first; the music fades
      over 60 (0x405415); the exit fades (0x405689 -> 0x4065E0).
- [x] **Escape in the front-end menus fades** like a click (0x40A1B0 -> 0x4065E0); Splash and Credits take a right
      click as a skip (0x405E6E, 0x405672).
- [ ] **The route save format** (probable; check with a save holding a unit in transit): +0x34 is a u32 entry count,
      the entries follow untagged, each with segment, index, five floats and a byte (0x425A60 / 0x427F70 /
      0x42B250); `SaveFile.cpp` reads a float and a WMOB-tagged list.
- [x] **Group stance and think interval** (update 104: the interval is averaged; the stance, traced, is left at 1: the
      groups are made before deployment starts the members, and the pool allocator doesn't clear, so the game averages
      leftovers) are the members' averages (0x4802E0: stance ftol(mean + 0.5), interval
      the mean of type +0x92), not 1 and 90. *Trace the call order first* (whether +0xD8 is set when it runs).
- [ ] **Object ids** start at 0, lowest free first, freed ones reused last in first out (0x4A3350); the engine counts
      up from 1. (They appear in saves and order packets.)
- [ ] **WOF pieces of equal depth**: the game sorts with `qsort` (unstable, 0x49ED32); the engine's stable sort can
      order them differently.
- [ ] Debug only: the reinforcement table with `byte_4D1279` (0x43D16D).
- [x] Docs (update 104): `docs/survey/part11.md` §2.9 (0x487F50's tables), part08 §2.6 (the 0xFBEF mask), part06 (+0x55 reading),
      part03 (0x41EDC0's view choice). (The stale *Provisional* at `Driving.cpp:44` is fixed, update 103.)

## 2. Game rules missing (they change how a game plays)

- [x] **Destroying the key building** (update 104; the victory delay's sound length still 150 assumed) ends the battle and the game (0x48AC50: 0x80AA to the owner; kinds 3 / 0x1E
      set `byte_4E9B94` and send 0x8136 "DESTROYING VEHICLES"); the war map's forfeit (0x41AED0 / 0x41F4B0: event 19
      with sector 30 / 3, codes 0x80FB / 0x80FD; the victory delay is sound 0x13E's length + 30).
- [x] **Leaving a battle** (0x439ED0): a holder that lost its command building has its units' factory tasks cancelled
      (0x813D -> 0x428F75 -> 0x460140 BuildList::CancelAll); every sound fades over 30. *Update 105.*
- [x] **Research multipliers in damage** (0x4958B0: armour and weapon by weapon type and target side; 0x494AF0
      upgrading shots). *Update 105.*
- [x] **Research triggers**: the battle map marks the unit table's +0xAC "encountered" (0x47F630, 0x488C00,
      0x492931) so `UpdateTriggers` sees it. *Update 105* (the crushing's mark waits on the crushing). The other
      side's research-done message (0xB9) reaching the AI goes with the next item.
- [x] **The AI's building upgrades** (0x442D80: retypes its buildings when research finishes a mark). *Update 105*
      (with the other side's 0xB9 reaching it).
- [x] **The war map economy's "working" demand** (0x429640: record 47/48 while a factory works or repairs).
      *Update 106* (the repairing part comes with repair).
- [ ] **Repair** on the war map (0x422AB0, 0x4297C0, 0x4299C0, 0x429B80's repair part). *Needs first*: each
      battle unit's hit points kept between the maps (the battle map starts every unit at its type's +0x4A and
      nothing is written back to the saved BMOL), so there's nothing damaged on the war map to repair yet.
- [x] **Retreat on the war map** (0x424F50 0x80A9, the +0x44 stop-at-next-node flag, the Retreat item in 0x429A70).
      *Update 106.*
- [~] **Routes keep their progress through a save** (0x4217E0, 0x4257C0; with the save format above); counters
      slide to the route start at `Clunting Speed` (0x428020 / 0x425080) rather than jump, and Clunting Speed isn't
      a travel speed. *Update 106*: the slide and Clunting Speed done; the save part waits on the route format.
- [~] **Staggered deaths** of a whole list (in for the key building's loss, update 104; Abandon goes at once as the game's 0x8067 does) (0x47CEE0: 30, then +30 + rand() % 30 each).
- [x] **Damage Layer**: scorch kept per sector in the save (DMGL), fading 2 a byte each battle entry (0x404EA0-
      0x4051B0, 0x439892), cleared by `shym`; needs the FFUH encoder (0x415520 / 0x4157E0 / 0x415720). *Update 107*
      (the `shym` clear goes with the cheats).
- [x] **Fog of war** (0x4344C0-0x434DF0: the `[BattleMap]` keys, two byte maps, 19 stamps, Reveal from units every
      update (0x4964B0), Decay every "Fog update freq", Flush; objects' visibility and shading 0x48B050 / 0x48B2D0 /
      0x48C570 / 0x48C840 / 0x47FEB0), feeding `TerrainView`'s fog input. *Update 107* (objects' brightness,
      0x8126 and the minimap's dots still to do; 0x47FEB0 not looked at).
- [~] **Deployment's remaining parts** (0x4222C0: the fixed-place bit, 0x80C3 to each unit, the pod centre).
      *Update 106*: 0x80C3 in; the fixed-place bit and +0x18C wait on the units' saved battle state.

## 3. Battle map objects and vehicles missing

- [x] **Buildings** (0x48E8A0-0x48F1C0: ground flattening, corner objects, ambient loop, damage frames, hit sound
      0x111) and **gun emplacements** (0x48F7C0: no guns on a damaged building); turret attach (0x489980).
      *Update 110*: the ambient loop and the hit sound 0x111.
      *Update 120*: the buildings' IOB parts drawn (the Factory had no roofs or chimneys), standing on the
      building's ground, and lost one a hit from the last as the health falls (0x48EF80 / 0x48A7D0, the "damage
      frames"). There's no ground flattening in 0x48E9D0: it only sets the parts' heights from the building's ground.
      The corner objects are for a building under construction, so they wait on construction placement.
      *Update 121*: a building being built (its war map object's +0x7C / +0x78 at most 0.9999) starts with that share
      of its health and, the player's, its four 0x17B corner markers (`bld_mark.spr`); an emplacement under 1.0 has
      no guns (0x48F7C0). The emplacements (0x32-0x39, 0x6F-0x76) aren't the building class any more (no ambient
      loop, no lost parts), as in the game. Left: the objects nearby re-seated (vt+0x78).
- [x] **Walkers / tripods** (0x48FC70-0x48FDD0, 0x490030-0x490070: legs, footprints, `startTurnRate` / `turnAnimRate`).
      (Reported in play: units' animations don't play; the models' pieces aren't animated yet. Comes with this.)
      *Update 113*: the WOF animation keys (every animated model, not just the walkers), the frame advance (0x493490
      by distance, the walkers' 0x490070 by turn), and the walkers' feet hidden and splashing in water (0x48FCB0).
      The walker class is types 0x3E-0x4C; 0x19-0x1B / 0x4D-0x4F are the fliers.
- [x] **Fliers** (0x490120-0x490170: height keeping and bob); wobble and dust / wake trail 0x3C8 (0x493490 /
      0x4934F0). *Update 113*: their frame advance (0x493490) is in, with every vehicle's. *Update 114*: the height
      keeping (0x490170, which is all of the "bob": a damped climb to 256 over the ground) and the wake 0x3C8 that
      every vehicle class but the walkers' leaves on water (0x4934F0). There's no separate wobble in the code.
- [x] **Ironclads and Submersibles' own steering** (0x493870-0x493B50, 0x493A40 arrival); the Tunnelling Track Layer's
      research speed and mound (0x493D00 / 0x493D70). *Update 114*: braking x2, avoidance x0.25, separation x2,
      the lookahead push / 1 instead of / 8; the Ironclads' damage off water (0x80C1); the Track Layer's underground
      speed and its mound marker 0x35A. (Their arrival is the same as every vehicle's.) Tested only for crashes:
      none of the battles at hand has a boat or a Track Layer.
- [x] **Stop / start footprints** (0x4915B0 / 0x491630) and **the collision filter** (0x4929A0: walkers ignore small
      crushable objects, bridges ignored on a link); crushing. *Update 109*: the footprints. *Update 115*: the filter
      and crushing. The crushable objects are the mines (and the line-linked objects 0x1D0-0x1D3 / 0x2B6 / 0x2B7,
      which the engine doesn't make yet); a vehicle on one sets off the mine's explosion 1 time in 4, which is how
      mines go off. Move classes 3 and 4 pass over small ones. The bridge-link part waits on the bridge pieces being
      objects.
- [x] **Drones** exploding on contact (0x48F9D0-0x48FAC0). *Update 108.*
- [x] **Pods** (0x48F5F0 / 0x48F620). *Update 115*: the battle's pod after a landing among the player's sectors,
      landed at random with its two blasts.
- [ ] **Fences and walls** (0x48D780-0x48DB80, WFence 0x4987F0-0x498D30) with route blocking and node removal
      (0x494370); line-linked objects (0x48B590-0x48C650); two-point structure build-up (0x4865A0 / 0x4866E0).
      *Looked at in update 116*: a fence (0x1C3-0x1C8, WFence on 0x498750) keeps its two end points at +0xE8 / +0xEC
      and +0xF0 / +0xF4, snapped to the vertex grid (0x498900); its battle record carries them (vt+0x08 0x4987F0
      reads four words and sends itself 0x80A5 / 0x80A6; vt+0x0C 0x498890 writes them). The engine carries a battle
      object's record as bytes and doesn't make fence objects, nor place a new construction in battle, and no save
      at hand has a fence: this waits on construction placement.
- [x] **Gas clouds / Black Dust** (0x488CC0-0x488D70). *Update 108.*
- [x] **Cascading death explosions** 0x255 -> 0x252 (0x48CDE0). *Update 108.*
- [x] **Shadows**: vehicles (0x48E6D0-0x48E850) and shells (0x4884C0-0x4885F0). *Update 116*: the vehicles'
      (parts 0x13B-0x14A, drawn darkening through dat\b16.hsh, stretched and offset by the sun, behind
      `[Options] "Show vehicle shadows"`). The "shells'" turned out not to be a shadow: 0x4884C0-0x4885F0 are the
      shell guns' glow at the target (update 119, with the shots' visuals below).
- [x] **Ambient-sound scenery** (0x4890A0-0x4892C0, with its 0x3AC substitution that shifts the scenery's rand()).
      *Update 109*: the 0x3AC substitution; its rand() was already drawn (`scenery::Burnt` for every object). The
      types (0x2AE-0x2B5, 940, 994-996; about 1,800 in the .nsb files) aren't in OBJ.ojd, so they've no sound.
- [x] **Shots' own visuals** (0x4950D0 / 0x495180 / 0x495280, the atlas sprite 0x49D710, the textured quad 0x49D820).
      *Update 118*: the beams: four atlas quads (heatray.raw, gunshot.raw) from the gun's model point to its target
      while it fires, animated, widening by record 12's +0xA / +0xC, at the type's +0xA1 / 4 level. Found with it:
      every flying object's sprite (explosions, smoke, shells) draws at its type's +0xA1 / 4 level (48 of BMEX's 64,
      0x487846 -> 0x40F040), where the engine had used level 0 (about a sixth as bright). *Update 119*: drawn at
      their depths among the view's objects, and the shell guns' glow at the target (GType 0xFF, BOTTOM2.SPR,
      0x4884C0 / 0x4885F0, a frame a draw).
- [x] **Lights on models**: point lights on WOF pieces (0x49EC00 / 0x49ED90), relighting IOB objects (0x49E2C0) and
      ISOLib's two nearest lights at `[Game] "ISO Brightness"` (0x4111B0 / 0x4113F0 / 0x410E00); the units'
      headlights (light parts 334-346).
      *Update 117*: the point lights on the WOF faces (0x49ED90 with 0x40C930 and the light's .INT level, x "WOF
      Brightness"), and the vehicles' own light parts (headlights) going with them. (Reported in play: the lorries
      looked malformed and had no turret; the turret was standing inside the body, now held to its model point.)
      *Update 119*: the IOB objects lit by the two nearest point lights (0x4113F0 / 0x4111B0, ISO Brightness 0.5).
- [x] **Group behaviour**: target-gone speech 0x10E / 0x10D (0x4821F0), the group's re-engage (vt+0x38, 0x483760),
      messages 0x80C3 (rally), 0x808A (halt), 0x80B9 in 0x490A70. *Update 109*: 0x4821F0 and vt+0x38 in (0x80C3 in
      106); 0x808A waits on the gun emplacements (its only sender, 0x496890), 0x80B9's bit 9 on what reads it.
      *Update 122*: 0x496890 is every unit's look round, not only the emplacements': aiming (+0xE4 bit 1) with its
      group mostly on screen (+0x1D8 bit 1 clear, set by 0x483760 from the members' off-screen bit 0) a unit holds
      (0x808A, steering bit 5: going, no arrival / separation / braking / matching / cohesion), else stops (0x8031);
      the emplacements first stand their group's guns down. Left: 0x80B9's bit 9, 0x496890's vt+0xAC.
      *Update 123*: correction: +0xE4 bit 0 is *halted* (0x8031 sets it through vt+0xAC; 0x805A and each look round
      clear it), not off screen, so a group mostly halted keeps its aiming members stopped, else they hold. A vehicle's
      stand-down (0x805A, 0x491180) stops it. Bit 9 is the retreat: set by Retreat's second 0x80B9, it makes a unit
      leave the map at its route's end (0x491A96) in place of the engine's "within 256 of the point".

## 4. Interface missing

- [ ] **Battle map keys and camera**: ',' and '.' step to the next unit / construction (0x43BE50 / 0x43BF30, helpers
      0x421D70 / 0x421E50); Home and the war map's sector glide zoom smoothly (Zoomer 0x446690 / 0x446850,
      `Battlemap Zoomer Speed` 2.5, `Warmap Zoomer Speed` 1.5; 0x43A480, 0x41E000); water twinkles (0x43A670).
- [ ] **The popup's grid** (0x432C60-0x433330: 10 x 8, the hovered row opening out, flipped at the right edge).
- [ ] **Unit icon tray** on the battle map (0x483DF0-0x4849D0) and the **group power-pip strip** (0x449150-0x449340,
      `hwpower.spr`).
- [ ] **The combined order panel** for a mixed group (0x448500 / 0x4484A0) and the **Information** button and page
      (0x440300 / 0x4403A0, InfoPage 0x4A3E60).
- [ ] **Build placement** on the battle map (cursor modes 3-6: 0x485930, 0x4A03A0-0x4A14C0).
- [ ] **Retreat markers**: tool tip "Retreat to" (0x487580), arrow frames by the nearest edge (0x487490), minimap dots.
- [ ] **War map counters**: the power bar, production blink and repair icon (0x42B850 / 0x42BC90); the tool tips'
      production lines (0x45F7D0 / 0x460A40, 0x428D70, 0x42A4B0); route arrows at 50% once started, solid on hover
      (0x425730); the mini map framing the selected sector (0x423040).
- [ ] **The Resource view's overlays** (0x425E80-0x426AE0, RFHS / ROHS: stacks, bars, tool tips, click to pick).
- [ ] **Battle music**: CD tracks 4 / 5 (0x4861E0).
- [ ] **Cursors**: animation (0x40CDC0), the wait cursor (0x40E680) and the busy icon (0x44E4B0-0x44E5C0).

## 5. Sound

- [x] **Ambient voices** from the per-tick update with the crossfade (0x463C30: 150 out, 15 in). *Update 111*
      (with the engine sounds' 4 loudest).
- [x] **The speech queue** (0x4652B0 / 0x466D00: one at a time, no duplicates). *Update 111* (which end plays,
      0x462FF0, is provisional: the routine is collapsed in the listing).
- [~] **Virtual voices** resuming at the right place (0x4654E0 / 0x4657B0); FadeOutKeep; a second FadeOut keeps the
      earlier end. *Update 111*: the virtual voices and
      the earlier fade end; FadeOutKeep still to do.
- [~] **`[Sound]` settings**: Minimum Audible Volume, Search Libraries First, Unload Limit (0x4646C0 / 0x465EB0 /
      0x465EF0); loose files only in debug play (0x4660A0). *Update 111*: Minimum Audible Volume (the
      voice hand-out's audible mark); the others, and the borders (`[Sound]` "Inner / Outer (Ambient) border",
      0x464580-0x464640, points), still to do.

## 6. Front end

- [x] **Cheats**: the seven missing codes (0x4011F0, 0x401320, 0x4013A0, 0x401440, 0x4014F0, `shym`, `datsun`), the
      reset at campaign start (0x4011D0), and a wrong letter returning to state 0 without re-testing it. *Update 110* (`curtains` sets
      its flag, whose use isn't traced; the hourly repair's part of `youlikeit` waits on repair).
- [x] **WGame command 0x79** (shut down to the main menu). *Update 110* (no sender is known; the review's two
      are a type id and a research index).
- [~] **CD Player seek** ±5 s (0x413740 / 0x413990); the music step's CD status (0x413EB0, unclear). *Update 111*:
      the seek (`Music::Seek`; no button sends transports 4 / 6); the CD status still unclear.
- [ ] The rest already on TASKS.md: Display Settings, the network menus, player objects in WGame::Tick (0x403680),
      commands one queue pass later.

## 7. Later or optional

- [ ] **Players and the order-packet queue** (0x425BF0 / 0x425D30, 0x4460A0-0x449EB0), the **network player** and
      **DeePlay** (0x445EA0-0x44A8D0).
- [ ] **Demo recording and playback** (0x42D290 / 0x42D2D0 / 0x42D520, `playback.N`).
- [ ] **Placing buildings at AI.ojd sites** for the player's side too (0x441420 / 0x444510; trigger 0x8112 has no
      sender found); `[Debug] Enemy Visible` (0x442850); the "threat seen" handler (0x445D60).
- [ ] Debug tools: the scenery editor and object placement (22 routines, 0x43xxxx), the Create menu (0x49BAA0).
