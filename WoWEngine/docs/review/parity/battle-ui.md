# Battle map interface and input: parity review

Scope: src/battlemap/BattleMap.cpp (input, selection, cursor, popup, minimap, brackets), the move-order flag
(Driving.cpp / Effects.cpp), FrontEnd cursor drawing. Original: 0x483590, 0x487800, 0x40EE10/0x40F040, 0x47F480,
0x4406A0, 0x497000/0x497110, 0x484DF0, 0x485610, 0x485930, 0x485D00, 0x485DE0/0x485F60, 0x432C60-0x433330,
0x4868B0/0x486CB0, 0x48B2D0, 0x4812B0/0x481CA0, 0x4833F0.

How the main finding was confirmed: headless run (London save; select at 381,263; click ground at 250,400; screenshot 2
updates later). The flag draws as a solid black disc at the route's last node
(/tmp/claude-0/-home-user-WoWRevived/f57fb885-4a9f-5c8e-87d7-5e85689fb7c2/scratchpad/bui/m2.png). Data was read from
OBJ.ojd and Dat.wow with wowdump plus a small record parser
(/tmp/claude-0/-home-user-WoWRevived/f57fb885-4a9f-5c8e-87d7-5e85689fb7c2/scratchpad/bui/ojd.py).

---

## Visible bugs

### 1. The move-order flag ("_ACK") is a black disc instead of its animation. Severity: HIGH
- Original: 0x483590 (the flag); 0x487800 (an effect's vt+0x14 start); 0x40F040 (the sprite renderer's vt+0x18,
  which sets the level); 0x40EE10 (the draw).
- Engine: src/battlemap/BattleMap.cpp:2049-2054 (the level of a flying object's sprite);
  src/battlemap/Effects.cpp:93-131 (SpawnEffect); src/battlemap/Driving.cpp:247-266.
- **The object:** unit type 0x17C (record 16 code `OTYPE_MOVE_ACK`), class WAnimEffect (0x488FB0, frame step
  +0xC0 = 0x80). Its GType is record 24 = 0x1A (26): `dat\hove-ack.spr` (Human) and `dat\move-ack.spr` (Martian).
  The GType has palette slot 5 (the battle's interface table, BMHI/BMMI.SHH, 30 levels) and hot spot 24,24. Each sprite
  is 48x48 with 7 frames. At 0x80 a step the flag lasts 13 updates and then goes (0x40EF20 with no loop, then vt+0x78).
  Type 0x17C has no record 62, so its +0xA1 is the default, 0xC0.
- **Original sequence (0x483590):**
  1. 0x446B30(0x17C) makes the object.
  2. vt+0x40 puts it at (x, y, height 0).
  3. vt+0x24 adds it.
  4. vt+0x14 (0x489000 -> 0x487800) starts it. This sets the renderer's level to the type's +0xA1 >> 2 (48) and +0x26
     to width/2 + 0x20.
  5. **The renderer's vt+0x18(0) then sets the level to 0 again** (0x40F040: 0 >> 2 = 0).
  6. `and word [renderer+0x14], 0FFFEh` clears blend mode 2, so the flag draws opaque.
- **What the engine does:** every effect sprite is drawn at its type's +0xA1 / 4. For 0x17C that is 48, which
  ShadeTable::Level clamps to 29, the last level of BMHI.SHH. Every colour in that level is 0 (black). I checked this
  by reading the file: the average brightness of level 0 is 358, level 20 is 192 and level 29 is 0. Each of the 7
  frames is drawn in solid black, so the player sees a black disc.
- **Fix:** give Effect a `level` field that defaults to +0xA1 >> 2 and draw with it. In the flag's spawn (both
  SpawnEffect(0x17C, ...) calls in Driving.cpp) set level = 0. The engine has no translucency, so the cleared bit 0
  needs no work, but record it in a comment.

### 2. The flag and the "acknowledged" speech appear for computer and automatic moves. Severity: HIGH
- Original: 0x483590 requires group +0x1D8 bit 3 (a player's order packet) and bit 4 (a player's group). Bit 3 is set
  only by the player's packets 0x0E (0x481CA0 at 0x481E4E), 0x10 (0x4814D8) and 0x13 (0x48167F). 0x483590 clears
  bit 3 after showing the flag, so one order gives one flag. The callers (0x4831B3 / 0x483370) also require the group's
  first member's +0x24 to be 0 (the player's side).
- Engine: src/battlemap/Driving.cpp:247-255 and 261-266. OrderGroupMove spawns 0x17C and speaks 0x10A for every caller
  that has no target: BattleAi.cpp:87/215/228 (the AI's moves), Weapons.cpp:669 (Avoid runs and home moves) and
  Reinforcements.cpp:126 (retreats).
- What the player sees: flags at the enemy's destinations, also inside the fog, and the acknowledgement speech for AI
  and automatic moves.
- Fix: add a per-group flag (bit 3) that the player's 0x0E, 0x10 and 0x13 set. Show the flag and the speech only when
  that flag is set, the group is the player's, and the first member is the player's. Clear the flag after the flag
  shows.
- The same applies to "ROUTE NOT FOUND": the original 0x4834D4 also requires bits 3 and 4 before speech 0x10B, but the
  engine's Driving.cpp:239 speaks it unconditionally.

### 3. An attack order's approach shows no flag. Severity: MEDIUM (path partly unverified)
- Original: 0x483590 shows the flag whatever the group's target (+0x2C). Only the speech 0x10A is skipped when +0x2C is
  set. The attack packets (0x10 and 0x13) set bit 3, so the first approach move (0x482E60) shows the flag.
- Engine: Driving.cpp:250 and 263 `if (!attacking)` skips both the flag and the speech.
- Fix: spawn the flag on bit 3 alone, and keep the speech conditional on having no target.
- Unverified: I did not trace the group tick's (0x483B70) path to 0x482E60 for the approach.

### 4. Selection brackets, power bar, click and box-select points sit at the unit's feet. Severity: HIGH for Martian walkers, low for tanks
- Original: 0x47F480 sets the object's screen point (+0x2C / +0x30) with 0x4406A0. It is x, and the projection of y at
  height +0x80 (the ground plus the object's own height), **less the type's +0x41 byte** (byte_51A131). The hotspot is
  centred on that point (0x4406A0 / 0x4405C0). The brackets and bar are drawn round the hotspot (0x497000), and
  box-select tests that point.
- Engine: BattleMap.cpp:283-289 (UnitScreen) uses the ground height and drops both the lift and +0x41.
- Examples (+0x41 is scaled by record 25): type 4 gives 5 px; type 0x42 (a Martian walker) gives 0x5A x 0.9 = **81 px**.
  So on a walker the engine's brackets and power bar sit at the feet, 81 px below where the game draws them, and the
  clickable square is in the wrong place.
- Fix: `sy = ProjectY(iy, HeightAt + lift) - size41 - scrollY`.

### 5. Enemy units hidden in the fog can still be hovered and clicked. Severity: MEDIUM
- Original: 0x48B2D0. When the other side's object goes into the fog, its hotspot is removed (0x4405C0(0) at 0x48B38A).
  When it comes out, the hotspot is made again (0x48A660 at 0x48B348).
- Engine: BattleMap.cpp:388-405 (HotspotAt) ignores `fogged` (and `hidden`). As a result:
  - the cursor turns to "attack" over an invisible enemy (CursorAt, :407);
  - clicking there attacks it (ReleaseLeft, :383).
- Fix: skip `u.fogged` units in HotspotAt and in the box-select test.

### 6. Battle popup layout: one column instead of collapsible rows by command. Severity: MEDIUM-HIGH
- Original WPopupMenu layout:
  - 0x432D70 / 0x432E90 each put a new item in the row whose first item has the same +0x28 (the item's command,
    stored by 0x42DE00), or start a new row (0x4331C0). A menu has at most 10 rows of 8 items.
  - 0x433220 lays each row left to right (right to left near the right edge, +0x1BC), and the rows go down.
  - 0x433330(-1) hides every item after the first in each row. On hover (0x4330B0 -> 0x433330(cmd)) that row opens
    out to the right.
  - In the battle:
    - row 1: Cancel (0x73);
    - row 2: Return to Warmap (0x76);
    - row 3: all the 0x80CE items (the stances, Retreat, Abandon), collapsed to "Roam and Attack" until hovered;
    - row 4: all the 0x80F3 items (Force Fire and the specials), collapsed the same way.
- Engine: BattleMap.cpp:1726-1753 (OpenPopup) and 2124-2129 (draw) stack every item in one column, all shown.
- Placement:
  - Original (0x432FB0): after layout, x - W/2 - left and y - H/4 (H is the whole menu's height, divided by 4
    rounding toward 0), then kept 3 px inside the screen.
  - Engine: uses item 0's width/2 and height/4.
- Fix: build rows keyed by command, show only the first of each row unless the row is hovered, and place the menu by
  its laid-out size. The war map's popup (src/warmap/WarMap.cpp:2143) has the same simplification, noted there.

### 7. A mixed selection's popup shows only the first group's orders. Severity: MEDIUM
- Original: the selection's 0x805C (0x4482D0):
  - if the selection is all of one kind (vt+0x2C or +0x28), its first object adds its orders;
  - otherwise 0x448500 builds the orders common to all of them: it asks each member 0x8130 / 0x8131 / 0x8132 and adds
    an item only when every member answers.
  - After the selection, the world (+0x84) gets 0x805C too (0x485EFD).
- Engine: BattleMap.cpp:1747-1748 uses the first selected unit's AddOrders.
- Fix: when the selected groups differ in type, add only the orders every group allows (the intersection of AddOrders
  per type). The world's own items are not traced.

### 8. Effect depth +64 instead of +32 (affects the flag's draw order). Severity: LOW
- Original: 0x487862-0x487868 sets +0x26 = (+0x2A - +0x28)/2 + 0x20. That is the sprite's width / 2 + 32, since
  0x40EC50/0x40EAF0 make +0x2A - +0x28 the width.
- Engine: Effects.cpp:118 `e.depth = image->width / 2 + 64`.
- Effect: every flying sprite, the flag included, sorts 32 world-y later, so it is drawn over objects up to 32 units in
  front of it.
- Fix: + 32. This is the effects reviewer's area too.

## Behaviour divergences

### 9. Edge-scroll margin in a window. Severity: LOW
- Original: 0x484E2C-0x484E4D uses 32 px when byte dword_4EA364 is clear (windowed) and [Screen] "Scroll Border"
  (dword_4BA05C, 4) when it is set.
- Engine: BattleMap.cpp:1590 always uses 4, though its own comment says "32 in a window".
- Scrolling also runs only while the mouse owner (dword_4EA444) is the view, or the owner allows it (+0x40 bit 30, or
  +0x28 == -1). The engine only checks the popup.

### 10. A ground click with no point found. Severity: LOW
- Original: 0x485C5F-0x485CD5 sends packet 0x0E / 0x13 with 0x47B3F0's y even when it is 0.
- Engine: BattleMap.cpp:351-352 drops the click when wy == 0.

### 11. Cursor over the other side's unit in mode 0. Severity: LOW
- Original: 0x4971B7-0x4971C3 says the message was handled without setting a cursor, so the previous cursor stays.
- Engine: BattleMap.cpp:416-419 returns 0 (the normal cursor). It rarely differs, because the view's own mode 0 cursor
  is 0 with nothing selected.

### 12. Cursor modes 2-7 are missing. Severity: LOW
- Original: 0x485F60 has more cursor modes than the engine's 0 and 1. It reads the global mode dword_4EA43C:
  - modes 4, 5 and 6 make area cursors (0x4A13B0, 0x4A03A0);
  - mode 7 is cursor 0;
  - mode 3 is object placement (0x485A68-0x485C55, by dword_4F7428's type: packet 0x18 or a direct place; types
    0x1C3-0x1C8, 0x128 and 0x1BC-0x1BF take two clicks, 0x80A5 / 0x80A6).
  - The hotspot's cursor for modes 2 and above is 12 (0x497176).
- Engine: Mode() is 0 or 1 only (BattleMap.h:512).
- Probably the debug/editor tools, plus whatever sets modes 4-6 (unverified).

## Architecture misalignments

### A. No per-effect renderer state
- Original: effect drawing reads the type's +0xA1 every draw. The original's renderer is an object with its own level
  (+0x34), flags (+0x14 bit 0 blend, bit 1, bit 2 hidden) and depth (+0x26), which callers change after the start
  (0x483590, 0x48B2D0's fog level, 0x49C300 for the box corners).
- Fix: give ViewObject (or Effect) the renderer's fields: level, blend flag, hidden. Let the spawners set them, the way
  the original's callers do.

### B. Groups keep no +0x1D8 bit 3 or 4
- The engine keeps no "player's order" (bit 3) or "player's group" (bit 4) flag on a group. OrderGroupMove therefore
  cannot tell a player's order from an AI or automatic move (items 2 and 3).
- Fix: add both flags to the group state, set as 0x4812B0 / 0x481CA0 set them.

### C. The screen point is computed when needed instead of stored
- Original: the object's screen point is stored (+0x2C / +0x30) when it moves (0x47F480), and its hotspot window is
  created or removed by the fog (0x48A660 / 0x4405C0).
- Engine: computes the point on demand from the ground height and has no hotspot lifetime (items 4 and 5).
- Fix: a stored per-unit screen point and a `hasHotspot` flag, both set where the original sets them.

### D. The popup is a flat list
- Original: WPopupMenu holds a grid of rows keyed by command, with collapsed rows.
- Engine: a flat vector (item 6). The war map shares this.
- Fix: a shared popup class with the 10 x 8 grid.

## Provisional items resolved

- **The ACK flag's draw state.** The flag is GType 0x1A at level 0 (not +0xA1 / 4), opaque (blend bit cleared), at
  height 0. It is placed at the route's last node, or at the goal vertex + 2 when going straight. Seven frames at 0x80,
  gone after 13 updates. Only one shows per player order.
- **The hotspot's position** (BattleMap.cpp:301-303, "taken as centred on the screen point"). Centred is right
  (0x4406A0: x - w/2, y - h/2). The point itself is projected at ground + lift, less the type's +0x41.
- **Popup rows** (0x4331C0 / 0x433330). A row's key is the item's +0x28, the command (0x42DE00 stores arg 0 there).
  Rows collapse to their first item and open on hover.
- **Return to Warmap's condition** (0x485E5F-0x485EAB). The engine's condition matches: no selection (0x4391F0 false)
  and either side's count (0x47D610 / 0x47D660) at 0.
- **The arrow keys, PgUp/PgDn, Home, Tab, Backspace and digits** (0x485610). They match the engine's steps and
  rounding exactly. The digits build packet 0x12, which nothing handles.
- **Edge scroll's step** (0x484E95-0x484EC5). (W/2 - mx) / ((W/2) / 40), truncated toward 0. Matches.
- **Brackets and power bar** (0x497000). Corners at L+8 / R-8 and T+8 / B-8, the bar at ((L+R)/2, B-8), the frame
  as 0x496DE3 / 0x497290. These match apart from where the point is (item 4).
- **The minimap** (0x4868B0 / 0x486CB0 / 0x480070):
  - position: the view's width less the picture less the scroll border;
  - the rectangle's size, and its colour dword_4DAF9C;
  - dots rebuilt every 16th draw, in the colours dword_4DAFA0 / A4 (by 0x440700 == 1) and A8 (the other side's);
  - draw order: picture, dots, rectangle.
  All match. Not resolved: whether fogged objects send 0x8077 (dots).
- **The drag box's corners** (0x49C2B5-0x49C31E). GType 6 frames 0-3, each with renderer +0x14 bits 1 and 0 set (blend
  mode 2) and depth 0x5DC0. The engine draws them opaque.

## Missing

- **Mixed-selection common orders** (0x448500, 0x8130-0x8132 queries). Medium. See item 7.
- **The world's and the tool's popup items** (0x485EB0-0x485F0C: dword_4EA444's 0x8111 / 0x805C, then world +0x84
  0x805C). Low; contents not traced.
- **Cursor modes 2-7 and placement mode 3** (0x485A53-0x485C55, 0x485F60 cases 4-7). Low (likely debug/editor). See
  item 12.
- **Fog shading of visible enemy units** (0x48B311-0x48B325: renderer vt+0x18(ftol(fog x flt_4AEA68))). This is
  outside the UI and noted for the fog reviewer; I did not check whether the engine shades them.
- **Keyboard gating.** The battle view takes keys only with +0x80 set (0x485006), and a current tool (0x439230) gets
  each key first (0x485036). Neither is in the engine. Low.
- **Cursor animation** (0x40CDC0: the cursor's frame steps by 0x80 every main-loop pass, looping). Low: every battle
  cursor sprite I checked (h-move, h-attack, hselcurs, hnrmcurs) has 1 frame. Only `hwaitcur.spr` (GType 0xC, class 1)
  could animate.
