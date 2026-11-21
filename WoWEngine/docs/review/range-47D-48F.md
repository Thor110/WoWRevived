# Review: 0x47D000-0x48FFFF (battle map objects: object base, WBMGroup, battle screen, effects, object classes)

**431 routines: PORTED 118, PARTIAL 28, MISSING 102, LIBRARY / structural 183.**
Port paths are relative to `WoWEngine/src/`. Class identities come from the vtables and the factory 0x446B30, cross-checked with `docs/survey/part10-12.md`.
"Structural" means base-class plumbing that the port replaces with its own data model (`Unit`, `Prop`, `Effect`, `Light` in `battlemap/BattleMap.h`). Those routines have nothing of their own to port.

## Missing

### Victory, defeat and destruction notices
- **0x48AC50** (vt+0x78 of every damageable object, OnDestroyed). The battle's key building is the Martian Base 0x33E (or 0x78) for kind 3, and Parliament 0x116 (or 0x13A) for kind 0x1E. When it is destroyed, this routine sends **0x80AA**(is the player's) to the battle owner (0x48ACCA). For kinds 3 and 0x1E it also sets `byte_4E9B94` (the Debriefing's forfeit/DEFEAT flag, see `frontend/Briefing.h:69`) unless `[0x4E9B7C]+0x54`, and sends **0x8136** "DESTROYING VEHICLES" (0x48AD0E). The port's `Die` (`battlemap/Combat.cpp:60`) only plays the 0x112/0x13B speech, and `CheckBattleEnd` (`BattleMap.cpp:1624`) only counts units. Task: implement 0x48AC50's 0x80AA/0x8136/forfeit path, then trace the 0x80AA and 0x8136 handlers.
- **0x48AD40** (0x812F to the owner side's list), **0x48D590** (bridge destroyed: 0x807B to both players, the war map sector record 0x42A0B0), **0x48DA40** (fence destroyed, plus the 0x43C750 resource recount), **0x486850** (route node removed with 0x494370). Together these are broken bridges and fences. Task: destroyable bridges and fences with their war map and route-finder updates.

### Object classes with no port
- **Buildings** 0x48E8A0 / 0x48E900 / 0x48E990 / 0x48E9D0 / 0x48EEE0 / 0x48EF80 / 0x48F030 / 0x48F130 / 0x48F1C0.
  - AddToWorld sets health from the construction state, flattens and samples the ground under the footprint, places four 0x17B corner objects and starts the ambient loop sound (unit table +0xA2).
  - TakeDamage drives the damage picture frame through 0x48A7D0 (**0x48A7D0** is also missing).
  - The health fraction is offset by -0.1.
  - An explosion hit plays sound 0x111 when rand() < 500.
  - Each tick has a 1% chance to restart the ambient sound; hiding or showing the building stops or restarts it.
  - In the port buildings are plain non-vehicle `Unit`s. Task: a building class with these behaviours.
- **Gun emplacement** 0x48F7C0 (vt+0x14). When the emplacement's root is not a unit (it sits on a building) and the root's health ratio is below flt_4AFD2C, its weapon children are removed and +0x3C is set to 0 (0x48F81D-0x48F873), so it has no guns. Task: implement this in SetUpGuns / AddToWorld.
- **Drone** 0x48F9D0 / 0x48F9F0 / 0x48FA80 / 0x48FAA0 / 0x48FAC0 (types 0x5D-0x5F). Within 64 (flt_4AFEA0) of its target (+0x190), it posts 0x80A0 to itself one tick later (0x48FA47), so it explodes on contact. In the port it drives as a generic vehicle. Task: drone class.
- **Tripods / walkers** 0x48FC70 / 0x48FCB0 / 0x48FDD0, with the `[Vehicles] startTurnRate`/`turnAnimRate` settings at 0x48FB60 / 0x48FBE0. They cover leg animation rate and footprints. Each type has its own list of joints that leave footprint object 0x3C7 on terrain types 1-4. Task: walker class with footprints.
- **Pod** 0x48F5F0 / 0x48F620 (type 0xB3). It lands at a random passable spot (0x47B520) and spawns effect 0x3DA twice with size 300. `BattleMap.cpp:442` says pods are not yet made. Task: battle pods.
- **Line-linked objects** 0x48B590-0x48C650 (13 routines; types 0x128, 0x1D0-0x1D3, 0x2B6/0x2B7).
  - Each draws 3-4 coloured line primitives to a linked object (0x48BE30, 0x48B700) and plays a loop sound (0x48C650).
  - Their minimap colour (**0x48C870**) is flagged "not yet" in `BattleMap.cpp:197`.
  - Task: implement after identifying the types (likely wires or telepath links).
- **Fences and walls** 0x48D780 / 0x48D800 / 0x48D9A0 / 0x48D9B0 / 0x48D9C0 / 0x48DAA0 / 0x48DB80 (types 0x1C3-0x1C8).
  - Segments every 1/64 of the length.
  - Death passes to each segment one tick apart.
  - Electric types spark with 0x17B.
  - Segments are drawn on the minimap.
  - `core/Scenery.h` loads only scenery, trees and bridges from `.nsb`. Task: fence class.
- **Two-point structure build-up** 0x4865A0 / 0x4866E0. Visible segments follow the health ratio, plus the messages 0x80A5-0x80A7 / 0x80D1 / 0x80D2 that set the ends. **Military bridge placement** 0x48D5F0 is also missing. Task: with construction in battle.
- **Ambient-sound scenery** 0x4890A0 / 0x489170 / 0x4891F0 / 0x489240 / 0x489290 / 0x4892C0 (factory cases 940, 994-996).
  - The looping sound follows the object's screen point and has a 100/32768 restart chance each tick.
  - It also turns into type 0x3AC with probability (count-3)/7. That substitution consumes a rand() that the port does not consume, so the scenery's rand() sequence differs; `core/Scenery.h` already notes the order is provisional.
  - `BattleMap.cpp:772` says "not yet". Task: ambient scenery.
- **Vehicle shadows** 0x48E6D0 / 0x48E700 / 0x48E740 / 0x48E750 / 0x48E850 (types 0x13B-0x14A). Each follows its parent at height/12 - 6, darkness flt_4DA770, and is gated by `[Options] Show vehicle shadows`. The option exists (`frontend/FrontEnd.cpp:324`) but nothing draws the shadows. Task: shadow objects.
- **Projectile ground shadows** 0x4884C0 / 0x488570 / 0x4885C0 / 0x4885F0. A 0x44-byte shadow sprite sits at the ground point under each shell. Task: draw shadows for shells and rockets.
- **Gas cloud / Black Dust** 0x488CC0 / 0x488D20 / 0x488D70 (types 915-917, lifetimes 240/480/720 for 0x393-0x395). It drifts with 0.65 damping, stops under 0.25, floors at height 10, spawns its +0x98 child puff every 15 ticks, and dies over land. Task: WGasCloud.
- **Cascading death explosion** 0x48CDE0 (types 0x252-0x255). At animation frame 11 it spawns the next smaller one (0x255 → 0x254 → 0x253 → 0x252) at a random offset of up to 8. `IsExplosion` (`Combat.cpp:25`) knows the types but not the chain. Task: in UpdateEffects.
- Small classes:
  - 0x48DC30 / 0x48DC60: types 0x19A-0x19C, vt+0xE4 0x495280, vt+0x14 0x494AF0.
  - 0x48D000: type 0x1C0 brightness.
  - 0x48F2E0: street lamp brightness from +0xA1. The lamps are drawn, but this shade is not applied.
  - Task: check these when the classes come in.

### Fog of war and object shading
- **0x48B050** (vt+0x58: fog visibility sampled at the object's 4 corners), **0x48B2A0** / **0x48B2D0** (vt+0x5C: brightness, and enemy units hidden or shown by `[BattleMap] "Fog darkness"`), **0x48C570** / **0x48C840** (vt+0x100 reveal, every `"Fog update freq"` ticks), **0x489750** (explored-map test).
- **0x47FEB0** / **0x4897D0**: shade factor from the per-cell shadow map onto sprite brightness.
- TASKS.md lists fog as still to do, and `BattleMap.cpp:198` says every unit shows. Task: battle fog (with 0x4346C0 / 0x434A80) and object brightness.

### Groups and orders
- **0x4821F0** (0x8058, the target destroyed or changed class). Speech **0x10E** when the target was a building (class 2/3; 0x482249). Speech **0x10D** when it was a unit that was its war map object's last (0x4216D0 == 1; 0x4822EC). The leader's portrait flashes, the members get 0x80B9 and 0x805A. The port just clears the target (`Weapons.cpp:554`) without either speech. Task: port 0x4821F0.
- **0x483760** (WBMGroup vt+0x38, vtable slot 0x4AD740) re-engages the target or stops when it is gone. The port says "vt+0x38 isn't in" in `BattleAi.cpp:179`, `Weapons.cpp:316` and `Weapons.cpp:493`. Task: port vt+0x38 and call it where 0x483B70 / 0x481F70 / 0x483880 do.

### Battle screen UI
- **Unit icon tray and icons** 0x483DF0 / 0x483E80 / 0x483F90 / 0x483FF0 / 0x484070 / 0x484120 / 0x484240 / 0x4842E0 / 0x4844A0 / 0x484840 / 0x484920 / 0x4849D0.
  - The icons are laid out in rows.
  - A click selects (packet 4, then 0x8088) and a double click within 10 ticks centres the view.
  - An icon blinks for 30 ticks while its unit speaks (the port cites 0x4842D0 only as a comment).
  - It has its own tooltip.
  - Task: the battle icon tray ("the panel" in TASKS.md).
- **0x487580**: a retreat marker's tooltip "Retreat to" (0x518) plus the sector name. The battle map's `TipText` (`BattleMap.cpp:1477`) covers only the popup. Task: object tooltips (0x807C), including 0x489FA0's name tooltip.
- **0x487490**: a retreat marker's arrow frame 0-3, chosen from the nearest map edge, with sprite y offset 500 and +0x24 = 1. The port always draws frame 0 (`Reinforcements.cpp:53-56`). **0x487380**: retreat markers' minimap dots in colour dword_4DAFB4, "not yet" in `BattleMap.cpp:197`. Task: both.
- **0x4861E0**: battle music. It alternates CD tracks 4 and 5, plays a track when it is over 10 s, then re-posts 0x813C after the track's length; otherwise it re-polls every 10 s. Nothing in `battlemap/` or `frontend/` does this (FrontEnd only has the war map's 0x813C, `FrontEnd.h:136`). Task: battle music.
- **0x489980**: turret attach. A damaged non-unit parent hides its child parts that have flag bit 11. The turret's initial heading is the parent's heading plus the hardpoint base. +0x192 = 1 unless the parent's kind is 4-9. Task: with turrets on buildings.

## Partial / divergent
| Addr | What differs | Port | Task |
|---|---|---|---|
| 0x4802E0 | The port computes the ground (terrain and slope costs, airborne, own-sector, +0x5A) but not three values. **+0x54 stance** = ftol((Σ members' +0xD8 + 0.5)/n) (0x48058F, dbl_4AD750 = -0.5); the port fixes it at 1. **+0x5C think interval** = average of type word +0x92 (0x480533, 0x4805E1); the port uses the constant 90. **+0x1D0** average war map speed. Bits 8/9 (category 5/6, 1-4) and bit 4 are also left out. The terrain and slope part matches. | `BattleMap.cpp:90`, `BattleAi.cpp:184`, `Driving.cpp:634` | Compute the stance and interval averages (see Bugs) |
| 0x483880 | Ignores hits while +0x1D8 bit 0 is set and bit 2 is clear; see Bugs | `Weapons.cpp:494-501` | — |
| 0x483B70 | The "busy" test in the original is only +0x1D8 bit 0 (0x483BD7, 0x483C48); the port also counts having a target. The member count and centre include every member (+0x24); the port uses vehicles only. vt+0x38 (0x483760) is not called. | `BattleAi.cpp:180-207` | Use bit 0 semantics once bit 0 is modelled |
| 0x482E60 | Missing: "CAN'T GET TO WITHIN ATTACK DISTANCE!" → 0x4833F0, the re-post of 0x804F after 10 ticks while another group's search runs, and the search spread over updates (limit 200 a tick). The header comment's "flag/speech aren't in" is stale: 0x483590 / 0x10A / 0x10B are in. | `Driving.cpp:35-45`, `Routes.cpp:248` | Finish 0x482E60 |
| 0x4826D0 | Under the ground the original also clears bit 0 and +0x56 (0x482770-0x482799). The attack-distance check is not in. | `Weapons.cpp:554` | Minor |
| 0x481F70 | 0x80B9 to members and vt+0x38 not in | `Weapons.cpp:317` | With 0x483760 |
| 0x4812B0 | Packets 0x0E / 0x10 / 0x11 / 0x13 / 0x15 are in. There is no packet or record layer (playback and network). | `BattleMap.cpp:1045`, `Driving.cpp:72` | With Players (Milestone 6) |
| 0x485930 | Box select and orders are in. **Build placement (cursor mode 3)** is missing: kinds 0-4, the two-click walls for types 0x1C3-0x1C8 / 0x128 / 0x1BC-0x1BF, packet 0x18. | `BattleMap.cpp:241` | Battle construction |
| 0x485F60 | Cursor modes 4-6 (building ghost 0x4A13B0, drag line 0x4A03A0) are missing | `BattleMap.cpp:336` | With build placement |
| 0x484FB0 | The battle menu, pause and SFX duck are in (FrontEnd). The 0x3ED dialog, 0x8123 music restart and 0x813C are not. | `frontend/FrontEnd.cpp:693` | With music |
| 0x47DF20 | 0x805C (orders) is in. Missing: 0x111/0x813E (in-battle build queue reset and suspend), 0x8066 (lit renderer flag), 0x80D1 / 0x80A7 | `BattleMap.cpp:1301` | — |
| 0x47E810 | 0x8130 query (0x8131 mobile, 0x8132 armed, specials). Only its logic inside AddOrders is in; the combined panel 0x448500 that asks it is missing. | `BattleMap.cpp:1298` | With the panel |
| 0x47F340 / 0x47F850 | Child parts should be posed on transformed WOF model vertices or hardpoints. The port uses the unit point plus a rotated offset (*Provisional*). | `BattleMap.cpp:642`, `Weapons.cpp:131`, `Weapons.cpp:751` | Pose parts on model vertices |
| 0x489B40 | The turret aims from its own position (+0x78 / +0x7C, 0x489BA0) and checks vt+0x9C before starting its sound (0x489C93). The port aims from the unit's point and has no vt+0x9C check. Rate 50, arc and sign match. | `Weapons.cpp:132-163` | With parts posing |
| 0x488200 | The original queries every object (0x47FC50) and always spares the firer object itself (0x4882A8). The port hits units, then props, and spares the firer's group unless Turned, so a turned firer can hit itself. | `Combat.cpp:121-133` | Minor |
| 0x48AB20 | The first drop under 40% of max should add the object to the world's damaged list (0x440060); this is not done | `Combat.cpp:43` | Find the list's reader |
| 0x48AEF0 | +0xC0 bit 3 → 0x80C1 to the owner's +0x6C ("under attack" alert) and vt+0x9C are not in | `Effects.cpp:259` | — |
| 0x48E100 / 0x48DF50 | 0x8066 to objects under a light is not in | `Effects.cpp:228` | — |
| 0x48DE80 | The daylight roll runs only for scenery lights; unit headlights (parts 334-346) are not made | `BattleMap.cpp:838`, `Effects.cpp:194` | Unit lights (TASKS "Reported") |
| 0x489FA0 | 0x80A0 death and speech are in. Missing: 0x807C tooltip, 0x8125 / 0x8126, the 0x111 rect tests through 0x48A4C0 / 0x48A530 (done by the port's own selection) | `Combat.cpp:55` | Tooltips |
| 0x48A660 | Power bars are drawn by the port's own code; the WPuppet and 0x8125 to children are not modelled | `BattleMap.cpp:225` | — |
| 0x48A830 | IsTargetable (vt+0xF0): type 0x3DB is always targetable; classes 1-3 or the cheat byte_4D1249. The port has no single equivalent and ignores the cheat. | `Weapons.cpp:317` | With cheats |
| 0x4889D0 | The shell starts at the unit's point, not at the mortar's place on it | `Weapons.cpp:751` | With parts posing |
| 0x4886A0 | The research multipliers (+0xC8 / +0xCC via 0x494AF0) are not applied | `Weapons.cpp:672`, `Weapons.cpp:817` | — |
| 0x48CC00 | The mine marker is in; the other 0x80F9 effects (0x273-0x276, bridges' 0x498110) are not | `Weapons.cpp:266` | — |
| 0x487890 | The parent's vt+0x9C "frozen" check is not in | `Effects.cpp:85` | — |
| 0x47D6B0 | `[Tweak] "ISO obj col rad mul"` is hard-coded as 0.66, not read from the ini | `BattleMap.cpp:632`, `BattleMap.cpp:904` | Read the setting |

## Bugs
1. **Groups retaliate or flee while carrying out a player's move order.**
   - Original: 0x483880, 0x48389E-0x4838AD. If +0x1D8 bit 0 (moving) is set and bit 2 is clear, the hit is ignored.
   - Every move order sets bit 0 and clears bit 2: 0x482510 (`or al,1` then `and al,0FBh`) and 0x480EB0. Only 0x483B70's own periodic roam or home move sets bit 2 (0x483DCB).
   - Port: `Retaliate` (`Weapons.cpp:501`) returns only on `groupBusy`. `OrderMove` / `OrderGroupMove` (`Driving.cpp:46`, `Driving.cpp:102`) never set it.
   - Result: a player's group on the move takes the attacker as its target (stances 1-3) or runs off 400 units (Avoid), where the original keeps driving.
2. **Group stance and think interval are fixed, not averaged.**
   - Original: 0x4802E0 runs for every new group (0x429FF0 → 0x47CFE0 → 0x4802E0). It sets +0x54 = ftol((Σ +0xD8 + 0.5)/n) (0x48058B-0x4805A2) and +0x5C = Σ type +0x92 / n (0x480533, 0x4805B5-0x4805E1).
   - Port: `groupStance.assign(..., 1)` (`BattleMap.cpp:90`) and the constant `count <= 90` (`BattleAi.cpp:184`). The members' own stance is loaded from record 56 (`BattleMap.cpp:74`) but never averaged into the group.
   - Caveat: whether +0xD8 is already set when 0x47CFE0 runs (it is set by 0x495740, the vt+0x14 start) depends on call order inside 0x4216E0, which I did not trace. If it is 0, the group stance would be 0 (Avoid) for one-member groups. Confirm this before changing the code.
3. **Survey error, not a port bug.** `docs/survey/part11.md` §2.9 says 0x487F50 uses water → 0x4D0450 and land → 0x4D0340. The code (0x487FB2-0x487FE3) uses size > 64 → 0x4D0450, terrain 1-3 → 0x4D0340, and everything else → 0x4D0230. The port's `ImpactAt` (`Weapons.cpp:831`) is correct; the survey line should be fixed.
4. **Stale comment.** The *Provisional* note at `Driving.cpp:44-45` says the destination flag 0x17C and speech 0x10A / 0x10B "aren't in", but `Driving.cpp:155-187` implements them.

## Library / not needed
**183 routines.**
- **Containers (11):** WObjectList ops 0x47D0B0-0x47D590 (push, for-each, remove-all, clear-positions); the port uses `std::vector`. Also 0x47D590 (Shift select-all for the war map).
- **Structural (~45):** the object-base plumbing that the port's data model replaces.
  - 0x47D6F0 (ctor), 0x47EAA0 (Remove), 0x47F220 (OnChildRemoved), 0x47F260 (Shutdown), 0x47F570 (ClipTest), 0x47F630 (Draw), 0x47FAC0 (SetHeading), 0x47FB20 (SetSelected), 0x47FBA0 (CollectInRadius; the port's `Gather` / `BucketCells` cover it).
  - Grid add/move/remove: 0x48A9F0, 0x48AA30, 0x48AA80.
  - BAMO save/load: 0x489ED0, plus 0x486380 / 0x486420 / 0x489680. Saves are carried as raw bytes by `core/SaveFile`.
  - 0x484A30-0x484F00 / 0x4855F0: battle screen ctor, dtor, view and paint. The port has its own `Screen`.
  - 0x487610-0x487AF0: pool allocators.
  - 0x4880F0 / 0x488400: spawn helpers.
  - 0x4873F0 / 0x487460: retreat-marker registration; the port keeps `rpoints`.
  - 0x4824A0: disband, vt+0x34. I found no caller (unclear).
- **Trivial (~127):** ctors and dtors (0x487B40, 0x48B480, 0x48CDD0, 0x48E680, 0x48F290, ...), thunks and forwarders (0x4802D0, 0x489940-0x489970, 0x489D30-0x489DA0, 0x488D10, 0x488EF0), constant returns (0x47D960 / 0x47D970 / 0x47D980, 0x47FFB0 = 10, 0x4898F0 / 0x489900, 0x48FB30-0x48FB50), flag setters (0x47D940, 0x47FFC0, 0x489E00), and static stats inits ("BM Groups" 0x480150, "Scenery objects left" 0x489340).

## Ported
- 0x47D030 → core/Scenery.h:38
- 0x47D240 → core/Scenery.h:3, core/Scenery.h:38
- 0x47D360 → core/SaveGame.h:35, core/Scenery.h:3
- 0x47D600 → battlemap/BattleAi.cpp:61
- 0x47D610 → battlemap/BattleMap.cpp:1222, battlemap/BattleMap.cpp:1614
- 0x47D660 → battlemap/BattleMap.cpp:1223, battlemap/BattleMap.cpp:1614
- 0x47DA20 → battlemap/BattleMap.cpp:392, battlemap/BattleMap.h:236
- 0x47DA80 → battlemap/BattleMap.cpp:1253, battlemap/BattleMap.cpp:1300
- 0x47E1E0 → battlemap/BattleMap.cpp:1297, battlemap/BattleMap.h:426
- 0x47EB00 → battlemap/BattleAi.cpp:61, battlemap/BattleMap.cpp:525
- 0x47EB10 → battlemap/BattleMap.cpp:612, battlemap/BattleMap.cpp:637
- 0x47EE10 → battlemap/BattleMap.cpp:639, battlemap/BattleMap.cpp:806
- 0x47EF30 → battlemap/BattleMap.cpp:1238, battlemap/Combat.cpp:84
- 0x47F050 → battlemap/BattleMap.h:145, battlemap/BattleMap.h:165
- 0x47F320 → battlemap/Sinking.cpp:61
- 0x47F480 → battlemap/BattleMap.cpp:911
- 0x47F770 → battlemap/BattleMap.h:145, battlemap/Driving.cpp:314
- 0x47F960 → battlemap/BattleMap.cpp:903
- 0x47FC50 → battlemap/Combat.cpp:158, battlemap/Combat.cpp:86
- 0x47FD40 → battlemap/BattleMap.cpp:526, battlemap/Driving.cpp:521
- 0x47FE10 → battlemap/BattleMap.cpp:526, battlemap/BattleMap.h:368
- 0x47FFE0 → battlemap/BattleMap.h:315, battlemap/Combat.cpp:89
- 0x480070 → battlemap/BattleMap.cpp:158
- 0x480190 → battlemap/BattleMap.h:430
- 0x480680 → battlemap/BattleMap.cpp:927, battlemap/BattleMap.h:384
- 0x480740 → battlemap/BattleMap.cpp:525
- 0x4807E0 → battlemap/Sinking.cpp:46, battlemap/Weapons.cpp:245
- 0x4807F0 → battlemap/BattleAi.cpp:64, battlemap/Weapons.cpp:485
- 0x480EB0 → battlemap/Driving.cpp:785, battlemap/Reinforcements.cpp:89
- 0x480EF0 → battlemap/BattleMap.cpp:1409, battlemap/BattleMap.h:258
- 0x481170 → battlemap/BattleMap.h:259, battlemap/Reinforcements.cpp:111
- 0x481770 → battlemap/BattleMap.cpp:1092, battlemap/BattleMap.h:170
- 0x4817C0 → battlemap/BattleMap.cpp:1401, battlemap/BattleMap.cpp:1409
- 0x481CA0 → battlemap/BattleMap.cpp:280, battlemap/BattleMap.cpp:287
- 0x4821A0 → battlemap/Reinforcements.cpp:88
- 0x482450 → battlemap/Driving.cpp:743
- 0x482510 → battlemap/BattleAi.cpp:174, battlemap/BattleAi.cpp:63
- 0x482570 → battlemap/BattleMap.h:280, battlemap/Weapons.cpp:289
- 0x482880 → battlemap/BattleAi.cpp:64, battlemap/BattleAi.cpp:83
- 0x482960 → battlemap/BattleMap.h:287, battlemap/Weapons.cpp:395
- 0x482B00 → battlemap/BattleMap.h:358, battlemap/Driving.cpp:127
- 0x483270 → battlemap/Driving.cpp:165, battlemap/Routes.cpp:247
- 0x4833F0 → battlemap/Driving.cpp:153
- 0x483590 → battlemap/Driving.cpp:168, battlemap/Driving.cpp:182
- 0x4836A0 → battlemap/BattleMap.cpp:353
- 0x483AE0 → battlemap/BattleMap.cpp:1402
- 0x4842D0 → battlemap/Reinforcements.cpp:87
- 0x484A30 → battlemap/BattleMap.h:1, frontend/FrontEnd.cpp:922
- 0x484DF0 → battlemap/BattleMap.cpp:1074, battlemap/BattleMap.h:3
- 0x485610 → battlemap/BattleMap.cpp:1007, battlemap/BattleMap.h:3
- 0x485D00 → battlemap/BattleMap.cpp:1059
- 0x485DE0 → battlemap/BattleMap.cpp:1130, battlemap/BattleMap.cpp:1209
- 0x4862C0 → core/Bridge.h:1
- 0x486490 → battlemap/BattleMap.h:341, core/Bridge.cpp:38
- 0x486820 → battlemap/Routes.cpp:119
- 0x4868A0 → core/Bridge.h:1
- 0x4868B0 → battlemap/BattleMap.cpp:155, battlemap/BattleMap.h:388
- 0x486CB0 → battlemap/BattleMap.cpp:1526, battlemap/BattleMap.cpp:196
- 0x486E80 → battlemap/BattleMap.cpp:334, battlemap/BattleMap.cpp:429
- 0x487730 → battlemap/BattleMap.h:140, battlemap/Effects.cpp:1
- 0x487A30 → battlemap/BattleMap.h:176
- 0x487B00 → battlemap/Combat.cpp:94
- 0x487B50 → battlemap/BattleMap.h:308, battlemap/Combat.cpp:157
- 0x487F50 → battlemap/BattleMap.h:295, battlemap/BattleMap.h:296
- 0x488070 → battlemap/BattleMap.h:305, battlemap/Combat.cpp:56
- 0x488180 → battlemap/BattleMap.h:141, battlemap/BattleMap.h:301
- 0x488350 → battlemap/BattleMap.h:305, battlemap/Combat.cpp:2
- 0x4883C0 → battlemap/Weapons.cpp:128
- 0x4883E0 → battlemap/Sinking.cpp:27, battlemap/Weapons.cpp:128
- 0x488410 → battlemap/Weapons.cpp:1, battlemap/Weapons.cpp:32
- 0x488460 → battlemap/BattleMap.cpp:1240, battlemap/Weapons.cpp:2
- 0x4887E0 → battlemap/BattleMap.cpp:1240, battlemap/Weapons.cpp:1
- 0x488830 → battlemap/BattleMap.h:218, battlemap/Weapons.cpp:3
- 0x488930 → battlemap/BattleMap.cpp:1239, battlemap/BattleMap.h:220
- 0x488CA0 → battlemap/BattleMap.h:218, battlemap/Weapons.cpp:669
- 0x488E80 → battlemap/BattleMap.h:143
- 0x488F00 → battlemap/BattleMap.h:144, battlemap/BattleMap.h:298
- 0x488FB0 → battlemap/BattleMap.h:140
- 0x489000 → battlemap/Combat.cpp:84
- 0x489050 → battlemap/Effects.cpp:2, battlemap/Effects.cpp:80
- 0x489380 → battlemap/BattleMap.h:133, core/Scenery.h:39
- 0x4895E0 → battlemap/BattleMap.h:133, core/Scenery.h:5
- 0x4896D0 → battlemap/BattleMap.cpp:818
- 0x489820 → battlemap/BattleMap.cpp:1238, battlemap/BattleMap.h:227
- 0x489860 → battlemap/BattleMap.h:244, battlemap/Combat.cpp:246
- 0x489880 → battlemap/BattleMap.h:245, battlemap/Combat.cpp:247
- 0x4898A0 → battlemap/BattleMap.h:246
- 0x4898F0 → battlemap/Driving.cpp:307
- 0x489E20 → battlemap/BattleMap.h:133
- 0x489F30 → core/SaveGame.h:35
- 0x48A4C0 → battlemap/BattleMap.cpp:234
- 0x48A530 → battlemap/BattleMap.cpp:234
- 0x48A5A0 → battlemap/BattleMap.h:301, battlemap/Combat.cpp:159
- 0x48A8B0 → battlemap/BattleMap.cpp:924
- 0x48A950 → battlemap/BattleMap.cpp:924, battlemap/BattleMap.h:185
- 0x48A9A0 → battlemap/Combat.cpp:222, battlemap/Combat.cpp:74
- 0x48ADA0 → battlemap/Effects.cpp:256
- 0x48ADD0 → battlemap/BattleMap.h:167, battlemap/Effects.cpp:256
- 0x48B580 → warmap/WarMap.cpp:1691
- 0x48C8E0 → battlemap/Weapons.cpp:261
- 0x48C9B0 → battlemap/BattleMap.h:282, battlemap/Weapons.cpp:261
- 0x48CCB0 → battlemap/Weapons.cpp:425
- 0x48CCD0 → battlemap/Weapons.cpp:436
- 0x48CD80 → battlemap/Combat.cpp:22
- 0x48D050 → core/Bridge.h:2
- 0x48D0A0 → core/Bridge.cpp:28, core/Bridge.h:23
- 0x48D2F0 → core/Bridge.h:11, core/Bridge.h:37
- 0x48DBE0 → battlemap/BattleMap.cpp:1240, battlemap/Weapons.cpp:2
- 0x48DCB0 → battlemap/BattleMap.h:143
- 0x48DD30 → battlemap/Effects.cpp:79
- 0x48DD50 → battlemap/BattleMap.cpp:825, battlemap/BattleMap.h:176
- 0x48DDC0 → battlemap/BattleMap.h:168, battlemap/Effects.cpp:191
- 0x48E050 → battlemap/BattleMap.cpp:841, battlemap/BattleMap.h:168
- 0x48E2A0 → battlemap/BattleMap.cpp:785
- 0x48E460 → battlemap/BattleMap.cpp:785
- 0x48F3B0 → core/Scenery.h:6
- 0x48F410 → core/Scenery.h:42
- 0x48F8A0 → battlemap/BattleMap.h:286, battlemap/Weapons.cpp:369
