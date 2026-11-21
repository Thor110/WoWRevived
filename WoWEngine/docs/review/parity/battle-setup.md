# Parity review: entering and running a battle (setup, update order, groups, AI, fog, reinforcements, scenery count, battle end)

Engine paths are relative to /home/user/WoWRevived/WoWEngine. Original addresses are from IDA-Output.txt.
"Verified" means the original's code and the engine's code were both read for this item.

Lists: the sector's six lists are player-relative. +0x4C HUNI (the player's units), +0x50 AUNI (the other side's),
+0x54 HCON, +0x58 ACON, +0x5C HRES, +0x60 ARES. This is confirmed by 0x43A980, which adds the player's side count
(chosen by byte_4B84C4) to +0x54, and by 0x446370's mapping. The engine's `BattleGroup::list` 0..5 is the same order.

---

## Visible bugs

### 1. The landscape shade tables (LANDR digit) and the burnt/red-weed trees use the wrong objects. Severity: high
- Original: 0x43981C-0x439864 (EnterBattleMap). If 0x404BA0 (byte_4B84C4, "I Am Human") is set, it computes
  dword_4F7430 = count(+0x60) + count(+0x58), which is ARES + ACON (the Martians' resources and constructions). In a
  Martian game it computes count(+0x54) + count(+0x5C), which is HCON + HRES (the player's own, so again the
  Martians'). Each count is 0x4216D0, the list's +0x24, which is the number of war map objects. So the value is
  always the number of Martian constructions and resources in the sector. It drives the next battle's LANDR shade
  digit (0x40B570 / 0x40B6C0 against dword_4BD4F8 = 3 and dword_4BD4FC = 10) and this battle's burnt trees
  (0x489380: rand() < 0x8000 / 7 x (n - 3) turns tree 0xD5-0xE5 into 0x399+). In effect it measures Martian
  infestation.
- Engine: src/warmap/WarMapAi.cpp:86-92 `EnemyObjects` sums `SectorObjects(sector, false, kind)` for kinds 0 and 1:
  the other side's units and constructions, with no resources. FrontEnd.cpp:1129 and 1218 use it for
  `enemyCount` and `lastEnemyCount`.
- What a player sees: in a Human game the count includes Martian fighting machines and leaves out Martian
  resources. In a Martian game it counts Human units and buildings, so a Martian-held sector full of Martian
  buildings shows no red weed and gets LANDR digit 0. A Human-held sector gets red weed for Human buildings.
- Fix: a `MartianInfestation(sector)` that counts the Martian side's constructions and resources (kinds 1 and 2;
  `players == !human`) as numbers of war map objects. Use it for both `enemyCount` and `lastEnemyCount`.

### 2. The pod lands after deployment and after the battle map is shown. In the original it lands before deployment, and the Martians deploy round it. Severity: medium
- Original: EnterBattleMap 0x439ADE-0x439B20 makes type 0xB3 when the sector's +0x64 is set. It adds it to +0x11C,
  then calls vt+0x24 and vt+0x14 (0x48F620: picks the random place, the two 0x3DA blasts, WScenery start). Only
  after that come the six 0x421FE0 deployments (0x439B24-0x439B79). 0x421FE0 (0x4220CB-0x422121) scans +0x11C for
  type 0xB3 and takes its +0x78 / +0x7C as a centre: for a list whose first object is Martian (0x4406E0 == 1)
  when the list's side doesn't hold the sector (0x422175-0x4221A5), and when it does hold it but there is no
  command building (0x4221D2-0x422200). So arriving Martians deploy round their pod. The blasts go off before any
  unit is placed, so they can't hit units.
- Engine: BattleMap.cpp:1188-1196 `Begin()` calls `LandPod` when the screen is first shown, long after `Deploy()`
  (BattleMap.cpp:88). Deployment never uses the pod (its comment at BattleMap.cpp:586-588 says the pod "comes
  after the deployment, as in the game"; that's wrong). The blasts (`Explode(0x3DA…)`, BattleMap.cpp:1248-1249)
  can damage units already deployed. The rand() calls happen after deployment's, not before.
  docs/BATTLEMAP.md:1142 repeats the wrong order.
- Fix: call `LandPod` (its place and its blasts) in the constructor after `LoadScenery` and before `Deploy()`. If
  the sound has to wait for `Begin()`, defer only the sound. In `Deploy()`, add the pod's point as a centre for
  Martian lists per 0x421FE0's two branches.

### 3. A forfeit (key building lost with no battle on) never returns to the war map. Severity: medium
- Original: 0x43AA18-0x43AA3B. Every update with no box up (dword_4F7450 == 0), if byte_4E9B94 is set it posts
  0x111/0x76 to itself with delay 0x96 (150 updates) through 0x4355A0, and sets dword_4F7450 = 1. This runs
  before the +0x54 (battle on) test, so it fires with no battle on.
- Engine: Combat.cpp:174 sets `forfeit`. `CheckBattleEnd` (BattleMap.cpp:2210) returns at once when `!battleOn`,
  and nothing else posts 0x76. The player stays on the battle map until they pick Return to Warmap themselves.
- Fix: at the top of `CheckBattleEnd`, before the `battleOn` test, add: if `forfeit && !battleDecided`, set
  `battleDecided = true` and schedule `Command{ReturnCommand}` at `ticks + 150`. This needs a pending-command
  return path out of `Update`.

### 4. Every unit's look-round, wake and paddle timers start at 0, so they all fire on the same update. Severity: low-medium
- Original: 0x495740 (each object's start, at deployment) sets +0xDA = rand() % 30 (0x4957A2-0x4957B2).
  0x4967E0 counts +0xDA against word_51A182 for the look-round (vt+0x104). The wake and paddle effects test
  +0xDA % 4 and % 3.
- Engine: `Unit::scanCount` defaults to 0 (BattleMap.h:288) and is never randomised at the start. The constructor
  loop (BattleMap.cpp:101-131) has no rand() for it. Every unit looks round and spawns wake on the same updates.
  The RNG sequence also differs from the original's.
- Fix: in the constructor loop, `u.scanCount = random::Rand() % 30`, in deployment order (0x495740 runs inside
  0x4222C0 after each unit's heading).

---

## Behaviour divergences

### 5. "Battle on" (report +0x54) is computed differently. Severity: medium
- Original: 0x439D22-0x439D83. Counts are numbers of war map objects (0x4216D0), and
  `on = (HUNI && (AUNI + ACON)) || ((HUNI + HCON) && AUNI)`. Resources never count, and two sides with only
  buildings is no battle.
- Engine: BattleMap.cpp:607-612 sets `battleOn = ours && theirs` over all battle units of all six lists,
  resources included.
- Effects: `CheckBattleEnd`, `SetUpReinforcements` (Reinforcements.cpp:27), the AI start (BattleAi.cpp:37), the
  Retreat popup item (BattleMap.cpp:1832), `Fought()` (whether losses reach the war map), and the forfeit flag
  (Combat.cpp:174). For example, player units against Martian resources only: the original has no battle and no
  end box, but the engine runs one until the resources are destroyed.
- Fix: compute it from `setup.groups` counts per list with the original's formula.

### 6. The AI start (0x8075) has its own condition, which the engine replaces with `battleOn`. Severity: medium
- Original: 0x439DF4-0x439E8D. 0x422860 counts category-1 objects (vehicles) of a list, and 0x4228B0 counts
  category 2 or 3. bl = "one side has none". If (HUNI veh + HCON bld/res) != 0, bl = (ACON bld/res + AUNI veh)
  == 0. Otherwise, if (AUNI veh + ACON bld/res) != 0, bl = (HCON + HUNI) == 0. Otherwise bl = 1. 0x8075 goes to
  both players only when bl == 0.
- Engine: BattleAi.cpp:37 `aiOn = battleOn`.
- Fix: a separate `aiStartOn` from the category counts (category 1 among units, categories 2 and 3 among
  constructions, by war map object as 0x440700 reads the type).

### 7. The planner's "strongest object" for the key building: the engine sends a strength-0 object, the original never does. Severity: low
- Original: 0x44578B-0x4457EE. best = 0. An object is taken only if (int64)best < 0x441E70's value (fcompp,
  `test ah,1`), with best = ftol(value). An object worth 0 is never chosen. With none chosen, 0x445873-0x445891
  sets the threat's flag again.
- Engine: BattleAi.cpp:129 `best < 0 || v > bestValue` picks the first object even when its value is 0.
- Fix: start `bestValue = 0` and take only `v > bestValue`. This settles the provisional "first of the greatest";
  strict `>` confirms it.

### 8. Reinforcement point room check: the original checks the wrong slot's sector (an original bug), and passes the group's member count. Severity: low
- Original: 0x43D3F0. After the search loop, edi equals dword_4F76E8, the count of entries. The room check at
  0x43D478-0x43D49E reads `dword_4F7460[edi*0x18]`, the sector of the slot one past the last entry, not the
  nearest one. EnterBattleMap's table clear (0x43D186) resets only +0 and +4 of each entry, so that sector is
  stale from an earlier battle or 0. The count is the group's 0x4216D0, its battle-unit count (0x480F22), and
  0x481F70 passes the same for markers (0x481FD2 / 0x481FF6).
- Engine: Reinforcements.cpp:80 checks the best entry's sector. Reinforcements.cpp:103 and 168/169 pass a count
  of 1.
- Fix: for parity, check room on the sector of `rpoints[rpoints.size()]` as a stale slot. The engine would need to
  keep the last battle's table, or use 0. Pass the group's battle-unit count. At minimum pass the member count.

### 9. The room check ignores units already retreated during this battle. Severity: medium
- Original: 0x43AC40 adds dword_4F86B8[sector] (land) or dword_4F76F0[sector] (boats) to the sector's count
  (0x43ACB3, 0x43AD2A). The war map object's 0x80B9 case (0x429453-0x4294D6) increments these as its members
  leave. EnterBattleMap (0x439795) and the battle's end (0x439EDE) clear them.
- Engine: `WarMap::HasRoom` (src/warmap/Production.cpp:87) counts only the war map's counters. Retreats in the
  battle aren't applied until it closes, so several groups can retreat into one sector past its cap.
- Fix: keep per-sector land and boat "left this battle" counts in BattleMap (`LeaveMap`). Pass them into
  `hasRoom`, or add them before calling it.

### 10. Marker retreat ignores the group's land/boat bits: boats can leave by a land point and land units by a boat point. Severity: medium
- Original: 0x4802E0 sets +0x1D8 bit 8 for any member of move class 5 or 6 (0x480383-0x48039F) and bit 9 for move
  class 1-4 (0x4803A5-0x4803CD). 0x481F70 (0x481FAD-0x482009): kind 1 or 7 requires bit 8 clear, then room as
  land. Kind 5 requires bit 9 clear, then room as boats (0x43AC40 with 0).
- Engine: Reinforcements.cpp:164-171. Its comment says "*Provisional*: +0x1D8 bits 8 and 9 are taken as clear",
  so any group can use any marker.
- Fix: refuse kinds 1 and 7 for a group with move class 5/6, and kind 5 for a group with move class 1-4. Speech
  0x121 as now. (Also settles the provisional; see "Provisional items resolved".)

### 11. Fog: an object's fog level is sampled at its +0xAC (type +0x44 word), not at the collision radius. Severity: low-medium
- Original: 0x48B050 reads [esi+0ACh] (0x48B09A) for the four corners.
- Engine: Fog.cpp:71 uses `Radius(u.type)`, which is the IOB model's max(2·hw, depth)·0.66 or type +0x46. The
  engine already holds +0xAC as `u.drive.radius` (BattleMap.cpp:105).
- Fix: `const int r = vertex(u.drive.radius);`. Enemies will then hide and show at the original's fog boundaries.

### 12. Fog timing uses the battle's own tick, not the game's global frame counter; the first decay comes before the reveals. Severity: low
- Original: the decay is 0x43A9B1-0x43A9C2 and the reveals 0x49681A / 0x48C84E, both
  `dword_4D2558 % dword_4EA614 == 0`. dword_4D2558 is the global frame counter (0x403749), which carries on from
  the war map. At entry, the reveals happen during deployment (0x495740 -> vt+0x100) and the first decay comes
  last (0x439D95), so revealed vertices start at 255 - returnRate.
- Engine: Fog.cpp:93 uses `ticks`, the battle-local counter starting at 1. Fog.cpp:32 decays inside `FogStart()`
  before `FogRevealOwn()` (BattleMap.cpp:130).
- Fix: keep a global frame counter in FrontEnd and pass it in, or seed `ticks` from it. Move the first
  `FogDecay()` after the start-up `FogRevealOwn()`.

### 13. A group's engagement range, terrain, slope and bridge costs come only from its vehicle members. Severity: low
- Original: 0x4802E0 skips everything from 0x48043A to 0x48050E for a member without +0xB8 bit 0: the bridge
  factor, the terrain costs (+0x70), max slope (+0x4C), class (+0x52), +0x50, and the range (+0x4E, from 100000;
  note it is stored as a word, so 100000 becomes 0x86A0).
- Engine: BattleMap.cpp:150-159 takes the range from every unit. Driving.cpp:930-985 `SetUpGrounds` takes every
  member (its comment says "*Provisional*: every member counts as +0xB8 bit 0").
- Fix: gate both on `u.vehicle`. A group with no vehicle keeps 100000 (as int16) and the default ground.

### 14. Update order within an update differs from the original's. Severity: low; see also architecture item A
- Original: the game loop 0x403680 runs the players' updates, then delivers the queued messages (0x434FA0 loop,
  where delayed 0x80A0, 0x80F4 and 0x808B arrive), then calls 0x435430 on the screen. The battle screen's update
  0x43A980 does the fog decay, then water twinkles (0x43A670), then the end check, then 0x435430 updates the
  screen's children (+0x20). Objects are children in the order they started: 0x495740 adds each with 0x440060 on
  [dword_4D2550+0x20] (0x495770-0x495779). Each object's update is complete in itself (move, look round, fog
  reveal, its weapon children).
- Engine: BattleMap.cpp:1631-1723. It runs fog first, including every reveal, before any unit moves. Then
  twinkles, deaths, popup, edge scroll and zoom, then `CheckBattleEnd`, then `AiUpdate` (after the end check),
  then each group's tick, then every group's chase. Next all units drive, then all guns, then the props, and the
  0x80F4 posts arrive only after the units (the original delivers them before).
- Fix: in the short term, deliver `controlPosts` (and any other delayed messages) before the screen work, alongside
  `dieAt`. Run the AI update before `CheckBattleEnd`. Run each group's chase (0x4826D0) inside its own
  `GroupTick`, before the counter, as 0x483B70 does. The structural fix is architecture item A.

### 15. GroupTick and the chase are split into two passes. Severity: low
- Original: 0x483B70 per group: if it has members, a target-tracking debug hook (0x483270), then the chase
  (0x4826D0) if +0x2C, then the counter, the think, and the roam or home logic.
- Engine: BattleMap.cpp:1649-1650 runs all `GroupTick`s, then `ChaseTargets()` over all groups. The chase's
  `groupTicks[g] % 8` test (Weapons.cpp:731) therefore sees the counter after its increment and reset, not
  before.
- Fix: call `ChaseTarget(g)` at the top of `GroupTick(g)`.

### 16. Deaths staggered by StaggerDeaths skip members with health <= 0, so later members die sooner and rand() is called fewer times. Severity: low
- Original: 0x47CEE0 posts 0x80A0 to every member, starting at delay 30 and adding rand() % 30 + 30 for each,
  regardless of health.
- Engine: Combat.cpp:186 `if (m.group != group || m.health <= 0) continue;` skips both the post and the rand().
- Fix: iterate every member. Keep `min(dieAt)` so an earlier death stands.

### 17. Losses from a visit with no battle on are thrown away. Severity: medium (unverified end to end)
- Original: battle units' deaths reach their war map objects as they happen (0x8057 / 0x80A0 paths, 0x428D70),
  whether or not +0x54 is set. In a forfeit, 0x80AA / 0x8136 wipe constructions and units in the war map lists
  directly.
- Engine: FrontEnd.cpp:883-891 passes `lost` / `retreated` only when `bm->Fought()`. A key building lost with no
  battle on, or `CheatRemoveSide` with one side absent, leaves the war map objects alive. The forfeit still raises
  EndGameLost, so the main visible case is the debug or cheat path.
- Fix: always pass `LostRefs()` / `RetreatedRefs()`.

### 18. Return-to-Warmap popup and end-of-battle counts use battle units, not every object with a side. Severity: low
- Original: 0x485E6E-0x485E97 and 0x43A9D9-0x43AA09 use 0x47D610 / 0x47D660 on the world's +0x118 list. These are
  absolute sides 0 / 1 (Human / Martian), mapped to the player by 0x404BA0. That counts every battle object with a
  side in +0x118 (whether effects are in that list is unverified).
- Engine: BattleMap.cpp:1737-1740 and 2213 count `units`. This is equivalent unless +0x118 holds other sided
  objects. Already marked provisional; left as is.

---

## Architecture misalignments

### A. Phase-ordered update instead of per-object updates in start order
The original's objects are children of the battle screen (0x495740 -> 0x440060 on [dword_4D2550+0x20]). The
screen's update (0x43A980 -> 0x435430 -> +0x20 vt+0x10) updates them one at a time in start order (deployment
order, with scenery and markers started where they were). Each object's vt+0x10 (0x4913B0 / 0x48F130 / 0x48AEF0
…) does its own movement, status timer, look-round, fog reveal and children (guns, turrets). Base 0x435590 sends
0x113 to itself, so its children follow.

The engine (BattleMap.cpp:1653-1691) runs one phase per behaviour over all units: drive everything, then all guns,
then the props, then the effects. Consequences:
- a gun fires from where its unit ends the update, not where it is when its own update runs;
- targets, positions and deaths interleave differently;
- the RNG sequence differs;
- groups (0x483B70, which are objects in the same tree) run in a separate pass.

Fix: keep one `objects` list in start order (scenery, markers, pod, deployed units, effects as they're made). Give
each entry `Update()` that runs its full per-object sequence, children included. Iterate it once per update.

### B. Groups are fixed to war map objects; the original's selection is itself a group that takes its members
0x439000 (an object added to the selection) gets or creates the player's +0x434 group through the screen's vt+0x24
(0x4390C2-0x4390D1). It adds the object with 0x4480F0, which unlinks it from its previous group (0x448137-0x448151:
the object's +0x34 group, 0x445E30) and sets +0x34 to the new one. It then sends 0x800F, so 0x4802E0 recomputes the
group's stance average, range, ground costs and think interval over the selection.

Orders (packets through 0x4812B0, a WBMGroup method) act on that combined group: one formation, home, target,
stance and interval. The engine's `BattleGroup` = war map object for the whole battle (`Unit::group`,
`groupTargets`, `groupStance` … indexed by `setup.groups`). A mixed selection gets separate per-object orders,
formations and homes.

Fix: separate "war map object" (`Unit::ref`, for losses and retreats) from "battle group" (`Unit::group`, a dynamic
`std::vector<BattleGroupState>`). Implement 0x4480F0's move-between-groups. Recompute (0x4802E0) when members are
added or removed. Unverified: what happens to the selection group on deselect. Trace 0x4392A4-0x4392E0 before
restructuring.

### C. The group's stats are computed once, from setup data
0x4802E0 runs whenever a member is added (0x47CFE0 from 0x429FF0) or the group gets 0x800F. The engine computes
range, interval and grounds once in the constructor (BattleMap.cpp:140-175, Driving.cpp:930). With dynamic groups
(B) this has to become a `RecomputeGroup(g)` called at those points.

---

## Provisional items resolved

1. **Reinforcement point kinds 7 and 1 share a point** (Reinforcements.cpp:20-22). Confirmed. 0x43D21E passes
   the kind (7, 5, 1) as 0x436050's move-class argument. 0x43705D-0x437088 returns the second (boat) point only
   for 5 or 6, so 7 and 1 both get the first.
2. **Retreat via marker, "+0x1D8 bits 8 and 9 taken as clear"** (Reinforcements.cpp:162). Bit 8 is set when any
   member is move class 5/6, and bit 9 when any member is move class 1-4 (0x4802E0). See item 10.
3. **"The group's objects as one"** (Reinforcements.cpp:162, 91). The count passed to the room check is 0x4216D0
   on the WBMGroup, its battle-unit count (0x480F22, 0x481FD2, 0x481FF6). See item 8.
4. **AI planner, "the strongest is taken as the first of the greatest"** (BattleAi.cpp:103). Correct, with
   strict `>` from 0. A value of 0 is never chosen; see item 7.
5. **SetUpGrounds, "every member counts as +0xB8 bit 0"** (Driving.cpp:929). No: only vehicles count. The
   airborne bit (5) alone is over all members. See item 13.
6. **The pod "comes after the deployment, as in the game"** (BattleMap.cpp:586-588, docs/BATTLEMAP.md:1142).
   Wrong: it lands before. See item 2.
7. **GroupTick, "attacking is taken as having a target or bit 0 set"** (BattleAi.cpp:196). The original tests
   only +0x1D8 bit 0 (0x483BD7, 0x483C48). The engine's `target != 0` is the same only while every targeting path
   also sets bit 0 (0x480EB0). 0x4826D0 / 0x4821F0 clearing paths are unverified. Strictly, the test should be
   `groupBusy` alone, with targeting setting `groupBusy`.
8. **Group stance (+0x54) at the start** (BattleMap.cpp:140-144). Partly resolved. The formula is
   ftol((Σ+0xD8 + 0.5) / n) (dbl_4AD750 = -0.5, with fsub). 0x4802E0 runs only from 0x47CFE0 (member add) and on
   0x800F (0x48090C). The member start 0x495740 sets +0xD8 and sends 0x8079 to +0x38, but the group handler
   0x4807F0 has no 0x8079 case, so no recompute follows. The value is therefore still whatever +0xD8 held when
   the group was built (no constructor writes +0xD8; only 0x495764 and 0x495F26 do). Still unresolved: whether the
   pool memory is zero. A player selection recomputes it properly through 0x800F (architecture item B).
9. **CheckBattleEnd's game-over flag (+0x55)**. 0x43A917-0x43A94B: set when, after 0x43CA40, sector 3's owner
   (dword_4EB05C) is 0 or London's (dword_4EC49C) is 1. The engine assumes the winner owns it
   (BattleMap.cpp:2236), which matches whenever the recompute gives the sector to the winner.
10. **Start-view HQ type for the "else" case**. 0x3A, not 0x13A, as Deployment.h says. Confirmed at 0x439B8D-0x439BAE.

---

## Missing

- **The forfeit's automatic 0x76 after 150 updates** (0x43AA18). See item 3.
- **The pod as a deployment centre** (0x4220CB / 0x42219D / 0x4221F8). See item 2.
- **The random start of +0xDA** (0x4957A2). See item 4.
- **Per-battle "left to sector" counts in the room check** (dword_4F86B8 / dword_4F76F0). See item 9.
- **The selection as a group** (0x439000, 0x4480F0). See architecture item B.
- **Halt (packet 0x11) resets the group's home**. 0x4812F0-0x481391 sets mode 0, then recomputes the members'
  centre into +0x60 / +0x64 before stopping them. Driving.cpp:299-308 `Halt` stops the members but leaves
  `groupHomeX/Y`, so a Retaliate- or Attack-stance group walks back to its old home within the next think if it
  halted more than 400 away. The engine's own comment says "*Not yet*: the group's target set to its centre".
  Fix: set the home to the members' (truncated, unsigned-divided) centre.
- **The members' 0x80B9 on retaliation** (0x48393A-0x483972). Not sent (provisional in Weapons.cpp:647). It clears
  +0x188 bit 9; it can't matter for a retreating group, since that group has bit 0 set and bit 2 clear and so
  ignores retaliation.
- **The report's statistics** (0x439C88-0x439D1F into dword_4E9B7C +0x18 .. +0x6C, and 0x43A800). These are left
  to the war map's settlement; not checked here.
