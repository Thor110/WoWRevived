# Review: routines 0x430000-0x43FFFF (256 routines)

**Counts: PORTED 107 · PARTIAL 17 · MISSING 41 (22 of them debug-only) · LIBRARY 91**

Sources: IDA listing (lines 82183-111785), port under `WoWEngine/src`, docs, and `docs/survey/part05.md` / `part06.md`, used to identify routines and checked against the disassembly where cited below. Addresses are vanilla. "Port" paths are relative to `WoWEngine/src`.

What the range holds: the UI widget toolkit (0x4301C0-0x434390), WFogOfWar (0x4344C0-0x434DF0), message queues, WWorld and WObject (0x434E50-0x4356B0), sector link tables and battle entry points (0x4356C0-0x437380), Tweak ini statics and the OBJ.ojd loader (0x4373D0-0x438964), the message pool and WPlayer command queue (0x438970-0x4392E0), the sector object, vtable 0x4AA578 (0x439480-0x43D4F0), debug object-placement menus (0x43D760-0x43FBE0), and maths helpers (0x43FD10-0x43FF30).

## Missing

### Battle map: fog of war (0x4344C0-0x434DF0), 13 routines
- 0x4344C0, 0x434500, 0x434540, 0x434580, 0x4345C0, 0x434600, 0x434640, 0x434680: the `[BattleMap]` ini statics. They read EnableFogOfWar=1, Fog darkness=0.7, Fog return rate=5, Fog update freq=15, Fog smooth index=128, Fog vert dropoff=3, Fog damage delta=0 and Fog vert damage radius=16. None of these keys appear in the port.
- 0x4346C0 ctor: two w×h byte maps (FogMap at +0x18, OldFog at +0x1C) and 19 circular stamps for radii 5..95. When fog is off or `byte_4BAD38` is cleared, both maps are filled with 0xFF.
- 0x4349A0 BuildStamp: 255 inside r-dropoff, falling linearly to 0 at r.
- 0x434A80 Reveal(x,y,range): the range is capped at 0x64 (0x434A9B) and multiplied by flt_4AA49C (0.2). The stamp index is clamped to 1..19 (0x434AB7-0x434AC4), then max-blended into FogMap. Called by vehicles (0x48C570) and puppets (0x4964B0).
- 0x434BD0 Flush: invalidates the screen rects of changed cells and copies FogMap to OldFog. Called by the renderer 0x49F4E0.
- 0x434DF0 Decay: each non-zero cell -= return rate (+0x94), floored at 0. It runs from 0x43A980 when `tick % dword_4EA614 == 0` (0x43A9B1-0x43A9C2).
- In the port, `core/TerrainView.h:57` has a `Fog` input and `TerrainView.cpp:259` applies it, but nothing creates fog data and every caller passes null. TASKS.md lists fog as "still to do".
- **Task:** port WFogOfWar (`battlemap/Fog.cpp`): the ini keys, stamps, Reveal from units each update, Decay every "Fog update freq" ticks, and feeding `TerrainView::Fog`.

### Battle map: keys and camera
- 0x43BE50 NextUnit: the ',' key (VK 0xBC, case 188 at 0x43AEBE). It steps through the player's units using the cursor at 0x4F8740 and 0x421D70, then scrolls to the unit (0x7A). **Task:** add ',' to `BattleMap::OnKey` (battlemap/BattleMap.cpp:1008).
- 0x43BF30 NextConstruction: the '.' key (VK 0xBE, case 190 at 0x43AED7). The same for constructions, moving there with a zoomer. **Task:** add '.' together with ','.
- 0x43A440: `[Tweak] Battlemap Zoomer Speed` = 2.5 (0x40200000), stored at 0x4F744C. The Zoomer object itself (0x446690) is outside this range. The port has no zoomer anywhere (grep "zoom" finds nothing). **Task:** add a smooth-scroll zoomer, used by Home, '.' and the battle start.
- 0x43A670 WaterTwinkles, with its ini static 0x439400 (`[Tweak] Water twinkles per frame` = 20). Each update while byte 0x4B880C is set, it picks random cells 20 px in from the edge. On water of depth 1-3 it spawns object 0x3CA+rand()%3 at scale 0.5 with life 100, if `rand() >= depth*10000-10000`. **Task:** port it into the battle map's update (cosmetic, visible).

### Battle map: leaving (0x439ED0), the game-visible part
- After the owner is recomputed (0x43A0B6), 0x439ED0 writes `S[n]+0 = HasCommandBuilding` (0x43A0CB). If the sector had its command building before but not after, the battle-entered flag +0x124 is set, and the report's +0x6C equals its +0x68, it sends **0x813D** to the holder's unit list (0x43A0FD-0x43A13A). Each unit's handler (0x428F75 -> 0x460140) then cancels its factory's tasks.
- It also fades all sound over 30 ticks (0x43A13D, 0x464AB0(0x1E)) and clears +0x124.
- The port has no 0x439ED0 equivalent: grep finds no 0x813D, and TASKS.md says "0x439ED0 ... aren't" in.
- **Task:** on return to the war map, apply the 0x813D cancel and the 30-tick fade in `WarMap::FightBattle` / `BattleMap`.

### Network / player command queue
- 0x438C90 `WPlayer::Execute`: the lockstep command switch (types 2-26, posting 0x8050-0x8055, 0x8026, 0x8028 and 0x8043 to the current world). The port applies orders directly, so this only matters for multiplayer. TASKS.md already tracks it under "Network game". **Task:** keep it under the network milestone.

### Debug tools (scenery editor and object placement), 22 routines
- 0x43D760, 0x43D8D0, 0x43DA40, 0x43DB70, 0x43DE80, 0x43DFF0, 0x43E170, 0x43E2D0, 0x43E590, 0x43E770, 0x43EA10, 0x43EC60, 0x43EEF0, 0x43F1E0, 0x43F500, 0x43F650, 0x43F7C0, 0x43F940, 0x43FA90 and 0x43FBE0 build the debug "place object" popup menus. Each one maps type names to ids for vehicles, buildings, towns, trees, terrain markers, explosions and smoke.
- 0x43D4F0 AddNewObject (message 0x8028, reached only through command 0x18 from cursor mode 3). It places a new object, using the AI.ojd build site for buildings.
- 0x43BE10 is the pick-object-id command (0x8094).
- 0x43ADA0 also handles 0x8092, which saves the scenery to `dat\%d.nsb` (0x43B0AD-0x43B103), so this group is a full built-in scenery editor.
- **Task (low priority):** none needed for play. Optionally extract the name→id list (survey part06 §4.4) into docs/UNITS.md as a type-id reference.

## Partial / divergent

| Address | What differs or is provisional | Port | Suggested task |
|---|---|---|---|
| 0x432080 (WScrollbar Draw) | The plate frame is `pos*(frames/2-1)/max` with **unsigned truncation** (`div` at 0x4320B6). The port rounds: `(value*10 + max/2)/max`. This shows on the resolution slider, whose max is modes-1 (0x409873). See Bugs. | frontend/Screen.cpp:50 | Use `value*10/max`. |
| 0x432C60/0x432D70/0x432E90/0x4331C0/0x433220/0x4332C0/0x433330/0x432FB0 (WPopupMenu) | The original lays items out in a 10×8 grid, one column per command, only the hovered row opened (0x433330), sized by 0x4332C0. When it is clamped at the right edge (0x42F130 returns bit 8, 0x432FF5) it **flips** (+0x1BC) and lays out again. It is then placed with the first item's x offset taken into account (`sub di,[ecx+2Ch]`, 0x433025). The port is a single column with no flip or row expansion, and Cancel Move / Information (0x805C) are not added. TASKS.md marks this `[~]`. | warmap/WarMap.cpp:2034-2081; BattleMap popup | Port the grid, row expansion and flip. |
| 0x439770 EnterBattleMap | In: landscape, shade tables, scenery, deployment, start view, reinforcement points. Missing: fog ctor and first decay (0x434DF0 at +0x625), zoomer, the Pod for +0x64, clearing the incoming counters 0x4F76F0/0x4F86B8, the battle info fields at 0x4E9B7C, and 0x8075 to both players when only one side is present. TASKS.md marks this `[~]`. | battlemap/BattleMap.cpp (various) | Finish with the fog and the zoomer. |
| 0x439ED0 LeaveBattleMap | Only the losses and retreat transfer exist (WarMap::FightBattle). The 0x813D cancel and the fade are missing (see Missing); 0x807F to both players is not modelled. | warmap/WarMap.cpp:1390 | As above. |
| 0x43A980 Tick / battle end | The original's "player has nothing left" test is `count(list +0x54) + side's battle objects == 0` (0x43AA59-0x43AA63), and for the enemy +0x58 (0x43AAF6-0x43AB00). +0x54 and +0x58 are the sector's **construction** lists, so a side keeps the sector while it still has constructions. The port counts units only, and its comment calls +0x54/+0x58 "war map objects waiting off the map". The forced-quit path (`byte_4E9B94` posts 0x76 with delay 0x96, 0x43AA18) is missing, and so are the fog decay and twinkles called from here. See Bugs. | battlemap/BattleMap.cpp:1613-1650 | Count the constructions, fix the comment, and add the 0x4E9B94 path. |
| 0x43A800 (game won, +0x55) | The port marks this *Provisional*. The disassembly settles it: after RecomputeOwner (0x43A806), +0x55 = (n==3 && `S[3].owner` (0x4EB05C) == 0) or (n==30 && `S[30].owner` (0x4EC49C) == 1) (0x43A917-0x43A94B). The port's `(3 && human) || (30 && !human)` gives the same result in the attack-won branch. survey/part06's reading of 0x4EB05C as "no Martian base" is wrong. | battlemap/BattleMap.cpp:1638 | Drop the provisional note, or test the owner directly. |
| 0x43A480 Home | The original computes the target and creates a **Zoomer** (0x446690) at speed 2.5 (0x43A592-0x43A5A4), centred with half the screen rect (0x4029C0), clamped (0x42F4E0). It also needs a selection group (0x439230). The port jumps at once with fixed 320/240. | battlemap/BattleMap.cpp:355-370 | Use the zoomer. |
| 0x43A400 Tab | Original: Tab reads the battle menu's visible bit (+0x40 bit 28), sets cursor mode 0 or 5 (0x42F310), and calls battle map vt+0x38. The port toggles the minimap. Whether vt+0x38 is the minimap toggle (0x80C8) is unclear; I did not trace it. | battlemap/BattleMap.cpp:1029 | Check vt+0x38 and the cursor mode. |
| 0x43AC40 HasRoom | The incoming counters `dword_4F86B8[n]` (land, 0x43ACB3) and `dword_4F76F0[n]` (boats, 0x43AD2A) are added. The port takes them as 0. Per survey part06 they are cleared on entering and leaving the battle map, so they are battle-time incoming counts, but who increments them is not traced. | warmap/Production.cpp:78 | Trace the writers of 0x4F86B8/0x4F76F0. |
| 0x43CA40 RecomputeOwner | Units answering 0x8116 are subtracted (0x421740, i.e. counters whose +0x60 is non-zero, handler 0x4294EC). The port says "none yet" and skips travelling counters instead. Matches otherwise (the demand zeroing on change, 0x43CAE5, is in the economy). | warmap/WarMap.cpp:1583 | Identify counter +0x60 (0x428C01 / 0x429453 write it). |
| 0x43ADA0 HandleMessage (sector object) | War-map parts are ported: cursor 0x20, 0x8010/0x8011, 0x8043, 0x8045/0x8047, 0x805C "View map". Battle-map keys ','/'.' and the zoomer are missing. 0x8112 (relink AI sites, 0x43B7C3-0x43B8F9), 0x80F4 (Infiltrate) and 0x8094/0x8092 debug are the rest. 0x80F4 is ported in Combat.cpp. | warmap/WarMap.cpp, battlemap/* | Covered by the tasks above. |
| 0x4321B0 WScrollbar WndProc | The drag formula matches in effect: the port clamps the result instead of the input (Screen.cpp:197-205). The port hard-codes margins of 21 (0x15). That is right for options rows; the calendar's own scrollbar (margins 18/31, 8/11) is in WarMap.cpp. | frontend/Screen.cpp:197 | None (noted as equivalent). |

## Bugs

1. **Slider frame rounding.** Disassembly 0x4320A1-0x4320B6: `frames/2 - 1` multiplied by pos, then `div [esi+90h]` (truncating). Port frontend/Screen.cpp:50: `(b.value * 10 + b.max / 2) / b.max` (rounds). With the resolution slider (max = modes-1, 0x409873), e.g. 4 modes at pos 2: the original shows frame 6, the port shows 7. Fix: `b.value * 10 / b.max`.
2. **The battle's end ignores constructions.** Disassembly 0x43AA59: `call 0x4216D0` on `[ebp+54h]` (the player's construction list), added to the side's battle-object count (0x47D610/0x47D660). The enemy uses `[ebp+58h]` (0x43AAF6). Port battlemap/BattleMap.cpp:1626-1628 counts only `units`, so a side with buildings but no units is declared beaten ("Sector Lost" / "Invaders Repelled!" etc.) too early. The comment at :1615 misnames +0x54/+0x58.
3. **Home jumps instead of zooming.** 0x43A578-0x43A5BC creates a Zoomer (speed `[Tweak] Battlemap Zoomer Speed` 2.5) and hands it to the world (0x41B040). The port (BattleMap.cpp:369) calls ScrollTo at once and uses 320/240 instead of half the screen rect (0x4029C0). Minor at 640×480 apart from the motion.
4. **Reinforcement points ignore the debug flag.** 0x43D16D-0x43D180: the table is built when the battle is on **or** `byte_4D1279` is set, and non-player sectors are recorded when `byte_4D1279` is set (0x43D2CE-0x43D2D9). Port battlemap/Reinforcements.cpp:27 and :45 check only `battleOn` / `src.players`. Debug-only, harmless.

(Checked and found consistent: the edit field's character set and the error sound only when the field is full. 0x4315F5 jumps past the beep for rejected characters; the beep is at 0x431684, after 0x431C30 fails. The survey's "rejected or field full -> 0x192" is wrong; the port is right.)

## Library / not needed (91)

- **UI toolkit plumbing (35):** ctors, dtors and wrappers of the menu items and push buttons. Examples: 0x4301C0/0x4303A0/0x430500 menu-item ctors, 0x430360 SetPos, 0x4308A0/0x4308C0 menu-item WndProc forwarding, 0x430A30 glass-button ctor, the dtors (0x430BC0, 0x430D90, 0x430E80, 0x432000, 0x433470, 0x433760, 0x433C80, 0x433F10, 0x434110, 0x434380, 0x434910), WDesktop ctor and handler (0x433700, 0x433AB0), WOverlayWindow (0x4335F0/0x433650/0x433660), WControl/WGraphicControl ctors (0x433B60, 0x433E70), 0x433D80, 0x433E50, 0x4336D0 GetDesktop and 0x4336E0 static init. The port's `Screen`/`Menu` replaces the window tree by design.
- **Message queues and worlds (27):** 0x434E50-0x435620 (critical section, MessageQueue, Post/Pop/Remove, PostDisplay), WWorld 0x435310-0x435450, WObject/WTask 0x4354B0-0x4356B0, and the WTask list 0x439300-0x4393D0 / 0x43FF80 / 0x43FFD0. The port has its own command delay (0x4355A0/0x435600 cited in FrontEnd.cpp and BattleMap.cpp).
- **Message pool and WPlayer plumbing (19):** 0x438970-0x438B00 (24-byte pooled objects; 0x438B00 is a getter used by DeePlay), WPlayer ctor, dtor, flush and append (0x438B20, 0x438BD0, 0x438BE0, 0x438BF0, 0x438C50), selection-group bookkeeping (0x438F00, 0x438F50, 0x439230, 0x4392A0, 0x4392C0, 0x4392E0), and the sector object's ctor, dtor and bounds (0x439480, 0x439630, 0x4396D0).
- **Small wrappers and getters (10):** 0x43A5E0/0x43A5F0 (landscape w/h), 0x43A600 (makes the selection view), 0x43C6D0-0x43C770 (render-list add/remove), 0x43CBB0/0x43CC00 (forward a vtable call to both lists), and 0x439730, `[Tweak] IMap Iteration Count` = 4: its value (0x4F8684) is never read in the exe, so it is dead.

## Ported

- 0x4306D0, 0x4307D0 (menu-item draw and text offset) -> frontend/Screen.cpp DrawButton (`textOffset`)
- 0x430AF0, 0x430C40, 0x430CD0, 0x430DA0, 0x430E10, 0x430E90, 0x434050, 0x434120 (push, check, radio and toggle) -> frontend/Screen.h:5-9, Screen.cpp:33-60,174-195
- 0x430FC0, 0x4312D0, 0x4313F0, 0x431490, 0x431500, 0x431730, 0x431C30, 0x431D50, 0x431DD0 (WEditField) -> frontend/Screen.cpp:62-160, 290-320
- 0x431E30, 0x431EF0, 0x4321B0, 0x4324D0 (scrollbar ctor, WndProc, SetPos) -> frontend/Screen.cpp:197; warmap/WarMap.cpp (calendar speed slider)
- 0x432520, 0x432A80, 0x432B30 (InfoBox) -> warmap/WarMap.cpp, battlemap/BattleMap.cpp:1595-1650 (EndBox)
- 0x4330B0, 0x434300, 0x434340, 0x434390 (popup WndProc and hot buttons: sounds 0x193-0x196) -> warmap/WarMap.cpp:2084-2240, battlemap/BattleMap.cpp:1067,1390
- 0x4333B0, 0x433480 (view buttons) -> warmap/WarMap.cpp:138,1250
- 0x433770, 0x4337B0, 0x4337F0, 0x433830, 0x433870 (tool-tip ini keys and manager; thresholds `>15/>70/>150` checked) -> frontend/FrontEnd.cpp:976-1005
- 0x433D00, 0x433DB0, 0x433F20 (control draw, frame for state, Esc/Tab keys) -> frontend/Screen.cpp:41, warmap/WarMap.cpp:1094
- 0x4355A0, 0x435600 (posting and flush semantics) -> battlemap/BattleMap.cpp, frontend/FrontEnd.cpp
- 0x4356C0 LandLinked (checked) -> warmap/WarMapAi.cpp:93
- 0x435710 link kinds -> warmap/WarMapData.h (RouteLinks)
- 0x436050 entry points (argument order and the previous*100+sector key checked against 0x4360A2 and the call at 0x43D21E) -> core/Deployment.cpp:7
- 0x437380, 0x4373A0, 0x4373C0 (sector geography) -> warmap/WarMapData.h
- 0x4373D0-0x437710 (VisRadMul 1.5; armour 1.2/1.8; research 0.75; manufacture 1.0/0.85, all checked) -> core/ObjOjd.h:115-117, frontend/FrontEnd.cpp:625-653,1108
- 0x437750 OBJ.ojd loader -> core/ObjOjd.cpp
- 0x439000, 0x4391F0 (select, selection voice 0x109) -> battlemap/BattleMap.cpp
- 0x439440 (`[Debug] Load scenery`), 0x4396F0 (`[System] ZMapRes` 400) -> frontend/FrontEnd.cpp:1069,1106
- 0x43A160 CountType, 0x43A250 RemoveAllForSide -> warmap/WarMap.cpp:1199,1637
- 0x43A800 battle report -> battlemap/BattleMap.cpp:1613, warmap/WarMap.cpp:1445 (see Partial for +0x55)
- 0x43AC10 HasRoomFor -> warmap/Production.cpp:75-99
- 0x43C090, 0x43C0A0, 0x43C0E0, 0x43C120, 0x43C160, 0x43C210 (Resource Buffer 30, Distance Crippler VX/VY 0.5; `/(count*V - (-1.0))`, flt_4AA5A8 = -1.0, checked) -> warmap/WarMap.cpp:340-382, frontend/FrontEnd.cpp:620
- 0x43C2B0, 0x43C340, 0x43C3F0 (war-map rubber band, box select, click in modes 2/3) -> warmap/WarMap.cpp:2158,2324,2341
- 0x43C4C0, 0x43C5E0 (sector load/save; float order effSupply, effDemand, ratio, demand, 0 checked) -> warmap/WarMap.cpp:774-786, core/SaveGame.h:80
- 0x43C790, 0x43C7C0 (add/remove object and reshade) -> warmap/WarMap.cpp:1638
- 0x43C7F0 CanBuildHere (vehicle count includes factory queues, 0x421C60, checked) -> warmap/Production.cpp:402-430
- 0x43C8D0 IsUndefended (checked) -> warmap/WarMap.cpp:1621
- 0x43C990 HasCommandBuilding (0x116/0x13A Human, 0x33E/0x78 Martian, both construction lists, checked) -> warmap/Production.cpp, battlemap/Reinforcements.cpp:13
- 0x43CA30 GetOwner -> warmap/WarMap.h:57
- 0x43CB20, 0x43CB60 (layout) -> warmap/WarMap.cpp:2931,2957
- 0x43CC40, 0x43CC70, 0x43CCA0 (counts and AddDemand) -> folded into WarMap::EconomyPhase phase 2 (warmap/WarMap.cpp:384)
- 0x43CD10, 0x43CEB0, 0x43CFA0 (object into or out of a sector: owner recompute, 0x8044/0x8045/0x8047) -> warmap/WarMap.cpp:585-663, Production.cpp:136
- 0x43D010 IsQuiet, 0x43D090, 0x43D0F0 (land and sea counts) -> warmap/WarMap.cpp:2010-2024,2769
- 0x43D150 (kinds 7/5/1, 100-px clamp, type 19/1 room check, marker 0x3DB checked), 0x43D3F0 -> battlemap/Reinforcements.cpp:11-90
- 0x43FD10, 0x43FDA0, 0x43FE90, 0x43FED0, 0x43FF00, 0x43FF30 (maths helpers) -> core/Landscape.cpp, battlemap/Driving.cpp, Weapons.cpp, Routes.cpp
