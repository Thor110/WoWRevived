# War map

What's worked out about the war map (WWarMapWorld, constructor 0x417CD0), and what the engine does with it in
`src/warmap`. Anything marked *provisional* is a stand-in until the game's own code for it is traced.

## Starting it

- **New campaign:** 0x8A (after the intro) starts from `Human.000` or `Martian.000` in the game folder, the
  new-campaign saves (0x4043D0 builds `%s.%03d` with slot 0). The name typed in Edit Campaign replaces theirs.
- **Resume Campaign:** 0x80B3 + slot starts from `SaveGame\Human.00n` or `Martian.00n`.
- WWarMapWorld fades every sound out over 30 ticks (0x464AB0) and loads palette group 0x2ED (hw).

## Pictures and colours

| GType | File | Palette slot | What it is |
|---|---|---|---|
| 0x7D | `HWM.SPR` / `MWM.SPR` | 6 | The map, 1498 x 2544 |
| 0x7E | `HWMHI.SPR` / `MWMHI.SPR` | 6 | The sector map, the same size (below) |
| 0x7F | `HMINI.SPR` / `mmini.spr` | 6 | The mini-map, 100 x 170 (a fifteenth of the map) |
| 0x82 | `HMINIHI.SPR` / `mminihi.spr` | 6 | The mini-map's highlight |
| 0x83 | `HSECTOR.SPR` / `MSECTOR.SPR` | 5 | 31 frames, one per sector, to light a sector on the mini-map |
| 0x76-0x78 | `HU-CNT24/32/48.SPR` | 5 | The round unit and building counters, 150 frames |
| 0x84 | `CHI.SPR` | 6 | Another numbered map: the sectors drawn a little smaller, and the sea areas numbered from 192. Used while the sector objects are set up, to find each sector's counter cells |
| 0x80 | `hmframe.SPR` 122 x 290 / `mmframe.SPR` 121 x 130 | 5 | The mini-map panel |
| 0x33, 0x35, 0x34 | `unit-but`, `man-but`, `res-but` / `munits`, `mcog`, `mfact` | 5 | Its Unit, Construction and Resource view buttons, 4 frames |
| 0x6E | `hcntfnt.fnt` | 9 | The counter font |
| 0x62-0x67 | `HUD-UL/UM/UR/BL/BM/BR.spr` | 5 | Frame corners and edges; where the war map uses them isn't traced yet |

**Palette group hw** (0x2ED, 0x40BCF2) loads `{S}W{S}W` into slot 6 (the map's 89-level table), `{S}W{S}I` into
slot 5 and `{S}WGI` into slot 9, with {S} the side letter (`HWHW`, `HWHI`, `HWGI` or `MWMW`, `MWMI`, `MWGI`).
The menus drawn over the war map use these slots too.

**The sector map.** Its pixels are numbers, not colours: 1-30 the sector the pixel is in, 32-117 a border line
(border = value - 31: 1-63 lie between two sectors, 65-86 are coastlines), 0 the sea. The highlight object
(0x49BE70) gives each value a shade level, 11 to start with, and the map is drawn with each pixel's colour taken
from the level of the sector map value under it. Changing one entry relights a whole sector or border.

**The 89 levels** of the map's table are tint ramps either side of the normal 11, then solid colours at 59-66. The
colours are each side's own: in the Human table 63 is dark red and 64 green, in the Martian table the other way
round, so each side sees its own frontier in its own colour.

**Borders** are coloured by 0x41D9B0 from the two sectors either side (a coastline has the sea, no one, on its
other side), through 0x49C050:

| Border | Level |
|---|---|
| On the selected sector | 66 |
| On the sector under the mouse (the other sector the war map tracks) | 61 |
| The side being played against no one | 64 |
| The other side against no one | 63 |
| The two sides against each other | 62 |
| The same owner either side, or no one either side | 65 |

The sectors themselves stay at 11, except the side being played's, which take a level from a table the view mode
fills (0x4192F0, flt_4E98A0): in Unit View from the strength of the units in each sector (dword_4F7654), in the
other views from its resources (0x424000, from the sector table at 0x4EAE20). Both need data that isn't read yet,
so the engine leaves them at 11. Who holds a sector is in the sector table at 0x4EAE1C (0xC0 bytes a
sector, read by 0x43CA30): 0 Human, 1 Martian, 2 no one. The save has it straight after each SECT's six lists, before
the sector's damage map (the sector object's writer, 0x43C5E0). Human.000 starts with sector 3 Martian and 18 sectors
Human, some with nothing in them.

## Sectors

Sector n's name is TEXT.ojd string 0x002 + n (0x002 is "The Sea", 0x003 "North Western Highlands"... 0x020
"London"). `src/warmap/WarMapData.h` has the two tables, read from the exe's data section:

- **0x4BAE78**, 32 bytes a sector: its bounds on the map (left, top, right, bottom; 0x418980 centres the view on
  them), then two points that belong to the next sector. 0x437380 reads a sector's route waypoint from 0x4BAE68 +
  sector x 32, the previous entry's first point, so sector n's waypoint is entry n - 1's (all 30 fall inside their
  own sectors; London's is 1111, 2034). The second point isn't worked out yet.
- **0x4BCEB0**, the sector links: a pointer per sector to a list of 36 byte links, read by the route arrows
  (0x4244E0): the neighbour, two points where a route over land crosses the border (leaving, arriving), and two
  for a route by sea; zeros where there's no such way. There are 170; the land links are exactly the neighbours
  of the border table, and some sectors also link by sea to ones they don't border (sector 1 to 6 and 8).
- **0x4D0C00**, 8 border numbers a sector. 0x49BE70 turns it into border -> (sector, sector); two sectors are
  neighbours when they share a land border.

## The save's war map data

- **WMAP** (writer 0x41AD80): the view's top-left corner at 640 x 480 (the centre less 320, 240: 0x418980), the
  sector, then 1. Human.000 starts at 803, 1805 in London (sector 30), Martian.000 at 159, 298 in sector 3.
- **CALN** (0x41F760): the game time in hours, a float. **DtTm** (0x41B930): hours elapsed, minute, hour, day and
  month from 0, year.
- **SECT** x 30 (0x4465B0): six lists each, HUNI / AUNI (units), HCON / ACON (buildings), HRES / ARES (resources).
  The H lists belong to the side being played: in Martian.000 the Martians are in HUNI. Each object is a u32
  unit type (buildings have two floats after it), `SELO` with an id and its position on the war map, `WMOB` and
  its sector, then its battle map units. Only the first object of a kind carries a position; the ones after it are
  at 0, 0 and show as the number on its counter. Resources are laid out like buildings and have no positions.
- A unit type's counter frame is OBJ.ojd record type 23, after record 15 picks the unit type (0x437D62,
  0x437EF4).

The whole save is read and written exactly by `core/SaveFile` (the layout is in SaveFile.h): the war map's own
parts field by field (WMAP, CALN, DtTm, MPAD, the routes, the pods, each sector's lists and owner) and the rest as it
was (the research, each sector's economy and damage map, each object's battle map units). Objects don't say how long
they are: an object starts 4 bytes (a unit) or 12 (a building or resource) before a `SELO` whose `WMOB` follows 16
bytes on, and each list's count must match. Human.000 and Martian.000 read and write back byte for byte
(`wowdump --resave`).

After the WMAP values come the calendar, Human then Martian research, the message list (MPAD: each line's text, life,
state and the game hour it came), the route list (each route: origin, destination, two flags, 5, the path's length,
its progress, the path's sectors, a byte, then its units as a WMOB list: units on their way aren't in any sector), the
pods' hours and count (world +0x48 / +0x4C), then sector 0 (owner and DMGL only, no SECT, no lists) and sectors 1-30.

## Saving

The campaign lives in its slot (world +0x128): a new one is the side's start file (Human.000 / Martian.000) written
to `SaveGame\Human.00N` under the new name (0x404660), and the war map plays from that file. Leaving it saves it there
(0x80B4 → 0x4042D0): Return to Main Menu (0x90) and Save & Exit (0x80B6 → 0x70), and in the game also coming back from
a battle (0x6E). Loading a slot (0x80B3) writes "Game Loaded" (0x465) to the message list.

The engine writes the save it read with the war map's state put back: the view's top-left, the selected sector and
the view mode; the hours and date; the message list; the pods; each sector's owner; and every object in the list of
the sector it's in now. An object keeps the position it was saved with unless its counter has moved (then the first
object of the counter takes the counter's place and the rest 0, 0). Units on their way are written as a route each;
loaded again, they're sent on from where they set out. A unit the engine adds (a pod's) is saved as a copy of a saved
unit of the same type, with its battle map units, under new ids: the lowest free ones of the 32768, as the game hands
them out. A save that can't be read exactly is still played, but not saved.

## Counters

Every object has a counter of its own (GType 0x77; WWarMapObject 0x42B2F0), except objects of class 7 (each type's
class is AI.ojd's, 0x441580 -> 0x518B14: gun emplacements, heat ray turrets, wire fences, mines), which share one
counter a type and sector (0x4282A0 / 0x428270) with a count in the counter font (GType 0x6E, right-aligned;
0x42B790, updated by 0x8145): "%d" of them finished, or "%d/%d" finished / all while some aren't, always shown
for such a counter, its bottom right on the counter's. Each view mode shows one kind: Unit View the units,
Construction View the buildings, Resource View the resources.

A counter's draw (0x42B850): depth 4, or 7 for the picked one (drawn over the rest), the pick ring at depth + 2,
the count at + 1. The player's unfinished constructions are drawn at 50% (the sprite's +0x14 bit 0); the other
side's counters are too while the intel on them is old (+0x3C under 168, not in yet). Still to come with the
systems they show: the power bar, the production blink, the idle icon (GType 0x76 frame 0x94) and the repair
icon (frame 0x95, while one of the object's units is being repaired, 0x4299C0).

Where they go is the counter placement's ("clunting", 0x420160, world +0x74), not the positions objects are saved
with:

- **Grids** (0x428110): each sector has a grid of `[Tweak] "WM clunting res"` (36) pixel cells over its bounds
  (0x4BAE78), the left and top rounded down and the right and bottom up to whole cells; a cell is usable when
  CHI.SPR (GType 0x84) has the sector's number at its corner. 1 is usable, 0x81 taken.
- **The search** (0x4283F0): modes 0 and 1 go row by row; the war map uses mode 3 in the Unit and Construction
  views and mode 2 in the Resource view, both round a spiral from the middle cell (w / 2, h / 2): the middle,
  then rings 1-15 (0x4282C0), each clockwise from the cell above the middle; 961 steps in room for 1024. The
  search keeps its place in the spiral: it moves on past every step off the grid or not free and stays at the
  one it takes, so a cell given back behind it isn't used again until the grid is cleared. Mode 2 tests the
  step's row offset against the grid's height rather than the row, so the rows above the middle are never used
  for resources (and rows below the grid would be read past its end; the engine counts them as not free).
- **Placing** (0x4205F0): the next free cell. When there's none, the sector's grid is made again with cells
  `"WM clunting adj"` (4) pixels smaller, every counter already placed in the sector takes a new cell and slides
  to it at 5 pixels a tick (0x4286C0; each tick 0x4209E0 -> 0x428730, done under 0.0005 left), and then the new
  one is placed (in the game's 0, 0 if there's still no room; the next one shrinks the grid again).
- **A new view** (0x419810): every grid cleared (0x4204A0; a shrunk grid keeps its size), then each sector's
  counters of the view placed, the player's first (0x41C2E0 -> 0x43CB20).
- **A sector laid out again** (0x43CB60, when units arrive): its grid cleared and back to the full size
  (0x4203C0), its counters of the view taken out and placed again, the player's first.
- **Setting off**: a travelling unit gives its cell back (0x4208B0) and is placed by its route (0x420540) instead.

## The mini-map panel

"WarMap miniMap view" (0x422C70) is the mini-map (0x7F) with its highlight (0x82) and the sector frames (0x83). It
sits in the bottom right corner; WoWRevived's screenshot puts it 115 in from the right and 175 up from the bottom.
"WarMap miniMap frame" (0x423420) is the panel: frame 0x80, and three view buttons (0x4333B0, command 0x811F, ids
0x94 / 0x93 / 0x92) at (6, 0x29), (0x2A, 0x29), (0x4F, 0x29) in Human games and (0xA, 0x24), (0x2D, 0x24),
(0x51, 0x24) in Martian ones, labelled 0x47B "Unit View", 0x47C "Construction view", 0x47E "Resource view".
0x418C14 puts the frame above the mini-map: 0x50 + 0x20 higher and 9 to the left in Human games (the mini-map then
shows through the frame's window), 0x44 + 0x28 and 10 in Martian ones (the mini-map hangs below it). A button
sets the view mode (0x419810: 1 units, 2 constructions, 3 resources, the world's +0x70) and stays pressed
(message 0x8034). The frame also holds the Research button (0x45BD70, GType 0x341 rese-but.spr, control 0x7D) at
(0x48, 5) in Human games and (0x3A, 0xC) in Martian ones; its tooltip is 0x47D "View Research" with the current
research. Tab hides the frame and the mini-map (0x41A450). Which of the buttons' 4 frames is which isn't confirmed; the engine uses 2 for the pressed one
and 1 under the mouse.

## The calendar

"Calendar View" (0x426C10, drawn by 0x4271E0, its frames worked out by 0x427500) sits in the top right corner, 4
pixels in (0x4BA05C): GType 0x93 (HU_CALEN.SPR 121 x 89, MA_CALEN.SPR 123 x 83) with its parts drawn on top.

| Part | Human | Martian |
|---|---|---|
| 0x96 | HU_TIME (36 x 36, 144 frames): 96 over 12 hours (8 an hour), then 48 blurred ones shown at speeds 8-10, at 0x52, 4 | ma_world, the globe (24 frames: the hour of the day) at 0xB, 8 |
| 0x95 | hu_month (60 frames, 5 a month) at 0x15, 0xC | |
| 0x94 | hu_am_pm (10 frames) at 0x5A, 0x2B | |
| 0x98, 0x99 | the day and year: six digits (5 frames each) at x 7, 14, 48, 55, 62, 69 (0x4BA688), y 0xC; the first is hu_num03 (0-3) | the days gone by: six digits (ma_spin) at x 32, 41, 50, 59, 74, 83 (0x4BA670), y 0x35: the game time over 24000, 2400, 240, 24, 2.4 and 0.24 hours (0x4BA910), so days to two decimal places ("2341.90" in WoWRevived's screenshot) |
| 0x9B | hu_pend, the pendulum (41 frames: 0-29 while the clock is stopped, 30-40 while it runs, drawn at 50% then; a frame an update) at 0x54, 0x41 | ma_goo (100 frames, 4 an update while the clock runs, 1 while stopped) at 0x31, 7 |
| 0x9A | hu_twirl, just left of the calendar at y 6 | |
| 0x97 | hu_speed, the speed slider (120 x 58) at 0, 0x1F | ma_speed (65 x 26) at 0x30, 0x16 |

The speed slider (0x431E30, a "Scrollbar" of 11 positions, 0-10) has 22 frames, the 11 positions then the same lit.
Moving it sends the calendar 0x114/0x115 with the position, which becomes the speed (0x427932 -> 0x41F640), unless
0x41AEA0 finds a sector that says no (0x43D010 for each of the 30; not traced), when it's 0. Where along the slider
a click lands is the Scrollbar's (0x431EF0): (max + 1) x (x - left margin) / (length - both margins), the margins 18
and 31 on the Human slider, 8 and 11 on the Martian one. The digits 0-9 set the speed too ('0' is 10).

A click on the rest of the calendar ("Advance time", 0x481) moves the game on an hour at once (0x4278A0: the
calendar's +0x30, then its tick, which takes an hour off +0x30 instead of the speed's step).

The wheels (0x427500 sets where each is heading, 0x427710 turns them, both every update) don't jump: each digit goes
on by a step for the speed (0x4BA6A0 Martian, 0x4BA7A8 Human, six to a speed; frames mod 50) and stops on its
target once it would pass it; the Human first digit, am/pm (0x4BA8B0), month (1) and clock face (0x4BA8E0: 2, 3,
3, 5, 6, 9, 13, 17, 21, 25, 35) go a frame at a time, up to their step, until they reach it. While the calendar is
paused (its +0x38, during dialogs) it flashes, two updates on and two off.

The clock's sound (0x41F860, on every change of speed): a Human clock ticks (0x1A2, looping) only while stopped and
fades over 15 updates once it runs; a Martian one has a sound for each speed, 0x1A1 + speed, faded in and out over 5.

## Moving units

In Unit View a click on one of your counters picks it (GType 0x54, cselecth.spr, drawn over it) and a click on a
sector sends it there.

- **Routes** (WWarMapRoute 0x4240D0) move on every game hour: the calendar sends 0x802D with the hours gone by and
  the route's handler (0x424F00) moves its entries on (0x425080). A route keeps its sectors, the start first
  (+0x4C, +0x48), its length (+0x68: waypoint to waypoint, 0x437380, whole pixels each) and an entry for each group
  of units (RouteEntry 0x42AFE0): its speed (unit +0x54 x 0.05), the way still to go (+0x30, the length at
  first), the hours still to go (+0x34, length / speed), what's left of the way to the next waypoint (+0x38, of
  +0x3C) and the path index it last reached (+0x24); and its place in a formation `[Tweak] "Route clunting width"`
  (4) wide and `"Route clunting spacing"` (4) apart (0x42AF20 / 0x42AF60): the n-th at ((n % w) - w / 2,
  (n / w) - w / 2) x spacing. Each hour (0x425080):
  - the first puts the units at the curve's start, plus their places (0x4406A0)
  - unless it's an attack or interrupted already, every sector from the slowest entry's on is checked: one with a
    battle (+0x124), or held by the other side, other than the start, interrupts the route (+0x60) with event 11,
    TravelInterrupted (0x41EF80: "Travel interrupted" 0x419, sound 0x147, and the event box if the option's on)
  - each entry goes (way still to go / hours still to go) x hours; on getting to a waypoint it arrives there if
    it's the last, or the route was interrupted (0xAD to the group with the sector; +0x58 the one before);
    otherwise the next waypoint's distance is added
  - on the map it's that far along the arrow's curve: point `segment x 32 + int(f)` and the fraction between it
    and the next, f = (1 - left / length) x 32; the smallest of these over the entries trims the arrow (+0x5C: its
    body is drawn only past it)
  - once every entry has arrived, a route that isn't an attack raises event 3, ArrivedInSector, for its goal, and
    goes from the route manager (0x41FBD0), which sends the world 0x802C when none are left (not traced).
  Attacks on the same sector arrive together: when a route is added the route manager (0x41FCF0) gives every
  attack heading for a sector the longest hours still to go of the routes heading there (0x4255F0), label too.
  The engine moves a counter as a route of one entry; where it stands on the curve is its centre (the counter's
  corner 16 up and left), *provisional*.
- **Arrows**: RArrow (0x4244E0) makes a curve through the route's points: for the start the point where a route
  over land leaves it for the next sector (its link, 0x4BCEB0), then each sector's waypoint (0x437380); a naval
  route (+0x50: its first unit's +0x52 is 5 or 6), or a point that's 0, 0, takes the sector's sea waypoint
  (0x4373A0) instead. 2^5 (+0x44 = 5) steps a stretch (0x424A40), and an RTypeArrow (0x49CAC0, drawn by 0x49CDB0) along it. Its style
  is OBJ.ojd record 14 (`wowdump --arrows`; a table of 24 bytes at 0x59EDB8): 0 / 1 Human, 2 / 3 Martian (one
  more when the route's +0x5E is clear), each a GType, four numbers and a float:

  | Style | GType | Body half width (tail, head end) | Head half width, length | Float |
  |---|---|---|---|---|
  | 0 | 0x19 (h/marrow2.raw) | 20, 12 | 32, 50 | 0 |
  | 1 | 0x18 (h/marrow1.raw) | 14, 6 | 18, 28 | 0 |
  | 2 | 0x19 | 8, 16 | 26, 64 | -0.4 |
  | 3 | 0x18 | 4, 12 | 20, 56 | -0.4 |

  The textures are raw 8-bit images (graphics class 6) 256 x 64: the body strip in the first 64 columns (across
  its width), the head's triangle in the next 128 (apex at 128, 0). The body is textured quads (0x410090) out to
  a third of a stretch from the end, its half width going from the first number to the second along the way
  (offsets at right angles from 0x43FE90); the head is textured triangles (0x410190) from there. A text object (0x49D260, font 0x71) at the end shows the
  arrival of an attack, set when the route starts (0x424160) and when its hours are set again (0x4255F0), by
  0x41B9A0: for the Humans the hour, "03pm" (hour mod 12, am/pm from 0x4B9FC0), then " 4 Sep" unless it's within
  24 hours on the same day, then the year if it's another year's; for the Martians the days since the start at the
  arrival with hundredths, "12.50". The engine draws the curve, body, head and label as the game does.
- **Speed**: each unit type's war map speed is the word at +0x54 in the unit table (0x51A0F0, 0xB0 bytes a type),
  set by OBJ.ojd record 37 (three words: +0x52, +0x54, +0x56) and scaled by record 38 (two floats, multiplying +0x54
  and +0x56); record 15 starts a type. A new route's arrival time (0x424160) is its length in map pixels (+0x68) over
  the slowest unit's speed x 0.05, in game hours, kept at +0x6C and shown as a date (0x41B9A0). A counter is one unit
  type, so it goes speed x 0.05 map pixels a game hour: 100 is 5. Reloading a route (0x4257C0) works it out again
  with x 0.01 between its first two points (not yet understood). `[Tweak] "Clunting Speed"` (0x428B70, default 7,
  the same as the fastest Human unit's 140 x 0.05) is only used for a counter with no speed. `wowdump --units`
  prints the table.
- **The way**: the game's route finder, an A* (FindRoute 0x44EFA0, set up by 0x44EE70 from 0x424160, at most
  1000 steps) over 31 nodes whose neighbour lists are at 0x4BD260. 0x435710 builds a 32 x 32 table of link kinds at
  0x4EAA18 from a switch on to x 100 + from (bit 1 widens to 7): 0xF over land, 0x10 by sea, 0x1F both, 0x18 by sea
  with an unknown 8. The table and the neighbour lists agree exactly (79 links, all both ways); the engine's copy is
  `warmap::RouteLinks`, and the neighbour lists `warmap::Neighbours`. Each step's cost (0x42ABD0) is 0 from a
  sector to itself, else the straight line between the two waypoints (0x437380), in whole pixels. The A* itself
  (0x44EE70 / 0x44EFA0 / 0x44F120, the engine's `warmap::AStar`, which the sector paths use too) has no separate
  estimate: a node's h is the cost function asked for (node, goal), and the start's for (goal, start), so it's -1
  unless they're neighbours. The open list is in f order, a new node going before the first with an f no lower
  than its own; a node reached again more cheaply takes the new parent and g but isn't moved in the list, and a
  closed one isn't opened again; neighbours are tried in the table's order; it gives up when the neighbours tried
  less one a node reach 1000. On the map it comes to the shortest way every time (checked over every pair of
  sectors with every link open). A route stands when it reaches the goal with more than the start in it
  (0x424160). A step is refused, with a code left at 0x4EA198 (by whichever call refused last, estimates
  included) that nothing reads:
  - 0: a unit in the route can't move (unit +0x54's +0x128; not done)
  - 1 / 2: the link isn't one of the unit's: the unit type's +0x4E (OBJ.ojd record 36) is 0-2 for land units
    (link bits 1, 2 or 4), 3 for boats (0x10), 4 for fliers (any)
  - 3: neither end has its owner's command building, finished (0x43C990: a Human sector needs a Command Post, type
    58, or in London (30) type 278 (the code adds 0x116 and the low byte 0x24 of -1, which wraps to 0x3A); a Martian one a Communications Centre, 120, or in sector 3 the
    landing site, 830. Finished is the construction's +0x7C at least its +0x78 (0x42AA70); the engine compares
    the save's two floats, *provisional* as to which is which)
  - 4 / 5: a sector passed through (not the goal) has no command building, or isn't the player's
  - 6: the start has no command building and the step goes into a sector that isn't the player's
  - 7 / 8: the goal would hold more than `[Tweak] "Max units in sector"` (default 15) units, or `"Max boats in
    sector"` (5) boats: the player's there already (0x43D090 / 0x43D0F0, boats being class 3), every route's
    heading there (0x420060; a route counts as boats when its first unit's +0x52 is 5 or 6) and these
  The Human campaign starts with command posts in 15 sectors, so most moves through the player's own sectors
  are allowed. The text for
  a refusal is the engine's own. Boats cross at a link's sea points, fliers at its land points if it has them.
- Arriving in a sector the other side holds gives "Battle in" and the sector (a battle, which needs the battle
  map) and the counter stays where it came from; arriving in a sector no one holds takes it (*provisional*).
- **Saved routes** (0x425A60): origin, destination, +0x5F, +0x5E (attack), +0x44 (5), the path's count, +0x34, the
  path (the start first), the u8 +0x50 (naval) and the units.

## Martian pods

Every whole game hour the world (0x802D, 0x419E9D) moves the routes, the players and both sides' research on, then
the pods (0x41A330): while fewer than 8 have come, every `[Tweak] "Pod Interval (hours)"` (24) one lands in sector
2, 4, 6, 7, 8, 9, 10 or 11 (`byte_4B9DE8[rand() & 7]`) with one of four loads (`off_4B9E28[rand() % 4]`: unit 62,
ScoutMachineMk1, alone or with 89, HandlingMachineMk1, or 91, DiggingMechanismMk1). In a Human sector the sector is
marked (+0x64) for the battle check; otherwise it's event 12, PodLanded (0x41F010): in a Martian sector sound
0x1B2 and "Pod landed: <sector>"; in one no one holds, a battle there (+0x124), sound 0x1B3 and "Pod landed in our
territory" (0x420, Human) or "Pod landed in enemy sector" (0x41F, Martian), with command 0x8072 to go to it. The
war map seeds rand() with 1 when it's built (0x417DA3), so the first pod of a new Human campaign lands in sector
4, Strathmore.

A sector with a battle pulses (0x41DE70): the view's frame count 0-22 as 0-11-0 on level 36 for ours, 47 for the
rest.

## Sector shading

0x4192F0 fills five tables of a value for each sector (at 0x4E98A0, each `{ float v[31]; min; max; lo; hi }`) and
the view (0x41D9B0) turns the one for its mode into levels of the map's shade table:

| View | Table | Value | Levels |
|---|---|---|---|
| Unit | 0 | the objects put in each sector of ours (`dword_4F7650[s]`, one more each time the sector's add method 0x43C790 runs); 0 elsewhere | 17-35 |
| Construction | 1 | the strain on the sector's economy, 1 - 1 / (r0 r1 r2) from the ratios of the side's three kinds (see "The economy"; top fixed at 1.0) | 67-77 |
| Resource | 2-4 | for the side's resource kind 0-2 (Human 0-2, Martian 3-5), what one more facility would add (0x424000): output(n + 1) - output(n), with output(n) = n r B / (n + r - 1), B the sector's deposit (OBJ.ojd record 8, the sector table's +0x04) and n its finished facilities of the kind (types 47-49, 108-110); B when there are none (top fixed at 80) | 78-88 |

A sector of ours gets `lo + (v - min) / (max - min) x (hi - lo)` (truncated; a range of 0 counts as 1); in the
Resource view so does every sector. Otherwise a sector no one holds is at 11, and the other side's is 11 too,
except in the Unit view, where it's 0.

## The economy

Each sector's record (0x4EAE18 + s x 0xC0) keeps six figures for each of the six resource kinds (0-2 Human: iron,
coal, oil; 3-5 Martian: human blood, copper, heavy elements; names 0x214-0x219): +0x08 the deposit (OBJ.ojd
record 8), +0x20 its finished facilities, +0x38 demand, +0x50 spread demand, +0x68 supply, +0x80 spread supply and
+0x98 the ratio. 0x41A760 works them out in five phases, one an update (the tick count % 5, from 0x41AB00), all
five when the war map starts (0x418BAF):

| Phase | What |
|---|---|
| 0 | **Supply**: in a sector someone holds, its owner's constructions (0x446370(3, ...)) are told 0x803C and each kind's output is added up (0x421D00). A finished facility of the kind (types 47-49, 108-110) makes `int(hp / maxHP) x r B / (n + r - 1)` (0x423B00), r the `[Tweak] "Resource Drop Off Rate"` (3), B the deposit and n the sector's finished facilities of the kind; nothing while the sector has no command building. A sector no one holds supplies nothing. |
| 1 | **Spread supply**: the sector's own, plus every other sector's over `count x VX + 1`, count being the sectors (start included) on the sector's cached path to it, plus the `"Resource Buffer"` (30). Sectors with no path give nothing. |
| 2 | **Demand**: zeroed, then each sector's four object lists are told 0x8143 (0x429640): a unit adds its upkeep (unit records 45 / 46, three floats for its side's kinds) for each man it carries (its battle map list's count), halved while it stays put, plus its work (records 47 / 48) while it repairs or builds (not in yet). A travelling unit's goes to the sector it's in now (0x424E90). |
| 3 | **Spread demand**: as phase 1 with VY and no buffer. |
| 4 | **Ratio**: `max(1, spread demand / spread supply)` (worked as `(D - S) / S + 1`); then table 1 for the Construction view, `1 - 1 / (r0 r1 r2)` over the side's three kinds, its least and most kept (0x4E99A8 / 0x4E99AC). The cheat byte 0x58913C makes it 0. |

VX and VY are `[Tweak] "Distance Crippler VX"` and `"VY"` (0.5 each; 0x43C0A0-0x43C125).

The cached paths: each sector object keeps one to every other sector (+0xA0, `GetPath(n)` 0x43C090), rebuilt when
it's told 0xB7 (0x43AFEF, after a construction is finished or a sector changes hands; the engine rebuilds them
when an owner or a command building changes). Each is a `WSectorPath(side, from, to)` (0x4A39E0) for the side
holding the start: the same A* over land links only, where a step into a sector held by
neither that side nor no one is impassable, so is one where neither end has its command building, or into one
without it that isn't the goal; otherwise it costs the whole pixels between the waypoints. A path of only its
start (or none) has a count of 0.

The save keeps each sector's figures at the end of its part (0x43C5E0): for each kind its spread supply, spread
demand, ratio, demand and a 0. The start saves have 30, 0, 1, 0, 0 everywhere.

**Resource warnings** (the hourly step, 0x419EFB): for each of the side's three kinds the least spread supply over
spread demand among its sectors (from 1). Under 0.3 a counter (0x4E9BA0) goes up one a call and past 336 raises
event 14, ResourceAlert, clearing both counters; otherwise, under 0.8, another (0x4E9BB0) goes up and past 504
raises event 13, ResourceWarning. They count calls, not hours.

## Messages and events

The war map's events (a table at 0x4BA0E8 of handler, "is it critical" test and name, 12 bytes each, raised by
0x41E5B0): 0 BattleInSector, 1 ConstructionCompleted, 2 UnitCompleted, 3 ArrivedInSector, 4 ResearchCompleted,
5 ResearchAvailable, 6 ResearchAlert, ConstructorOnline, FacilityOnline, ConstructorIdle, FacilityIdle,
TravelInterrupted, PodLanded, ResourceWarning, ResourceAlert, CommandCentreBuilt, EnteringNeutralSector,
UndefendedSectorLost and End Game. A critical event (the Critical Event Options) also opens a dialog (0x41E4E0;
one at a time, the rest queued; not done); every event writes a line to the message list.

The message list (PopupMessageController 0x420A30, at 0x28, 4) keeps at most 10 lines (0x4BA4FC), each a
"Message Item" (0x428810, font 0x72) shown for 5 x 30 updates (0x4BA4F8), drawn top down 16 pixels apart
(0x4BA500). Lines that have gone are kept (they count towards the 10). When a line runs out (0x420CD0) the list
scrolls up a pixel an update; once it has gone up 16 the first run-out line is gone and the list drops back, and it
goes on scrolling while more have run out. M (world 0x6C, a toggle on +0x6C) opens the history with sound 0x193:
every line with less than 5 seconds left gets 5 more (0x421150), so the last 10 show; closing it (0x6D, sound
0x196) takes the 5 seconds off them all. Command 0x80CC gives the last 2 (0x4BA504) another 5 seconds.

So far the engine raises:

| Event | Line | Sound |
|---|---|---|
| ArrivedInSector (0x41EAE0) | "Vehicle/s arrived in" (0x211) and the sector | 0x120 |
| BattleInSector (0x41E850) | "Battle in" (0x210) and the sector | 0x139 attacking (0x13A for sector 3 or London), 0x138 defending |
| ResourceWarning (0x41F280) | "Resource Warning: " (0x41B) and the resource (0x214 + kind) | 0x124, 0x122, 0x123 (Human), 0x125-0x127 (Martian) |
| ResourceAlert (0x41F340) | the same | 0x12A, 0x128, 0x129 (Human), 0x12B-0x12D (Martian) |

The sector under the mouse, the panel button's name and a refused route's reason show in a strip at the bottom
left (*provisional*: the engine's own).

## The clock

The calendar object (0x41F550) adds `speed / 30` hours to the game time every update (0x41F640, 0x41F6C0; 30 is "Game
Frequency"), with the speeds at 0x4BA470 the floats 0.0-10.0 (the slider's position). Each whole hour moves the date on (0x41B770),
with February's leap years. The engine starts at speed 1, one game hour a second; the calendar's slider or the
digits change it.

## Music

The war map stops the CD when it starts and takes its first music step 5 seconds later (0x4181BF, command 0x813C).
A step (0x41B070) fades out the last ambient sound over 60 updates; then, with CD music on ([Sound\Volume] "CD"),
if the disc is still playing (the CD player's music) it stops it and looks again in 10 seconds, otherwise it plays
the next of tracks 2 and 3 in turn (from 2) and comes back when it ends (or in 10 seconds if the track is 10 seconds
or shorter). Without CD music it plays ambient sound 0xFD + rand() % 3, faded in over 60, every 10 seconds.

## The war map menu

Escape opens 0x80B5 ("Warmap menu", 0x49B980), drawn over the map: heading 0x3FC "Campaign Options", 0x8123
"Resume Campaign (Escape)" (focused), 0x90 "Return to Main Menu", 0x8117 critical event options, 0x3ED Sound
Control, and the bottom button 0x80B6 "Save & Exit Game". The battle map has its own (0x80B8, 0x49BD50).

## Keys

The view (0x41C5E0, 0x41D640) and the world (0x41A450) take a key press at a time:

| Key | |
|---|---|
| F1 / F2 / F3-F5 | Unit view, Construction view, Resource view with resource 0-2 |
| Space | the next view: Unit, Construction, Resource 0, 1, 2, Unit |
| Tab | hide or show the mini-map and its frame |
| 0-9 | the speed, '0' being 10 |
| Home | centre on the selected sector (its rectangle's middle, 0x41E000) |
| PgUp / PgDn | to the top / bottom of the map |
| arrows | a seventh of the map's width or a sixteenth of its height (Ctrl: a third / an eighth) |
| Esc | the war map menu (0x80B5) |
| R | research (0x7D) |
| M | the message history (0x6C / 0x6D) |
| Backspace | clears the world's time-running flag (+0x64), which tells the opponent; not in yet |

## Scrolling

Besides the keys, the engine scrolls at the screen edges and by clicking or dragging on the mini-map, at
`[Screen] "Scroll Speed"` / 5 pixels a tick (*provisional*: the game's own edge scrolling isn't traced).

## Still to do

The game's scroll rules, the event messages (top left; the engine shows only the latest), the own sectors' shading,
the arrows' exact curve and head, a unit that can't move (refusal 0), construction (so command posts can be built), research, the AI's turns, saving, and the
battle map.
