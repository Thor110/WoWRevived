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
      Open: tree circles (0xCF-0xD4, no graphics, +0x44 48 / 24 / 12) keep their trees as children (0x48F410); whether
      a circle can be destroyed and take its trees with it needs the children's lifetime traced (0x47EAA0 and the
      circle class's vt+0x78) before the engine models it.

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

**Still to do**
- [ ] warmap 1, 2, 4-12, 14-20 (the resource view, counter icons, the mini-map, edge scrolling, the rubber band,
      route arrows, messages, the status strip, dimming, hover, transit after a load, the saved view mode, the menu's
      View map, the ending, unit view shading, the first-hour jump, the battle pulse).
- [ ] battle-ui 9-12 (edge-scroll margin in a window, a ground click with no point, the cursor's modes).
- [ ] combat 10, 13-22 (the beam glow's frame, light rolls, dying units, the firer's sparing, removal timing,
      underground targets, emplacements, gun timing, precision, cascades, fire timings).
- [ ] movement 17, 19-21 (the route search held off, the second target, Muster's first step, broken bridges).
- [ ] battle-setup 7, 12, 13, 16, 18 (the planner's strongest object, fog timing, group costs from all members, the
      staggered deaths, the counts' lists).
- [ ] rendering 9, 12, 13 (the gun flash level, a WOF's piece order, view list ties).
- [ ] frontend 9 and behaviour 1-8; data 4-13; the architecture items.

## Original bugs to keep

- 0x43D3F0 checks room in the sector of the table slot one past the last, not the nearest point's (battle-setup 8).
- The group's starting stance is read from uninitialised memory (battle-setup, provisional 8): kept as Retaliate.
