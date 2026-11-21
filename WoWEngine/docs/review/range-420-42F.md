# Review 0x420000-0x42FFFF (port vs IDA)

**327 routines: PORTED 141 · PARTIAL 34 · MISSING 23 · LIBRARY/framework 129.** Two definite bugs, one probable bug (see "## Bugs").

What's in this range: the second half of the war map, from route lists to the Win95DeskTop window and the WWindow UI base classes. Port files: `src/warmap/WarMap.cpp` (3136 lines), `Production.cpp`, `WarMap.h`, `src/core/SaveFile.*`, `src/core/Deployment.h`, `src/battlemap/BattleMap.cpp`, `src/frontend/Screen.*`, `src/main.cpp`. How I checked: every routine was matched to its port citation and to `docs/survey/part03-05.md`. I read the disassembly for the economy (0x423F80, 0x424000, 0x429640), routes (0x425080, 0x424A40, 0x42ABD0, 0x420060, 0x425A60, 0x427F70, 0x42B250, 0x428020), clunting (0x4205F0), the message list (0x421150), the calendar tables (0x427710), the end codes (0x4212A0), constructions (0x42A4B0, 0x42A980), selection (0x42C160) and deployment (0x421FE0, 0x4222C0).

## Bugs

1. **The Human command building outside London is 314 in the battle map; the disassembly gives 58.** In `core/Deployment.h:31`, `HumanHq()` returns `sector == 30 ? 278 : 314`. The disassembly at 0x422082-0x42208B (in 0x421FE0) is `sub eax,1Eh; neg; sbb eax,eax; and al,24h; add eax,116h`. `and al` only masks the low byte, so the value is 0xFFFFFF24 + 0x116, which wraps to **0x3A = 58** (the Command Post). The same code is at 0x42A9C9 (0x42A980) and 0x439B94 (start view). `WarMap::HasCommandPost` (WarMap.cpp:2703-2716) already explains this wrap correctly, and survey part06 also says 58. Effect: `BattleMap.cpp:119` (start view) and `BattleMap.cpp:485` (deployment centre) never find a Human Command Post outside London. Human defenders then deploy at entry points or the map centre instead of round their HQ. BATTLEMAP.md:19/317/341 repeat the 314. **Fix:** return 58, and correct the docs.
2. **The route save format does not match 0x425A60 / 0x427F70 / 0x42B250 (probable; check it against a save with a unit in transit).** The game writes:
   - per route: u32 source, dest, +0x5F, +0x5E, +0x44, +0x48 (node count), then **+0x34 = the route's entry count, a u32** (0x425570 increments it), then the nodes, then u8 +0x50;
   - then the entry list (0x427F70): **u32 count with no 'WMOB' tag**;
   - then per entry (0x42B250): u32 group type (vfn+0x2C = 0x44EAA0), the WMOB object's own save, u32 segment (+0x24), u32 index (+0x18), five f32 (+0x2C..+0x3C) and u8 (+0x28).

   The port does something else (`core/SaveFile.cpp:212-240` reading, `:309-321` writing; `SaveRoute` in SaveFile.h:63):
   - it reads and writes field +0x34 as an f32 "progress";
   - it expects `Tag("WMOB")` + count right after the u8;
   - it writes the objects with `List()`, which adds the WMOB tag (SaveFile.cpp:77-82), with no per-entry segment, index, floats or flag.

   A save the port writes with a unit in transit will then not load in the original, and a game save with routes will be misread. The vtable at 0x4AA8A8 confirms the slot widths: +0x20/+0x2C/+0x30 are u32 and +0x38 is u8. TASKS "Saves read and written exactly" only covers saves with no routes.
3. **Counters jump to the route start instead of sliding there.** 0x425770 → 0x428020 slides each counter to the curve start plus its formation offset at `[Tweak] Clunting Speed` (7 px a tick, via 0x42A3E0 → 0x4208B0). On the first hour, 0x425080 (0x4250FB-0x425132) slides it again at 1.0. The port teleports it on the first hour (`WarMap.cpp:858-863`). It also uses `cluntingSpeed` for something else: the travel speed of counters with no speed (`WarMap.cpp:173`, `WarMap.h:339`). The game never uses Clunting Speed that way.

## Missing

**War map: repair** (TASKS has it as `[ ]`)
- 0x422AB0 `MostDamaged`: the battle unit with the lowest hp/maxHP in a WMOB list, skipping those flagged `+0xB8 & 0x1000`. Task: add war-map HP per battle unit and port the search.
- 0x4297C0 `PickRepairTarget`: MRV 30/31 pick from the construction and resource lists; workshops 60/121 pick from units once finished; the target gets flag 0x1000. Task: port it together with 0x429B80's repair part.
- 0x4299C0 repair flags clear/test. Task: part of the repair task.

**War map: Resource view hot spots** (RFHS / ROHS; nothing in the port draws them)
- 0x425E80, 0x426140, 0x4263B0, 0x426430, 0x426330/0x426370 (`Res Stack XO` 4 / `YO` 16): the stacked facility counters per sector, unfinished ones at 50%, and the tooltip `"%s\n%0.2f %s %s…"` (produced, used, "Next facility will add").
- 0x426590, 0x426840, 0x4269C0, 0x426AE0: the per-kind bars (GType 0xB9+slot, height = produced/3), the `coalind.spr` used-marker (frame 1 on a deficit), the selected kind solid and the rest at 50%, and a click posting 0x8113 to select the kind.
- Task: build the Resource view overlay (bars, stacks, tooltips, click to pick the kind) from `economy[]`.

**War map: counter decorations**
- 0x42BC90 production icon: the idle frame 0x94, or the item's counter frame blinking in 8 states, waiting `(10 − eff·10)·6` ticks between cycles. Task: draw it in `drawCounter` from the factory state.

**War map: Retreat / cancelling a move**
- 0x424F50 `RemoveGroup`: before the route starts, a 0x80A9 sends the group back to its source and deletes the route if it's empty.
- The +0x44 "stop at the next node" flag (set by 0x428D70 on 0x80A9) is never read by the port's `MoveRoutes`.
- Task: add the Retreat item (0x429A70) and both behaviours.

**Players / command queue** (TASKS `[ ]` Players)
- 0x425BF0: a WM Group packet 8 is "move to sector".
- 0x425D30 `MoveTo`: routes through world 0x419500, Start, then 0x41FCF0.
- 0x425C50 group `Update`.
- The port calls `StartRoute` directly. Task: comes with the player objects and the network/playback queue.

**Battle map keys**
- 0x421D70 / 0x421E50 `PrevById` / `NextById`: the helpers behind ',' (next unit, 0x43BE50) and '.' (next construction, 0x43BF30). No SDLK_COMMA or SDLK_PERIOD handling exists anywhere in the port. Task: add the cycle keys to the battle map (and the war map, if the callers are shared).

**Demo recording / playback**
- 0x42D290 (`[Demo] Playback Files` 4), 0x42D2D0 `PlaybackPump` (`playback.N`, 16-byte records {tick, msg, wParam, lParam}), 0x42D520 `Record` (`Playback.1`). TASKS line 244 has this as `[ ]`. Task: optional; record and replay SDL input in the same format.

## Partial / divergent

| Addr | What differs | Port | Task |
|---|---|---|---|
| 0x421FE0 | Human HQ type (bug 1). Martian pod (type 179 in the scenery list, world+0x11C) as a centre: not in ("no pod object yet"). | Deployment.h:31; BattleMap.cpp:436-514 | Fix 58; add the pod centre once pods are on the battle map |
| 0x4222C0 | Marked provisional in the port: +0x188 bit 13 fixed place (+0xF4/+0xF8 at 0x422412) not read; +0x18C (0x422463) not read; the occupancy grid is approximated by circles; **0x80C3 sent to every unit (0x422787, 0x42282A) with its spot / the map centre is not done** | BattleMap.cpp:516-610 | Trace 0x80C3 (rally/home point) and the fixed-place bit |
| 0x422910 | Battle groups per WMOB are built differently (setup.groups); the HQ test inherits bug 1 | BattleMap.cpp:119 | With bug 1 |
| 0x421210 / 0x4212F0 / 0x421740 / 0x421790 | The battle report's own counters (+0x10..+0x4C) and the debriefing counts (0x8116 answers) are not filled; the port auto-resolves | WarMap.h:429, WarMap.cpp:1368 | TASKS already tracks this (auto-resolve [~]) |
| 0x4217E0 | After a load, routes are rebuilt from the units' orders (0x80BA) in the game; the port **restarts each saved route from its origin** (`UseSave`, WarMap.cpp:730-740) | WarMap.cpp:730 | TASKS "Route progress" [ ] |
| 0x4257C0 / 0x427E80 / 0x42AFA0 / 0x42B150 | Route load: the entries' progress (segment, hours, segment left) is ignored. Load quirk: the game recomputes +0x6C as dist(node0,node1)/(speed×0.01) | as above | Port it once bug 2's format is fixed |
| 0x425A60 / 0x427F70 / 0x42B250 | Save format (bug 2) | SaveFile.cpp:309-321 | Bug 2 |
| 0x425080 | Ported closely (interrupt test against the player's side, as the game does; arrival when segLeft ≤ 0; the curve index; the trim), except: no group +0x44 stop (0x4252A8); the first-hour slide (bug 3); the arrived sector refreshed even when it's the destination (game: only if ≠ dest, 0x4254B2; harmless) | WarMap.cpp:846-909 | Retreat task; the slide |
| 0x425770 / 0x425730 / 0x428020 | Start: counters slide at Clunting Speed (bug 3); **the arrow is drawn at 50% once started and solid while a counter of the route is hovered (0x42BEB0 0x20 / 0x8006)**. The port always draws it opaque (`DrawRouteArrow`, WarMap.cpp:2621-2660, no blend) | WarMap.cpp:2621 | Draw with Translucent; solid on hover |
| 0x424F00 | Route OnMessage: 0x802D is done (MoveRoutes); 0x111/0x80A9 RemoveGroup isn't | WarMap.cpp:846 | Retreat task |
| 0x427C30 / 0x427CD0 | Show/hide the groups per entry list: the port handles visibility per counter; the network/hidden cases are not modelled | — | Low |
| 0x423B00 | Facility rate `int(hp/maxHP)·r·B/(n+r−1)`: the port takes hp/maxHP as 1, so damaged facilities never produce 0 (WarMap.cpp:257-271 says so). Heal-to-max on 0x80AC not modelled | WarMap.cpp:257 | Once war-map HP exists |
| 0x429640 | Hourly demand: **the "working" rates (unit table 0x51A15C-164, OBJ records 47/48, already loaded as `UnitData::activity`) are added when the factory is working (+0x12C) or the object has a repair target (+0x50)** (0x42965C-0x42969D). This applies to constructions too. The port's phase 2 adds only units' upkeep (WarMap.cpp:385-398; its comment still says "neither is in yet", although production now exists) | WarMap.cpp:385 | Add `activity` for busy factories (and repairers) in EconomyPhase(2) |
| 0x429B80 | Hourly: intel ageing is done (WarMap.cpp:501); the repair accumulation (`24 × sector[+0x94+12·side] × …`, +10/+20/+30 HP) is not | WarMap.cpp:501 | Repair task |
| 0x428D70 | WMOB OnMessage: tooltip name / "In transit" / resource lines are done. Not done: the factory lines ("Currently building", "Days to completion" = hours/24, "Efficiency" ×100, "Idle", "Construction suspended", 0x48E-0x493); 0x80B9 / 0x80BA / 0x80BB arrival counts (dword_4F86B8 / 4F76F0, taken as 0 in `HasRoomFor`, Production.cpp:73-77); 0x8107 repair; 0x80A9 | WarMap.cpp:1193-1245 | Add the factory lines now that factories exist |
| 0x42A4B0 | Construction tooltip: name, " X %d", "%2.1f%% complete" match (0x42A5C9). The finished factory lines (0x42A5EB-0x42A6CA) are missing, as in 0x428D70 | WarMap.cpp:1238-1245 | Same |
| 0x42A980 | Completion: a non-command building calls 0x4297C0 (repair pick) and posts 0x8145 (count refresh, player's or network only). The port re-lays out the whole sector (`RefreshSector`, Production.cpp:260) and has no repair pick; full HP (+0xC4) not modelled | Production.cpp:237-261 | Use the count refresh, not a relayout |
| 0x429A70 | Context buttons: Build / Suspend / Resume are done. **Retreat** (0x80A9, GType 0x350, tip 0x477) is missing. The "current object" is marked *provisional* | WarMap.cpp:2042-2072 | Retreat task |
| 0x42A030 / 0x42A090 | Battle units spawned by unit record 55 (+0x58): the port copies a saved object of the type instead (WarMap.cpp:620-645) | — | Spawn record-55 counts |
| 0x42A0B0 | A battle unit destroyed: removed, 0x807A sent to both players, the object dies when none are left. The port gets losses through its auto-resolve and FightBattle; the per-unit path is unclear | — | Check once the battle map reports losses |
| 0x42B2F0 / 0x42B850 | Counter: done: depth 4/7, own unfinished at 50%, enemy intel < 0xA8 at 50%, cselecth, class-7 counts. Missing: the **power bar** (GType 8 `hwpower.spr`, HP ratio), the factory overlay GType 0x76, the unfinished label (font 0x6F), the **repair icon** (0x76 frame 0x95) | WarMap.cpp:1046-1072 | TASKS "Counter decorations" [~] |
| 0x423040 | Mini-map: the game frames the **selected** sector (`[world+0x84]+0x60→+0x48`); the port lights hovered, else selected (WarMap.cpp:1094). The view-rectangle colour is `dword_4DAF9C`; the port uses white (unclear what value that dword holds) | WarMap.cpp:1094-1103 | Check the colour; frame the selected sector only |

## Library / not needed (129)

- **Static initialisers** for `[Stats]` / `[Screen]` (12): 0x424090 "Routes left", 0x425B40 "WM Groups", 0x42C460-0x42C660 Full Screen / Top left / Size / BPP / Support size / AllowResize / FixedAspect. The port has its own video settings; AllowResize / FixedAspect (0x42D660 `OnSizing`) are replaced by SDL's window handling.
- **Destructors, scalar-deleting destructors, std::list helpers, SEH stubs** (≈35): 0x420270, 0x420AD0, 0x4211D0, 0x421290, 0x4213B0, 0x4213D0, 0x422BE0, 0x422C10, 0x422F60, 0x4237C0, 0x4248B0, 0x426900/0x426920, 0x427080, 0x427AF0/0x427B10, 0x428CB0, 0x42A4A0, 0x42B0E0, 0x42B5D0, 0x425B80/0x425BD0 …
- **Tiny helpers and broadcasts** (≈15): 0x4200D0 list begin, 0x421510, 0x4216E0 frame refresh, 0x421F50 (debug 0x806C), 0x425000, 0x425C30, 0x425DD0/0x425DF0 bounds, 0x427E20, 0x4288D0, 0x428940, 0x429FE0/0x429FF0, 0x42ABC0, 0x4238D0-0x423910 (highlighted-sprite subclass).
- **WUnitHotSpot pool** 0x42C340 / 0x42C3E0 (128 × 0xB8 blocks).
- **Win95DeskTop / Win32 / DirectDraw plumbing** (≈20): 0x42C6A0-0x42C9B0 (ctor, SetMode, mode-change posts 0x803B), 0x42CD10, 0x42D280, 0x42DA10/0x42DA50 cursor mapping, 0x42DB00-0x42DB90 (cursor visibility, pixel format, desktop WndProc with dead Ctrl+E 0x8049), 0x42DDF0, 0x42E750 MessageBox. These are replaced by SDL in `main.cpp`. The game-visible parts are kept: Pause toggles activity (main.cpp:334); Alt+Enter toggles full screen (main.cpp:333); losing the focus pauses (main.cpp:314).
- **WWindow / WView / WDialog framework** (≈45): 0x42DE00-0x42F830 (ctor and dtor, broadcast, draw, hit test, coordinate conversion, capture, focus, modal, child lists, z-order, flags, sound position, WView owner forwarding, WDialog open/close notifications). The port replaces these with its own `frontend/Screen` + `Canvas`. The game-visible behaviours I checked are reproduced:
  - Esc → 0x8123 (Screen.h:15);
  - Tab / Shift+Tab / arrow focus moves 0x8007 / 0x8008 (Screen.cpp:235-237);
  - Enter / Space activate (Screen.cpp:238);
  - push buttons take the right button too (Screen.h:91, 0x42FEF0);
  - `SetScroll` clamping (0x42F6F0 → BattleMap.cpp:991);
  - Align (0x42F020, used provisionally in BuildWindow.cpp:145);
  - Show/hide of the mini-map panel with Tab (0x42F310, WarMap.cpp:1316).

## Ported

- 0x420060 CountArriving → WarMap.cpp:2732 (limits 7/8), Production.cpp:73
- 0x4200E0 / 0x420120 tweaks WM clunting res 36 / adj 4 → FrontEnd.cpp:618
- 0x420160-0x4209E0 counter placement (grid, place with shrink, slide 5 px, free, view relayout) → WarMap.cpp:2823-3030 (checked against 0x4205F0)
- 0x420A30-0x421150 message list (10 lines, life 5×30, scroll 1 px/tick to −16, MPAD, history 0x6C/0x6D, last 2 on 0x80CC) → WarMap.cpp:184-230, 725, SaveFile (checked against 0x421150 / 0x4BA4F8)
- 0x4212A0 end code 0x80FA-0x80FD → WarMap.cpp:1568-1579 (checked)
- 0x421340-0x421F90 WarMapObjectList (activate = fog 0x421630, count 0x4216D0, load/save WMOB, find/count/sum 0x421C10 / 0x421C60 / 0x421CC0 / 0x421D00 / 0x421F90) → WarMap.cpp:265, 283, 3005; Production.cpp:406; WarMapAi.cpp:485; SaveFile
- 0x422860 / 0x4228B0 counts → WarMap.cpp:2012 (SpeedChangeAllowed); 0x422BA0 MarkAllSeen (336 h) → WarMap.cpp:2984
- 0x422C70-0x423820 mini-map view and frame, Research button, view buttons → WarMap.cpp:134-160, 2235-2290
- 0x423920 kind of facility type; 0x4239C0 / 0x423A00; 0x423F40 drop-off 3.0; 0x423F80 / 0x424000 marginal output → WarMap.cpp:248-271 (formula checked: (n+1)rB/(r+n) − n·rB/(r+n−1), B when n = 0)
- 0x4240D0 / 0x424160 route ctor (A*, attacks adjacent only, ETA length/(speed×0.05)) → WarMap.cpp:3056-3133; 0x4244E0 / 0x424A40 curve (5 passes, −5/11, chord heading checked at 0x424A82) → WarMap.cpp:2567-2618; 0x4249B0 / 0x424E90; 0x4254E0-0x4256E0 (show, hide, draw, AddEntry, SetAllHours via ArriveTogether WarMap.cpp:927)
- 0x426C10-0x4278A0 calendar (targets, odometer step tables 0x4BA6A0 / 0x4BA7A8 / 0x4BA8B0 / 0x4BA8E0 verified, slider, Advance time, 0x481 at level 3) → WarMap.cpp:2397-2530, 1268
- 0x427A80-0x4280C0 entry-list bookkeeping (add, remove, count, set route, min segment) → per-counter fields in WarMap.cpp
- 0x428110-0x4287A0 grid, spiral and clunter → WarMap.cpp:2801-2905, 3019-3030; 0x4287C0-0x428B10 message item → WarMap.cpp:198-219; 0x428B70 Clunting Speed tweak (but see bug 3)
- 0x428BB0 WMOB ctor; 0x429DB0 / 0x429E80 enter / change sector → WarMap.cpp:918, WarMapAi.cpp:154; 0x429F20 / 0x429FC0 counter create / destroy; 0x42A020; 0x42A190 / 0x42A220 / 0x42AA90 / 0x42AAC0 load / save (raw bytes) → core/SaveFile
- 0x42A2A0 / 0x42A340 battle-map centre → BattleMap.cpp:115; 0x42A3E0-0x42A450 accessors; 0x42AA70 IsFinished
- 0x42AAF0 / 0x42AB30 max units 15 / boats 5; 0x42AB70 / 0x42ABD0 A* cost with refusals 0-8 (checked: land bits 0-2, sea bit 4, command-post rules, `> max` limits with arriving entries) → WarMap.cpp:2719-2797
- 0x42AF20 / 0x42AF60 / 0x42AFE0 formation 4×4 → WarMap.cpp:3108-3127
- 0x42B790 counts "%d" / "%d/%d"; 0x42BEB0 cursors / select; 0x42C080; 0x42C160 select modes (checked); 0x42C2B0 box select → WarMap.cpp:1049, 2295-2395
- 0x42CCF0 movie post → FrontEnd.cpp:770; 0x42CD40 / 0x42DDB0 activation, Pause, Alt+Enter → main.cpp:314-334; 0x42D610 / 0x42D640 right-hold (Quick Cancel) → WarMap.cpp:948
- 0x42E9B0 capture, 0x42EBE0 focus, 0x42F310 show, 0x42F6F0 scroll clamp, 0x42F020 align, 0x42F850 dialog keys, 0x42F9B0-0x42FEF0 push buttons → frontend/Screen.*, BattleMap.cpp:991/1351
