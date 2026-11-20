# Part 06: 0x437380 to 0x443340

The OBJ.ojd unit database, the sector objects (war map sector and battle map world), the player command queue, the AI player and AI.ojd.

The source is piece 6 of the IDA listing. There are 236 functions in the piece. All addresses are from the vanilla build.

Conventions:
- `T[x]` is the OBJ.ojd unit-type table entry at `0x51A0F0 + x*0xB0`.
- `S[n]` is the per-sector data block at `0x4EAE18 + n*0xC0`.
- "sector object" means the C++ object made by 0x439480. The world holds 31 of them at `WWarMapWorld+0x94`, fetched with 0x419240.
- I mark guesses as *guess* and write "unclear" where I could not work something out.

---

## 1. Overview

The piece holds most of the game's mid-level logic between the war map (WWarMapWorld, 0x417CD0…) and the battle map (0x47AF10 WLandscape, 0x480xxx/0x484A30 battle map, 0x4868B0 battle map menu):

| Range | Subsystem |
|---|---|
| 0x437380–0x4373CA | Sector geography getters: route waypoints in 0x4BAE68/0x4BAE70, and the "landlocked" byte table 0x4BD2E0 |
| 0x4373D0–0x437710 | `[Tweak]` static initialisers: VisRadMul and the armour, research and manufacture modifiers for each side and relation |
| **0x437750–0x438964** | **OBJ.ojd loader `LoadSetupData`**: 65 record types filling the unit table 0x51A0F0 and eight side tables |
| 0x438970–0x438AF4 | Pooled 24-byte "message objects" (the queue items used by the world message/task queue 0x434F00/0x435050) |
| 0x438B00–0x4392F1 | **WPlayer (local player) and its command queue**: name "Human"/"Martian", side, current selection group, and the command executor 0x438C90 |
| 0x439300–0x4393E8, 0x43FF80–0x4400A2 | World task list (tasks run once a tick, 0x440030), the base of WPlayer and the sector objects |
| 0x439400–0x4396F0, 0x439730, 0x43A440, 0x43C0A0–0x43C120 | More Tweak/Debug/System settings (water twinkles, Load scenery, ZMapRes, IMap Iteration Count, Battlemap Zoomer Speed, Resource Buffer, Distance Crippler VX/VY) |
| **0x439480–0x43D3EB** | **Sector object** (vtable 0x4AA578, 0x12C bytes): war map lists, ownership, the resource network, entering and leaving the battle map, battle-end detection, battle map keys/mouse/commands, reinforcement entry points, the scenery editor |
| 0x43D4F0 | Add a new object (unit/building) to a sector, placing buildings at AI.ojd build sites |
| 0x43D760–0x43FBE0 | Debug "place object" popup menus (object type name to object type id; see 4.4) |
| 0x43FD10–0x43FF73 | Small math helpers: 3x3 neighbour sum, distance, octagonal distance, 12-bit angle wrap, arrow geometry |
| 0x4400C0–0x4407E0 | **WSelectableObject**: id table, pooled allocator (1024 objects per block), info page, name lookup |
| 0x440860–0x440D70 | **SGroupList** (the world's list of unit groups, world+0x58) |
| 0x440DD0–0x441420 | AI.ojd build-site lists: 2 x 32 `std::list`s, linking sites to real buildings |
| **0x441500–0x4419BE** | **AI.ojd loader** and its strength multipliers |
| 0x4419E0–0x442460 | AI helper queries: sector strength sums, neighbour sector lists, resource views, research and prerequisite checks |
| 0x4424B0–0x442E80 | AI Tweak settings and **the AI player class** (vtable 0x4AA680, 0x548 bytes) with its message handler |
| 0x442EC0–0x443338 | AI hourly state machine (10 states) and state 0 (strength-sorting its units). States 1–8 are in part 07. |

I found no hand-written assembly in this piece. Everything has normal MSVC frames, SEH prologs and `this`-in-ecx conventions. The OBJ.ojd loader keeps a redundant "bytes consumed" counter in `ebx` that nothing reads, which is compiler output from the original source, not hand code.

---

## 2. Subsystems

### 2.1 OBJ.ojd loader, 0x437750 (`LoadSetupData`)

It is called once from game initialisation (0x402630+0x1A7).

1. It zeroes the unit table with `rep stosd` (0xAD6C dwords from 0x51A0F0, i.e. 0x2B5B0 bytes = **1009 unit types**, 0..0x3F0; 0x5456A0 is the next object).
2. It writes these defaults into every type (0x437767..0x4377ED):

| Field | Default |
|---|---|
| +0x0C faction | 2 |
| +0x40 (byte) | 0x10 |
| +0x43 (byte) | 1 |
| +0x46 | 0 |
| +0x4A armour | 100 |
| +0x50 | 0 |
| +0x58 | 1 |
| +0x92 | 90 |
| +0x98 | 0x250 |
| +0xA0 (byte) | 0 |
| +0xA1 (byte) | 0xC0 |
| +0xA8 scale | 1.0 |

3. It opens `obj.ojd` through 0x415200/0x415120 (FILE_Load).
4. It loops over the records. Each record is 3 skipped bytes, an `int16` type, then the payload. A `switch` on the type at 0x43785F (65 cases, table 0x438860) handles each one; a type above 64 is ignored with no payload skipped.

Loader state (stack variables):

| Variable | Meaning |
|---|---|
| `var_38` | The "current index", from record 0. It is the GType id, and also the index used by records 6, 9, 10 and 11. |
| `var_30` | GType class, from record 1 |
| `var_48` | Load-on-use flag, from record 7 |
| `var_44/40` | Hotspot, from record 5 |
| `var_34`/`edi` | The current unit type, from record 15 |
| `var_18` | The category given to new unit types, from record 17 |
| `var_3C` | The current scale, from record 25; it starts at 1.0 |
| `var_49` + `var_14..8` | A pending "part box", from record 27 |
| `var_28` | The largest record-30 value seen; never used afterwards |

The full record table is in **3.1**.

### 2.2 Message-object pool, 0x438970–0x438AF4

| Addr | Name | What it does |
|---|---|---|
| 0x438970 | `MsgObj::operator new` | Objects of 0x18 bytes or less come from a free list (0x4F73C8). Blocks of 0x1800 bytes (255 items) are recorded in 0x4F70A0[0x4F73C0]; the live count is at 0x4F73C4. Larger objects use plain `new`. |
| 0x438A00 | `MsgObj::operator delete` | Pushes the object onto the free list. When the live count reaches 0 and there are more than 2 blocks, it frees them all ("UNLOADING ALL MESSAGE OBJECT MEMORY"). |
| 0x438A80 | `Msg::Clear` | Zeroes +4..+0x14 |
| 0x438AA0 | `Msg::Set(a,b,c,d,e)` | Fills +4,+0xC,+0x10,+0x14,+8 |
| 0x438AD0 | `Msg::Copy` | |
| 0x438B00 | Getter for 0x4F73D4 | Used by DeePlay 0x44A160; unclear |

These are the 24-byte queued messages of the world message queue (callers 0x434F00, 0x434FA0 and 0x435050 in part 05).

### 2.3 WPlayer and its command queue, 0x438B20–0x4392F1

0x4185F0 creates two player objects: **0x4E9B74** (the local player, always 0x438B20) and **0x4E9B78** (the opponent). The opponent is the AI player 0x442890 (0x548 bytes) in a single-player game, or 0x445EA0 (0x43C bytes, the remote network player) when 0x4185F0's argument is set.

The class derives from strstreambuf and the task base 0x4354C0. Its vtable is 0x4AA528, whose slot +0x30 is the flush routine 0x438BF0. Fields:

| Offset | Meaning |
|---|---|
| +0x1C | `isLocal` = (arg4 == 0) |
| +0x20 | 2 |
| +0x24 | **side** (0 Human, 1 Martian) |
| +0x28 | Write pointer |
| +0x2C | Bytes queued |
| +0x30..0x42F | Command buffer (1 KB) |
| +0x430 | Pending new group (SGroup, 0x28 bytes, made by 0x449450) |
| +0x434 | Current selection group |
| +0x438 | 1 |

Global 0x4F73D0 = !IAmHuman.

| Addr | Name | What it does |
|---|---|---|
| 0x438B20 | `WPlayer::WPlayer(name, remote, side)` | |
| 0x438BB0/0x438BD0 | dtor | |
| 0x438BE0 | stub, returns 0 | streambuf virtual |
| 0x438C50 | `WPlayer::Append(data,len)` | Copies a command into the buffer |
| 0x438BF0 | `WPlayer::Flush` | Walks the buffer. Each record: byte +2 = type (bit 7 cleared), byte +3 = length. It runs each one with 0x438C90, then resets the buffer. |
| **0x438C90** | `WPlayer::Execute(cmd)` | Switch on the command type (table 0x438EA8/0x438EDC); see the table below |
| 0x438F00 | `ClearSelection` | Drops the pending group (0x449670) or releases the current one (vtbl+0x28) |
| 0x438F50 | `NewSelection` | Makes a new SGroup (0x449450) at +0x430 |
| 0x439000 | `SelectObject(cmd)` | Adds the object to the group (0x4480F0 or 0x47D190) and marks it selected (0x440610, msg 0x8012). For the local player with exactly one unit selected and "Show trivial info" (0x4D2554) set, it plays the unit type's selection voice (`T[t]+0xA0`) through 0x464EE0 with sound 0x109. |
| 0x4391F0 | `HasSelection` | |
| 0x439230 | `GetSelectionGroup` | Converts the pending group into a real one (world vtbl+0x24, 0x4496C0, msg 0x800F) |
| 0x4392A0/0x4392C0 | `OnGroupDestroyed` | IDA mislabels the caller as `std::locale::_Locimp::~_Locimp` |
| 0x4392E0 | `GetSelectionVoice` | 0x449120 |

0x438C90 command types:

| Type | Action |
|---|---|
| 2, 3 | Clear selection, then refresh the UI (0x402950) if local |
| 4 | Start a new selection |
| 5 | Select an object (by id) |
| 10 | World 0x8050 with arg |
| 11 | World 0x8051 |
| 12 | World 0x8054 with arg |
| 13 | World 0x8055 |
| 22 / 23 | Object(id)+0x54 gets 0x8026 with 0/1 and data |
| 24 | World gets **0x8028** with the whole command: create an object (see 0x43D4F0) |
| 25 | Object(id) gets 0x8026 |
| 26 | Sector(id) gets 0x8043 with data |
| 6–9, 14–21 | Forwarded to the current group (vtbl+0x24): orders such as move or attack |

`dword_4D2550` is the "current world", the target of these messages. It is the war map world, or the sector while its battle map is open (set at 0x4397C8).

Commands are built with 0x4460A0 (`Cmd(player,type)`) and posted with 0x4460C0. Other builders are 0x447D80/0x447DB0 (select), 0x4493E0/0x4A4470 and 0x449A70/0x449AB0.

### 2.4 Task base and world task list, 0x439300 / 0x43FF80–0x4400A2

| Addr | Name | What it does |
|---|---|---|
| 0x439300 | `WTask::WTask(name)` | vtable 0x4AA560 (base 0x435640). +0x10 is the owning list, +0x14 the next task. |
| 0x439350 | `WTask::~WTask` | Unregisters from the list (0x440080) |
| 0x4393B0 | `WTask::SetList` | |
| 0x4393D0 | `WTask::Unregister` | |
| 0x43FF80 | `WTaskList::WTaskList` | vtable 0x4AA610 (made at 0x435310) |
| 0x43FFD0 | `WTaskList::~WTaskList` | |
| 0x440030 | `WTaskList::RunAll` | Calls vtbl+0x10 on each task, coping with a task that removes itself |
| 0x440060 | `WTaskList::Add` | |
| 0x440080 | `WTaskList::Remove` | |

### 2.5 Sector object, 0x439480… (vtable 0x4AA578)

WWarMapWorld makes 31 of them (0..30; 0 is the sea) at 0x417F56: `new(0x12C) sub_439480(world, n, Text(2+n))`. The class is based on 0x4460E0 (a WCmdTarget-style object).

Fields:

| Offset | Meaning |
|---|---|
| +0x18 | **The battle map is open** (byte; the keyboard is only handled when it is set) |
| +0x1C | The world |
| +0x44 | Paused/frozen (byte; 0x43A980 does nothing when it is set) |
| +0x48 | Sector number |
| +0x4C / +0x50 | Units: player's side / other side (HUNI / AUNI) |
| +0x54 / +0x58 | Constructions (HCON / ACON) |
| +0x5C / +0x60 | Resources (HRES / ARES) |
| +0x64 | Pod-landing pending: on entry it creates a Pod, object 0xB3 |
| +0x68 | Day countdown |
| +0x6C | Hours accumulator |
| +0x74..+0x98 | Zeroed in the ctor; 0x43C3F0 reads +0x74/+0x78 |
| +0x84 | WFogOfWar (0x4346C0) |
| +0x88 | WLandscape (0x47AF10) |
| +0x8C | Damage map (0x404EA0, refcounted) |
| +0x90 | Battle map (0x484A30) |
| +0x94 | Battle map menu (0x4868B0) |
| +0x9C..+0x117 | 31 dwords zeroed |
| +0xA0..+0x11B | 30 pointers to "distance to sector j" objects (0x4A39E0, 16 bytes, +8 = distance); made on command 0xB7 |
| +0x118 | Zoomer (0x449710) |
| +0x124 | "Entered as attacker" flag (byte) |
| +0x128 | Last 0x80F4 argument |

0x446370(kind, mySide) picks a list: kind 1 units, 2 constructions, 3 resources; mySide ? +0x4C/+0x54/+0x5C : +0x50/+0x58/+0x60. 0x4216D0 is a list's count (+0x24).

Vtable 0x4AA578:

| Slot | Addr | Name |
|---|---|---|
| 0 | 0x439610 | Scalar dtor |
| 1 | 0x4356A0 | (base) |
| 2 | 0x43C4C0 | `Load(stream)` |
| 3 | 0x43C5E0 | `Save(stream)` |
| 4 | 0x43A980 | `Tick` (battle map) |
| 5 | 0x439770 | `EnterBattleMap(arg)` |
| 6 | 0x439ED0 | `LeaveBattleMap` |
| 7 | 0x43ADA0 | `HandleMessage(msg, wparam, lparam)` |
| 8 | 0x446310 | (base) |
| 9 | 0x43A600 | Make the "BattleMapSelection" view (0x480190, 0x1E0 bytes) |
| 10 | 0x43C790 | Add object (0x4463C0), then refresh the shading (0x4192F0) |
| 11 | 0x43C7C0 | Remove object (0x446460), then refresh the shading |

Functions:

| Addr | Name | What it does |
|---|---|---|
| 0x439480 | `WSector::WSector(world,n,name)` | Zeroes the fields and the sector's six 6-dword resource arrays in S[n], sets `S[n]+0 = 0` and `S[n]+4` owner = 2, makes the damage map (0x404EA0), and sets 0x4F7650[n] = 0 |
| 0x439630 | dtor | Releases +0x8C and +0x88, base dtor 0x446270 |
| 0x4396D0 | `GetBounds` | Rect at 0x4BAE78 + n*32 into 0x40AED0 |
| **0x439770** | `EnterBattleMap` | Clears the incoming-unit counters 0x4F76F0[32] (boats) and 0x4F86B8[32] (land). Sets 0x4F873C = this and **0x4D2550 = this**. Queues player command type 2. Makes WLandscape(n) at +0x88 and loads the shade tables (0x40B570). Works out 0x4F7430 = the enemy's unit + construction count. Sizes the damage map to the landscape, then the zoomer, fog of war, battle map and battle map menu. Clears 0x59D9C0[256] and sets 0x59D9B8 = 1. If `Load scenery` (0x4F73F1) is set, loads **`dat\%d.nsb`** (n = sector) into world+0x11C. Makes 0x4F8738 (0x483DF0) and puts all six lists onto the battle map (0x422910/0x4216E0/0x421FE0). Creates the Pod if +0x64 is set. Centres the start view on the player's HQ (type 58 Command Post / 278 Parliament in sector 30 for the Humans; 120 Communications Centre / 830 Martian Base in sector 3 for the Martians), or on the map centre, through 0x47B170 and the iso projection below; the result goes in 0x4E9B68/0x4E9B6C. Fills the battle info 0x4E9B7C (+0x40/+0x44 unit counts, +0x30/+0x34, +0x18/+0x1C totals, +0x6C defender = owner, +0x54 = "a battle is on"). Builds the reinforcement entry points (0x43D150), refreshes the fog, sends 0x813C to the battle map and **0x7A** (scroll to x,y) to itself. If only one side is present it sends **0x8075** (battle starts) to both players. Then 0x44E5B0(0) and 0x41B360. |
| **0x439ED0** | `LeaveBattleMap` | Clears the incoming counters and sends **0x807F**(n) to both players. Unregisters, queues player command type 3, empties world lists +0x118/+0x11C/+0x120 (0x449CF0), frees the zoomer, landscape, battle map, menu, fog and 0x4F8738. Sends 0x8141 to both unit lists and clears the groups (0x440B40). Recomputes the owner (0x43CA40), stores `S[n]+0 = HasCommandBuilding`. If the attacker left without taking the sector (+0x124, battle not ongoing, not decided), sends **0x813D** to the losing side's unit list (*guess*: "retreat"). Fades all sound over 30 ticks (0x464AB0(0x1E)). |
| 0x43A160 | `CountType(type, side, kind)` | Counts objects of a type in this sector's unit list, or in its construction + resource lists |
| 0x43A250 | `RemoveAllForSide(side)` | Removes every object of a side (vtbl+0x2C) and its war map counter (0x420930). Recomputes the owner; if a resource went, sends 0x8044, then world 0x8047 and 0x8045 (resource network refresh). |
| 0x43A400 | `ToggleBattleMenu` (Tab) | Reads the menu's +0x40 bit 28, sets cursor mode 0 or 5 (0x42F310), and calls battle map vtbl+0x38 |
| 0x43A480 | `CentreOnSelection` (Home) | Group bounds (vtbl+0x30), projected to the screen; makes a Zoomer (0x446690, 0x44 bytes) with speed `[Tweak] Battlemap Zoomer Speed` (0x4F744C, default 2.5) |
| 0x43A5E0 / 0x43A5F0 | Landscape width / height | +0x88 +0x1C / +0x20 |
| 0x43A670 | `WaterTwinkles` | Picks `[Tweak] Water twinkles per frame` (0x4F76E4, default 20) random cells, 20 px in from the edge. On water depth 1–3 (landscape cell byte +8) it spawns, with chance `rand() >= depth*10000-10000`, object 0x3CA + rand()%3 with scale 0.5, 0.5, life 100 and +0x4C+0x26 = 8. It only runs when byte 0x4B880C is set. |
| 0x43A800 | `ComputeBattleResult` | Recomputes the owner and fills the battle info 0x4E9B7C. It sets the "game won" flag +0x55 when sector 3 is taken with 0x4EB05C == 0 (no Martian base), or sector 30 with 0x4EC49C == 1 (London). Calls vtbl+0x78 on every object in 0x59DDC8[0x59DDC0]. |
| **0x43A980** | `Tick` | Updates the fog when `0x4D2558 % 0x4EA614 == 0`, runs the water twinkles, then checks for the end of the battle (below). |
| 0x43AC10 / **0x43AC40** | `HasRoomFor(type,…)` / `HasRoom(isLand, extra)` | Room check: own land units + 0x4F86B8[n] + units en route (0x420060) + extra < `Max units in sector` (0x4EA1B4); boats + 0x4F76F0[n] + … < `Max boats in sector` (0x4EA1CC) |
| **0x43ADA0** | `HandleMessage` | See 4.1 |
| 0x43BE10 | Command 0x8094 | Pick object id: queue player command type 4, then 0x47D590 |
| 0x43BE50 | `NextUnit` (',' key) | Cycles through the player's units (cursor 0x4F8740, 0x421D70) and scrolls to it (0x7A) |
| 0x43BF30 | `NextConstruction` ('.' key) | Same for constructions, with a zoomer |
| 0x43C090 | stub | |
| 0x43C160 | `SpreadSupply` (msg 0x8047) | `S[n].supplyEff[i] = S[n].supply[i] + Σ_j S[j].supply[i] / (dist(n,j)*DistanceCripplerVX + 1) + ResourceBuffer`, for i in 0..5 |
| 0x43C210 | `SpreadDemand` (msg 0x8045) | `S[n].demandEff[i] = S[n].demand[i] + Σ_j S[j].demand[i] / (dist*DistanceCripplerVY + 1)` |
| 0x43C2B0 | Msg 0x8010 | Counts own units inside a rect (world+0x114++) and marks each (0x440680): drag-box highlight |
| 0x43C340 | Msg 0x8011 | Box select: queues a select command (type 5) for each own unit in the rect; Shift (VK 16) adds to the selection |
| 0x43C3F0 | `OnLButtonUp` | Cursor mode 0x4EA43C. Mode 3 (build placement) queues command type **0x18** (create object 0x4F7428 at the point). Mode 2 (order) with a group queues command type 8 (move/attack at the point), then type 2, and plays the selection's acknowledge sound 0x11F. |
| 0x43C4C0 / 0x43C5E0 | `Load` / `Save` | See 3.4 |
| 0x43C6D0..0x43C770 | Small wrappers | world+0x120 / +0x11C 0x47D190 (add to render list) or 0x440A90 (remove) |
| 0x43C7F0 | `CanBuildHere(type)` | Called from the build list 0x462DC0: the type's faction (+0xC) must equal the sector owner and the count of that type (category +0x10: 1 units, 2 constructions, 3 resources) must be below `T+0x90` (max per sector) |
| 0x43C8D0 | `IsUndefended` | No units of the owner, and no AI class-7 (defence) buildings. Called by 0x41AB00, the UndefendedSectorLost logic. |
| 0x43C990 | `HasCommandBuilding` | Already in WARMAP.md: Human types 58 / 278 (sector 30); Martian 120 / 830 (sector 3); finished = 0x42AA70 |
| 0x43CA30 | `GetOwner` | `S[n]+4` |
| **0x43CA40** | `RecomputeOwner` | See below |
| 0x43CB20/0x43CB60/0x43CBB0/0x43CC00 | Apply to both unit lists | 0x421A50, 0x421AA0 (+world+0x74 0x4203C0), vtbl+0x14, vtbl+0x18 |
| 0x43CC40 / 0x43CC70 | `++/-- S[n].count[i]` | 0x4EAE38 |
| 0x43CCA0 | `AddDemand(base, a, b, c)` | Adds to `S[n].demand[base..base+2]` (from 0x429640) |
| 0x43CD10 / 0x43CEB0 / 0x43CFA0 | Add an existing object to the sector | Moves a WMOB here: 0x803A, owner recompute, war map counter 0x4205F0, messages 0x8044/0x8045/0x8047, 0x80A6/0x80A7, 0x80D1/0x80D2 (callers 0x4620B0, 0x41A330, 0x425C50) |
| 0x43D010 | `IsQuiet` | Returns 0 when both sides have armed forces here, i.e. a battle is due. Used by the calendar speed check 0x41A450 (*guess* on the meaning of 0x422860/0x4228B0: counts of armed units/buildings). |
| 0x43D090 / 0x43D0F0 | Count land / sea units of a side | moveClass ≠ 3 / = 3; used by the route step check 0x42ABD0 |
| 0x43D150 | `BuildReinforcementPoints` | See below |
| 0x43D3F0 | `FindReinforcementPoint(…, kind)` | Nearest table entry of the matching kind; called by the battle map 0x480EF0 |
| **0x43D4F0** | `AddNewObject(type, packedPos)` (msg 0x8028) | Makes a WMOB (0x446950), sends 0x803A, recomputes the owner, sends 0x80AC. On the war map, for a non-unit, it takes the next free AI.ojd build site for the type (0x4411B0) and sends **0x80D1**(x,y) and **0x80D2**(x2,y2) to the object's battle object, then relinks the sites (0x440EA0). It adds the war map counter when shown (0x4205F0). On the battle map it places the new units at the point, 0x20 apart in x. Then: if the sector owner changed, world 0x4192A0(1); for a resource, 0x8044, 0x8047 and 0x8045. |

`RecomputeOwner` (0x43CA40): "player present" = player units + constructions − 0x421740 (*guess*: non-combat units). "Enemy present" is the same for the enemy. If both are present the owner is unchanged; if one side is present it becomes the owner; with neither it becomes 2. On a change it zeroes `S[n].demand[0..2]` and notifies the world (0x418890). It returns "changed".

`BuildReinforcementPoints` (0x43D150) runs when a battle is on or Debug 0x4D1279 is set. For every other sector j (1..30) that has its owner's command building, and each of 3 approach kinds (7, 5, 1), it gets j's border point into this map (0x436050). The point is clamped 100 px inside the map; kind 5 uses unit type 0x13 (boat) and the others type 1 (land). If j can send units (0x43AC40), it records {x, y, j, kind, isOwnSide} in **0x4F7458** (24-byte entries, count 0x4F76E8) and spawns marker object **0x3DB** there (+0xCC = j, +0xC8 = kind).

### 2.6 Battle end and outcome (0x43A980)

While `0x4E9B7C+0x54` (a battle is on) is set and 0x4F7450 == 0:

- If byte 0x4E9B94 is set (*guess*: forced quit or retreat), it posts 0x76 with a delay of 0x96 and sets 0x4F7450.
- If the player has no constructions and no live battle units (0x47D610/0x47D660, chosen by side), it calls 0x43A800 and then:
  - If the defender (`+0x6C`) is the player, it shows the dialog (0x432520, 0x9C bytes) **0x49D "Sector Lost"**, button command **0x76** (back to the war map).
  - Otherwise it shows **0x49B "Attack Failed"** with 0x76.
- If the enemy has none left, it calls 0x43A800 and then:
  - If the defender is the player, it shows **0x49C "Invaders Repelled!"** with button command **0x8024** ("Decided to stay"; command 0x8024 only logs).
  - Otherwise, if `+0x55` (game won) is set, it shows **0x49A "Attack Successful!"** with 0x76.
  - Otherwise it shows 0x49A with 0x8024.

### 2.7 Debug object-placement menus and scenery editor

These are popup menus built by 0x49BAA0 (0x88 bytes) and 0x49BBF0 (add item: command **0x8027**, object type id, label). Picking an item sends 0x8027(type), which stores 0x4F7428 = type (the type to place). A click in cursor mode 3 then sends command 0x18 → 0x8028 → 0x43D4F0.

| Command | Builder | Contents |
|---|---|---|
| 0x8029 / 0x8061 / 0x8062 | 0x43D760 / 0x43D8D0 / 0x43DA40 | Human vehicles Mk1/Mk2/Mk3 |
| 0x802A | 0x43DB70 | Human buildings |
| 0x802B | inline | Human resources |
| 0x805E / 0x8063 / 0x8064 | 0x43DE80 / 0x43DFF0 / 0x43E170 | Martian machines Mk1/2/3 |
| 0x805F | 0x43E2D0 | Martian buildings |
| 0x8060 | inline | Martian resources: HumanFarm 108, CopperForge 109, HeavyElementFacility 110 (*note*: the labels are as the code has them) |
| 0x8068 | 0x43E590 | Village buildings |
| 0x8069 / 0x806A / 0x806B | 0x43E770 / 0x43EA10 / 0x43EC60 | Town buildings and LH (lighthouse/landmark?) variants |
| 0x8104 / 0x8105 | 0x43EEF0 / 0x43F1E0 | LH variants |
| 0x806F / 0x806E | 0x43F500 / 0x43F650 | C trees / D trees |
| 0x8070 | 0x43F7C0 | Terrain markers |
| 0x8071 | 0x43F940 | Explosion parts |
| 0x80FF | 0x43FA90 | Smoke |
| 0x8100 | 0x43FBE0 | Death explosions |
| 0x8103 | inline | Mines |

**0x8092** saves the battle map's scenery to `dat\%d.nsb` (0x44EA20 open for write, world+0x11C vtbl+0xC). It Beeps if the file can't be written. 0x439770 loads the same file when `[Debug] Load scenery` (default 1) is on. Together they form a built-in scenery editor.

The full name→id list is in 4.4. It is the best type-id table found so far.

### 2.8 WSelectableObject, 0x4400C0–0x4407E0

| Addr | Name | What it does |
|---|---|---|
| 0x4400C0 | Static init | id allocator object 0x518748 with 0x8000 ids (0x4A3350) |
| 0x440110 | `ObjectById(id)` | 0x4F8748[id] |
| 0x440120 | `WSelectableObject::WSelectableObject(type)` | Base 0x4354C0, named by the 4-char code at `T[type]+0x30`. +0x20 = type, +0x24 = isEnemy (faction ≠ player side), +0x38 = owning player (0x4E9B74/78), +0x8 = id from 0x4A35C0, registered in 0x4F8748. vtable 0x4AA630. |
| 0x4401E0 / 0x440270 | dtor | Frees the id (0x4A3590); the delete goes back to the pool |
| 0x440300 | `HandleMessage` | 0x807C(arg 2): returns the name, Text(`T+0x14`). 0x805C: if `T+0x18` (info page) is set, adds a context button 0x808C "Information" (text 0x478, icon 0x2A) through 0x432D70. 0x111/0x808C: opens the InfoPage (0x4A3E60, 0x88 bytes) for `T+0x18`. |
| 0x4403A0 | `OpenInfoPage` | |
| 0x440440 / 0x440450 | Getters | +0x2C (position), +0x20 (type) |
| 0x440460 / 0x440470 | | 0x435560 / 0x435570 wrappers |
| 0x4404A0 / 0x440550 / 0x4405C0 / 0x4406A0 | Virtual helpers | unclear; draw/name via vtbl+0x5C/0x60/0x30/0x34, 0x42EDC0 |
| 0x440610 | `SetSelected(b)` | +0x25; sends 0x8012 to itself and to +0x28 |
| 0x440670 / 0x440680 / 0x440690 | Getters/setters | +0x26 etc. |
| 0x4406E0 | `GetFaction` | `T[+0x20]+0x0C` |
| 0x440700 | `GetCategory` | `T+0x10`: 1 unit, 2 building, 3 resource |
| 0x440720 | `GetName` | Text(`T+0x14`) |
| 0x440740 | Pooled `operator new` | Objects of 0x1A0 bytes or less come from blocks of **1024** ("Allocating memory block for 1024 WSelectableObjects"): free list 0x5187E0, blocks 0x518768[0x5187E4], live count 0x5187E8 |
| 0x4407E0 | Pooled delete | Frees everything at count 0 ("UNLOADING ALL SELECTABLE OBJECT MEMORY") |

### 2.9 SGroupList, 0x440860–0x440D70

0x440860 is `SGroupList::SGroupList(name)`: base 0x4354C0, `std::list` at +0x20, count +0x24, vtable 0x4AA658. The world makes one named "SGroupList" at world+0x58 (0x417E7A).

| Addr | What it does |
|---|---|
| 0x4408F0 | dtor |
| 0x440A70 | Releases an element |
| 0x440A90 | `Remove(group)` |
| 0x440B40 | `Clear` (from 0x419030 and when leaving the battle map) |
| 0x440C60 | Message handler: forwards to every group |
| 0x440CB0 | Sends 0x8058 to groups whose vtbl+0x2C is false (called from the landscape, 0x47EAA0) |
| 0x440D10 | Sends 0x811A the same way (from the vehicles, 0x491870) |
| 0x440D70 | Frees a node |

### 2.10 AI.ojd, 0x441580 (and its build-site lists)

The AI ctor calls `AiData::Load(side)`, stores the result at AI+0x464 (8 bytes: +0 side, +4 average strength) and bumps the instance counter 0x5187F0.

The file is read with the usual record format (3 skipped bytes, `int16` opcode). The record table is in **3.2**. Each build site is 16 bytes `{int16 type, 0, int16 x, int16 y, int16 x2, int16 y2, ptr obj}`. It is appended to the `std::list` for its sector: **0x518990 + 12*sector** (the player's side's sites) if the record's side matches the side being played, otherwise **0x518810 + 12*sector** (the other side's). There are 32 lists each, built by the static initialisers 0x440DD0 / 0x440E50; a list is {alloc, head 0x518814/0x518994, size}.

| Addr | Name | What it does |
|---|---|---|
| 0x440EA0 | `LinkSites(sector, mySide)` | For each site, finds an existing construction of that type at (x,y) (0x440F80) and stores it in site+0xC |
| 0x440F80 | `FindBuildingAt(sector, type, x, y, side)` | Searches the side's constructions, then resources, for the type at the battle position (object 0x47D600 +0x78/+0x7C floats). A command building (types per 0x43C990) always counts as a match. |
| 0x441110 | `LinkAllSites` | Sectors 1..30, both sides (from 0x418500 at war map start) |
| 0x441160 | `GetSiteList(sector, side, &begin, &end)` | |
| 0x4411B0 | `NextFreeSite(sector, type, &x,&y,&x2,&y2, side)` | Finds a site with `site.type == T[type]+0` (the **base type**, see record 18) and no building. Used to place new buildings. |
| 0x441290 | Same, by AI class | (from part 07, 0x443CA0) |
| 0x441310 | `FirstBuildableSite(sector, class, side)` | The first free site whose type's AI class matches and which is buildable (0x4423D0) |
| 0x4413C0 | `UnlinkSites(sector, side)` | Sets every site's obj to 0 |
| 0x441420 | `RelinkObject(obj)` | Gives a building the first free site of its type and sends it 0x80D1/0x80D2 |

### 2.11 AI player, 0x442890 (vtable 0x4AA680)

0x442890 is the ctor: `AIPlayer(name, remote, side)`. It is a WPlayer (0x438B20) with these fields:

| Offset | Meaning |
|---|---|
| +0x43C | State (0..9) |
| +0x440 / +0x444 | Index / count of sorted units in 0x546230 |
| +0x448 | Hours accumulator (float) |
| +0x44C | Copy of +0x448 |
| +0x454 | Current research item (0x45B870 = done?) |
| +0x460 | Total strength |
| +0x464 | AiData |
| +0x470/+0x474/+0x478 | 0 |
| +0x47C | 16 dwords |
| +0x4BC | 32 dwords |
| +0x53C | Byte arg (side) |
| +0x540/+0x544 | A `std::list` |

The ctor also fills the pointer table 0x545EB0[] with 0x545778 + 12*i (90 entries of 12 bytes up to 0x545BB0). Their meaning is unclear (*guess*: a per-sector-pair scratch table for part 07).

| Addr | Name | What it does |
|---|---|---|
| 0x4429E0 | dtor | |
| 0x442A70 | Thunk | → 0x438BF0 flush |
| 0x442A80 | | Queues player command type N (0x4460A0/0x4460C0) |
| **0x442AC0** | `AIPlayer::HandleMessage` | Table below |
| 0x442D80 | `ApplyUpgrade(newType)` | Only if `T[newType]+0x10 == 2` (building). For every sector the AI owns with a command building (0x441A80), it retypes each AI construction whose type's `+0x84` or `+0x88` equals newType (in-place Mk upgrade). Sets the flag byte 0x56625C while it runs. |
| **0x442EC0** | `AIPlayer::OnHours(h)` | Table below |
| 0x443030 | State 0 `RankUnits` | Below |

`AIPlayer::HandleMessage` (0x442AC0):

| Message | Action |
|---|---|
| 0x4E (arg 2) | Returns 1 |
| 0x111 / 0xB9 | `ApplyUpgrade(lparam)` |
| **0x802D** (hours elapsed) | Runs the AI turn, 0x442EC0 |
| 0x8075 (battle starts in sector) | 0x445210, then registers with the world's task list (world+0x20) |
| 0x807F (battle map closed) | Unregisters, 0x445620 (part 07), relinks both sides' build sites for that sector, empties +0x540, sets the state to 0 |
| 0x807A | 0x445B90 |
| 0x80B1 | 0x445D60 |
| 0x80CB | nullsub on the AiData |
| 0x811E | Returns 1 |

`AIPlayer::OnHours(h)` (0x442EC0):
1. `+0x448 += h`. If there is a current research and it is done, clear it; if there is none, ask the research manager 0x4E9B8C (vtbl+0x2C) for one.
2. Run one state step:
   - 0: 0x443030
   - 1: 0x443340 (from 0x546230[i++]) until i == count
   - 2: 0x443610
   - 3: 0x443CA0
   - 4: 0x4440B0
   - 5: 0x4443D0
   - 6: 0x444690
   - 7: go to 8
   - 8: 0x444C10
   - 9: back to 0

   Most states move on when their function returns true; some skip a state (`+=2`). States 1–8 are in part 07. It also calls `_imeGetTime`, but the result looks unused.

`RankUnits` (0x443030) collects, for sectors 1..30, the other-side units list (sector+0x50) entries of AI class 1. For each it computes strength = count × AI strength (0x441E70). It builds a binary tree of 20-byte nodes (0x449AC0) with the root chosen nearest `min + (max−min)*0.5`, sums the strengths into +0x460, then walks the tree in order (0x449B10/0x449B30/0x449B50, an explicit stack) into **0x546230[]**, count +0x444. It returns true if the count is 0.

Helpers 0x4419E0–0x442460 (used by part 07):

| Addr | What it does |
|---|---|
| 0x4419E0 | `CanUnitEnter(sector, type)`: boats (base type 0x13 Ironclad / 0x16 Submersible) need a sector that isn't landlocked (0x4BD2E0) and has room |
| 0x441A40 | Returns the AiData side |
| 0x441A50 | `IsMine(sector)` |
| 0x441A80 | `IsMineWithHQ(sector)` |
| 0x441AD0 | Neighbours of a sector (0x4356C0 adjacency) into a byte list (0x445FF0/0x446020/0x446060) |
| 0x441B80 | Neighbours that are mine |
| 0x441C50 | Neighbours held by the enemy (not mine, not 2) |
| 0x441D30 | `GetResourceTotals(sector, &a,&b,&c)` via 0x423F80 kinds 0–2 (Human) or 3–5 (Martian) |
| 0x441DB0 | Effective demand triple `S[n]+0x50` (side 0 → [0..2], side 1 → [3..5]) |
| 0x441E10 | Effective supply triple `S[n]+0x80` |
| 0x441E70 | `UnitStrength(obj)` = group count × AI strength[type] |
| 0x441EB0 | `AiClass(type)` = 0x518B14[type*8] |
| 0x441EC0 | Sector units (other side) |
| 0x441EE0 | Sector constructions (other side) |
| 0x441F00 | `SectorAttackStrength(sector, …)`: sum over class-1 units, honouring the boat rule |
| 0x442080 | Similar sum (*guess*: incoming/neighbour strength) |
| 0x442330 | `SectorDefenceStrength(sector)`: sum over class-7 buildings |
| 0x4423D0 | `IsBuildable(type, consList)`: the research `T+0x04` is done (0x45B870), and the prerequisite building `T+0x94` (record 54), if any, exists and is finished |
| 0x442420 | `HasPrereqBuilding(type, consList)` (also used by the build list 0x462DC0) |
| 0x442460 | `IsResearchLinked(x, type)`: x is one of `T+0x78/+0x7C/+0x80` (records 49–51) and the type's research is done |
| 0x4424B0 | `ResetTypeFlags`: clears byte `T+0xAC` for all types (new game, from 0x418500) |

### 2.12 Math helpers

| Addr | What it does |
|---|---|
| 0x43FD10 | 3x3 weighted neighbour sum of a byte grid (centre ×8 plus 4 neighbours…); landscape smoothing at 0x47BD80 |
| 0x43FDA0 | Arrow heading/curvature from `asin` (used by RArrow 0x424A40) |
| 0x43FE90 | Arrow perpendicular offset |
| 0x43FED0 | Euclidean distance (float) |
| 0x43FF00 | Octagonal distance approximation: max + min/2 |
| 0x43FF30 | Difference between two 12-bit angles, wrapped to −2048..2047 |

**Iso projection** (0x439C05, 0x43A4F2, 0x43BE9A): `h = landscape.Height(x,y)` (0x47B170 or 0x47B120). Then `screenY = (y<<16)*0xB4B10D5D>>48 + y − ((h<<16)*0x2C3E63FB>>46) + landscape+0x48`. The constants are about y × 0.7071 and h × 0.0108 in 16.16 fixed point (*approximate*), and the screen centre comes from 0x4029C0. This matches ISOLib; copy it exactly from the listing.

---

## 3. Data structures and globals

### 3.1 OBJ.ojd record types (0x437750; payload sizes in bytes)

`T` = unit type selected by record 15. `idx` = the payload's own first int16. "cur" = the value set by record 0.

| Rec | Payload | Destination | Meaning |
|---|---|---|---|
| 0 | i16 | cur | Current GType id, also the index for records 6/9/10/11 |
| 1 | u16 | var | GType class |
| 2 | u16 len, name[len], u16 slot | 0x40E7A0 (GType table 0x4DAFD0) | Register a graphics file, both sides |
| 3 | same | | Human games only |
| 4 | same | | Martian games only |
| 5 | i16 x, i16 y | var | Hotspot for the next registration (reset after 2/3/4) |
| 6 | f32 a, f32 b, u16 c, u16 d | 0x4F0530 + cur*12: +0 c, +2 d, +4 a, +8 b | Per-index table read by the battle map 0x4802E0 (meaning unclear) |
| 7 | u16 | var | Load-on-use = (v == 0) |
| 8 | i16 idx, u16 len, name[len], 4×i16 (skipped), 6×i16 | **Sector table `S[idx]`**: name → `S+0xB0` (0x4EAEC8, landscape base name read by WLandscape 0x47B900); 6 ints → `S+0x08..+0x1C` (0x4EAE20..34) | Sector definitions. The six ints are read by the war map (0x423B00, 0x423F80) as per-sector resource values (*guess*: the 3+3 resource yields). |
| 9 | i16 idx, i16 v, then 3 × {u16 len, name[len], 4×f32} | 0x4DA828 + idx*64: +0 v, +4 first char of name1, +8..+14 4 floats, +0x18 char2, +0x1C..+0x28, +0x2C char3, +0x30..+0x3C | **Time-of-day lighting**, one entry an hour (0x40B030: v is the sun angle → sin/cos; the 3 named float sets are light colours/intensities) |
| 10 | 17×i16 | 0x4F6E38 + cur*0x44 (ints) | Per-index table read by the battle map 0x4802E0 (unclear) |
| 11 | i16, f32 | 0x4F04C0[cur], 0x4F0510[cur] | Per-index (cur < 8), battle map 0x4802E0 |
| 12 | 8×u16 | 0x4F2F10 + T*16 | Per-type words, read by 0x494CB0 (route finder/puppets area) |
| 13 | f32, 3×i16 | 0x4EC598 + T*16 (float, 3 ints) | Per-type vehicle values, read by 0x48DDC0 |
| 14 | i16 idx, i16 gtype, 4×i16, f32 | 0x59EDB8 + idx*24 | Route arrow styles (done) |
| 15 | i16 t | `T=t`; if `T+0==0`: `T+0=t`, `T+0x10`=var17, `T+0x90`=20 | Start a unit type |
| 16 | u16 len, data[len] | `T+0x30..0x33` = last 4 data bytes before the NUL, `T+0x34`=0 | 4-char code (the WSelectableObject name) |
| 17 | u16 | var | Category for the next new types: 1 unit, 2 building, 3 resource (also 0..: scenery etc.) |
| 18 | i16 src | Copies `T[src]` (all 0xB0 bytes, **including +0 id**) to T, then clears +0x38, +0x42, +0x78, +0x7C, +0x80 | Inherit from a template. **`T+0` therefore holds the base (template) type**, used by the AI build sites (0x4411B0) and the boat rule (0x13/0x16). |
| 19 | i16 a, i16 b | `T+0x88`=a, `T+0x8C`=b | Two related types (*guess*: upgrade-from / next Mk; 0x442D80 compares `+0x84`/`+0x88`) |
| 20 | i16 a | `T+0x88`=a, `T+0x8C`=0 | |
| 21 | i16 | `T+0x14` | Name text id (0x440720) |
| 22 | i16 | `T+0x18` | Info page id (0x4403A0 "Information") |
| 23 | i16 | `T+0x2C` | War map counter frame (done) |
| 24 | i16 | `T+0x28` | unclear |
| 25 | f32 | `T+0xA8`; current scale | Scale applied by records 28/30/31 |
| 26 | i16 | `T+0x0C` | **Faction**: 0 Human, 1 Martian, 2 neutral (default) |
| 27 | 4×i16 | pending | Box (+0x12..+0x18) for the next Part |
| 28 | i16 a, u16 b, u16 c, u16 d, f32 f | New "Part____" (0x1C bytes) prepended to `T+0x38`, `++T+0x42`: {next, a, f*scale, b, c, d, box or (−1,0,0,0)} | Model parts/hotspots |
| 29 | i16 a, u16 b, u16 c, 4×f32 | New "Emitter" (0x1C) prepended to `T+0x3C`: {next, a, b, c, f0..f3} | Effect emitters (smoke, muzzle) |
| 30 | u16 hp, u16 b, u16 c | `T+0x44`=`T+0x46`=hp*scale; `T+0x40`=b*scale; `T+0x41`=c*scale | Hit points (max, current) + 2 byte sizes (*guess*) |
| 31 | u16 | `T+0x46` = v*scale | |
| 32 | u16 | `T+0x48` = v * **VisRadMul** (1.5) | Vision radius |
| 33 | u16 | `T+0x50` | |
| 34 | i16 | `T+0x4A` = v × armour modifier (below) | Armour |
| 35 | f32 | `T+0x4A` *= f | |
| 36 | u16 | `T+0x4E` | Move class (0–2 land, 3 sea, 4 air) |
| 37 | 3×u16 | `T+0x52`, `T+0x54` (war map speed), `T+0x56` | |
| 38 | 2×f32 | `T+0x54` *= f0, `T+0x56` *= f1 | |
| 39 | i16 days | `T+0x08` = days*24 × research modifier | Research/build time in hours |
| 40 | i16 | `T+0x1C` | unclear (costs? *guess*) |
| 41 | i16 | `T+0x20` | unclear |
| 42 | i16 | `T+0x24` | unclear |
| 43 | f32 | `T+0x5C` = v × manufacture modifier | Manufacture time/cost |
| 44 | f32 | `T+0x5C` *= f | |
| 45 | 3×(4 bytes) | `T+0x60/64/68` raw | Resource costs, 3 kinds (*guess*) |
| 46 | f32 | scales +0x60..+0x68 | |
| 47 | 3×(4 bytes) | `T+0x6C/70/74` raw | Second triple (*guess*: resource consumption) |
| 48 | f32 | scales +0x6C..+0x74 | |
| 49 | i16 | `T+0x78` | Linked type 1 (0x442460) |
| 50 | 2×i16 | `T+0x78`, `T+0x7C` | |
| 51 | 3×i16 | `T+0x78`, `T+0x7C`, `T+0x80` | |
| 52 | u16 | `T+0x90` | Max per sector (0x43C7F0; default 20) |
| 53 | u16 | `T+0x92` | default 90 |
| 54 | i16 | `T+0x94` | Prerequisite building type (0x4423D0) |
| 55 | u16 | `T+0x58` | default 1 |
| 56 | u8 | `T+0x43` | default 1 |
| 57 | u16 | `T+0x4C` | |
| 58 | i16 | `T+0xA2` (word) | |
| 59 | i16 | `T+0xA4` (word) | |
| 60 | i16 | `T+0x98` | default 0x250 |
| 61 | u16 | `T+0xA0` = (byte)(v−1) | Selection voice/sound class (0x439000 → 0x464EE0) |
| 62 | u8 | `T+0xA1` | default 0xC0 |
| 63, 64 | i16 idx, 3×i16 | 0x5888C0 + idx*12 | Read by the research screen 0x45CB70 (*guess*: research tree links) |

**Armour / research / manufacture modifier** (records 34, 39, 43): taken from `[Tweak]` by the type's faction (+0xC) and the side being played (0x4B84C4):

| Faction | Player Human | Player Martian |
|---|---|---|
| 0 | HumanFriend | HumanEnemy |
| 1 | MartianEnemy | MartianFriend |
| 2 | 1.0 (armour uses `Scenery Armour Modifier`) | same |

Not set by the loader, but read elsewhere:
- `T+0x04` (research item, 0x4423D0/0x442460)
- `T+0x84` (0x442D80)
- `T+0xAC` byte (flag cleared by 0x4424B0)

### 3.2 AI.ojd record types (0x441580)

| Op | Payload | Effect |
|---|---|---|
| 0 | u16 type | Current unit type; entry count++ |
| 1 | i16 | **AI class** of the type → 0x518B14[type*8]. Values seen in the code: **1 = combat unit** (counted in attack strength), **7 = defensive building** (emplacements, turrets, fences, mines: counted in defence strength, and they share a war map counter) |
| 2 | i16 | **AI strength** × multiplier → word 0x518B10[type*8]; the scaled value is summed into AiData+4 and divided by the entry count at the end (average strength) |
| 3 | u16 side | Side the following sites belong to (0 Human, 1 Martian) |
| 4 | i16 sector | Sector for the following sites |
| 5 | i16 type, x, y | Build site {type, x, y} |
| 6 | i16 type, x, y, x2, y2 | Build site with a second point |
| other | | Logs "Invalid data in T…" (nullsub) |

The multiplier is `[Tweak] AI strength table Human multiplier` (1.0, object 0x5187F8) when the player is Human, else `… Martian multiplier` (2.0, 0x51A0D0). The table at 0x518B10 has 8 bytes per type and 696 entries; it is zeroed before loading.

Sites whose side equals the side being played go in the 0x518990 lists, the others in 0x518810. (x,y) is the battle map position matched against a building's battle object (+0x78/+0x7C). The building gets 0x80D1(x,y) and 0x80D2(x2,y2); x2,y2 is *unclear* (a second anchor such as an entrance or facing point).

`WoWEngine/src/core/AiTable.h` reads these records correctly, but its comments should be updated: "value" is the AI class, "weight" is the strength, and "lists" are the per-sector build sites.

### 3.3 Sector data block `S[n]` = 0x4EAE18 + n*0xC0

| Off | Addr (n=0) | Meaning |
|---|---|---|
| +0x00 | 0x4EAE18 (byte) | Has command building (cached on leave/load) |
| +0x04 | 0x4EAE1C | **Owner**: 0 Human, 1 Martian, 2 none |
| +0x08..+0x1C | 0x4EAE20 | 6 ints from OBJ.ojd record 8 |
| +0x20 | 0x4EAE38 [6] | Counters ++/-- by 0x43CC40/0x43CC70 (*guess*: number of consuming facilities per resource) |
| +0x38 | 0x4EAE50 [6] | **Demand** (added by 0x43CCA0; [0..2] zeroed on owner change) |
| +0x50 | 0x4EAE68 [6] | Effective demand incl. neighbours (0x43C210, Crippler VY) |
| +0x68 | 0x4EAE80 [6] | **Supply/production**, from the owner's resource objects (msg 0x8044, 0x421D00 kind i) |
| +0x80 | 0x4EAE98 [6] | Effective supply incl. neighbours + Resource Buffer (0x43C160, Crippler VX) |
| +0x98 | 0x4EAEB0 [6] | `min(+0x50/+0x80, 1.0)` (msg 0x8046), a supply/demand ratio (*guess*: production efficiency) |
| +0xB0 | 0x4EAEC8 | Name from record 8 (landscape file base) |

Index i: 0..2 belong to the Human side's three resources, 3..5 to the Martian side's (0x441D30/0x441DB0).

### 3.4 Sector save record (0x43C5E0 / 0x43C4C0)

It is written after the base object (0x4465B0, the six lists). Each line below follows the one before it in the record:

1. dword owner
2. damage map (+0x8C vtbl+0xC)
3. 6 × {f32 `S.supplyEff[i]`, f32 `S.demandEff[i]`, f32 `S.ratio[i]`, f32 `S.demand[i]`, f32 0}

Load reads the owner into `S+4`, sets `S+0 = HasCommandBuilding`, and reads the 30 floats into the staging arrays 0x4F7770/88/A0/B8/D0 (30 floats a sector). They are not written straight back into S (*unclear* where they are used; *guess*: 0x41A760).

### 3.5 Other globals

| Address | Meaning |
|---|---|
| 0x4E9B70 | WWarMapWorld (+0x58 SGroupList, +0x74 counters, +0x80, +0x90 routes, +0x94[31] sectors, +0x114 box count, +0x118/+0x11C/+0x120 object/render lists) |
| 0x4E9B74 / 0x4E9B78 | Local player / opponent (+0x24 side) |
| 0x4E9B7C | Battle info (0x421210, 0x70 bytes): +0x10/+0x14/+0x38/+0x3C, +0x18/+0x1C side totals, +0x20/+0x24, +0x28/+0x2C buildings+resources per side, +0x30/+0x34, +0x40/+0x44 unit counts, +0x48/+0x4C remaining, +0x50 sector, +0x54 battle on, +0x55 game won, +0x68 owner, +0x6C defender |
| 0x4E9B68/6C | Battle map start view |
| 0x4E9B60 | Set by 0x80F4 (object faction) |
| 0x4E9B8C | Research manager |
| 0x4E9B94 | (byte) force-end flag |
| 0x4D2550 | Current command target world |
| 0x4D2558 | Tick counter |
| 0x4EA614 | Fog update period |
| 0x4EA43C | Battle cursor mode |
| 0x4EA448 | UI parent for popups |
| 0x4EA1B4/0x4EA1CC | Max units/boats in sector |
| 0x4F7428 | Type to place / build |
| 0x4F7430 | Enemy object count on entry |
| 0x4F7450 | Battle-end dialog |
| 0x4F7458[…] | Reinforcement points (24 B), count 0x4F76E8 |
| 0x4F7650[32] | Per-sector unit strength (also the war map Unit View shading, dword_4F7654) |
| 0x4F76F0[32] / 0x4F86B8[32] | Incoming boats / land units |
| 0x4F8738 | Battle map helper (0x483DF0) |
| 0x4F873C | Current sector |
| 0x4F8740 | ',' / '.' cycle cursor |
| 0x4F8748[0x8000] | Selectable objects by id |
| 0x59D940 | Current WLandscape |
| 0x59D9B8, 0x59D9C0[256] | Reset on battle map entry |
| 0x59DDC0/0x59DDC8 | Battle object array |
| 0x546230[] | AI ranked units |
| 0x56625C | AI upgrading |
| 0x4BAE68/0x4BAE70 | Waypoint tables (getters 0x437380/0x4373A0) |
| 0x4BD2E0 | Landlocked-sector bytes |

---

## 4. Messages, commands, strings, tuning

### 4.1 Sector `HandleMessage` 0x43ADA0

| Message | Handling |
|---|---|
| 0x20 (cursor query) | Cursor id by owner: arg 0 → 0 (own) / 0xC (other); arg 2 → 4 (own/neutral) / 5 (enemy), through 0x42E780 |
| 0x100 WM_KEYDOWN (battle map open only, table 0x43BAB0) | VK 9 Tab → 0x43A400; VK 0x24 Home → 0x43A480; VK 0xBC ',' → 0x43BE50; VK 0xBE '.' → 0x43BF30 |
| 0x202 WM_LBUTTONUP | 0x43C3F0 |
| 0x111 WM_COMMAND | Sub-ids below |
| 0x8006 | Returns 1 |
| 0x800F | Refreshes the battle map and menu; cursor 0x42E790(selection ? 1 : 0) |
| 0x802D (hours) | Forwarded (0x446310); +0x6C += h, every 24 h `--+0x68` |
| 0x8044 | Recompute supply `S+0x68` from the owner's resource list (sends it 0x803C first) |
| 0x8045 / 0x8046 / 0x8047 | Effective demand / ratio / effective supply |
| 0x8057 | Remove object (vtbl+0x2C) |
| 0x805C | On the war map: context button 0x8133 / command 0x8019 "View map" (text 0x475, icon 0x2E) through 0x432E90 |
| 0x80AA | Forward to the construction + resource lists of a side |
| 0x80F4 | +0x128 = lparam, 0x4E9B60 = faction of object(wparam) |
| 0x8112 | Unlink and relink all AI build sites in the sector |
| 0x8136 | Forward to a unit list by side |

WM_COMMAND (0x111) sub-ids:

| Id | Handling |
|---|---|
| 0x73 | Queue command 2 (deselect) |
| 0x76 | Forward 0x76 to the world (return to the war map) |
| 0x7A | Scroll to (lparam lo/hi) → 0x8004 to the battle map and menu |
| 0xB7 | Build the 30 distance objects |
| 0x8010/0x8011 | Box highlight/select |
| 0x8024 | "Decided to stay" (log) |
| 0x8027 | Set the type to place |
| 0x8028 | 0x43D4F0 create object |
| 0x8029..0x8105 | Debug menus (2.7) |
| 0x806C/0x806D | (debug) apply 0x421F50 to all lists by side |
| 0x8092 | Save `dat\%d.nsb` |
| 0x8094 | Pick object |
| 0x8043 | 0x43CEB0 |
| other | Forwarded to the player's selection group |

### 4.2 Player-command / world messages sent from this piece

0x7A, 0x76, 0x8004, 0x800F, 0x8012, 0x8024, 0x8026, 0x8028, 0x803A, 0x803C, 0x8043, 0x8044, 0x8045, 0x8047, 0x8050, 0x8051, 0x8054, 0x8055, 0x8058, 0x8075, 0x807F, 0x80A6, 0x80A7, 0x80AC, 0x80D1, 0x80D2, 0x808C, 0x811A, 0x8133, 0x813C, 0x813D, 0x8141.

Sounds: 0x109 (select unit), 0x11F (order acknowledge), both through 0x464EE0 with the type's sound class.

### 4.3 Strings

- Dialog texts (TEXT.ojd): 0x49A "Attack Successful!", 0x49B "Attack Failed", 0x49C "Invaders Repelled!", 0x49D "Sector Lost", 0x475 "View map", 0x478 "Information", sector names 0x002+n.
- Literals: `obj.ojd`, `AI.ojd`, `LoadSetupData`, `Part____`, `Emitter`, `dat\%d.nsb`, `BattleMapSelection`, `SGroupList`, `Human`, `Martian`, "Invalid data in T…", "Decided to stay…", "UNLOADING ALL MESSAGE/SELECTABLE OBJECT MEMORY", "Allocating memory block for 1024 WSelectableObjects".

### 4.4 Object type ids (from the debug menus)

| Group | Ids |
|---|---|
| Human vehicles | ArmouredLorry 1/2/3, ArmouredTrackLayer 4/5/6, TunnellingTrackLayer 7/8/9, SelfPropelledGun 10/11/12, MobileAntiAircraftArray 14/15/16, SappersLorry 17/18, Ironclad 19/20/21, Submersible 22/23/24, ObservationBalloon 25/26/27, ConstructionWorkers 28/29, MobileRepairVehicle 30/31, SpecialForcesMortarBike 32/33/34 |
| Human buildings | VehicleFactory 35/36/37, TrainingCentre 38, ShipYard 39/40/41, Hangar 42/43/44, MunitionsFactory 45, ConstructionLabs 46, SteelRefinery 47, CoalMine 48, OilRefinery 49, MediumGunEmplacement 50/51, HeavyGunPosition 52/53/54, AntiAircraftArray 55/56/57, **CommandPost 58**, RailwayPlatform 59, RepairWorkshops 60, **Parliament 278**, Bridge Military 444 / Country 445 / City 446 / Railway 447, Wire Fence 451, Wire Wall 452, Mined Wire Wall 453 |
| Martian machines | ScoutMachine 62/63/64, FightingMachine 65/66/67, Tempest 68/69/70, BombardingMachine 71/72/73, ElectricMachine 74/75/76, FlyingMachine 77/78/79, ScanningMachine 80/81/82, Constrictor 83/84/85, XenoTelepath 86/87/88, HandlingMachine 89/90, DiggingMechanism 91/92, Drone 93/94/95 |
| Martian buildings | Constructor Lvl 96/97/98, EnergyWeaponPlant 99, SuspensionFieldSite 100/101/102, TelepathicTrainingCentre 103/104/105, BiochemicalPlant 106, ExplosivesPlant 107, HumanFarm 108, CopperForge 109, HeavyElementFacility 110, RapidHeatRay 111/112, HeatRayTurret 113/114/115, ProjectileLauncher 116/117/118, PowerPlant 119, **CommunicationsCentre 120**, RepairFacility 121, MatterTransferStation 122, **Martian Base 830**, Electric/Laser/Plasma Fencing 454/455/456 |
| Mines | Mines 627, WireTriggered 628, Undetectable 629, Enhanced 630 |
| Other | Pod 179 |
| Scenery | Trees C 207–209 / 213–219, D 210–212 / 220–227; village 230–241 (VHall…Abbey); town 242–277, 279 (Castle…Stonecircle); LH variants 721–741 (45F), 845–904 (00F/30F/60F), 973–985 (90F) |
| Effects and markers | Explosion parts 348–366 (even), Smoke 367–376, Death_Explosion 590–597, Sea1 686, Sea2 687, Mountain 688, Field 689, Factory 690, City 691, Docks 692, Wind 693, CP MARKER 696, Forest 994, River 995, Brook 996, water twinkle 970–972 (0x3CA+r), reinforcement marker 0x3DB = 987 |

### 4.5 Tuning settings (`[section] key`: default, value address = object + 0x14)

| Setting | Default | Address |
|---|---|---|
| `[Tweak] VisRadMul` | 1.5 | 0x4F706C |
| `[Tweak] Scenery Armour Modifier` | 1.2 | 0x4F04F4 |
| `[Tweak] HumanFriend / HumanEnemy / MartianFriend / MartianEnemy Armour Modifier` | 1.8 each | 0x4F2EAC / 0x4F04BC / 0x4F2E94 / 0x4F7084 |
| `[Tweak] … Research Modifier` (all four) | 0.75 | HF 0x4F6E34, HE 0x4F050C, MF 0x4F2F0C, ME 0x4F709C |
| `[Tweak] HumanFriend / MartianFriend Manufacture Modifier` | 1.0 | 0x4F2EF4 / 0x4EC594 |
| `[Tweak] HumanEnemy / MartianEnemy Manufacture Modifier` | 0.85 | 0x4F2EC4 / 0x4F2EDC |
| `[Tweak] Water twinkles per frame` | 20 | 0x4F76E4 |
| `[Debug] Load scenery` | 1 | 0x4F73F1 |
| `[System] ZMapRes` | 400 | 0x4F7410 obj |
| `[Tweak] IMap Iteration Count` | 4 | 0x4F8670 obj |
| `[Tweak] Battlemap Zoomer Speed` | 2.5 | 0x4F744C |
| `[Tweak] Resource Buffer` | 30.0 | 0x4F740C |
| `[Tweak] Distance Crippler VX` | 0.5 | 0x4F86B4 |
| `[Tweak] Distance Crippler VY` | 0.5 | 0x4F869C |
| `[Tweak] AI strength table Human multiplier` | 1.0 | 0x5187F8 obj |
| `[Tweak] AI strength table Martian multiplier` | 2.0 | 0x51A0D0 obj |

AI float tweaks (0x4A3600 objects; the default is the float at the cited address):

| Setting | Default |
|---|---|
| AI Frontline Attractor for Defences | 2.0 (0x4BED88) |
| AI Eff Threshold | 0.6 |
| AI Panic Build Threshold | 500.0 |
| AI Move Threshold | 1.05 |
| AI Frontline Attractor | 2.0 |
| AI Build Resfac Eagerness | 50.0 |
| AI Aggression Value | 0.5 |
| AI Invasion Threshold PC | 150.0 |
| AI Outnumber Force | 1.1 |
| AI Retain Force | 0.2 |
| AI Invader NFriend Mul | 3000.0 |
| AI safe sector repeller Mul | 2.0 |
| AI Minimum Rr | 8.0 |
| AI Movement Temperature | 0.25 (0x4BEDBC) |

Other AI and debug settings:
- `[Debug] Enemy Visible`: 0 (0x546038)
- `[Tweak] AI Oracle`: 0, bool (0x566248)
- `[Tweak] AI Hours Per Turn`: 5 (0x545760)

The AI float tweaks are used in part 07.

Fixed constants:
- Max per sector default 20
- Reinforcement point inset 100 px
- Twinkle margin 20 px
- AI sort initial min 100000.0 (0x47C35000), root pivot 0.5
- Message pool block 0x1800; selectable pool 1024 × 0x1A0

---

## 5. Cross-references to other pieces

| Callers into this piece | Callees out of this piece |
|---|---|
| 0x402630 game init → 0x437750 OBJ.ojd | 0x415200/0x415120 FILE_Load, 0x40E7A0 GType register, 0x4148A0 tagged alloc |
| 0x417CD0 WWarMapWorld ctor → 0x439480 ×31, 0x440860 SGroupList | 0x4460E0/0x446270 sector base, 0x446370 list select, 0x4465B0/0x446500 base save/load |
| 0x4185F0 → 0x438B20 player, 0x442890 AI | 0x445EA0 remote player (part 07), 0x445210/0x445620/0x445B90/0x445D60 AI parts |
| 0x418500 war map start → 0x441110 link sites, 0x4424B0 | 0x443340/0x443610/0x443CA0/0x4440B0/0x4443D0/0x444690/0x444C10 AI states (part 07) |
| 0x4198C0 → 0x43D4F0, 0x4391F0, 0x439230 | 0x47AF10 WLandscape, 0x484A30 battle map, 0x4868B0 battle menu, 0x4346C0/0x434DF0 fog |
| 0x41A330/0x41A450/0x41AB00/0x41D9B0/0x423A00/0x423B00/0x424A40/0x424F50/0x425C50/0x429640/0x429A70/0x429E80/0x42A4B0/0x42ABD0 (war map) → sector helpers | 0x421xxx WWarMapList (count 0x4216D0, find 0x421C10, by type 0x421CC0/0x421C60, resource kind 0x421D00), 0x42AA70 finished, 0x4205F0 add counter, 0x420930 remove counter |
| 0x462DC0 build list → 0x43C7F0, 0x442420 | 0x45B870 research done, 0x4E9B8C research manager |
| 0x480EF0 battle map → 0x43D3F0; 0x4868B0 → 0x43A5E0/0x43A5F0; 0x485930/0x487800/0x4878F0/0x4895A0/0x48DA40/0x47CFA0 → 0x43C6D0..0x43C770 | 0x47D190/0x47D590/0x47D600/0x47D610/0x47D660 battle object lists, 0x47B120/0x47B170 height |
| 0x44A160 DeePlay → 0x438B00 | 0x464EE0 play sound, 0x464AB0 fade all |
| 0x4081D0 CD player menu → 0x4393D0, 0x441A40 (odd: *unclear*) | 0x432520 dialog, 0x432D70/0x432E90 context buttons, 0x4A3E60 InfoPage, 0x49BAA0/0x49BBF0 popup menu, 0x480190 BattleMapSelection |
| 0x435050/0x434F00/0x434FA0 → message pool | 0x44E950/0x44EA20/0x44EA60/0x44ED20/0x44E9D0 file stream (nsb) |

---

## 6. Missing in WoWEngine: candidate tasks (in priority order)

What WoWEngine has for this piece:
- `ObjOjd.cpp`: the GType part of OBJ.ojd, unit counters, a few unit fields (+0x4E, +0x52..+0x56) and arrows
- `AiTable.cpp`: an AI.ojd parser
- `WarMap.cpp`: owners, HasCommandPost, route refusals 3–8 with "Max units/boats in sector"

Nothing else in this piece is implemented.

**P1: data foundations (needed by everything else)**
1. **Full unit type table.** Port 0x437750 completely: 1009 entries, the defaults (0x437767), all records 15–62 with record-18 inheritance (copy then clear +0x38/+0x42/+0x78/+0x7C/+0x80, keeping the template id at +0), the scale from record 25, the Part (28) and Emitter (29) lists with the record-27 box, and the faction/side armour/research/manufacture modifiers with their `[Tweak]` defaults (0x4373D0–0x437710). Also store the side tables: records 6, 8, 9, 10, 11, 12, 13, 63/64.
2. **Sector table from record 8.** The landscape names (`S+0xB0`) and the 6 ints are needed for the battle map (0x47B900) and the war map resource views (0x423F80, part 04).
3. **Time-of-day lighting from record 9** (0x4DA828, 64 bytes per hour; consumer 0x40B030).
4. **AI.ojd semantics.** Fix the AiTable comments and model: class (1 combat / 7 defence), strength × side multiplier (`[Tweak]` 1.0/2.0), average strength, per-sector build-site lists split into player side / other side (0x441580). WARMAP.md already uses class 7 for the shared counters.

**P2: war map sector logic (part of "war map completeness")**

5. **Sector ownership recompute** (0x43CA40) after any add/remove/arrival, instead of WarMap.cpp's provisional "arriving takes a neutral sector". On a change it zeroes demand and notifies the world.
6. **Adding new objects** (0x43D4F0): a WMOB, the owner update, buildings placed at the next free AI.ojd build site (0x4411B0, matching the base type `T+0`), and site linking (0x440EA0/0x441110/0x441420).
7. **Resource network**: supply from resource objects (0x8044), demand (0x43CCA0), spreading to other sectors by distance with `Distance Crippler VX/VY` and `Resource Buffer` (0x43C160/0x43C210), the ratio (0x8046), the 30 distance objects (0xB7), and the save layout (0x43C5E0/0x43C4C0). This feeds the Resource View shading (WARMAP.md "own sectors' shading") and the production speed.
8. **Build permission checks**: `CanBuildHere` (0x43C7F0: faction = owner, count < `T+0x90`), prerequisites (0x442420/0x4423D0: research `T+0x04`, prerequisite building `T+0x94`).
9. **Calendar speed lock** (0x43D010 per sector, used by 0x41AEA0), plus the land/sea counts for route checks (0x43D090/0x43D0F0) and the room checks (0x43AC40, including the incoming-unit counters 0x4F86B8/0x4F76F0, still missing).
10. **UndefendedSectorLost test** (0x43C8D0) and the related events.
11. **Sector save record**: owner, damage map and 30 floats (0x43C5E0). The engine's SaveGame reads the owner; it should also round-trip the rest.

**P3: AI player (single-player needs it)**

12. **AIPlayer skeleton** (0x442890/0x442AC0/0x442EC0): the 10-state machine driven by 0x802D game hours, the research choice (0x4E9B8C), the reactions to 0x8075/0x807F/0xB9 (upgrade-in-place 0x442D80), and state 0 ranking (0x443030). The other states are in part 07. Include all the AI `[Tweak]` floats with their defaults (4.5) and `AI Oracle`, `AI Hours Per Turn`, `Enemy Visible`.
13. **AI helper queries** (0x4419E0–0x442460): strengths by class, neighbour lists (0x441AD0/0x441B80/0x441C50), boat rule (base types 19/22, landlocked table 0x4BD2E0, which must be extracted from the exe like WarMapData.h).

**P4: player command model**

14. **WPlayer command queue** (0x438B20/0x438C50/0x438BF0/0x438C90): every order goes through a typed record queue and is flushed and executed. This is needed for faithful selection/orders, and later for network and playback (0x42D290, DeePlay). At minimum, mirror the command types 2/3/4/5/8/24 and the selection-group semantics (0x438F00/0x438F50/0x439000/0x439230), including the selection sound 0x109 with `T+0xA0`.
15. **WSelectableObject basics**: id table (0x4F8748, 0x8000), name = Text(`T+0x14`), "Information" context button and InfoPage (`T+0x18`, 0x4403A0).

**P5: battle map entry/exit glue (when the battle map work starts)**

16. **EnterBattleMap** (0x439770) and **LeaveBattleMap** (0x439ED0) in that exact order. Include the start-view centring on the HQ (types 58/278/120/830) with the iso projection constants, the battle info block (0x4E9B7C), the 0x8075/0x807F notifications and the sound fade.
17. **Battle end dialogs** (0x43A980/0x43A800): Sector Lost / Attack Failed / Invaders Repelled! / Attack Successful!, with 0x76 versus 0x8024 ("stay"), and the game-won flag for sector 3/30.
18. **Reinforcement entry points** (0x43D150/0x43D3F0, marker object 987).
19. **Battle map keys**: Tab (0x43A400), Home (0x43A480, zoomer speed 2.5), ',' / '.' cycling (0x43BE50/0x43BF30); box select with Shift (0x43C340); left-click orders and building placement (0x43C3F0, cursor modes 2/3, ack sound 0x11F); cursor by owner (msg 0x20).
20. **Water twinkles** (0x43A670: 20 a frame, objects 970–972, depth chance).
21. **Fog update cadence** (0x4D2558 % 0x4EA614).

**P6: optional / debug**

22. Developer scenery editor: placement menus (2.7), `dat\%d.nsb` load (`[Debug] Load scenery`, default **on**, so the `.nsb` scenery files *are* part of normal battle maps and must be loaded) and save (0x8092).
23. Pools (0x438970, 0x440740) are not needed. Use normal allocation, but keep the 1024-object and 0x8000-id limits in mind for ENGINE_LIMITS.
24. A `wowdump --units` extension to print every unit-table field and the 4.4 names, to check the port against the exe.
