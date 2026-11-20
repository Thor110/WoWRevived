# Part 11: 0x47F480–0x48A4C0 (battle map: object base, unit groups, battle map screen, effects)

Source: `pieces/part11.txt` (220 functions). Everything here is compiled MSVC C++ (SEH frames, `__ftol`,
`??2@YAPAXI@Z`). The `imul` by 0x2C3E63FB / 0xB4B10D5D / 0x55555556 / 0x92492493 sequences are compiled
divisions, not hand-written code. **No hand-written assembly was found in this piece.**

Notation: "obj+0x78" is a field offset; "unit table +0x52" is a field of the per-type table at 0x51A0F0
(0xB0 bytes per type, indexed by obj+0x20). `vcall +0x1C` is the virtual message handler
`OnMessage(id, p1, p2)` that every WCmdTarget-derived object has. "Packet type N" means a command built with
0x4493E0 / 0x4A4440 / 0x4460A0 and sent with 0x4A4470 / 0x4460C0 through the local player object
(dword_4E9B74 = Human player, dword_4E9B78 = Martian player). These packets are the network/playback command
stream (DeePlay, `playback.%d`). Guesses are marked *(guess)*.

---

## 1. Overview

The piece runs from the end of WLandscape (0x47AF10–0x47F470, part 10) into the vehicle classes (0x48A4C0+,
part 12). In address order it holds:

1. **0x47F480–0x480100: WObject base virtuals.** These are battle-map world objects, class vtable 0x4AD620 (ctor
   0x47D6F0 in part 10). They cover position and terrain height, map clamping, parent/hardpoint attachment,
   heading, the selected flag, the 4-layer spatial "Collisions" query, nearest-target search, the shadow/shade
   factor, and minimap dot plotting. Almost every battle-map class vtable (0x4AD650–0x4B0E9C) points here.
2. **0x480150–0x483B70: WBMGroup**, the battle-map unit group or selection (vtable 0x4AD708, ctor 0x480190).
   Each group recalculates its members' movement stats and terrain-cost tables and executes commands that
   arrive as packets: move, attack, force fire, stop, stance, retreat, and the special abilities (Black Dust,
   Freeze, Tunnel and so on). It drives the route finder and plays unit speech ("ROUTE FOUND", "ROUTE NOT
   FOUND", "GOING STRAIGHT THERE!"). Per tick it runs the AI behaviour for stances.
3. **0x483DF0–0x4849D0: battle UI widgets.** One is a WCmdTarget icon tray that lays out icons in rows. The
   other is a unit icon button (vtable 0x4AD788), clicked to select and double-clicked to centre the view.
4. **0x484A30–0x4861E0: WBattleMap screen** ("BattleMap", window id 0x8018, vtable 0x4AD7C8). It handles
   keyboard scrolling and the group hotkeys, edge scrolling, the mouse (box select, move, force fire, build
   placement), cursor modes, the Escape menu (0x80B8), the right-click "Battlemap menu", sound ducking and the
   battle music alternation.
5. **0x4862C0–0x486850: two-point structure.** A wall-like structure with start and end cells (vtable
   0x4AD810). It registers with the route finder and builds up its segments as construction progresses.
6. **0x4868B0–0x487380: "BattleMap miniMap view"** (id 0x6B, vtable 0x4AD928). It handles minimap rendering,
   click-to-scroll, the viewport rectangle and the "1 renderer"/"2 renderers" toggle.
7. **0x4873F0–0x487580: retreat points** on the map edge (vtable 0x4AD968), with the tooltip "Retreat to
   <sector>".
8. **0x487610–0x487A50: WTransientObject and WEffectObject** pool allocators (512 per block) and their base
   classes.
9. **0x487B00–0x489050: weapon effects.** This covers area-effect blasts (freeze, scare, control, shock),
   explosions with splash damage, projectiles with ground shadows, cluster munitions, gas/smoke clouds,
   falling debris and sprite animation effects, plus the spawn helpers.
10. **0x4890A0–0x4897D0: scenery.** Ambient-sound scenery, the "Scenery objects left" counter, and random
    alternate variants.
11. **0x489820–0x489D80: turret sub-object** (vtable 0x4AE6B0) and the shared status-effect getters.
12. **0x489DB0–0x489FA0: the BAMO class.** This is the persistent battle-map object base (vtable 0x4AE938):
    save/load of the "BAMO" chunk, plus the generic message handler (damage, tooltip, destroy, abandon).

WoWEngine has **no battle map code at all** (only `Models.h` Terrain/WOF/IOB loaders and the SaveGame chunk
names), so everything below is new work.

---

## 2. Subsystems

### 2.1 WObject base (battle-map world object), 0x47F480–0x480100

Base vtable 0x4AD620 (ctor 0x47D6F0, part 10). The functions are listed by the vtable slots they fill.

| Addr | Proposed name | What it does |
|---|---|---|
| 0x47F480 | `WObject::UpdateScreenPos` | Projects x/z/height (obj+0x78/+0x80/+0x7C) to screen coordinates. The projection is the isometric fixed-point transform, using landscape+0x48 and unit table +0x41 (byte height offset). It writes them into the puppet/sprite (obj+0x4C: +0x1A,+0x1C,+0x1E) and into the model's +0x38..+0x3C, then calls vcall +0x20(obj+0xAC). Also calls 0x4406A0 (grid update). |
| 0x47F570 | `WObject::ClipTest(view)` | Compares the puppet's screen rectangle (+0x1A/+0x1C + extents +0x28..+0x2E) with the view rectangle (view+0xB4..+0xC0) and calls vcall +0xA4(offscreen). |
| 0x47F630 | `WObject::Draw(renderer)` | Calls vcall +0x60. Marks the type as seen (unit table +0xAC = 1). If flag bit 9 is set, copies the frame obj+0xA8 into the model. Adds the puppet to the renderer (vcall +0x18), then recurses into the children list obj+0x70 (vcall +0x4C). |
| 0x47F6F0 | `MarkDirtyRecursive` | Sets puppet flag 0x10 on itself and all children. |
| 0x47F740 | `SetOffset(x,y,z)` | Writes obj+0x88/+0x8C/+0x90. |
| 0x47F770 | `UpdateWithOffset` | If flag bit 10 is clear, sets the position to the terrain height (0x47B170) plus the offset; otherwise to the raw offset. Adds the vertical speed obj+0x94 to obj+0x90 (gravity/fall). |
| 0x47F850 | `AttachToHardpoint` | Reads hardpoint obj+0xB0 (word index, word sub-index) from the parent's model (+0x48→+8, 12-byte vec3s) and calls SetLocalOffset (vcall +0x38). |
| 0x47F8D0 | `SetLocalOffset(x,y,z)` | Writes obj+0x98/+0x9C/+0xA0. |
| 0x47F900 | `SetPosPacked(xy)` | Unpacks a packed short x/y and calls vcall +0x40. |
| 0x47F940 | forwarder | Calls vcall +0x7C. |
| 0x47F960 | `SetPosition(x,y,h)` | If there is a parent, position = parent + local offset. Clamps to [margin, landscape+0x1C - margin] (margin obj+0xAC) and to the map depth (landscape+0x28 × +0x30 >> 16). Sets the height to terrain + h, calls vcall +0x34 (moved), and moves the children (vcall +0x7C). |
| 0x47FAC0 | `SetHeading(a)` | Sets obj+0x40 = a & 0xFFF (4096 units a turn) and recurses through vcall +0x30. |
| 0x47FB20 | `SetSelected(b)` | Sets flag bit 6 (0x40) on obj+0xB8 recursively. On deselect, notifies group+0x54 (vcall +0x18). |
| 0x47FBA0 | `CollectInRadius` | Walks one grid bucket and adds objects within radius (distance 0x43FF00 ≤ obj+0xAC + r) to a list (0x47D190). Skips kind 4 (unit table +0x10 == 4) and itself. |
| 0x47FC50 | `QueryCollisions(x,y,r)` | Makes a new list named **"Collisions"** (0x47CD30) and calls 0x47FBA0 for layers 0–3 of the grid (0x449920 on dword_4D2550+0x118). Returns the list. |
| 0x47FD40 | `NearestInBucket` | Nearest object (Manhattan-ish distance, start value 10000) that passes vcall +0xBC (e.g. "is a valid target"), within its radius plus r. |
| 0x47FE10 | `FindNearestTarget(x,y,r)` | Calls 0x47FD40 over layers 0–3 and returns the first hit. Called by 0x4222C0 and 0x492350 (vehicle AI). |
| 0x47FEB0 | `GetShadeFactor` | If there is a parent, returns the parent's. Otherwise samples the per-cell shadow byte map (dword_4D2550+0x84: width +0x84, data +0x18) and returns 1.0, or 1 − (1 − v/thr)·k with thr = map+0x9C and k = map+0x90. |
| 0x47FFB0 | returns 10 | A class id or priority *(unclear)*. |
| 0x47FFC0 | `SetFlag3(b)` | Sets obj+0xB8 bit 3. |
| 0x47FFE0 | `IsOnLand` | True if the landscape cell's terrain nibble is 1–3 (0 = water, ≥4 = other). |
| 0x480070 | `PlotOnMinimap(minimap)` | Sends **0x8077**(colour, packed x\|y<<16) to the minimap. Colour: dword_4DAFA0 if kind 1 (unit), dword_4DAFA4 otherwise, dword_4DAFA8 if obj+0x24 (AI/enemy) is set. |
| 0x4800E0 | `GetRoot` | Follows obj+0x44 (parent) to the top. |
| 0x480100 | `SetGroupRecursive(g)` | Sets obj+0x48 = g on itself and its children. |

### 2.2 WBMGroup: unit groups, orders, routing, stance AI (0x480150–0x483B70)

`WBMGroup` derives from 0x447DC0 (a WCmdTarget with a member list at +0x20/+0x24). Its vtable is 0x4AD708, ctor
0x480190 and dtor 0x4802B0. Groups are created by "BattleMapSelection" 0x43A600, and the stats are refreshed by
0x47CFE0 when the selection changes. The live count is in dword_59D9A4, and there is a stats counter "BM Groups"
(0x480150 registers it with 0x412160 under "Stats").

| Addr | Proposed name | What it does |
|---|---|---|
| 0x480150 | static init | Registers the "BM Groups" stats counter (unk_59D990). |
| 0x480190 | `WBMGroup::WBMGroup(owner)` | Clears its fields; +0x54 stance = 1, +0x5C = 0x5A. Sets +0x58 = rand()%30 (AI think phase), registers in dword_4D2550+0x20 and increments dword_59D9A4. |
| 0x4802B0 / 0x480290 | dtor / scalar-deleting dtor | |
| 0x4802D0 | thunk | To 0x4490E0. |
| 0x4802E0 | `RecalcStats` (msg 0x800F) | Over the members: +0x4C = min of the per-category max slope (dword_4F04C0[cat], start 1000) and +0x4E = min speed (unit table +0x48, start 100000; also member vcall +0xB0). +0x50 = max attack range (unit table +0x44), with +0x52 the category giving the best flt_4F0510. +0x70[16] = max terrain cost per terrain type from unk_4F6E38[cat*17]. +0x5A = average dword_4F6E78[cat*17] (−1 = n/a). +0x1D0 = average war-map speed (unit table +0x54). +0x5C = average unit table +0x92. +0x54 = average stance from members' +0xD8. +0xB4[8][8] = pow(flt_4F0510[cat], slope) slope penalties (__CIpow). Flags (+0x1D8) are listed in §3. |
| 0x480680 | `CellCost(x,y,strict)` | Landscape cell (10 bytes; +8 = terrain nibble \| (slope−1)<<4, +9 = flags). Returns 0 if the terrain cost is −1, the slope > +0x4C, or blocked by flags (bit1 always, bit2 if strict, bit3 if the group is in its own sector) unless the group is airborne (bit5). Otherwise cost = slopeTable[+0x52][slope] × terrainCost. |
| 0x480740 | `RectPassable(x0,y0,x1,y1)` | All cells in the rectangle have nonzero 0x480680 cost. |
| 0x4807E0 | `SetMode(m)` | Writes +0x1DC. |
| 0x4807F0 | `WBMGroup::OnMessage` | Message switch (§4). WM_COMMAND (0x111) from the order buttons is turned into packet type 0x15 to the Human player (0x80CE stance / 0x80F3 ability) or to the Martian player (0x8118/0x8119). The default goes to 0x4482D0 (base). |
| 0x480EB0 | `SetMoving(b)` | Sets +0x1D8 bit0. On clear, also clears bit2; on stop, resets the arrival counter +0x56. |
| 0x480EF0 | `Retreat` (0x80CE with param 0x80B7) | Finds the nearest retreat point of this side and category (0x43D3F0). If there is none, or the group is mixed (bit8 and bit9), plays speech **0x121** ("can't") and flashes the leader portrait (0x4842D0: portrait+0x94 = 30 ticks). Otherwise stops, sets bit3 (player move), sends 0x804F(point) to itself and 0x80BB(point idx) to every member's group object. |
| 0x481170 | `ExitVia(obj)` | Same as above, but towards a given exit object (its +0xCC index). |
| 0x4812B0 | `ExecutePacket(pkt)` | Ignored while 0x498D30 returns true. Switches on pkt[2]: **0x0E** move/ability at a point (0x481CA0); **0x10** target object (force fire if mode 1, else attack 0x481F70); **0x11** halt (centroid, members get 0x8031); **0x13** force fire at the ground (speech 0x10C, sets the target point +0x40/+0x44/+0x48 with height from 0x47B170); **0x15** order (`0x4817C0(pkt+4, pkt+8)`). |
| 0x481770 | `Speak(id, voice)` | 0x464EE0(id, dword_4E9250, dy, 1.0, voice) and flashes the leader portrait. |
| 0x4817C0 | `ApplyOrder(cmd, param)` | 0x80CE: param 0x8067 sends Abandon (0x8067) to the members, 0x80B7 Retreat (0x480EF0), anything else sends stance 0x80B2(param) to itself. 0x80E1/2/3 send Pulse levels 1/2/3 to the members immediately; 0x80F0 sends Scan. 0x80ED/EE/EF set mode +0x1DC = 1 (Force Fire) / 2 (Tunnel: also stops and halts) / 3 (Submerge). 0x80D5–0x80DD → mode 4–0xC (Black Dust, Shock Bomb, Explosives ×3 levels), 0x80DE–0x80E0 → 0xD–0xF (Infiltrate), 0x80E4–0x80EC → 0x10–0x18 (Freeze, Scare, Control). Mode N waits for the next target or point click. |
| 0x481CA0 | `OrderAtPoint(pkt)` | Mode 1: force fire at the point (speech 0x10C). Mode 2: if no member has +0x18C set, sends 0x80EE (tunnel) to all. Mode 3: sends 0x80EF (submerge) to all. Then stops, sets bit3 and moves (0x482510). |
| 0x481F70 | `AttackObject(obj)` | If the target is type 0x3DB (an exit/transport *(guess)*), checks its sector link (0x419240, 0x43AC40) and exits via it (0x481170), or plays speech 0x121. Otherwise members get 0x80B9, the target is set (0x482880), the group moves, and speech 0x10C plays. |
| 0x4821A0 | `Broadcast(msg,a,b)` | Sends vcall +0x1C to every member. |
| 0x4821F0 | `OnTargetGone` (0x8058) | Called when the current target (+0x30) is destroyed or changes class (kind 2/3). Plays speech 0x10E (target destroyed) or 0x10D, stops, and sends 0x805A and 0x8086 to the members. |
| 0x482450 | `MemberArrived` | Increments +0x56; when it reaches the member count, clears moving. |
| 0x4824A0 | `Disband` | Members get 0x80A0, then the group deletes itself (vcall +0, 1). |
| 0x482510 | `MoveTo(x,y)` | Sets moving and sends 0x804F(x\|y<<16) to itself. |
| 0x482570 | `AI_RandomAbility` | rand()%27 picks one of 0x80F0, 0x80D5–0x80DD or 0x80E1–0x80EC. If the leader can do it (0x47DA80), applies it with 0x4817C0. |
| 0x4826D0 | `AI_ChaseTarget` | If not a player group and +0x58 is due, calls 0x482570. If the target (+0x2C) has height ≥ 0 and moved more than 80 units from its last position (+0x34..+0x3C), re-sends 0x804F. Calls the target's vcall +0xC8/+0xC4 (engage). |
| 0x482880 | `SetAttackTarget(obj)` | Members get 0x80B9; sets +0x2C = obj, +0x30 = 0, +0x34..+0x3C = −1; members get 0x805A; clears moving. |
| 0x482960 | `Retaliate(attackerId)` | Same as above, against the attacker (0x440110 maps an id to an object). |
| 0x482B00 | `NearestPassableCell(x,y)` | Spirals over 8 neighbour directions out to distance 200 (0xC8), using 0x480680 with strict=1. |
| 0x482E60 | `OrderMove(xy)` (0x804F) | If the route finder (dword_59E418) is busy with another group, re-posts 0x804F after 10 ticks (0x4355A0). Otherwise finds the nearest and farthest members (+0x68/+0x6C) and converts world to cells (landscape +0x30 cell size 16.16, +0x34 bias). Within 3 cells logs "ALREADY AT DESTINATION". Snaps both ends with 0x482B00. When attacking and the reachable distance is greater than the range +0x4E, logs "CAN'T GET TO WITHIN ATTACK DISTANCE!" → 0x4833F0. If the leader is a ground unit (vcall +0x70) and the route is not trivially clear, logs "SEARCHING FOR ROUTE WITH DFT %d" and starts an A* search (0x494440, limit 200). Otherwise logs "GOING STRAIGHT THERE!": members get **0x80C2**(packed dest) and the marker spawns (0x483590). |
| 0x483270 | `OnRouteFound` | Takes the route from dword_59E418 (0x494690), stores it at +0x1D4, and members get **0x802E**(distance × speed, route). Spawns the destination marker and logs "ROUTE FOUND". If dword_59E41C (pending) is set, calls 0x4833F0. |
| 0x4833F0 | `OnRouteNotFound` | Releases the route finder, members get 0x8031, stops, logs "ROUTE NOT FOUND", plays speech **0x10B** and tells the Martian player object (vcall +0x1C(0x4E,2)) *(AI hook, guess)*. |
| 0x483590 | `SpawnDestinationMarker(cx,cy)` | Only for player moves (bit3 and bit4). Creates object type **0x17C** (flag) at the cell centre, adds it to the world (dword_4D2550+0x90) and plays speech **0x10A** (acknowledge). |
| 0x4836A0 | `Bounds(rect*)` | Min/max of the members' x/z. |
| 0x483760 | `ValidateTarget` | If the target id no longer resolves, stops; otherwise re-engages through vcalls +0xC8/+0xC4 and member vcall +0xA8. |
| 0x483880 | `OnEnemySpotted(obj)` (0x8085) | Depending on stance: 3 (Roam & Attack) attacks; otherwise wanders (rand offset of 400.0 via 0x43FE90) and sends 0x804F. Members are told 0x805A. |
| 0x483AE0 | `RecalcCentre` | +0x60/+0x64 = the members' centroid. |
| 0x483B70 | `WBMGroup::Tick` | Polls the route finder (0x483270), runs 0x4826D0 when targeting, and the think counter +0x58/+0x5C. Stance 3 (Roam) when idle: random destination anywhere on the map (rand × landscape size), then 0x482510. Stance 1/2: if the group drifted more than 400 (0x190) from home (+0x60/+0x64), walks back (0x804F). |

### 2.3 Icon tray and unit icon button (0x483DF0–0x4849D0)

| Addr | Proposed name | What it does |
|---|---|---|
| 0x483DF0 | `WIconTray::WIconTray` | Named "WCmdTarget", vtable 0x4AD760, with a list at +0x20/+0x24. Created by battle map set-up 0x439770. |
| 0x483E80 | dtor | |
| 0x483F90 | `Add(btn)` | Appends a button and re-lays out the tray. Called from 0x422910. |
| 0x483FF0 | `Remove(btn)` | Called from 0x428D70 and 0x42A0B0. |
| 0x484070 | `Layout` | Places the buttons left to right and wraps by width. Margin is 32 px if byte dword_4EA364 is set, else dword_4BA05C (4). Uses 0x42EDC0 (move widget). |
| 0x484120 | `WUnitIcon::WUnitIcon(..., obj)` | Base 0x4A4290 (widget id 0x76), vtable 0x4AD788. Holds the object (+0xB0), its counter gfx (unit table +0x2C into +0x90) and a preview at +0xAC (0x449150). |
| 0x484240 | dtor | |
| 0x4842D0 | `FlashPortrait` | Sets +0x94 = 30 (blinks for 30 ticks while the unit speaks). |
| 0x4842E0 | `Draw` | Corner sprites (+0x9C..+0xA8), blink counter, frame from +0x90 (0x40EC30/0x40F000), preview 0x449340. |
| 0x4844A0 | `OnMessage` | 0x20 (WM_SETCURSOR) sets the cursor (0xC, or 5/1 by owner). 0x201/0x202 click: a double click (≤10 ticks, dword_4D2558) centres the view (0x43A480). A click selects: packet type 4 via the Human player, then **0x8088** to the object. 0x8111 adds the same. 0x80C1 rebuilds the preview. 0x80C8 shows/hides. 0x807C (tooltip) gives the object's name (0x440720). |
| 0x484840 | `MakeCorners` | Builds four 0x44-byte sprites with frame 5. |
| 0x484920 | `OnClickSelect` | Double-click and select logic (above). |
| 0x4849D0 | `GetTooltipRect` | InflateRect by 10, OffsetRect, then 0x40AED0. |

### 2.4 WBattleMap screen (0x484A30–0x4861E0)

| Addr | Proposed name | What it does |
|---|---|---|
| 0x484A30 | `WBattleMap::WBattleMap(rect, world, mapNo)` | Window class 0x8018 "BattleMap", style 0x10200000, full screen (dword_4E9250/dy + dword_4EA370/4EA374). Fields: +0x84 world, +0x88 drag rectangle, +0x8C, +0x90 flag, +0x94/+0x98 list. Posts 0x8052 (deferred init). Created by 0x439770. |
| 0x484BF0 | dtor | Stops CD audio (0x412C00, 0x412940), frees the view +0x7C and the rectangle +0x88. |
| 0x484D00 | `CreateView` (0x8052) | Allocates the 3D view renderer +0x7C (0x49F240, 0x17C bytes, flag 0x1000, 64×64 tiles *(guess)*) over the window rectangle using the landscape renderer (world+0x88). Loads **palette group 0x2F0** (`bm.pal`) and scrolls to dword_4E9B68/6C. |
| 0x484DF0 | `EdgeScroll` | If the mouse is within 32 px (dword_4BA05C when not full-screen) of an edge, scrolls by (centre − mouse)/(width/2/dword_4BA058 (40)). Posts WM_COMMAND **0x7A**(x\|y<<16) (scroll-to). Skipped while another window has focus (dword_4EA444). |
| 0x484F00 | `Paint` | View: begin (0x40E630), clear, draw the three object lists (dword_4E9B70+0x118/+0x11C/+0x120, via 0x47D430), the drag rectangle (+0x88), end, blit (0x42E220). Sets +0x80 (first frame drawn). |
| 0x484FB0 | `WBattleMap::OnMessage` | See §4.1. |
| 0x4855F0 | `DeleteDragRect` | |
| 0x485610 | `OnKeyDown(vk)` | **0–9**: packet type 0x12 (select group n; 0 → 10). **Backspace**: packet 0x11 (halt). **Esc**: WM_COMMAND 0x80B8 (battle menu). **Tab**: posts 0x80C8 (toggle minimap) and calls 0x40AEB0. **Home**: 0x43A480 (centre on selection *(guess)*). **PgUp**: scroll to the top. **PgDn**: scroll to the bottom (map height). **←/→**: scroll by width/7 (Ctrl: width/3). **↑/↓**: height/16 (Ctrl: height/8). Scrolling uses WM_COMMAND 0x7A. Ctrl is read with GetAsyncKeyState(VK_CONTROL). |
| 0x485930 | `OnLButtonUp` | If a drag rectangle exists and is big enough: packet type 4 (clear selection), then WM_COMMAND **0x8011**(rect) to the Human object list (+0x118), and also to the Martian list (+0x11C) when byte_4D1249 (debug, both sides) is set. Otherwise by cursor mode dword_4EA43C: **1 (order)**: ground pick (0x47B3F0); Ctrl gives packet 0x13 (force fire at the ground), otherwise packet **0x0E** (move to point). **3 (build)**: building type dword_4F7428 with unit table +0x10 placement kind; types 0x1C3–0x1C8, 0x128, 0x1BC–0x1BF are forced to kind 0 (two-point walls). Kind 1–3 sends packet **0x18**(type, landscape, point) (place building). Kind 0: the first click stores the start (dword_59D9A8/AC, byte_59D9B0); the second spawns the object (0x446B30) with 0x80A5(end) and 0x80A6(start). Kind 4 spawns directly. |
| 0x485D00 | `OnMouseMove` / 0x800A | Updates the drag rectangle (vcall +0xC) and sends **0x8010**(rect) (hover highlight) to the object lists. |
| 0x485DE0 | `ShowContextMenu(pt)` (0x805B) | Popup **"Battlemap menu"** (0x432C60, id 0x91, 0x38 wide, parent cmd 0x71). Items: **0x73 "Cancel" (0x476)**, gfx 0x1C. If the player has nothing selected (0x4391F0) and one side has no units left (0x47D610/0x47D660 on +0x118), adds **0x76 "Return to Warmap" (0x47A)**, gfx 0x2C. Then asks the focused panel (dword_4EA444) with 0x8111, or the player's selection (0x439230), to add its items through **0x805C**(menu, pt). Shown at the mouse (0x432FB0). Only when the right-button double-click time matches dword_4E9F24. |
| 0x485F60 | `UpdateCursor(mode)` (WM_SETCURSOR) | Mode 0: cursor 4 if something is selected, else 0. Mode 1: cursor 5 (force fire) with Ctrl, else 4. Modes 3/7: 0. Mode 4: building ghost 0x4A13B0 (0x48 bytes, type dword_4EA440). Modes 5/6: drag line 0x4A03A0 (0x58 bytes; arg 1/0) *(wall / two-point placement, guess)*. |
| 0x4861E0 | `NextBattleTrack` (0x813C) | Only with CD music on (dword_4B8A88). If nothing is playing (0x413EB0), toggles dword_4D0124 between the two entries of dword_4D0118 (= CD tracks **4, 5**). Track length via 0x413DF0. If it is over 10 s, plays it (0x414150) and posts 0x813C after length × dword_4B84C8 (30 ticks/s); otherwise re-polls after 10 s. Debug string "Playing track %d for %d seconds". |

### 2.5 Two-point structure (walls, fences; *guess*) (0x4862C0–0x4868A0)

The vtable is 0x4AD810, the ctor 0x4862C0 (base 0x489E20) and the factory 0x497890.

| Addr | What it does |
|---|---|
| 0x4862C0 | Ctor. Start (+0xC8,+0xCC) and end (+0xD0,+0xD4), +0xE0..+0xE8 cleared, flag +0xC0 \|= 2. |
| 0x486340 | Returns flag +0xC0 bit2. |
| 0x486380 / 0x486420 | Load / save of the two endpoints (base 0x489ED0/0x489F30, then 4 words). |
| 0x486490 | Snaps both ends to cell centres and computes the length +0xD8 (0x43FDA0). Registers in the global table dword_59D9C0[] (index +0xDC, count dword_59D9B8). |
| 0x4865A0 | Build progress: health ratio (+0x7C/+0x78) < 0.9999 → number of visible segments +0xC4. Updates the child segments: sends 0x17B (hide) or shows them with the side palette byte (unit table +0xA1). |
| 0x4866C0 | Unregister from dword_59D9C0, then 0x48A9F0. |
| 0x4866E0 | Messages: 0x80A5 set end, 0x80A6 set start, 0x80A7 set end and position, 0x80D1/0x80D2 set start/end. Others go to 0x489FA0. |
| 0x486820 / 0x486850 | Block / unblock its cells in the route finder (0x494400 / 0x494370, dword_59E418). |
| 0x4868A0 | → 0x47F220. |

### 2.6 Minimap, "BattleMap miniMap view" (0x4868B0–0x487380)

| Addr | What it does |
|---|---|
| 0x4868B0 | Ctor: window 0x6B, parent dword_4E9658, vtable 0x4AD928. **RTypeMiniBattleMap** 0x4A1F90 loads `maps\hmin%02d.spr`/`mmin%02d.spr` for battle map +0x8C into +0x9C. Viewport frame +0x98 (0x49C740, colour dword_4DAF9C) sized from the view (0x43A5E0/0x43A5F0 = view w/h). Dot overlay +0xA0 (0x4A1CD0). +0xA4 = "2 renderers". Posts 0x8052. |
| 0x486C20 | (0x8052) Creates a 32×32-tile offscreen surface +0x7C (0x40CF00, flag 0x400). |
| 0x486CB0 | Paint: map sprite, overlay; dots for all objects (0x47D480 on +0x118, i.e. each object's 0x480070) and retreat points (0x487380); viewport rectangle at +0x90/+0x94. |
| 0x486E80 | OnMessage. 0x20 → cursor 0. 0x201/0x200+button/0x800A → converts the minimap point to world and sends WM_COMMAND **0x7A** (scroll). 0x8004 (view moved) → recomputes the viewport rectangle +0x90/+0x94, clamped. 0x80C8 (Tab) → toggles visibility (0x42F310), redraws dots. 0x8077 → plots a dot (0x4A1D80, colour, x·w/mapW, y·h/mapH). 0x8052 → 0x486C20. 0x80C5/0x80C4 → "1 renderer"/"2 renderers" (+0xA4 = 0/1). |
| 0x487380 | Sends 0x8077 for every retreat point in dword_59DDC8[0..dword_59DDC0) with colour dword_4DAFB4. |

### 2.7 Retreat points (0x4873F0–0x4875F0)

Vtable 0x4AD968, base 0x48B480, factory ranges routed to 0x4479B8.

| Addr | What it does |
|---|---|
| 0x4873F0 | Ctor: index +0xCE in dword_59DDC8[], count dword_59DDC0; +0xCC = target sector. |
| 0x487460 | Dtor: unregister. |
| 0x487490 | Place: unit table +0x40 = 0x10. Frame (arrow direction 0–3) from the nearest map edge (left/right/top/bottom). Sprite y-offset 500. Sets obj+0x24 = 1. |
| 0x487580 | Tooltip 0x807C: **"Retreat to" (0x518) + sector name (text 2 + sector)**, formatted "%s %s" into byte_59EF50. |
| 0x487570 / 0x4875F0 | Base forwarders (0x48B520 / 0x48B560). |

### 2.8 Transient and effect object pools and bases (0x487610–0x487AF0)

| Addr | What it does |
|---|---|
| 0x487610 | `WTransientObject::operator new`: pool of 0xD4-byte blocks, 512 per 0x6A00-byte chunk (dword_59DE18[] chunks, count dword_59DFA8, free list dword_59DFB0 via +0xBC, live count dword_59DFAC). Debug string "Allocating memory block for 512 WTransientObjects??? objects". |
| 0x4876B0 | `operator delete`: when live reaches 0 and there is more than 1 chunk, frees all ("UNLOADING ALL TRANSIENT OBJECT MEMORY"). Called from 0x48CD80, 0x48DCB0, 0x4A4470. |
| 0x487730 / 0x4877F0 / 0x487750 | `WTransientObject` ctor (base 0x47D6F0, vtable 0x4ADA70) / dtor / deleting dtor. |
| 0x487800 | Init: adds to the world (0x43C6D0 if it has no parent; dword_4D2550+0x20), sets the side palette and centres the sprite (+0x26 = w/2 + 32). |
| 0x487880 | → 0x47F260. |
| 0x487890 | Tick: unless the parent says "frozen" (vcall +0x9C), applies physics (0x47F050) and vcall +0x54. If the height goes < 0, posts **0x8057** (delete). Then 0x435590. |
| 0x4878F0 | Remove from the world (0x47EAA0, 0x43C6F0). |
| 0x487910 / 0x4879B0 | `WEffectObject` pool: 0xD8 bytes, 512 per chunk (dword_59DFB8[], dword_59E148/14C/150). Debug strings "…512 WEffectObjects…" / "UNLOADING ALL EFFECT OBJECT MEMORY". |
| 0x487A30 / 0x487AF0 / 0x487A50 | `WEffectObject` ctor (vtable 0x4ADB58) / dtor / deleting dtor. |

### 2.9 Weapons and effects (0x487B00–0x489050)

| Addr | Class / function | What it does |
|---|---|---|
| 0x487B00 | `WAreaBlast` ctor (vtable 0x4ADC38, base 0x488180) | Factory cases 718–720. |
| 0x487B50 | `WAreaBlast::Detonate` | Collisions query at its radius (+0xAC), excluding the firer's side (firer id +0xCC). By its own type +0x20: 0x1F8/0x21E/0x244/0x32F → **0x80F5** level 1; 0x1F9/0x21F/0x245 → level 2; 0x1FA/0x220/0x246 → level 3. 0x32C/0x32D/0x32E → delayed **0x80F4**(firer, level 1/2/3) after 120/240/360 ticks. 0x330/0x331 → 0x80F5 2/3. 0x332–0x334 → **0x80F6** level 1–3 plus damage (vcall +0xC0(self, 1.0)). 0x335–0x337 → **0x80F7** level 1–3. 0x2CE–0x2D0 damage only targets with height < 0 (submerged). 0x314–0x316 damage only kind-1 targets. Ability mapping *(guess)*: 0x80F5 = Freeze, 0x80F4 = Scare (timed), 0x80F6 = Pulse/Shock, 0x80F7 = Control. |
| 0x487F50 | `SpawnImpactEffect(size,x,y,h,owner,firer)` | size ≤ 64. Picks the effect type from a table by the terrain under (x,y): water → unk_4D0450, land 1–3 → unk_4D0340, other → unk_4D0230, indexed [size−1]. Creates it (0x446B30) and sets +0xC4 owner, +0xCC firer, position, init, add to world. |
| 0x488070 | `SpawnObject(type,x,y,h,owner,firer)` | Same as above without the terrain table. Callers: 0x48CDE0, 0x48F620, 0x492350. |
| 0x4880F0 | `SpawnObjectEx(..., c8, ca)` | Also sets +0xC8/+0xCA (damage / radius). |
| 0x488180 | `WExplosion` ctor (vtable 0x4ADD20, base 0x488FB0) | +0xC8 = unit table +0x44 (damage), +0xCA = +0x4A (radius). Factory cases 715–717. |
| 0x488200 | `WExplosion::Detonate` | Splash: every object in the radius except the firer itself takes damage (vcall +0xC0(self, 1.0)). Same-side objects are skipped unless vcall +0xF8 ("always damage") says otherwise. If height < 64 and not over water, sends **0x804B**(self) to the landscape (crater / damage map). |
| 0x488350 | `WExplosion::Init` | Plays the type's sound (unit table +0xA2) at the screen position, sets radius +0xAC = +0xC8, flag +0xC0 = 0x80, vcall +0xE0. |
| 0x4883C0 | `GetTypeSound` | Unit table +0xA2. |
| 0x4883E0 | `PlaySoundAt(id, x, y)` | 0x464EE0(id, x, y, 1.0, 0). Returns the sound object. |
| 0x488410 / 0x488460 | `WShell` (vtable 0x4ADE08) on `WProjectile` (vtable 0x4ADEF0, base 0x4949A0) | Cases 700–702 / 449–450. |
| 0x4884C0 | `WProjectile::Init` | Ground-shadow sprite +0xF4 (0x44-byte sprite, frame 0xFF, palette unit table +0xA1, y-offset 0x60). |
| 0x488570 / 0x4885C0 | Dtor (frees the shadow) / Draw with shadow alpha 0x100. |
| 0x4885F0 | `DrawShadow` | Positions the shadow at the ground point under the projectile (iso transform plus landscape+0x48). |
| 0x4886A0 | `WShell::Tick` | 0x494DB0 (flight). On impact state (+0xD8 == 2), once (flag +0xF0 bit1) and when +0xD4 % 5 == 0: random scatter radius ∝ distance/32/speed(+0xCC) + 1. Spawns the impact effect (0x487F50 with size word_4F2F16[type×16] = weapon table, 16 bytes a weapon). |
| 0x4887E0 / 0x488830 | `WRocket` (vtable 0x4ADFE0) | Same as above, triggered on state 3. Cases 697–699. |
| 0x488930 / 0x488980 | `WMortar` (vtable 0x4AE0D0) | Draw with an extra height word_4F2F1E. Cases 703–708. |
| 0x4889D0 | `WClusterShell::Release` | Sub-munition list at +0xB4 (12-byte entries {count?, type, speed (+8), max (+0xA), float spread (+0x14)}). For each entry, computes a ballistic launch (angle unit 2π/4096 = 0.0015339; sin/cos) towards a random spot near the target and spawns sub-projectiles that inherit the owner/root. |
| 0x488CA0 | Tick | Releases when +0xD8 == 1. |
| 0x488CC0 / 0x488D00 | `WGasCloud` (vtable 0x4AE1E0, base 0x488E80) | Cases 915–917. |
| 0x488D20 | `SetLifetime` | Type 0x393 → 240, 0x394 → 480, 0x395 → 720 ticks (+0xC2), else the argument. (**Black Dust** levels 1/2/3 *(guess)*.) |
| 0x488D70 | `WGasCloud::Tick` | While height ≥ 10: if over land (0x47FFE0) vcall +0x78; drift velocity (+0x88..+0x90) × 0.65 with z reversed (bounce); stops under 0.25; height fixed at 10. Every 15 ticks spawns the child type unit table +0x98 (damage puff). |
| 0x488E80 | `WFallingDebris` (vtable 0x4AE2E0, base 0x487730) | Cases 713–714. |
| 0x488F00 | Tick | Physics (0x487890). On hitting the ground (or the timer +0xC2 running out), spawns an impact effect of size +0xC0 at h − 8, then deletes itself (0x8057). |
| 0x488F90 / 0x488FA0 | Remove / SetTimer | |
| 0x488FB0 / 0x488FF0 / 0x488FD0 | `WAnimEffect` (vtable 0x4AE3D0, base 0x487730) | Flag +0xC0 = 0x80. Cases 918–920, 952–972, 988–993, 1000. |
| 0x489000 | Init | Sprite centred, +0x26 = w/2 + 64, flag 1. |
| 0x489050 | Tick | Advances the animation (0x40EF20); at the end, vcall +0x78 (die). |

### 2.10 Scenery (0x4890A0–0x4897D0)

| Addr | What it does |
|---|---|
| 0x4890A0 | `WAmbientScenery` ctor (vtable 0x4AE4B0, base 0x48B480; cases 940, 994–996). With probability (dword_4F7430 − 3)/(10 − 3) the type becomes **0x3AC**, except types 0x2AE–0x2B0, 0x2B5, 0x3E3, 0x3E4. dword_4F7430 is set by battle set-up 0x43983E from the sector's object lists; meaning *unclear* (likely "how fought-over the sector is", giving ruined variants). |
| 0x489170 | Init: if the type has an ambient sound (unit table +0xA2), starts it looped at its screen position (+0xC8). |
| 0x4891F0 | Dtor: stops the sound. |
| 0x489240 / 0x489290 | Moves the sound with the object's screen position (+0x2C/+0x30). |
| 0x4892C0 | Tick: 100/32768 chance a tick of restarting the sound. |
| 0x489340 | Static init: "Scenery objects left" stats counter (unk_59E158). |
| 0x489380 | `WScenery` ctor (vtable 0x4AE5B0, base 0x489E20; cases 721–741). Counts dword_59E16C; unit table +0x40 = 1. Types **0xD5–0xE5** become **0x399–0x3A9** with the same probability as above. |
| 0x489580 / 0x4895A0 | Dtor (decrements dword_59E16C) / remove (0x48AC50, 0x43C710). |
| 0x4895C0 | Messages → 0x489FA0. |
| 0x4895E0 / 0x489680 | Load / save the position (two words), clamped to the map (outside → 0,0). |
| 0x4896D0 / 0x489710 | If flag +0xB8 bit1 is set, marks / unmarks its landscape cell as occupied (0x47C830 / 0x47C8A0). |
| 0x489750 | `IsInExploredArea` *(guess)*: tests the byte map dword_4D2550+0x8C (w +0x10, h +0x14, data +0x20) at its cell. |
| 0x4897D0 | Sets the sprite brightness = GetShadeFactor × 255 (vcall +0x58), clears puppet flag 0x10. |

### 2.11 Turret sub-object and status effects (0x489820–0x489D80)

| Addr | What it does |
|---|---|
| 0x489820 | `WTurret` ctor (vtable 0x4AE6B0, base 0x490820 vehicle, 0x194 bytes; cases 905–908). +0x190 relative heading = 0. |
| 0x489860 / 0x489880 / 0x4898A0 | `IsFrozen` / `IsControlled` / `Is0x80F9` *(guess)*: status timer +0xDC ≠ 0 and effect id +0xE0 == 0x80F5 / 0x80F7 / 0x80F9. Shared by all vehicle vtables. |
| 0x4898C0 / 0x4898D0 | Get / set the status timer +0xDC. |
| 0x4898F0 / 0x489900 | Constants 3 and 8.0. |
| 0x489940 / 0x489950 / 0x489970 | Forwarders (0x4935C0; root vcall +0xDC→+0x9C; 0x47EE10). |
| 0x489980 | Attach: if the parent is not kind 1 and is damaged (health ratio < 1.0), hides child flag-bit-11 parts. Frees its grid slot +0xBC (0x449900). Initial heading = parent heading + hardpoint base +0xB0→+4. +0x192 = 1 unless the parent kind is 4–9. |
| 0x489AC0 | Messages → 0x490A70 (vehicle handler). |
| 0x489AE0 | Draw: layer above the parent (+0x26 +4, +0x1E +1). |
| 0x489B40 | `WTurret::Tick` | Turns towards the target point (+0xC8,+0xCC), or back to rest, at **50 units/tick** (of 4096), limited to the hardpoint arc (+0xB0→+6 around +4). Starts or stops the traverse sound (unit table +0xA2 → +0xE8). |
| 0x489D00 | SetHeading(parent + 0x190 + a). |
| 0x489D30 / 0x489D60 / 0x489D80 / 0x489DA0 | Forward to the parent's vcalls +0xD0/+0x84/+0xAC/+0xA8 (fire, etc.). |

### 2.12 BAMO persistent object base (0x489DB0–0x489FA0)

| Addr | What it does |
|---|---|
| 0x489DB0 / 0x489DF0 | Class (vtable 0x4AE828) on 0x48E8A0. Factory cases 824–829, 831–844, 909–914. |
| 0x489E00 | Sets flag +0xB8 bit5. |
| 0x489E20 | `WBMObject` ctor (vtable 0x4AE938, base 0x47D6F0): +0xBC = −1 (grid slot), +0xC4 = 0x47DA20() (max health?), flags +0xC0 \|= 1, +0xB8 \|= 0x80. |
| 0x489EC0 | Dtor. |
| 0x489ED0 | `Load(stream)`: base 0x4404A0, then word, +0x40 heading, floats +0x78/+0x7C/+0x84, word +0xC4. |
| 0x489F30 | `Save(stream)`: base 0x440550, tag **"BAMO"** (0x4F4D4142), then the same fields. |
| 0x489FA0 | `WBMObject::OnMessage` | See §4.1. |

---

## 3. Data structures and globals

### WObject (battle map), selected fields
| Off | Meaning |
|---|---|
| +0x18 | byte: alive/visible (handlers bail if 0) |
| +0x20 | type id (index into the unit table 0x51A0F0) |
| +0x24 | byte: AI/enemy-controlled (0 = player) |
| +0x40 | heading 0..4095 |
| +0x44 | parent object |
| +0x48 | group (WBMGroup) / model data for hardpoints |
| +0x4C | puppet/sprite (+0x14 flags, +0x1A/+0x1C/+0x1E screen x/y/z, +0x26 y-offset, +0x28..+0x2E extents) |
| +0x54 | group panel back-pointer |
| +0x70 | children list |
| +0x78, +0x7C, +0x80 | world x, z, y (height incl. terrain) |
| +0x84 | height above terrain |
| +0x88..+0x90 | offset; +0x94 vertical speed |
| +0x98..+0xA0 | local offset to parent |
| +0xA8 | sprite frame (flag bit9) |
| +0xAC | radius |
| +0xB0 | hardpoint (word idx, word sub) |
| +0xB8 | flags: bit0 selectable/unit, bit1 occupies cell, bit3, bit5, bit6 selected, bit9 use frame, bit10 raw offset, bit11 hide when damaged |
| +0xC0.. | subclass fields |
| +0xCC/+0xC4 | (effects) firer id / owner id |
| +0x188/+0x18C | (vehicles) flags / "can't tunnel" byte |
| +0xD8 | (vehicles) stance |
| +0xDC/+0xE0 | status timer / status message (0x80F5 freeze…) |

### WBMGroup (0x1E0 bytes)
+0x1C list iterator base, +0x20 member list, +0x24 count, +0x2C target object, +0x30 target id, +0x34..+0x3C target's
last position, +0x40/+0x44/+0x48 target point, +0x4C max slope, +0x4E min speed / attack reach, +0x50 max range,
+0x52 movement category, +0x54 stance (0 Avoid, 1 Retaliate, 2 Attack, 3 Roam & Attack), +0x56 arrived count,
+0x58/+0x5C think counter / interval, +0x5A, +0x60/+0x64 home/centroid, +0x68/+0x6C min/max member distance,
+0x70[16] terrain cost, +0xB0/+0xB4[8×8] slope cost, +0x1D0 average war-map speed, +0x1D4 route, +0x1D8 flags,
+0x1DC pending mode (0 none, 1 Force Fire, 2 Tunnel, 3 Submerge, 4–0xC Black Dust / Shock / Explosives, 0xD–0xF
Infiltrate, 0x10–0x18 Freeze / Scare / Control).

+0x1D8 flags: bit0 moving, bit2, bit3 player-issued move (marker and voice), bit4 player group (some member
+0x24 == 0), bit5 all airborne (category 7), bit6 in own sector, bit7 *(unclear, AI)*, bit8 has category 5/6,
bit9 has category 1–4.

### Unit/object type table 0x51A0F0 (0xB0 bytes a type), fields used here
+0x0C side, +0x10 kind (1 unit; 2/3 buildings; 4 non-target; also the placement kind), +0x14 name text id, +0x2C
counter/icon frame, +0x40 byte flags (1 scenery, 0x10 retreat point), +0x41 byte height offset, +0x44 word
range/damage, +0x48 word speed, +0x4A word radius, +0x52 word movement category (1–7; 7 = air), +0x54 war-map
speed, +0x92 word, +0x98 dword child or destruction effect type, +0xA0 byte voice set (selects the
SC/SB/SA variant), +0xA1 byte palette, +0xA2 word ambient/engine sound, +0xAC byte "seen".

### Landscape (dword_59D940, WLandscape)
+0x1C map width (world), +0x24/+0x28 cells W/H, +0x30 cell size (16.16), +0x34 bias, +0x48 screen y origin,
+0x64 cells (10 bytes: +8 terrain nibble \| slope<<4, +9 flags bit1 blocked, bit2, bit3). 0x47B170 height(x,z),
0x47B3F0 screen→ground pick, 0x47C830/0x47C8A0 occupy/free a cell.

### Other globals
| Global | Meaning |
|---|---|
| dword_4D2550 | World: +0x20 object registry, +0x84 shadow map, +0x8C explored map, +0x90 render list, +0x118 spatial grid |
| dword_4E9B70 | Game/world (+0x114 drag active, +0x118/+0x11C/+0x120 Human / Martian / neutral object lists) |
| dword_4E9B74 / dword_4E9B78 | Human / Martian player objects (packet senders) |
| dword_4E9B7C | War map world (+0x54 flag) |
| dword_59E418 / dword_59E41C | WRouteFinder (+0x38, +0x44 = group owning the search) / pending flag |
| dword_4EA43C | Cursor/order mode (1 order, 3 build, 4–7 placement) |
| dword_4EA440 / dword_4F7428 | Building type being placed |
| dword_4EA444 | Focused panel / window |
| dword_4CA998 | SFX volume scale: 1.0 normal, 0.25 while the battle menu is open |
| dword_4B84C8 | Ticks per second (30) |
| dword_4D0118[2] | Battle music CD tracks {4, 5}; dword_4D0124 current index |
| dword_4DAFA0/A4/A8/B4, dword_4DAF9C | Minimap colours (own unit / own building / enemy / retreat point / view frame) |
| dword_59D9A4 | Live group count |
| dword_59D9B4 | Selection change counter |
| dword_59D9A8/AC, byte_59D9B0 | Wall placement first point |
| dword_59D9C0[], dword_59D9B8 | Two-point structures |
| dword_59DDC8[], dword_59DDC0 | Retreat points |
| dword_59E16C | Scenery count |
| byte_4D1249 | Debug "both sides" (box select also hits Martian units) |
| dword_4F7430 | Sector value driving the scenery variant chance, range 3..10 (dword_4BD4F8 = 3, dword_4BD4FC = 10) |
| dword_4E9F24 | Double-click time |
| dword_4BA058 = 40, dword_4BA05C = 4 | Scroll divisor, margin |
| flt_4F0510[8], dword_4F04C0[], unk_4F6E38/dword_4F6E78[cat×17] | Movement category tables (slope base, max slope, terrain costs) — filled from OBJ.ojd *(guess)* |
| word_4F2F10.. (16 bytes a weapon: +6 impact size, +0xE extra height) | Weapon table |
| unk_4D0230 / 4D0340 / 4D0450 | Impact effect type tables (other / land / water) |

---

## 4. Messages, commands, strings, tuning

### 4.1 Messages handled

**WBMGroup 0x4807F0**
0x111 (button ids 0x80CE, 0x80F3, 0x8118, 0x8119 → packet 0x15), 0x800F recalc, 0x80C0 attack obj, 0x805A stop,
0x8031 halt, 0x8058 target gone, 0x811A cancel if target, 0x8081 member arrived, 0x8085 enemy spotted, 0x80B1
attacked by (15% chance of speech 0x10F, then retaliate), 0x804E distance query (→ 0x802E), 0x804F move,
0x805C → base, 0x80B2 set stance (3 → recentre).

**Messages a group sends to its members**
0x80B9 (clear orders), 0x805A (stop), 0x8031 (halt), 0x8067 (abandon), 0x80B2 (stance), 0x80E1–3 (pulse),
0x80F0 (scan), 0x80EE (tunnel), 0x80EF (submerge), 0x80C2 (go straight to), 0x802E (follow route), 0x8086,
0x80A0 (disband/self-destruct), 0x80BB (retreat point, to the group object).

**WBattleMap 0x484FB0**
0x20 cursor; 0x100 key (player hotkeys first via 0x439230, then 0x485610, then the world); 0x101; 0x111:
0x3ED → 0x4057F0 (dialog, returns 0x80B8) *(unclear which)*, 0x80B4 → unpause (SFX 1.0, world+0x44 = 0),
0x80B6 → 0x409460 (Quit menu, returns 0x8123), 0x80B8 → battle menu 0x49BD50 (SFX 0.25, world+0x44 = 1 pause),
0x8123 → unpause and restart the music; 0x200 (mousemove) → 0x485D00; 0x201 → starts the drag rectangle
(0x49C080) in modes 0/1; 0x202 → 0x485930; 0x205 → cancel and WM_COMMAND 0x73; 0x8004 scroll-to;
0x8006 cancel drag; 0x800A drag; 0x800F → 0x42E790 (cursor depends on selection); 0x803B → 0x42DB00(1);
0x8052 create view; 0x805B context menu; 0x80A4/0x8137 → reset cursor mode 7; 0x80C4/0x80C5 re-posted;
0x813C music.

**Minimap 0x486E80**: 0x20, 0x200/0x201/0x202, 0x8004, 0x800A, 0x8052, 0x8077, 0x80C4, 0x80C5, 0x80C8.

**WBMObject 0x489FA0**: 0x111+0x8010/0x8011 (rect hover/select tests 0x48A4C0/0x48A530), 0x804B damage,
0x807C tooltip, 0x8125 (0x48A660), 0x8126 (to children), 0x8067 abandon → 0x80A0, 0x80C1 (vcall +0xE0),
0x80A0 destroy: posts 0x8057, spawns the destruction effect (unit table +0x98) at h − 1.414 × size, and for some
sector types (when war map flag dword_4E9B7C+0x54 is set) plays speech 0x112 (Human) or 0x13B (Martian).

**Status effects**: 0x80F4 (timed, *Scare?*), 0x80F5 (*Freeze*), 0x80F6 (*Pulse/Shock*, with damage), 0x80F7
(*Control*), 0x80F9 (unknown).

**WM_COMMAND ids**: 0x7A scroll-to(packed point), 0x73 cancel/deselect, 0x76 Return to Warmap, 0x80B8 battle menu.

### 4.2 Command packets (via the player objects; network and playback)
4 (clear selection before a box select or click), **0x0E** move to point / pending ability at point, **0x10**
target object, **0x11** halt (Backspace), **0x12** select group n (0–9), **0x13** force fire at the ground
(Ctrl+click), **0x15** order (0x80CE stance/abandon/retreat or 0x80F3 ability), **0x18** place building.
Receiver for groups: 0x4812B0.

### 4.3 Order buttons (built at 0x448700 in part 05; executed here)
| Cmd | Param | Text | Gfx |
|---|---|---|---|
| 0x80CE | 0/1/2/3 | Avoid / Retaliate / Attack / Roam and Attack (0x471–0x474) | 0x34C–0x34F |
| 0x80CE | 0x80B7 | Retreat (0x479) | 0x2D |
| 0x80CE | 0x8067 | Abandon (0x46F) | 0x34B |
| 0x80F3 | 0x80ED / 0x80EE / 0x80EF | Force Fire / Tunnel / Submerge (0x48A–0x48C) | 0x1E / 0x34A / 0x349 |
| 0x80F3 | 0x80D5–D7, D8–DA, DB–DD, DE–E0, E1–E3, E4–E6, E7–E9, EA–EC | Black Dust, Shock Bomb, Explosives, Infiltrate, Pulse, Freeze, Scare, Control (0x482–0x489) | 0x25, 0x26, 0x351–0x356 |
| 0x80F3 | 0x80F0 | Scan (0x48D) | 0x358 |
| 0x813E | 0x8065 / 0x813F | Build / Resume building tasks (0x470 / 0x496) | 0x20 / 0x23 |

### 4.4 Speech (SFX.ojd ids; 3 variants a voice set, chosen by unit table +0xA0)
0x10A acknowledge move, 0x10B route not found, 0x10C attacking / firing, 0x10D / 0x10E target lost /
destroyed, 0x10F under attack (15%), 0x112 / 0x13B object destroyed (Human / Martian), 0x121 "can't"
(retreat or exit impossible). Ambient/engine sounds from unit table +0xA2.

### 4.5 Strings
"Collisions", "BM Groups", "Stats", "Scenery objects left", "ALREADY AT DESTINATION", "CAN'T GET TO WITHIN ATTACK
DISTANCE!", "SEARCHING FOR ROUTE WITH DFT %d", "GOING STRAIGHT THERE!", "ROUTE FOUND", "ROUTE NOT FOUND",
"WCmdTarget", "BattleMap", "Battlemap menu", "BattleMap miniMap view", "Playing track %d for %d seconds",
"1 renderer", "2 renderers", the transient/effect pool messages, "%s %s", "\n%2.1f%% %s", "\n%s %s\n%s %0.1f",
"\n%s". TEXT.ojd ids: 0x476 Cancel, 0x47A Return to Warmap, 0x518 Retreat to, 0x4B5 complete, 0x48E Currently
building:, 0x48F Days to completion: (value × 1/24), 0x491 Idle, 0x492 Construction suspended, plus the order
names in 4.3.

### 4.6 Tuning constants
Think phase rand()%30; route search limit 200; re-route threshold 80 units; roam/leash radius 400; retry route
after 10 ticks; portrait blink 30 ticks; under-attack speech chance 5000/32768; turret traverse 50/4096 per tick;
gas cloud lifetimes 240/480/720, puff every 15 ticks, drift damping 0.65, stop under 0.25, floor height 10; scare
delays 120/240/360; destination marker type 0x17C; impact effect sizes ≤ 64; crater only below height 64; shell
scatter every 5 ticks; music re-poll 10 s; edge scroll margin 32 px; arrow scroll width/7 (Ctrl /3), height/16
(Ctrl /8); double click ≤ 10 ticks (icons); SFX duck 0.25; damaged-scenery chance (v − 3)/7.

No `[Tweak]` ini settings are read in this piece.

---

## 5. Cross-references to other pieces

- **Callers into this piece**: 0x446B30 object factory (part 05; all the constructors here), 0x43A600
  BattleMapSelection → 0x480190, 0x439770 battle set-up → 0x484A30, 0x4868B0, 0x483DF0, 0x47CFE0 → 0x4802E0,
  0x422910 / 0x428D70 / 0x42A0B0 → icon tray, 0x4222C0 / 0x492350 → 0x47FE10, 0x491020 → 0x483880,
  0x491870 → 0x482450, 0x445010 (WObject) → 0x482510, 0x48F8A0 → 0x480EB0, 0x497890 → 0x4862C0,
  0x48B3D0 / 0x48E850 / 0x495180 → 0x47F630 (Draw), the vehicle and puppet classes (0x48A4C0+, 0x4949A0,
  0x495620+) for the shared virtuals.
- **Callees outside**: WLandscape 0x47B170 / 0x47B3F0 / 0x47C830 / 0x47C8A0 / 0x47D430 / 0x47D480 / 0x47DA20 /
  0x47DA80 / 0x47EB00 / 0x47EE10 / 0x47F050 / 0x47F220; route finder 0x494370 / 0x494400 / 0x494440 /
  0x494690 / 0x494960, A* 0x44EFA0; sound 0x464EE0 / 0x464E30 / 0x465590; CD audio 0x412940 / 0x412960 /
  0x412C00 / 0x413DF0 / 0x413EB0 / 0x414150; UI framework 0x42F520 (window ctor), 0x42F650 (default handler),
  0x42E780 (cursor), 0x432C60 / 0x432D70 / 0x432FB0 (popup menu); messages 0x4355A0 (post delayed), 0x4355D0;
  packets 0x4493E0 / 0x4A4440 / 0x4460A0 / 0x449A70 and send 0x4A4470 / 0x4460C0; battle menu 0x49BD50,
  quit menu 0x409460, dialog 0x4057F0; RTypeMiniBattleMap 0x4A1F90 / 0x4A1CD0 / 0x4A1D70 / 0x4A1D80;
  placement ghosts 0x4A13B0 / 0x4A03A0; drag rectangle 0x49C080; 3D view 0x49F240; sprites 0x40E700 /
  0x40EC30 / 0x40F000 / 0x40EF20; text 0x4146F0.

---

## 6. Missing in WoWEngine: candidate tasks (prioritised)

WoWEngine currently stops at the war map: there is no battle map, no world object system and no unit logic. All
items are new. Order of dependency:

1. **Battle-map world object base (P0).** Position, height from terrain, clamping, heading 0..4095, parent and
   hardpoint attachment, selection flag, iso screen projection, Draw/Clip.
   Addresses: 0x47F480–0x480100 (vtable 0x4AD620, ctor 0x47D6F0 in part 10), unit table fields (§3).
2. **Spatial queries (P0).** Grid buckets with 4 layers, the "Collisions" radius query, nearest valid target.
   Addresses: 0x47FBA0, 0x47FC50, 0x47FD40, 0x47FE10, 0x449920.
3. **Landscape cell passability and cost (P0).** Terrain nibble, slope, cell flags, per-category terrain and
   slope cost tables, NearestPassableCell. Addresses: 0x480680, 0x480740, 0x482B00, 0x47FFE0, 0x4802E0 (table
   build), tables 0x4F04C0, 0x4F0510, 0x4F6E38, 0x4F6E78 (find their OBJ.ojd records).
4. **WBattleMap screen shell (P0).** Window, palette group 0x2F0, 3D view creation, paint order of the three
   object lists, edge scrolling, keyboard (digits, Backspace, Esc, Tab, Home, PgUp/PgDn, arrows ± Ctrl),
   WM_COMMAND 0x7A scroll. Addresses: 0x484A30, 0x484D00, 0x484DF0, 0x484F00, 0x484FB0, 0x485610.
5. **Selection and orders input (P0).** Drag rectangle with hover 0x8010 and select 0x8011, click to move
   (packet 0x0E), Ctrl force fire (0x13), cursor modes and cursor sprites, right-click popup ("Cancel" /
   "Return to Warmap" when one side is wiped out, plus the panel's own items via 0x805C). Addresses: 0x485930,
   0x485D00, 0x485DE0, 0x485F60, 0x48A4C0 / 0x48A530 (part 12).
6. **Command packet layer (P0).** Even single player routes orders through packet types 4, 0x0E, 0x10, 0x11,
   0x12, 0x13, 0x15, 0x18 (needed for playback and network). Addresses: 0x4812B0 (receiver), 0x4807F0 (0x111
   → 0x15), 0x485610, 0x485930.
7. **WBMGroup (P0/P1).** Stats recalculation, move with the route finder (A*) or straight line, destination
   flag 0x17C, arrival counting, attack, retaliate, chase, retreat to the nearest retreat point, stance AI
   (Avoid / Retaliate / Attack / Roam), AI random ability use. Addresses: 0x480190–0x483B70 (all of §2.2).
8. **Unit speech feedback (P1).** 0x10A–0x10F, 0x121, 0x112 / 0x13B with voice set from unit table +0xA0
   (SC/SB/SA), and the portrait flash. Addresses: 0x481770, 0x4842D0, 0x483590, 0x4833F0, 0x4821F0, 0x480BB5.
9. **Order panel semantics (P1).** Stances (0x80CE 0–3), Retreat, Abandon, Force Fire / Tunnel / Submerge
   pending modes, the ability modes and the immediate abilities (Pulse, Scan). Addresses: 0x4817C0, 0x481CA0,
   0x480EF0; buttons at 0x448700 (part 05).
10. **Minimap (P1).** `maps\hmin%02d.spr` / `mmin%02d.spr`, dot colours by own unit / own building / enemy /
    retreat point, viewport frame, click and drag to scroll, Tab toggle. Addresses: 0x4868B0–0x487380,
    0x480070, RTypeMiniBattleMap 0x4A1F90+.
11. **Explosions, splash damage and craters (P1).** Addresses: 0x488180, 0x488200, 0x488350, 0x487F50
    (terrain-dependent impact effects), 0x488070, 0x4880F0, 0x804B to the landscape.
12. **Projectiles (P1).** Shells, rockets and mortars with ground shadows, impact scatter, cluster
    sub-munitions. Addresses: 0x488410–0x488CA0, base 0x4949A0 / 0x494DB0 (part 12/13), weapon table 0x4F2F10.
13. **Special weapon area effects (P2).** Freeze / Scare / Control / Pulse status effects with levels and
    durations, the status timer at +0xDC / +0xE0, gas clouds with lifetimes. Addresses: 0x487B00, 0x487B50,
    0x488CC0–0x488D70, 0x489860–0x4898D0.
14. **Transient and effect object pools (P2).** 512 a block (in C++ a pool or plain allocation is fine; keep the
    lifetime semantics, e.g. delete via 0x8057). Addresses: 0x487610–0x487AF0, debris 0x488E80 / 0x488F00,
    animated effects 0x488FB0 / 0x489050.
15. **Retreat points on map edges (P1).** Edge-facing arrow frame, tooltip "Retreat to <sector>", minimap dots.
    Addresses: 0x4873F0–0x487580, 0x43D3F0.
16. **Turrets (P2).** Traverse rate 50 a tick within the hardpoint arc, traverse sound. Addresses:
    0x489820–0x489DA0.
17. **Scenery (P2).** Ambient-sound scenery, variant substitution (D5–E5 → 399–3A9, → 0x3AC) driven by
    dword_4F7430 (meaning to confirm at 0x43983E / 0x40B570), explored-map test, cell occupancy. Addresses:
    0x4890A0–0x4897D0.
18. **Two-point structures (walls; P2).** Two-click placement (types 0x1C3–0x1C8, 0x128, 0x1BC–0x1BF), endpoint
    snapping, segment build-up, route finder blocking. Addresses: 0x4862C0–0x486850, 0x485930 (kind 0).
19. **Object tooltips (P2).** Name, "% complete", factory "Currently building: / Days to completion:" and
    "Idle" / "Construction suspended". Address: 0x489FA0 (0x807C), 0x45F7D0, 0x460A40.
20. **Battle map audio behaviour (P2).** Battle music alternating CD tracks 4/5 (in WoWEngine, the
    `Music-Human\`/`Music-Martian\` WAV equivalents), re-polled every 10 s; SFX ducking to 0.25 while the battle
    menu is open (dword_4CA998) and pause via world+0x44. Addresses: 0x4861E0, 0x484FB0 (0x80B4 / 0x80B8 /
    0x8123).
21. **Save/load of battle-map objects (P1 for saves).** The "BAMO" chunk (heading, x, z, height, +0xC4) and the
    subclass extensions (two-point endpoints, scenery position). Addresses: 0x489ED0 / 0x489F30, 0x486380 /
    0x486420, 0x4895E0 / 0x489680; WoWEngine's `SaveGame.h` already names BAMO (0x489F30) but does not parse it.
22. **Unit icon tray (P2).** Click to select, double-click to centre, blink while speaking. Addresses:
    0x483DF0–0x4849D0.

Open questions to resolve when implementing: the exact meaning of message 0x80F9 and of group flag bit7; whether
0x80F4 is Scare; what dword_4F7430 measures; the identity of the 0x3ED dialog (0x4057F0) on the battle map; the
OBJ.ojd records behind the movement category tables 0x4F04C0 / 0x4F0510 / 0x4F6E38.
