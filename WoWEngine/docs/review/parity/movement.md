# Parity review: battle map MOVEMENT

Scope: src/battlemap/Driving.cpp, src/battlemap/Routes.cpp, src/core/AStar.h, the bridge links, the movement parts of Sinking.cpp,
the move orders, footprints and heights/lift. On the original side: the vehicle classes 0x48F970-0x4935C0, the group move
0x482510 / 0x482E60 / 0x482B00 / 0x483270 / 0x4833F0 / 0x483590, WRouteFinder 0x494220-0x494690, and the bridge 0x4983D0 /
0x4984C0 / 0x4984E0 / 0x486820 / 0x486850.

Checked and found to match: 0x482B00 (the 8-point ring search), 0x4984E0 (deck height: constants -8.0 and 0.0 confirmed),
0x4983D0 (nodes 48 out, C8/CC = end 0, D0/D4 = end 1), 0x494400 (link inserted after the 4 grid neighbours), 0x491790
(SetWaypoint), 0x4911B0 (FollowRoute, ties go to the later node), 0x490A70 case 0x80C2 (constants -20.0, 0.5, 0.01, -1.0), the
0x492F40 behaviour order and gating, the turn limit (8.0, -120.0), the growth test (1.0, -0.625), 0x492DE0 speed control,
0x491870's waypoint and arrival tests, 0x4915B0 / 0x491630 footprints (1.25, bit 4), 0x490070 walker animation (-2.0,
0.33333334, -0.2), 0x490170 hover, 0x48FDD0's feet for 0x3E-0x4C, and the 0x494440 straight-line shortcut (flt_4B0888 = 1.0).

---------------------------------------------------------------------------------------------------------------------

## Visible bugs

### 1. Units crossing a bridge are drawn behind it: the bridge's draw-key raise is missing (HIGH)
- Original: 0x47F630 (each object's render update) calls vt+0x48 when +0xB8 bit 9 is set ("moved": 0x47F320(1), set by
  every step at 0x49345A). For every vehicle class, vt+0x48 is **0x491730** (the walkers' 0x489B30 just jumps there). It
  calls 0x47F340 (pose, which also resets the renderer's +0x1E and +0x26). Then, if +0x18C (the bridge link) is set and
  dword_59D9C0[link] exists, it calls the bridge's **vt+0xFC = 0x4984C0**(this). That adds **+0x30 to the renderer's +0x1E**
  (its sort y) and **+0x40 to +0x26** (its depth).
- The renderer list is drawn in order of **+0x1E + +0x26** (0x49F610 at 0x49F729-0x49F735). So while a unit is on a link,
  its key is raised by **0x70 (112)** and it is drawn after the bridge segments it is standing on. The raise is applied
  at each pose and lasts until the next one, so a unit that stops on the deck keeps it.
- Engine: a unit's key is `int(u.y) + o.depth` (BattleMap.cpp:1708, set at 1133), with nothing added for a link. Each
  bridge segment's key is `y + 2*six.d` (BattleMap.cpp:1487). A unit on the deck whose y is less than a segment's y + depth
  is therefore drawn before (behind) that segment. This is the "units crossing bridges are drawn behind the bridge" report.
- Fix: when re-keying (BattleMap.cpp:1704-1708), use `key = int(u.y) + 0x30 + o.depth + 0x40` for every view object of a
  unit whose `drive.link` is set at pose time. Keep a per-unit "bridgeRaise" flag that is set when it is posed on a link and
  cleared at the next pose off one, so that the raise also outlasts the arrival that clears the link (item 4).
- Test: drive a group across a N-S bridge, then an E-W one, with `--screenshot` taken mid-crossing.

### 2. AI groups put the move-acknowledge flag on the map and say "acknowledged" (HIGH)
- Original 0x483590 (the flag and speech): it does nothing unless the group's **+0x1D8 bit 3 (a player's order: set by
  0x481CA0 at 0x481E4E / 0x481F09, and by 0x480EF0 / 0x481170) and bit 4 (the group is the player's: 0x4802E0 sets it from
  its member's +0x24 == 0 at 0x4803D3-0x4803F0)** are both set. It then **clears bit 3**, so there is one flag per order.
  0x482E60 additionally calls it only when the first member's +0x24 == 0 (0x4831B5-0x4831D6), and so does 0x483270
  (0x483370-0x4833A0).
- Engine: Driving.cpp:250-255 and 263-267 spawn 0x17C and `Speak(0x10A)` for **any** caller of OrderGroupMove whose group has
  no target. That includes the AI's wander and go-home moves (BattleAi.cpp:87, 215, 228), reinforcement arrivals
  (Reinforcements.cpp:126) and Weapons.cpp:669. Enemy groups (and the player's groups on non-player moves) drop flags and play
  0x10A.
- Fix: carry a "player order" bit per group, set by OrderMove (0x481CA0) and cleared after the flag, and test it together
  with `players` before the flag and the speech. The same gate applies to the 0x10B speech in item 9.

### 3. The Martian walkers 0x50-0x5C are driven as wheeled vehicles (MEDIUM-HIGH)
- Original: the factory 0x446B30 builds **types 0x3E-0x4C and 0x50-0x5C** with the walker class 0x48FC20 (0x446D6C-0x446DA2;
  the Scanning Machine, Constrictor, Xeno Telepath, Handling Machine and Digging Mechanism). Its vtable 0x4AFEA8 gives:
  - vt+0x11C = 0x490040 (the feet);
  - vt+0x130 = 0x490070 (the turn-smoothing animation);
  - vt+0x134 = nullsub (**no wake**).
  0x48FDD0 has feet for all of them: 0x50: 2,3,4,5; 0x51-0x52: 4,5,6,7; 0x53: 8,9,10,11; 0x54-0x55: 9,10,11,12; 0x56: 1-5;
  0x57: 2,3,4,15,16; 0x58: 3-7; 0x59-0x5A: 13-18; 0x5B: 1-4; 0x5C: 2,3,4,5. The 0x4D-0x4F gap holds the fliers.
- Engine: walkers are only 0x3E-0x4C (Driving.cpp:615 Wake, 670 Paddle, 698 Animate). The 0x50-0x5C machines leave wakes,
  never hide their feet or splash in water, and their frame follows the distance moved, not the smoothed turn.
- Fix: a walker test covering 0x3E-0x4C and 0x50-0x5C (better, a vehicle-class table: see Architecture A1) and the feet
  table above, indexed by type rather than by `(type - 0x3E) / 3`.

### 4. A vehicle that arrives while on a bridge link never stops (MEDIUM-HIGH)
- Original 0x491870 at 0x491A9C: after the arrival is counted and 0x808B(1) is posted, **`+0x18C = 0`** (the link is
  dropped). vt+0x144 (0x4909D0) counts a link as "going", so clearing it is what lets the vehicle stop. Its +0x84 (deck
  height) is left as it was, so it stands on the deck.
- Engine: Advance (Driving.cpp:1087-1104) never clears `d.link`. `active` (Driving.cpp:374) and SpeedControl
  (Driving.cpp:1038) keep it "going" at no less than 0.2, so it keeps creeping round the point with the "going" engine sound.
- Fix: `d.link = 0;` after `Post808B(u, 1, ...)` in Advance. Keep `drive.lift` as it is.

### 5. The move flag (0x17C) is drawn with the wrong shade level, a likely cause of the "black circle" (MEDIUM, unverified value)
- Original 0x483590 after making the flag (0x4835B4-0x48362F): vt+0x40 (place), vt+0x24 (add), vt+0x14(dword_4D2550+0x90),
  then **the renderer's vt+0x18(0)**, which for a sprite is 0x40F040 (`level = arg >> 2`, giving 0), and **renderer +0x14 bit 0
  cleared** (passed to the blitter 0x40FD80 as `(bit0 << 1)`).
- Engine: SpawnEffect (Effects.cpp:93-136) does neither. Draw (BattleMap.cpp:~2040) draws every effect sprite at
  `(type.fieldA1 & 0xFFFF) >> 2`. If 0x17C's +0xA1 is non-zero, the flag is drawn through a darker shade table than level 0,
  which would turn the "_ACK" animation into a dark disc. This is unverified: I did not read 0x17C's +0xA1 from OBJ.ojd.
- Fix: give the flag a level override of 0 (and clear its blit flag) after SpawnEffect in both flag paths. To confirm,
  print `setup.units->at(0x17C).fieldA1`.

### 6. No flag when the group has a target (LOW-MEDIUM)
- Original 0x483590: the flag is made whenever bits 3 and 4 are set. Only the **0x10A speech** is skipped when the group has
  a target (+0x2C), at 0x48363B-0x483649.
- Engine: Driving.cpp:250 and 263 skip both the flag and the speech when `attacking`.
- Fix: gate only `Speak` on `attacking`, and gate the flag on the bits from item 2.

### 7. The engine sound stays where the vehicle started (LOW)
- Original 0x492F40 at 0x49330A-0x49332A: after each step, with a sound in +0xE8, 0x489290 sets the sound's +0x34/+0x38 to the
  object's screen point (+0x2C/+0x30), so it follows the vehicle.
- Engine: EngineSound (Driving.cpp:346-358) plays it once at the start point (marked *Provisional*), and Sound has no way to
  move a playing sound.
- Fix: add `Sound::Move(handle, x, y)` and call it after each step with `x, ProjectY(y, ground + lift)`.

---------------------------------------------------------------------------------------------------------------------

## Behaviour divergences

### 8. Mines (crushables) are never hit by move class 3/4 vehicles: the collision filter's test is inverted (MEDIUM)
- Original 0x4929A0: for +0x52 == 3 (or 4), when the object's renderer is under 0x30 (0x20) tall, it calls vt+0xD8
  (crushable). **If crushable it goes on to the normal test (it collides); if not, it returns false** (0x492A06-0x492A16).
  So for class 3/4 vehicles, low objects that can't be crushed are ignored and low crushable ones (mines) are hit. They are
  then crushed or set off in 0x492350 (1 in 4, rand() < 0x2000).
- Engine: Collides (Driving.cpp:795-804) returns false for low **crushable** objects and true for everything else. Class 3/4
  vehicles drive straight through mines without ever touching them.
- Fix: `if ((cls==3||cls==4) && height < thr && !Crushable(o)) return false;`.

### 9. "ROUTE NOT FOUND" leaves the group busy and still targeting, and speaks for every group (MEDIUM)
- Original 0x4833F0:
  - stops the route finder if the search is this group's;
  - with no target, clears +0x1D8 bit 0;
  - sends 0x8031 to every member;
  - sends 0x80B9 and 0x805A to the group, with +0x2C..+0x3C cleared (the target dropped);
  - clears bit 0 again;
  - speaks **0x10B only with bits 3 and 4** and flashes the portrait (0x4842D0), then clears bit 3;
  - for an AI group's order (bit 3 set, bit 4 not) tells the AI (dword_4E9B78 vt+0x1C(0x4E, 2, group)).
- Engine (Driving.cpp:236-243): it speaks 0x10B for any group and sends 0x8031, but `groupBusy` stays true (it was set at
  Driving.cpp:183) and the target stays. BattleAi treats a busy group as attacking (BattleAi.cpp:205), so an AI group whose
  route fails never wanders or goes home again.
- Fix: in the not-found branch, clear `groupBusy[g]`, call `StopAttacking(g)`, and gate the speech as in item 2.

### 10. All members on a bridge link: the engine routes from (0,0) (MEDIUM)
- Original 0x482E60 at 0x482F98: if the farthest is still -1000000 (no member off a link counted), it returns.
- Engine: Driving.cpp:190-205 skips link members but carries on with `sx = sy = 0` and asks for a route from vertex (0,0).
- Fix: `if (farthest == -1000000) return;` after the loop.

### 11. Boats and drones steer every update, and drones give way by 1.0 (MEDIUM)
- Original vtables: vt+0x148 (the steering period, used at 0x492F7A) is **0x48FB30 (returns 1)** for these classes:
  - 0x493870 (Ironclads 0x13-0x15, vtable 0x4B0568);
  - 0x4935E0 (0x16-0x18, vtable 0x4B03C8);
  - 0x48F970 (**Drones 0x5D-0x5F**, vtable 0x4AFD30).
  All the others use 0x4898F0 (3). vt+0x14C (the give in 0x492350's push) is 1.0 for all three of these classes
  (0x48FB40 / 0x493860 / 0x493B40; flt_4AFEA4 = flt_4B0564 = flt_4B06E8 = 1.0) and 8.0 (0x489900) for the rest.
- Engine: `d.phase % 3` for all (Driving.cpp:435). The give is `Boat ? 1.0 : 8.0` (Driving.cpp:913), so drones get 8.0.
- Fix: a per-class period (1 for 0x13-0x18 and 0x5D-0x5F) and give (1.0 for the same).

### 12. Boats have no cohesion (LOW-MEDIUM)
- Original: vt+0x164 is a **nullsub** in both water vtables (0x4B03C8, 0x4B0568). Their arrival, vt+0x168, is 0x493A40,
  which is equivalent to 0x491FA0.
- Engine: cohesion is added for boats too (Driving.cpp:511-517).
- Fix: skip cohesion when `Boat(u.type)`.

### 13. Tunnelling Track Layers underground don't ask for a route (MEDIUM)
- Original 0x482E60:
  - For each vehicle member, vt+0x118 is asked (0x493600: +0x190 bit 1, "under", for the sinker classes). var_21 is set if
    any member answers yes, and var_22 is cleared if any answers no.
  - If the **first member's vt+0x70** is true (only class 0x493B90, the Tunnelling Track Layers 7-9, whose vt+0x70 returns
    1), then: if all members are under, there is **no route search** ("GOING STRAIGHT THERE!"); if only some are under, the
    order is **dropped** (0x483166 jumps to the return).
  - Otherwise the route is searched.
- Engine: always searches (Driving.cpp:231-234, whose comment says "vt+0x70 and vt+0x118 both no"), so an underground
  tunneller follows a surface route round water and slopes.
- Fix: implement the two flags and the first member's class test as above.

### 14. Steering neighbours and obstacles compare ground heights, not object heights (MEDIUM, bridges)
- Original: 0x492A90 (each neighbour in 0x492C00) and 0x492350 (the object ahead, 0x4926EA) compare **+0x80** (the absolute
  height, which includes +0x84 lift) and ignore anything more than 64 apart.
- Engine: Gather (Driving.cpp:750-757) and LookAhead (Driving.cpp:858, 883) compare `land.HeightAt` only. A vehicle on a
  bridge deck and a boat or vehicle under it push and brake against each other, where in the original they don't.
- Fix: compare `HeightAt + drive.lift` on both sides.

### 15. A group's arrival count carries over between orders (LOW-MEDIUM)
- Original: +0x56 (arrivals) is reset only when it reaches the group's count (0x482450 at 0x482486). 0x482510 does not
  reset it, because bit 0 is set just before the test. The 0x808B stop delay is `9 x (count - +0x56) + 60` (0x491A67-0x491A7C).
  A new order given mid-move therefore starts with the earlier arrivals counted, and +0x1D8 bit 0 clears early.
- Engine: a fresh MoveOrder for each order (Driving.cpp:228), with `arrived = 0`.
- Fix: keep the arrived count per group, not per order (see A2).

### 16. Group counts: the engine uses the ordered members, the original the whole group (LOW)
- Original: these all use the group list's count (0x4216D0: +0x24):
  - the growth test (0x493140);
  - 0x80C2's growth and stagger (0x490B04, 0x490B7C);
  - 0x802E's stagger (0x49131C);
  - the route scale `ftol(sqrt(count) + 1)` (0x483317);
  - SetWaypoint (0x491832).
- Engine: `orders[].members` or `members.size()` (Driving.cpp:227, 256, 292, 540; Routes.cpp:367), which is the player's
  vehicles only, and 1 for a unit with no order. Driving.cpp:270-278 counts the whole group, matching the original there
  but not elsewhere.
- Fix: use the group's member count throughout.

### 17. The route search is synchronous and doesn't hold off other groups (LOW-MEDIUM)
- Original:
  - If the route finder's +0x44 is another group, 0x482E60 re-posts 0x804F to itself with a **10-update delay**
    (0x482E6B-0x482E9E).
  - 0x494440 builds the A* and returns. 0x483B70 -> 0x483270 polls the state (0 running: 0x44EFA0 again; 1: ROUTE FOUND;
    2: not found).
  - A bridge's RemoveNode (0x494370) sets dword_59E41C, which aborts the search (0x4833BA).
- Engine: RequestRoute runs the whole search at once (Routes.cpp:284, *Provisional*). Units start, and the flag appears,
  earlier than in the game, and two orders on the same update both go at once.
- Fix: see A3.

### 18. "CAN'T GET TO WITHIN ATTACK DISTANCE!" is missing (LOW-MEDIUM)
- Original 0x482E60 at 0x4830FD-0x48313F: with a target (+0x2C), the radius is the vertex of the group's +0x4E (its attack
  range). If the goal's ring distance from 0x482B00 (its return value, esi) is more than that, it calls 0x4833F0 (route not
  found) and returns. Otherwise the radius is that vertex value. Without a target the radius is 4.
- Engine: Weapons.cpp:759-760 passes the radius but never compares it with NearestPassable's return, which the engine
  ignores (Driving.cpp:225-226).
- Fix: keep the goal's NearestPassable result and add the check.

### 19. The second target (+0xC8/+0xCC) is ignored on arrival (LOW)
- Original 0x491870 at 0x491A0C-0x491A43: on arriving, if +0xC8 and +0xCC are both set and differ from the current target,
  the target becomes that point and it does not arrive yet.
- Engine: not in (*Provisional* at Driving.cpp:1053).

### 20. Muster (0x80C3) doesn't take its first step at once (LOW)
- Original case 0x80C3 (0x490D06-0x490D7D): after the heading, speed 0.2 and velocity, it calls 0x47F320(1) (+0xB8 bit 9,
  "re-pose") and **vt+0x11C**, the class's drive step (0x492F40 or the walker / flier variant), straight away.
- Engine: Muster (Driving.cpp:100-109) sets the velocity only.
- Fix: call `Steer(u)` plus the class's extras (Paddle / Hover) and mark the unit moved.

### 21. A broken bridge's link isn't handled (LOW now, since bridges can't break)
- Original:
  - 0x492F40 at 0x4933B6-0x493422: on a link whose bridge's +0xEC is 0 (broken), a vehicle over water (0x47FFE0) gets
    0x80A0 (death) and its group 0x8031; otherwise the link is cleared and +0x84 is set to 0.
  - 0x486850 (the bridge's vt+0x108) clears +0xEC and removes both nodes (0x494370 RemoveNode, which also aborts searches).
- Engine: none of it (*Provisional* at Driving.cpp:429).

### 22. Footprint removal and the "bit 8" filter in the group move (LOW)
- Original 0x482E60: vt+0x114 (clear the footprint) runs for **every** vehicle member, before the link check
  (0x482F17-0x482F3F). Members with +0x188 bit 8 are excluded from the nearest and farthest.
- Engine: link members `continue` before ClearFootprint (Driving.cpp:194-196), and there is no bit 8 test. This is harmless
  while link members can't hold a footprint (they are "going"), but item 4's fix lets a vehicle stand still on the deck after
  the link is cleared, and that one does mark one.

### 23. EngineSound's point leaves out the lift (LOW)
- Original 0x491450 plays at +0x2C/+0x30 (the screen point of the posed object).
- Engine: Driving.cpp:356 projects without `drive.lift`. It is a few pixels off on a deck. Add the lift.

### 24. A unit's depth (+0x26) is recomputed at each pose (LOW-MEDIUM, draw order)
- Original 0x47F340 at 0x47F392-0x47F3A3: after transforming the model, **+0x26 = (model +0x72 - model +0x6C) / 2**, half the
  posed model's extent on its third axis (0x49E690 keeps the minima at +0x68/+0x6A/+0x6C and the maxima at +0x6E/+0x70/+0x72).
  The initial `2 x six.d` (0x49DB1C) only lasts until the first pose. The bridge segments' class jumps to 0x47F340 as well
  (0x498370).
- Engine: the depth is fixed at `2 x six.d` (BattleMap.cpp:1125-1133, 1474-1487).
- Fix: after `wof->Transform`, set `o.depth = (wof->MaxZ() - wof->MinZ()) / 2` (exposing the third-axis extents) and re-key.
  Together with item 1, this decides unit-versus-bridge order.

---------------------------------------------------------------------------------------------------------------------

## Architecture misalignments

### A1. Vehicle classes are guessed from type ranges instead of the factory's classes
The original picks a class per type in 0x446B30:
- 7-9: 0x493B90 (tunnellers);
- 0x13-0x15: 0x493870 (Ironclads);
- 0x16-0x18: 0x4935E0 (water sinkers);
- 0x19-0x1B and 0x4D-0x4F: 0x490120 (fliers);
- 0x3E-0x4C and 0x50-0x5C: 0x48FC20 (walkers);
- 0x5D-0x5F: 0x48F970 (drones);
- the rest: 0x490820 (plain).

Each vtable differs in these slots (all verified):
- vt+0x70 / vt+0x118: route bypass;
- vt+0x11C: the step variant;
- vt+0x124: the sinkers' 0x499120;
- vt+0x12C: the tunnellers' speed;
- vt+0x130 / vt+0x134: animation and wake;
- vt+0x144: Ironclads 0x4938D0 (just a jump to 0x4909D0);
- vt+0x148: the period;
- vt+0x14C: the give;
- vt+0x154 / vt+0x15C / vt+0x160 / vt+0x164 / vt+0x168: the water behaviours.

The engine scatters `Boat()`, `Sinker()`, `0x3E..0x4C` and `0x19..0x1B` tests across Driving.cpp, which produced items 3,
11, 12 and 13. Fix: a `VehicleClass` enum set once from the factory's ranges, with a table of these per-class properties
that Steer, Drive, Animate, Wake and the group move consult.

### A2. Move state lives in a growing `orders` vector rather than on the group
The original keeps the arrival count (+0x56), nearest (+0x68), farthest (+0x6C), busy bits (+0x1D8) and route (+0x1D4) on
the group (WBMGroup). The engine's `orders.push_back` per order (Driving.cpp:228) keeps old orders forever, resets
the arrival count, and so can't reproduce item 15. Fix: per-group fields, overwritten by each order.

### A3. The route finder is a function, not a stateful singleton
WRouteFinder (dword_59E418) holds one search (+0x44 the group, +0x48 the BatRoute cost, +0 the A*), stepped by the
group's poll. Busy groups wait 10 updates, and RemoveNode aborts a search. Fix: a RouteFinder object with
`Begin(group, ...)` / `Step()` / state, polled from GroupTick, and 0x804F re-posting for other groups.

### A4. A unit's draw key is a single number with no renderer +0x1E / +0x26 split
The original key is the renderer's +0x1E (its sort y) + +0x26 (its depth). The pose rewrites both, and the bridge raises
both while a unit is on a link (item 1). The engine's ViewObject has `key = y + depth` with a fixed depth. Fix: keep
`sortY` and `depth` separate on ViewObject, recompute both at each pose (items 1 and 24), and re-key and sort.

### A5. Bridges are pure scenery
In the original a bridge is an object with a link number, an intact flag (+0xEC), two route nodes (+0xE4 / +0xE8) and vtable
hooks (vt+0xFC raise, vt+0x100 deck height, vt+0x104 nodes, vt+0x108 break). The engine keeps a `BridgeLink` beside the
segment models, which can't break, unlink or raise its riders. Fix: a Bridge battle object owning its segments, nodes and
state.

---------------------------------------------------------------------------------------------------------------------

## Provisional items resolved

| Engine note | Answer from the original |
|---|---|
| EngineSound "the sound stays at the point it started from (the game's may follow the vehicle)" (Driving.cpp:344) | It follows. 0x492F40 at 0x49330A calls 0x489290 each step, setting the sound's +0x34/+0x38 to the object's +0x2C/+0x30. vt+0x110 / vt+0x114 are already in as Mark/ClearFootprint. |
| Muster "0x47F320(1) and vt+0x11C after it aren't in" (Driving.cpp:99) | 0x47F320(1) sets +0xB8 bit 9 (the "re-pose" flag, cleared by 0x47F630 after vt+0x48). vt+0x11C is the class's drive step, taken at once (item 20). |
| LookAhead "bit 7 and the bridge links aren't in" (Driving.cpp:844) | The link part is in and matches (0x4924B1-0x4924D4: a blocked probe only counts off a link). The probes need the group (+0x34) to be non-null and **+0x188 bit 7** (not +0x1D8) to be clear. Bit 7's setter was not found (unverified). |
| 0x4984C0 "unclear" (docs/survey/part13.md:139, docs/review/range-490-4A4.md:172) | The bridge's vt+0xFC. It raises a rider's renderer sort y by 0x30 and depth by 0x40, called from 0x491730 (the vehicles' vt+0x48) while the rider is on its link (item 1). |
| RequestRoute "the search is run to its end at once" (Routes.cpp:248) | Asynchronous. State 0/1/2 polled by 0x483270, other groups deferred by 10 updates (item 17). The limit 200 is per poll, so the result is the same; only the timing differs. |
| OrderMove "the attack range ... aren't in" (Driving.cpp:120) | The radius is the vertex of the group's +0x4E, with a failure if 0x482B00's ring distance is larger (item 18). |
| Advance "the second target (+0xC8 / +0xCC) taken first when set" (Driving.cpp:1053) | Confirmed: on arrival, if both are non-zero and differ from the target, the target is switched and it does not arrive (item 19). |
| Steer "broken bridges aren't in" (Driving.cpp:429) | Defined in item 21 (0x80A0 over water, otherwise the link and +0x84 dropped). |
| Paddle / Animate "every vehicle class but the walkers' is taken to be 0x493490's" (Driving.cpp:662) | True for vt+0x130 (all non-walker classes use 0x493490). The walker set is 0x3E-0x4C **and 0x50-0x5C** (item 3). |
| "vt+0x144 ... bit 8 isn't set by anything ported" (Driving.cpp:373) | 0x4909D0 confirmed: (not stopped, or bit 5, or bit 8, or link) and not vt+0x108. The Ironclads' 0x4938D0 is a plain jump to it. |

---------------------------------------------------------------------------------------------------------------------

## Missing

1. The bridge raise of a rider's draw key (0x491730 -> 0x4984C0) (item 1).
2. The link cleared on arrival (0x491A9C) (item 4).
3. The player-order and player-group gate on the flag and on the 0x10A / 0x10B speech, with bit 3 cleared after (item 2).
4. The flag's level 0 and blit-flag reset (0x483622-0x48362F) (item 5).
5. The walker set 0x50-0x5C, their feet, no wake, and the turn animation (item 3).
6. The per-class steering period and give for boats and drones, and no cohesion for boats (items 11, 12).
7. The underground route bypass for the Tunnelling Track Layers (vt+0x70 / vt+0x118) (item 13).
8. Moving the engine sound with the vehicle (0x489290) (item 7).
9. In ROUTE NOT FOUND: busy and target cleared, the 0x80B9 / 0x805A group messages, and the AI notification (item 9).
10. The asynchronous route search, the 10-update deferral of other groups, and aborts (item 17).
11. "CAN'T GET TO WITHIN ATTACK DISTANCE" (item 18), the second target (item 19), and Muster's immediate step (item 20).
12. Broken bridges: 0x486850 unlink with RemoveNode 0x494370, and the death of a vehicle over water on a broken link
    (item 21).
13. The per-pose depth +0x26 = extent / 2 (0x47F340) (item 24).
14. The early return when no member is off a link (item 10).

Not verified (not reached in this pass):
- the Halt / backspace "group's target set to its centre" (0x4812B0 case 0x11);
- the water classes' avoidance and braking scale factors (0x4939A0 / 0x4937C0 / 0x4938E0 / 0x493700);
- the exact meaning of the z argument in the step 0x47F770 -> vt+0x40 (`ftol(+0x80 - +0x84) - ground + +0x84 + +0x90`);
- 0x17C's +0xA1 value.
