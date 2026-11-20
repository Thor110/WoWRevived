# Part 07: 0x443340 to 0x44E4B0

Vanilla `WoW_patched.exe`. Source listing: `pieces/part07.txt`, about 19k lines.

## 1. Overview

This piece is not one subsystem. It holds the end of the AI source file, then several small engine classes, then the whole of DeeDraw. In address order:

| Range | Subsystem | Size |
|---|---|---|
| 0x443340–0x444FDA | **War-map AI turn phases**. These are the strategic computer player: it moves groups, builds, invades and orders production. Phase 0 is 0x443030 and the dispatcher is 0x442EC0, both in part 06. | ~7 KB |
| 0x445000–0x445E94 | **Battle-map AI**: the targets it can see, attack tasks and unit registration. These are the AI player's handlers for messages 0x8075, 0x807A, 0x807F and 0x80B1. | ~3.5 KB |
| 0x445EA0–0x445FED | **Network (remote) player**. It sends and receives the lockstep order buffer. | small |
| 0x445FF0–0x446070, 0x449AC0 | AI helper value types: a neighbour-sector set and a scored candidate. | tiny |
| 0x4460A0–0x4460C0, 0x4493E0, 0x449EB0, 0x449A70, 0x447D80 | **Player order packets**: 4-byte header and a payload, appended to a player's order buffer. | tiny |
| 0x4460E0–0x4465B0 | **Sector object lists**: homeUnits/awayUnits/…, the SECT/HUNI/… save tags. | ~1.3 KB |
| 0x446690–0x446946 | **Zoomer**: the smooth "scroll the view to X,Y" animation on the war map and battle map. | small |
| 0x446950–0x447D7E | **Object factories**: war-map objects (0x446950) and battle-map objects (0x446B30, a 4 KB switch over type ids). | ~4.6 KB |
| 0x447DC0–0x4490E0 | **Unit group** (vtable 0x4AA800): members, messages, and the **order panel** (Build / stances / Retreat / Abandon / special weapons). | ~5 KB |
| 0x449120–0x449410 | Group helpers: the group's power/health pip strip (8 pips) and the `[Stats] Holding Groups` setting. | small |
| 0x449450–0x449EDC | WCmdTarget composite lists, a 2D **spatial bucket grid** for battle-map objects, and a small int stack. | ~2.5 KB |
| 0x449EE0–0x44AD07 | **DeePlay**: DirectPlay 3 networking, with session enumeration, host/join, and send/receive threads. | ~3 KB |
| 0x44AD20–0x44E470 | **DeeDraw**: DirectDraw 2 setup, mode list, surfaces, palette, lock/unlock, Blt/Flip present, the software mouse cursor with save-under, DDERR strings, and the **movie threads** (Smacker `.smk` and Rage "Reaper" `.fmv`). | ~14 KB |

Nothing here looks hand-written. Every function has normal MSVC prologues and SEH frames, and switch tables are generated in the usual way. The only odd code is ordinary compiled `rep movsd` copying.

**Correction to ENGINE-MAP.md.** The "Reaper" at 0x4783D0 is not the game's fatal-error handler. It is the error box (`"Reaper Fatal Error"`) of Rage's own FMV codec, at roughly 0x477DC0–0x47A000. The `[Screen] "Use Reaper"` setting (bool, default 0, 0x44DC60) chooses that codec (`FMV\<name>.fmv`, 0x44E060) over Smacker (`FMV\<name>.smk`, 0x44DE20).

---

## 2. Subsystems

### 2.1 War-map AI: the computer player's strategic turn

**Context.** The AI player object is built by `sub_442890` with vtable handler 0x442AC0 (part 06). The war-map world creates it at 0x41876E when the opponent is computer-controlled. Message 0x802D, sent each AI turn, runs `sub_442EC0`. That function switches on `this+0x43C`, the phase from 0 to 9, and runs **one phase per call**.

| Phase | Function | Name I'd give it | What it does |
|---|---|---|---|
| 0 | 0x443030 (part 06) | `AI_GatherGroups` | Walks sectors 1–30 and collects candidate unit groups. Each becomes a `WAICandidate` (0x449AC0, `{vptr 0x4AA878, +0xC unit, +0x10 strength}`) on a stack (0x449B10/30/50). |
| 1 | **0x443340** | `AI_MoveGroup` | Takes the next group from `dword_546230[this+0x440++]` and decides whether to stay or move to a neighbouring sector. See the formula below. |
| 2 | **0x443610** | `AI_EvaluateEconomy` | Rebuilds the per-sector AI records at 0x545BB0 (0x18 bytes a sector) and the AI's movable groups into `dword_546230[]` (count `this+0x444`). Scores sectors by resources (0x441D30 / 0x441DB0 / 0x441E10 read the sector table 0x4EAE98…) plus `AI Build Resfac Eagerness` (×3 resource types), and bubble-sorts the candidate sectors `dword_545EB0[]` (count `dword_546050`) by priority `flt_5460B8[]`. |
| 3 | **0x443CA0** | `AI_DispatchBuilder` | For the current constructor group, decides where to build. Uses `AI Eff Threshold` (0.6), `AI Panic Build Threshold` (500) and `AI Minimum Rr` (8.0). Picks the building type from `dword_4BEDC0[] = {0x2F,0x30,0x31,0x6C,0x6D,0x6E}` (AI.ojd value-6 types, i.e. resource buildings, 3 a side; a guess) and calls 0x444510. |
| 4 | **0x4440B0** | `AI_ScoreFrontline` | For each AI-held sector that borders an enemy (0x441C50), scores it with `AI Frontline Attractor for Defences` (2.0) and the enemy strength next door (0x442330). Sectors 3 and 30 are special-cased (0x8 → `dbl_4AA6E8`, `dbl_4AA708`). Then regathers groups. |
| 5 | **0x4443D0** | `AI_SendBuilderToFront` | If the current group can build (0x442460), the best candidate sector scores > 0, and a building site of AI category 7 is free (0x441310), calls 0x444510. |
| 6 | **0x444690** | `AI_PlanInvasion` | For each AI sector whose strength exceeds `AI Invasion Threshold PC` (150), compares it with enemy neighbours. The comparison uses `AI Outnumber Force` (1.1), `AI Retain Force` (0.2), a random factor `(rand()%100+50)/(sector.+0x68+1)` with `flt_4AA710`, resources plus Resfac Eagerness, and `AI Invader NFriend Mul` (3000). Picks the best target and calls 0x444990. |
| (6) | **0x444990** | `AI_LaunchInvasion(from, to)` | Sends combat groups (AI value 1 and passable, 0x4419E0) from `from` towards `to` with WM_COMMAND 0xAD. It keeps adding groups (random pick, `rand()`) until the committed strength (0x441E70) is enough. |
| 7 | (inline) | | Sets the phase to 8. |
| 8 | **0x444C10** | `AI_OrderProduction` | For each AI factory group (AI value 4, via 0x441EE0 lists), builds the list of what it can make (0x45F970, 0x45F900, 0x4603B0), then queues production with a random choice (`rand()`). Partly a guess: it calls into the build list at 0x45F900–0x4607C0. |
| 9 | (inline) | | Sets the phase to 0. |

**Group move decision (0x443340), exact.** Let S = `0x441E70(group)` (strength = count `0x4216D0` × AI weight `word_518B10[type*8]`). Let F(s) = `0x441F00(s,0)` (sum of value-1 unit strengths in sector s).

- Stay value: `v = (F(cur) − S) × (1 + rand15/32768 × AI Movement Temperature[0.25])`.
- If every neighbour is ours (`|0x441AD0| == |0x441B80|`), `v *= AI safe sector repeller Mul [2.0]`.
- If cur is sector 3 or 30, `v *= 0.8` (`dbl_4AA6D0`).
- Candidate scores: each friendly neighbour n the group can enter (0x4419E0: unit class 0x13/0x16 needs water access 0x43AC10) scores `v − F(n) / (AI Frontline Attractor[2.0] × e(n) + 1) × AI Move Threshold[1.05]`, where e(n) is n's enemy-neighbour count (0x441C50).
- If the best score is > 0, the group is sent there: `group+0x58 = old sector`, then `group->vtbl[7](0x111, 0xAD, sectorObj)`.

`0x444510(group, sector, buildType)` is `AI_SendToBuild`:
- Moves the group with WM_COMMAND 0xAD, and keeps the per-sector "groups heading here" count at `0x545BB4 + s*0x18`.
- Looks up the build site for that type in the sector from the AI.ojd rule lists (0x4411B0: `{type,x,y}` or wall `{type,x1,y1,x2,y2}`).
- Queues a build order `{type, x, y, x2, y2, 0xDEADDEAD}` on the group's constructor (`0x460240` on `group+0x54`).
- Building type 0x78 also queues 0x77. These are AI value-3 types; my guess is a paired building.

**Debug.** If `[Debug] "Enemy Visible"` (value `dword_54604C`, default 0) is set, every AI move also snaps the group's war-map counter to the target sector's waypoint (`0x437380` → `0x4406A0`). That is how you watch the AI.

AI helpers used (part 06; listed for orientation):

| Address | Use |
|---|---|
| 0x441EB0 | AI value or category of a type |
| 0x441E70 | Group strength |
| 0x441F00 | Sector strength |
| 0x441AD0, 0x441B80, 0x441C50 | All, friendly and enemy neighbour sets (`{vptr 0x4AA774, u8 count, u8 sectors[7]}`, built from 0x4356C0 adjacency and 0x43CA30 owner) |
| 0x441A50, 0x441A80 | Sector is ours / ours and usable |
| 0x441EC0, 0x441EE0 | The sector's lists at +0x50 and +0x58 |

### 2.2 Battle-map AI

These are handled by the AI player's message handler 0x442AC0 (part 06).

| Address | Message | Name | What it does |
|---|---|---|---|
| 0x445210 | 0x8075 | `BAI_BattleStarted(sector)` | Records the battle sector (`+0x468`, object `+0x46C`), and whether it is defending (`+0x450` = the AI owns the sector). Clears 16 target slots (`+0x47C`) and 32 task slots (`+0x4BC`). Fills targets from the enemy's construction and resource lists of the sector, as ref-counted `WObject` (vtable 0x4AA720, `+0x10` = object, flag bit 0 in `+0x14`). Registers each of its own units (list 1) with 0x445500 and stores the unit count in `+0x444`. |
| 0x445500 | | `BAI_RegisterUnit(unit)` | Ignores empty groups. Category-2 units (AI value 2, possibly static defences) get msg 0x80B2 with param 0. Other units are appended to the AI's unit list (`+0x53C/+0x540`, count `+0x544`) and get 0x80B2 with param 2. 0x80B2 goes to the unit's battle-map object (0x47D600 → 0x47EB00); it probably sets its behaviour (a guess). |
| 0x445B90 | 0x807A | `BAI_UnitGone(unit)` | Removes a unit from tasks and targets and re-registers survivors. Only runs while the battle map is active (`dword_4E9B7C+0x54`). |
| 0x445D60 | 0x80B1 | `BAI_EnemySeen(obj)` | Adds a target (up to 16) if it is not already known. |
| 0x445620 | 0x807F | `BAI_BattleEnded` | Releases all targets and tasks. Called after 0x4393D0 and the 0x440EA0 cleanup in the handler. |
| 0x4455D0 | vtable tick | `BAI_Tick` | `+0x45C++`. With no tasks (`+0x478 == 0`) it runs the planner 0x4456B0. Otherwise it advances a round-robin cursor `+0x470` over the 32 task slots. |
| 0x4456B0 | | `BAI_Plan` | Assigns attackers to targets by strength (0x441E70), creating up to 32 tasks with 0x445010. It uses thresholds `flt_4AA6D8` (1.0), `flt_4AA714` and `flt_4AA718` on the ratio of AI units to targets. |
| 0x445010 | | `WAttackTask::ctor(ai, attacker, target, attacker2)` | Vtable 0x4AA730, "WObject". With no target, sends the attacker 0x80B2 with param 3. If attacker and target are in the same place (0x4406E0), it moves the attacker to the target's position (0x482510). Otherwise it sends 0x80C0 (attack object) to the attacker, and to attacker2 if there is one. |
| 0x4451A0 | | `WAttackTask::dtor` | Re-registers the attackers (0x445500). |
| 0x445000 | | `WObject(target)::dtor` | |

### 2.3 Players and the lockstep order buffer

**Player classes.** `dword_4E9B74` is the local player: class 0x438B20, "Human" or "Martian", vtable 0x4AA528, with an order buffer at `+0x30` (up to 0x400 bytes, used size `+0x2C`). `dword_4E9B78` is the opponent, which is one of:
- the AI (0x442890), or
- a **network player** (0x445EA0, vtable 0x4AA740; dtor 0x445F00). It sets `+0x20 = 1` and `dword_4F73D4 = 0`.

**Network player slots.**
- slot 9, 0x445F20 `Receive`: copies the last DeePlay packet (0x44A130) into the order buffer and sets `+0x438` (has data).
- slot 10, 0x445F80 `Send(localPlayer)`: if the local buffer is empty it posts a null order (type 1, param 1). It then hands the buffer to DeePlay (0x44A0E0) and sets `dword_4F73D0 = 1`.
- slot 12, 0x445FD0 `(flag)`: if `!flag` it posts order param 0x0D. Its meaning is unclear; it may be "end of cycle" or "pause".

**Order packets.** Each packet has a 4-byte header: bytes 0 and 1 are the type, byte 2 is a parameter, byte 3 is the total length. A packet is appended to the target player with `0x438C50(this, len)`. The target is held in `dword_566260`.

| Builder | Header | Length | Payload |
|---|---|---|---|
| 0x4460A0(target, param) + 0x4460C0 post | `01 01 pp 04` | 4 | none. 22 call sites across war map, battle map and UI. |
| 0x4493E0(target, param, d1) | `04 04 pp 08` | 8 | +4 dword |
| 0x449EB0(…, d2) | `03 03 pp 0C` | 12 | +8 dword |
| 0x447D80(…, d2) | `06 06 pp 0C` | 12 | +8 dword |
| 0x449A70(…, d3) | `05 05 pp 10` | 16 | +0xC dword |

What each param means is decided by the order executor, which is outside this piece (around 0x438C90–0x439230).

### 2.4 Sector object lists and the SECT save chunk

Each sector has a `WSectorObjects` object: ctor 0x4460E0 (called by 0x439480), vtable 0x4AA778, base WCmdTarget 0x435310, `+0x48` sector number. It owns six `0x2C`-byte named lists (0x421340):

| Offset | Name | Save tag |
|---|---|---|
| +0x4C | homeUnits | HUNI |
| +0x50 | awayUnits | AUNI |
| +0x54 | homeConstructions | HCON |
| +0x58 | awayConstructions | ACON |
| +0x5C | homeResources | HRES |
| +0x60 | awayResources | ARES |

| Address | Name | What it does |
|---|---|---|
| 0x446270 | dtor | Deletes the six lists. |
| 0x446310 | `Broadcast(msg,w,l)` | Sends vtbl[7] to all six lists, in the order 4C, 54, 5C, 50, 58, 60. |
| 0x446370 | `GetList(kind 1..3, bool home)` | Kind 1 is units, 2 constructions, 3 resources. Any other kind returns +0x58. |
| 0x4463C0 | `Add(obj)` | Kind from 0x440700. Home when `obj+0x24 == 0` (side byte). Adds with 0x47D190. |
| 0x446460 | `Remove(obj)` | Removes with 0x440A90. |
| 0x446500 | `Load(stream)` | vtbl[2]. Reads the SECT tag, then for each list a tag (`stream->vtbl[0x60/4]`) and the list's own loader (vtbl[2]). Skipped for sector 0. |
| 0x4465B0 | `Save(stream)` | vtbl[3]. Writes `SECT` (0x54434553), then each tag followed by the list's writer. Tags are written with `stream->vtbl[0x34/4]`. |

So "home" is the object's side byte being 0. In a Martian save the Martians are in HUNI, which matches SaveGame.h's note that H lists are "the side being played".

### 2.5 Zoomer: smooth view scroll

The Zoomer is vtable 0x4AA7B0 and registers itself with the task list `dword_4D2538+0x20` (0x440060).

- **ctor 0x446690(view, x, y, accel).** Clamps the target to the scroll range of `dword_4EA448` (0x42F500 / 0x42F510 minus the view size). It computes the angle and distance with 0x43FDA0 and keeps:
  - `+0x1C` the heading
  - `+0x28` the distance, with `+0x2C = distance × 0.5`
  - `+0x38/+0x3C` the current position as floats
  - `+0x20` the acceleration
  - `+0x24` the speed, starting at 0
  - `+0x40` the view window
- **tick 0x446850.**
  - Subtracts the speed from the remaining half-distance. While it is ≥ 0, `speed += accel`; after the midpoint, `speed −= accel`. This is a triangular velocity profile.
  - Steps the position by (speed, heading) (0x43FE90).
  - Sends `WM_COMMAND 0x7A, (x & 0xFFFF) | (y << 16)` to the view.
  - When the speed reaches ≤ 0, deletes itself (0x4393D0).
- **Callers:**
  - 0x41E0CE, war map: "go to" an object or message, with `[Tweak] "Warmap Zoomer Speed"`, default **1.5** (0x41DFC0, value at `dword_4E9F3C`).
  - 0x43A5A4 and 0x43C050, battle map: with `[Tweak] "Battlemap Zoomer Speed"`, default **2.5** (0x43A440, `dword_4F744C`).
- 0x446900 / 0x446940 are the ctor and dtor of a trivial WCmdTarget subclass (vtable 0x4AA7D8, callers 0x41F550 / 0x435310). Their purpose is unclear.

### 2.6 Object factories

- **0x446950 `CreateWarMapObject(type)`.** Allocates from the selectable-object pool (0x440740):
  - type 1–0x22 → 0x78 bytes, ctor 0x4A3BD0
  - 0x2F–0x31, 0x6C–0x6E → 0x84 bytes, ctor 0x4239C0 (resource buildings)
  - 0x23–0x2E, 0x32–0x3A, 0x3C, 0x60–0x6B, 0x6F–0x7A, 0x116, 0x1BC, 0x1C3–0x1C8, 0x273–0x276, 0x33E → 0x80 bytes, ctor 0x42A450
  - anything else → NULL

  Callers: 0x421B20, 0x42B150 and 0x43CD10/0x43CEB0/0x43D4F0 (save loading and war-map construction).
- **0x446B30 `CreateBattleObject(type)`.** A binary-search switch over type ids up to about 0x3E8. There are three allocators:
  - 0x440740: the selectable pool, 1024 objects
  - 0x487610 and 0x487910: the transient and effect pools, 512 each

  Some type → ctor mappings, read from the code:

| Type ids | Size | Ctor |
|---|---|---|
| 1–6, 0xA–0x12, 0x1C–0x22 | 0x190 | 0x490820 |
| 7–9 | 0x1A0 | 0x493B90 |
| 0x13–0x15 | 0x190 | 0x493870 |
| 0x16–0x18 | 0x1A0 | 0x4935E0 |
| 0x19–0x1B, 0x4D–0x4F | 0x190 | 0x490120 |
| 0x32–0x39, 0x6F–0x76 | 0x190 | 0x48F750 |
| 0x3E–0x4C, 0x50–0x5C | 0x194 | 0x48FC20 |
| 0x5D–0x5F | 0x194 | 0x48F970 |
| 0x23–0x31, 0x3A–0x3C, 0x60–0x6E, 0x77–0x7A, 0x116 | 0xF0 | 0x48E8A0 |
| 0xB3, 0xCB–0xCE | 0xC8 | 0x48F590 |
| 0xCF–0xD4 | 0xCC | 0x48F330 |
| 0x110 | 0xC8 | 0x48F290 |
| 0x13B–0x14A (transient) | 0xC0 | 0x48E680 |
| 0x14B–0x14D (transient) | 0xD4 | 0x48E2A0 |
| 0x14E–0x15A (transient) | 0xD8 | 0x48DD50 |
| 0x16F–0x173 (effect) | 0xC4 | 0x48DCB0 |
| 0x17B (effect) | 0xC0 | 0x487730 |
| 0x19A–0x19C | 0xF8 | 0x48DBE0 |
| 444 | 0xF8 | 0x48D510 |
| 445–447 | 0xF8 | 0x48D050 |
| 448 | 0xC8 | 0x48CFB0 |
| 451–456 | 0x118 | 0x48D780 |
| 0x253–0x255 (effect) | 0xD4 | 0x48CD80 |

The rest of the switch falls into shared targets around 0x447773–0x447B78: ctors 0x48C8E0, 0x48B590, 0x48B480, 0x489E20, 0x489DB0, 0x489820, 0x489380, 0x4890A0, 0x488FB0, 0x4873F0, 0x488E80, 0x488CC0, 0x488930, 0x4887E0, 0x488460, 0x488410, 0x488180 and 0x487B00. All of these are 0xEC-byte objects, mostly from the transient and effect pools. My guess is that the 0x48xxxx ctors are vehicles, buildings, projectiles and effects; part 0x48 has the names. **Use this table, not guesses, when porting object creation.** There are 23 callers, including the landscape loader 0x47D240 and the vehicles.

### 2.7 Unit groups and the order panel

`WGroup` has ctor 0x447DC0 and vtable 0x4AA800.
- **Fields:** `+0x1C` side, `+0x20` member list (std::list of objects), `+0x24` count, `+0x28` flag (members share a sector), `+0x2A` flag.
- **Id.** `+0x8` is a unique id from `dword_4BEFBC` (starts at 1, ++). `dword_566264` counts live groups.
- **Callers:** 0x425B80 (war-map clunting) and 0x480190 (battle-map unit groups).

| Address | Name | What it does |
|---|---|---|
| 0x447FE0 | `Clear` | Releases members (`member+0x34 = 0`) and notifies both players (0x4392C0 on `dword_4E9B74` and `dword_4E9B78`). |
| 0x4480F0 | `AddMember(obj)` | If `obj` was in another group (`obj+0x34`) it is removed from there first. Sets `obj+0x34 = this`. Calls vtbl[0x2C/4]. Clears `+0x28` if members are in different sectors (`obj+0x48`). Notifies both players. |
| 0x448210 | `RemoveMember(obj)` | |
| 0x4482D0 | message handler | See the list below. |
| 0x4484A0 | `AllMembersCan(cmd)` | Sends 0x8130 to each member and returns true if all answer. |
| 0x448500 | `BuildOrderPanel(panel)` | See the button table below. |
| 0x4490E0 | vtbl[10] | Calls 0x440610(0) on every member (e.g. deselect). |
| 0x449120 | `FirstMemberClass` | Returns `byte_51A190[type*0xB0]` of the first member, or 0. |

The message handler 0x4482D0 (vtbl[7]) handles:
- **0x805C** (fill panel): if the group isn't uniform, calls 0x448500; otherwise forwards to the first member.
- **WM_COMMAND 0x808C** (query): forwarded to the first member.
- **0x80A9**: broadcast to all members.
- **0x813E** with lParam 0x8065 or 0x813F: forwarded to the first member (Build, or Suspend/Resume).

The order panel built by 0x448500 adds buttons with `0x432E90(cmd, param, GType, tooltip text id)`. A button is shown only if a member answers the 0x8130 "can you do" query.

| Command, param | GType | Text |
|---|---|---|
| 0x813E, 0x8065 | 0x20 RBHbuild | 0x470 "Build" (when the leader's +0x128 is set) |
| 0x813E, 0x813F | 0x23 / 0x22 | 0x496 "Resume" / 0x495 "Suspend building tasks", toggled by +0x12C |
| 0x80CE, 0–3 | 0x34C–0x34F | 0x471 Avoid / 0x472 Retaliate / 0x473 Attack / 0x474 Roam and Attack (stances) |
| 0x80CE, 0x80B7 | 0x2D | 0x479 "Retreat" (only on the battle map, `0x43D010`) |
| 0x80CE, 0x8067 | 0x34B | 0x46F "Abandon" |
| 0x80F3, 0x80ED | 0x1E | 0x48A "Force Fire" |
| 0x80F3, 0x80EF / 0x80EE | 0x349 / 0x34A | 0x48C "Submerge" / 0x48B "Tunnel" |
| 0x80F3, 0x80D5–D7 | 0x25 | 0x482 "Black Dust" |
| 0x80F3, 0x80D8–DA | 0x26 | 0x483 "Shock Bomb" |
| 0x80F3, 0x80DB–DD | 0x351 | 0x484 "Explosives" |
| 0x80F3, 0x80DE–E0 | 0x352 | 0x485 "Infiltrate" |
| 0x80F3, 0x80E1–E3 | 0x353 | 0x486 "Pulse" |
| 0x80F3, 0x80E4–E6 | 0x354 | 0x487 "Freeze" |
| 0x80F3, 0x80E7–E9 | 0x355 | 0x488 "Scare" |
| 0x80F3, 0x80EA–EC | 0x356 | 0x489 "Control" |
| 0x80F3, 0x80F0 | 0x358 | 0x48D "Scan" |

**Group power pips.** 0x449150 is the ctor (vtable 0x4AA848) and 0x4492D0 the dtor. They build up to 8 sprite objects (0x40E700/0x40EC50 with GType 8, `hwpower.spr`), one per member of `group+0x4C`.
- Each pip's frame is `ftol(0.5 + 9 × hp / maxHp)`, so frames run from 0 to 9. hp comes from `vtbl[0xB4/4]` and max from 0x47DA20.
- The pips are bubble-sorted by frame, highest first.
- 0x449340 lays them out side by side, centred on x,y, and adds them to a draw list.
- 0x449330 sets the draw level `+0x28`.
- Callers: 0x42B2F0 and 0x42B850 (war map), 0x484120 and 0x4844A0 (battle map).

**Other group helpers.**
- 0x449410 registers `[Stats] "Holding Groups"` (int, default 0). Its value word `dword_56627C` is used as a live counter of WCmdTarget holding lists (incremented in 0x449450 and decremented in 0x4494F0). It is a debug stat.
- `WHoldingGroup` (ctor 0x449450 "WCmdTarget", vtable 0x4AA850, dtor 0x4494F0):
  - 0x449670 broadcasts 0x440610(arg) to members.
  - 0x4496C0 moves all members into another group (0x4480F0).
- 0x449B60 / 0x449BD0 `WCompositeList` (vtable 0x4AA880):
  - 0x449D90 and 0x449DF0 forward vtbl[5] and vtbl[6] (activate/deactivate) to the children.
  - 0x449E40 broadcasts a message (and vtbl[8]) to the children.
  - 0x449CF0 clears the list.

### 2.8 Spatial bucket grid

`WBucketGrid` has ctor 0x449710, vtable 0x4AA874 and dtor 0x4497F0. It is built by 0x439770 (battle-map setup).

| Field | Meaning |
|---|---|
| `+4` | cell width |
| `+8` | cell height |
| `+0xC` | columns = ceil(W/cw) |
| `+0x10` | rows |
| `+0x14` | cells, 0x28 bytes each (a WCompositeList; ctor 0x449B60, dtor 0x449BD0) |

| Address | What it does |
|---|---|
| 0x449850 | Cell index of (x,y), clamped |
| 0x4498A0 | Insert an object at x,y and return its cell |
| 0x449900 | Remove an object from cell i |
| 0x449920(x,y,q) | Returns the cell itself (q=0), or the neighbour cell on the side of the cell the point is nearer to: horizontal (1), vertical (2) or diagonal (3). This is for checking collisions across a cell edge. Callers: 0x47FC50 and 0x47FE10 (landscape) and 0x492C00 (vehicles). |

### 2.9 DeePlay: DirectPlay networking

**Globals.**

| Global | Meaning |
|---|---|
| `dword_586C20` | IDirectPlay3A |
| `dword_586C24` | provider GUID ptr |
| `dword_586C28` | my DPID |
| `dword_586C2C` | remote DPID |
| `byte_586C3C` | running |
| `byte_586C40` | is host |
| `byte_586C4C` | packet ready |
| `dword_586380` | receive buffer, 0x400 bytes |
| `dword_586820` | send buffer: u16 sequence, then payload |
| `dword_586C54` | send size |
| `dword_586C58` | sequence counter |
| `byte_586C5C` | guaranteed delivery |
| `byte_4D1231` | debug logging (logs go to `nullsub_4`) |

**Functions.**

| Address | Name | What it does |
|---|---|---|
| 0x449F40 | `DeePlay::Get` | Singleton at 0x586284, vtable 0x4AA8A4. Creates the send and receive events (`586C38` / `586C34`). Called by 0x402630 and the menus. |
| 0x449FA0 / 0x44A020 | dtor | DestroyPlayer, Close, Release, CloseHandle. |
| 0x44A0E0 | `Send(data,len)` | Copies into the send buffer and sets the send event. |
| 0x44A130 | `TakePacket(&p,&len)` | If a packet is ready, returns the receive buffer + 2 and length − 2. |
| 0x44A160 | **send thread** | Waits up to 10 s for the event, then `Send(myId, 0, guaranteed?1:0, buf, len)` (vtbl+0x68). Logs `"DeePlay::SEND packet # %d of %d bytes on cycle %d"` with the cycle from 0x438B00. |
| 0x44A290 | **receive thread** | `Receive(&from,&to,DPRECEIVE_ALL,buf,&0x400)` (vtbl+0x64). |
| 0x44A400 | `StartEnumSessions` | Thread 0x44A430: `EnumSessions(desc{size 0x50, app GUID 0x4BEFE0..EC}, 0, cb 0x44A4B0)`. |
| 0x44A4B0 | enum callback | Stores up to 10 sessions: GUIDs at `0x586780[16*i]`, names in the std::string array `0x5862E0` (10 × 16 bytes, made by 0x449EE0, freed by 0x449F10). Count is `dword_586C44`. Caller: the session menu 0x4073D0. |
| 0x44A5B0 | | Registers `[Network] "Session Name"` (string, default `"War of the Worlds - session name"`) at 0x5862A0. |
| 0x44A5F0 | `Host` | Closes any old session. `Open(desc, DPOPEN_CREATE)` with the session name and the app GUID. `CreatePlayer` named "Human player" / "Human Player". Starts both threads (stacks 0x100 and 0x800). Called from WGame 0x402B80. |
| 0x44A8D0 | `Join(sessionIndex)` | `Open(DPOPEN_JOIN)` on the enumerated session GUID. `CreatePlayer` "Martian player". Starts both threads. Called from 0x4073D0. |
| 0x44AB40 | `Close` | DestroyPlayer + Close. Callers: 0x402880, 0x402B80, 0x403AB0, 0x403C10, 0x403E40. |
| 0x44ABB0 | `Create(providerGuid)` | Releases any old object, then `DirectPlayCreate` and QI IDirectPlay3A (GUID 0x4AC980). Caller: 0x406F00, the service-provider menu. |
| 0x44AC50 | atexit stub | |
| 0x44A0B0, 0x44ACC0, 0x44ACF0 | static inits | Critical-section objects (0x434E50) at 0x5862C0, 0x586C80 and 0x586DB8. |

The receive thread handles system messages (from == 0):
- **DPSYS_CREATEPLAYERORGROUP (3)** with a player: records the remote id. **If we are host, it sends `WM_COMMAND 0xAB` to WGame (`dword_4D2538`)**, meaning the opponent joined and the game starts.
- **DPSYS_DESTROYPLAYERORGROUP (5)**: clears the remote id.
- Anything else is a game packet: it sets `byte_586C4C`.

Hosting is always "Human" and joining always "Martian", which agrees with FRONTEND.md.

### 2.10 DeeDraw: DirectDraw 2 display

The `DeeDraw` "object" is a set of globals (`0x586C60`–`0x586EB0`). 0x44AD20 returns the singleton `unk_586E68`.

**Globals.**

| Global | Meaning |
|---|---|
| `586DF4` | IDirectDraw2 |
| `586DE8` | primary IDirectDrawSurface3 |
| `586EA0` | back buffer |
| `586E34` | palette |
| `586E70` | clipper |
| `586E74` | hwnd |
| `586E98` / `586DEC` / `586E0C` | width / height / bpp |
| `586DE4` | bytes per pixel |
| `586DF0` | pixel format code: 8, 15 (555), 16 (565) or 32 |
| `586E08` | windowed |
| `586DE0` | flipping chain |
| `586CC8` | mode list, 12 bytes each {w,h,bpp}; count `586EA8` |
| `586CA8` / `586CAC` | 2 cursor image surfaces, 64×64; index `586E9C` |
| `586E28` | back-buffer save-under, with rects `586E18`/`586C68`; valid flag `586C78` |
| `586E2C` | primary save-under, with rects `586DF8`/`586E40`; valid flag `586DD4` |
| `586CA0` | cursor visible |
| `586DD8` / `586DDC` | last cursor position |
| `586E6C` | cursor object (0x40CB60; 0x40CD30 sets the shape from `dword_4EA438`) |
| `586E30` | a 0x49D620 object (RTypeISO-related; unclear) |
| `586DBC` | critical section |

**Set-up and surfaces.**

| Address | Name | What it does |
|---|---|---|
| 0x44ADC0 / 0x44AD60 | ctor / dtor | The dtor stops the movie thread (waits 50 ms, then TerminateThread), releases everything (0x44BB80), and deletes the cursor objects. |
| 0x44AE50 | `SetHwnd` | |
| 0x44AE60 | `SetMode(w,h,bpp,windowed)` | bpp 8 → 1 byte, 15/16 → 2, anything else gives the error `"DeeDraw::LockSurface: Invalid pixel depth"`. Then 0x44AF40 and 0x44B2D0. The cursor is hidden while this runs. Caller: Win95DeskTop 0x42C9B0. |
| 0x44AF40 | `CreateDirectDraw` | `DirectDrawCreate`, then QI IDirectDraw2. On the first call it runs `EnumDisplayModes` (cb 0x44BAC0). If the configured `[Screen]` mode (`dword_4EA3D8` × `cy` × `dword_4BAAC8`) is missing and bpp is 16, it **falls back to 8 bpp**; otherwise `"Failed to find a compatible default screen mode!"`. Windowed: `SetCooperativeLevel(DDSCL_NORMAL)`. Fullscreen: `EXCLUSIVE|FULLSCREEN`, then `SetDisplayMode(w,h,bpp)`; `"Screen mode does not exist"` if it isn't listed. |
| 0x44B2A0, 0x44BB20 | mode list lookups | |
| 0x44B2D0 | `CreateSurfaces` | 8 bpp: a palette with an initial grey ramp (`DDPCAPS_8BIT|ALLOW256`). Windowed: a primary surface, a w×h system-memory back buffer, four 64×64 cursor surfaces (0x44B780), the clipper on the hwnd, and 0x44E4B0 (part 08). Fullscreen: a primary with a 1-back-buffer flip chain, then `GetAttachedSurface(BACKBUFFER)` and 0x44B800 (cursor surfaces). Pixel format through 0x44C660. |
| 0x44B780 | `CreateSurface3(desc,&out)` | |
| 0x44BB80 | `ReleaseAll` | Every release loops until the refcount is 0. |
| 0x44BD20 | `ClearSurface` | `Blt` colour fill 0, with restore-on-lost and retry-while-drawing. |
| 0x44BD90 | `ClearScreen` | Clears primary and back buffer. Callers: 0x418220 and 0x42DB90. |
| 0x44BE80 | `RestoreSurfaces` | |
| 0x44BEA0 | error box helper | Message box (0x42E750, title "Error"), returns false. |
| 0x44BEC0 | `FlipToGDISurface` | Before system message boxes (called from 0x42E750). |
| 0x44BEE0 | `SetPaletteEntries(first,count,rgb)` | 8 bpp only. Caller: 0x40B120. |
| 0x44C3A0 | `CreateOffscreen(surfaceObj)` | `+4` w, `+8` h → `+0x1E` LPDDS. Caller: 0x40CF00. |

**Present and blitting.**

| Address | Name | What it does |
|---|---|---|
| 0x44BF90 | `Present` | Called from the renderer 0x40AF20. 1) Draws the cursor into the back buffer at the mouse position (`dword_4E9250`, `dy`) with 0x44C9D0. 2) Windowed: restores the primary under the old direct-drawn cursor, then Blts the back buffer to the client rect. Fullscreen: `[Screen] "BLIT instead of FLIP"` (`byte_4B92DC`, default 1) chooses BltFast back→primary over `Flip(DDFLIP_WAIT)`. 3) Restores the back buffer under the cursor from `586E28`. All of this runs inside critical section 0x586DBC. |
| 0x44C4F0 | `Lock(surface, desc)` | Fills `{+0 bits, +4 w, +6 pitch, +8 h, +0xA fmt, +0xC Bpp}`. |
| 0x44C580 / 0x44C620 | `LockBackBuffer` / `Unlock` | `586E38` holds the locked surface. Callers: 0x40DDB0 (renderer). |
| 0x44C640 / 0x44C650 | `PixelFormat` / `BytesPerPixel` | |
| 0x44C660 | `DetectPixelFormat` | |
| 0x44CFD0 | `ClipToScreen(src,dst)` | |
| 0x44D070 | `BltRetry` | `Blt` with `DDBLT_WAIT`, handling lost surfaces. |
| 0x44D110 | `BltFastRetry` | Caller: 0x40DAC0. |
| 0x44D1A0 | `ReportDDError(hr)` | Shows a "DDERR_*" message box (`"Unknown Error"` otherwise). 19 callers. |

**Cursor.**

| Address | Name | What it does |
|---|---|---|
| 0x44C700 | `MoveCursor(x,y)` | From the mouse handlers 0x403680 and 0x42CD40, between frames. If the position or shape changed: restores both save-unders and redraws directly on the primary (0x44CCE0). |
| 0x44C9D0 | `DrawFlipCursor(surf,x,y)` | Renders the cursor sprite into the alternate 64×64 cursor surface (Lock 0x44C4F0, draw 0x40CC70) when it is dirty (`cursor+0x48`, set by 0x40CD10). Sets a source colour key (`SetColorKey(DDCKEY_SRCBLT)`). Saves the area under it to `586E28`, then Blts with `DDBLT_KEYSRC|WAIT` (0x1008000). Debug strings: `"DeeDraw::DrawFlipCursor …"`. |
| 0x44CCE0 | same for the primary | Saves to `586E2C`. |
| 0x44CFA0 | `ShowCursor(bool)` | Handles Win95DeskTop message 0x8084. |

### 2.11 Movies

**0x44DCA0 `PlayMovie(name, placement*)`.** This is Win95DeskTop's handler for message 0x8041.
- **Placement** is `{flags (Smacker open flags), x, y, u8 opts[4]}`.
  - With no placement: flags 0x60 (0x6C windowed), centred, `opts[1]` = Escape skips, `opts[3]` = clear the screen afterwards.
  - With a placement in windowed mode, flags `|3` or `|0xC` depending on whether a position is given.
  - The meaning of `opts[0]` is unclear; `opts[2]` = a mouse click skips.
- **Thread.** Only in 16 bpp, or when Use Reaper is set, does it start a thread (stack 0x20000, handle `586C9C`). In 8 bpp without Reaper **no movie plays** (the function returns true).
  - 0x44DE20 (Smacker) if `[Screen] "Use Reaper"` is 0.
  - 0x44E060 (Reaper `.fmv`) otherwise.

**0x44DE20, the Smacker movie thread.**
1. Kills the game timer (0x402050).
2. QIs IDirectDraw (v1) and IDirectDrawSurface (v1) of the primary.
3. Builds the path: setting 0x4D12D0 (CD/install path) + `FMV\` + name + `.smk`.
4. **If the file exists**, stops all sounds (0x465B20) and calls `0x44F2C0(path, hwnd, dd1, back, dds1, &placement)`.
5. Optionally clears the screen.
6. Unless `byte_4D253C` (shutting down) is set, sends **0x8056** ("movie done") to WGame.
7. Restarts the timer (0x401FF0).

A missing file is skipped silently, and 0x8056 is still sent.

**0x44E060, the Reaper thread.** It works the same way with `.fmv` and Reaper's open/frame/close (0x478130 / 0x4780E0 / 0x477DC0 / 0x478300). It loops while `byte_586E50` is set and no skip is requested.

**Helpers.**
- 0x44E3E0 `MovieSkipRequested`: `opts[2]` and a mouse button (0x4038C0), or `opts[1]` and `GetAsyncKeyState(VK_ESCAPE)`. It is called per frame from 0x44F2C0.
- 0x44E420 `StopFmv`: clears `byte_586E50`. Called from WGame 0x402B80.
- 0x44E470 `CancelPendingFmv`: clears `586EAC`. Called from 0x404C10.
- 0x44DC60 registers `[Screen] "Use Reaper"`.
- 0x44AC00 is the static init of the default placement.

---

## 3. Data structures and globals

**AI player (class 0x442890).**

| Field | Meaning |
|---|---|
| `+0x43C` | phase |
| `+0x440` | group cursor |
| `+0x444` | group count |
| `+0x448`, `+0x44C` | turn clock (float) |
| `+0x450` | defending flag |
| `+0x454` | research object |
| `+0x45C` | battle tick count |
| `+0x460` | total strength |
| `+0x464` | side object (`[+0]` = side) |
| `+0x468` / `+0x46C` | battle sector / sector object |
| `+0x470` | task cursor |
| `+0x474` | target count |
| `+0x478` | task count |
| `+0x47C[16]` | targets |
| `+0x4BC[32]` | tasks |
| `+0x53C/+0x540/+0x544` | registered units list / head / count |

**AI tables (bss).**

| Address | Meaning |
|---|---|
| `0x545BB0` | 31 × 0x18 per-sector records: +4 groups heading there, +0x15 build ordered, +0x16, +0x17 flags |
| `0x545EB0[]` + `dword_546050` | candidate sectors |
| `0x5460B8[]` | priorities |
| `0x546230[]` | the AI's groups |
| `0x566280[]` + `dword_4BEFDC` | int stack |

**AI.ojd table.** `word_518B10[type*8]` is the scaled strength weight. `dword_518B14[type*8]` is the category: 1 combat unit, 2 static (a guess), 3 paired building 0x77/0x78, 4 factory, 6 resource building, 7 a building-site category. The per-side multipliers are `[Tweak] "AI strength table Human multiplier"` (0x5187F8+0x14) and `"… Martian multiplier"` (0x51A0D0+0x14).

**Unit type table** (`0x51A0F0`, 0xB0 bytes per type):

| Offset | Meaning |
|---|---|
| +0 | class (0x13, 0x16 = waterborne?) |
| +4 | research gate object |
| +0x10 | ? (0x51A100 == 1 test in 0x448500) |
| +0x4E | word (== 3 test) |
| +0x6C, +0x70, +0x74 | three resource costs? (floats used in 0x443610) |
| +0x78, +0x7C, +0x80 | buildable-by ids (0x442460) |
| +0xA0 | byte class (0x449120) |

**Other structures.**
- **Sector record table** `0x4EAE1C + s*0xC0`: owner. +0x7C… holds the resource stock read by 0x441E10 / 0x4607C0.
- **Order header** (§2.3), **WSectorObjects** (§2.4), **Zoomer** (§2.5), **WGroup** (§2.7), **WBucketGrid** (§2.8).
- **DeePlay** and **DeeDraw** globals are in §2.9 and §2.10.

**Players.** `dword_4E9B74` is the local player and `dword_4E9B78` the opponent (AI or network). The order target is `dword_566260`. `dword_4F73D0` = orders sent this cycle; `dword_4F73D4` = a network flag.

## 4. Messages, commands, strings and tuning

**Messages handled or sent in this piece.**

| Id | Meaning |
|---|---|
| 0x111 / 0xAD | Move group to sector (AI → group) |
| 0x111 / 0xAB | Network opponent joined (DeePlay → WGame) |
| 0x111 / 0x7A | Zoomer scroll position, packed x \| y<<16 (→ view) |
| 0x802D | AI turn tick (→ AI) |
| 0x8075, 0x807A, 0x807F, 0x80B1 | Battle-AI events (§2.2) |
| 0x80B2 | Set unit behaviour, params 0, 2 and 3 (AI → battle unit; a guess) |
| 0x80C0 | Attack object |
| 0x805C | Fill the order panel |
| 0x808C, 0x80A9, 0x813E | Group commands |
| 0x8130 | Can-you-do query to members; queried commands include 0x8131, 0x8132 and 0x80D5–0x80F0 |
| 0x80CE, 0x80F3 | Order-panel commands (§2.7) |
| 0x8056 | Movie finished (→ WGame) |
| 0x8041 | Play movie (→ Win95DeskTop) |
| 0x8084 | Show/hide cursor (→ Win95DeskTop) |

**Settings registered in this piece or used by it** (value at setting object +0x14, or +0x11 for bools):

| Setting | Default | Value at |
|---|---|---|
| [Tweak] AI Frontline Attractor for Defences | 2.0 | 0x4BED88 |
| [Tweak] AI Eff Threshold | 0.6 | 0x4BED8C |
| [Tweak] AI Panic Build Threshold | 500 | 0x4BED90 |
| [Tweak] AI Move Threshold | 1.05 | 0x4BED94 |
| [Tweak] AI Frontline Attractor | 2.0 | 0x4BED98 |
| [Tweak] AI Build Resfac Eagerness | 50 | 0x4BED9C |
| [Tweak] AI Aggression Value | 0.5 | 0x4BEDA0 |
| [Tweak] AI Invasion Threshold PC | 150 | 0x4BEDA4 |
| [Tweak] AI Outnumber Force | 1.1 | 0x4BEDA8 |
| [Tweak] AI Retain Force | 0.2 | 0x4BEDAC |
| [Tweak] AI Invader NFriend Mul | 3000 | 0x4BEDB0 |
| [Tweak] AI safe sector repeller Mul | 2.0 | 0x4BEDB4 |
| [Tweak] AI Minimum Rr | 8.0 | 0x4BEDB8 |
| [Tweak] AI Movement Temperature | 0.25 | 0x4BEDBC |
| [Tweak] AI Oracle (bool) | 0 | 0x566248 (0x442E40) |
| [Tweak] AI Hours Per Turn | 5 | 0x545760 (0x442E80) |
| [Debug] Enemy Visible | 0 | `dword_54604C` |
| [Tweak] Warmap Zoomer Speed | 1.5 | `dword_4E9F3C` |
| [Tweak] Battlemap Zoomer Speed | 2.5 | `dword_4F744C` |
| [Stats] Holding Groups | 0 (counter) | |
| [Network] Session Name | "War of the Worlds - session name" | |
| [Screen] Use Reaper | 0 | |
| [Screen] BLIT instead of FLIP | 1 (used here; registered at 0x40CE40) | |

The war-map/AI tuning registrations themselves are at 0x4424D0–0x442E80 (part 06).

**Strings used.**
- "WObject", "WCmdTarget", "Zoomer"
- "homeUnits" … "awayResources"
- "Human player" / "Human Player", "Martian player" / "Martian Player"
- DeePlay log formats
- DeeDraw errors: "DirectDrawCreate Failed", "DirectDraw2Create Failed", "SetCooperativeLevel Failed", "SetDisplayMode Failed", "Screen mode does not exist", "Failed to find a compatible default screen mode!", "Create surface Failed", "Create Surface 3 Failed", "Create lpDDClipper Failed", "Attach lpDDSBack Failed", "DirectDraw 1 Query failed Failed", "Create Surface 1 Failed", "DeeDraw::LockSurface: Invalid pixel depth", the three "DeeDraw::DrawFlipCursor …" strings, and about 90 "DDERR_*" names
- "FMV\\", ".smk", ".fmv"

**TEXT.ojd ids used** (by 0x448500, for the order panel): 0x46F–0x474, 0x479, 0x482–0x48D, 0x495, 0x496.

**GTypes used.**
- Order-panel buttons: 0x1E, 0x20, 0x22, 0x23, 0x25, 0x26, 0x2D, 0x349–0x358 (except 0x350 and 0x357).
- 8 (`hwpower.spr`) for the power pips.

## 5. Cross-references to other pieces

**Callers into this piece.**

| Caller | Calls |
|---|---|
| AI dispatcher 0x442EC0 and handler 0x442AC0 (part 06) | All the AI phases and the battle-AI handlers |
| War-map world 0x4185F0 | Creates the network player 0x445EA0 or the AI 0x442890 |
| WGame 0x402B80, 0x402630, 0x403AB0/C10/E40, 0x404C10 (part 01) | DeePlay host/close and the FMV controls |
| Menus 0x406C10/0x406F00/0x4073D0 (part 02) | DirectPlay create, enumerate and join |
| Win95DeskTop 0x42C9B0, 0x42CD40, 0x42DB90 | SetMode, MoveCursor, PlayMovie, ShowCursor, ClearScreen |
| Renderer 0x40AF20, 0x40DDB0, 0x40DAC0, 0x40B120, 0x40CF00 (part 01/02) | Present, Lock, BltFast, palette, offscreen surfaces |
| War map 0x41E000 / battle map 0x43A480, 0x43BF30 | Zoomer |
| Sector setup 0x439480 / 0x43C4C0 / 0x43C5E0 | Sector lists and SECT load/save |
| War-map clunting 0x425B80–0x425DD0 and battle-map groups 0x480190–0x4807F0 | WGroup |
| 0x42B2F0/0x42B850/0x484120/0x4844A0 | Power pips |
| 0x439770 | The bucket grid |
| 0x47D240, 0x47EE10… | 0x446B30 battle-object factory |

**Callees outside this piece.**

| Callee | What it is |
|---|---|
| 0x419240 | War-map world → sector |
| 0x43CA30 / 0x43C990 / 0x43C090 | Sector owner, usable, list |
| 0x4356C0 | Sector adjacency, `off_4BD260` |
| 0x437380 | Sector waypoint |
| 0x4406A0 / 0x4406E0 | Set/get object position |
| 0x441xxx | AI helpers (part 06) |
| 0x45F900–0x4607C0 | Build list (part 09/10) |
| 0x47D600 / 0x47EB00 | Battle-map unit of a war-map object |
| 0x482510 | Move to |
| 0x432E90 | Add order button |
| 0x438C50 | Append order |
| 0x438B00 | Game cycle |
| 0x40CB60–0x40CD30 | Cursor object |
| 0x44F2C0 | Smacker player (part 08) |
| 0x477DC0–0x478300 | Reaper FMV codec |
| 0x465B20 | Stop all sounds |
| 0x402050 / 0x401FF0 | Stop/start the game timer |
| 0x42E750 | Message box |
| 0x44E4B0 | Next piece: creates one more 64×64 surface, probably `586E14` |

## 6. Missing in WoWEngine: candidate tasks (in priority order)

WoWEngine currently has the front end, a partial war map (rendering, scrolling, clunting with provisional movement), `core/AiTable` (loading AI.ojd only), `core/SaveGame` (save-tag scanning, with SECT found by pattern), `MovieScreen` and its own cursor. None of the gameplay in this piece exists yet.

**P1. Computer opponent on the war map.** This is needed before a single-player campaign is playable.
1. **AI turn state machine.** Port 0x442EC0 and phases 0–9: 0x443030, 0x443340, 0x443610, 0x443CA0, 0x4440B0, 0x4443D0, 0x444690/0x444990 and 0x444C10, one phase per AI tick (msg 0x802D, cadence `[Tweak] AI Hours Per Turn` = 5 game hours; check the cadence at 0x442AC0 in part 06). Use the exact formulas in §2.1 and the 14 `[Tweak]` AI values, and keep the `rand()` call order, since the AI is seeded by the C runtime's rand. Needs AiTable's `word_518B10` weight scaled by the strength multipliers, and the category at `518B14`.
2. **AI build orders.** 0x444510 moves the builder and queues `{type,x,y,x2,y2}` from the AI.ojd site lists (0x4411B0). The 0x78 → 0x77 pairing must be kept. Depends on the war-map build list (0x460240).
3. **AI production.** 0x444C10 together with the build-list helpers 0x45F900–0x4603B0.
4. **`[Debug] Enemy Visible`.** Snaps AI counters to sector waypoints. It is a cheap debugging aid; worth adding as soon as the AI moves.

**P1. Sector object model and the SECT chunk** (0x4460E0–0x4465B0). Replace `SaveInfo::WarMapObjects`' pattern search with a real object-by-object reader: the SECT tag, then six (tag, list) pairs, with the order 4C, 50, 54, 58, 5C, 60 on load and save. Keep "home = side byte 0". Add a `GetList(kind, home)` with kinds 1, 2 and 3, and `Add`/`Remove` via the object's kind (0x440700). Saving must write the same tags.

**P2. Unit groups and the order panel** (0x447DC0–0x4490E0).
- Group membership: one group per object (`obj+0x34`). The "uniform sector" flag `+0x28` and the unique id counter `dword_4BEFBC` must match the original.
- The order panel's button list and conditions from 0x448500: Build, Suspend/Resume, the four stances, Retreat (battle map only), Abandon, and the 11 special-weapon buttons with their GTypes and tooltips (§2.7). Each button appears only if some member answers 0x8130.
- The power-pip strip (0x449150/0x449340): frame = round(0.5 + 9·hp/max), sorted descending, at most 8.

**P2. Zoomer smooth scroll** (0x446690/0x446850). The engine's `WarMap::CentreOn` jumps instantly. The original glides when it centres on an event or object (0x41E000), with a triangular speed profile: accelerate by `Warmap Zoomer Speed` = 1.5 px/tick² up to half way, then decelerate. The battle map uses 2.5. Minimap clicks: check whether 0x41E000 or a direct centre is used. The engine already centres directly; verify against 0x4192F0 / 0x41E000.

**P2. Player/order-buffer architecture** (0x438B20, 0x4460A0–0x4460C0, packet builders). Route every player action through the 4-byte-header order packets (types 1, 3, 4, 5 and 6), appended to the local player's buffer and executed in order each cycle. This keeps single player deterministic and makes network play and playback (0x42D290) possible later. Payload meanings belong to the order executor around 0x438C90–0x439230 (part 06).

**P3. Battle-map AI** (0x445010–0x445E94). Target list (16), task list (32), the planner 0x4456B0, the unit registration messages 0x80B2 with params 0, 2 and 3, and attack 0x80C0. This needs the battle map first.

**P3. Battle-object factory table** (0x446B30) and the war-map factory (0x446950). Port the type-range → class/size/pool mapping exactly as in §2.6, including the pool limits: 1024 selectable, 512 transient, 512 effect. War-map object creation (0x446950) is needed for proper save loading of WMOB objects (three classes: 0x4A3BD0, 0x4239C0 for resource buildings, 0x42A450).

**P3. Spatial bucket grid** (0x449710–0x449920) for battle-map collision lookups, including the quadrant-neighbour rule in 0x449920.

**P3. Movie details** (0x44DCA0–0x44E470). `MovieScreen` already plays and skips on Escape. Differences to check:
- (a) The original skips on a mouse click only when `opts[2]` is set; the default placement allows Escape only.
- (b) A missing movie file is silently skipped, but 0x8056 is still sent.
- (c) All sounds stop when a movie starts (0x465B20).
- (d) The screen is cleared to black afterwards (`opts[3]`).
- (e) The game timer is paused during the movie.
- (f) `[Screen] Use Reaper` selects `FMV\*.fmv`. No `.fmv` files ship on the discs, as far as known, so this can be ignored or documented.
- (g) In 8 bpp without Reaper no movie plays at all. That is a quirk; it probably shouldn't be copied, but note it.

**P3. Cursor behaviour** (0x44C700–0x44CFA0).
- The engine draws its own cursor every frame, which is fine.
- Win95DeskTop message 0x8084 hides and shows it (0x44CFA0), for example during movies; the engine already hides it over movies.
- The original redraws the cursor between game frames (0x44C700 on WM_MOUSEMOVE), so it stays responsive even when the game renders slowly. The engine renders at 30 Hz; consider drawing the cursor at display rate.

**P4. Display set-up quirks** (0x44AF40, 0x44B2D0, 0x44BF90). These are for the launcher or display settings only:
- If the configured 16-bit mode is missing, the game falls back to 8 bpp at the same size.
- `"BLIT instead of FLIP"` (default 1) only changes how frames are presented.
- 8-bit mode uses a hardware palette (`SetPaletteEntries` 0x44BEE0), which the engine replaces with shade-table rendering.

Nothing to port beyond honouring `[Screen] Size/BPP` and windowed mode.

**P4. Networking (DeePlay, 0x449F40–0x44ABB0).**
- The model: 2 players, host = Human, join = Martian.
- Session name `[Network] "Session Name"`, default "War of the Worlds - session name".
- Up to 10 enumerated sessions.
- The host starts the game with WM_COMMAND 0xAB when the other player's DPSYS_CREATEPLAYER arrives.
- Packets are a u16 sequence followed by one player's order buffer, sent once per game cycle (lockstep, via the network player 0x445F20/0x445F80).

A rebuild would replace DirectPlay with its own transport (e.g. TCP or SDL_net) but keep the lockstep order-buffer exchange and the 0xAB start trigger. This depends on the P2 order-buffer task. The FRONTEND menus (0xA4/0x9F/0xA6/0xAC) are already present as shells.

**Documentation fix.** Update ENGINE-MAP.md: 0x477DC0–0x47A000 is the Reaper FMV codec (`FMV\*.fmv`, enabled by `[Screen] Use Reaper`), not a "fatal error handler". The range 0x443340–0x444FDA belongs to the AI (it ends at 0x444FDA), and 0x445010–0x445E94 is the battle-map AI, not "WObject, sector stats".
