# Part 13: 0x4956D0 to 0x4A14C0

Survey of `pieces/part13.txt` (190 functions). Addresses are vanilla `WoW_patched.exe`. Unit type names come from
the game's own debug "Create..." menu (the `sub_49BBF0` calls in 0x43B2xx to 0x43E7xx; there are 344 of them, saved
in `typenames.txt` here). Text ids come from `text.txt` (the Human strings). Anything marked *guess* is
inferred and has not been confirmed.

## 1. Overview

Most of this piece is **battle map** code. The war map and front end parts are the menus, the sector highlight and
the route arrows. In address order:

| Range | What it is |
|---|---|
| 0x4956D0-0x496C20 | **WPuppet**, the base class for every battle-map unit and structure: vtable 0x4B0988, constructor 0x495660 (in part 12), and the counter "Puppets objects left". It handles messages, tick, move and attack orders, auto-targeting, damage with the research armour multipliers, fog reveal, and destruction. |
| 0x496C40-0x497890 | **WPuppetHotSpot**, the clickable UI control on each puppet: a pool of 256 per block, the selection corner brackets, the health bar, the cursor choice, and click-to-select or click-to-order (Ctrl force-fire, Shift add). |
| 0x497890-0x498750 | **WBridge** (types 0x1BC to 0x1BF), the bridge spans. They build deck segments, stitch the WOF meshes together, add route-finder nodes, and give the deck height for units on the bridge. |
| 0x498750-0x498D30 | **WFence** (types 0x1C3 to 0x1C8: wire fence, wire wall, mined wire wall, electric, laser and plasma fencing). It has two end posts, segment objects of type 0x17B, grid snapping and route-finder blocking. |
| 0x498D40-0x4993F0 | **WSubmergingVehicle** (a vehicle subclass: Tunnelling Track Layer types 7 to 9, Submersible types 0x16 to 0x18). It handles dive and surface, the hidden state, and the label and bubble sprite. |
| 0x499420-0x499850 | The **BatRoute** cost object for the battle-map A* (line-of-cells terrain cost sampling) and the "BatRoute" node-array container. |
| 0x4998F0-0x49B900 | The **Briefing** (0x499930) and **Debriefing** (0x49A7B0) full-screen views shown around a battle, with `[Briefing] Delay` and `[Debriefing] Delay`. |
| 0x49B980-0x49BE50 | The **War map menu** 0x80B5, the **debug CreationMenu** ("Create..."), and the **Battle map menu** 0x80B8. |
| 0x49BE70-0x49C070 | The **war map sector-map sprite / highlight tables**: per-value shade levels, the border-to-sector table, and the pixel hit test. |
| 0x49C080-0x49C720 | **WMouseDrag**, the rubber-band drag box base class (vtable 0x4B1070). |
| 0x49C740-0x49C990 | **RTypeRect** (a filled or outlined rectangle) and the **highlight map renderer** (vtable 0x4B10B0, draws the war map through the sector map). |
| 0x49CAC0-0x49D2A0 | **RTypeArrow** (war map route arrows) and **RTypeBoxedText** (text on a filled box with an outline). |
| 0x49D430-0x49D600 | **Tooltip** objects and `[Screen] "Tool Tip Offset"`. |
| 0x49D620-0x49DA40 | Small render types: a scaled sprite, a texture-atlas sprite and a textured-quad sprite. |
| 0x49DA50-0x49E2F0 | **RTypeWOF** (3D model render object) and **RTypeISO** (isometric object with "ISOLight"), plus `[Game] "WOF Brightness"`. |
| 0x49E330-0x49F230 | **WOFLib**, the 3D model instance: the piece hierarchy transform, face sort, sun and point-light face shading, and polygon draw. |
| 0x49F240-0x4A0370 | **WLandscapeView**, the battle-map terrain view (vtable 0x4B11F8). It draws terrain row by row, interleaving depth-sorted objects, and updates the mini-map viewport. |
| 0x4A03A0-0x4A14BC | The **construction placement drags** (subclasses of WMouseDrag) for buildings and fences/walls. They draw a footprint projected onto the terrain in valid or invalid colours and post 0x80A4 / 0x8137. |

There is no hand-written assembly *inside* this piece: all of it follows the compiler's prologues and switch tables.
It does call into assembly blitters elsewhere: `sub_450528` (fill rectangle, an unaligned label), and `sub_44F890`,
`sub_44F967` and `sub_44FE7F` (unaligned labels, a scaled sprite blit used by 0x49D670). The low-level polygon and
line routines are 0x410090 (textured quad), 0x410190 (textured triangle, which forwards to 0x456DD0), 0x410070
(flat polygon) and 0x4A24F0 (line). See section 5.

## 2. Subsystems

### 2.1 WPuppet: the battle unit/structure base (0x4956D0-0x496C20)

The constructor, 0x495660 (part 12), chains to 0x489E20, clears +0xC8 to +0xD4 and +0xDA, and increments
`dword_59E464` (the value of the stat "Puppets objects left", registered by 0x4955E0). Subclass constructors call it:
0x48B590, 0x48C8E0, 0x48E8A0, 0x490820 (the vehicle base, part 12) and 0x498750 (fence, below).

| Addr | vtbl slot | Name (proposed) | What it does |
|---|---|---|---|
| 0x4956D0 | dtor body | `WPuppet::~WPuppet` | Resets the vtable, decrements `dword_59E464`, then calls 0x489EC0. |
| 0x4956F0 | +0x08 | `Save` | Calls the base 0x489ED0, then writes through stream vfunc +0x60. What is written is *unclear* (probably the chunk tag). |
| 0x495710 | +0x0C | `Load` | Calls the base 0x489F30, then reads and checks the tag 0x50544142 ("BATP") through stream vfunc +0x34. |
| 0x495740 | +0x14 | `Init/Place(arg)` | Sets stance +0xD8 = unit table byte +0x43 (the default stance); sends 0x8079 to the group (+0x38); adds itself to the world list (0x440060 on `dword_4D2550`+0x20); calls 0x48A8B0(arg); clears its targets; sets +0xDA = rand()%30 (staggers the rescan timers); clears E4 bit1; calls vfuncs +0xAC/+0x88/+0x90. A friendly unit (+0x24 == 0) also calls vfunc +0x100 (fog reveal); otherwise it calls vfunc +0xA0(1) and 0x48A780. |
| 0x495820 | | | Thunk to 0x47FFC0. |
| 0x495830 | +0x18 | | Calls vfunc +0xCC (Stop), then 0x48A9F0. |
| 0x495850 | +0xB0 | | Returns the max over its sub-objects (list +0x70) of their vfunc +0xB0, or 10000 if it has none. *Unclear* meaning. |
| 0x4958B0 | +0xC0 | `TakeDamage(weapon, float dmg)` | Applies the research armour multipliers (below), then calls 0x48A5A0(weapon, dmg). Returns 1 without damage for weapon types 0x393 to 0x395 hitting a non-friendly unit. |
| 0x495BB0 | +0x78 | `Destroy` | Sends 0x807B (object destroyed) to both side controllers `dword_4E9B74` and `dword_4E9B78`; calls 0x48AD40; detaches its war-map object (+0x48 → 0x42A0B0); calls 0x48AC50 ("DESTROYING VEHICLES"); removes itself from the world (0x43C750). |
| 0x495C10 | +0xFC | `Broadcast(msg, w, l, radius)` | Sends a message to every world object (`dword_4E9B70`+0x118 → list +0x20) closer than the radius (distance from 0x43FF00). |
| 0x495CB0 | +0x1C | `HandleMessage` | See the table below. |
| 0x496100 | | `AttackNearestHostile` | Finds an enemy (0x4406E0 side differs) that is alive (+0xB8 bit0), within its sight radius (unit table word +0x48) and above ground (+0x84 >= 0, for both objects), then calls vfunc +0xC4 (attack). |
| 0x496200 | +0xC8 | `MoveTo(x, y, z)` | Stores the goal at +0xC8/+0xCC/+0xD0. Every member of the group (list +0x70) gets the goal jittered by ±(dist/16 + 1) on each axis (rand), through member vfunc +0xC8. Sets E4 bit1 (moving). |
| 0x496300 | +0xC4 | `Approach(target)` | Moves to the target's position pulled back along the line by target size (target +0xAC/3 when target +0xB8 bit1 is set), with z = target +0x80 − unit byte +0x41 × 1.414 (dbl_4B0AA8 = −1.414). |
| 0x496440 | +0xCC | `Stop` | Every member's vfunc +0xCC; sets +0xD4 = 1000000; clears its goal and E4 bit1. |
| 0x4964B0 | +0x100 | `RevealFog` | 0x434A80 (WFogOfWar, on `dword_4D2550`+0x84) at its landscape cell, radius = unit table word +0x48 (sight), in landscape scale `dword_59D940`+0x30/+0x34. |
| 0x496520 | static init | | Registers `[Debug] "Show Orders"` (bool, default 0) at 0x59E420. |
| 0x496560 | | | Thunk to 0x48B3D0. |
| 0x496570 | | `AutoTarget` | Scans enemies within sight × `[Tweak] "Audible radius multiplier"` (flt_59E44C, default 1.0, registered by 0x495620). It prefers a target with +0x3C set or the enemy's sector command building (Command Post 58, or 278 in London; Communications Centre 120, or 830 "Martian Base" in sector 3), and otherwise takes the last seen. It sends 0x80B1(target) to its group (+0x34). If this unit belongs to the other controller (`dword_4E9B78`+0x24), it also tells that AI 0x80B1. |
| 0x496760 | +0x90 | `SetState(s)` | Leaving state 0x80F6 restores the group stance (0x80B2 with the default stance, unit byte +0x43). Leaving 0x80F7 calls group 0x482880(0). Stores the state at +0xE0. |
| 0x4967E0 | +0x10 | `Tick` | Counts the state timer +0xDC down, and on 0 calls SetState(0). Every `[BattleMap] "Fog update freq"` ticks (`dword_4EA614`, default 15; frame counter `dword_4D2558`) a friendly unit reveals fog. The rescan counter +0xDA counts up to unit table word +0x92, then calls vfunc +0x104 (AutoTarget wrapper 0x496890) and vfunc +0xB4; if that is < 0 it calls vfunc +0x78 (Destroy). Ends with 0x48AEF0. |
| 0x496890 | +0x104 | `Rescan` | If moving, posts itself 0x808A, or 0x8031 when group flag +0x1D8 bit1 is set. If stance is 2 or 3 and the group allows it (+0x1D8 bit0 clear, or bit2 set), calls 0x496570. Resets +0xDA. |
| 0x496920 | +0x84 | `CanJoinGroup(type)`? | Compares the group's kind (+0x1DC) with type ranges: 0x197-0x199 need kinds 4/5/6; 0x2B9 needs 0xA; 0x2BA/0x2BB need 0xB/0xC; 0x2C2-0x2C4 need 7/8/9; 0x320-0x32B need 0xD-0x18; 0x323, 0x3AA and 0x3AB need 0x10; everything else needs kind 0. *Guess*: unit and squad class compatibility. |
| 0x496BC0 | +0xD0 | | Resets +0xDA; calls vfunc +0xAC(1); if the group's kind is set, calls 0x482880(0) and 0x4807E0(0). |
| 0x496C00 / 0x496C20 | | | Set or get E4 bit0. |

**0x495CB0 message handler** (ids are WM_COMMAND-style numbers; the default goes to 0x489FA0):

| Msg | Action |
|---|---|
| 0x802F | wParam ≠ 0: MoveTo(x = lo16(w), y = hi16(w), z = l). Otherwise Approach(target = l). |
| 0x805A | Stop, then vfunc +0xAC(0). |
| 0x80B2 | Stance +0xD8 = l. Stances 2 and 3 auto-engage (see Rescan). |
| 0x80E1-0x80E3 | Broadcast 0x80F8 within sight radius; group 0x4807E0(0). |
| 0x80F0 | Broadcast 0x80F9 within sight radius (*guess*: a detector pulse). |
| 0x80F4 | If w is a valid object (0x440110), this unit is friendly, and it is the sector's command building (see above): forward 0x80F4 to the group owner, then play sound 0x115 at the unit with volume from unit table byte +0xA0. |
| 0x80F5 | Timed state 0x80F5 for (l+2)×150 ticks. |
| 0x80F6 | Group: 0x805A (stop) and 0x80B2 (stance 0). Timed state 0x80F6 for (l+2)×150 ticks (*guess*: hold or cease-fire). |
| 0x80F7 | AttackNearestHostile; timed state 0x80F7 for (l+2)×150 ticks. |
| 0x80F9 | Hidden units ignore this unless researched: types 7-9 and 0x16-0x18 (tunnellers and submersibles) unless `byte_589134`, types 0x273-0x276 (mines) unless `byte_589130`. Otherwise timed state 0x80F9 for 240 ticks (*guess*: "revealed"). |

**Damage multipliers, 0x4958B0.** Each is applied when research is complete (0x45B890 on a research slot). All
the multiplier floats default to 1.0 and are written by the research code at 0x45A1C0 and 0x45B320.

- Weapon type 0x1AB-0x1B0: ×`dword_4CA180` if slot `dword_588D6C` (research 0x256) is done; then ×`dword_4CA184` if `dword_588D70` (0x257) is done.
- Weapon type 0x1B5-0x1B7: ×`dword_4CA184` (0x257).
- Weapon type 0x393-0x395: ×`dword_4CA194` (0x25C), then ×`dword_4CA198` (0x25D).
- Weapon type 0x2C8-0x2CA: ×`dword_4CA1A0` (0x266).
- 0x1AB-0x1B0 again: ×`dword_4CA35C` (0x27F).
- A friendly target with +0xB8 bit0 set: ×`dword_4CA348` (0x27B) and ×`dword_4CA34C` (0x27C).
- Every friendly target: ×`dword_4CA188` (0x259).

Which weapon names these ranges are has not been resolved.

### 2.2 WPuppetHotSpot: per-unit UI control (0x496C40-0x497890)

The constructor 0x496D60 (called from 0x48A6EE) is a UI control (base 0x4335F0) with vtable 0x4B0AC0 and the puppet
at +0x7C. It has a custom operator new, 0x496C40, that keeps a pool of 0x9C-byte objects 256 to a block (log "Allocating
memory block for 256 WPuppetHotSpot objects"). The free list is at `dword_59E468`, the block pointers at 0x59E470 (count
`dword_59E600`) and the live count at `dword_59E604`. The deleters 0x496CE0 and 0x496E60 free every block when the
count reaches 0 ("UNLOADING ALL WPuppetHotSpot OBJECT MEMORY").

| Addr | Name | What it does |
|---|---|---|
| 0x496D60 | ctor | +0x98 = 1 − (puppet vfunc +0xB4 / 0x47DA20) = the health fraction. If the bar exists, sets its frame. |
| 0x497000 | `Draw(surface)` | Places the four bracket sprites at the control rect's corners, inset 8 px, and the health bar at the bottom centre. |
| 0x497110 | `HandleMessage` | Forwards to the owner's hotspot (puppet +0x44 → +0x28) when there is one. WM_SETCURSOR (0x20): cursor 0xC ("don't") by default; cursor 5 (attack) over an enemy or with Ctrl held; cursor 1 (select) otherwise. WM_LBUTTONDOWN: captures the mouse (0x42E9B0). WM_LBUTTONUP: 0x497770. 0x8012: Select(w ≠ 0). 0x805C is forwarded to the puppet. 0x807C is re-sent to itself. 0x8111 runs Click-select (0x497680). |
| 0x497290 | `SetHealth(f)` | +0x98 = 1 − f; the health bar frame is round(frames × f), clamped. |
| 0x4972F0 | `Select(bool)` | Tells the puppet's group (+0x48 → +0x6C) 0x8012. On select, it creates four GType 5 sprites (frames 0-3, depth 0x464F, flag 1) at +0x84 to +0x90 and a GType 7 health bar at +0x94. On deselect it releases them. Ends with puppet 0x47FB20(bool). Enemies can only be selected in debug (`byte_4D1279`). |
| 0x497680 | `ClickSelect` | Ignored if the puppet is flagged unselectable (+0xB8 bit6) or is an enemy outside debug. With Ctrl held, only puppets with +0xB8 bit0 set can be selected. Increments `dword_59D9B4` (the order sequence). Issues command 4 (0x4460A0/0x4460C0: clear selection). A building, or a unit for which vfunc +0xEC is false, sends 0x8088 to its group; otherwise it issues command 5 (0x447D80: select object id +8, add when Shift / VK 16 is held). |
| 0x497770 | `ClickUp(x, y)` | Releases capture. Mouse mode `dword_4EA43C`: 0 → ClickSelect. 1 (order mode) on a friendly unit: with Ctrl, command 0x13 (force fire at the ground point, terrain z from 0x47B3F0) via 0x4A4440; otherwise command 4 plus 0x8088. 1 on an enemy: command 0x10 (attack object id) via 0x4493E0. |

**Keys.** `GetAsyncKeyState(VK_CONTROL=17)` at 0x497199, 0x4976B9 and 0x4977AD is the force-fire modifier.
`GetAsyncKeyState(VK_SHIFT=16)` at 0x49773B adds to the selection. Both are relevant to `ida-notes-thor/ida-map.txt`
(the remap list).

### 2.3 WBridge (0x497890-0x498750)

The constructor is 0x497890 (base 0x4862C0, vtable 0x4B0B00). The factory 0x446B30 makes type 0x1BC (Bridge
Military) through 0x48D510 and types 0x1BD-0x1BF (Bridge Country, City, Railway) through 0x48D050. A bridge spans
from (+0xC8, +0xCC) to (+0xD0, +0xD4), with length +0xD8.

| Addr | Name | What it does |
|---|---|---|
| 0x497900 | `Build` | Segment count +0xF0 = length/64 − 3; ramp length +0xF4 = length/count. It spawns count−1 segment objects (0x446B30 of type +0x20) along the line starting at −0.5 step, setting their +0x44 owner and positions, and clears their +0xB8 bit8 and sets bit7. |
| 0x497AC0 / 0x497E00 | `StitchStart/StitchEnd` | Copy shared vertex positions (6-byte ints / 12-byte floats) between adjacent segments' WOF piece vertex arrays so the deck is seamless. |
| 0x498110 | `Update(arg)` | Runs the segments; stitches neighbours; then recomputes each piece's 2D bounds (+0x68 to +0x70) from its 9×4 vertices. |
| 0x498300 | | Six calls to 0x49E630 (world-to-fixed vertex conversion), copying the deck points. |
| 0x4983D0 | `AddRouteNodes` | Inserts two WRouteFinder nodes (`dword_59E418`, 0x494220) at the bridge ends, 48 units (42400000h) beyond each end, at +0xE4 and +0xE8; then 0x486820. |
| 0x4984C0 | | Moves a sprite +0x1E by +0x30 and +0x26 by +0x40 (*unclear*). |
| 0x4984E0 | `HeightOn(obj)` | If the object is within the span (both end distances ≤ length), sets obj +0x84 to the deck height: interpolates the terrain z of both ends (0x47B170), adds 8 (flt_4B0C20 = −8 subtracted), and blends over the ramp length +0xF4 at each end. Height relative to the ground, never < 0. Returns 1 when on the bridge. |
| 0x498370 / 0x498380 / 0x4983A0 | | Thunks and wrappers (0x47F340, 0x4866C0 + vfunc +0x108, 0x4868A0). |

### 2.4 WFence: wire fences, walls and energy fencing (0x498750-0x498D30)

The constructor 0x498750 (vtable 0x4B0C28) is created for types 451-456 (0x1C3 Wire Fence, 0x1C4 Wire Wall, 0x1C5
Mined Wire Wall, 0x1C6 Electric, 0x1C7 Laser and 0x1C8 Plasma Fencing) via 0x48D780. Its ends are A (+0xE8, +0xEC)
and B (+0xF0, +0xF4).

| Addr | Name | What it does |
|---|---|---|
| 0x4987F0 / 0x498890 | `Save/Load` | WPuppet save/load plus four 16-bit values. Save re-posts them as 0x80A5 (A) and 0x80A6 (B), packed (x \| y<<16). |
| 0x498900 | `Snap` | Snaps both ends to landscape cells (`dword_59D940` +0x30 scale, +0x34 offset); heading +0xF8 from 0x43FDA0. |
| 0x4989F0 | `Init(arg)` | Damage-dependent: when the owner's health ratio (+0x7C / +0x78) < `dbl_4B0D48`, sets +0xC4 = segments × ratio (at least 1) and clears C0 bit0; otherwise sets bit0. Through 0x48A7D0 it either rebuilds the segments as type 0x17B (379) with the unit-table byte +0xA1 frame, or ticks them. Calls WPuppet::Init, then adds each segment (other than 0x17B) to the world (0x47D1E0, 0x480100). |
| 0x498B70 | `HandleMessage` | 0x80A5: A = l. 0x80A6: B = l. 0x80A7: vfunc +0x3C, then A = l. 0x80D1: A = l and also position +0x78/+0x7C. 0x80D2: B = l. Default → 0x495CB0. |
| 0x498CB0 | `BlockRoute` | +0x10C = 1; WRouteFinder 0x494400(node +0x104, node +0x108). *Guess*: cuts the link between the posts. |
| 0x498CE0 | `UnblockRoute` | +0x10C = 0; removes both nodes (0x494370). |
| 0x498D30 | | Returns false. |

### 2.5 Submerging and tunnelling vehicles (0x498D40-0x4993F0)

The constructor 0x498D40 is built on the vehicle base 0x490820, vtable 0x4B0D50. Its subclasses are 0x4935E0 for
types 0x16-0x18 (Submersible Mk1-3, vtable 0x4B03C8) and 0x493B90 for types 7-9 (Tunnelling Track Layer Mk1-3,
vtable 0x4B06F0). Fields:

- +0x190 bit0: underground/submerged. bit1: marked.
- +0x194: dive timer.
- +0x198: name label (RTypeBoxedText).
- +0x19C: GType 0x359 sprite (*guess*: bubbles or dust), depth 0x20.

| Addr | What it does |
|---|---|
| 0x498DB0 | Init: base 0x490E70, surfaces (vfunc +0x188(0)), clears bit1. |
| 0x498DE0 | Cleanup: base 0x490F10, height +0x84 = 0; if submerged, surfaces and repositions; releases +0x198 and +0x19C. |
| 0x498E90 | Messages 0x80EE / 0x80EF: if not busy (vfunc +0x118), start the dive (vfunc +0x184(1)). Others → 0x490A70. |
| 0x498EE0 | Can be hit: while submerged only weapon types 0x2CE-0x2D0 hit it; otherwise the base rule 0x491020. |
| 0x498F20 | Tick: dive animation, 64 ticks. Going down: height = (t−64)×3, from 0 to −192, then submerged (vfunc +0x188(1)). Coming up: height = −3t, back to 0, then surfaced (vfunc +0x188(0)). Calls vfuncs +0x10C, +0x8C, +0x178 and +0x174 to manage the label and sprite. |
| 0x499010 | Collision/other through 0x491110, only when surfaced. |
| 0x499040 | Draw: shifts the model's screen y (+0x26) by height × flt_4B0EE0; places the label 16 px below and the sprite at the unit. |
| 0x4990D0 | `StartDive(bool)`: true marks bit1; otherwise begins surfacing if submerged. Timer = 64; vfunc +0x170. |
| 0x499120 | If busy, cancels (vfunc +0x184(0)); 0x491590. |
| 0x499150 | `SetSubmerged(bool)`: shows or hides the label and sprite (vfuncs +0x17C/+0x174 or +0x180/+0x178); group 0x4807E0(0). |
| 0x4991D0 | Makes the label: RTypeBoxedText with the name from 0x440720(font 0x6F, colour Human `dword_4DAF90` / Martian `dword_4DAF88`, x, y). |
| 0x4992D0 | Makes the GType 0x359 sprite at +0x19C, depth 0x20. |
| 0x499350 / 0x4993A0 | Release the label and the sprite. |
| 0x4993F0 | Skips the base draw while marked and not visible. |

### 2.6 Battle-map A* cost and node array (0x499420-0x4998D0)

| Addr | What it does |
|---|---|
| 0x499420 | Constructor of a cost/graph object for AStar (base 0x44F280, vtable 0x4B0EE8), called from 0x494548 (WRouteFinder). Fields: +4 unit, +8, +0xC goal node, +0x14/+0x18 node x/y arrays (s16), +0x1C node region bytes, +0x20 arrival radius, +0x10 = landscape +0x24. |
| 0x4994B0 | `LineCost(x0, y0, x1, y1)`: Bresenham over landscape cells summing 0x480680(x, y, 1) (cell cost); returns −1 if any cell is impassable, else length × average cost. |
| 0x499660 | `StepCost(a, b)`: an 8-direction code from the signs of dx, dy and \|dx\|−\|dy\| through table `dword_4D0AF8`, a turn penalty from `dword_4D09F8[dir_ab + dir_goal×8]` × 5/16 + 1, times the line cost (or straight distance × unit +0x5A when both nodes share a region byte). |
| 0x499800 | `IsGoal(a, b)`: distance between nodes ≤ +0x20. |
| 0x499850 / 0x4998B0 / 0x4998D0 | "BatRoute" container (0x435640 named object, vtable 0x4B0EF8): an array of n×16 bytes at +0x10, n at +0x14. Created by 0x4946EA. |

### 2.7 Briefing and Debriefing (0x4998F0-0x49B900)

Both are full-screen views (base 0x4061B0): backdrop GType 0x91, palette group 0x2EE (hb), and up to 32 text objects
at +0xA0 (count `dword_59E614` / `dword_59E664`). The war map makes them: the Briefing at 0x41A17A and the Debriefing
at 0x41A108.

| Addr | Name | What it does |
|---|---|---|
| 0x4998F0 | static init | `[Briefing] "Delay"`, default 30 (struct 0x59E628, value `dword_59E63C`). |
| 0x499930 | `Briefing::Briefing` | vtable 0x4B0F08; schedules WM_COMMAND 0x71 after Delay ticks (0x4355D0 → timer queue 0x4DAF28). |
| 0x4999E0 | `Briefing::Create` | Builds the layout below. |
| 0x49A5A0 | `Briefing::~Briefing` | Releases the texts and the minimap; 0x406470. |
| 0x49A660 | `Briefing::Draw` | Backdrop (+0x88), the texts, the minimap (+0x9C), the cursor (0x42E220 at `dword_4E9250`, `dy`), then flip. |
| 0x49A700 | `Briefing::OnMsg` | WM_KEYDOWN (any key), WM_LBUTTONDOWN and WM_RBUTTONDOWN post WM_COMMAND 0x98 at once. Others → 0x4065E0. |
| 0x49A770 | static init | `[Debriefing] "Delay"`, default 300 (struct 0x59E648, value `dword_59E65C`). |
| 0x49A7B0 | `Debriefing::Debriefing` | vtable 0x4B0F50; +0x120 = 1 (first draw pending). |
| 0x49A820 | `Debriefing::Create` | Builds the layout below. |
| 0x49B750 | `Debriefing::~Debriefing` | |
| 0x49B830 | `Debriefing::Draw` | On the first draw, posts 0x8017 after Delay ticks (queue 0x4D24C0 via 0x4355A0). Then draws like the Briefing. |
| 0x49B900 | `Debriefing::OnMsg` | Key or click: post 0x99 after 5 ticks. Message 2 (WM_DESTROY): post 0x8090. |

**Briefing layout (0x4999E0).**

- Font 0x73; line height = font height + 2 (`dword_59E61C`).
- Columns: Human games x = 0x37 (55) and 0x11D (285); Martian games 0x55 (85) and 0x114 (276) (`dword_59E610` / `dword_59E618`). The centre column is half-way between them.
- First y (`dword_59E620`) = 170 (0xAA), or 72 when `dword_4E9B74`+0x24 is non-zero.
- Colours: `word_4B9430` (heading), `word_4B9434` (section) and `word_4B9438` (values).
- It fades in the briefing sound ("Fading up briefing sound over 1 second").

The lines, in order:

1. Heading, centred: 0x497 "VIEWING" if the sector says no (0x43D010); otherwise 0x498 "ATTACKING" if the sector's owner (0x43CA30) ≠ the player, else 0x499 "DEFENDING". Then "\n" and the sector name (`dword_4E9B70`+0x60 → +0xC). Gap +16.
2. 0x4A0 "Local Conditions:", centred, gap +16.
3. 0x4A1 "Date/Time:" in the left column (align 1). The value goes in the right column (align 8): Human games "hh:mmap dd mmm yyyy" from the calendar (0x41B440 on 0x4EA0B0); Martian games `"%07.2f"` of the game time (`dword_4E9B80`+0x28 × dbl_4B0F48).
4. If the sector has forces (0x4216D0 on sector +0x4C, or +0x54): 0x4A4 "Forces Present:", centred.
5. 0x4A6 "Units (Vehicles):" with `"%d (%d)"` (0x4216D0, 0x421F90 of +0x4C).
6. 0x4A7 "Facilities:" with `"%d"` (+0x54).
7. An RTypeMiniBattleMap (0x4A1F90, GType from sector +0x48) centred in a box at x 354 + (255 − w)/2, y 95 + (285 − h)/2.

**Debriefing layout (0x49A820).** The same columns and fonts. The result comes from the battle object
`dword_4E9B7C`:

| Condition | Sound | Heading |
|---|---|---|
| +0x54 == 0 and `byte_4E9B94` == 0 | 0x18E | 0x4A8 "RETURNING TO WAR ROOMS" |
| `byte_4E9B94` set | 0x190 | 0x4AA "DEFEAT" |
| +0x54 set and (+0x28 + +0x48) ≠ 0 | 0x18F | 0x4A9 "VICTORY!" |
| +0x54 set and (+0x28 + +0x48) == 0 | 0x190 | 0x4AA "DEFEAT" |

The sound handle is kept at +0x98.

After the heading come 0x4AB "The Martians in this sector were eliminated." or 0x4AC "Martians are occupying this
sector.", word-wrapped (0x4161E0, 0x441A40) to the column span; then sub_464E60(60) fades the music in. Then 0x4AD
"Forces lost:" with 0x4A6 / 0x4A7 counts, 0x4AE "Enemy forces destroyed:" with the same counts, and the minimap as in
the Briefing. The exact meaning of +0x28/+0x48/+0x54 is *guessed* from the branch shape.

### 2.8 War map menu, battle map menu and the debug creation menu (0x49B980-0x49BE50)

| Addr | What it does |
|---|---|
| 0x49B980 | **Warmap menu** 0x80B5 (WFullScreenMenu 0x40A060, Escape → 0x8123, vtable 0x4B0F90; called from 0x41CFE0). Heading 0x3FC "Campaign Options" at y 0x78. Buttons: 0x8123 → 0x405 "Resume Campaign (Escape)" (focused), 0x90 → 0x40A "Return to Main Menu", 0x8117 → 0x412 "Critical event options", 0x3ED → 0x411 "Sound Control". **Bottom button: command 0x80B6 → 0x404 "Save & Exit Game".** |
| 0x49BD50 | **Battlemap menu** 0x80B8 (vtable 0x4B1010; called from 0x48517E). Heading 0x3FD "Battle Options" at y 0x78; 0x8123 → 0x405 (focused); **bottom button 0x3ED → 0x411 "Sound Control"**. That is all: there is no save and no quit. |
| 0x49BE30 | Battle menu handler: forwards to 0x40A1B0. |
| 0x49BAA0 | **CreationMenu** (debug). A popup list (0x42F730, command 0x801B, style 0x10000000) with a "Create..." label (0x4303A0) and a "Close" button (0x42FA70, 0x8123), font 0x71; owner at +0x84. Built by the object-type table code 0x43B2A5 to 0x43E03B. |
| 0x49BBF0 | `CreationMenu::Add(cmd, id, name)`: adds a button (0x42FC20); every type uses cmd 0x8027 with the object type id. It grows the menu. This is where the type-name list comes from. |
| 0x49BCC0 | CreationMenu WM_COMMAND: closes (0x42E790(0,0) for 0x8123, (3,0) otherwise) and forwards the command to its owner. |
| 0x49BE50 | `BorderSectors(b)` returns `&dword_59E6F8[b*2]` (the pair of sectors either side of border b). Used by 0x41D9B0 (border colouring, 0x41DB6E). |

### 2.9 War map sector highlight (0x49BE70-0x49C070, 0x49C8D0-0x49CAB0)

| Addr | What it does |
|---|---|
| 0x49BE70 | Constructor of the sector-map sprite (base sprite 0x40EC50(GType), vtable 0x4B1050). Called by 0x417F3E, 0x418A8B, 0x4201B3, 0x420411, 0x4206CF and 0x422D13. It sets all 256 value levels `dword_59E9B0[v]` to 11, and the sector copy `dword_59E678[v]` (v < 31) to 11. Once (`dword_59EDB0` == 0) it builds border → (sector, sector) in `dword_59E6F8` from the border table 0x4D0C00 (8 dwords per sector, 0 ends, rows stop at the "RTypeArrow::~RTypeArrow" string, i.e. 0x4D0C00 + 32×30), and stores the highest border number in `dword_59EDB0`. |
| 0x49BF50 | `SectorAt(x, y)`: reads the sector-map pixel (0x40FDD0) with bounds 0..w and 0..h inclusive. Values below 31 are returned as they are. For a border pixel (≥ 31), b = value − 31 and it returns the **first sector (1-based) whose border list contains b**, else 0. Called from 0x419220 (war map mouse) and 0x4281A7 (counter grid). |
| 0x49C000 | `SetLevel(value, level)`: `dword_59E9B0[v]` (and `dword_59E678[v]` if v < 31). Called by 0x41DB33 and 0x41DF23. |
| 0x49C030 | `GetSectorLevel(v)` = `dword_59E678[v]`. |
| 0x49C050 / 0x49C070 | Set and get the border level `dword_59EA2C[b]` (= `dword_59E9B0[31 + b]`). |
| 0x49C8D0 | Constructor of the highlight-map renderer (vtable 0x4B10B0; sprite GType + the sector-map object at +0x44). Called by 0x422D4C and 0x4238DF. |
| 0x49C990 | `Draw`: for each of the 256 values it loads the remap colour = ShadeLevel(own palette slot +0x18, `dword_59E9B0[v]`) (0x40B500 → 0x40FDF0). It sets the sector-map pixels as the index source (0x40FE00), then blits its own frame (+0x38>>8) with the clip rect (+0x28..+0x2E) in mode 4 through 0x40FD80. |
| 0x49C920 | Destructor. |

### 2.10 Drag boxes: WMouseDrag and placement (0x49C080-0x49C720, 0x4A03A0-0x4A14BC)

**WMouseDrag base** (constructor 0x49C080, vtable 0x4B1070):

- Fields: +4/+8 start, +0xC/+0x10 current, +0x14..+0x17 flags (+0x16 active, +0x17 dragging), +0x18/+0x1C threshold 4 px, +0x20..+0x2C four corner sprites.
- Vtable: 0 dtor 0x49C0B0, +4 Start 0x49C230, +8 0x49C340 (returns false), +0xC Move 0x49C250, +0x10 End 0x49C350, +0x14 Draw 0x49C3E0, +0x18 0x49C720 (normalise the rect via 0x4163A0), +0x1C Clamp 0x49C6B0 (at most 640 px either way).
- Move: once the pointer is more than 4 px away, it starts dragging and makes four GType 6 sprites (frames 0-3, flags 3, depth 0x5DC0) as corners.
- Draw: corner sprites at the rect corners inset 7. When a side is ≥ 23 px, it adds edge lines as two-colour RType lines (0x49C630 → 0x4A21F0, depth 0x5DC0) with colours `dword_4DAF9C` and `dword_4DAFB8`, inset 12 px.
- End: returns true if it was a drag.

**Placement drags** (battle map, created by 0x485F60's cursor-mode switch at 0x4860A3 and 0x4860E7):

- **0x4A13B0** (vtable 0x4B1270). Building placement for type `dword_4EA440`. Footprint = (unit table word +0x46 × 2 + 14) & ~15 (a 16-px grid); +0x41 = 1 for ShipYard Lvl 1-3 (0x27-0x29), *guess*: a water placement rule. End (0x4A1460) posts 0x80A4 {type, x, y, x2, y2, 0xDEADDEAD} to the battle controller `dword_4EA448` (vfunc +0x20), then 0x8137. Its Draw is 0x4A14C0 (next piece).
- **0x4A03A0** (vtable 0x4B1240). Placement with a flag (+0x38, set to 1 by its caller in cursor mode 5) and a footprint from unit table word +0x44. The two-click logic in Start (0x4A0490): the first click sets the anchor (with terrain z from 0x47B3F0); the second posts 0x80A4 {type, anchor, end, 0xDEADDEAD} and, if +0x38, 0x8137 (*guess*: chained fence or wall placement). 0x4A0590 releases capture. 0x4A05B0 is End.
- **0x4A0630** (3448 bytes) draws the footprint. The mouse position is smoothed (the average of the old and new), and its corners are tested with 0x47B610 (area buildable). The outline is drawn in `dword_4DAF80` (valid) or `dword_4DAF7C` (invalid) as up to 10 line objects (0x4A21F0), each following the terrain height (0x47B170) in ±16 / ±32 steps (flt_4B1260..4B126C).

### 2.11 Render types (0x49C740-0x49DA40)

All are RType render objects. Common fields: +0x14 flags (bit0 draws at 50%), +0x18 palette slot, +0x1A/+0x1C
screen position, +0x1E depth, +0x28..+0x2E bounds, +0x34 brightness/level. Vtable slot +0x10 is `Draw(surface)`;
the surface origin is +0x4E/+0x52.

| Addr | Class | What it does |
|---|---|---|
| 0x49C740 / 0x49C7C0 | **RTypeRect** (vtable 0x4B1090) | Rect x1, x2, y1, y2 (+0x28..+0x2E), filled flag +0x38, colour +0x3C. Filled → 0x450528 (asm fill); outline → 4 lines (0x4A24F0). Created at 0x422E6C (war map) and 0x486A1D (battle map). |
| 0x49CAC0 / 0x49CDB0 / 0x49CD40 | **RTypeArrow** | Section 2.12. |
| 0x49D260 / 0x49D2C0 / 0x49D300 | **RTypeBoxedText** (vtable 0x4B10F0) | A text object (0x40F620 or 0x40F6D0) with a box colour at +0x2A8. Draw: fills the bounds (0x450528) with the box colour; draws a 1-px outline in `dword_4DAF88` (Human) or `dword_4DAF9C` (Martian); draws the text offset by (1, 1). Used for the route arrival label (0x424776), unit labels (0x499239), tooltips (0x49D4B8), the InfoPage (0x4A3F68) and 0x42B51A. |
| 0x49D430 | static init | `[Screen] "Tool Tip Offset"`, default 16 (`dword_4D0FD8`). |
| 0x49D470 | **Tooltip** (vtable 0x4B1110) | BoxedText, font 0x6F; box colour Human `dword_4DAF90` / Martian `dword_4DAF88`; flags \|= 6 (centred); alignment 6. It asks its owner for a position with message 0x80D0 (point = mouse x, y − ToolTipOffset). If the owner says nothing, it uses (x + 32, y − offset + 32). Centred on that point, clamped to the screen width − 4 (0x4336D0 → +0x34) and to y ≥ 4. The box grows by 2 on each side; depth 0x7D00. Created by the widget code 0x433A41. |
| 0x49D620 / 0x49D670 | scaled sprite (vtable 0x4B1130, GType 0xC) | Draws a sprite frame scaled through the assembly blitter entries 0x44F890 / 0x44F967 / 0x44FE7F. Created at 0x44AE27 (DeeDraw; *guess*: the cursor or the wait icon). |
| 0x49D710 / 0x49D7E0 | atlas sprite (vtable 0x4B1150) | Tile size +0x3C; it picks a sub-image (+0x3E) out of a 256-wide texture by frame index. Created by 0x47EDB6 (landscape) and 0x495126. |
| 0x49D820 / 0x49D930 | textured quad | Four corners at +0x40..+0x4E, bounding box; drawn with 0x410090 (mode 6, \|8 if translucent). |
| 0x49D9C0 | sprite with a 1.0 scale (vtable 0x4B1170) | Landscape object (0x47ED7C). |

### 2.12 RTypeArrow: war map route arrow (0x49CAC0-0x49D254)

The constructor `0x49CAC0(style, nPoints, points, stepsPerStretch)` is called from RArrow 0x42483D (0x4244E0).

- The style table is at `dword_59EDB8` (24 bytes per style, OBJ.ojd record 14). +0x00: GType. Then the four ints go to the object's +0x44 w0 (tail half width), +0x48 w1 (half width at the head end), +0x4C head half width and +0x50 head length; the float goes to +0x58 (wing offset factor).
- Points are 12 bytes each: float x, float y, heading (4096 steps to a turn, 0x400 = 90°). They are copied, rounded to whole pixels, into a "RArrow" memory block (0x4148A0, tag 0x1100); the pointer is at +0x38 and the count at +0x40.
- +0x54 = stepsPerStretch. +0x5C = the first index drawn (0 at construction; the owner raises it as the route is travelled — *unverified*, the writer isn't in this piece). +0x60 = texture (0x40E980). Depth +0x1E = 3. The bounds are the points' box padded by the largest of the four numbers.

**Draw, 0x49CDB0**:

- **Mode** 4, or 0xC when flag bit0 (+0x14) is set. The shade is ShadeLevel(slot +0x18, 0) → 0x410010.
- **Perpendicular.** For each point, 0x43FE90(heading + 0x400, halfWidth) gives (dx, dy). Left = p + (dx, dy), right = p − (dx, dy). The game uses the stored heading, not a direction worked out from neighbouring points.
- **Half width** starts at w0 and goes up by (w1 − w0) / nPoints at each point.
- **Body.** For i = 1 .. nPoints − steps/3 − 1, when i > +0x5C: a textured quad 0x410090(prevLeft, curLeft, curRight, prevRight, tex, 64, mode). Every quad maps the 64-wide strip.
- **Head.** At the last body point B (its heading h): D1 = 0x43FE90(h, headHalfWidth), D2 = 0x43FE90(h, headLength), and k = float +0x58 (0 for the Human styles, −0.4 for the Martian ones).
  - Wings: W1 = B + (D1.y, −D1.x) − k·D1 and W2 = B − (D1.y, −D1.x) − k·D1.
  - Tip T = B + D2. L and R are the last body edge points.
  - Three textured triangles (0x410190, argument order x1, y1, x2, y2, x3, y3, then uv for vertices 1, 3 and 2):
    1. (W1, R, T) with uv W1 (64, 63), R (112, 63), T (127, 0)
    2. (R, L, T) with the texture shifted 112 columns: R (112, 63), L (144, 63), T (128, 0)
    3. (L, W2, T) with uv L (144, 63), W2 (191, 63), T (128, 0)

So the head's base in the texture runs along row 63 from u = 64 to 191, with the apex at (128, 0). The two middle
base points are the body's own edge points, not points on the wing line.

### 2.13 RTypeWOF, RTypeISO and WOFLib (0x49DA50-0x49F230)

| Addr | What it does |
|---|---|
| 0x49DA50 | **RTypeWOF** constructor (base 0x4A2A20, vtable 0x4B1190; created by 0x47ECF5, the landscape objects). Brightness 1.0 at +0x40. Loads the model (0x49DCA0). Bounds = ± the per-GType radius `word_4F0532[g*6]`; screen y offset +0x26 = 2 × radius. |
| 0x49DCA0 | `SetModel(gtype)`: +0x44 = GType data; makes a WOFLib (0x78 bytes, 0x49E330) at +0x48; scales its +0x3C/+0x40/+0x44 by the per-GType float `flt_4F0534[g*3]`, and +0x60 = `word_4F0530[g*6]`; transforms (0x49E690 with zero angles). +0x4C/+0x50 = the sun angles `flt_4DA778` / `flt_4DAE28`; +0x34 = 255. |
| 0x49DBD0 | `Draw`: shade level 0 of slot +0x18; bounds from the model's projected box (+0x68..+0x70; it marks itself dirty, \|0x10, when they change); then 0x49EC00(ox, oy, surface +0x60, brightness +0x34/255). |
| 0x49DDA0 | **RTypeISO** constructor (base 0x4A2A20, vtable 0x4B11B8; created by 0x47EBD0). Calls Change(gtype). |
| 0x49E1E0 | `RTypeISO::Change(gtype)`: ISO header (+0x0E width, +0x12, +0x16 height → +0x26 = height × 256 / 360 (×0x2D4279A3 >> 6)); bounds ± width/2. Allocates "ISOLight" (3 bytes per point, from header +0x06 or +0x02); sun angles; brightness 255; relights (0x49DF40). |
| 0x49DF40 | Relight(brightness): per-point light from the sun (0x40CA90 × `flt_4DA764`). |
| 0x49E2C0 | `SetBrightness(b)`: relights if b changed, then 0x40EC30. |
| 0x49DE90 | `RTypeISO::Draw`: ISOLib 0x4113F0 with the point lights (0x410E40 + 0x3400 → 0x410EE0), shade 0x410E50 and brightness/255. |
| 0x49DE00 / 0x49DE20 | Destructor ("RTypeISO::~RTypeISO"). |
| 0x49E2F0 | `[Game] "WOF Brightness"`, default 2.5 (value `flt_59F364`). |
| 0x49E330 | **WOFLib::WOFLib(data)** (vtable 0x4B11E0). Sets pointers into the WOF header: +0x24 → pieces at +0x30 (0x61 bytes each, vertex count at +0x11), +0x2C, +0x0C, +0x28, +0x20 (+0x18). Piece count +0x64 = header +0; vertex total +0x66 = header +8; +0x62 = (header +0x1C ≠ 0); scale +0x38 = header +0x14; +0x4C = header +0x10. Allocates "WorldPts" (20 bytes a piece + 12 a vertex) and "ScrnPts" (8 a piece + 6 a vertex); logs "Inconsistent number of vertices" if the piece counts don't add up; per-light array (`dword_4B8E28` + 1 entries; that is 2). |
| 0x49E630 | Converts a world point to landscape fixed point (`dword_59D940`). |
| 0x49E690 | `Transform(angles…)`: builds the rotation (0x4A2C50, 0x4A2E00, 0x4A2FA0, 0x4A3020 matrix helpers; ±16384 limits) and walks the piece tree (0x49E900). |
| 0x49E900 | Recursive piece transform; keeps the projected bounds +0x68..+0x72. |
| 0x49EC00 | `Render`: collects the faces, sorts them by depth with qsort (CompareFunction: painter's algorithm), draws them (0x49EEF0). |
| 0x49ED90 | `FaceLight`: the face normal (cross product) · the sun (0x40CA90 with `flt_4DA778` / `flt_4DAE28`) × `dword_4DA774` × 256, plus each point light in the list (+0x14; position +0x38..+0x3E; falloff 0x40C930 with 0x1C) × WOF Brightness. |
| 0x49EEF0 | Draws each face with 0x410070 using the shade remap bytes `byte_59F368` / `byte_59F768` (256 each). |

### 2.14 WLandscapeView: battle-map terrain view (0x49F240-0x4A0370)

The constructor 0x49F240 is created by 0x484D7D. It is built on the screen/view class 0x40CF00 and holds the
landscape (arg → +0xF4, with map data at +0x68) and copies the map parameters (+0x12C..+0x174). The view count is
`dword_59FD68`, and the mini-map view is `dword_4D2550`+0x84 → +0x178. Vtable 0x4B11F8.

| Addr | What it does |
|---|---|
| 0x49F4E0 | `UpdateViewport`: works out the visible cell rectangle and tells the mini-map (0x434BD0). |
| 0x49F5D0 | Sets the ISOLib frame size (0x410F60). |
| 0x49F610 | `Render(objects, rect…)`: ShadeLevel(0x14, 0) at +0x16C; works out the first row and the origin (+0x4E/+0x52); 0x49FDA0 sets up the rows. Then it alternates: draw one terrain row (0x49F890), then draw every object whose (+0x26 + +0x1E) is above the row's y — terrain and objects are interleaved by row (painter's algorithm). |
| 0x49F770 | Row bounds helper (+0x12C, +0x144, +0x154, +0x158, +0x164). |
| 0x49F890 | Draws one terrain row through the ISOLib tile primitives (0x410E40..0x410F40; switch tables at 0x49FC6C, 0x49FC90 and 0x49FCD8 inside the function; `byte_4DA660` tile flags). |
| 0x49FDA0 | Row setup (ISOLib 0x410E90..0x410F10). |
| 0x4A0140 | Screen ↔ terrain helper (×256, ×1/65536; 0x40C930). |
| 0x49F430 | Destructor. |

## 3. Data structures and globals

**WPuppet** (offsets used here):

- +0x20 type; +0x24 side (0 = the player's); +0x34 group (+0x1D8 flags, +0x1DC kind); +0x38 controller; +0x48 war-map object; +0x70 sub-object list.
- +0x78/+0x7C/+0x80 position; +0x84 height above the ground.
- +0xB8 flags: bit0 alive, bit1 has size, bit6 unselectable. +0xC0 flags.
- +0xC8/+0xCC/+0xD0 goal; +0xD4 goal distance (1e6 when idle); +0xD8 stance; +0xDA rescan counter; +0xDC state timer; +0xE0 state; +0xE4 flags (bit0, bit1 moving).

**Unit table** (0x51A0F0, 0xB0 bytes a type), fields used here:

| Offset | Size | Meaning |
|---|---|---|
| +0x41 | byte | height/size, used by Approach |
| +0x43 | byte | default stance |
| +0x44 | word | placement footprint for 0x4A03A0 |
| +0x46 | word | building footprint |
| +0x48 | word | sight radius (fog, targeting, broadcasts) |
| +0x5A | | per-unit straight-line cost factor (unit +0x5A in the A*) |
| +0x92 | word | rescan period |
| +0xA0 | byte | sound volume |
| +0xA1 | byte | fence segment frame |

**Globals:**

| Address | What it is |
|---|---|
| `dword_4D2550` | the world (+0x20 object list, +0x84 fog / mini-map view) |
| `dword_4E9B70` | the battle world (+0x60 the current battle sector record: +0xC name, +0x48 minimap GType, +0x4C units, +0x54 facilities; +0x118 object list) |
| `dword_4E9B74` / `dword_4E9B78` | the player and opposing side controllers (+0x24 side) |
| `dword_4E9B7C` | the battle result |
| `byte_4E9B94` | the player lost |
| `dword_4E9B80` | the calendar (+0x28 hours) |
| `dword_59D940` | WLandscape (+0x24, +0x30 cell scale, +0x34 offset) |
| `dword_59E418` | WRouteFinder |
| `dword_4EA43C` | battle mouse mode |
| `dword_4EA440` | selected construction type |
| `dword_4EA448` | battle controller (receives 0x80A4 / 0x8137) |
| `dword_59D9B4` | order sequence number |
| `byte_4D1279` | debug |
| `byte_589130` / `byte_589134` | research: detect mines / detect tunnellers and submersibles (*guess*) |

**Colours:** `dword_4DAF7C` / `dword_4DAF80` placement invalid / valid; `dword_4DAF88` / `dword_4DAF90` /
`dword_4DAF9C` / `dword_4DAFB8` UI box and outline colours per side; `word_4B9430` / `word_4B9434` / `word_4B9438`
briefing text levels.

**Highlight tables:** `dword_59E9B0`[256] value → level; `dword_59E678`[31] sector levels; `dword_59EA2C` border
levels (alias of 0x59E9B0 + 31×4); `dword_59E6F8`[b][2] border → sectors; `dword_59EDB0` max border.

**Arrow style table:** `dword_59EDB8` (24 bytes a style).

**Per-GType model table:** `word_4F0530` / `flt_4F0534` / `word_4F0532` (12 bytes a GType).

## 4. Messages, commands, strings and tuning

**Settings registered in this piece:**

| Setting | Default | Address |
|---|---|---|
| `[Tweak] "Audible radius multiplier"` | 1.0 | 0x495620 → 0x59E438 |
| `[Debug] "Show Orders"` | 0 | 0x496520 |
| `[Briefing] "Delay"` | 30 | 0x4998F0 |
| `[Debriefing] "Delay"` | 300 | 0x49A770 |
| `[Screen] "Tool Tip Offset"` | 16 | 0x49D430 |
| `[Game] "WOF Brightness"` | 2.5 | 0x49E2F0 |
| Stat "Puppets objects left" | | 0x4955E0 |

Used here but registered elsewhere: `[BattleMap] "Fog update freq"` (15, 0x434580).

**WM_COMMAND ids posted to WGame / the owner:** 0x71 (the Briefing's timeout: go to the battle, *guess*), 0x98 (a key
or click on the Briefing), 0x99 (a key or click on the Debriefing, after 5 ticks), 0x8017 (the Debriefing's timeout),
0x8090 (Debriefing WM_DESTROY). Menus: 0x80B5, 0x80B8, 0x8123, 0x90, 0x8117, 0x3ED, 0x80B6. Debug: 0x801B, 0x8027.

**Object messages:** 0x8012, 0x802F, 0x8031, 0x805A, 0x805C, 0x8079, 0x807B, 0x807C, 0x8088, 0x808A, 0x80A4,
0x80A5, 0x80A6, 0x80A7, 0x80B1, 0x80B2, 0x80D0 (tooltip position query), 0x80D1, 0x80D2, 0x80E1-0x80E3, 0x80EE,
0x80EF, 0x80F0, 0x80F4-0x80F9, 0x8111, 0x8137.

**Order commands** (the network/playback command objects 0x4460A0, 0x447D80, 0x4493E0, 0x4A4440): 4 clear
selection, 5 select (+Shift adds), 0x10 attack object, 0x13 force-fire at a point.

**Sounds:** 0x115 (command building), 0x18E (returning), 0x18F (victory), 0x190 (defeat).

**Text ids:** 0x3FC, 0x3FD, 0x404, 0x405, 0x40A, 0x411, 0x412, 0x497-0x4A1, 0x4A4, 0x4A6-0x4AE.

**Format strings:** "hh:mmap dd mmm yyyy", "%07.2f", "%d (%d)", "%d".

**GTypes:** 5 (selection brackets), 6 (drag corners), 7 (health bar), 0x91 (briefing backdrop), 0x359 (submersible
sprite), 0x18 / 0x19 (arrow textures); fonts 0x6F, 0x71, 0x73.

**Constants:**

| Value | Where |
|---|---|
| Dive 64 ticks, depth 192 | 0x498F20 |
| Bridge segment 64 px, end nodes +48 | 0x497900, 0x4983D0 |
| State timer (l+2)×150; reveal 240 ticks | 0x495CB0 |
| Drag threshold 4 px; edges from 23 px; clamp 640 | WMouseDrag |
| Placement grid 16 | 0x4A03A0, 0x4A13B0 |
| Rescan jitter rand%30 | 0x495740 |

## 5. Cross-references to other pieces

**Callers into this piece:**

| From | What it reaches |
|---|---|
| WWarMapWorld / war map (0x417F3E, 0x418A8B, 0x419220, 0x41A108 / 0x41A17A, 0x41CFE0, 0x41D9B0, 0x41DB24..0x41DF23, 0x4201B3, 0x422D4C, 0x422E6C, 0x4238DF, 0x4244E0 / 0x424776 / 0x42483D, 0x4281A7, 0x42B51A) | the sector map, briefing, menus, highlight, arrows and labels |
| Factory 0x446B30 | bridges, fences, submersibles and tunnellers |
| Vehicles and puppets (0x489xxx-0x4935xx) | the subclass constructors |
| WRouteFinder 0x4946EA / 0x494548 | BatRoute and the cost object |
| Battle map 0x484D7D, 0x48517E, 0x485F60 | the view, the battle menu and the placement drags |
| Landscape 0x47EBD0..0x47EDB6 | the render types |
| UI widgets 0x433A41 | tooltips |
| InfoPage 0x4A3F68 | boxed text |
| DeeDraw 0x44AE27 | the scaled sprite |

**Callees outside this piece:** the sprite/RType base (0x40E700, 0x40E910, 0x40E980, 0x40EC30, 0x40EC50,
0x40F000, 0x40EBE0); text 0x40F620, 0x40F6D0, 0x40F770, 0x40F9F0, 0x40FB90; shade 0x40B500; polygons 0x410010,
0x410070, 0x410090, 0x410190; ISOLib 0x410E40-0x4113F0; lines 0x4A21F0 and 0x4A24F0; minimap 0x4A1F90; matrices
0x4A2A20-0x4A3020; geometry 0x43FDA0, 0x43FE90, 0x43FF00; landscape 0x47B170, 0x47B3F0, 0x47B610, 0x480680;
sound 0x464EE0 and 0x464E60; text 0x4146F0; the full-screen view 0x4061B0, 0x4062E0, 0x406470, 0x4065E0; menus
0x40A060, 0x40A7D0, 0x40A9B0, 0x40AB00; research 0x45B890; fog 0x434A80; route finder 0x494220, 0x494370,
0x494400; timers 0x4355A0 and 0x4355D0.

## 6. Missing in WoWEngine: candidate tasks

WoWEngine (`src/`) has a front end and a partial war map. It has no battle map, so everything in 2.1-2.6, 2.10,
2.13 and 2.14 is missing. The tasks are ordered by how soon the current war-map and front-end work needs them.

### P1: war map and front end, usable now

1. **Arrow head as three triangles** (0x49CDB0, section 2.12). `WarMap::DrawRouteArrow` draws one triangle from u 64 to 191. The game draws a fan W1-R-L-W2 with the exact uv above, wings offset by −k·D1 (k = style float +0x58: −0.4 for the Martian styles, so their wings sit 0.4 × head half width toward the tip), and the middle base points at the body's own edges. It also uses each point's stored heading +0x400 for the perpendicular; the width step is (w1 − w0) / nPoints; quads map the whole 64-wide strip; mode 0xC is used when the arrow is translucent. Drawing starts after index +0x5C. Needs the heading values from the curve builder 0x424A40 (part 3/4).
2. **Arrival label as RTypeBoxedText** (0x49D260 / 0x49D300). The engine draws plain text. The game fills the text bounds with a box colour (the constructor's argument) and draws a 1-px outline in `dword_4DAF88` (Human) or `dword_4DAF9C` (Martian), with the text at (+1, +1). Check which colour 0x424776 passes.
3. **War map menu's bottom button** (0x49B980): the original sends **0x80B6** ("Save & Exit Game", string 0x404). `FrontEnd::WarMapMenu` sends `CmdQuitMenu`. Confirm what WGame does with 0x80B6 (save, then quit or return) and match it. The heading y is 0x78 rather than 0xA0, which makes no difference because the bottom button centres the stack.
4. **Border pixels hit-test** (0x49BF50). The engine's `SectorAt` returns 0 on a border pixel (value ≥ 31). The game returns the lowest-numbered sector whose border list (0x4D0C00) contains (value − 31), so hovering or clicking a border line selects that sector. The bounds test is inclusive of w and h.
5. **Tooltips** (0x49D470 plus `[Screen] "Tool Tip Offset"` = 16, created by 0x433A41). Needed for the CD Player transport tooltips (0x4B7-0x4BD; FRONTEND.md "still to do") and the war-map panel buttons. Font 0x6F, side colours, owner query 0x80D0, fallback +32/+32, clamped to the screen width − 4 and y ≥ 4, box grown by 2 px, drawn on top (depth 0x7D00).
6. **Briefing screen** (0x499930 / 0x4999E0) for BattleInSector and for viewing a sector. This is the full layout of section 2.7: columns by side, heading VIEWING/ATTACKING/DEFENDING, date in Human or Martian format, Forces Present with the unit and facility counts, and the mini battle map (0x4A1F90, `maps\hmin%02d.spr` / `mmin%02d.spr`) in the box 354..609 × 95..380. Key or click → 0x98; timeout `[Briefing] Delay` 30 → 0x71. It can be built before the battle map exists, as the screen shown when the war map stops a counter for a battle.
7. **Debriefing screen** (0x49A7B0 / 0x49A820): VICTORY / DEFEAT / RETURNING TO WAR ROOMS with sounds 0x18F / 0x190 / 0x18E, the result text 0x4AB / 0x4AC, Forces lost and Enemy forces destroyed counts, `[Debriefing] Delay` 300 → 0x8017, key or click → 0x99 after 5 ticks. Needs a battle result (or an auto-resolve stand-in).

### P2: battle map front end

8. **Battle map menu 0x80B8** (0x49BD50): heading 0x3FD, Resume 0x8123 (focused), bottom button Sound Control 0x3ED. This is easy to add to `FrontEnd` now, behind `--open 0x80B8`.
9. **WMouseDrag rubber band** (0x49C080..0x49C720): the 4 px threshold, corner sprites GType 6 frames 0-3, edge lines from 23 px in two colours, the 640 clamp. Used for battle-map unit selection.
10. **Construction placement drags** (0x4A03A0 / 0x4A0630 / 0x4A13B0): a 16-px snapped footprint from the unit table, a terrain-following outline in valid or invalid colours (0x47B610), smoothed cursor, 0x80A4 payload {type, x, y, x2, y2, 0xDEADDEAD}, 0x8137 to chain, the shipyard flag.
11. **Selection hotspots** (0x496D60..0x497770): brackets (GType 5), health bar (GType 7, frame = round(frames × health)), cursors 1/5/0xC, Ctrl force-fire (cmd 0x13 at a ground point), Shift add-select, click-on-enemy attack (cmd 0x10), mouse mode `dword_4EA43C`.

### P3: battle simulation (needs WLandscape, the vehicles and the route finder)

12. **WPuppet behaviour** (2.1): the message set; MoveTo with ±dist/16 jitter per member; Approach; Stop; stances (2 and 3 auto-engage); a rescan every unit-table +0x92 ticks with a rand%30 stagger; auto-targeting within sight × "Audible radius multiplier" that prefers command buildings; fog reveal every "Fog update freq" (15) ticks; timed states 0x80F5-0x80F9 ((l+2)×150, reveal 240); Destroy notifications 0x807B.
13. **Research damage multipliers** in TakeDamage (0x4958B0), keyed to research 0x256, 0x257, 0x259, 0x25C, 0x25D, 0x266, 0x27B, 0x27C and 0x27F. Needs the weapon type ranges named.
14. **Bridges** (2.3): segment build (length/64 − 3), mesh stitching, deck height for units with end ramps, route-finder nodes 48 px past each end.
15. **Fences and walls** (2.4): post-to-post segments of type 0x17B, damage-scaled segments, route blocking.
16. **Submersibles and tunnellers** (2.5): a 64-tick dive to −192, invulnerable except to weapons 0x2CE-0x2D0, hidden unless revealed by 0x80F9 and research (`byte_589134`), the name label and the GType 0x359 sprite.
17. **Battle A* cost** (2.6): Bresenham terrain cost (0x480680), the 8-direction turn penalty tables 0x4D0AF8 and 0x4D09F8, the region shortcut, the goal radius.
18. **Renderers**: WOFLib (piece hierarchy, painter's sort, sun and point-light face shading with "WOF Brightness" 2.5); RTypeISO lighting ("ISOLight"); WLandscapeView row-interleaved terrain and object drawing; RTypeRect; the atlas and quad sprites. The engine's `core/Models` parses WOF but has no renderer.

### Notes for the docs

- ENGINE-MAP.md labels 0x495620-0x496E60 "Puppets". More precisely, it is WPuppet (the unit base) plus WPuppetHotSpot. 0x49BE70 is the war-map sector-map sprite, not RTypeArrow: the "RTypeArrow::~RTypeArrow" string is only used there as an end-of-table address. RTypeArrow proper is 0x49CAC0-0x49D254.
- Add to `ida-notes-thor/ida-map.txt`: Ctrl (force fire) at 0x497199, 0x4976B9 and 0x4977AD; Shift (add to selection) at 0x49773B.
