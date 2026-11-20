# Part 04: 0x422860 to 0x42DA10

The listing is `pieces/part04.txt`. It holds 199 functions, all ordinary MSVC code: SEH frames, `__thiscall`, vtables in `.rdata` 0x4A9BF8 to 0x4A9FCC, and `[Tweak]` static initialisers in `.data` 0x4B816C to 0x4B81C8.

Below, "the world" means WWarMapWorld, whose global is `dword_4E9B70`. "Sector object" means the 30 per-sector objects that `sub_419240(n)` returns. "WMOB" means a war map object: a unit group, a construction or a resource facility.

**Mislabelled FLIRT names (not real library functions):**
- `__RTC_NumErrors` / `__RTC_NumErrors_0` at 0x42394F / 0x423955 are cases of a switch inside 0x423920.
- `std::strstreambuf::~strstreambuf`, `std::ios_base::~ios_base`, `std::locale::_Locimp::~_Locimp` and `codecvt_base::do_always_noconv` are the game's own base-class destructors and stubs. The real names are 0x435690 (WCmdTarget-style base dtor) and the trivial stubs.

**Hand-written assembly:** none in this piece.

---

## 1. Overview

The piece is the second half of the war map: everything after the map view itself, which lives in part 03. In address order:

1. **Battle map hand-off helpers.** These count units and buildings in a WMOB list and seed the battle map's group manager (`dword_4F8738`) with a sector's command post and vehicles.
2. **Mini-map view and mini-map frame.**
   - "WarMap miniMap view" 0x422C70: clicking or dragging recentres the map (command 0x7A).
   - "WarMap miniMap frame" 0x423420: the three view buttons, plus a **Research button** (GType 0x341 `rese-but.spr`, control id 0x7D) that the docs don't mention yet.
3. **Resource economy.**
   - Resource facility class: vtable 0x4A9C98, handler 0x423B00.
   - `[Tweak] "Resource Drop Off Rate"`, default 3.0, and the diminishing-returns output formula (0x423F80 / 0x424000).
   - The Resource View hot spots: "RFHS", the facility stacks with their tooltip, 0x426140; and "ROHS", the produced/used bars, 0x426840.
4. **War map routes.**
   - WarMapRoute: constructors 0x4240D0 / 0x424160, handler 0x424F00.
   - The arrow curve: 0x424A40, a midpoint subdivision with tangent bending. It's fully decoded below.
   - Per-hour movement (0x425080), save/load, the route entry list "WarMapRouteEntryList" (0x427A80) and "RouteEntry" (0x42AFA0 / 0x42AFE0).
   - The A\* step cost and refusal codes (0x42ABD0; already in WARMAP.md).
5. **"WM Groups".** The war map unit group class, 0x425B80. It takes network/AI packet type 8, "move group to sector".
6. **Calendar View.** 0x426C10 to 0x427A80, mostly documented already. New here: the odometer digit-roll animation tables and the "Advance time" button.
7. **Sector cell grid and the counter "clunter".** The slide animation for counters is 0x428620 to 0x4287A0.
8. **Message Item.** One line of the message list, 0x4287C0 to 0x428B10.
9. **The WMOB class hierarchy:**

   | Class | Constructor | vtable | Handler |
   |---|---|---|---|
   | WMOB base | 0x428BB0 | 0x4A9E80 | 0x428D70 |
   | Construction | 0x42A450 | 0x4A9ED8 | 0x42A4B0 |
   | Resource facility | 0x4239C0 | 0x4A9C98 | 0x423B00 |

   Units come from 0x4A3BD0, in another piece. These classes carry tooltips, hourly upkeep (resource consumption), **repair** (repair vehicles and workshops), construction progress and completion, sector entry/exit, save/load ('WMOB') and battle-map unit lists ("BMOList").
10. **WUnitHotSpot, the counter widget.**
    - 0x42B2F0 creates it.
    - 0x42B850 draws it: power bar, production icon, repair icon, "%d/%d" count, enemy-intel fading.
    - 0x42BEB0 is its message handler: Shift-click multi-select, cursors, route highlight on hover.
    - 0x42C340 is a pool allocator of 128 objects.
11. **Win95DeskTop.**
    - `[Screen]` settings.
    - Window creation and mode change (0x42C9B0).
    - The WndProc (0x42CD40): Alt+Enter, Pause key, activate/deactivate.
    - Fixed-aspect resizing (0x42D660).
    - **Demo playback and recording of input** (`playback.%d`, `Playback.1`; 0x42D2D0 / 0x42D520).

---

## 2. Subsystems

### 2.1 Battle map hand-off helpers (callers are in the battle map set-up, 0x439770)

| Addr | Name (proposed) | What it does |
|---|---|---|
| 0x422860 | `WMOList::CountUnits` | Counts the entries of the list at +0x20 whose unit type has category (unit table +0x10, `sub_440700`) == 1 (vehicles). |
| 0x4228B0 | `WMOList::CountBuildings` | Counts the entries whose category is 2 or 3 (constructions, resource facilities). |
| 0x422910 | `WMOList::PlaceOnBattleMap(arg)` | First looks for the sector's command building. That's type 830 (Martian landing site) in sector 3 or 120 (Communications Centre) elsewhere, and 278 in London (30) or 58 (Command Post) elsewhere. If one is found, it allocates a 0xB8-byte battle group (`sub_484120`), stores it in the WMOB's +0x6C and adds it to `dword_4F8738` (`sub_483F90`). It then does the same for every category-1 WMOB. |
| 0x422AB0 | `WMOList::MostDamaged` | Over every WMOB's BMOList (+0x4C), finds the battle unit with the lowest `hp (vfn +0xB4) / maxHP (unit table +0x4A)`, skipping units already flagged `+0xB8 & 0x1000` (being repaired). Returns it, or 0 when all are at ≥ 1.0. |
| 0x422BA0 | `WMOList::MarkAllSeen` | Sets every WMOB's +0x3C (intel age) to 0x150 (336 hours = 14 days). Called from the world view set-up (0x418980). |
| 0x422BE0 / 0x422C10 | std::list constructor / insert | Library inlines. |

### 2.2 Mini-map view and frame

| Addr | Name | What it does |
|---|---|---|
| 0x422C70 | `WarMapMiniMapView::ctor` | Window "WarMap miniMap view", id 0x6B, depth word 0x5DDE. It sets up four things:<br>• +0x98: the highlight object for GType 0x82 (`sub_49BE70`).<br>• +0x94: the mini-map with highlight, GType 0x7F (`sub_49C8D0`).<br>• +0x9C: the sector frames, GType 0x83.<br>• +0x90: the view rectangle (`sub_49C740`), sized `screen w,h × minimap w,h / map w,h` (`sub_4191F0` / `sub_419200` = map width / height), colour `dword_4DAF9C`.<br>Position: bottom right, 4 px in (`dword_4BA05C`), with an extra 8 px in Human games. |
| 0x422F60 | dtor | |
| 0x423040 | `Draw` (vtable slot 9) | If `byte_4E9F48` is set, clears it, calls `sub_40AEB0` (full redraw) and sets flag 0x10 on the mini-map. Then draws the mini-map, the sector frame (frame = the selected sector, `[world+0x84]+0x60 → +0x48`) and the view rectangle at +0x88 / +0x8C. |
| 0x423130 | `OnMessage` (slot 7) | **0x201** (left button down): converts the click to a map position (`x × mapW / miniW − screenW/2`, likewise y), sends itself **WM_COMMAND 0x7A** with `lParam = x | y<<16`, and captures the mouse (`sub_42E9B0(1)`).<br>**0x200 / 0x800A** (move or drag) with button 1 held: the same 0x7A.<br>**0x202**: releases the capture.<br>**0x8004** (the view moved, `lParam = mapX | mapY<<16`): recomputes the rectangle's offset +0x88 / +0x8C, clamped inside the mini-map with a 2 px margin.<br>**0x20** (set cursor): cursor 0. |
| 0x423420 | `WarMapMiniMapFrame::ctor` | "WarMap miniMap frame", id 0x9B. +0x88 is the frame sprite, GType 0x80. Creates **the Research button** with `sub_45BD70(id 0x7D, parent, x, y)`: GType 0x341 `rese-but.spr`, label font 0x72, at (0x48, 5) in Human games and (0x3A, 0xC) in Martian ones. Then the three view buttons (`sub_4333B0`, command 0x811F, ids 0x94 / 0x93 / 0x92, strings 0x47B / 0x47C / 0x47E, already documented). Finally it presses the button for the current view mode with 0x111 / 0x8033. |
| 0x4237C0 / 0x4237D0 | dtor / `Draw` | The frame sprite, then the children (`sub_42E150`). |
| 0x423820 | `OnMessage` | **0x111** with id 0x811F: sends itself 0x8034 (pressed) and passes the command on.<br>**0x8034**: also sets it pressed.<br>**0x8006**: `sub_42EBE0` on the desktop.<br>**0x20**: cursor 0. |
| 0x4238D0 / 0x4238F0 / 0x423910 | A subclass of `sub_49C8D0` (highlighted sprite), vtable 0x4A9C78 | Made by the world view set-up (0x418980 + 0x144). Probably the big map (0x7D) with its sector-map highlight (0x7E). *Guess.* |

### 2.3 Resource economy (sector table 0x4EAE1C, 0xC0 bytes a sector)

**Resource kinds.** There are six, k = 0 to 5: Human Iron/Steel, Coal, Oil (0 to 2) and Martian Blood, Copper, Heavy elements (3 to 5). Their names are TEXT ids 0x214 + k (table `dword_4BA650`).

**Facility type to kind** (the switch at 0x423920 and its copies):

| Type | Name | Kind |
|---|---|---|
| 47 | Steel Refinery | 0 |
| 48 | Coal Mine | 1 |
| 49 | Oil Refinery | 2 |
| 108 | Human Farm | 3 |
| 109 | Copper Forge | 4 |
| 110 | Heavy Element Plant | 5 |
| any other | | 6 (none) |

**Output formula.** With r = `[Tweak] "Resource Drop Off Rate"` (0x423F40, default **3.0**, value at `flt_4EA104`), B = the sector's deposit for kind k (`+0x04 + 4k`) and n = the number of finished facilities of kind k (`+0x1C + 4k`):

```
output(n) = n * r * B / (n + r - 1)
```

With r = 3, one facility gives B, two give 1.5 B and three give 1.8 B.

| Addr | Name | What it does |
|---|---|---|
| 0x423920 | `ResourceKindOfType(type)` | The table above. |
| 0x4239C0 | `WResourceFacility::ctor` | The construction base (0x42A450), then vtable 0x4A9C98. Made by `sub_446950` (the object factory). |
| 0x423A00 | dtor | If finished, decrements the sector's facility count for its kind (`sub_43CC70`). |
| 0x423B00 | `OnMessage` | **0x80AC** (construction completed):<br>• Heals the first battle unit to max (`+0xC4 = maxHP`).<br>• Increments the facility count (`sub_43CC40`).<br>• Unless loading (`byte_4D2554`): world `sub_418890` (sends 0x111 / 0xB7 to all 30 sectors), and sets +0x78 and +0x7C to 1.0.<br>• If the view mode equals this object's category and it's the player's (or `dword_54604C`): posts **0x8145** (counter refresh) with the sector to the counter manager (`world+0x80`).<br>**0x111 / 0x803A** (entered a sector): the base handler, then increments the count if finished.<br>**0x111 / 0x803C** (recompute rate): `+0x80 = int(hp / maxHP) * r * B / (n + r - 1)`. The integer division means a damaged facility produces **0**. When the sector has no command building or the facility is unfinished, it's 0. |
| 0x423F80 | `MarginalOutput(sector, kind)` | `output(n+1) − output(n)`: "Next facility will add". |
| 0x424000 | Same, with a shortcut | Returns B when n == 0. Used by the Resource View sector shading (`sub_4192F0`). |
| 0x426140 | `RFHS` hot spot ctor (sector, slot) | "Resource Facility Hot Spot", base `sub_4335F0`. +0x80 = side base (0 or 3; it flips when `dword_54604C > 1`, the network case: *unclear*), +0x84 = facility type (`0x2F + 0x3D·side + slot`), +0x90 = sector, +0x94 = slot (0 to 2). It caches produced (`+0x7C + 4k`, `dword_4EAE98`), used (`+0x4C + 4k`, `dword_4EAE68`) and marginal output (+0xA0). |
| 0x425E80 | `RFHS::Rebuild` | Walks the sector's resource list (+0x5C, or +0x60 in network games). For each WMOB of this type it makes a GType 0x77 counter whose frame is the unit table's +0x2C. Finished ones count in +0x88 and unfinished in +0x8C; unfinished are drawn at 50%. The hot spot rectangle is centred at `waypoint.x + slot·32 − w/2 − 32`, `waypoint.y + 8`. |
| 0x4263B0 | `RFHS::Draw` | Stacks the counters, each offset by `[Tweak] "Res Stack XO"` (0x426330, default **4**) and `"Res Stack YO"` (0x426370, default **16**). |
| 0x426430 | `RFHS::OnMessage` | **0x8083** (0x20 + 0x805C + 0x27) for this sector: `Rebuild`.<br>**0x807C** (tooltip, lParam == 1): sprintf into `byte_59EF50`: `"%s\n%0.2f %s %s\n%0.2f %s %s\n%s %0.2f %s"` = name, produced, "tons p/w" (0x517), "produced" (0x514), used, "tons p/w", "used" (0x516), "Next facility will add" (0x515), marginal, "tons p/w". |
| 0x426560 | static init | Copies the default hot spot rectangle 0x4E9658 to 0x4EA170. |
| 0x426590 | `ROHS::Rebuild` | The Resource View bar for one kind in one sector: GType 0xB9 + slot (`Steel.spr` / `Coal.spr` / `Oil.spr`, or `blood` / `copper` / `Helemen.spr`).<br>• Shown height = `produced / 3` (`flt_4A9DA4` = 1/3), clamped to the sprite height. The bottom part shows, via `sub_40EDF0`.<br>• Marker GType 0xBC `coalind.spr` at y = `barH − used/3`. Frame 1 when negative (a deficit), else 0.<br>• Placed at `waypoint + (slot·32 − w/2 − 32, −8 − barH)`. |
| 0x426840 | `ROHS` ctor (sector, slot) | Same side logic as RFHS. |
| 0x4269C0 | `ROHS::Draw` | Rebuilds when produced or used changed. The bar is drawn solid only when its slot equals `dword_4E9F40` (the selected resource kind), otherwise at 50%. |
| 0x426AE0 | `ROHS::OnMessage` | **0x807C**: the same tooltip as RFHS.<br>**0x202** (click): posts **0x8113** with the slot to the counter manager (select that resource kind).<br>**0x20**: returns 1. |

**Hourly consumption** (0x429640, called on message 0x8143):
- A working factory (`+0x54 → +0x12C`) or a busy repairer (+0x50) uses the unit table's "working" rates +0x6C / +0x70 / +0x74.
- A unit group adds `n × moving rates` (+0x60 / +0x64 / +0x68) while on a route, in the route's current sector (`sub_424E90`), or `n × 0.5 ×` the same rates when idle.
- The total is added to the sector's "used" through `sub_43CCA0(category, a, b, c)`.

### 2.4 War map routes (WarMapRoute, vtable 0x4A9CC8)

**The route object:**

| Offset | Field |
|---|---|
| +0x1C | id (`dword_4EA108`++) |
| +0x20 | **destination** sector object |
| +0x24 | **source** sector object |
| +0x28 / +0x2C | curve points, 12 bytes each: x, y (float), heading (int, 4096 a turn); memory block "RArrow" |
| +0x30 | age in hours (0 = not started) |
| +0x34 | entry count |
| +0x38 | point count = `(nodes − 1)·(2^k − 1) + nodes` |
| +0x40 | `2^k`; +0x44 = k = 5 |
| +0x48 / +0x4C | node count and node sector indices |
| +0x50 | "naval" flag: the first unit's type has unit table +0x52 == 5 or 6, so it uses the **sea waypoint** (*guess*: boats and fliers) |
| +0x54 | arrival text (font 0x71, "--:--") |
| +0x58 | RTypeArrow |
| +0x5C | shown |
| +0x5D | route found |
| +0x5E | **attack**: the destination is held by neither the group's side nor no one |
| +0x5F | visible (drawn) |
| +0x60 | interrupted |
| +0x64 | WarMapRouteEntryList |
| +0x68 | total length |
| +0x6C | hours to arrive |

**Sector waypoints:** 0x437380 returns the land waypoint (`0x4BAE68 + 32n`). **0x4373A0 returns the second point, `0x4BAE70 + 32n`.** This is the sea or alternative waypoint that WARMAP.md lists as "not worked out". The route uses it when +0x50 is set, or when a link's land crossing point is (0, 0).

| Addr | Name | What it does |
|---|---|---|
| 0x423F40 / 0x424090 / 0x425B40 | Tweak / stats statics | "Resource Drop Off Rate", plus `[Stats] "Routes left"` (`dword_4EA124`) and `"WM Groups"` (`dword_4EA13C`) debug counters. |
| 0x4240D0 | `WarMapRoute::ctor()` | For loading. +0x44 = 5, +0x5E = 1, +0x30 = 2. |
| 0x424160 | `WarMapRoute::ctor(dest, source, group, show)` | Sets +0x5E (attack) as in the table. Runs the A\* (`sub_44EE70(0x4BD260, 31 nodes, cost object 0x42AB70, target, 1000 steps)`, then `sub_44EFA0`). Valid if found; **an attack route must have exactly 2 nodes (adjacent)**. Copies the nodes, builds the curve (`sub_4244E0`) and, for attack routes, computes the ETA = the max over the group's units of `length / (speed(+0x54) × 0.05)` and the label (`sub_41B9A0` → `sub_40FA90`). |
| 0x4244E0 | `BuildCurve` | Counts the route on the destination: `dest+0x78`++ for attack routes, else `dest+0x74`++. Fills the control points every `2^k`: the first node = the link's leaving point (link +4, +8; the sea waypoint if naval or zero); middle nodes = land waypoints; the last node = the land waypoint (the sea one if naval or zero). Then subdivides (0x424A40) and sums the length +0x68 between waypoints. If visible: **attack routes** get the text label (+0x54) and arrow style `side == Martian ? 2 : 0`; **other routes** get no label and style `1 / 3`. The RTypeArrow is `sub_49CAC0(points, count, 2^k)`. In Unit View the route shows (`sub_435560`). Creates the entry list (+0x64). |
| 0x4248B0 | dtor | Undoes the +0x74 / +0x78 counts, frees things, `"WWarMapRoute::~WWarMapRoute"`. |
| 0x4249B0 | `SectorOfEntry(group)` | The group's next node sector, or the source while not started. |
| **0x424A40** | `SubdivideCurve` | **Exact algorithm** (heading H in 4096ths of a turn from `sub_43FDA0(x0, y0, x1, y1, &H, &len)`; `sub_43FE90(a, d, &x, &y)` = d·(cos, sin)(a·2π/4096); `sub_43FF30` = signed angle difference in −2048..2047). For step = 32, 16, 8, 4, 2 (k = 5 passes):<br>(a) For every i on the stride, point[i].H = heading(i → i+step).<br>(b) For each midpoint m = i + step/2, start with M = (P[i] + P[i+step]) / 2, and L, H = the segment's length and heading. If i > 0: `o1 = tan(diff(H, P[i].H)·2π/4096)·L·0.5`, clamped to ±L/2, and `v1 = polar(H − 0x400, o1)`. If the segment isn't the last (`i < count − 2·step`): `o2` likewise from `diff(P[i+step].H + 0x800, H + 0x800)`, and `v -= polar(H − 0x400, o2)`. Then v /= (number of terms) and `P[m] = M + 0.4545…·v` (`dbl_4A9D20` = −5/11, subtracted).<br>Finally every point's H = heading(P[i−1] → P[i+1]); the first copies the second's, the last copies the one before. |
| 0x424E90 | `CurrentSectorOf(group)` | The node sector at the entry's segment index. |
| 0x424F00 | `OnMessage` (slot 7) | **0x802D** (an hour passed, wParam = hours) → `Advance`. **0x111 / 0x80A9** before it starts (+0x30 == 0) → `RemoveGroup(lParam)`. |
| 0x424F50 | `RemoveGroup` | Sends the group 0x111 / 0xAD (back to the source), clears its +0x58 and state (+0x70 = 0), removes the entry and refreshes the source sector (`sub_43CB60`). If the route is now empty, deletes it from the route manager (`world+0x90 → sub_41FBD0`). |
| 0x425000 | `WaypointDistance(i, j)` | |
| **0x425080** | `Advance(hours)` | +0x30++. On the first hour it moves every counter to the start plus its formation offset (`sub_4406A0`) and registers it with the counter manager (`sub_4208B0`, scale 1.0).<br>**Interruption:** if the route is neither an attack nor already interrupted, every node from the slowest entry's segment on is checked. A node is bad if the sector is flagged +0x124, or is held by someone else and isn't the destination. Then +0x60 = 1 and **event 0x0B TravelInterrupted** (`sub_41E5B0`) is raised with the first group's id.<br>**Per entry:** `speed = remDist / remHours` (+0x30 / +0x34); `remHours -= h`; `segLeft (+0x38) -= speed·h`. When `segLeft <= 0`: index++ and the "just arrived" flag (+0x28) is set. If at the last node, or the group's +0x44 is set, or interrupted: the group **arrives** (0x111 / 0xAD with that node's sector, `+0x58 = previous node`, state 0, that sector refreshed, entry removed). Otherwise the next waypoint-to-waypoint length is added.<br>**Position:** `frac = 1 − segLeft / segLen`; the flag is cleared once frac ≥ 0.5; `idx = seg·step + int(frac·step)`; linear interpolation along the curve plus the entry's offset.<br>**Arrow trim:** the smallest idx over all entries goes to the arrow's +0x5C, so the arrow shrinks behind the leading units.<br>**When empty:** if not an attack, **event 3 ArrivedInSector** for the destination; refresh, delete. |
| 0x4254E0 / 0x425510 | `Show` / `Hide` (slots 5 / 6) | Visible routes only; passed on to the entries. |
| 0x425530 | `DrawArrow(target)` | Draws the arrow and label if shown (called by the route manager, 0x41FCA0). |
| 0x425570 | `AddEntry(group)` | `new RouteEntry(group, this, index)`. |
| 0x4255F0 | `SetAllHours(h)` | Every entry's +0x34 = h, +0x6C = h, and the label. Used by the route manager at 0x41FCF0 so that all groups arrive together. |
| 0x4256E0 | `MaxHoursLeft` | |
| 0x425730 | `SetArrowTranslucent(bool)` | Arrow flag bit 0. A counter's hover makes it solid (0) and leaving makes it 50% again. |
| 0x425770 | `Start` | +0x5C = 1; `sub_428020` (counters to the start, sent 0x111 / 0xAD with sector 0, i.e. "in transit"); entries' state = this route (`sub_427FD0`); source refreshed; arrow at 50% and redrawn. |
| 0x4257C0 / 0x425A60 | `Load` / `Save` | **Save format:** source idx, dest idx, show (+0x5F), attack (+0x5E), k (+0x44), nodes, entry count, nodes[n], the naval byte, then the entry list. Load rebuilds the curve and arrow trim. **Load quirk:** +0x6C is recomputed as `dist(node0, node1) / (speed × 0.01)`, not ×0.05 over the whole length (looks like a bug in the original). |
| 0x427A80 to 0x4280C0 | `WarMapRouteEntryList` (vtable 0x4A9E30) | A std::list at +0x24 with its count at +0x28.<br>Methods:<br>• 0x427C30 / 0x427CD0: show / hide every group.<br>• 0x427C80: re-register the counters (`sub_420540`).<br>• 0x427D10: add an entry.<br>• 0x427D60: remove an entry.<br>• 0x427E10: count.<br>• 0x427E20: pass a message on.<br>• 0x427E80 / 0x427F70: load / save.<br>• 0x427FD0: set the groups' route.<br>• 0x428020: send the counters to the start at `"Clunting Speed"`.<br>• 0x4280C0: the smallest segment index. |
| 0x42AFA0 / 0x42AFE0 | `RouteEntry` ctor (load) / ctor(group, route, index) | +0x18 = index, +0x1C = group, +0x20 = route, +0x24 = segment, +0x28 = arrived flag, +0x2C = speed (`unit +0x54 × 0.05`), +0x30 = remaining length, +0x34 = hours, +0x38 / +0x3C = segment left / length.<br>**Formation:** `off.x = (idx % W − W/2)·S`, `off.y = (idx / W − W/2)·S`, with `[Tweak] "Route clunting width"` W = **4** (0x42AF20, `dword_4BAA14`) and `"Route clunting spacing"` S = **4** (0x42AF60, `dword_4BAA18`). |
| 0x42B0E0 / 0x42B150 / 0x42B250 | dtor / `Load` / `Save` | Save: group type, the group object (`sub_446950` factory on load), segment, index, five floats, flag. |
| 0x42AB70 / 0x42ABC0 / 0x42ABD0 | The A\* cost object (vtable 0x4A9F10) | Its cost function 0x42ABD0 is documented in WARMAP.md (refusal codes 0 to 8, `dword_4EA198`). It counts units and boats with `dword_4F86B8[s]` / `dword_4F76F0[s]`, which the WMOB message 0x80B9 increments (see 2.6). |
| 0x42AAF0 / 0x42AB30 | Tweaks | "Max units in sector" 15, "Max boats in sector" 5. The listing's comment says "Limit 255" (patched). |

### 2.5 WM Groups (vtable 0x4A9D30, base `sub_447DC0`)

| Addr | Name | What it does |
|---|---|---|
| 0x425B80 / 0x425BD0 | ctor / dtor | `dword_4EA13C` ± 1 (stats). |
| 0x425BF0 | `OnPacket` | Packet byte +2 == **8** ("move to sector"; dword +4 = sector) → `MoveTo`. |
| 0x425C30 | Passes on to `sub_4482D0` | |
| 0x425C50 | `Update` | For every unit: `sub_448210`; if its category is the view mode, `sub_420930` (counters); sector vfn +0x2C, `sub_43CB60`, `sub_43CFA0`. |
| 0x425D30 | `MoveTo(sector)` | `world → sub_419500(sector, +0x29)` makes routes for the group. Each valid one (+0x5D) is added to the route manager and started (0x425770); then `sub_41FCF0` (equalise arrival times). |
| 0x425DD0 / 0x425DF0 | Clear / `Bounds` | The units' bounding box (+0x2C, +0x30), initialised to ±1 000 000. |

### 2.6 WMOB base (vtable 0x4A9E80), constructions (0x4A9ED8) and repair

**WMOB fields:**

| Offset | Field |
|---|---|
| +0x20 | unit type |
| +0x24 | belongs to the other side (0 = own) |
| +0x26 | selected |
| +0x28 | counter (WUnitHotSpot) |
| +0x2C / +0x30 | map x, y |
| +0x34 | group |
| +0x38 | hot spot |
| +0x3C | **intel age in hours** (−= hours each hour; 336 when seen) |
| +0x40 | 0xDEADF00D, then `dword_59D9B4` stamp |
| +0x44 | stop at the next node |
| +0x48 | sector object |
| +0x4C | BMOList (the battle-map units, "BMOList", `sub_47CD30`) |
| +0x50 | repair target |
| +0x54 | factory (`sub_45F770`, 0x134 bytes) |
| +0x58 | previous sector |
| +0x5C / +0x60 | arrived / expected counts |
| +0x64 | repair hours accumulated |
| +0x6C | battle placeholder |
| +0x70 | route (state) |

Constructions add +0x78 = **build total** and +0x7C = **progress**; finished when progress ≥ total (0x42AA70). The save's first float is the total and the second the progress, which settles WARMAP.md's "provisional".

`dword_4D2568` / `dword_4D2564` count WMOBs of the player's side and the other side.

| Addr | Name | What it does |
|---|---|---|
| 0x428BB0 / 0x428CB0 | `WMOB::ctor(type)` / dtor | |
| **0x428D70** | `WMOB::OnMessage` | **0x111 wParam:** 0xAD = change sector (→ 0x429E80); 0x803A = enter sector (→ 0x429DB0); 0x80A9 = set +0x44 and pass on to the route; 0x813E with lParam 0x8065 = open the factory panel (`[+0x54]+0x20 = 1`, vfn +0x14) or 0x813F = toggle suspend (`sub_45FEF0` / `sub_45FE60`).<br>**0x8012** (deselect): hide the factory panel.<br>**0x802D**: hourly (→ 0x429B80).<br>**0x805C**: build the context buttons (→ 0x429A70).<br>**0x807C** tooltip (below).<br>**0x8088**: stamp change → `sub_47D590`.<br>**0x80AA / 0x8136**: post 0x80A0.<br>**0x80B9**: one more battle unit back; when all are back, remove the placeholder and increment `dword_4F76F0[sector]` (boats, unit +0x4E == 3) or `dword_4F86B8[sector]`.<br>**0x80BA / 0x80BB**: reset / set the counts.<br>**0x8107**: pick a repair target.<br>**0x8116**: has a pending count.<br>**0x812F / 0x8139**: factory pass-throughs.<br>**0x8141**: clear repair flags.<br>**0x8143**: hourly consumption (0x429640). |
| — | Tooltip (0x807C, wParam = `char**` out, lParam = detail level) | Format (own objects only):<br>• `"%s"` name, then one of:<br>• `"\nIn transit"` (0x51D) while on a route;<br>• with a working factory: `"\n%s %s\n%s %0.1f"` = "Currently building:" (0x48E), item, "Days to completion:" (0x48F) = hours/24, then `"\n%s %0.1f"` "Efficiency:" (0x493) ×100;<br>• `"\nConstruction suspended"` (0x492) or `"\nIdle"` (0x491).<br>• At detail ≥ 2: `"\n%s (%s, %s, %s)"` "Resource use" (0x519) with the side's three resource names; `"\nWorking: d, e, f"` (0x51B); `"\nIdle: …"` (0x51A, n × 0.5 × moving rates); `"\nMoving: …"` (0x51C, n × moving rates). |
| 0x429640 | `ComputeConsumption` | See 2.3. |
| **0x4297C0** | `PickRepairTarget` | **Mobile Repair Vehicle (30) and Modified MRV (31):** the more damaged of the most-damaged in the sector's own **construction** list and **resource** list (`sub_446370` kinds 2 and 3).<br>**Repair Workshop (60) and Martian Repair Facility (121):** if `0x8124` says finished, the most-damaged in the **unit** list (kind 1).<br>The target gets flag `+0xB8 |= 0x1000`. A target already at full HP is dropped. |
| 0x4299C0 | `RepairFlags(clear)` | Clears or tests flag 0x1000 on the BMOList units. |
| 0x429A70 | `BuildContextButtons(panel)` | For own finished objects with a factory, or units:<br>• 0x813E / 0x8065, icon 0x20, tip 0x470 (the build list);<br>• when working, 0x813E / 0x813F with icon 0x22 / 0x23 and tip "Suspend building tasks" 0x495 / "Resume building tasks" 0x496.<br>For a group on a route whose destination isn't enemy-held and `sub_43AC10` allows: **"Retreat"** 0x80A9 with GType 0x350, tip 0x477. (*0x477's text isn't checked; 0x518 is "Retreat to".*) Uses `sub_432E90` / `sub_432D70` (the HUD button bar). |
| **0x429B80** | `Hourly(hours)` | Intel age +0x3C −= hours (floor 0).<br>**Repair:** if the target left the sector, choose again. Accumulate `+0x64 += hours`. Threshold = `24 × sector[+0x94 + 12·side] × float[+0x98] × float[+0x9C]` (sector factors; 1.0 when `byte_58913C` is set, *cheat?*). For each threshold passed: target HP += **10** (type 30), **20** (type 31) or **30** (60 / 121), capped at max (then flag cleared and target chosen again). With no target it looks again every 24 h.<br>Passes 0x802D on to the factory. |
| 0x429DB0 | `EnterSector(s)` | Sector vfn +0x28 (add). Deletes the factory and makes a new one if the type appears in any build list (unit table +0x78 / +0x7C / +0x80). |
| 0x429E80 | `ChangeSector(s)` | Leaves the old sector (vfn +0x2C), recomputes the owner (`sub_43CA40`), reshades (`sub_4192F0(0)`), enters the new one, recomputes and reshades again; `sub_47D540`. |
| 0x429F20 / 0x429FC0 | `CreateCounter` / `DestroyCounter` | A WUnitHotSpot (pool), rectangle (x, y, x+32, y+32). |
| 0x429FE0, 0x429FF0 | BMOList reset; hot spot 0x8078 | |
| 0x42A020 | `SectorIndex` | |
| 0x42A030 / 0x42A090 | `SpawnBattleUnits` / `AddBattleUnit` | Spawns unit table +0x58 (word) battle units of this type (`sub_446B30` → `sub_480100`) into the BMOList. |
| 0x42A0B0 | `OnBattleUnitDestroyed(u)` | Removes it from the BMOList, notifies 0x80C1. If none are left: delete the factory, tell both players **0x807A**, update the group, drop the placeholder, leave the sector (the object dies). |
| 0x42A190 / 0x42A220 | `WMOB::Load` / `Save` | Tag **'WMOB'** (0x424F4D57), sector index, BMOList, hasFactory, factory, then +0x3C. On load it re-enters the sector with 0x111 / 0x803A. |
| 0x42A2A0 / 0x42A340 | Battle-map centre | (min + max) / 2 of the units' +0x78 / +0x7C (used by 0x439770 and 0x43BE50). |
| 0x42A3E0 | `ClunterTo(x, y, speed)` | Through the counter manager `sub_4208B0`. |
| 0x42A410 / 0x42A430 / 0x42A440 | Small accessors | Factory has a job (`+0x54 → +0x128`); get / set the route state +0x70. |
| 0x42A450 / 0x42A4A0 | `WWarMapConstruction` ctor / dtor | +0x78 = 1.0, +0x7C = 0. |
| **0x42A4B0** | `Construction::OnMessage` | **0x807C** tooltip: name; class 7 (AI.ojd class, `dword_518B14[type·8]`) adds `" X %d"` (the count in the sector, `sub_43A160`). Unfinished: `"\n%2.1f%% complete"` (0x4B5). Finished: the factory lines as in WMOB. At detail ≥ 2: `"\n%s (%s, %s, %s)\n%s %0.1f, %0.1f, %0.1f"` resource use / Working.<br>**0x80AC**: complete (→ 0x42A980).<br>**0x80AD**: progress += *(float\*)wParam.<br>**0x80AE**: total = *(float\*)wParam, progress = 0.<br>**0x8124**: finished?<br>Otherwise the base handler. |
| 0x42A980 | `OnCompleted` | Full HP; finished. For a **command building** (58 / 278 / 120 / 830): all sectors refreshed (`sub_418890`) and, if it's the player's, **event 0x0F CommandCentreBuilt**. Otherwise picks a repair target and posts 0x8145 to the counters (via `dword_4EA448`). |
| 0x42AA70, 0x42AA90 / 0x42AAC0 | `IsFinished`, `Save` / `Load` | Total and progress, then WMOB. |

### 2.7 Calendar View (most is in WARMAP.md; additions only)

| Addr | Name | What it does |
|---|---|---|
| 0x426C10 / 0x427080 | ctor / dtor | Window "Calendar View", id 0x801A, depth 0x5DCA. The slider `sub_431E30(0x97, 0x0A positions, 0x12 Human / 0x0B Martian…)`. Initial frame state from 0x427500; the digit targets go to +0x90 / +0x9C / +0xA8 / +0xB4 and the arrays at +0xD4 / +0xEC. |
| 0x4271E0 | `Draw` | |
| 0x427500 | `ComputeTargets` | Documented. |
| **0x427710** | `AnimateDigits` (odometer) | Each digit wheel moves toward its target frame by a **speed-dependent step a tick**: Martian `dword_4BA6A0[speed·6 + digit]`, Human `dword_4BA7A8[speed·6 + digit]`, frames mod 50 (10 digits × 5); the Human first digit is mod 20. When the step would pass the target, it snaps (also when it's within 10 frames of 0). am/pm (+0x9C) steps `dword_4BA8B0[speed]` mod 10; the month (+0xA8) steps 1 mod 60; the hour wheel steps `dword_4BA8E0[speed]`. **The engine jumps straight to the target.** |
| 0x4278A0 | `OnMessage` | **0x114 / 0x115** (slider): speed = position, or 0 if `sub_41AEA0` (any sector refuses).<br>**0x201** (a click on the calendar body): if allowed, **stop time** (`world.sub_41A6C0(0)`), calendar +0x30++ and vfn +0x10 (*unclear: step one hour?*).<br>**0x4E** with code 0x202 (slider released) and speed ≠ 0: **start time** (`sub_41A6C0(1)`).<br>**0x8073**: set the slider (lParam >> 16).<br>**0x807C** tooltip detail 3 = "Advance time" (0x481).<br>**0x80D0**: return the position. |

`world.sub_41A6C0(on)` (part 03) sets `world+0x64` and sends the local player packet 0x0A (time running) or 0x0B (stopped), and the AI `vfn +0x30(on)`. **Clicking a counter also stops time** (0x42C160).

### 2.8 Counter slide ("clunter", vtable 0x4A9E58) and the sector cell grid

The grid functions 0x428110 to 0x4283F0 are documented in WARMAP.md (0x4282C0 spiral, 0x4283F0 cell picker). 0x4283B0 clears the occupancy bits (`& 0x7F`) and 32 slot ids (−1).

**Clunter** 0x428620 (ctor: object, x, y), fields +0x0C speed, +0x10 / +0x14 step vector, +0x1C distance left, +0x20 / +0x24 position, +0x28 / +0x2C target:
- 0x4286C0 `SetTarget(x, y, speed)`: direction from `sub_43FDA0`, `step = polar(H, speed)`.
- 0x428730 `Tick`: if speed < left, then pos += step and left −= speed; else snap to the target. Then moves the counter (`sub_4406A0`).
- 0x4287A0 `Done`: left < 0.0005.

Used by the counter manager (part 03: 0x420583 to 0x420A0C). The speed is `[Tweak] "Clunting Speed"` (0x428B70, default **7.0**, `flt` at `dword_4EA194`).

### 2.9 Message Item (vtable 0x4A9E68)

| Addr | What it does |
|---|---|
| 0x4287C0 / 0x428810 | ctor (load) / ctor(text, life). Text font 0x72, colour `word_4B9420`, depth 0x11; +0x1C = the game time at creation (`dword_4E9B80 + 0x28`). |
| 0x4289D0 | `SetLife(n)` |
| 0x428A00 | `TickLife`: state +0x18: 0 live, 1 dying, 2 dead. |
| 0x428A50 | `Draw` |
| 0x428A80 / 0x428B10 | Load / save: text, life, state, time. |

### 2.10 WUnitHotSpot: the war map counter widget (vtable 0x4A9F50, pool 0x42C340 / 0x42C3E0)

**Pool.** Blocks of 128 × 0xB8 bytes (0x5C00). The free list is `dword_4EA2A8`, the live count `dword_4EA2A4`, the blocks `dword_4EA200[]` (count `dword_4EA2A0`). When the count drops to 0 with more than one block, everything is freed ("UNLOADING ALL WUnitHotSpot OBJECT MEMORY").

| Addr | Name | What it does |
|---|---|---|
| 0x42B2F0 | ctor(parent, rect, WMOB) | +0x7C = WMOB, +0x90 = counter frame (unit +0x2C), +0x94 / +0x98 = blink state (random 0 to 7 + the tick), +0x9C = GType 0x77 counter, +0xB5 = has a factory, +0x8C = class 7 (shared counter). A class-7 counter gets a count text (+0xAC, font 0x6E, right-aligned flag 8) and calls itself 0x8145. Otherwise a **power bar** (+0xB0, `sub_449150`, GType 8 `hwpower.spr`, from the units' HP ratio). Buildings: if unfinished, a label (+0xA0, `sub_49D2C0`, font 0x6F); if finished with a factory, an overlay (+0xA0, GType 0x76 24 px). Units with a factory get the overlay too. |
| 0x42B790 | `UpdateCount` | Over the sector's own (+0x54) or enemy (+0x58) construction list, counts this type: `"%d/%d"` (finished / total) when any are unfinished, else `"%d"`. |
| **0x42B850** | `Draw` | Depth 7 if selected (`dword_4EA444 == this` or WMOB +0x26), else 4.<br>Buildings: frame; the overlay is dropped and made again when finished.<br>**Own** unfinished buildings draw at 50%.<br>**Enemy** counters draw at 50% when intel is older than 7 days (+0x3C < 0xA8).<br>Then the selection ring (+0xA4), the power bar under it, the count text at the bottom right. **Repair icon:** while any unit of this object is being repaired, GType 0x76 frame **0x95** is drawn (+0xA8). |
| 0x42BC90 | `DrawProductionIcon` | Own factory with a job: when not working, frame **0x94** (idle icon), solid. When working, the item's counter frame (`[+0x54]+0xA8 → +0x18` type → unit +0x2C), **blinking**: cycle state 1 to 8. States 1 and 6 are at 50%, 2 to 5 solid. It waits `(10 − efficiency·10)·6` ticks between cycles, efficiency from `sub_460740` (`dbl_4A9F48` = 10). |
| **0x42BEB0** | `OnMessage` | **0x20** (cursor, own only): wParam 0 → cursor 1; 1 → Shift held? (3 if selected, else 2) : 1; otherwise 0xC. If on a route, the arrow goes solid.<br>**0x8006** (leave): arrow back to 50%.<br>**0x201**: capture.<br>**0x202**: release + `Select` (0x42C160).<br>**0x8012**: selection ring on/off (GType 0x54 `cselecth.spr`, depth +1; on select `world.sub_419260(0)`).<br>**0x8111**: `SelectFromBox` (0x42C2B0).<br>**0x8145** (lParam = sector): `UpdateCount`.<br>Everything else goes to the WMOB's handler (tooltips etc.). |
| 0x42C080 | `SetSelected(bool)` | |
| **0x42C160** | `Select` | **Stops time** first. Mode `dword_4EA43C`:<br>• 0: deselect all (packet 4), then select this (packet 5, additive = Shift).<br>• 1: Shift → toggle (packet 6 = deselect if selected, else 5 additive); no Shift → deselect all + select.<br>Packets go to the local player through `sub_4460A0` / `sub_447D80` / `sub_4493E0` (network-safe command queue). |
| 0x42C2B0 | `SelectFromBox` | Own and not yet selected: without Shift, deselect all first; then select (packet 5). |

### 2.11 Win95DeskTop (window and platform layer) and demo playback

**Settings statics:**

| Addr | Section | Key | Default | Global |
|---|---|---|---|---|
| 0x42C460 | (implicit section) | "Full Screen" | 1 | `unk_4EA350` |
| 0x42C4A0 | `[Screen]` | "Top left" | 0, 0 | `dword_4EA340 / 344` |
| 0x42C500 | `[Screen]` | "Size" | 640 × 480 | `dword_4EA370 / 374` |
| 0x42C540 | `[Screen]` | `off_4BAAFC` key (BPP) | 16 | `dword_4BAAC4` |
| 0x42C5A0 | `[Screen]` | "Support screen size" | 640 × … | `dword_4EA3D8` |
| 0x42C5E0 | `[Screen]` | "Support screen BPP" | 16 | `dword_4BAAC8` |
| 0x42C620 | `[Screen]` | "AllowResize" | 0 | `byte_4EA3D1` |
| 0x42C660 | `[Screen]` | "FixedAspect" | 1 | `byte_4EA339` |
| 0x42D290 | `[Demo]` | "Playback Files" | 4 | `dword_4BAAD8` |

| Addr | Name | What it does |
|---|---|---|
| 0x42C6A0 | `TicksSince` | `dword_4D2558 − dword_4EA404` |
| 0x42C6C0 / 0x42C780 | `Win95DeskTop::ctor` / dtor | Stores hInstance; desktop colour depth = BITSPIXEL × PLANES (`dword_4EA3BC`). The dtor calls DestroyWindow. |
| 0x42C7E0 | `OpenDefault` | `SetMode(Support size, BPP, full screen)`. |
| 0x42C810 / 0x42C890 / 0x42C910 | `RequestMode…` | Post **0x803B** (`lParam = w | h<<16`, `wParam = bpp | fullscreen<<16`) to the desktop when the mode differs. The first two also call `sub_464B10` (sound). |
| 0x42C990 | `Post 0x803D(arg)` | Called from the menus at 0x405470 and 0x406310. |
| **0x42C9B0** | `SetMode(w, h, bpp, full)` | Guards re-entry ("Mode change re-entered!") and pauses the game. On the first call it creates the window: class with WndProc 0x42CD40, icon 0x82, IDC_ARROW, black brush, 640 × 481, then `sub_44AE50` / `sub_465BB0` (DirectDraw, DirectSound). Sets the style (windowed `|= 0xCE0000`, full screen strips it), `sub_44AE60(w, h, bpp, full)` (DeeDraw mode), SetWindowPos with AdjustWindowRectEx at the saved top-left, then resumes. |
| 0x42CCF0 | `Post 0x8041(a, b)` | |
| 0x42CD10 | `RestoreSurfaces…` | `sub_44CFA0(0)`; jumps into 0x44AF20. |
| **0x42CD40** | WndProc | **Mouse:** 0x200 to 0x205 track the left/right exclusivity (`dword_4EA400`: 1 left, 2 right; `byte_4EA408` = right held). They call `sub_4038D0` / `sub_403900` (input state, part 01), `GetCursorPos` + `sub_42DA50` (client mapping), and every button / move goes through `Record` (0x42D520).<br>**Keys:** WM_KEYDOWN / WM_CHAR / WM_SYSKEYDOWN go to `Record`. WM_CHAR also goes to `sub_401000` ("englishrose"). **VK_PAUSE** (0x13) toggles `byte_4EA3E0` (pause / resume). **Alt+Enter** (WM_SYSKEYDOWN 0x104 with VK_RETURN) switches between windowed and full screen via `sub_42C910` / 0x803B, or "Incompatible mode" (`sub_4164A0`) when the desktop BPP doesn't match.<br>**Activation:** WM_ACTIVATEAPP (0x1C) logs "OnActivateApp On/Off", restores surfaces (`sub_44BE80`, "Restoring surfaces") and pauses / resumes. WM_ACTIVATE (7 / 8) does the same.<br>**Window:** WM_MOVE (3) stores the top-left. WM_DESTROY (2) → PostQuitMessage. WM_NCMOUSEMOVE (0xA0) shows the cursor; WM_MOUSEMOVE hides it again (`byte_4BAACC`). WM_SIZING (0x214) → 0x42D660. MM_MCINOTIFY (0x3B9) → `Record` (CD audio).<br>Everything else → DefWindowProc. |
| 0x42D280 | `Desktop()` | |
| **0x42D2D0** | `PlaybackPump` | Called each frame from 0x403680. When playback is on (`byte_4EA3E4`), opens `playback.N` ("rb", N cycling 1 to "Playback Files") and replays the records **{u32 tick, msg, wParam, lParam}** whose tick has come, posting them to the desktop (`sub_4355A0`). For 0x200 it also moves the real cursor (scaled to the client rectangle when windowed). Stops at EOF. |
| **0x42D520** | `Record(msg, wParam, lParam)` | Unless playing back: when recording (`byte_4EA3F4`) appends the same 16-byte record to `Playback.1` ("wb"). Always posts the message to the desktop, so **all input goes through this one queue**. |
| 0x42D610 | `PostRightHold` | While the right button is held: **0x805B** with the ticks held and lParam. |
| 0x42D640 | `PostMouseTick` | **0x800A** with the stored wParam / lParam: a "mouse state this tick" message (the mini-map drag uses it). |
| 0x42D660 | `OnSizing` | Windowed with AllowResize: keeps the 640:480 aspect when FixedAspect, minimum 160 × 120; otherwise locked to the mode size. |

---

## 3. Data structures and globals

**Unit table `0x51A0F0`, 0xB0 bytes a type.** Fields read in this piece:

| Offset | Field |
|---|---|
| +0x0C | side (record 26) |
| +0x10 | category: 1 units, 2 constructions, 3 resource facilities |
| +0x14 | name text id (record 21) |
| +0x2C | counter frame |
| +0x4A | max HP (word, scaled by the side's armour float) |
| +0x4E | move class (3 = boat) |
| +0x52 | 5 / 6 = naval route (*guess*) |
| +0x54 | war map speed |
| +0x58 | battle units a counter holds |
| +0x60 / +0x64 / +0x68 | moving consumption (Iron/Coal/Oil or the Martian three) |
| +0x6C / +0x70 / +0x74 | working consumption |
| +0x78 / +0x7C / +0x80 | build-list entries (any non-zero means it gets a factory) |
| +0x88 | (`dword_51A178`) |

**AI class `dword_518B14[type·8]`:** 7 = shared counter (fences, mines, guns).

**Sector table `0x4EAE1C`, 0xC0 a sector:**

| Offset | Field |
|---|---|
| +0x00 | owner |
| +0x04[6] | deposit B per kind |
| +0x1C[6] | finished facilities per kind |
| +0x34[3] | cleared when the owner changes (`sub_43CA40`) |
| +0x4C[6] | used per week |
| +0x7C[6] | produced per week |
| +0x94 + 12·side | int, float, float: repair-time factors |

**Per-sector arrays:** `dword_4F86B8[31]` units (non-boat) and `dword_4F76F0[31]` boats in a sector.

**Sector object lists:** +0x4C HUNI, +0x50 AUNI, +0x54 HCON, +0x58 ACON, +0x5C HRES, +0x60 ARES. `sub_446370(kind 1..3, own)` picks one. Sector +0x124 = blocked flag (it interrupts travel).

**World (`dword_4E9B70`):** +0x64 time running, +0x70 view mode, +0x74 counter manager, +0x80 counter / HUD message target, +0x84 calendar / selection holder, +0x90 route manager, +0x94 + 4n the sector objects.

**Players:** `dword_4E9B74` is the local player (+0x24 = side 0 Human, 1 Martian) and `dword_4E9B78` the other one / AI.

**Other globals:**

| Global | Meaning |
|---|---|
| `dword_4D2558` | the tick counter |
| `byte_4D2554` | loading |
| `dword_54604C` | network player count (`> 1` = network) |
| `dword_4E9F40` | the selected resource kind |
| `dword_4EA444` | the hovered or selected counter |
| `dword_4EA43C` | selection mode |
| `dword_4EA448` | the counter message target |
| `byte_59EF50` | the shared tooltip buffer |
| `dword_59D9B4` | stamp |
| `byte_58913C` | repair cheat? |
| `dword_4EA198` | the last route refusal code |
| `dword_4EA108` | route id counter |
| `byte_4EA3E0` | game running (not paused) |
| `byte_4EA3E4` / `byte_4EA3F4` | playback / record |
| `byte_4EA40C` | inside a mode change |
| `dword_4EA3A0 / 4EA348 / 4EA300 / 4EA364` | current w / h / bpp / full screen |

**Route waypoint table `0x4BAE68 + 32n`:** land waypoint (x, y), then **sea waypoint (x, y)** at +8 (`0x4BAE70`), then the bounds `0x4BAE78…`.

---

## 4. Messages, commands, strings and tuning

**WM_COMMAND (0x111) ids:**

| Id | Meaning |
|---|---|
| 0x7A | Mini-map go to (`lParam = x | y<<16`) |
| 0xAD | WMOB changed sector (lParam = sector object, 0 = in transit) |
| 0xB7 | Sector refresh |
| 0x803A | Entered a sector |
| 0x803C | Recompute resource rate |
| 0x80A9 | Leave or stop a route (the "Retreat" button) |
| 0x811F | View button |
| 0x8033 | Set a button pressed |
| 0x813E + 0x8065 / 0x813F | Factory panel / suspend-resume |

**Private messages:**

| Id | Meaning |
|---|---|
| 0x8004 | View moved |
| 0x8006 | Mouse leave |
| 0x800A | Mouse tick |
| 0x8012 | Selected on/off |
| 0x802D | Hours passed |
| 0x8034 | Pressed |
| 0x803B | Mode change |
| 0x803D, 0x8041 | Desktop requests |
| 0x805B | Right button held |
| 0x805C | Build context buttons |
| 0x8073 | Set the calendar slider |
| 0x8078 | (to the hot spot) |
| 0x807A | Object died (to the players) |
| 0x807C | Tooltip text |
| 0x8083 | Rebuild resource stack |
| 0x8088 | (stamp) |
| 0x80A0 | (posted on 0x80AA / 0x8136) |
| 0x80AC / 0x80AD / 0x80AE | Construction completed / add progress / set total |
| 0x80B9 / 0x80BA / 0x80BB | Battle-unit return counting |
| 0x80C1 | (to the battle placeholder) |
| 0x80D0 | Get position |
| 0x8107 | Pick repair target |
| 0x8111 | Select from box |
| 0x8113 | Select resource kind |
| 0x8116, 0x8124 | Is finished |
| 0x812F, 0x8136, 0x8139, 0x8141 | Clear repair |
| 0x8143 | Consumption |
| 0x8145 | Counter count update (wParam sector) |
| 0x114 / 0x115 / 0x4E | Slider |

**Events raised (0x41E5B0):** 3 ArrivedInSector (0x425080), 0x0B TravelInterrupted (0x425080), 0x0F CommandCentreBuilt (0x42A980).

**Player and group packets:**

| Packet | Meaning |
|---|---|
| 4 | Deselect all |
| 5 | Select (id, additive) |
| 6 | Deselect |
| 0x0A / 0x0B | Time on / off |
| Group type 8 | Move to sector |

**Strings (TEXT ids):**

| Ids | Text |
|---|---|
| 0x47B / 0x47C / 0x47E | View buttons |
| 0x481 | Advance time |
| 0x48E | Currently building |
| 0x48F | Days to completion |
| 0x491 | Idle |
| 0x492 | Construction suspended |
| 0x493 | Efficiency |
| 0x495 / 0x496 | Suspend / Resume building tasks |
| 0x470, 0x477 | Button tips (*not checked*) |
| 0x4B5 | complete |
| 0x514 to 0x517 | produced / Next facility will add / used / tons p/w |
| 0x519 to 0x51D | Resource use / Idle: / Working: / Moving: / In transit |
| 0x214 to 0x219 | Resource names |

**Format strings:**
- `"%s\n%0.2f %s %s\n%0.2f %s %s\n%s %0.2f %s"` (0x4BA624)
- `"\n%2.1f%% %s"`, `" X %d"`, `"%d/%d"`, `"%d"`, `"--:--"`
- `"ROUTE NODE % 4d % 4d % 4d"` (debug, nullsub)

**Tuning:**

| Setting | Default |
|---|---|
| `[Tweak]` Resource Drop Off Rate | 3.0 |
| `[Tweak]` Res Stack XO / YO | 4 / 16 |
| `[Tweak]` Clunting Speed | 7.0 |
| `[Tweak]` Max units in sector / Max boats in sector | 15 / 5 |
| `[Tweak]` Route clunting width / spacing | 4 / 4 |
| `[Stats]` Routes left / WM Groups | debug |
| `[Screen]` Full Screen / Top left / Size / BPP / Support screen size / Support screen BPP / AllowResize / FixedAspect | see 2.11 |
| `[Demo]` Playback Files | 4 |

**Constants:**
- Curve subdivision exponent 5 (32 steps), bend factor 5/11 (0.4545), angle unit 4096 a turn.
- Route speed factor 0.05 (0.01 on load).
- Intel: fresh = 336 h, stale below 168 h.
- Repair: +10 / +20 / +30 HP a step, rescan every 24 h.
- Production blink: `(10 − eff·10)·6` ticks.
- Mini-map view rectangle margin: 2 px.

---

## 5. Cross-references to other pieces

- **Part 01** (0x401000 to 0x405A70): `sub_401000` (englishrose), `sub_4038D0` / `sub_403900` (input state), `sub_403680` (calls PlaybackPump), `sub_404BA0` / `sub_404BB0` (is Human / is Martian), `sub_404BC0` / `sub_404C10` (pause / resume), `sub_402630` (creates the desktop), `sub_402B80` (WGame, calls the mode requests).
- **Part 03 (war map world):**
  - 0x417CD0 world ctor; 0x418980 view set-up (calls 0x422C70, 0x423420, 0x4238D0, 0x422BA0).
  - 0x418890 sector refresh; 0x4192F0 shading (uses 0x424000); 0x419500 / 0x419520 route creation (calls 0x424160 / 0x425570); 0x419240 sector lookup; 0x41A6C0 / 0x41AEA0 time run / allowed.
  - 0x41B9A0 date text; 0x41E5B0 events; 0x41F640 / 0x41F550 calendar; 0x41FBD0 / 0x41FCA0 / 0x41FCF0 route manager.
  - 0x4200D0, 0x420160 sector cells; 0x4203C0, 0x420540 / 0x4208B0 / 0x420930 counter manager (uses the 0x428620 clunter); 0x420A30 / 0x420DA0 message list (uses 0x428810); 0x4216D0 / 0x4216E0 / 0x4211D0 list helpers; 0x421F30 group-is-own.
- **UI widgets** (0x42E000 to 0x4337B0): `sub_42F520` window base, `sub_42E150` draw children, `sub_42E780` set cursor, `sub_42E9B0` / `sub_42EA00` capture, `sub_42EDA0` / `sub_42EDC0` size / position, `sub_4333B0` button, `sub_431E30` / `sub_4324D0` slider, `sub_432D70` / `sub_432E90` HUD buttons, `sub_4335F0` hot spot base, `sub_435050` message queues (`unk_4D24C0` posted, `unk_4DAF28` immediate).
- **Sector objects** (0x43C000 to 0x43D200): 0x43C990 has command building, 0x43CA30 owner, 0x43CA40 recompute owner, 0x43CB60 refresh counters, 0x43CC40 / 0x43CC70 facility counts, 0x43CCA0 add consumption, 0x43D010 allows time, 0x43A160 type count, 0x43AC10.
- **Object factory and players:** 0x446950 (object factory), 0x446370 (list picker), 0x446B30 (battle unit), 0x447DC0 group base, 0x4460A0 / 0x447D80 / 0x4493E0 packets, 0x449150 power bar, 0x440120 / 0x4404A0 / 0x440550 WSelectableObject base / load / save.
- **Research and factory:** 0x45BD70 Research button, 0x45F770 factory, 0x45F7D0 hours left, 0x460740 efficiency, 0x460A40 item name, 0x45FE60 / 0x45FEF0 suspend.
- **Battle map:** 0x439770 set-up; `dword_4F8738` group manager (0x483DF0, 0x483F90, 0x483FF0, 0x484120); 0x47CD30 / 0x47CFA0 / 0x47D190 / 0x47D540 / 0x47D590 / 0x47DA20 BMOList and unit max HP; 0x480100.
- **DeeDraw and sound:** 0x44AD20 / 0x44AE50 / 0x44AE60 / 0x44BE80 / 0x44C640 / 0x44CFA0; DirectSound 0x465B20 / 0x465BB0; 0x464B10.
- **Pathfinding:** 0x44EE70 / 0x44EFA0 A\*, 0x44F280.
- **Rendering:** 0x49BE70 / 0x49C740 / 0x49C8D0 highlight and mini-map; 0x49CAC0 RTypeArrow; 0x49D260 / 0x49D2C0 text objects.

---

## 6. What WoWEngine is missing in this piece: candidate tasks (in priority order)

WoWEngine's `src/warmap/WarMap.cpp` has one simplified counter per kind of object, A\* routes with refusal codes, a Catmull-Rom arrow, ArrivedInSector and BattleInSector, the calendar without its animation, and the mini-map panel without the Research button. There is no economy, repair, construction or tooltip, and no per-object model.

1. **A real WMOB / counter model** (0x428BB0, 0x42A450, 0x4239C0, 0x42B2F0, 0x429F20). One object per WMOB, with sector, BMOList count, route state, intel age, factory and the construction total/progress. Load it from the save: 'WMOB' layout at 0x42A190 / 0x42A220; construction floats = total, progress (0x42AAC0); RouteEntry 0x42B150; route 0x4257C0. This replaces the engine's aggregated `Counter`. Prerequisite for most of the items below.
2. **Load and continue saved routes** (0x4257C0, 0x427E80, 0x42B150), including the 0.01 ETA quirk on load (decide whether to copy the bug).
3. **The route arrow's exact curve** (0x424A40, decoded in 2.4) and the head (part 03 / 0x49CDB0). Also the **arrow trim** behind the leading units (0x425080 → arrow +0x5C) and the **style split**: attack routes are style 0 / 2 with an ETA label; ordinary moves are 1 / 3 **without a label** (0x4244E0). The engine labels every route and uses 0 / 2.
4. **Attack-route rules** (0x424160). A route to an enemy-held sector must be adjacent (2 nodes). Count it on the destination (+0x74 / +0x78). It raises no ArrivedInSector (0x425080).
5. **Formation offsets while travelling** (0x42AFE0: W = 4, S = 4) and the per-hour movement model (speed = remaining distance / hours; segment lengths between waypoints, not curve points; frac ≥ 0.5 clears the "arrived" flag). Also the **route start**: counters slide to the start at Clunting Speed (0x428020); the clunter is 0x428620 to 0x4287A0.
6. **TravelInterrupted** (0x425080 → event 0x0B). Nodes on the way that turn enemy-held or blocked (sector +0x124) stop the groups at the next node.
7. **Sea waypoint** (0x4373A0, 0x4BAE70 + 32n) for naval routes (+0x50, unit +0x52 ∈ {5, 6}) and as the fallback when a link point is zero. Check +0x52's meaning in OBJ.ojd record 37.
8. **Resource economy** (0x423B00, 0x423F80, 0x429640, sector table fields in 3): facility counts, output `n·r·B/(n + r − 1)`, per-facility rate gated by command building, finished state and **full HP (integer)**, hourly consumption (moving, idle at half, working), the sector produced/used arrays, and the Resource View shading (0x424000 into `sub_4192F0`).
9. **The Resource View widgets.** RFHS facility stacks (0x425E80, Res Stack XO / YO 4 / 16) and ROHS bars (0x426590: sprites 0xB9 to 0xBB plus the 0xBC deficit marker, height = value / 3). Clicking a bar selects the kind (0x8113, `dword_4E9F40`); the others draw at 50%.
10. **Tooltips** (0x807C): WMOB (0x428D70), construction (0x42A4B0), RFHS / ROHS (0x426430 / 0x426AE0), calendar "Advance time". Includes the detail level (≥ 2 adds resource use) and the formats in section 4. This needs the tooltip widget from the UI-widget piece (font 0x6F).
11. **Counter decorations** (0x42B850 / 0x42BC90): the power bar (GType 8, HP ratio), production icon blink (`(10 − eff·10)·6`), idle icon (0x76 frame 0x94), repair icon (frame 0x95), `"%d/%d"` / `"%d"` counts for class-7 and shared types (0x42B790), 50% for unfinished own buildings and enemy intel older than 168 h, depth 7 when selected.
12. **Selection model** (0x42BEB0 / 0x42C160 / 0x42C2B0): Shift-click to add or toggle, box select (0x8111), cursors (GTypes 1 / 2 / 3 / 0xC), arrow solid on hover, **any counter click stops time**. Send it through a command/packet layer (4 / 5 / 6) so network play can reuse it later.
13. **Repair** (0x4297C0, 0x429B80, 0x422AB0). MRV (30) and modified MRV (31) repair the most damaged building or facility at +10 / +20 a step; Repair Workshop (60) and Repair Facility (121) repair units at +30; the threshold is `24 h × sector factors`. Needs battle-unit HP, i.e. the BMOList.
14. **Construction progress and completion** (0x42A4B0 / 0x42A980: messages 0x80AD / 0x80AE / 0x80AC). Completing a command building raises **CommandCentreBuilt** (event 0x0F) and refreshes the sectors; completing a facility increments its kind's count.
15. **Context buttons** (0x429A70): factory panel (0x813E / 0x8065), Suspend / Resume (0x813F, tips 0x495 / 0x496), **Retreat** (0x80A9, GType 0x350) for groups on a route. Needs the HUD button bar (0x432D70 / 0x432E90).
16. **The mini-map frame's Research button** (0x423420 → 0x45BD70, GType 0x341 at (0x48, 5) Human / (0x3A, 0xC) Martian). It opens research; that part is in the research piece.
17. **Calendar odometer animation** (0x427710, step tables 0x4BA6A0 / 0x4BA7A8 / 0x4BA8B0 / 0x4BA8E0) and the **time start/stop** behaviour: releasing the slider at speed > 0 starts time, a click on the calendar body stops it (0x4278A0 → 0x41A6C0).
18. **Mini-map details** (0x423130): click and drag send 0x7A centred on the point; the view rectangle is clamped with a 2 px margin; the sector frame shows the selected sector. The engine's version is its own, so check it against this.
19. **Intel ageing** (0x429B80 +0x3C, 0x422BA0 sets 336): enemy counters fade after 7 days without contact.
20. **Platform / desktop** (0x42CD40 and friends):
    - Pause key (VK_PAUSE) toggles the game.
    - Deactivate pauses, activate resumes and restores.
    - Alt+Enter toggles full screen (the engine has `ToggleFullScreen`; check the key binding).
    - Windowed resize: AllowResize / FixedAspect 640:480, minimum 160 × 120.
    - `[Screen]` keys as in 2.11.
21. **Demo playback and recording** (0x42D2D0 / 0x42D520). 16-byte records {tick, msg, wParam, lParam} in `playback.1` to `playback.4` (`[Demo] "Playback Files"`). Low priority, but it's the original attract-mode / test-harness mechanism. With all input in one queue this is cheap to copy, and useful for regression tests against the original if any `playback.*` files exist.
22. **Battle hand-off** (0x422910, 0x42A030, 0x42A2A0 / 0x42A340, 0x42A0B0). Groups the sector's command building and vehicles for the battle map; spawns unit table +0x58 battle units per counter; the war map object dies when its last battle unit does (notify 0x807A). Do this with the battle map work.
23. **Small correctness items:**
    - The units/boats-in-sector limits count from `dword_4F86B8` / `dword_4F76F0`, incremented on 0x80B9 (units returning from battle), not from the counters drawn.
    - Max units / boats are patch-limited to 255.
    - The engine's `Counter::players` / owner logic should follow `+0x24` and the HUNI…ARES list layout.
