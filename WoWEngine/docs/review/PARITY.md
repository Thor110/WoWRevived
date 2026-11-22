# Parity review (after update 126)

Eight reviewers compared the engine with the original, routine by routine, for 1:1 parity (not coverage, which the
`range-*.md` reviews did). Each finding quotes the original's addresses and the engine's file:line; the full reports
are in `parity/`:

| Area | Report | Visible bugs | Divergences | Architecture |
|---|---|---|---|---|
| Battle drawing | [rendering.md](parity/rendering.md) | 9 | 4 | 3 |
| Battle interface and input | [battle-ui.md](parity/battle-ui.md) | 8 | 4 | 4 |
| Movement, routes, bridges | [movement.md](parity/movement.md) | 7 | 17 | 5 |
| Combat, weapons, effects | [combat.md](parity/combat.md) | 10 | 12 | 5 |
| Battle set-up, update order, AI, fog | [battle-setup.md](parity/battle-setup.md) | 4 | 14 | 3 |
| War map | [warmap.md](parity/warmap.md) | 12 | 8 | 3 |
| Front end and platform | [frontend.md](parity/frontend.md) | 9 | 8 | 3 |
| Data loading and formats | [data.md](parity/data.md) | 2 | 11 | 4 |

The data layer (OBJ.ojd records, unit table, GTypes, terrain, lighting, normals, heights) matches field by field. The
divergences sit above it, and many share a few structural causes, so those come first.

Tick each item here with the update that fixes it.

## Phase 1: structure (fixes whole families of bugs)

- [x] **S1. View objects as the original's renderers.** Each drawn thing keeps its own sort y (+0x1E), depth (+0x26),
      shade level (+0x34) and an optional parent renderer; the key is y + depth, worked out after the parent's, every
      update. Fixes: units on bridges drawn behind the deck (0x491730 -> 0x4984C0, +0x70 while on it); turrets under
      their hulls (0x489AE0, hull + 1); shadows (0x48E850); WOF depth recomputed at each pose (0x47F340); mine and
      sinker markers on top (+0x1E 0x4650); effects' depth width/2 + 32 (0x487865); per-effect level (the move
      marker 0x17C at level 0, 0x483590: the black disc). [rendering 1-10, A1-A3; battle-ui 1, 8, A; movement 1, 24, A4]
      *Update 128*: `RekeyViews` each update (hull: posed depth, +0x30/+0x40 on a bridge link; turrets hull + 1 / + 1;
      shadows hull - 1 / - 1, held while under; parts their own pose; markers 0x4650 + 0x20 raised by +0x41); bridge
      segments at their posed depth; effects width / 2 + 32; a level on each view object: the move flag 0, scenery
      sprites 63, lamps and 0x1C0 +0xA1 >> 2, corners 45, reinforcement markers 63. Tested: a lorry crossing
      London's bridge now draws on the deck; the move flag shows its animation. The fog's share of the level is S2.
- [x] **S2. Fog level on every renderer.** Each object's update sets its renderer level from the fog (0x48B3D0 ->
      vt+0x5C): WOF sun light scaled, IOB lights rebuilt per instance at that level (0x49E2C0), sprites at level / 4.
      Fogged buildings and trees are dimmed. [rendering 3, 11, A2]
      *Update 128*: each update every unit's and prop's view objects take ftol(the fog level there x 255) (x +0xA1
      for lamps and 0x1C0); WOF faces' sun term and point lights x level / 255; IOB base lights rebuilt at the level
      (`BaseLights(level)`, cached by model and level) with the point-light cap 63 x level / 255; prop sprites at
      level >> 2. The fog level samples the type's +0x44 word (+0xAC), not the collision radius. Tested: the trees in
      London's fog are dimmed with the ground.
- [x] **S3. Vehicle classes from the factory (0x446B30).** One table of each type's class (vehicle, walker 0x3E-0x4C
      and 0x50-0x5C, flier, boat, drone, track layer, emplacement, building, scenery...) instead of scattered type
      ranges. [movement 3, A1]
      *Update 128*: `BattleMap::ClassOf` (Plain, Tunneller, Ironclad, WaterSinker, Flier, Walker, Drone) with
      `Walker`, `Flier`, `Boat`, `SteerPeriod` (1 for the water classes and drones, else 3) and `Give` (1.0 / 8.0).
      Walkers now include 0x50-0x5C, with every type's feet from 0x48FDD0; drones steer every update and give 1.0;
      boats have no cohesion; Tunnelling Track Layers all under go straight without a route, and some under drops
      the order (0x482E60). [movement 3, 11, 12, 13]
- [x] **S4. Group state as the original keeps it.** +0x1D8 bits 0-9 (3/4: a player's order to a player's group, which
      gate the move marker and "acknowledged" speech), arrival counts on the group, not a growing `orders` list; the
      selection as a group of its own (0x439000 / 0x4480F0). [battle-ui 2, B; movement 2, 6, 9, 15, 16, A2;
      battle-setup B, C]
      *Update 128 (part)*: `groupOrdered` (bit 3, set by the player's 0x0E, 0x10 and 0x13) and `PlayersOrder` (bits 3
      and 4 and the first member the player's): the move flag and "acknowledged" only then (the flag also for an
      attack's approach; the speech without a target), bit 3 cleared after; ROUTE NOT FOUND stops the members, drops
      the target, clears bit 0 and speaks only for a player's order; arrivals per group (+0x56, reset at the group's
      count, so they carry over between orders); the move's counts from the whole group (0x4216D0).
      *Update 130*: battle groups are separate from war map objects: each unit keeps its war map object (`ref`, for
      lists, losses, retreats and ',' / '.') and its battle group (`group`). Selecting (a click: its war map object;
      the box: every war map object it touches, 0x8088 -> 0x439000) takes their units into one new group of the
      player's (0x4480F0, `AddGroup` / `SelectRefs`), unless they already make one whole group; the group stays
      theirs once deselected (0x4392A0 only drops the pointer). The new and the old groups are recomputed (0x800F ->
      0x4802E0: range, think interval, the members' stance averaged, ground costs). Tested: the Martian battle's four
      walker objects selected together became one group with one route, all eight walkers moving.
- [x] **S5. Per-object updates in start order** (the world list, 0x43A980 / 0x47D610): each object does its whole
      update in turn, instead of the engine's passes (all driving, then all guns...). Look-round timers start at
      rand() % 30 (0x4957A2). [battle-setup 4, 14, 15, A; combat A5]
      *Update 129*: each unit's whole update, then its turrets' and guns' (its children, made as it started) and its
      pose, before the next unit's, in place of a driving pass then a guns pass; the look round's count starts at
      rand() % 30. The groups, the computer player and the scenery keep their places before and after the units, as
      the world list has them (made before deployment / after).
      *Update 130*: each group's chase (0x4826D0) runs first in its own update (0x483B70), with members only; the
      update goes the game loop's way: the computer player, then the queued messages (the delayed deaths and 0x80F4
      posts), then the screen (fog, twinkles, the end check, the groups, the units in start order, the scenery).
- [x] **S6. Parent / child objects for parts.** Guns, turrets, building parts, lights and tree circles as children
      with a parent link and offsets, removed from the parent's list as they go. [combat 4, A1, A3; data A]
      *Update 129 (part)*: a building's lost parts come off its list at once, so one big hit takes several (0x48EF80
      with 0x47F220), and each part's explosion has the building as its firer, sparing it and its group (0x489FA0 ->
      0x488200); tested: 900-damage hits on a Factory take two parts each and its health falls by 900 each.
      *Update 130*: turrets and guns have points of their own, posed after their parent each update (`PoseChildren`:
      held to their parent's model point, else at their offset turned with the parent); the range arc, the turret's
      turn, the aim's jitter (each 0x496200 from its own object), the mortar's launch and the landing spread measure
      from them, and the beams start there. A lost part's light goes with it (0x47EAA0). The explosion's own object
      is made before its parts (0x488070 then 0x488350).
      *Update 136*: a circle under a starting building goes with its trees (0x48EB3A, `ClearScenery`).
      *Update 140*: a circle is the explosion's target, not its trees, and dies with them (`CircleDies`). Open:
      what 0x47EAA0 does for each child as its parent dies beyond taking it out.

## Phase 2: high-severity behaviour (small, independent)

- [x] Units never pick targets: the look-round scale `flt_59E44C` is `[Tweak] "Audible radius multiplier"` (1.0), not
      0. [combat 1]
      *Update 131*: `setup.lookScale` from the ini (1.0), in the look round's emplacement check and the auto-target
      sight.
- [x] Air-burst table chosen by impact height over 64, not weapon size (0x487F50). [combat 2]
      *Update 131*: `ImpactAt` takes the air bursts when the height over the ground is over 64, else water or land by
      the terrain; the size only indexes (1-67).
- [x] A building's lost part fires as its building (0x489FA0), so it doesn't hurt it. [combat 3]
      *Update 129* (S6): the part's explosion has the building's group as its firer.
- [x] Selection marks at ground + lift + the type's +0x41 (0x47F480 / 0x4406A0), not the feet. [battle-ui 4]
      *Update 131*: `UnitScreen` projects at the ground + lift, less the type's +0x41 (scaled); tested: the Martian
      battle's box-selected walkers have their brackets round their bodies.
- [x] Scenery sprites (street lamps, ~49), reinforcement markers (63) and building corners (45) at their levels, not 0.
      [rendering 4]
      *Update 128* (S1/S2): each view object's level (lamps and 0x1C0 +0xA1, markers 63, corners 45), fogged.
- [x] Landscape tint and red weed from the Martian constructions and resources (0x43981C). [battle-setup 1]
      *Update 131*: `WarMap::MartianInfestation(sector, human)`: the Martian side's constructions and resources there,
      as numbers of war map objects (destroyed ones left out), for both `enemyCount` and `lastEnemyCount`.
- [x] Human command building 0x3A, not 0x13A, in ReinforcementSources. [warmap 3]
      *Update 131*: `ReinforcementSources` asks `HasCommandPost`.
- [x] Destroyed constructions leave `finished[]`. [warmap 13]
      *Update 131*: `finished[]` is gone; `WarMap::Finished(sector)` walks the live counters' objects (not destroyed,
      +0x7C at least +0x78) each time.
- [x] Menus open with the back button focused (Quit: Enter mustn't quit). [frontend 1]
      *Update 131*: `FocusLast()` after the bottom button in Load Game, Quit Game, Sound Control, Display Settings and
      Critical events.
- [x] A vehicle arriving on a bridge clears its link (0x491A9C). [movement 4]
      *Update 131*: `Advance` sets `link = 0` after its 0x808B(1); the lift stays.
- [x] Mines: the collision filter's test the right way round (0x4929A0). [movement 8]
      *Update 131*: a class 3 / 4 vehicle passes over a low object only when it isn't crushable (*provisional* as
      before: the height is the GType's first frame's).
- [x] The pod lands before deployment (0x439AE5); a forfeit returns to the war map 150 updates on (0x43AA18).
      [battle-setup 2, 3]
      *Update 131*: `LandPod` runs after the scenery and before `Deploy()`, its point a centre for Martian lists (as the
      other side's, and as the owner's with no command building); its blasts' sounds wait for `Begin`. A forfeit
      (`forfeit`, no box up) sets `forfeitAt` = now + 150 and `Update` sends 0x76 then (`TakeCommand`).

## Phase 3: the rest

Every other item in the eight reports, a batch at a time, the medium ones first. Items the earlier phases already
settled: battle-ui 8, combat 4 and 9, movement 15, 16 and 24, battle-setup 4, 11, 14 and 15.

**Update 132**
- [x] battle-ui 5: the other side's units in the fog have no hotspot (0x48B2D0), so they can't be hovered or clicked.
- [x] battle-ui 6: the popup's rows: items with the same command share a row (0x432D70 / 0x432E90, 10 rows of 8),
      laid out left to right or right to left off the right edge (0x433220), each row closed to its first item
      (0x433330(-1)) and opened by hovering it (0x80F1). The placement already used the first item's size (0x432FB0
      reads `[esi+7Ch]`, item 0, not the menu); the menu's own size for the edge test is *provisional* (the laid-out
      extent). Tested: a walker's stances row opens out on hover. The war map's popup still has one column.
- [x] battle-ui 7: a mixed selection gets only the orders every type of its members has (0x448500; "one kind" taken
      as one type, *provisional*).
- [x] combat 5: only shells and gas keep their absolute height as they fly; the rest follow the ground (0x47F770 with
      +0xB8 bit 10).
- [x] combat 6: scenery smoulders only once a hit has taken it from over 40% to under (0x48AB20 -> 0x440060).
- [x] combat 7: no smoke and no turret sound from a unit in the fog (0x48AEF0, 0x489B40).
- [x] combat 8: a vehicle whose look round finds its health under 0 vanishes with no explosion (0x4967E0 -> vt+0x78
      0x495BB0), and that update has no smoulder.
- [x] combat 11: the rocket's landing takes two rand() calls and the target's height (0x488830).
- [x] combat 12: the research multipliers scale the raw hit, truncated once (0x48A5A0); gas spares the Martians
      whatever the research.
- [x] movement 7 / 23: the engine sound follows the vehicle (0x49330A, `Sound::Move`) and is placed with the lift.
- [x] movement 10: no member off a bridge link: the move does nothing (0x482F98).
- [x] movement 14: steering's neighbours and the object ahead compare heights with the lift (+0x80).
- [x] movement 18: "CAN'T GET TO WITHIN ATTACK DISTANCE!": with a target the route's radius is the range in vertices,
      and a goal moved further out than that is route not found (0x4830FD).
- [x] movement 22: every vehicle member's footprint is cleared before the link test (0x482F17).
- [x] battle-setup 5: battle on from the lists' numbers of war map objects: (HUNI and (AUNI + ACON)) or ((HUNI + HCON)
      and AUNI) (0x439D22).
- [x] battle-setup 6: the computer player starts (0x8075) only when both sides have vehicles or buildings
      (0x439DF4).
- [x] battle-setup 8 (part): Retreat and the markers ask room for the group's battle units (0x480F22), not one. The
      stale slot's sector (an original bug) isn't reproduced.
- [x] battle-setup 9: the war map objects that have left by each sector count against its room (dword_4F86B8 /
      dword_4F76F0, 0x429453).
- [x] battle-setup 10: +0x1D8 bits 8 and 9 from the members' move classes: land markers refuse boats, boat markers
      land units, Retreat refuses a group with both (0x4802E0, 0x481F70).
- [x] battle-setup 17: the losses reach the war map whether or not a battle was on.
- [x] data 1: no burnt roll for the types the factory doesn't build on WScenery (the bridges 445-447 and the others
      0x446B30 sends elsewhere).
- [x] data 2 / 3: a sound's variations move on before use (1, 2, 0 ...), and an empty group falls back to the first
      (0x466BC0).
- [x] frontend 2: the wide menus centre each control (0x42F020 with 0x10), the event columns placed by hand, then the
      union (0x40ABDA).
- [x] frontend 3 / 4: the research screen neither fades in nor out (0x45E6B8); Display Settings fades in.
- [x] frontend 5 / 6: the CD Player's time panel is solid; Play is focused, and the focus follows Play / Stop.
- [x] frontend 7: radio buttons take the right button (checkboxes *provisional*, untraced).
- [x] frontend 8: the click fade: 50% one tick after, black the tick after that.

**Update 133** (the war map; details in WARMAP.md, "Parity fixes in update 133")
- [x] warmap 1: the Resource view's bars, demand markers and facility rows, no counters (row offsets and tool tips
      *provisional*). Tested: the bars stand at each sector's waypoint in the London save.
- [x] warmap 2 (part): the production / idle icon with its blink; the repair icon and power bar wait for repair.
- [x] warmap 4 / 5: the mini-map at (528, 298) Human / (536, 306) Martian, the selected sector's frame, the view's
      outline fixed in size and in the side's colour.
- [x] warmap 6: edge scrolling as 0x484DF0 (the window's 32-pixel border *provisional*).
- [x] warmap 7: the rubber band's two-colour open-cornered lines.
- [x] warmap 8: set-off routes' arrows translucent (flag 8: the blend-table span routine 0x455276), solid under the
      mouse.
- [x] warmap 9 / 10: no "Battle in X" line; the status strip behind [Modern] "War map status strip".
- [x] warmap 11 / 12: the other side's dimming sticks; unfinished resources dim; the hovered counter is raised.
- [x] warmap 15 / 16 / 17 / 18 / 19: the saved view restored; View map after an object's items (and the menu's rows);
      the ending after the sound's length; no travellers in the Unit view's shading; the first-hour jump only in
      the Unit view.
- [x] `Finished()` (update 131) now counts the facilities, which are resources: a regression found while doing
      warmap 1.

**Update 134** (the battle's low items; details in BATTLEMAP.md, "Parity fixes in update 134")
- [x] battle-ui 10 / 11: a ground click with no point is still sent; mode 0's cursor over the other side stays.
- [x] combat 10, 13-22: the glow's frame, the light roll, the dying hit, the firer's sparing, the one-update delay,
      the underground target, the idle emplacement, the gun's aim flag, damage precision, the cascade's weapon,
      "Game Frequency" (read from [System], *provisional*: it's a registry value).
- [x] battle-setup 7, 12 (part), 13, 16: the planner's worth over 0, the first decay after the reveals, vehicles only
      for a group's range and grounds, every member's staggered death.

**Update 135** (the rest that could be settled from the disassembly alone)
- [x] rendering 9: checked, already matching (the beam's level +0xA1 >> 2). Rendering 12 (qsort vs stable_sort of a
      WOF's pieces) and 13 (the view list's ties) are left on purpose: unstable orders only differ for coplanar
      pieces and equal keys, and copying them would mean copying the CRT's qsort.
- [x] frontend 9: text centred on a point takes the widest line's box (0x40FBF0).
- [x] frontend behaviour 1: the tick is 1000 / "Game Frequency" whole milliseconds (33 ms, 0x401CAA), the value read
      from [Main] (the registry redirect's root section, where the game key's root values live); the battle's
      gun and light timings read the same value (update 134's [System] corrected).
- [x] frontend behaviour 3: a fade-out's ratio replaces the fade-in's (0x464DC6).
- [x] frontend behaviour 5: the movies' sound ignores Master (no SmackVolumePan).
- [x] frontend behaviour 4 and 7: checked, equivalent (the reviewer's finding).
- [x] data 4 / 5: a sample already in any of the sound's groups isn't added again and a group has no cap
      (0x466B40); an opcode 0 type over 5 keeps the last priority (0x463DAB).
- [x] data 7 / 9 / 10 / 11 / 12: records 38 and 39 in double, one truncation; record 35 multiplies +0x4A's 100 with no
      record 34; record 6's first word and record 12's words signed; [Tweak] "VisRadMul" read; an off-map scenery
      object at (0, 0).

**Update 136** (reported in play: walkers drawn behind the Martian command building; details in BATTLEMAP.md,
"Deployment")
- [x] The view list's order was right (0x40D750's key, checked again); the walkers stood inside the building. The
      deployment now goes as 0x439AC0 / 0x4222C0 have it: the objects with saved places start first (the world
      list's vt+0x14, 0x47D030), then the pod, then each placed object starts as it's placed, so its footprint is
      down before the next one's checks; vehicles are placed through their group's cost (0x480740 -> 0x480680),
      which refuses occupied vertices, and against the started objects (0x47FE10: octagonal, the other's +0xAC).
      Tested: the Martian battle's walkers deploy clear of the command building, and a move ordered across it
      routes round it.
- [x] An IOB object's radius is its model's (0x47EB10: max(2 x half width, depth) x 0.66 to +0xAC and the type's
      +0x44), and its footprint's (+0x46) too for a child or a scenery class (vt+0xB8). The engine read both words
      from OBJ.ojd: the Martian command building's radius was 50, not 110, and its fourteen parts had no footprint.
      Explosion reach, the fog's sample, smoke, point lights and the footprints now take them (`Radius44` /
      `Radius46`).
- [x] A building's start takes out the scenery standing on it (0x48EB3A: the scenery list's objects nearer than
      their +0xAC + its +0x46, vt+0x78 0x4895A0, no explosion). This is where a tree circle goes, its trees with
      it (S6's open point, in part: a circle hit by weapons is still untraced).
- [x] The look round (0x496570) asks the target's +0xC0 bit 0, which a building's start clears while its
      construction isn't done (0x48E9D0); the engine asked for health over 0 instead. A building site isn't picked.
- [x] The log no longer reports IOB and WOF models as missing sprites.
- Checked and matching, nothing to change: 0x48A5A0 (splash damage), 0x4025F0 (Random), 0x41A330 (pods), the
  route record's header (0x425A60 / 0x4257C0), flt_59E44C as the "Audible radius multiplier" setting (0x495620).

**Update 137** (PLAN.md Stage 1, first part)
- [x] The listing against WoWRevived's executable edits (PLAN 1.0). `IDA-Output.txt` shows the retail code at the
      sites looked at: the CD Player's check is `jnz` (0x40892D); Display Settings makes the resolution slider, both
      colour radios and the lights toggle (0x40987F-0x40994E); the drag rectangle's clamp is 640 (0x49C6B0, six
      0x280s). Only the import is changed (`_INMM.dll`). The .smk check wasn't looked at. Nothing in the engine
      came from a patched value. The clamp (the rubber band's vt+0x1C: at most 640 either way) isn't in the
      engine; at 640 x 480 it can't act.
- [x] warmap 14 (PLAN 1.1): units on a route are put back where they were. The route's units are a
      WarMapRouteEntryList (0x427E80 / 0x427F70): a u32 count, then each entry: its object, then eight values
      (0x42B150 / 0x42B250): +0x24 the path index reached, +0x18 its number in the route (its formation place,
      +0x10 / +0x14, comes from it), +0x2C its speed, +0x30 the way left, +0x34 the hours left, +0x38 / +0x3C how
      far along its segment, +0x28. `SaveRouteEntry` reads and writes them; `WarMap::UseSave` starts each route
      on its saved path and sets each unit's entry, the arrow trimmed as 0x4258F8 does. The route's +0x30 (hours
      on) isn't saved, and the label's hours are worked out again the game's way (0x425999, the x 0.01 quirk).
      Tested: a unit sent from London, saved part of the way and loaded, stands at the same place on its arrow.
- [x] The route list's layout in the save was the engine's guess ("WMOB", a count, the objects); the game writes
      the count alone, then the entries. The engine now writes the game's and reads both.
      *Provisional*: checked against the disassembly only; no save made by the original with units on their
      way has been read yet.
- [x] Scenery on scorched ground doesn't come back (PLAN 1.2): ActivateAll (0x47D030) takes out an object that
      answers vt+0x64 after its start; for WScenery, the street lamp, the pod's class and the tree circles that is
      0x489750, the sector's Damage Layer not 0 at its vertex. For every other class it is never (0x48B580).
- [x] The deployment's object check counts the scenery (PLAN 1.2): 0x47FE10 searches dword_4D2550's +0x118 grid,
      the same one an explosion searches for its victims (0x47FC50), four buckets round the place (0x449920).
- [x] data 6, 8, 13: an SFX id met again keeps its entry (0x463DE1); record 44 multiplies +0x5C there and then
      (0x438595); Random(n) takes the remainder for n under -1.

**Update 138** (PLAN.md Stage 1, second part)
- [x] battle-setup 12 (the rest): the fog's decay and reveals (0x43A9B1, 0x49681A, 0x48C84E) and the smoulder's
      smoke (0x48AF5D) go by the game's frame counter, dword_4D2558, one more each pass of the game's update
      (0x403749) whatever screen is up. `FrontEnd::frames`, given to the battle (`Setup::frame`, `Frame()`).
- [x] movement 20: the muster order (0x80C3, 0x490D06) ends with one step of the vehicle's driving (vt+0x11C,
      0x492F40; the walkers' and fliers' with it), after setting +0xB8 bit 9 (0x47F320(1), *provisional*: what
      the bit is for isn't traced).

**Update 139**
- [x] Loose files as the game takes them (0x414DE0 / 0x415030): an archive entry is marked when a loose file of
      its name exists as the archives are opened, and a marked entry is read as a file. Dat.wow's are looked for
      in `dat\`; MAPS.WoW's in `Map\` (0x414FAB, a folder the game doesn't have), then read from `maps\`. The
      engine let any loose `maps\` file win; now it's the game's rule, slip and all. Tested with the owner's
      `dat` files loose, and with a loose map file with and without a `Map` folder.

**Update 140**
- [x] What is in the world's grid (PLAN 1.2 / 1.3, S6's last point): an object goes in as its position is set
      (vt+0x40, 0x48AA80), unless it's a child with +0xB8 bit 2 (a circle's tree); children are placed by
      vt+0x7C (0x47F960), which doesn't touch the grid. So a building's parts and a circle's trees aren't in it:
      the deployment's check and an explosion (0x47FC50) never meet them. A tree circle is in it: radius 48 / 24
      / 12, health 120. The engine hit each tree; now it hits the circle, and a dead circle (0x80A0, its effect
      591) takes its trees with it (`CircleDies`). Not seen running (no combat in the test battles).
- [x] movement 19 (PLAN 1.7): on arriving, a vehicle with a point aimed at (+0xC8 and +0xCC set, 0x496200) that
      isn't its target takes it as its target and hasn't arrived (0x491A0C); standing down clears the point
      (0x49648B).
- [x] battle-setup 18 (PLAN 1.6): nothing to change. The list the counts walk (the world's +0x118) holds only
      the war map objects' battle units (0x4216E0 -> 0x43C770 is all that adds to it), which is what the engine
      counts.
- [x] warmap 20 (PLAN 1.10): the battle pulse goes by its own counter, put back to 0 when it has passed 22 while a
      battle is pulsed (0x41DED1).
- [x] frontend behaviour 2, 6, 8 (PLAN 1.9): a held button hides the tool tip only of a window that answers 0x80CF
      (the controls, 0x42E2EC / 0x434465; the war map's counters keep theirs); the CD Player opens on the track
      that's playing (0x413D30) and a stopped track shows where it stopped; Debriefing's delay runs from its
      second draw. The CD Player's +/- 5 s seek was already in.
- [x] A pod under a starting building is taken out like the rest of the scenery (PLAN 1.2).
- [x] battle-ui 9 (PLAN 1.8): the game's display flag (byte dword_4EA364, 0x42CA28) as `Screen::windowed`, kept by
      main as Alt+Enter changes the window; the battle map's and war map's edge scrolling take 32 pixels in a
      window and 4 full screen (0x484E2C). The mouse owner's part (dword_4EA444) is still only the popup's.
- Loose map files: a **modern fix**, not parity (MODERN.md). The engine marks MAPS.WoW's entries from `maps\`,
  not the game's `Map\` (0x414FAB), so they work. Leave it.

**Update 141** (PLAN.md Stage 2: the route finder and the bridges)
- [x] movement 17, A3 (PLAN 2.1): the route finder is one search kept between updates (`RouteFinder`, on a
      resumable `AStarRun`). A group's move begins it (0x494440) and returns; the group's update carries it on 200
      neighbours at a time before its chase (0x483B8C -> 0x483270, `PollRoute`) and ends the move: ROUTE FOUND
      (the route, the flag, the acknowledgement, each member's 0x802E) or 0x4833F0. While it's another group's,
      a move is put off 10 updates (0x482E75, `heldMoves`); a group left with no members lets its search go
      (0x494960). Tested: the same routes as before in both battles. Not tested: two groups ordered at once.
- [x] movement 21, A5 (PLAN 2.2): a bridge is an object in the world's grid at its own point, with its type's
      health (+0x4A, 100) and radius (+0x44). An explosion damages it like the scenery; dead (0x80A0), it gives
      its type's destruction effect, its segments go, its footprint comes off, its table entry is cleared
      (0x4866C0) and it breaks (0x486850): both route nodes come out (0x494370 `RemoveNode`), which gives up a
      search under way at its group's next poll (dword_59E41C, 0x4833BA). A building starting over one takes it
      out without the effect (0x48D2E0). A vehicle on its link keeps link and height, as the game's does with
      the table's entry empty (0x493374). Tested by killing London's bridges: the deck is gone and a move
      across the river ends in ROUTE NOT FOUND; with them standing the lorries cross.
      *Provisional*: a bridge broken but still standing (segments lost at its start, 0x4865A0: only from a
      saved battle), which drops its riders or drowns them (0x4933D0), waits for battle saving.

**Update 142** (PLAN.md Stage 2, continued)
- [x] PLAN 2.3: the vehicles' look ahead (0x492350 -> 0x47FE10) finds the scenery as well as the units, the world's
      grid holding both: a scenery object in the way (0x4929A0: not a tree; a walker passes one under 0x30 /
      0x20 tall, the renderer's +0x2E less +0x2C, an IOB's being its header's 0x12; on a bridge link not the
      bridges' pieces; else category 0, 2 or 3) is steered round and, at the unit's own place, pushes it off by
      its radius. Crushing (the mines) and the footprints' and route nodes' removal were already in (updates
      131, 136, 140, 141); the plan's wording was stale. The line-linked crushable scenery (0x1D0-0x1D3, 0x2B6,
      0x2B7) still isn't made by the engine.
- [x] PLAN 2.4: nothing to do; the walkers' leg animation and feet (0x490070, 0x48FDD0) have been in since
      updates 113 and 128.
- [x] PLAN 2.5: a bridge's segments take their object scale (the part's float x the type's +0xA8, 0x47EE24 /
      0x47EEC0) like a unit's parts. IOB and sprite renderers have no scale (0x47EB10), so the scenery has
      none to apply; the lorries' headlights were already in (their light parts).
- [x] PLAN 2.10: the battle map's music (0x4861E0, its 0x813C from 0x439DBE): CD tracks 4 and 5 in turn from 4,
      the next step at the track's end, nothing with the CD option off. *Provisional*: where the track is stopped
      on leaving the battle.

**Update 143** (from the owner's screenshot of the original: a new Human game, London, nothing moved)
- [x] The Human game's side colours had red and blue the wrong way round. 0x40B3B0 takes (palette, r, g, b) and
      its callers in 0x40B120 push b, g, r: the Human view rectangle and rubber band (dword_4DAF9C / 4DAFB8) are
      0, 0, 47 and 0, 0, 40, blue, not red; the player's non-vehicle dot (4DAFA4) 0, 63, 32; the other side's dot
      (4DAFA8) 63, 0, 0. Fixed on the battle map's minimap and the war map's mini-map outline and rubber band. The
      Martian game's were right. The screenshot's rectangle is #0000BD.
- [x] The burnt trees (0x399-0x3A9) aren't trees to the game (+0xB8 bit 2 is set for 0xCF-0xE5 only, 0x47D828):
      the exclusions added in updates 137 and 142 took them for trees.
- **Open, not understood: where the lorries deploy.** The original puts London's five lorries north-east of Big Ben
  (the block's spot about 4331, 2790: ring 95, the fifth of its six angles, 288 degrees); the engine takes the
  second angle, 72 degrees (4331, 2970), south of the tower by the hedge. Same centre (the HQ, 4302, 2880), same
  ring, same block. So the original refuses the 72 degree spot and the engine doesn't. Checked and matching:
  the ring (0x4222C0: k, size, e, step, the angles), the ground cost (0x480680) and its tables (terrain 11 costs
  a lorry 8, not -1), the footprint marking (0x47C280), the HQ's class (0x48E8A0, vt+0xB8 false, so its +0x46
  stays OBJ.ojd's 28). What would explain it, none confirmed: the footprint of the Parliament part 825 reaching
  one vertex further (its radius 5 cells, not 4: 121 against a needed 128), or the HQ's reaching two cells. Tried
  and ruled out: the HQ's footprint at its model radius (then 288 degrees is refused too).
- **Correction to update 140: a building's parts are in the world's grid.** An object's start (0x48A8B0 ->
  0x48A660) sets its own position again (vt+0x40, 0x48AA80) when vt+0xF0 (0x48A830) says so, and it does for a
  part whose parent is a building (category 2 or 3). So the parts are found by the deployment's check, the look
  ahead and an explosion (0x47FC50), each with its own radius and health. A tree circle's trees still aren't
  (bit 2 with a parent). **Not yet in the engine**: its explosions and look ahead still treat a building as one
  object.
- **Seen in the screenshot, not yet traced**: Big Ben and Parliament are lit brighter and warmer than the
  engine's; the road has light pools the engine doesn't draw, and the engine draws one round the lorries that the
  original doesn't show; the bridge's deck is much darker. The lorries' dots are #E7FF00 (explained in update
  144; the screenshot is 16-bit colour, the owner's word).

**Update 144** (from two more of the owner's screenshots of the original: a new Human game's sector 28 and a
new Martian game's first battle, nothing moved; 16-bit colour, `ISO obj col rad mul` 0.66)
- [x] The minimap's dots. 0x4A1D80 keeps each dot as x, y and a colour word, and makes the word as the 16-bit
      colour OR its own low byte moved up a byte (mov dh, al; or edx, eax). A colour with an empty low byte
      comes through unchanged: the Martian game's red (0xF800) and orange (0xFC00). The Human game's green,
      0x07E0, becomes 0xE7E0: the yellow the original's Human dots are (#E7FF00 in all three screenshots), where
      the engine drew green. Its other colour 0x07F0 becomes 0xF7F0 and the enemy's 0xF800 stays red.
- Confirmed by the two frames: sector 28's deployment (the Command Post 58, its corner markers and the units'
  block are where the original puts them) and the Martian start (each walker where the original has it). So
  deployment is right in general and London's spot (below) is particular to that sector.
- **London's spot, still open.** More was ruled out. The centre is right (0x421FE0: the last object of the
  sector's command type in the world's list, 278 in sector 30, at its +0x78 / +0x7C). The object check
  (0x47FD40) uses each object's own radius (+0xAC: the type's +0x44 as it's made, 0x47D7A2, or the IOB model's)
  and is asked by each member in turn; with the HQ's six parts put in as obstacles at their own radii the
  72 degree spot still passes. The bridges' segments look to be in the grid too (each is placed with vt+0x40
  as it's made, 0x497A6C; the bridge class's own vt+0x40 is 0x48AA80, the segments' class wasn't checked, but
  the collision test 0x4929A0 names their types), and Westminster Bridge's last (type 462, radius 48, at
  4407, 2978) comes within 3 of refusing the block's fourth member (4345, 2984: 65 against 48 + 14), but
  doesn't. The ground round the spot is nowhere 0 for a lorry in the 3 x 3 the area check reads. The
  two other footprint routines (0x48B690, the line-linked objects 0x128 / 0x1D0-0x1D3 / 0x2B6 / 0x2B7: radius
  32, flag 8; 0x48CB10, the mines 0x273-0x276: their +0x46, flag 8; both only once built) don't come into
  it: London starts with the HQ and five lorries only. What's left untested is anything that would move the
  bridge's last segment or widen it by 3, and the rand() order (a different heading doesn't move a spot).
- **Not yet in the engine, noted while reading**: flag 8 on the ground from the mines and the line-linked
  objects (the cost routine already refuses it for the sector owner's groups); the bridges' segments and the
  buildings' parts as objects in the grid.
- **Seen in the two frames, not yet done**: the buildings' ground shadows (part type 331, class 14,
  `dat\b-shad01.raw`) and the vehicles' are missing or fainter in the engine; the group panel top left (icons
  with strength pips, Stage 2.6); the engine lights an unfinished building's corner markers yellow where the
  original's are grey.

**Update 145** (from the owner's notes on the frames: Southampton's bridge lighter than the original's; the
Martian start's units turned differently)
- [x] A bridge's deck was drawn too bright. Its segments (types 0x1CC-0x1CE) are objects of type 0x1C0's class
      (the factory's 0x447541, constructor 0x48CFB0), and that class's vt+0x5C (0x48D000) gives the renderer the
      fog level round the object x its type's +0xA1, 150 for the three segment types, where the engine drew them
      at 255. London's deck measured 24, 33, 52 against the original's 12, 17, 38; it is 9, 16, 31 now (the
      engine's lorries stand by it and the original's don't, so the two frames aren't lit alike there).
- [x] The rand() calls of a unit's start are in the original's order: a vehicle's phase (+0x146) is drawn in its
      constructor (0x490943), which runs on the war map when the unit is made, not as it starts; the look
      round's count (0x4957A2) is drawn after the base start has started the parts, so after the light parts'
      rolls (0x48DE80). The engine draws the phases ahead of the scenery, so they don't come between the tree
      circles' seeds (0x48F42A) and the deployment's headings.
- **Open: how the deployed units end up facing.** rand() itself is the C runtime's, linked into the exe
  (0x467B30), the same generator the engine has; what can differ is where in its run each draw falls. By the
  listing a placed unit is given a heading of rand() & 0xFFF (0x422738, the renderer's vt+0x30), its start
  zeroes +0x100 (0x490EA4), the muster (0x80C3, 0x490D06) sets +0x100 to the heading from the spot to the unit
  (0 for a unit alone, which stands on the spot) with speed 0.2, and each driving step turns the renderer to
  +0x100 (0x49333D). The engine does that, and then turns on for a few updates while the speed dies away
  (flags 0x409, target 0, 0). The owner's frame of the original shows other facings, for the lone units and
  the groups of three alike. Not explained. The steering only runs on the updates the phase picks
  (0x492F59), and the phase comes from the war map's run of rand() (srand(1) as the war map is built,
  0x417DA3), which the engine doesn't reproduce: if the original's facings are the same on every fresh start,
  that run is what to trace next (the constructors after 0x417DA3, the pods' 0x41A330, the music's 0x41B070).

**Update 146** (the owner: a fresh Martian game puts the units in the same places every time)
- [x] A tree made burnt is still a tree. The base constructor (0x489E20 -> 0x47D828) sets +0xB8 bit 2 from the
      type the object is made as (0xCF-0xE5); only then does WScenery's constructor (0x489380) roll it burnt and
      change the type to 0x399-0x3A9. So in a sector the Martians hold, the burnt trees leave no footprint
      (0x48A950), aren't in a vehicle's way (0x4929A0) and don't refuse a deployment spot. Update 143 had taken
      every burnt tree for a solid object; an object placed as a burnt type in a map file still is one. The
      flag is now kept from the type each object was made as (`PlacedObject::tree`, `Prop::tree`).
      Seen in the owner's Martian frame: the minimap showed the second group of three (type 65) north-east of
      the first, where the engine had it to the west, refused at ring 380's fifth spot (3748, 788) by two tree
      circles' burnt trees. After the fix every dot of the engine's minimap is on the same pixel as the
      original's.
- **Open: the facing of the deployed units.** Measured from the owner's frame by drawing the engine's units at
  32 headings and taking the nearest: the two units that stand alone (types 89 and 91) both face about 3330 to
  3450 of 4096 in the original, the same for both, where the engine has them at about 4057 and their rand()
  headings were 2467 and 2571. So it is neither the random heading nor the muster's 0. A common heading for
  two units in different places fits a turn at a steady rate for the same time: about 12 a step (the turn
  limit at speed 0.2, 0x493233) from 0 for some 55 to 60 updates. That would mean the mustered units stay
  active at speed 0.2 and go on turning, where by the listing as read so far the start leaves them stopped
  (+0x188 = 0x801, 0x490EDC) and the speed dies away in ten updates (0x492EF5). Not found: what sets them
  going. To check with the owner: whether the units of a fresh battle keep turning slowly if left alone.

**Update 147** (the first of PLAN.md's "next" list)
- [x] The ground shadows of unit buildings. A shadow part (331 / 0x14B-0x14D, 0x48E2A0) lays its mask as it
      starts (0x48E310 -> 0x48E460) whatever its parent is; the engine did it for the scenery only. A unit
      building's own shadow part is laid from its renderer, and so is the shadow part of each of its parts: every
      wing, leg and pod of a Martian building is a part with a shadow part of its own (0x47EE10 makes a child's
      parts as it does a unit's). In the owner's Martian frame the HQ's dark patches are under its four arms and
      feet; the engine now has them there. The mask is also taken off and laid again by 0x8126 / 0x8125
      (0x48E3A0: sent to the children as the parent is fogged, 0x48B3AB, and as it is placed, 0x48A742);
      *not yet*: taking it off.
- **Open, tracked in PLAN.md**: the deployed units' facing. The owner: units do sometimes turn on after a level
  loads, not every time; possibly the computer player.

**Update 148** (PLAN.md's N2 and N3)
- [x] The world's grid holds the children too. Every object's position goes through 0x48AA80 (vt+0x40), which
      puts it in the grid unless it's a child with +0xB8 bit 2 (a tree circle's tree): a part as it's made
      (0x47EE4E), a bridge's segment likewise (0x497A6C; the segments' class, 0x48CFB0, has 0x48AA80 at vt+0x40
      too, 0x4AEEC0). So the grid search (0x47FE10, `ObjectAt`) now finds a building's parts still standing, each
      at its own point with its own radius (an IOB part's model radius), and a bridge and its segments (radius
      48), through the same test as the scenery (0x4929A0). The look ahead sees them; the deployment's check now
      asks the same routine, each member for itself (0x4225B5), in place of its own loop.
      Checked against the owner's frames: sector 28's and the Martian start's minimap dots are still on the
      original's pixels. London is unchanged (still the 72 degree spot).
- [x] A built mine (0x273-0x276) marks the vertices within its +0x46 with flag 8, not the footprint's 2 (its own
      vt+0xE4 / vt+0xE8, 0x48CB10 / 0x48CB60), which the ground's cost refuses to the sector owner's groups.
- *Not yet*: an explosion's victims by part (0x47FC50); the parts of scenery objects and vehicles in the grid;
  the line-linked objects' flag 8 (0x48B690, radius 32); a mine finished during the battle.

**Update 149** (PLAN.md's N4, first part: the group panel)
- [x] The group icons top left of a battle (seen in every frame of the original). As the battle is set up
      (0x439A69) a holder is made (0x483DF0) and 0x422910 gives it an icon window (0x484120, 0xB8 bytes) for the
      first of the player's unit list that is the sector's command building, then one for each of its objects
      of category 1. The holder lays them out (0x484070) from 32, 32, each its width + 2 on from the last, a new
      row the height + 2 down when one would pass the window's width less 32. An icon shows GType 0x76 at its
      type's war map counter frame (+0x2C, 0x40F000); under it the strength pips (0x449150 / 0x449340): one for
      each unit of the object, GType 8 at frame ftol(0.5 + 9 x health / the type's), the sprite's width - 1
      apart, centred; with the object selected (0x8012 -> 0x484840) GType 5's four corners 8 in from its own.
      The left button let go on one (0x484920) selects its object alone (0x8088); in cursor mode 1, on one
      already selected, a second click within 10 frames centres the view on the selection (0x43A480).
      Against the owner's frames: the Martian start's four icons (3, 3, 1 and 1 pips) and London's one (5 pips)
      are on the original's pixels (the pips' row was set by eye, 3 up from the icon's foot).
- *Not yet*: the icon lit under the mouse (its renderer's level 1, 0x4843D2) and flashing while +0x94 counts
  down (0x8111 and the other messages of 0x4844A0's table); the cursor shapes it asks for (0x20); the windowed
  game's left margin (dword_4BA05C).

**Update 150** (the rest of Stage 2.6)
- [x] The label on a unit gone under (a tunnelled Track Layer, a submerged boat). 0x4991D0 (vt+0x17C, as
      vt+0x188(1) hides one of the player's) makes a BoxedText (0x49D260, the tool tip's class) of the unit's
      name (0x440720) in font 0x6F at its screen point, grey in a Human game and black in a Martian one, its
      box grown by 2 / 2 / 1 / 1 (0x4163C0); it goes as the unit comes up (vt+0x180). The "0x6F" the earlier
      notes took for a string is the font. Drawn now (`DrawLabels`); not seen running (neither test battle has
      one).
- [x] The cursor over a group icon (its 0x20, 0x4844CD): the select cursor in modes 0 and 1, 0xC otherwise.
- Checked, and not things the game does: the group keys (packet 0x12 has no handler); Shift adding to the
  selection (0x497680 sends packet 5 only with the debug flag byte_4D1279, or a class whose vt+0xEC says yes,
  and all 182 vtables have 0x48D7C0, no, there); the hotspot's cursor asking vt+0x6C (0x497110 doesn't).
  TASKS.md's and BATTLEMAP.md's notes that listed them as missing are corrected.

**Update 151** (PLAN.md's N5: objects under a light pool; measured against the owner's sector 28 frame)
- [x] Point lights on a building's parts. Each part is an object of its own (0x47EE10), so it is lit from its
      own point with its own radius (its renderer's +0x38..+0x3E); the engine lit every part from the building's
      point with the building's radius. And the object's height goes in a quarter of its model's height up
      (0x41141C: header 0x12, sar 2, added to the z before the lights are picked and passed on to 0x4111B0), so
      a lamp at the foot of a tall model is below the point it is lit from. Mean colours in the frame, the
      original / before / now: the tower's top 80, 98, 159 / 110, 121, 161 / 81, 100, 160; the tower's body
      26, 33, 61 / 18, 25, 49 / 25, 32, 60; the sandbags 74, 79, 71 / 126, 116, 76 / 74, 77, 66, then
      90, 88, 68 / 112, 106, 75 / 89, 88, 68, then 79, 82, 67 / 77, 79, 67 / 79, 82, 67; the roof unchanged and
      right. (The "corner markers" of updates 144-150's notes on this frame are the Command Post's sandbag
      parts 909-913, not an unfinished building's markers.)
- London's brighter Big Ben and Parliament in the original are its lorries' headlights: they stand beside the
  tower there and to the south in the engine. That difference is the deployment spot's, not the lighting's.
- Stage 2.7 looked into: the zoomer and the battle-end box are in; the deployment is only run as a battle is
  entered, so "reinforcements arriving during a battle" isn't a routine to port.
- MODERN.md: group keys and Shift adding to the selection noted as modern options for later (the owner's word:
  they were assumed from the genre, the game has neither).

**Update 152** (Stage 2.8)
- [x] 0x80B1 to the computer player (0x445D60), the one battle AI message not ported. In a unit's look round
      (0x496570), an unarmed object of the other side that it sees is told to the AI player when the looker is
      of that player's side (0x49666F: 0x4406E0 of the looker against dword_4E9B78's +0x24); the player makes the
      object's war map object a threat, active, unless it has 16 or it is one already, and plans nothing there
      and then. (The engine's note had it the other way round: "a player's unit seeing an unarmed one".)
      Not seen running: neither test battle has the two sides in sight of each other.

**Update 153** (Stage 3.1: repair)
- [x] A battle unit's health is kept. It is BAMO's fifth value in the save (0x489F30 writes the heading, the
      place and +0xC4); a battle now starts each unit with it and hands it back as it ends
      (`SaveObject::BattleHealths`, `BattleMap::UnitHealths`). Parliament's reads 17999 in the test save, its
      type's full health as the engine works it out (0x47DA20), which checks both.
- [x] Repair on the war map (0x4297C0, 0x429B80, 0x422AB0): the Mobile Repair Vehicles (types 30, 31) mend the
      most damaged construction or resource of their side in their sector, the workshops (60, 121) the most
      damaged unit; 10, 20 or 30 health for every 24 hours x the sector's supply ratios; the repair icon (GType
      0x76 frame 0x95, 0x4299C0) on the counter. WARMAP.md "Repair" has the rules.
- *Provisional / not yet*: 0x8124 (taken as "finished"); the icon's sharing of the corner with the production
  icon and its power bar; a dead unit of a surviving object still comes back in the next battle. Not seen in a
  game (needs a repairer and a damaged unit together); exercised with a stand-in only.

**Update 154** (Stage 3: counters and tool tips)
- [x] The strength pips under a war map counter (0x42B477, 0x42BADB): one for each battle unit of its object,
      by its health, highest first; not for the class 7 counters. Seen in the engine's own frame (five under
      the lorries, one under a single unit); the row's height is the battle icons', not checked against a war
      map frame of the original.
- [x] The task lines of a unit's and a finished construction's tool tip (0x42901D, 0x42A5EB): Currently
      building / Days to completion / Efficiency while its factory is busy, Construction suspended while it
      isn't. By the listing; not seen (a tool tip needs the mouse held still on a counter).
- Looked at and already in: the factory's context buttons, Suspend / Resume building tasks, CommandCentreBuilt
  (TASKS.md and PLAN.md had them as missing).

**Update 155** (Stage 3: the Resource view, the popup, facilities)
- [x] The Resource view's tool tips (0x426AE0, 0x426430): the resource's name, the sector's spread supply and
      spread demand, and what one more facility would add.
- [x] The popup asks the counter under the mouse first (0x41D1A4, a player's counter answering 0x8111), then the
      player's current object; the engine went by the selection alone.
- [x] A damaged resource facility makes nothing (0x423B00's health / full health as whole numbers), now that
      the health is kept.
- The notes that two events' boxes weren't in were stale: `RunEvent` opens them. (This entry also said the
  popup had no Information item. That was wrong; see update 156.)
- Seen while checking the kept health: in the Human start save 15 of the 116 battle units, all Martian
  constructions of types 113 and 116, are saved under the full health the engine works out for their types
  (1439 of 2159 and 1259 of 1439); every other unit is at exactly its type's. They are left as saved, so they
  start a little damaged (and a Martian Repair Facility would mend them): either the start save was made with
  older figures or the engine's full health for those two types is high. Not settled.

**Update 156** (from the owner's two frames of the original: the battle popup with Information, and the page)
- [x] **Correction to update 155: the popup does have Information.** An object's own popup routine (0x429A70 on
      the war map, 0x47E1E0 on the battle map) isn't the whole of it: its handler hands on to WSelectableObject's
      (0x440300), which adds Information (0x808C, icon 0x2A, text 0x478) for a type with an information page
      (+0x18). Update 155 read only the first and concluded the item didn't exist. Lesson kept in HANDOFF.md:
      before saying the game lacks something, follow the handler's fall-through, and ask the owner.
- [x] Information is in both popups, after the object's other items, and opens the page (0x4403A0, "InfoPage"
      0x4A3E60): the text wrapped at 256 in font 0x73 at level 15, a black box round it, centred 32 right and
      down of the object, closed by any key or button let go. The engine's battle popup for the lorries shows
      the frame's four items; the page's text colour is the frame's exactly (132, 121, 99).
- *Open*: in the frame the word gaps are about twice the engine's and the lines break earlier (same glyphs):
  likely the English release's font 0x73 has a wider space than the Italian install's the engine was run on.
  The page's speech (0x414700, 15 updates on) isn't in.

**Update 157** (from the owner's frame of the original: the war map menu on a unit in transit)
- [x] **"Cancel move" was in already, under the wrong name.** It is the object's 0x80A9 item (0x429B08, icon
      0x350, text 0x477), shown for a unit on its way between sectors; the notes called it "Retreat". Renamed.
- [x] **Who the menu asks (0x41D120), followed to the letter.** A counter of the player's under the mouse
      answers and nothing follows (var_11 set at 0x41D287): no "View map", and the sector under it isn't
      selected. Only after the current object's items (0x41D1F8) is the world's selected sector still asked.
      The engine added "View map" after a unit's items and selected the sector under the counter.
- [x] **A counter becoming selected clears the selected sector** (0x42C080 -> 0x419260(0)): the sector's bold
      outline and the mini-map's frame go, as in the frame.
- [x] **A unit in transit can be selected.** 0x42C160 has no test for it and the frame shows one with its marks;
      the engine dropped such counters from the selection every update (untraced). The route preview passes over
      them as before.
- The scripted run now gives the frame's picture: the marks on the counter, the thin sector outline, Cancel /
  Cancel move / Information, the route's arrow translucent with the mouse off the counter.
- *Open, seen in the same frame*: the mini-map shades the sectors by owner (the engine's is plain); the counter
  at the route's far end sits about 18 left and 13 up of the engine's. Both for update 158.

**Update 158** (the mini-map panel, from the same frame of the original)
- [x] **The mini-map is shaded by sector, as the map is.** It is a map sprite through its own sector map (0x82,
      0x422D4C) and the levels are the global table the big map uses (0x49C000). The engine drew it plain and
      took 0x82 for a highlight picture.
- [x] **The panel sat 5 pixels too far right (Human), the view 11 (Martian).** The frame places itself (the
      parent's width less its own less 4, 0x42371A) and the world then moves the view to the frame's x + 9 / + 10
      (0x418C35 / 0x418C70); the engine placed the frame from the view.
- [x] **The view's outline**: one pixel smaller each way (it is the rectangle, 42 x 32), one further in (0x423331),
      held 2 inside the far edges, and its colour through the 16-bit screen (0, 0, 189). The rubber band's two
      colours go through the same.
- Against the owner's frame the mini-map's 17,000 pixels, outline included, are identical.
- *Open*: the panel's frame, buttons and the calendar are the same shapes in the same places but not the same
  pixels: the original's are smooth where the engine's (from the Italian discs' `hmframe.spr` etc.) are dithered,
  and the frame has a black drop shadow the engine's copy has as transparent. No palette slot or level turns one
  into the other (one index of the engine's picture lies under several colours of the original's), so the
  picture files differ. Asked of the owner: whether their install's files are the same as the Italian discs'.
- *Open*: the Martian panel's new place is from the listing alone.

**Update 159** (from the owner's frame of the original's Martian war map, a new campaign, Enhanced Assets off)
- [x] **Closed: the panel art of update 158.** The Human frame was taken with WoWRevived's Enhanced Assets on
      (sprites scaled up and back down); nothing was wrong with the engine's. Noted in MODERN.md as an option.
- [x] **The Martian panel's place is confirmed**: frame 515, 198 and view 525, 306 match the frame.
- [x] **Placed counters sat 16 right and 16 down.** A cell's point is the object's point (0x4406A0 at 0x420875),
      the middle of its counter; the engine put the counter's corner there. All four frames to hand agree
      (this was also update 157's "counter at the route's far end").
- [x] **Where the view starts**: centred on the selected sector's bounds (0x4189B9), 150, 324 for a new Martian
      campaign; the engine used the save's 159, 298.
- [x] **The current view's button** shows frame 3 (held, the mouse elsewhere), 2 under the mouse (0x433DB0); the
      engine showed 2 always.
- With these the engine's frame of a new Martian campaign differs from the owner's only in the clock (theirs had
  run to 0001 01), their mouse pointer, and the first pod.
- *Open*: **where the first pod lands.** In the owner's frame, just past the first day, a Martian counter stands
  alone in sector 2 (south-west of the base, no one's before). The engine's first pod lands in Sector 10, a Human
  sector, and asks to attack. Asked of the owner: whether the first pod always lands there.

## Deferred (with the reason each is still open)

Each needs either something the engine doesn't have yet, or tracing the disassembly hasn't covered; none is a
known visible bug in the two test battles.
- battle-ui 12: cursor modes 2-7 (the editor / placement tools, not in the engine).
- deployment: how the deployed units end up facing (update 145); the update order as the
  world list's (HUNI, HCON, HRES, AUNI, ACON, ARES).
- movement 21 (the rest): a bridge broken but standing (needs a saved battle to arise).
- warmap 2 (part): the repair icon and power bar (repair isn't in). warmap 14: units on a route restart after a load
  (done, update 137). warmap 20: the battle pulse's own draw
  counter (the same at a steady 30 fps).

The architecture items, by report:
- Settled by Phase 1: rendering A1-A3, battle-ui A / B, movement A1 / A2 / A4, combat A1-A3 / A5, battle-setup A-C,
  data A (tree circles' children: their lifetime is still open, see S6).
- Still as they were, by choice for now: movement A3 (the route finder is a function, not a stateful singleton),
  movement A5 (bridges are scenery, not objects that can break), combat A4 (hits take ints; Hit now takes the raw
  double), battle-ui C (the screen point is computed when needed; it's the same value), battle-ui D / warmap
  (popups: rows now, but still a flat list of items, not child windows), warmap A (one `Counter` for the original's
  counter and its objects), warmap B / C (views and drawing are immediate, not child objects and a depth-sorted
  display list), frontend 1-3 (the Fullscreen View, controls and the cursor as one Screen / Button / int), data B-D
  (OBJ.ojd parsed by several loops, modifiers applied at use, saves carried as blobs found by tag).

## Original bugs to keep

- 0x43D3F0 checks room in the sector of the table slot one past the last, not the nearest point's (battle-setup 8).
- The group's starting stance is read from uninitialised memory (battle-setup, provisional 8): kept as Retaliate.
