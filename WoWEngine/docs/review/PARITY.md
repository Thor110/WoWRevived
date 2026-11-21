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
- [ ] **S4. Group state as the original keeps it.** +0x1D8 bits 0-9 (3/4: a player's order to a player's group, which
      gate the move marker and "acknowledged" speech), arrival counts on the group, not a growing `orders` list; the
      selection as a group of its own (0x439000 / 0x4480F0). [battle-ui 2, B; movement 2, 6, 9, 15, 16, A2;
      battle-setup B, C]
      *Update 128 (part)*: `groupOrdered` (bit 3, set by the player's 0x0E, 0x10 and 0x13) and `PlayersOrder` (bits 3
      and 4 and the first member the player's): the move flag and "acknowledged" only then (the flag also for an
      attack's approach; the speech without a target), bit 3 cleared after; ROUTE NOT FOUND stops the members, drops
      the target, clears bit 0 and speaks only for a player's order; arrivals per group (+0x56, reset at the group's
      count, so they carry over between orders); the move's counts from the whole group (0x4216D0). Left: the
      selection as a group of its own (0x439000 / 0x4480F0) and the rest of +0x1D8.
- [ ] **S5. Per-object updates in start order** (the world list, 0x43A980 / 0x47D610): each object does its whole
      update in turn, instead of the engine's passes (all driving, then all guns...). Look-round timers start at
      rand() % 30 (0x4957A2). [battle-setup 4, 14, 15, A; combat A5]
- [ ] **S6. Parent / child objects for parts.** Guns, turrets, building parts, lights and tree circles as children
      with a parent link and offsets, removed from the parent's list as they go. [combat 4, A1, A3; data A]

## Phase 2: high-severity behaviour (small, independent)

- [ ] Units never pick targets: the look-round scale `flt_59E44C` is `[Tweak] "Audible radius multiplier"` (1.0), not
      0. [combat 1]
- [ ] Air-burst table chosen by impact height over 64, not weapon size (0x487F50). [combat 2]
- [ ] A building's lost part fires as its building (0x489FA0), so it doesn't hurt it. [combat 3]
- [ ] Selection marks at ground + lift + the type's +0x41 (0x47F480 / 0x4406A0), not the feet. [battle-ui 4]
- [ ] Scenery sprites (street lamps, ~49), reinforcement markers (63) and building corners (45) at their levels, not 0.
      [rendering 4]
- [ ] Landscape tint and red weed from the Martian constructions and resources (0x43981C). [battle-setup 1]
- [ ] Human command building 0x3A, not 0x13A, in ReinforcementSources. [warmap 3]
- [ ] Destroyed constructions leave `finished[]`. [warmap 13]
- [ ] Menus open with the back button focused (Quit: Enter mustn't quit). [frontend 1]
- [ ] A vehicle arriving on a bridge clears its link (0x491A9C). [movement 4]
- [ ] Mines: the collision filter's test the right way round (0x4929A0). [movement 8]
- [ ] The pod lands before deployment (0x439AE5); a forfeit returns to the war map 150 updates on (0x43AA18).
      [battle-setup 2, 3]

## Phase 3: the rest

Every other item in the eight reports, by severity, area by area: the popup's rows (battle-ui 6-7), fogged enemies
not clickable (battle-ui 5), the war map's resource view, counter icons and mini-map (warmap 1, 2, 4-12), the menus'
layout and fades (frontend 2-9), effects hugging the ground (combat 5), smoke timing and fog (combat 6-7), route
handling (movement 9-23), the reinforcement checks (battle-setup 8-10), the sound variations and bridge rand()
(data 1-2), and the latent data items.

## Original bugs to keep

- 0x43D3F0 checks room in the sector of the table slot one past the last, not the nearest point's (battle-setup 8).
- The group's starting stance is read from uninitialised memory (battle-setup, provisional 8): kept as Retaliate.
