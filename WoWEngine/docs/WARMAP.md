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
| 0x62-0x67 | `HUD-UL/UM/UR/BL/BM/BR.spr` | 5 | Frame corners and edges; where the war map uses them isn't traced yet |

**Palette group hw** (0x2ED, 0x40BCF2) loads `{S}W{S}W` into slot 6 (the map's 89-level table), `{S}W{S}I` into
slot 5 and `{S}WGI` into slot 9, with {S} the side letter (`HWHW`, `HWHI`, `HWGI` or `MWMW`, `MWMI`, `MWGI`).
The menus drawn over the war map use these slots too.

**The sector map.** Its pixels are numbers, not colours: 1-30 the sector the pixel is in, 32-117 a border line
(border = value - 31: 1-63 lie between two sectors, 65-86 are coastlines), 0 the sea. The highlight object
(0x49BE70) gives each value a shade level, 11 to start with, and the map is drawn with each pixel's colour taken
from the level of the sector map value under it. Changing one entry relights a whole sector or border.

**The 89 levels** of the map's table are tint ramps either side of the normal 11, then solid colours at 59-66 (red,
bright green, cream, orange, dark red, green, tan, white). WoWRevived's screenshots show borders coloured by who
holds the sectors either side; which levels the game picks for that isn't traced yet. The engine marks the selected
sector's borders white and the hovered sector's cream (*provisional*).

## Sectors

Sector n's name is TEXT.ojd string 0x002 + n (0x002 is "The Sea", 0x003 "North Western Highlands"... 0x020
"London"). `src/warmap/WarMapData.h` has the two tables, read from the exe's data section:

- **0x4BAE78**, 32 bytes a sector: its bounds on the map (left, top, right, bottom; 0x418980 centres the view on
  them) and two more points, not worked out yet.
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
  at 0, 0 and show as the number on its counter. Resources are laid out differently and aren't read yet.
- A unit type's counter frame is OBJ.ojd record type 23, after record 15 picks the unit type (0x437D62,
  0x437EF4). The engine draws counter GType 0x77 at each object's position, moving a counter along when two kinds
  share one (*provisional*: the game's counter layout isn't traced).

## The clock

The calendar (0x41F550) adds `speed / 30` hours to the game time every update (0x41F640, 0x41F6C0; 30 is "Game
Frequency"), with the speeds at 0x4BA470: 0, 1, 2, 3, 4, 6, 8, 10. Each whole hour moves the date on (0x41B770),
with February's leap years. The engine starts at speed 1, one game hour a second; `+` and `-` change it
(*provisional* keys).

## The war map menu

Escape opens 0x80B5 ("Warmap menu", 0x49B980), drawn over the map: heading 0x3FC "Campaign Options", 0x8123
"Resume Campaign (Escape)" (focused), 0x90 "Return to Main Menu", 0x8117 critical event options, 0x3ED Sound
Control, and the bottom button 0x80B6 "Save & Exit Game". The battle map has its own (0x80B8, 0x49BD50).

## Scrolling

The engine scrolls with the arrow keys, at the screen edges, and by clicking or dragging on the mini-map, at
`[Screen] "Scroll Speed"` / 5 pixels a tick (*provisional*: the game's own scroll rules aren't traced).

## Still to do

The game's scroll rules and HUD (the resources readout top right and the control panel above the mini-map in
WoWRevived's screenshots), ownership colours, the counters' own layout and their numbers' font, resources,
selecting and moving units, routes, research, the AI's turns, saving, and the battle map.
