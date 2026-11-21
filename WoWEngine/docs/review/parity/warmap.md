# War map parity review (src/warmap/*, docs/WARMAP.md)

Scope: I read WarMap.cpp in full, plus the parts of WarMapAi.cpp, Production.cpp and BuildWindow.cpp that these
findings touch. I checked them against the original's draw (0x41D9B0, 0x41DE70, 0x42B850, 0x42BC90, 0x423040,
0x422C70, 0x49C3E0, 0x49CDB0), keys and scrolling (0x41D640, 0x41E000, 0x484DF0), world tick (0x41AB00), clock
(0x41F640, 0x41F6C0), shading (0x4192F0), economy demand (0x429640), owner (0x43CA40, 0x43C990), routes (0x424160,
0x425080, 0x4257C0, 0x425730, 0x425770), pods (0x41A330), events (0x41E4E0, 0x41E850, 0x41E960, 0x41EAE0, 0x41EB90,
0x41F010), InfoBox (0x432520), message list (0x420C10, 0x420CD0), menu (0x41D120), save (0x41AC40, 0x41AD80),
Resource view (0x41C2E0, 0x426140, 0x425E80, 0x426590, 0x426840) and the AI dispatcher (0x442EC0).

Most of the event handlers, the economy phases, the border and sector shading formulas, the route stepping, the
pods, the clock and the AI phase dispatch match the original exactly. The divergences are below.

---

## Visible bugs

### 1. The Resource view shows the wrong things. Severity: high
- **Original:** 0x41C2E0, mode 3 branch, 0x41C329-0x41C478. In the Resource view the sectors' counters are **not**
  laid out: there's no 0x43CB20 / 0x43CBB0 call in that branch. Instead, for each sector 1-30 and each of the side's
  three kinds, two widgets are made:
  - **"ROHS"** (0x426840 -> 0x426590), a bar chart. Sprite GType `0xB9 + kind`, cropped to a height of
    `ftol(spread supply (sector +0x80, dword_4EAE98) x 1/3)` (flt_4A9DA4), no taller than the sprite. It sits at the
    sector's waypoint (0x437380), offset by `kind x 32 - w/2 - 32` in x and `-8 - h` in y, at depth 6. A marker (GType
    0xBC, rect -2..0x12 by 3, depth 7) sits at `bar height - ftol(spread demand (+0x50, dword_4EAE68) / 3)`, frame 1
    when that goes below 0. Its tooltip is 0x426AE0 with "%s\n%0.2f %s %s\n%0.2f %s %s..." (0x4BA624).
  - **"RFHS"** (0x426140 -> 0x425E80), a row with one counter (GType 0x77, counter frame) for each facility of the
    kind's type. The facilities are taken from the sector's +0x5C list (+0x60 in network games). Finished ones are
    counted at +0x88 and unfinished ones at +0x8C, and the unfinished ones are drawn at 50%. Depth 6. Its tooltip is
    0x426430 (strings 0x514-0x517 and the %0.2f figures), and it has command 0x80A3.
- **Engine:** `WarMap.cpp:3078` (LayoutAll), `:1145-1158` (Draw). The Resource view lays out the HRES/ARES objects
  as ordinary counters through the clunting grid (FindCell resource mode). There are no bars, demand markers, facility
  rows or their tooltips.
- **Fix:** In view 3, skip the counter layout. Add per-sector ROHS and RFHS widgets that read the economy figures
  every update, with their tooltips and the 0x80A3 click. Trace 0x426AE0, 0x426430 and 0x42EF60 for the exact text and
  hit rects.

### 2. Counters have no production or idle icon, power bar or repair icon. Severity: high
- **Original:** 0x42B850 / 0x42BC90. An object with a factory (+0xB5 = `object+0x54 != 0`, set at 0x42B3AF) that has
  something queued (0x42A410: `factory+0x128 != 0`) and is a vehicle or a finished construction gets a second small
  sprite (+0xA0, GType 0x76) at the counter's bottom right (`x + cw - iw + 2`, `y + ch - ih + 2`), depth + 2.
  - **Busy:** the frame is the head entry's type's counter frame (`dword_51A11C[type x 0xB0]`). It blinks: phases 1-8,
    with 1 and 6 at 50% and 2-5 solid, then a pause of `(10 - ftol(eff x 10)) x 6` ticks, where eff = 0x460740 (the
    factory's efficiency).
  - **Queued but suspended:** frame 0x94, the idle icon.
  - **+0xA8:** frame 0x95 (repair) while 0x4299C0 is true. With it comes +0xB0, a power bar (0x449150 / 0x449330 /
    0x449340) at depth + 1.
- **Engine:** `WarMap.cpp:1138-1153` draws only the counter, the ring and the class 7 count.
- **Fix:** Add the +0xA0 icon with this blink rule (production is already in, so the data exists). Add the repair
  icon and bar when repair goes in.

### 3. The Human command building is the wrong type in ReinforcementSources. Severity: high
- **Original:** 0x43C990: `0x116 + (sector == 30 ? 0 : (-1 & 0x24 in AL only = 0xFFFFFF24))`, which is 0x3A (58,
  Command Post) outside London. Martian: 0x33E in sector 3, else 0x78.
- **Engine:** `WarMap.cpp:1471`: `faction == 0 ? (n == 30 ? 0x116 : 0x13A)`. 0x13A is 314, not 58. HasCommandPost
  (`:2832`) correctly uses 58, so the two disagree. Every Human sector except London reports `hq = false` to the
  battle map's reinforcement sources (`FrontEnd.cpp:1147`).
- **Fix:** Use 0x3A, or better, call HasCommandPost(n).

### 4. The mini-map lights the hovered sector, and its view outline is white. Severity: medium
- **Original:** 0x423040. The sector frame sprite (+0x9C, GType 0x83) is given the **selected** sector's number
  (world +0x60 -> +0x48), or frame 0 when none is selected. The hovered sector isn't used. The view rectangle (+0x90,
  made by 0x49C740 at 0x422E6C) is drawn in `dword_4DAF9C`, the side colour from 0x40B120: Human RGB6 (0x2F, 0, 0),
  a dark red; Martian (12, 42, 4), a green. Its size is fixed at creation: `miniW x 640 / mapW` by
  `miniH x 480 / mapH`.
- **Engine:** `WarMap.cpp:1186` uses `lit = hovered ? hovered : selected`. `:1188-1196` draw the outline in
  0xFFFFFF.
- **Fix:** Use the selected sector only, and the side colour.

### 5. The mini-map panel is a few pixels off. Severity: medium
- **Original:** 0x422EC9-0x422F12. x = parent width - 100 - 4 (dword_4BA05C) - 8 (Human only); y = parent height -
  170 - 4 - 8 (Human only). With a 640 x 480 parent that's (528, 298) for Human and (536, 306) for Martian. The frame
  then follows from 0x418C14.
- **Engine:** `WarMap.cpp:144` uses (525, 305) for both sides, taken from a screenshot.
- **Fix:** Use the formula. Unverified only in that I assumed the parent (+0x3C) is the 640 x 480 view.

### 6. Edge scrolling doesn't follow the game's rules. Severity: medium
- **Original:** 0x484DF0, the war map view's tick (vtable 0x4A9AD8).
  - The edge zone is 32 pixels (0x20), or 4 (dword_4BA05C) when byte dword_4EA364 is set (the full-screen flag from
    0x42C9B0).
  - When the mouse is outside the inner rectangle, the view moves by `(mouse - viewCentre) / ((viewW / 2) / Scroll
    Speed)`, which is `/8` at the default 40. That's in **both** axes at once, proportional to the distance from the
    centre: up to about 40 px a tick at the left or right edge. It's sent as 0x7A.
  - It only runs while this view is the hot window (dword_4EA444), or under the flag tests at 0x484E10-0x484E1F.
- **Engine:** `WarMap.cpp:24` uses a 4-pixel zone. `:107` and `:1063-1071` move a fixed `max(2, speed/5)` = 8 pixels
  along the edge's axis only.
- **Fix:** Port 0x484DF0. This settles the doc's "the game's scroll rules" *provisional*.

### 7. The rubber band is a white one-pixel box. Severity: low
- **Original:** tracker 0x49C3E0 -> 0x49C630 (line objects at depth 0x5DC0). It draws two-colour lines in
  `dword_4DAF9C` and `dword_4DAFB8`: Human (0x2F,0,0) and (0x28,0,0); Martian (12,42,4) and (5,39,4). There are
  12-pixel insets, and it's only drawn when the box is at least 0x17 tall (0x49C446).
- **Engine:** `WarMap.cpp:1163-1170` draws a 0xFFFFFF single box.
- **Fix:** Port 0x49C3E0 (its exact segment geometry is still to be read from 0x49C412-0x49C61E).

### 8. Route arrows are always solid. Severity: medium (the meaning of the flag is unverified)
- **Original:** 0x49CDB0 passes `var_34 = 4`, or 0xC when the arrow's +0x14 bit 0 is set, to the textured triangle
  rasteriser (0x410090 / 0x410190 -> 0x456DD0). The bit is set when a route sets off (0x425770 at 0x42579E, and
  0x4257C0 when loaded). It's cleared (0x425730(0)) while the mouse is on one of the route's counters (0x42BEB0 on
  0x20) and set again when the mouse leaves (0x8006 -> 0x425730(1)). Preview arrows (not set off) are drawn with 4.
- **Engine:** `WarMap.cpp:2740-2795` draws every arrow the same, opaque.
- **Fix:** Find out what bit 8 of 0x456DD0's flags does (very likely translucency) and draw set-off routes that way
  except while their counter is hovered.

### 9. The engine writes its own "Battle in X: won/lost" message line. Severity: low
- **Original:** nothing in the battle result path (0x41AED0, 0x419AA2) writes a line to the message list.
- **Engine:** `WarMap.cpp:1586` and `:1617` do.
- **Fix:** Remove it.

### 10. The engine draws a status strip that the original doesn't have. Severity: low
- **Engine:** `WarMap.cpp:1208-1226`, a darkened bottom-left strip with the sector name, button name or refusal.
- **Original:** I found nothing in the view's draw (0x41D9B0) for this. Button names are tooltips (0x807C).
- **Fix:** Remove the strip, or put it behind a [Modern] option.

### 11. The other side's counters undim when they shouldn't. Severity: low
- **Original:** 0x42BA20. For the other side, bit 0 (50%) is only ever **set** (intel under 0xA8). Nothing clears it
  while the sprite lives, so a counter whose intel is refreshed stays dim until its sprite is rebuilt.
- **Engine:** `WarMap.cpp:1143` recomputes the dimming every draw.
- **Fix:** Only clear it when the counter is rebuilt. Also, the original dims unfinished category-3 objects
  (resources), not only constructions (`k.kind == 1`), at 0x42B8C5-0x42B8D7.

### 12. The counter under the mouse isn't raised. Severity: low
- **Original:** 0x42B88C. A counter is drawn at depth 7 when it's selected or lit (0x440690) **or it's the hot window
  (dword_4EA444)**.
- **Engine:** `WarMap.cpp:1154-1158` raises only selected and in-band counters.
- **Fix:** Raise the hovered counter too.

---

## Behaviour divergences

### 13. Destroyed constructions stay in `finished[]`. Severity: high
- **Original:** 0x43C990 looks the command building up in the sector's live construction lists every time it's asked.
  0x423B00 / 0x424000 count the live finished facilities the same way.
- **Engine:** `finished[sector]` (`WarMap.h:422`) is only ever appended to (`WarMap.cpp:53`, `Production.cpp:253`).
  ClearSide (`:1742`), RemoveObject (`:1722`), the `lost` list in FightBattle (`:1532`) and AiApplyUpgrade never
  remove or retype entries.
- **Consequences:** After a command post or facility is destroyed, HasCommandPost stays true. That affects route
  refusals, border colours, the economy supply, the sector paths and the CancelAll test at `:1590`. The engine also
  keeps counting the facility in FacilityOutput and ResourceGain.
- **Fix:** Derive `finished` from the live counters (with their refs and progress) when it's needed, or keep it in
  step wherever objects are removed or retyped.

### 14. Units in transit restart from their origin after a load. Severity: medium
- **Original:** the route reader 0x4257C0 reads origin, destination, +0x5F, +0x5E, +0x44, the count, +0x34 and the
  path. Its unit list (+0x64, vtable +8) brings back each entry's progress (+0x24, +0x38 / +0x3C are read at
  0x4258F8-0x42591C to work out the arrow's trim). +0x6C is worked out again.
- **Engine:**
  - On save (`WarMap.cpp:844-857`), the route is written with `progress` = 0 and no entry state.
  - On load (`:769-775`), each unit is put back at the origin and `SendTo` plans a **new** route with the A*, from
    the start.
- **Fix:** Save and restore each RouteEntry's +0x24/+0x30/+0x34/+0x38/+0x3C and the route's +0x34. Restore the
  counter on its curve without planning again. Which bytes of the WMOB part hold the entry state is still unverified.

### 15. The saved view mode is ignored on load. Severity: medium
- **Original:** 0x41AC40 reads the WMAP 4th value into world +0x70, and the set-up (0x418CA4-0x418CAA) calls
  0x419810 with it.
- **Engine:** the value is written (`WarMap.cpp:813`), but the constructor (`:104-115`) never reads `warMap[3]`, so a
  game always reopens in the Unit view.
- **Fix:** `SetView(clamp(warMap[3], 1, 3))` in the constructor.

### 16. The Warmap menu leaves out "View map" when something is selected. Severity: medium
- **Original:** 0x41D120. After Cancel:
  1. If the hot window answers 0x8111, it adds its items, and `var_11` is set.
  2. Otherwise, if the player has a current object (0x439230), it adds its items **without** setting `var_11`
     (0x41D1F8 jumps past 0x41D287).
  3. Otherwise, the own sector under the mouse is selected (0x419260), the player's command 2 is sent, it adds its
     items, and `var_11` is set.

  Whenever `var_11` is still clear, the world's selected sector adds its items (View map). So a current object gets
  its Build/Suspend items **and** View map.
- **Engine:** `WarMap.cpp:2167-2192` adds View map only when there's no selection.
- **Fix:** Follow the flag logic above. Separately, the engine treats the first selected counter as the "current
  object" (*provisional*); 0x439230 still needs tracing.

### 17. The ending waits a guessed 5 seconds. Severity: low
- **Original:** 0x41F400 waits for the victory sound's length plus 30 ticks.
- **Engine:** `WarMap.cpp:1966` uses `endingDelay = 30 + 150`.
- **Fix:** Use the length of sound 0x13E.

### 18. Unit view shading counts units that have set off. Severity: low
- **Original:** dword_4F7650[s] goes up and down with the sector's object container (0x43C790 / 0x43C7C0). A unit on
  a route has left that container.
- **Engine:** `WarMap.cpp:463-464` counts every counter whose `sector == s`, which includes travelling ones (they keep
  their origin until Arrive).
- **Fix:** Skip `Travelling()` counters.

### 19. A route's first-hour jump happens in every view. Severity: low
- **Original:** 0x4250E4-0x425132. The first-hour placement (0x4406A0 and 0x4208B0 at 1.0) only happens for objects
  whose +0x28 is set and only when the view is the Unit view (0x42A430 == 1).
- **Engine:** `WarMap.cpp:915-923` does it always.
- **Fix:** Gate it on the Unit view.

### 20. Battle pulse timing. Severity: low
- **Original:** 0x41DCD0 / 0x41DED1. The pulse uses the view's own draw counter (+0x8C, +1 per draw), which is reset
  to 0 only when it passes 22 while a battle sector is being pulsed.
- **Engine:** `WarMap.cpp:490` uses `animation % 23`. It looks the same at a steady 30 fps; this is noted for
  completeness.

---

## Architecture misalignments

### A. One `Counter` stands for both the original's counter and its objects
The original has one WWarMapObject (0x42B2F0) per object, each with its own child sprites (+0x9C counter, +0xA0
production icon, +0xA4 ring, +0xA8 repair, +0xAC count text, +0xB0 power bar) and its own route entry. The engine has
one `Counter` per type-and-sector group, with `refs` pointing at save records. That's why:
- per-object state (the factory icon, repair and intel dimming) has no home;
- shared state is duplicated in parallel structures (`finished[]`, `factories`, `destroyed`), which is how finding 13
  happened;
- routes hold one counter, not a group with per-entry progress (finding 14).

**Fix:** a WarMapObject per saved object (owning its factory, progress, intel and route entry), with the counter as
the drawing of one object, or of a class 7 group. Then derive the per-sector views (finished constructions,
command building, counts) from the sector's lists, as 0x43C990 and 0x421C10 do.

### B. The views aren't separate kinds of child object
In the original the Resource view builds different child objects (ROHS, RFHS) instead of activating counters. The
engine's `viewMode` only filters `Counter::kind`. Supporting finding 1 needs per-view widget sets, or at least a
Resource-view draw path that doesn't go through counters.

### C. Drawing is immediate rather than a depth-sorted display list
The original adds sprites to the canvas with depths (counter 4/7, ring +2, count +1, icons +2, ROHS 6/7, rubber band
0x5DC0, popup 0x61B2, InfoBox 0x65FE). The engine draws in code order with two hand-made passes. Today that's fine
for the counters, but adding the icons, the bars and the hover raise is easier with a small depth-sorted queue.

---

## Provisional items resolved

- **Which construction float is which** (`WarMap.cpp:49`, docs "*provisional* as to which is which"). The writer
  0x42AA90 and reader 0x42AAC0 put +0x78 first, then +0x7C. So `value[0]` = +0x78 (the whole time) and `value[1]` =
  +0x7C (done), and `finished = value[1] >= value[0]` (0x42AA70) is right. The engine's progress `value[1]/value[0]`
  is also right.
- **The scroll rules** (doc "Scrolling", `WarMap.cpp:107`): 0x484DF0. See finding 6.
- **The rubber band's look** (`WarMap.cpp:1163`, doc *provisional*): two-colour side-coloured lines (0x4DAF9C /
  0x4DAFB8) at depth 0x5DC0, only when at least 0x17 tall. See finding 7.
- **The mini-map's place** (doc "WoWRevived's screenshot puts it 115 in..."): 0x422EC9-0x422F12. See finding 5.
- **The mini-map's lit sector and outline:** the selected sector only, and the outline in the side colour. See
  finding 4.
- **The other side's counters at 50% while intel < 168** (doc "not in yet"): confirmed (0x42BA20, `cmp [+3C], 0A8h`),
  but the original only ever sets the bit. See finding 11.
- **"Still to come: power bar, production blink, idle icon, repair icon"**: the rules are traced in finding 2.
- **The time each AI phase moves on:** 0x442EC0 runs exactly one phase per hour (ebx = 1). Phases 0, 2 and 4 return
  true to skip one. The engine's AiTurn (`WarMapAi.cpp:174-209`) matches.
- **The second InfoBox line:** 0x432520 makes the second text (+0x88) at x = the backdrop's centre without
  0x40F9F0(2). The first text gets alignment 2; the second keeps the constructor's default +0x44 = 6. Whether 6 also
  centres isn't verified; the engine centres it, as the doc says screenshots show. The gap is "first text's height +
  0x10" (0x4327EA-0x4327FA), as the engine has it.

---

## Missing

1. The Resource view's ROHS bars, demand markers and RFHS facility rows, with their tooltips and 0x80A3 (finding 1).
2. The production, idle, repair and power-bar icons on counters (finding 2).
3. Route progress across save and load (finding 14).
4. Restoring the view mode from the save (finding 15).
5. The unit tooltip's task lines (0x428D70: "Currently building:", "Days to completion:", "Efficiency:", "Idle",
   "Construction suspended", 0x48E-0x493). The doc already lists these as missing.
6. Cancel Move (0x80A9 on an unset-off route) and an object's Information (0x440300) in the Warmap menu. The doc
   already lists these.
7. The world's per-tick gating. 0x41AB00 skips the slides, battle detection and economy while the war map is hidden
   (+0x18) or paused (+0x44), or once the game is over. The engine checks only `ending` (`WarMap.cpp:1028`); it should
   also check whatever the engine has for hidden and paused.
8. When a sector changes hands, 0x43CA40 clears its first three +0x38 figures (dword_4EAE50, 0x43CAE5) and calls
   0x418890. The engine only rebuilds the paths (`WarMap.cpp:1691`). The effect is small, because phase 2 recomputes
   demand within five ticks.

Not verified in depth: BuildWindow.cpp (0x460B00-0x462EE0), the bodies of the AI phases (0x443030-0x444C10), the
calendar wheel tables and the A* refusal codes. They were spot-checked only, and nothing wrong was found.
