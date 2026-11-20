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

## Counters

The world's constructor makes 30 sector objects (0x420160 -> 0x428110), each laying a grid of 36 pixel cells
(dword_4EA0D4) over the sector's bounds, a cell counting as the sector's when CHI.SPR has the sector's number at its
corner. Saved positions are cell corners (468 = 13 x 36). Each view mode shows one kind: Unit View the units, 
Construction View the buildings, Resource View the resources; a unit and a building can share a cell.

The engine draws one counter (GType 0x77) per kind of object in a sector's list, at its first object's position,
with the count in the counter font when there's more than one. Kinds without a position (all resources) go in the
sector's first free cell, row by row (*provisional*: the game's own fill order, 0x4282C0, isn't traced, nor where
it puts the count).

## The mini-map panel

"WarMap miniMap view" (0x422C70) is the mini-map (0x7F) with its highlight (0x82) and the sector frames (0x83). It
sits in the bottom right corner; WoWRevived's screenshot puts it 115 in from the right and 175 up from the bottom.
"WarMap miniMap frame" (0x423420) is the panel: frame 0x80, and three view buttons (0x4333B0, command 0x811F, ids
0x94 / 0x93 / 0x92) at (6, 0x29), (0x2A, 0x29), (0x4F, 0x29) in Human games and (0xA, 0x24), (0x2D, 0x24),
(0x51, 0x24) in Martian ones, labelled 0x47B "Unit View", 0x47C "Construction view", 0x47E "Resource view".
0x418C14 puts the frame above the mini-map: 0x50 + 0x20 higher and 9 to the left in Human games (the mini-map then
shows through the frame's window), 0x44 + 0x28 and 10 in Martian ones (the mini-map hangs below it). A button
sets the view mode (0x419810: 1 units, 2 constructions, 3 resources, the world's +0x70) and stays pressed
(message 0x8034). Which of the buttons' 4 frames is which isn't confirmed; the engine uses 2 for the pressed one
and 1 under the mouse.

## The calendar

"Calendar View" (0x426C10, drawn by 0x4271E0, its frames worked out by 0x427500) sits in the top right corner, 4
pixels in (0x4BA05C): GType 0x93 (HU_CALEN.SPR 121 x 89, MA_CALEN.SPR 123 x 83) with its parts drawn on top.

| Part | Human | Martian |
|---|---|---|
| 0x96 | HU_TIME (36 x 36, 144 frames, 12 an hour) at 0x52, 4 | ma_world, the turning globe (24 frames) at 0xB, 8 |
| 0x95 | hu_month (60 frames, 5 a month) at 0x15, 0xC | |
| 0x94 | hu_am_pm (10 frames) at 0x5A, 0x2B | |
| 0x98, 0x99 | the day and year: six digits (5 frames each) at x 7, 14, 48, 55, 62, 69 (0x4BA688), y 0xC; the first is hu_num03 (0-3) | the days gone by: six digits (ma_spin) at x 32, 41, 50, 59, 74, 83 (0x4BA670), y 0x35: the game time over 24000, 2400, 240, 24, 2.4 and 0.24 hours (0x4BA910), so days to two decimal places ("2341.90" in WoWRevived's screenshot) |
| 0x9B | hu_pend, the pendulum (41 frames) at 0x54, 0x41 | ma_goo (100 frames) at 0x31, 7 |
| 0x9A | hu_twirl, just left of the calendar at y 6 | |
| 0x97 | hu_speed, the speed slider (120 x 58) at 0, 0x1F | ma_speed (65 x 26) at 0x30, 0x16 |

The speed slider (0x431E30) has 22 frames, 11 positions then the same lit. Which position gives which game speed isn't
traced; the engine uses position = speed index (0-7), and clicking along it sets the speed (*provisional*).

## Moving units

In Unit View a click on one of your counters picks it (GType 0x54, cselecth.spr, drawn over it) and a click on a
sector sends it there.

- **Routes** (WWarMapRoute 0x4240D0) move on every game hour: the calendar sends 0x802D with the hours gone by and
  the route's handler (0x424F00) moves its entries on (0x425080), point to point. A route goes from a sector's
  waypoint, across the link's two border points, to the next sector's waypoint (0x425080 and the arrows both take
  the points from 0x437380 and the link table).
- **Arrows**: RArrow (0x4244E0) makes a curve through the route's points, 2^5 steps a stretch, which RTypeArrow
  draws. The engine draws a dotted line through the points instead (*provisional*).
- **Speed**: each unit's speed is in the unit data, which isn't read yet; every counter goes at
  `[Tweak] "Clunting Speed"` (0x428B70, default 7) map pixels a game hour (*provisional*).
- **The way**: the shortest over land through the links (*provisional*: the game's FindRoute, 0x44EFA0, isn't
  traced).
- Going into a sector the other side holds stops the counter at the border with a message (a battle, which needs
  the battle map), and arriving in a sector no one holds takes it (*provisional*).

## The clock

The calendar object (0x41F550) adds `speed / 30` hours to the game time every update (0x41F640, 0x41F6C0; 30 is "Game
Frequency"), with the speeds at 0x4BA470: 0, 1, 2, 3, 4, 6, 8, 10. Each whole hour moves the date on (0x41B770),
with February's leap years. The engine starts at speed 1, one game hour a second; the calendar's slider, or `+` and
`-` (*provisional* keys), change it.

## The war map menu

Escape opens 0x80B5 ("Warmap menu", 0x49B980), drawn over the map: heading 0x3FC "Campaign Options", 0x8123
"Resume Campaign (Escape)" (focused), 0x90 "Return to Main Menu", 0x8117 critical event options, 0x3ED Sound
Control, and the bottom button 0x80B6 "Save & Exit Game". The battle map has its own (0x80B8, 0x49BD50).

## Scrolling

The engine scrolls with the arrow keys, at the screen edges, and by clicking or dragging on the mini-map, at
`[Screen] "Scroll Speed"` / 5 pixels a tick (*provisional*: the game's own scroll rules aren't traced).

## Still to do

The game's scroll rules, the event messages (top left; the engine shows only the latest), the own sectors' shading,
the counters' fill order, unit speeds, the route finder and the arrows' drawing, sea routes, construction, research, the AI's turns, saving, and the
battle map.
