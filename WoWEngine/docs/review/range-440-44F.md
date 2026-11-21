# Review: 0x440000-0x44FFFF (297 routines)

**Counts: PORTED 78 · PARTIAL 20 · MISSING 47 · LIBRARY 150 · unclear 2**

Paths below are relative to `/home/user/WoWRevived/WoWEngine/src/`. What the range holds: WSelectableObject and SGroupList (0x4400C0-0x440D70), the AI.ojd build sites and loader (0x440DD0-0x4419BE), the war-map AI helpers, settings, player and turn phases (0x4419E0-0x444FDA), the battle AI (0x445010-0x445E94), the network player, order packets, sector lists, Zoomer, object factories, WGroup and the order panel, power pips, the bucket grid, DeePlay, DeeDraw, movies, the busy icon, the save stream, AStar, the Smacker player and the ASM cursor blitter (0x44F890-).

What I checked against the disassembly: the war-map AI is a careful port. Constant by constant, these routines match: the phase dispatcher 0x442EC0 (phase changes, research handling); 0x443030 (min starts at 100000, max at 0, pivot ×0.5, strict comparisons, in-order walk); 0x443340 (rand & 0x7FFF, ×3.0517578e-5, ×0.8 in sector 3 or 30, the repeller, the score formula); 0x444690 (the 0.0099999998 factor, the resource order c,a,b, the clamp at 0, cooldown = rand & 7 + 10); 0x444990 (need + 1, a shuffle of 4n swaps, HQ neighbours only); 0x4440B0 (multipliers 2.0 and 1.5, the 1.4 divisor in sectors 3 and 30); 0x443CA0 (efficiency > 0.4, the index-vs-500 "panic" quirk, which the port already notes); and the 14 tweak defaults. The divergences in my range are in the battle AI and in the missing support pieces.

## Missing

### War map / AI
- **0x442D80** `AIPlayer::ApplyUpgrade(newType)`, reached through the handler 0x442AC0 on 0x111/0xB9 (the AI's research finished). For a building type (`T+0x10 == 2`), every AI construction in an AI sector with a command building (0x441A80) whose `T+0x84` or `T+0x88` equals newType is retyped in place. The port never delivers 0xB9 to the AI: `warmap/WarMap.cpp:696` ("the AI, not in yet"), and the AI's Research listener is empty. So the computer's buildings never upgrade. *Task:* route the AI side's research completion to a WarMap::AiApplyUpgrade that does this retype.
- **0x442850** `[Debug] "Enemy Visible"` (dword_54604C). The AI moves (0x443555, 0x44456D, 0x444B8C) also snap the counter to the sector waypoint (0x437380 → 0x4406A0), and about 10 war-map sites outside the range read it. *Task:* add the setting and its uses (low priority, debug).
- **0x446690 / 0x446850** Zoomer: a smooth scroll with a triangular speed profile (`speed += accel` until half the distance, then `-=`), sending 0x7A x|y<<16 each tick and deleting itself at speed ≤ 0. It is used for the war map's go-to (Warmap Zoomer Speed 1.5) and the battle map's Home and centre (Battlemap Zoomer Speed 2.5). Nothing in the port does this (no "Zoomer" anywhere in src). *Task:* add a Zoomer task for WarMap::CentreOn and the battle map's Home.
- **0x449150 / 0x4492D0 / 0x449330 / 0x449340** The group power-pip strip (`hwpower.spr`, GType 8): up to 8 pips, frame = ftol(0.5 + 9·hp/max), bubble-sorted highest first, centred. Callers are the war-map counter 0x42B493/0x42BADB/0x42BB7B and the battle-map group panel 0x4841DD/0x484483/0x4846BD. WARMAP.md:157 lists "the power bar" as still to come. *Task:* port the strip and draw it on war-map counters and the battle-map group panel.

### Front end / UI (context panels)
- **0x440300 / 0x4403A0** WSelectableObject handler: on 0x805C it adds the context button 0x808C "Information" (text 0x478, icon 0x2A) when `T+0x18` (the info page, record 22) is set; 0x808C/0x111 opens the InfoPage (0x4A3E60). In the port `core/ObjOjd.h:75` loads infoPage, but nothing uses it; `warmap/WarMap.cpp:2030` says "Move and Information aren't in yet". *Task:* add the Information context item and the info page.
- **0x448500 / 0x4484A0** The combined order panel for a non-uniform group: Build, Suspend/Resume, stances 0-3, Retreat (battle map only), Abandon, Force Fire, Submerge/Tunnel, and 9 special weapons. Each button is shown only if all members answer 0x8130 (0x4484A0). TASKS.md:196 lists it as "the combined panel (0x448500)" still to do. *Task:* port it with the GType and text ids of survey/part07 §2.7.

### Players / network
- **0x4460A0 / 0x4460C0, 0x4493E0, 0x449A70, 0x449AB0, 0x449EB0, 0x447D80, 0x447DB0** The order-packet builders: a 4-byte header plus payload, appended to a player's buffer. The port executes orders directly, so there is no recording, playback or lockstep. TASKS "Players" is unchecked. *Task:* the order buffer, before network and playback.
- **0x445EA0 / 0x445F00 / 0x445F20 / 0x445F80 / 0x445FD0** The network (remote) player: receive into the order buffer; send, with a null order (type 1, param 1) when the buffer is empty; slot 12 posts param 0x0D. *Task:* part of the network game.
- **0x44A0E0, 0x44A130, 0x44A160, 0x44A290, 0x44A400, 0x44A430, 0x44A4B0, 0x44A5B0, 0x44A5F0, 0x44A8D0, 0x44AB40** DeePlay game logic. Host is always Human and join is always Martian. `[Network] "Session Name"` defaults to "War of the Worlds - session name". Up to 10 sessions. The host starts the game on DPSYS_CREATEPLAYER (WM_COMMAND 0xAB). Packets are u16 sequence + order buffer. *Task:* a transport replacing DirectPlay that keeps the lockstep and the 0xAB start (TASKS "Network game").

### Display / loading
- **0x44E4B0 / 0x44E5B0 / 0x44E5C0** The busy ("wait") icon: GType 0x0C (`hwaitcur`/`mwaitcur`), drawn at (8,8) on the front buffer and stepped once per file chunk read (0x415C95). It is enabled while saving, starting the war map, and loading the battle map. TASKS.md:238 lists it as open. *Task:* show the animated icon during those loads.

## Partial / divergent

| Addr | What differs | Port | Task |
|---|---|---|---|
| 0x445210 | **The threat lists are wrong**; see Bugs #1. | `battlemap/BattleAi.cpp:41-43` | Use lists 3 and 5, constructions first, cap 16 |
| 0x445500 | **0x80B2 lparam 2 is not sent** to registered units; see Bugs #2. | `battlemap/BattleAi.cpp:48-49` | `SetStance(g,2)` on registration |
| 0x4451A0 / 0x445B90 | **Attackers are not re-registered** when a task dies; see Bugs #3. Also: on an own object gone, the original removes only the *first* task with that owner (and re-registers its +0x1C second); for an enemy object gone it first tests `AiClass(type == 1)` (0x445BC9, an index of 0 or 1, almost certainly a compiler or source bug) and does nothing when that is nonzero. | `battlemap/BattleAi.cpp:158-172` | Re-add own groups to aiOwn with stance 2 when a task is erased |
| 0x445D60 | 0x80B1 (threat seen: add a target if it is new, max 16) is not ported. BATTLEMAP.md:837 says its sender never fires (flt_59E44C = 0); I did not verify that. | none | Port it anyway (cheap) or verify the claim |
| 0x441E70 | The strength uses the unit count from the SELO ids after the object, or the type's battleUnits × count, instead of the live group count (+0x4C → 0x4216D0). Provisional by design. | `warmap/WarMapAi.cpp:25` | Use the real member count once groups are modelled |
| 0x440EA0 / 0x441110 / 0x4413C0 | Site links are computed on demand instead of being stored (marked *provisional* in code). | `warmap/WarMapAi.cpp:448-469` | Fine while objects stay put; revisit when buildings can move |
| 0x441420 | The original relinks **both sides'** constructions and resources (it uses 0x518994 for objects with +0x24 == 0, i.e. the player's), from sector message 0x8112 (0x43B7BA; I found no sender), and sends 0x80D1 **and** 0x80D2 (x2,y2 for fences). The port does only the AI's freshly ordered constructions and sets only x,y. | `warmap/WarMapAi.cpp:578-590`, `warmap/Production.cpp:324` | Find the 0x8112 sender; place player constructions too; set x2/y2 |
| 0x444510 | The original queues `{type,x,y,x2,y2,0xDEADDEAD}` from the site (0x4411B0) on the constructor. The port calls OrderConstruction without a place, then places it afterwards. | `warmap/WarMapAi.cpp:559-574`, `warmap/Production.cpp:306` ("0x4411B0, not in") | Pass the site through the order |
| 0x442AC0 | The handler: 0xB9 → 0x442D80 is missing (see Missing); 0x80CB is a nullsub; 0x807F → 0x440EA0 relink not traced. | `warmap/WarMapAi.cpp:173` (AiTurn only) | |
| 0x441580 | Opcode 3: the original sends side 0 / side 1 to the player's list by IsHuman, and **any other value to the other side's list** (0x4416D0). The port maps every non-1 value to side 0. This only matters if AI.ojd holds other values. | `core/AiTable.cpp:24` | Keep the raw side value |
| 0x446B30 | The battle-object factory: only some classes are in (weapons, effects, explosions). TASKS.md:176 lists vehicles, fliers, boats, buildings, fences, mines and pods as still to do. | `battlemap/Weapons.cpp:32`, `battlemap/Effects.cpp:22`, `battlemap/Combat.cpp:22` | Port the full type → class table (survey/part07 §2.6) |
| 0x4498A0 / 0x449900 | Bucket insert and remove: the port recomputes cells from live positions and puts only units in the grid; the game holds every placed object (provisional, `battlemap/Driving.cpp:463`). | `battlemap/Driving.cpp:469` | Add the scenery and buildings to the grid |
| 0x44DCA0 / 0x44E3E0 | Movie options: the original skips on a mouse click only when `opts[2]` is set, stops all sounds (0x465B20), clears the screen afterwards (`opts[3]`), pauses the game timer, and still sends 0x8056 when the file is missing. MovieScreen handles Escape; the other points were not verified. | `frontend/MovieScreen.h:1-26` | Check the mouse-skip option and the stop-all-sounds |
| 0x44F5E0 | Smacker scaling: in 16 bpp, lines are interlaced when the screen height is at least 2× the movie's. The port only centres at 640×480. | `core/Smacker.h`, `frontend/MovieScreen.h` | Only matters at other resolutions |
| 0x44C700 | The cursor is redrawn between frames on mouse move. The port draws it per 30 Hz frame. | n/a | Optional: draw the cursor at display rate |

## Bugs

1. **0x445210: the battle-AI threat list.** The original builds it from `sub_446370(2, 0)` and `sub_446370(3, 0)` (0x445298 and 0x4452B2). 0x446370 maps (2,0) to `+0x58` (awayConstructions) and (3,0) to `+0x60` (awayResources) (0x4463A5, 0x446391). These are the AI's own constructions and its own resources, which it defends: 0x445010 then moves to a same-faction threat. The port uses `setup.groups[g].list / 2 == 1`, i.e. lists 2 **and** 3: the player's constructions plus the AI's constructions, and never the AI's resources (`battlemap/BattleAi.cpp:41-43`; the same misreading is in BATTLEMAP.md "either side's (the sector's lists 2 and 3)"). The original takes lists 3 then 5, both the AI's, up to 16.
2. **0x445500: missing stance 2.** Every registered (non-class-2) AI unit is sent `0x80B2` with lparam **2** (0x4455B5 `push 2`), i.e. stance Attack. The port only appends it to `aiOwn` (`battlemap/BattleAi.cpp:49`), so AI groups keep the default stance 1 (`battlemap/BattleMap.cpp:90`) unless a task changes it. In particular, groups sent to a threat by 0x80C0 attack with Retaliate instead of Attack.
3. **0x4451A0 via 0x445B90: attackers are not returned to the pool.** When an enemy target dies, each task on it is deleted; its destructor (0x4451A0) calls 0x445500 on the task's own object (+0x18) and second (+0x1C), putting them back in the AI's list with stance 2. When the task count reaches 0 (0x4455D0), the planner runs again with them. The port erases the tasks only (`battlemap/BattleAi.cpp:169-171`), so those groups are lost to the AI for the rest of the battle; with an empty `aiOwn` the planner does nothing (`BattleAi.cpp:98`).
4. **0x445B90, own object gone (minor).** The original removes only the first task owned by the object (it `break`s after 0x445CE7) and re-registers its second. The port removes all of them (`BattleAi.cpp:163`). There are no practical consequences, since the planner never sets a second and an object owns one task.

## Library / not needed (150)

- **C++ / engine plumbing (≈60):** the task list 0x440030/0x440060/0x440080; static inits 0x4400C0, 0x440DD0, 0x440E00-0x440E80, 0x44A0B0, 0x44AC00, 0x44ACC0, 0x44ACF0; WSelectableObject ctor, dtors, getters, setters and the pool 0x440120-0x440290 and 0x440440-0x4407E0 (except 0x440300/0x4403A0); SGroupList 0x440860-0x440D70 (10); 0x441160, 0x441A40, 0x4429E0, 0x442A70; neighbour-set value types 0x445FF0-0x446070; WObject dtor 0x445000; 0x446270, 0x446900, 0x446940; holding groups 0x449410-0x4496C0; WCompositeList 0x449B60-0x449E40; 0x4497F0, 0x449840; HeepHeep pool 0x44ED50-0x44EE30; 0x44F280; the base stream 0x44F830-0x44F880.
- **Unused settings:** 0x442E40 `AI Oracle` and 0x442E80 `AI Hours Per Turn` (5). Both values have no reader (0x566259 and 0x545774 have no xrefs), so the port is right to ignore them.
- **DirectPlay wrappers (9):** 0x449EE0, 0x449F10, 0x449F40, 0x449FA0, 0x44A020 area, 0x44ABB0, 0x44AC50.
- **DeeDraw (≈45), replaced by SDL:** 0x44AD20-0x44D1A0 (surfaces, modes, palette, Lock/Blt/Flip, the save-under cursor, DDERR strings).
- **Reaper FMV:** 0x44DC60 `Use Reaper`, 0x44E060 thread, 0x44E420, 0x44E470. No .fmv files ship.
- **Hand-written ASM cursor blitter A (≈14):** 0x44F890-0x44FE7F. The port draws its own cursor.

## Ported (spot-checked unless noted)

- 0x440110, 0x4406A0, 0x4406E0, 0x440700, 0x440720, 0x440680 (getters): `battlemap/*`, `warmap/WarMap.cpp`, `core/ObjOjd.h`
- 0x440550 (SELO save): `core/SaveGame.h:36`, `core/SaveFile.cpp`
- 0x440F80, 0x4411B0, 0x441290, 0x441310: `warmap/WarMapAi.cpp:455, 515, 526`
- 0x441500 / 0x441540 (strength multipliers), 0x441580 AI.ojd: `frontend/FrontEnd.cpp:575-590`, `core/AiTable.cpp`
- 0x4419E0, 0x441A50, 0x441A80, 0x441AD0, 0x441B80, 0x441C50: `warmap/WarMapAi.cpp:102-124`
- 0x441D30, 0x441DB0, 0x441E10 (resource triples, via economy/ResourceGain): `warmap/WarMapAi.cpp:330-336, 670-680`
- 0x441EB0, 0x441EC0, 0x441EE0, 0x441F00, 0x442080, 0x442330: `warmap/WarMapAi.cpp:40-153, 775`
- 0x4423D0, 0x442420, 0x442460: `warmap/WarMapAi.cpp:486-512`; `warmap/Production.cpp`
- 0x4424D0-0x442810 (14 AI tweaks, defaults verified against 0x4BED88-0x4BEDBC): `warmap/WarMap.h:91-107`, `frontend/FrontEnd.cpp:633-646`
- 0x442890 ctor (state), 0x442A80: `warmap/WarMap.h`, `warmap/WarMap.cpp:1986`
- 0x442EC0: `warmap/WarMapAi.cpp:173`; 0x443030 + 0x449AC0/0x449B10/0x449B30/0x449B50 (tree and stack): `:213`; 0x443340: `:269`
- 0x443610: `:604`; 0x443CA0: `:704`; 0x4440B0: `:762`; 0x4443D0: `:810`; 0x444690: `:306`; 0x444990: `:349`; 0x444C10: `:385`
- 0x445010: `battlemap/BattleAi.cpp:65`; 0x4455D0: `:55` (the cursor rotation is a no-op, as in the original); 0x445620 (clear, done at AiStart); 0x4456B0 + 0x445E30: `:96` (ratios 5.0/3.0, own/3, own>>2, the key types 0x33E/0x78 and 0x116/0x13A, strictly-greater "strongest" all verified)
- 0x446310, 0x446370, 0x4463C0, 0x446460 (sector lists by counters): `warmap/WarMap.cpp:486`, `warmap/WarMapAi.cpp:46`
- 0x446500 / 0x4465B0 SECT load/save: `core/SaveFile.cpp:248-330`
- 0x446950 (war-map object made by design): `warmap/Production.cpp` AddObject
- WGroup 0x447DC0, 0x447FE0, 0x4480F0, 0x448210, 0x4482D0, 0x4490E0, 0x449120 (by design, BattleMap groups): `battlemap/BattleMap.h:170, 426`
- 0x449710, 0x449850, 0x449920 (bucket grid, nearer-side rule verified at 0x449976): `battlemap/Driving.cpp:469`
- 0x44DE20 (Smacker thread), 0x44F2C0, 0x44F4A0: `platform/Movie.h`, `frontend/MovieScreen.*`, `core/Smacker.*`
- 0x44E7D0-0x44ED20 save stream (GAME header 0x44E8A0, CopyRest 0x44EBB0): `core/SaveFile.h`, `frontend/FrontEnd.cpp:493`
- 0x44EE70, 0x44EFA0, 0x44F120 AStar (LIFO ties, no re-sort, the step limit): `core/AStar.h`, `warmap/AStar.h`, `battlemap/Routes.cpp`

## Unclear
- 0x4424B0 clears byte `T+0xAC` for every type at a new game (set at 0x47F662 and 0x488C00, read at 0x492931). I did not trace what the flag means or whether the port resets an equivalent.
- 0x4405C0 / 0x4404A0: virtual helpers of WSelectableObject (draw or name through vtable slots); their purpose was not determined.
