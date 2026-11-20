# Part 03: 0x417A80 to 0x422860, the war map core (WWarMapWorld and friends)

## 1. Overview

This piece is almost entirely the **war map**: the game object `WWarMapWorld` (constructor 0x417CD0) and its
helpers. That covers its message handler and keyboard, the war map view window ("WarMap", 0x41C010), events and
critical-event dialogs, the calendar/clock (`WDocument`, 0x41F550), the date maths and date strings, the route
lists, the counter placement grid ("clunting", 0x420160), the message list (`PopupMessageController`, 0x420A30),
the `Battle Report` object (0x421210) and the generic `WarMapObjectList` (0x421340). It also covers the per-sector
economy (resources, warnings and alerts), Martian pods, the war map music and briefing sound, the end-of-game
results, and at the very end how war map units are **deployed onto the battle map** (0x421FE0 / 0x4222C0).

Two small items fall outside the war map. The first function, 0x417A80, is an inlined STL `string::rfind`
(called from 0x4175B0). 0x417BF0/0x417C30 are the debug log: `[Debug] "Log Debug Output"` (default 0); it
`vsprintf`s, calls `OutputDebugStringA` and appends to a log file opened `"a+"`.

**No hand-written assembly.** Everything here is ordinary MSVC C++: SEH frames, `thiscall`, `std::list` nodes
(`{next, prev, value}`, 12 bytes), refcounted objects (`word +4` is the refcount, `vtbl[0](1)` deletes) and
static-initialiser stubs that register settings followed by `atexit`.

### Object model used everywhere here (base class at 0x435310 / 0x4354C0)
- Every game object has a vtable. `+4` is a word refcount, `+0x10` its parent/owner, `+0x18` an "active"
  byte, `+0x1C` the owner world, `+0x20` a child list ("World tasks") and `+0x40` the current **child**
  (modal sub-screen, set by `sub_435450(child, arg)`). `+0x44` is a pause byte.
- vtable slots: [0] scalar deleting dtor, [1] `GetTag` (returns `'WOBJ'` 0x4A424F57), [2] **Load(stream)**,
  [3] **Save(stream)**, [4] **Tick**, [5] **Activate(parentView)**, [6] **Deactivate**, [7] **OnMessage(msg,
  wParam, lParam)**, [8] extra (broadcast or draw).
- `sub_4355A0(msg, w, l, delayTicks)` posts a delayed message to self through the game-thread queue at
  0x4D24C0. `sub_4355D0` does the same through the render-thread queue at 0x4DAF28. `sub_435600` flushes
  both queues for an object. `dword_4D2558` is the game tick counter.
- Message 0x111 is WM_COMMAND (low word = command id), 0x100 is WM_KEYDOWN, 0x200/0x201/0x202/0x204/0x205 are
  mouse messages and 0x20 is SETCURSOR. 0x8000+ are the game's private messages.

## 2. Subsystems

### 2.1 WWarMapWorld lifecycle (vtable 0x4A9A50)

| Addr | Name (proposed) | What it does |
|---|---|---|
| 0x417CD0 | `WWarMapWorld::WWarMapWorld(game, saveSlot, isNetwork)` | Zeroes its fields and sets view mode `+0x70 = 2`. Fades all sounds over 30 ticks (0x464AB0), calls `srand(1)`, clears the critical-event queue (0x41E7D0) and creates 3 `BattleMapObjectList` (0x47CD30) at +0x118/+0x11C/+0x120, `SGroupList` (0x440860) at +0x58 and the `Battle Report` (0x421210) at global 0x4E9B7C. Creates the players (0x4185F0) and the `PopupMessageController(0x28,4)` at +0x110. Loads palette group 0x2ED (hw), the sector highlight object `0x49BE70(0x7E)`, 31 sector objects `0x439480(world, i, Text(2+i))` at +0x94..+0x10C, the clunting grid (0x420160) at +0x74 and `activeRouteList` (0x41F990) at +0x90. Creates the research objects: own side's in 0x4E9B88 and the other side's in 0x4E9B8C (`0x459540` "Human Research", `0x45A680` "Martian Research"). Creates the calendar `WDocument` (0x41F550) at 0x4E9B80. Default selected sector `+0x60` = sector 30 (Human) / sector 1 (Martian). Sets the shade ranges of the five sector-shading tables (§3.3). Loads the save (0x418500), starts the CD (0x412960) and posts 0x813C (music) after 5 s (`dword_4B84C8*5`). |
| 0x418220 | `~WWarMapWorld` | Sets `byte_4D2530 = 1` (world shutting down), stops sound 0x4E9B84 (fade 0x78), stops the ambient sound in +0x12C, fades all sounds, stops and resets the CD (0x412C00, 0x412940, 0x412B50), then releases every owned object, the 31 sectors, both players and the research objects. |
| 0x418500 | `LoadGame(slot)` | `byte_4E9B90 = 1` (loading: events are suppressed). Builds the file name (0x404C60, `SaveGame\%s.%03d`), resets globals (0x4011D0, 0x4424B0) and, if the file exists, opens a stream (0x44E7D0/0x44E8F0) and calls `this->Load` (vtbl+8), then 0x48B580 and close. Ends with 0x441110. |
| 0x4185F0 | `CreatePlayers(isNetwork)` | `byte_4B84C4` ("I Am Human") picks the side. The local player is 0x438B20 (0x43C bytes, "Human"/"Martian"). The opponent is 0x445EA0 (0x43C, **network** player) when isNetwork, else 0x442890 (0x548, **AI** player). Results go to 0x4E9B74 (local) and 0x4E9B78 (opponent). The player object's `+0x24` is its side (0 Human, 1 Martian). |
| 0x418980 | vtbl[5] `Activate` | Builds the view. Centres on the selected sector's rect (0x4BAE78, 32 bytes a sector) or the saved `+0x50/+0x54`; unselects if the sector isn't ours. Creates the highlight (0x49BE70, 0x7E) at +0x78, the map object `0x4238D0(highlight, 0x7D)` at +0x7C (`+0x1E=2`), the WarMap view (0x41C010) at +0x80, the miniMap frame (0x423420) at +0x88 and the miniMap view (0x422C70) at +0x84. Refreshes sectors (0x4192A0, 0x4192F0) and runs the economy update phases 0-4 (0x41A760). Activates the calendar, the popup list and the frame position (see WARMAP.md, 0x418C14). Sets the view mode (0x419810) and posts 0x7A (scroll-to). Sends 0x8074 to both players and research vtbl+0x28. Rebuilds routes from each sector's object lists (0x4217E0 on +0x4C/+0x50) and sends 0x8107 to all 6 lists of every sector. If `4E9B7C+0x54` or `byte_4E9B94`, posts 0x808D (battle result) after 30 ticks. **Post-battle intel**: for each sector with `+0x128` set (a battle has just been fought there), all 6 of its lists are marked visible (0x422BA0 sets each object's `+0x3C = 0x150` = 336 hours). Then 1-3 neighbouring enemy sectors (neighbours from 0x441AD0) are revealed too, the count being how many of 3 research items are completed (`0x45B890`: state==2; Human items 0x588D8C/90/94, Martian 0x589064/68/6C; which set is used depends on `dword_4E9B60`). Finally it re-dispatches any queued critical event (0x41E730) and sets `+0x130 = 1` (music allowed). |
| 0x419030 | vtbl[6] `Deactivate` | Releases the view objects, the zoomer (+0x124) and the calendar/popup views. If the message list is open (+0x6C), closes it (sound 0x196, cmd 0x6D). |
| 0x41AB00 | vtbl[4] `Tick` | Skipped when shutting down, inactive or paused (`+0x44`). Ticks the counter placement (0x4209E0). **Battle detection**: if no battle is pending (`dword_4E9B98 == 0`), for each sector 1-30 it counts both sides' units and constructions (lists +0x4C+0x54 vs +0x50+0x58, 0x4216D0). If both sides are present and the sector isn't already flagged (`+0x124`), then: if `0x43C8D0` (undefended?) is true it raises event 17 *UndefendedSectorLost* and transfers the sector (`0x43A250(isPlayer)`); otherwise it raises event 12 *PodLanded* (if the pod flag `+0x64` is set) or event 0 *BattleInSector*, and sets `dword_4E9B98 = sector`. Last it runs the economy phase `0x41A760(tick % 5)`. |
| 0x41AC40 | vtbl[2] `Load` | Reads the WMAP block: view x, y, selected sector, view mode. Then the calendar, both research objects (own first), popup messages, routes, `+0x48` (pod hour accumulator), `+0x4C` (pods landed), then 31 sectors. Sends 0x80CB to both players. |
| 0x41AD80 | vtbl[3] `Save` | Writes `'WMAP'`, then `+0x50`, `+0x54`, the selected sector index and `+0x70`, then the same order as Load. |
| 0x41A3F0 | vtbl[8] | Broadcasts (msg, w, l) to all 31 sectors through vtbl+0x1C, and also vtbl+0x20 if the 4th argument is nonzero. |
| 0x419750 | vtbl[9] `NewSelection(flag)` | New `WarMapSelection` (0x425B80, 0x2C bytes, `+0x29 = flag`), added to SGroupList +0x58. `dword_4E9B9C` counts them. |
| 0x4197D0 / 0x4197F0 | `AddSelection` / `RemoveSelection` | Adds to or removes from +0x58, adjusting 0x4E9B9C. |
| 0x418890 | `RefreshAllSectors` | Sends WM_COMMAND 0xB7 to sectors 1-30, then 0x4192F0. |
| 0x4192A0 | `RefreshSectors(now)` | The same 0xB7, either immediately or posted with delay = sector index. |
| 0x4188D0 | `BroadcastToSectors(m,w,l)` | Sends to sectors 1-30 unless shutting down or `byte_4D253C`. |
| 0x418930 | `SendToSectorsUntilHandled(id, l)` | Sends WM_COMMAND id to sectors 1-30 and stops at the first nonzero return. Used with 0x8010/0x8011 (rubber-band select). |
| 0x4191A0 | `Message(world, fmt, ...)` | `vsprintf` then add a line to the popup list (0x420DA0). |
| 0x4191F0 / 0x419200 | `MapWidth` / `MapHeight` | Width and height of the map object +0x7C (`+0x2A-+0x28`, `+0x2E-+0x2C`). |
| 0x419210 | `SectorAt(x,y)` | Looks the value up in the highlight object (0x49BF50) and returns the sector if it is below 31. |
| 0x419240 | `Sector(i)` | `+0x94 + i*4`, or 0 if i ≥ 31. |
| 0x419260 / 0x419280 | `SetSelectedSector` / `SetHoverSector` | Write `+0x60` / `+0x5C`. |

### 2.2 WWarMapWorld::OnMessage 0x4198C0 (vtbl[7]) and keyboard 0x41A450

**WM_KEYDOWN → 0x41A450(vk)** (world level; the view handles other keys, see 2.4):

| Key | Action |
|---|---|
| Backspace (8) | `0x41A6C0(0)` (clears the auto-advance flag, see below) |
| Tab (9) | Toggles the miniMap view and frame (0x42F310 with 0 or 5, chosen by bit 28 of the view's `+0x40`) |
| Space (0x20) | Cycles the view mode: Unit → Construction → Resource(sub 0) → Resource(sub 1) → Resource(sub 2) → Unit. Uses `dword_4E9F40` (resource sub-type 0-2), `byte_4E9F44` and `byte_4E9F48` |
| Home (0x24) | Centres on the player's current object (`player->0x439230`, vtbl+0x30 rect) by sending 0x7A |
| '0'..'9' | Only if no sector vetoes (0x43D010 for sectors 1-30). Sets the clock speed to the digit, with '0' meaning **10** (0x41F640), then `0x41A6C0(1)` |

`0x41A6C0(on)` is the world `+0x64` flag. **Guess:** "auto-advance / clock running unattended". When it changes
it records action 0xA (on) or 0xB (off) into the player's command stream (0x4460A0/0x4460C0: a 4-byte record
`{01,01,code,04}` appended at `player+0x28`; probably the playback or network command log) and calls the
opponent's vtbl+0x30(on). It is cleared by Backspace, by left/right mouse down on the map, by speed 0, and by
every critical-event dialog. Its exact gameplay meaning is unclear.

**WM_COMMAND** (low word):

| Id | Action |
|---|---|
| 0x6C / 0x6D | Open / close the message history (flag `+0x6C`; sounds 0x193 / 0x196; forwarded to the popup list) |
| 0x71, 0x7D, 0x97, 0x98, 0x99, 0xBD, 0x8017, 0x8019 | **Deferred**: stored in `4E9B64/4E9B5C` and posted back as 0x95, then dispatched on the stored id: |
| ↳ 0x7D | **Research screen**: `0x41A6C0(0)`, deactivate, child = own research object 0x4E9B88 |
| ↳ 0x71 (and 0x98 after a queue flush) | **Enter the sector (battle map)**: 0x44E5B0(1), research vtbl+0x28, 0x42DB00(1), deactivate, `byte_4D2554 = 1`, the selected sector becomes `sectors[4E9B98]`, child = that sector |
| ↳ 0x97 | **Debriefing**: 0x42DB00(0), child = `new 0x49A7B0(world, 0)` (0x124 bytes) |
| ↳ 0x99 / 0x8017 | **Back to the war map**: clear child, reactivate, fade sounds, drop the ambient sound, post 0x8147 after 30 ticks, send the view 0x6E (unless the game is over, `4E9B7C+0x55`) |
| ↳ 0xBD | Clear child and reactivate |
| ↳ 0x8019 | **Briefing**: if a sector is selected, child = `new 0x499930(world, 0)` (0x120 bytes) |
| 0x73 | If no sector is selected, record action 2 (deselect) |
| 0x76 | Flush queues, then 0x97 (debrief) |
| 0x7A | **Scroll to** lParam (packed x \| y<<16): posts 0x8004 to the WarMap view and miniMap; stores `+0x50/+0x54` |
| 0x92 / 0x93 / 0x94 | View mode 3 / 2 / 1 |
| 0x811F | View mode from lParam (the panel buttons) |
| 0x8028 | `sector[l->+4]->0x43D4F0(l->+0xC, l->+8)` (unclear; adds or moves objects in a sector) |
| 0x8050/0x8054, 0x8051/0x8055 | Increment / decrement the counter `+0x68` and set the calendar's `+0x38` paused flag when it equals 1. On increment with `byte_4D256C` (debug?) the speed is set to 10. Purpose unclear (probably a modal-dialog nesting counter) |
| 0x8072 | **Go to the pending battle**: selected = `sectors[4E9B98 or l]`, send self 0x8133 with l = 0x8019 |
| 0x808D | Battle result announcement (0x41AED0) |
| 0x808F | `byte_4EA3E4 = 1` (unclear) |
| 0x80A8 | Add the message line (string lParam) |
| 0x80B3 | Message 0x465 "Game Loaded" |
| 0x80CC | Forwarded to the popup list (show the last 2) |
| 0x8123, 0x813C, 0x8147 | Music step (0x41B070). 0x8147 also sets `+0x130 = 1` |
| 0x8133 | Briefing transition (0x41B210) with the follow-up command in lParam |
| default | Forwarded to the selected sector, else to the player's current object |

Other messages: **0x800F** (refresh; also sent when the view mode changes) is broadcast to sectors, the view and
the miniMap, then the cursor is set (`0x42E790(player.hasSelection,0)`). **0x802D (hours elapsed)** is the hourly
step (see 2.6).

### 2.3 View modes and sector shading
| Addr | Name | Notes |
|---|---|---|
| 0x419810 | `SetViewMode(m)` | `player->0x438F00` (clear selection?) and self 0x800F. Presses the panel button with 0x8034/0x811F (0x94/0x93/0x92). Hides and reshows the view, `+0x70 = m`, clears and replaces the counters (0x4204A0), re-activates the view. |
| 0x41A730/40/50 | `ViewUnits/Constructions/Resources` | Thin wrappers used by event handlers to switch view when announcing |
| 0x4192F0 | `ComputeSectorShadeTables` | Fills 5 tables (§3.3): table 0 unit strength (`dword_4F7654[s]` for our sectors, 0 otherwise); tables 2-4 the three resources of the side (`0x424000(s, kind)`, kind base 0 for Human, 3 for Martian; in a network game `dword_54604C>1` the base is reversed). Records min and max (starting at ±1e6). |
| 0x41A760 | `EconomyPhase(p)` | One phase per tick (`tick % 5`) over the sector table 0x4EAE1C (0xC0 a sector): **0**: for each owned sector, gets the construction list (`0x446370(3, isPlayer)`), sends it 0x803C and stores 6 floats = production per resource (0x421D00) at `+0x64`. **1**: `+0x7C[k] = Σ_j (stock / (sector_j->0x43C090(s)->+8 * flt_4F86B4 + 1)) + flt_4F740C` (distribution weights; exact meaning unclear). **2**: zeroes `+0x34[6]` and sends 0x8143 to each sector's lists +0x4C..+0x58 (consumption pass). **3**: `+0x4C[k]` = similar sum with `flt_4F869C`. **4**: `+0x94[k] = min(1, (x[k]-x[k+0x30])/... + 1)`; then per sector `1 - 1/(r0*r1*r2)` (forced to 0 by `byte_58913C`, likely a cheat) goes to table 1 with min/max at 0x4E99A8. Treat the formulas as "to be re-derived". |
| 0x41D9B0 | WarMap view draw (shading part) | Table index: Unit view 0, Construction 1 (max forced to 1.0), Resource 2+sub (max forced to 80.0). **Own sectors**: `level = lo + (v-min)/(max-min) * (hi-lo)`. **Other sectors**: in Resource view they use the table too; otherwise neutral = 11 and enemy = 11 if index≠0 but **0 in Unit view** (enemy hidden). Borders as in WARMAP.md (0x41/0x42/0x3D/0x3E/0x40/0x3F logic at 0x41DBxx-0x41DE64). |
| 0x41DE70 | Per-sector overlay | Draws all routes once (the hover preview `world+0x8C` and the active routes, 0x41FCA0). If the sector has a battle (`+0x124`) it **pulses** it: frame counter `+0x8C` 0..22 as a triangle 0..11, level = 36 (+11 if not ours) + tri. |

### 2.4 The WarMap view window ("WarMap", vtable 0x4A9AC8)
| Addr | Name | Notes |
|---|---|---|
| 0x41C010 | ctor(world, parent) | Window id 0x66, style 0x10200000, the screen rect (0x4E9250/dy, size 0x4EA370/74); `+0x84 = world`. Posts 0x8052 on the render queue. Creates the hover route list `"1st wmrl"` in `world+0x8C`. |
| 0x41C140 | dtor | |
| 0x41C1F0 | 0x8052 `CreateSurface` | Tiled renderer `0x40CF00(0x400, rect, 64, 64)` at +0x7C sized to the map (0x40E580). Palette group 0x2ED. Scroll to (0x4E9B68, 0x4E9B6C). |
| 0x41C2E0 | Activate | Unit view: activates the route list and 0x41FF20. Construction / Resource views: per sector `0x43CB20` + `0x43CBB0`. Mode 3 creates 3 `0x426840` + 3 `0x426140` objects per sector (0xAC/0xF4 bytes; resource gauges?) into 0x4E9BF8[180]. |
| 0x41C530 | Deactivate | Releases the above and calls `0x43CC00` per sector |
| 0x41C5E0 | `OnMessage` | See below |
| 0x41CD50 | `OnCommand(id, l)` | Menus, see below |
| 0x41D120 | 0x805B `ContextMenu(w==QuickCancelTimeout, pos)` | Right-button **hold** for `[Tweak] "Quick Cancel Timeout"` (5) ticks opens popup menu `0x432C60(0x91, 0x38, "Warmap menu", 0x71)` with item 0x73 "Cancel" (0x476). Items are filled by the active tool (`0x4EA444` if it answers 0x8111), else the player's current object, else the sector under the cursor (own sector, or any with cheat flags `byte_4D1249/4D1279`; selects it and records action 2) through 0x805C. Shown at the cursor (0x432FB0). |
| 0x41D300 | WM_LBUTTONUP | Ends a drag. If the rubber band (tracker `dword_4E9EE0`, vtbl+0x10) completed and `world+0x114`: record action 4 and send sectors 0x8011 (select in rect), cursor (1,0). Else, if the cursor mode (`dword_4EA43C`) is 2 or 3, forward the click to the hover object `world+0x5C`. Else select the clicked sector (own, or with cheats) and record action 2. |
| 0x41D470 | Hover (mouse move without drag, also 0x800A) | Updates the hovered sector: rebuilds the **route preview** (0x41D590) and sets the cursor (2 = valid move if `0x41FF70` > 0). Sends 0x8006 (leave) to the old target and 0x20 to the new, stores `world+0x5C` and forwards the message to it. |
| 0x41D590 | `BuildRoutePreview(sector)` | In Unit view, when not dragging (0x460D60) and the player has a selection: `world+0x8C = 0x419500(selection, sector)`. |
| 0x419500 / 0x419520 | `MakeRoutes(selection, dest)` | Groups the selected objects into lists by (sector, move class: land 0, flier 1 (class 7), boat 2 (classes 5/6) from unit `+0x52` at 0x51A142) and makes one `WarMapRoute` (0x424160, 0x70 bytes) per group, added to a new `WarMapRouteList`. |
| 0x41D640 | View keys | Esc → 0x80B5 (war map menu). 'M' → 0x6C (messages). 'R' → 0x7D (research). Home → centre on the selected sector (0x41E000). PgUp → y=0. PgDn → y=map height. Up/Down: ∓h/16 (Ctrl ∓h/8). Left/Right: ∓w/7 (Ctrl ∓w/3). All go through 0x7A. |
| 0x41E000 | `CentreOnSector(s, animate)` | The rect centre less half the view. Animated: `new Zoomer 0x446690(view, x, y, [Tweak]"Warmap Zoomer Speed"=1.5)` stored in `world+0x124` (0x41B040). Otherwise it sends 0x7A. |
| 0x41E140 | vtbl | Forwards to 0x42E0E0 |

**0x41C5E0 messages:**
- **0x100**: F1 Unit view; F2 Construction; F3/F4/F5 Resource view with sub-type 0/1/2. Other keys go to the player's object, then the selected sector, then 0x41D640, then the world.
- **0x200** mouse move: with LButton, update the drag tracker (sectors get 0x8010 while dragging); else hover.
- **0x201** LButtonDown: `0x41A6C0(0)`, start the tracker, capture (0x42E9B0).
- **0x202** LButtonUp: 0x41D300.
- **0x204** RButtonDown: `0x41A6C0(0)`.
- **0x205** RButtonUp: if not MK_CONTROL and the time held is ≤ Quick Cancel Timeout, command 0x73 (**quick cancel**).
- **0x8004** scroll (0x42F6F0); **0x800F** refresh (drops the hover route list); **0x803B** → 0x42DB00(1); **0x8113** set the resource sub-type.

**0x41CD50 (commands):**

| Id | Action |
|---|---|
| 0x6E / 0x80B3 / 0x80B4 | Unpause (`world+0x44 = 0`), time scale `dword_4CA998 = 1.0`; 0x6E also sends self 0x80B4 with `world+0x128` |
| 0x6F | Load Game menu (0x409560, back 0x80B5) |
| 0x70 / 0x90 | Sends self 0x80B4 with the slot (return to the main menu) |
| 0x3ED | Sound control / volume menu (0x4057F0) |
| 0x3EF, 0x8117 | Event Menu (0x409DE0, critical event options) |
| 0x80B5 | **War map menu** (0x49B980): pause (`+0x44 = 1`), time scale 0.25 |
| 0x80B6 | Quit Game menu (0x409460, back 0x8123) |
| 0x8122 | Critical-event dialog closed: run the next queued one (0x41E730) |
| 0x8123 | Unpause, scale 1.0, forward 0x8123 to the world (music resumes) |
| 0x813E | Forward to the player's object |

All menus are added modal with flag 0x100000 (0x42F3D0).

### 2.5 Events and critical-event dialogs
Table 0x4BA0E8, 12 bytes an entry (handler, isCritical, name), raised by `0x41E5B0(id, param)`. Events are
ignored while loading (`byte_4E9B90`). If an entry is critical and a dialog is already up (`byte_4EA0A0`), the
event is queued (std::list at 0x4EA064, count 0x4EA068); otherwise the handler runs and sets the dialog flag.
Non-critical entries just run the handler. 0x41E730 (cmd 0x8122) pops the next queued event of the same kind.
0x41E7D0 clears the queue.

`0x41E4E0(text, sector, buttonText, cmd)`: `new InfoBox 0x432520` (0x9C bytes), stop auto-advance
(`0x41A6C0(0)`), centre on the sector with the zoomer, **sound 0x198**.

| # | Name | Handler | Critical test | Behaviour |
|---|---|---|---|---|
| 0 | BattleInSector | 0x41E850 | always | `sector+0x124 = 1`. Own sector: dialog 0x4AF "Enemy have attacked" / button 0x4B2 "Defend", sound 0x138. Else dialog 0x4B0 "Invasion commencing" / 0x4B1 "Attack", sound 0x139 (0x13A for sectors 3 and 30) with volume from `player->0x4392E0`. Cmd 0x8072. Message "Battle in %s" (0x210). |
| 1 | ConstructionCompleted | 0x41EDC0 | 0x4EA088 | "%s completed." (0x4B4 + name 0x440720). Switches to the Construction view (Resource view if the type's `0x51A100 == 2`) and opens a dialog if the setting is on. Sound 0x12F unless class 7. |
| 2 | UnitCompleted | 0x41ECD0 | 0x4EA08C | "%s unit completed." (0x4B3). Sound 0x131 if `0x51A148 == 1`, else 0x1C0. |
| 3 | ArrivedInSector | 0x41EAE0 | 0x4EA094 | "Vehicle/s arrived in %s" (0x211), sound 0x120 |
| 4 | ResearchCompleted | 0x41EB90 | 0x4BA0DC | 0x46B + item name, "%s .. %s"; `0x414700(0x46B,0)` |
| 5 | ResearchAvailable | 0x41EC30 | 0x4BA0DC | 0x46C "New research available" |
| 6 | ResearchAlert | 0x41F220 | !cheat 0x4D1261 | If `research+0x88`: dialog 0x421 "No research is currently in progress", sound 0x135 |
| 7/8 | ConstructorOnline / FacilityOnline | nullsub | 0x48B580 | nothing |
| 9 | ConstructorIdle | 0x41EEA0 | 0x4BA0D0 | 0x416 "Vehicle idle" (Unit view) |
| 10 | FacilityIdle | 0x41EF10 | 0x4BA0D4 | 0x417 "Facility idle" (Construction view) |
| 11 | TravelInterrupted | 0x41EF80 | 0x4BA0D8 | 0x419, sound 0x147 |
| 12 | PodLanded | 0x41F010 | !0x4D1261 | Martian-held sector: sound 0x1B2, "Pod landed: %s" (0x41E). Otherwise `+0x124 = 1`, sound 0x1B3, Human "Pod landed in our territory" (0x420)/Defend, Martian "Pod landed in enemy sector" (0x41F)/Attack, cmd 0x8072 |
| 13 | ResourceWarning | 0x41F280 | 0x4EA098 | "Resource warning: <res>" (0x41B, resource name 0x214+side*3+k), sound `0x4BA1D8[k]` (0x124,0x122,0x123 / 0x125,0x126,0x127) |
| 14 | ResourceAlert | 0x41F340 | 0x4BA0E0 | The same with sounds `0x4BA1F0[k]` (0x12A,0x128,0x129,0x12B,0x12C,...) |
| 15 | CommandCentreBuilt | 0x41F1C0 | 0x4EA090 | 0x415 |
| 16 | EnteringNeutralSector | nullsub | 0x4EA09C | nothing |
| 17 | UndefendedSectorLost | 0x41E960 | !0x4D1261 | 0x49E "Undefended Sector Lost" (ours) / 0x49F "...captured", prefixed "Pod landed" + sound 0x1B2 if the pod flag is set; clears `+0x64` |
| 18 | End Game!! (win) | 0x41F400 | always | Dialog 0x533 "Victory is ours!", sound 0x13E, `byte_4D2530 = 1`, posts 0x80FE to the game (0x4D2538) with lParam = `BattleReport::EndCode` (0x4212A0) after the sound length + 30 |
| 19 | End Game!! (loss) | 0x41F4B0 | always | `byte_4D2530 = 1`; lParam 0x80FB (Human) / 0x80FD (Martian) if `byte_4E9B94`, else EndCode; posts 0x80FE after 30 ticks |

`[Event]` settings (0x411890 binds a bool; defaults are 1 for the 0x4BA0xx bytes and 0 for the 0x4EA0xx
bytes): "Construction Completed" 0x4EA088=0, "Unit Manufactured" 0x4EA08C=0, "Command Centre Built" 0x4EA090=0,
"Constructor Idle" 0x4BA0D0=1, "Facility Idle" 0x4BA0D4=1, "Unit Arrived At Destination" 0x4EA094=0,
"Travel Interrupted" 0x4BA0D8=1, "Research Completed" 0x4BA0DC=1, "Resource Warning" 0x4EA098=0,
"Resource Alert" 0x4BA0E0=1, "Entered Neutral Sector" 0x4EA09C=0 (statics 0x41E160-0x41E3E0, tests
0x41E420-0x41E4C0). These are the toggles in the Event Menu (0x409DE0).

### 2.6 Hourly step (message 0x802D, hours in wParam)
Forwarded to the active route list, both players and both research objects (`0x458BD0(hours)`). Then:
- **Pods** (0x41A330), Martian landings. `+0x48 += hours`. When `+0x48 ≥ [Tweak] "Pod Interval (hours)"`
  (default 24, 0x4E989C) and `+0x4C < 8`, a pod lands. The sector is `byte_4B9DE8[rand()&7]` = {2,4,6,7,8,9,10,11}.
  The contents are `off_4B9E28[rand()%4]` = {62}, {62,89}, {62,91}, {62,89} (unit types, each added with
  `sector->0x43CEB0(type)`). A Human-held sector gets the pod flag `sector+0x64 = 1` and the battle detection
  raises PodLanded; otherwise event 12 is raised directly. `+0x4C++`.
- Every sector gets 0x802D.
- **Resource warnings**: for the player's sectors, ratio per resource k = `stock(+0x30)/max(+0)` from the
  sector table (`0x4EAF28 + side*... + s*0xC0`, 3 resources). The per-resource minimum over sectors decides:
  below 0.3, counter `0x4E9BA0[k]` grows by 1 a call and past 0x150 (336) raises **ResourceAlert** (event 14,
  param `sector<<16|k`); below 0.8, counter `0x4E9BB0[k]` past 0x1F8 (504) raises **ResourceWarning** (event
  13). Note: the counters grow per 0x802D message, not per hour.

### 2.7 Battle result, end of game, music
| Addr | Name | Notes |
|---|---|---|
| 0x41AED0 | `AnnounceBattleResult` (cmd 0x808D) | If `byte_4E9B94` (set by the battle map 0x48ACEF): event 19 (loss) for sector 30/3. Else if the Battle Report has winner (+0x68) ≠ loser (+0x6C): if the player won, event 18 (Victory) when `+0x55` (game over) else sound 0x13C; if the player lost, event 19 when `+0x55` else sound 0x140. A draw plays sound 0x13D (if the player is +0x6C) or 0x13F. Clears `+0x54`. |
| 0x421210 | `BattleReport` ctor ("Battle Report", 0x70 bytes, vtable 0x4A9B98) | `+0x10..+0x4C` 16 counters, `+0x50` sector, `+0x54` result pending, `+0x55` game over, `+0x68`/`+0x6C` = 2 (sides; meaning winner/loser by usage) |
| 0x4212A0 | `EndCode` | If game over: sector 3 & winner Human → 0x80FA (Human victory) / 0x80FD (Martian defeat). Sector 30 & winner Martian → 0x80FB (Human defeat) / 0x80FC (Martian victory). Used as lParam of 0x80FE to WGame (→ ending FMV, to be confirmed in WGame 0x402B80). |
| 0x4212F0 | `ComputeSurvivors` | `+0x58..+0x64 = start - lost - retreated` for 4 categories (fields +0x40/+0x48/+0x20 etc.). Called by the debriefing 0x43A800. |
| 0x41B070 | `MusicStep` (0x813C, 0x8123, 0x8147) | Only when active and `+0x130`. Stops the ambient sound (+0x12C, fade 60). If CD music is enabled (`dword_4B8A88`): when the CD isn't playing (0x413EB0), alternate the track `ArgList[idx]` = **CD tracks 2 and 3**; if its length (0x413DF0) > 10 s, play it (0x414150, log "Playing track %d for %d seconds") and repost 0x813C after length×30 ticks; else check again in 10 s. Without CD music: ambient sfx **0xFD + rand()%3** with fade-in 60, repost in 10 s. |
| 0x41B210 | `BriefingTransition(cmd)` (0x8133) | "Fading up briefing sound over 1 second": fade all, stop the CD, send the view 0x6E, play sfx **0x18E** into 0x4E9B84 with fade-in 60. If the game CD is present (0x408B70: `Human.cd`/`Martian.cd`), send self cmd; else open the **Insert CD menu** (0x4089D0) with the cmd. |
| 0x41B360 | `StopBriefingSound` | Fade 0x78 (called by 0x439770, battle map setup) |

### 2.8 Calendar / clock (`WDocument`, vtable 0x4A9B18; global 0x4E9B80) and dates
| Addr | Name | Notes |
|---|---|---|
| 0x41F550 | ctor | `+0x20` calendar view, `+0x24` speed index, `+0x28` game time in hours (float), `+0x2C` hours per tick, `+0x30` forced-hours countdown, `+0x34` clock sound, `+0x38` paused. **Start date: day 0, month 8, year 0x76A = 1 September 1898** (0x4EA0BA/BC/BE). |
| 0x41F5C0 | dtor | Stops the sound |
| 0x41F640 | `SetSpeed(i)` | Sends 0x8073 (i<<16) to the calendar view; hours/tick = `0x4BA470[i]` (0,1,...,10) / Game Frequency (30); speed 0 → `0x41A6C0(0)`. Also 0x41F860. |
| 0x41F6B0 | | world+0x20 (task list) `0x440060` (unclear; called after 0x8050) |
| 0x41F6C0 | vtbl[4] `Tick` | If `+0x30 > 0`: +1 hour and decrement (forced fast-forward); else `+= +0x2C`. Whole hours passed → `date.Advance(h)` (0x41B770) and world 0x802D(h). |
| 0x41F720 / 0x41F760 | Load / Save | `'CALN'`, float hours, then `'DtTm'` (0x41B8D0/0x41B930: 6 words) |
| 0x41F7A0 / 0x41F820 | Activate / Deactivate | Calendar View (0x426C10, 0x110 bytes) |
| 0x41F860 | `ClockSound` | Human: when speed is 0 plays sfx 0x1A2 (looping) and stops it (fade 15) once running. Martian: plays `0x4BA4A0[speed]` = 0x1A1+speed with 5-tick fades. |
| 0x41B3A0/0x41B3F0/0x41B430 | DateTime ctor/copy/dtor | vtable 0x4A9AB0. Fields: `+4` hours elapsed, `+6` minute, `+8` hour, `+0xA` day (0-based), `+0xC` month (0-based), `+0xE` year (u16 each). Global instance 0x4EA0B0 (0x41F510). |
| 0x41B770 | `Advance(hours, minutes)` | Normalises 60 min / 24 h / month lengths (`0x4B9FE0`, Feb 29 when y%4==0 && (y%100 || y%400==0)) / 12 months |
| 0x41B440 | `FormatCalendarDate` | Human: "hh:mm am dd Mon yyyy" (12-hour, am/pm from 0x4B9FC0, month names `off_4B9F90`). Martian: "dddd.hh" days with hundredths (hours×4.1666665). Buffer 0x4B9FC8. Used by 0x4999E0 (briefing). |
| 0x41B9A0 | `FormatArrival(hoursAhead, forceDate)` | (described in WARMAP.md; buffer 0x4B9FF8) |
| 0x41BDA0 | `FormatDuration(hours)` | Human: ≥ 2 days → "N days"/"day" (0x45E/0x45D), else "N hrs"/"hr" (0x460/0x45F); Martian: always days. Buffer "1234days........". Used by 0x45B6D0 (research). |
| 0x41BF90 | `LoadMonthNames` | Text ids `0x4B9F60[12]` (0x521...) → `off_4B9F90` (once, flag 0x4BA01C) |

### 2.9 Route lists (`WarMapRouteList`, 0x41F990, vtable 0x4A9B48)
Lists of `WarMapRoute` (0x4240D0). `+0x1C` byte, `+0x20` std::list, `+0x24` count. Instances:
`activeRouteList` (world+0x90), `"1st wmrl"` (hover preview, world+0x8C) and temporary ones.

| Addr | Name | Notes |
|---|---|---|
| 0x41FA20 | dtor | |
| 0x41FDD0 / 0x41FEC0 | Load / Save | Count, then each route; Load calls 0x41FCF0 afterwards |
| 0x41FB40 / 0x41FB90 | Activate / Deactivate | All routes |
| 0x41FBD0 | `Remove(route)` | When the list becomes empty, sends world 0x802C |
| 0x41FCA0 | `DrawAll(surface)` | 0x425530 each |
| 0x41FCF0 | `SyncArrivals` | Per destination sector, the max of 0x4256E0 (arrival time). Routes with `+0x5E` get `0x4255F0(max)`, so routes into the same sector arrive together (guess) |
| 0x41FF20 | Activates each route's arrow (+0x64, 0x427C80) | |
| 0x41FF70 | `CountValid` | Routes with `+0x5D` (valid) |
| 0x41FFB0 | Starts routes without `+0x5C` (0x425770) | |
| 0x41FFF0 | `Find(from, to, isBoat)` | Matches route +0x24 sector, +0x20 sector, +0x50 |
| 0x420060 | `CountArriving(sector, &land, &boat)` | Sums 0x427E10 per route into the sector. Used by 0x42ABD0 for the "Max units/boats in sector" limits. |
| 0x4217E0 | `RebuildRoutesFromOrders(list)` | For each object, asks 0x80BA for its destination. Own units: reuse or create a route (0x424160) and add the unit (0x425570). Enemy: `+0x58` = sector, cmd 0xAD; in a network game it is placed at the destination waypoint. |

### 2.10 Counter placement ("clunting", 0x420160, vtable 0x4A9B6C; world+0x74)
`+4` byte, `+8` list of placed counters (`0x428620`: 0x30 bytes {obj, cellIndex(+4), ...}), `+0xC` count,
`+0x14..` 30 sector grids (`0x428110`, 0xAC bytes) of `[Tweak] "WM clunting res"` (default **36**, 0x4EA0D4)
pixel cells over the sector rects (0x4BAE78). CHI.SPR (`0x49BE70(0x84)`) is the cell test.

| Addr | Name | Notes |
|---|---|---|
| 0x420270 | dtor | |
| 0x4203C0 | `ResetSectorGrid(s)` | Rebuilds the grid if its cell size isn't the default (called by 0x43CB60) |
| 0x4204A0 | `ClearAll` | On a view mode change |
| 0x420540 | `AddAt(obj)` | Placed at the object's saved position (`+0x2C,+0x30`) |
| 0x4205F0 | `Place(obj)` | Class-7 objects share one counter a type (0x4282A0/0x428270). Spiral mode: 3 in Unit/Construction view, 2 in Resource view (0x4283F0). **If the sector is full**, the grid is rebuilt with cells smaller by `[Tweak] "WM clunting adj"` (default **4**, 0x4EA0EC) and every counter in the sector slides to its new cell (`0x4286C0(x,y,5.0)`). The new counter is placed (0x4406A0) with `0x4286C0(x,y,0)`. |
| 0x4208B0 | `MoveTo(obj, x, y, t)` | Frees the old cell (`grid.cells[i] &= 0x7F`), animates |
| 0x420930 | `Remove(obj)` | |
| 0x4209E0 | `Tick` | Per counter: `0x4287A0` (done?) else `0x428730` (step the animation) |
| 0x421A50 / 0x421AA0 | `PlaceAll(list)` / `ReplaceAll(list)` | |

### 2.11 Message list (`PopupMessageController`, 0x420A30, vtable 0x4A9B70; world+0x110)
`+0x1C` scroll y, `+0x20` scrolling, `+0x24` byte, `+0x28` list of "Message Item" (0x428810, 0x20 bytes;
`+0x14` life, `+0x18` state 0 live / 1 expired / 2 gone), `+0x2C` count, `+0x30/+0x34` position (0x28, 4),
`+0x38` items scrolling.

| Addr | Name | Notes |
|---|---|---|
| 0x420AD0 | dtor | |
| 0x420DA0 | `Add(str)` | Drops the oldest when the count is ≥ 10 (0x4BA4FC); life = 5 s × 30 (0x4BA4F8) |
| 0x420C10 | `Draw(surface)` | Lines 16 px apart (0x4BA500), offset by the scroll |
| 0x420CD0 | vtbl[4] `Tick` | Ages items. When one expires the list **scrolls up 1 px a tick** until −16, then the top item is hidden |
| 0x420EE0 / 0x420FD0 | Load / Save | Tag `'MPAD'` (0x4441504D), count, items (0x4287C0) |
| 0x421040 | OnMessage | 0x6C → 0x421150 (show all again: reset each life, then repaint `0x4EA448` vtbl+0x38). 0x6D → hide all (life -= max). **0x80CC** → show the last **2** (0x4BA504) again |

### 2.12 WarMapObjectList (0x421340, vtable 0x4A9BA8)
The generic list of war map objects (`+0x20` list, `+0x24` count, `+0x28` owner). The sector object lists
(+0x4C/0x50/0x54/0x58/0x5C/0x60 in each sector) use it.

| Addr | Name | Notes |
|---|---|---|
| 0x4213D0 | dtor | |
| 0x421510 | vtbl[7] broadcast | |
| 0x421570 | `Clear` | |
| 0x421630 / 0x421690 | Activate / Deactivate | Activate shows an object only if it is ours (`+0x24 == 0`), it's a network game, or it's **revealed** (`+0x3C != 0`). This is the war map fog of war. |
| 0x4216D0 | `Count` | |
| 0x4216E0 | `RefreshFrames` | 0x429FE0, frame by side, 0x429FF0 |
| 0x421740 / 0x421790 | Count / sum of 0x8116 answers | Used by the debriefing 0x43A800 |
| 0x421B20 / 0x421B80 | Load / Save | `'WMOB'` (0x424F4D57), count, then (type, object) via factory 0x446950 |
| 0x421C10 | `FindByType(t)` | |
| 0x421C60 | Σ `0x45FE30(arg)` over objects passing 0x42A410 | Production capacity? (unclear) |
| 0x421CC0 | `CountType(t)` | |
| 0x421D00 | `SumProduction(k)` | Σ `obj+0x80` where `0x423920()==k` |
| 0x421D70 / 0x421E50 | `PrevById(n)` / `NextById(n)` | Cycle through objects by id `+8` (wraps). Used by 0x43BE50/0x43BF30. |
| 0x421F30 | `FirstIsOurs` | |
| 0x421F50 / 0x421F90 | Per object `+0x4C` → 0x47D4E0 / Σ counts | |

### 2.13 Battle map deployment (0x421FE0, 0x4222C0); called from the battle map setup 0x439770
- **0x421FE0(arg)**: finds the defenders' HQ position in the BattleMapObjectList world+0x118. The HQ type is
  **830** (landing site) in sector 3, **120** (Communications Centre) elsewhere for the Martians, and **278**
  (London) / **58** (Command Post) for the Humans; its position is `+0x78,+0x7C`. It also finds the position of
  type **179** in world+0x11C (entry point? unclear). Then for each war map object group it picks a deployment
  centre: our own HQ when defending, else the edge facing the group's previous sector (`0x436050(prevSector,
  0x42A020(unitClass))`), else the map centre (landscape `dword_59D940`: `+0x1C/2`, `(+0x28*+0x30>>16)/2`).
  The player's own centre is stored in `0x4F742C/0x4F7434` (camera start, guess).
- **0x4222C0(arg, group, x, y)**: places a group's battle units in **rings** of radius step
  `sqrt(n)-0.5 × unitSize(0x51A136) × 0.7075`, angle step 2π/k. It checks land (`0x47EB00`+`0x480740` for
  ground units, `0x47B610` for others) and occupancy (`0x47FE10`), the map edge margin `dword_59D93C`, and fixed
  positions (`+0x188` bit 13 → `+0xF4/+0xF8`). Each unit gets its position (vtbl+0x40), a random heading
  `rand()&0xFFF` (vtbl+0x30), is activated, and is sent 0x80C3 (rally to the centre).

## 3. Data structures and globals

### 3.1 WWarMapWorld (0x134 bytes)
`+0x44` paused, `+0x48` pod hour accumulator, `+0x4C` pods landed (max 8), `+0x50/+0x54` saved view centre,
`+0x58` SGroupList (selections), `+0x5C` hover sector/target, `+0x60` selected sector, `+0x64` auto-advance flag,
`+0x68` modal counter (?), `+0x6C` message history open, `+0x70` view mode (1 units, 2 constructions,
3 resources), `+0x74` clunting grid, `+0x78` highlight (0x7E), `+0x7C` map object (0x7D), `+0x80` WarMap view,
`+0x84` miniMap view, `+0x88` miniMap frame, `+0x8C` hover route list, `+0x90` active routes,
`+0x94..+0x10C` sectors[31] (0 = sea), `+0x110` popup messages, `+0x114` drag flag, `+0x118/+0x11C/+0x120`
BattleMapObjectLists, `+0x124` zoomer, `+0x128` save slot, `+0x12C` ambient sound, `+0x130` music allowed.

### 3.2 Sector object fields used here (class at 0x439480, 0x12C bytes; not in this piece)
`+0x48` index, `+0x4C/+0x50/+0x54/+0x58/+0x5C/+0x60` six WarMapObjectLists (HUNI/AUNI/HCON/ACON/HRES/ARES
order to be confirmed: +0x4C/+0x54 are summed as one side and +0x50/+0x58 as the other), `+0x64` pod-landed
flag, `+0x9C[]` (0x43C090), `+0x124` battle-pending flag (pulses), `+0x128` battle-just-fought flag.

### 3.3 Sector shade tables at 0x4E98A0 (5 × 0x8C bytes)
Each is `{ float v[31]; float min; float max; int lo; int hi; }`. Ranges (set in the ctor): table 0 units
17..35, table 1 supply/efficiency 67..77, tables 2-4 resources 78..88 (in the map's 89-level table).

### 3.4 Sector economy table at 0x4EAE1C (0xC0 bytes a sector, sector s at +s*0xC0)
`+0` owner (0/1/2), `+4 int[6]` stock, `+0x1C int[6]` (2nd value used by 0x424000), `+0x34 float[6]`
consumption accumulator, `+0x4C float[6]`, `+0x64 float[6]` production, `+0x7C float[6]` distribution,
`+0x94 float[6]` ratio clamped to 1. Resources 0-2 are Human (Iron, Coal, Oil: texts 0x214-0x216) and 3-5
Martian (Human blood, Copper, Heavy elements: 0x217-0x219).

### 3.5 Other globals
0x4E9B68/6C view centre, 0x4E9B70 world, 0x4E9B74 local player, 0x4E9B78 opponent, 0x4E9B7C battle report,
0x4E9B80 calendar, 0x4E9B84 briefing sound, 0x4E9B88/8C research (own/other), 0x4E9B90 loading,
0x4E9B94 game-over-from-battle, 0x4E9B98 pending battle sector, 0x4E9B9C selection count,
0x4E9B5C/64 deferred command, 0x4E9BA0[3]/0x4E9BB0[3] resource alert/warning counters, 0x4E9BF8[180]
resource gauges, 0x4E9EE0 rubber-band tracker (static, 0x41BFD0; flags 0x4E9EF6/7), 0x4E9F40/44/48 resource
sub-type, 0x4EA064/68 event queue, 0x4EA0A0 dialog up, 0x4EA0B0 game date, 0x4CA998 time scale
(1.0, or 0.25 in the menu), 0x4D2530 world shutting down, 0x4D2550 = world (for 0x439230),
0x4D2554 entering battle, `dword_4F7654[30]` unit strength per sector, `byte_4D1249/4D1261/4D1279` cheat flags
(set by the cheat key parser 0x401320; 4D1261 suppresses event dialogs, 4D1249/4D1279 allow selecting enemy
sectors), `byte_58913C` cheat (economy ratio = 1).

### 3.6 Save blocks written here
`WMAP` (0x41AD80), `CALN` + `DtTm` (0x41F760/0x41B930), `MPAD` popup messages (0x420FD0), route lists
(0x41FEC0), `WMOB` object lists (0x421B80). Order in WMAP: view x, view y, sector, mode, CALN, research ×2,
MPAD, routes, `+0x48`, `+0x4C`, 31 sectors.

## 4. Messages, commands, strings, tuning

**Settings** (section, name, default, storage):
- `[Tweak] "Pod Interval (hours)"` 24 (0x4E989C)
- `[Tweak] "Quick Cancel Timeout"` 5 ticks (0x4E9F24)
- `[Tweak] "Warmap Zoomer Speed"` 1.5f (0x4E9F3C)
- `[Tweak] "WM clunting res"` 36 (0x4EA0D4)
- `[Tweak] "WM clunting adj"` 4 (0x4EA0EC)
- `[Screen] "Scroll Speed"` 40 (0x4BA058)
- `[Screen] "Scroll Border"` 4 (0x4BA05C, also used by the calendar and miniMap placement 0x422EE1/0x426F22)
- `[Debug] "Log Debug Output"` 0
- the 11 `[Event]` toggles in §2.5

**Constants**: speeds 0..10 (0x4BA470); Game Frequency 30 (0x4B84C8); pods max 8; resource alert
< 0.3 for > 336 calls, warning < 0.8 for > 504 calls; intel reveal 336 h; message list 10 lines × 150 ticks,
16 px; show-last 2; start date 1 Sep 1898; CD tracks 2, 3; ambient sfx 0xFD-0xFF.

**Sounds (SFX ids)**: 0x120 arrived, 0x12F construction, 0x131/0x1C0 unit, 0x135 research alert, 0x138/0x139/0x13A
battle, 0x13C/0x13D/0x13F/0x140 battle results, 0x13E victory, 0x147 travel interrupted, 0x18E briefing,
0x193/0x196 message list open/close, 0x198 critical dialog, 0x1A1-0x1AB Martian clock, 0x1A2 Human clock,
0x1B2/0x1B3 pods, 0x122-0x12C resources.

**Text ids**: 0x002+n sector names, 0x210, 0x211, 0x214-0x219, 0x415-0x421, 0x45D-0x460, 0x465, 0x46B, 0x46C,
0x476, 0x49E, 0x49F, 0x4AF-0x4B4, 0x521.. months, 0x533.

**Private messages** handled or sent: 0x8004, 0x8006, 0x800A, 0x800F, 0x8010, 0x8011, 0x8017, 0x8019, 0x802C,
0x802D, 0x8028, 0x8034, 0x803B, 0x803C, 0x8050/51/54/55, 0x8052, 0x805B, 0x805C, 0x8072, 0x8073, 0x8074,
0x808D, 0x808F, 0x80A8, 0x80B3-0x80B6, 0x80BA, 0x80C3, 0x80CB, 0x80CC, 0x80FA-0x80FE, 0x8107, 0x8111, 0x8113,
0x8116, 0x8117, 0x811F, 0x8122, 0x8123, 0x8133, 0x813C, 0x813E, 0x8143, 0x8147. Plain commands: 0x6C-0x73,
0x76, 0x7A, 0x7D, 0x92-0x99, 0xAD, 0xB7, 0xBD, 0x3ED, 0x3EF.

## 5. Cross-references to other pieces

- **Called from outside:**
  - WGame 0x4048F0 → 0x417CD0
  - 0x415410 → 0x417C30 (log)
  - Sector code 0x43A250/0x43CEB0/0x43D4F0 → 0x4188D0, 0x4192A0, 0x4192F0, 0x4205F0
  - Battle map setup 0x439770 → 0x41B360, 0x4216E0, 0x421C10, 0x421F90, 0x421FE0
  - Debrief 0x43A800 → 0x4212F0, 0x421740, 0x421790
  - 0x43CB20/0x43CB60 → 0x421A50, 0x421AA0, 0x4203C0
  - 0x43C7F0 → 0x421C60, 0x421CC0
  - 0x43BE50/0x43BF30 → 0x421D70, 0x421E50
  - Routes 0x424160 → 0x41B9A0, 0x421F30; 0x424F50 → 0x41FBD0; 0x425080 → 0x4208B0; 0x427C80 → 0x420540
  - Route cost 0x42ABD0 → 0x420060
  - Calendar view 0x4278A0 → 0x41AEA0 (any sector vetoes the speed change); 0x427500 → 0x41B3F0
  - Briefing 0x4999E0 → 0x41B440
  - Research 0x45B6D0 → 0x41BDA0
  - Init 0x402630 → 0x41BF90
  - MiniMap 0x422C70 → 0x4191F0/0x419200
  - 0x423B00 → 0x418890
- **Called from here** (next pieces): miniMap 0x422C70 and frame 0x423420; map object 0x4238D0; WarMapRoute
  0x4240D0/0x424160/0x425570/0x4255F0/0x4256E0/0x425770; Calendar View 0x426C10; route arrows 0x427C80/0x427E10;
  grid 0x428110/0x4283F0/0x428620/0x4286C0; Message Item 0x428810/0x4289D0/0x428A00; counters
  0x429FE0/0x42A020; Win95DeskTop 0x42DB00/0x42C6B0/0x42C990; widgets 0x42E0E0-0x42F6F0, InfoBox 0x432520,
  popup menu 0x432C60/0x432D70/0x432FB0; sectors 0x439480, 0x43C090, 0x43C8D0, 0x43C990, 0x43CA30, 0x43D010;
  route data 0x437380, 0x436050; players 0x438B20/0x442890/0x445EA0; research 0x459540/0x45A680/0x458BD0;
  Zoomer 0x446690; sound 0x464AB0/0x464E30/0x464E60/0x464EB0/0x464EE0; CD 0x412940-0x414150; menus 0x4057F0,
  0x4089D0, 0x409460, 0x409560, 0x409DE0, 0x49B980; briefing/debrief 0x499930/0x49A7B0; highlight/RTypeArrow
  0x49BE70-0x49C3E0; landscape 0x47B610/0x47EB00/0x47FE10/0x480740.

## 6. Missing in WoWEngine / candidate tasks (prioritised)

WoWEngine's `src/warmap/WarMap.cpp` (785 lines) draws the map, borders, mini-map panel, calendar and slider. It
moves counters along A* routes with arrows, keeps a 10-line log, and raises only BattleInSector and
ArrivedInSector. It has no world object model, events system, economy, pods, menus beyond Escape, or
save/load of the war map.

**P1: core gameplay loop**
1. **Hourly step and economy.** Port 0x41F6C0 (clock with the `+0x30` forced hours), 0x802D fan-out, the
   economy phases 0x41A760 (round-robin `tick%5`) and the sector table 0x4EAE1C. Needs the sector and
   construction code (0x43Cxxx) too.
2. **Battle detection and flow.** 0x41AB00: both sides in a sector → BattleInSector / UndefendedSectorLost
   (0x43C8D0, 0x43A250) / PodLanded. Then 0x8072 → 0x8133 (0x41B210: briefing sound 0x18E, CD check / Insert CD
   menu 0x4089D0) → 0x8019 briefing (0x499930) → 0x71 enter sector (battle map) → 0x97/0x99/0x8017 debrief
   and return. Engine today stops the counter at the border.
3. **Critical-event dialogs.** InfoBox 0x432520 via 0x41E4E0 (pause auto-advance, zoom to the sector with
   Zoomer speed 1.5, sound 0x198), the one-at-a-time queue (0x41E5B0/0x41E730, cmd 0x8122), all 20 event
   handlers (0x41E850-0x41F4B0) with their texts and sounds, and the 11 `[Event]` toggles plus the Event Menu
   (0x409DE0).
4. **Martian pods.** 0x41A330: interval 24 h, max 8, sectors {2,4,6,7,8,9,10,11}, contents tables
   0x4B9DF0-0x4B9E18, unit spawn 0x43CEB0.
5. **Resource warnings and alerts.** 0x4198C0 at 0x419EFB: thresholds 0.3/0.8, counters 336/504, events 13/14
   with per-resource sounds.
6. **End of game.** Victory/defeat (events 18/19, 0x41AED0, 0x4212A0): codes 0x80FA-0x80FD sent as 0x80FE to
   WGame; `byte_4D2530` shutdown.
7. **War map save/load.** WMAP, CALN/DtTm, MPAD, route lists, WMOB, sectors (0x41AC40/0x41AD80 order). The engine
   reads parts of the save but cannot write it. Also rebuild routes from units' orders after loading (0x4217E0).

**P2: controls and UI faithful to the original**
8. **Keyboard.** Replace the provisional +/- and arrow scrolling with the original keys:
   - Digits 0-9 set the speed ('0' = 10), unless a sector vetoes (0x43D010).
   - F1-F5 pick the views (F3/F4/F5 = resource sub-types 0/1/2).
   - Space cycles the views, Tab hides the mini-map panel.
   - Home centres on the selected sector (animated zoomer) or the selected object.
   - PgUp/PgDn go to the top/bottom; arrows step ±w/7 / ±h/16 (Ctrl ±w/3 / ±h/8) per key press.
   - M opens the message history, R the research screen, Esc the war map menu (0x80B5: pause, time scale
     0.25), Backspace clears auto-advance.

   Addresses 0x41A450, 0x41C5E0, 0x41D640.
9. **Mouse.**
   - Rubber-band selection (tracker 0x4E9EE0, sectors 0x8010/0x8011) and left-click sector selection (own
     sectors only), 0x41D300.
   - Quick right-click cancel (≤ 5 ticks, cmd 0x73).
   - Right-hold context menu "Warmap menu" with "Cancel" plus items from the object or sector (0x41D120,
     0x805C).
   - Hover route preview for the selection, with arrows and cursor 2 for a valid move (0x41D470/0x41D590/0x419520).
10. **View shading.**
    - Own sectors shaded by unit strength or resource value, normalised to [lo,hi] (0x4192F0, 0x41D9B0, tables
      §3.3); Construction view max 1.0, Resource view max 80.
    - Enemy sectors level 0 in Unit view.
    - Battle sectors pulse 36-47 / 47-58 (0x41DE70).
    - Resource view with 3 sub-resources (`0x4E9F40`) and the per-sector gauges (0x426140/0x426840).
11. **Message list behaviour.** Scroll-up animation on expiry (0x420CD0). 0x6C shows the history (sound 0x193),
    0x6D hides it (0x196), 0x80CC re-shows the last 2. Messages are saved (MPAD). "Game Loaded" (0x465) after a
    load.
12. **Calendar text.** Month names from TEXT 0x521+ (the engine hard-codes English, WarMapData.h:322). The Human
    date format of 0x41B440 and the duration strings of 0x41BDA0 (day/days/hr/hrs) for research and briefings.
    Start date 1 Sep 1898.
13. **Clock and music sounds.**
    - Human clock 0x1A2 when stopped; Martian 0x1A1+speed (0x41F860).
    - War map music: CD tracks 2/3 alternating, or random ambient 0xFD-0xFF every 10 s (0x41B070, cmd 0x813C).
14. **Menus from the war map** (0x41CD50): Load Game 0x6F, Sound Control 0x3ED, Event Menu 0x3EF/0x8117, Quit
    0x80B6, Return to Main Menu 0x90/0x70 → 0x80B4. Pausing (`+0x44`) and time scale 0x4CA998.

**P3: correctness details**
15. **Counter placement.** When a sector is full, shrink the cells by "WM clunting adj" (4) from "WM clunting
    res" (36) and slide the counters over 5 ticks (0x4205F0, 0x4208B0, 0x4209E0); spiral mode 3 for units and
    constructions, 2 for resources.
16. **War map fog of war.** Enemy counters are shown only when revealed (`obj+0x3C`, 336 h after a battle; 1-3
    neighbours by research level), 0x421630 and 0x418980 (0x418E17..). In a network game everything shows.
17. **Arrival sync.** Routes into the same sector arrive together (0x41FCF0; verify), and 0x802C fires when no
    routes remain.
18. **Players.** Local / AI / network player objects and the command recording `{01,01,code,04}` (0x4460A0) for
    playback and network (codes 2 select, 4 band-select, 0xA/0xB auto-advance).
19. **Battle deployment.** 0x421FE0/0x4222C0 (ring placement, HQ and entry-edge centres, random headings, 0x80C3
    rally); needed when the battle map is built.
20. **Debug log.** `[Debug] "Log Debug Output"` file logging (0x417C30). Optional.

Open questions: what world `+0x64` (auto-advance) changes in the opponent (vtbl+0x30); the meaning of the
0x8050-0x8055 counter `+0x68`; the exact formulas of economy phases 1/3/4; what object type 179 is; and the
battle report counters at +0x10..+0x4C.
