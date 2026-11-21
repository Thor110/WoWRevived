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
other side), each counted as no one's unless its holder has its command building there finished (0x41DC1A-0x41DC47,
0x43C990; until update 103 the engine took the holder alone), through 0x49C050:

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

A counter of the player's has a tool tip (0x807C; the other side's have none), its level going up the longer the
mouse stays:

- **Units** (0x428D70): the type's name (unit table +0x14, OBJ.ojd record 21); "In transit" (0x51D) on its way (a
  unit with a task shows "Currently building:", "Days to completion:" and "Efficiency:", or "Idle" / "Construction
  suspended", 0x48E-0x493, which come with construction); from level 2, if it uses resources, "Resource use (Iron,
  Coal, Oil)" (0x519 and the side's kinds), "Working:" (0x51B) its work figures when it has any, "Idle:" (0x51A) its
  upkeep for all its men halved, and "Moving:" (0x51C) its upkeep for all its men, "%0.1f, %0.1f, %0.1f".
- **Constructions** (0x42A4B0): the name, " X n" for a class 7 counter (0x43A160); unfinished, "\n%2.1f%% complete"
  (0x4B5, +0x7C / +0x78) and no more (nothing for a class 7 one); finished, from level 2, "Resource use (...)" and
  "Working:" when it has work figures.

**Intel and the fog** (WMOB +0x3C, hours, not saved): an object is shown (activated and placed, 0x421630) only if
it's the player's, there's intel on it, or it's a network game. Intel goes down by the hours every hour, to 0
(0x429B80); one that runs out stays until the next layout. It comes only from Infiltrate: on the battle map the
special's 0x80F4 taken by the other side's key building sets the sector's +0x128 to its level (1-3) and
dword_4E9B60 to the firer's side (0x43B920; docs/BATTLEMAP.md; nothing else writes +0x128 but the constructors).
The war map's set-up after the battle (0x418E17) takes the first sector with it set, clears it, and n = the level
+ 3, 2 or 1 by which of three research items the firer's side has done (the highest of Human 0x588D94 / 0x588D90 /
0x588D8C, Martian 0x58906C / 0x589068 / 0x589064, taken as items 85 / 84 / 83 and 83 / 82 / 81); every object in the
sector gets 336 hours (0x422BA0), and so do those in its neighbours (0x441AD0) not held by the firer's side, n of
them. Without an Infiltrate nothing gives intel, so the other side's counters stay hidden, as at the start of the
original's campaigns. (Before this update every battle revealed its sector, which the game doesn't do.)
*Provisional*: the research globals' item numbers (their place from 0x588C40 / 0x588F20, four bytes each); the
neighbours' order; not checked end to end.

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
research's status (0x45BEB0, see RESEARCH.md). Tab hides the frame and the mini-map (0x41A450). Which of the buttons' 4 frames is which isn't confirmed; the engine uses 2 for the pressed one
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

**Selecting and sending** (the war map view's mouse, 0x41C5E0 / 0x41D300 / 0x41D470; the player object's
selection at +0x430, filled by its recorded commands 4 clear, 5 select (Shift adds), 6 drop, 2 cancel):

- **A counter of yours** takes the mouse when pressed (0x42BEB0 0x201) and on letting go (0x42C160): in cursor
  mode 1 with Shift it's dropped from the selection if it's in it and added if not; in modes 0 and 1 otherwise
  the selection is just it; in modes 2 and 3 nothing.
- **The rubber band** (tracker 0x4E9EE0): dragging on the map tells the sectors 0x8010 with the box (0x43C2B0):
  each of your counters of the view's kind with its corner inside (PtInRect) is lit and counted (world +0x114).
  Letting go with any inside clears the selection and selects them (0x8011, 0x43C340), cursor mode 1. The
  engine counts it as a drag once the mouse has moved more than 2 pixels, and draws the box as a white line
  (*provisional*: the tracker's look isn't traced).
- **The route preview** (0x41D590 -> 0x419520), made again whenever the mouse goes onto another sector: in the Unit
  view, with units selected and no band out, the selected units are put in groups by their sector and how they go
  (naval: the unit's +0x52 is 5 or 6; 7; the rest), and each group gets a route to the sector with its arrow
  (+0x5F), its label hours the length over the slowest unit's speed. Cursor mode 2 when any can go (0x41FF70),
  back to 0 from 2 when none can.
- **A click on a sector** in mode 2 (0x43C3F0), with routes heading there: the selection is sent (the player's
  command 8, then cancel), sound 0x11F; each group's units take their places in its formation in order. In mode 0
  or 1 a click selects the sector if it's yours (or with the cheats 0x4D1249 / 0x4D1279) and cancels.
- **The cursor** (0x4EA438, set by what's under the mouse when told 0x20 with the mode): over a counter of yours 1,
  in mode 1 with Shift 3 if it's selected and 2 if not, 12 in modes 2 and 3; over a sector in mode 0 the default
  if it's yours and 12 if not, in mode 2 4 to move into yours or no one's and 5 into the other side's.
- Selected and lit counters get the ring (cselecth.spr, GType 0x54) and are drawn over the rest. A quick right
  click (Cancel) clears the selection.
- **The Warmap menu** (0x41D120): the right button held for `[Tweak] "Quick Cancel Timeout"` (5) updates opens a
  WPopupMenu (0x432C60, "Warmap menu", z 0x61B2) with Cancel (command 0x73, icon GType 0x1C RBHcncl, text 0x476),
  then the items the active tool, the selection or the sector under the mouse adds through 0x805C (the sector,
  if it's yours, is selected first). The menu is a grid of WHotButtons (0x434300, font 0x71): items with the same
  command share a row (0x4331C0), the rows go down a column (0x433220), and only the hovered item's row opens out
  (0x433330). It's shown with its first item centred on the mouse, a quarter of its height above it, kept 3 pixels
  inside the screen (flipped right to left if it hits the right edge), sound 0x193 (0x432FB0); Escape closes it
  with 0x196, a click outside closes it, and an item's command goes to the war map. It stays open only while held:
  the popup (0x4330B0) closes on any press or release of either button off its items (0x42F2A0), and an item acts
  as a button is let go on it (`[Modern] "Sticky popup menus"` keeps it open instead, docs/MODERN.md). Each item
  (WHotButton's 0x434390) plays 0x194 as the mouse comes onto it (0x20) and 0x195 as it's clicked (0x202 / 0x205), so going into
  a battle by View map clicks (the engine plays the screen's sounds before a command can put the war map aside). The engine shows the items'
  text as their tool tips and the icon's next frame under the mouse (*provisional*). After Cancel come the items of
  whoever answers 0x805C: the active tool if it answers 0x8111, else the player's current object, else the own
  sector under the mouse (selected first); and when neither a tool nor that sector filled it, the world's selected
  sector (+0x60). A sector adds **View map** (0x43B9F0: command 0x8133 with 0x8019, icon 0x2E `RBHview`, text
  0x475, which 0x432D70 / 0x432E90 replace with the hovered sector's name) while its battle map isn't open (+0x18):
  the briefing transition and the Briefing for that sector, VIEWING it (see Battles). An object (0x429A70) with
  a factory that isn't on its way anywhere, a vehicle or a finished construction (0x42AA70), adds **Build**
  (0x813E with 0x8065, icon 0x20 `RBHbuild`, 0x470) and, with something queued, **Suspend building tasks**
  (0x813E with 0x813F, icon 0x22, 0x495) while it's busy or **Resume building tasks** (icon 0x23, 0x496); see
  Production. The engine takes the first selected counter's first object as the current object (*provisional*).
  Still to come: Cancel Move (0x80A9, for a unit on its way) and an object's Information (0x440300).

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
  - 0: a unit in the route can't move: its factory (+0x54) has something queued (+0x128), a constructor at work
    (0x42A410)
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

Every whole game hour the world (0x802D, 0x419E9D) moves the routes, the players and both sides' research
(RESEARCH.md) on, then
the pods (0x41A330): while fewer than 8 have come, every `[Tweak] "Pod Interval (hours)"` (24) one lands in sector
2, 4, 6, 7, 8, 9, 10 or 11 (`byte_4B9DE8[rand() & 7]`) with one of four loads (`off_4B9E28[rand() % 4]`: unit 62,
ScoutMachineMk1, alone or with 89, HandlingMachineMk1, or 91, DiggingMechanismMk1). Each unit added works out who
holds the sector again (0x43CEB0 -> 0x43CA40), so a sector no one held is the Martians' once the pod is in. Then
in a Human sector the sector is marked (+0x64) for the battle check; otherwise it's event 12, PodLanded (0x41F010):
in a Martian sector (so also one no one held) sound 0x1B2 and only the note "Pod landed: <sector>" (0x41E, the
button 0x8122); in any other, a battle there (+0x124), sound 0x1B3 and "Pod landed in our territory" (0x420, Human)
or "Pod landed in enemy sector" (0x41F, Martian), with command 0x8072 to go to it: the case of a marked Human
sector the battle check raises. The
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
except in the Unit view, where it's 0 (0x41DAB6-0x41DAEF; the engine had it 0 in the Construction view too until
update 102). Battle sectors pulse only in the Unit view (0x41DD35).

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

## Production

An object that can build (its type among some type's builders, OBJ.ojd records 49-51: constructor vehicles,
factories) has a factory, WBuildList (0x45F770-0x462EE0, at the object's +0x54; `core/BuildList`,
`warmap/Production.cpp`): a queue of up to 32 entries and a busy flag (+0x12C).

**Saved** at the end of the object's WMOB part (0x42A220): after its battle map units, u32 has-a-factory, the
factory (0x460900) if so, then u32 the object's +0x3C (a vehicle's VEHU tag follows). The factory: u32 the queue's
length and, if that isn't 0, u8 busy and each entry (0x461F00): u32 type, u32 the object it's working on
(0xDEADDEAD for none), four u32s (where a construction goes), u32 repeats (1-3, 4 for ever) and the hours left
(+0x34, a float). Every object in Human.000, Martian.000 and Human.001 reads and writes back exactly
(`wowdump --factories`); none has anything queued, so the entries are as the code reads them (provisional until a
save with a queue turns up). Most objects' units end with BATP (VEHI for a vehicle), but some write more after it
(type 451 four more u32s); those have no factory, so their tail is the last 8 bytes.

**What it can make** (0x45F970, the list the Build List window shows): types 1-1008 whose builders include the
object's type and whose research (the unit table's +0x04) is done or obsolete, or which have none (0x442460).
In a sector without its command building finished (0x43C990) only the command building is offered (58, 278 in
sector 30; 120, 830 in sector 3; both sides' are tried), and nothing while the object is working on something
(0x42A410); in a sector with one, neither 58 nor 120 is. Type 39 isn't offered in the sectors byte_4BD2E0 marks
(3, 7, 10, 13, 16, 19, 20, 24; boats can't go there either, 0x4419E0).

**Every game hour** (its 0x802D, 0x45FB20 -> 0x45FC90), only while busy: the hours are divided by the product of
the sector's three ratios for the factory's side (0-2 Human, 3-5 Martian, "The economy"; 1 with the cheat
byte_58913C), so a short supply slows it. Then while there's time left and a queue, the entry at the head
(0x461D60):
  - starts if it's at 0 hours: ftol(record 43 x the manufacture modifier x 24), times the type's battle units for
    a vehicle (category 1). The modifier is `[Tweak] HumanFriend / HumanEnemy / MartianFriend / MartianEnemy
    Manufacture Modifier` (1.0, 0.85, 1.0, 0.85), by the type's faction and the side being played, as record 43 is
    read
  - tells the object it's working on (not for a vehicle) 0x80AD with the time, or 0x80AC once it's done
  - done, counts its repeats down (not 4) and passes what's left over on
A vehicle done is made in the sector (0x4620B0 -> 0x43CD10) and raises UnitCompleted (event 2) for it if the
factory is the local player's; anything else raises ConstructionCompleted (event 1) for its object. Then
0x45FF00: the head goes, back on the end with its whole time again if it has repeats left (and there's room), and
the factory goes on unless the queue is empty or a vehicle is next and the sector has no room for it (0x43AC40:
the local player's units, or boats, there and on their way, plus one, below `Max units in sector` / `Max boats in
sector`; the game adds counts from 0x4F86B8 / 0x4F76F0, the battle map's, see docs/BATTLEMAP.md, taken as 0 here;
`WarMap::HasRoom` takes the land flag and the count for the battle map's callers). When it stops it isn't busy any
more (the world redraws its shading, 0x4192F0) and, for the local player's, raises ConstructorIdle (event 9) if it's
a unit, else FacilityIdle (event 10).

Tested with one entry injected into a Human factory (type 35 in sector 13): type 1, 4 days and 5 battle units, so
480 hours, with 2 repeats. It made the two units 480 and 998 hours in, then raised FacilityIdle.

**The construction's side** (WMOB 0x42A4B0): 0x80AE, started, sets +0x78 to its whole time and +0x7C to 0;
0x80AD adds the hours to +0x7C (the counter shows +0x7C / +0x78 done); 0x80AC (0x42A980) sets both to 1.0. A
command building finished has the world think about the sectors again (0x418890; the engine builds the paths
again) and, if it's the local player's, raises CommandCentreBuilt (event 15) for the sector; anything else is laid
out again (0x4297C0, 0x8145). (The survey has 0x80AC and 0x80AD the other way round; 0x461D60 sends 0x80AD with
the time while it's short of the entry's hours, and the handler adds it.)

**Ordering** (the window's commands, recorded for playback as 0x17 and 0x16, 0x4601C0 / 0x460200, and handled by
the factory's 0x8026, 0x45FB20):
  - a unit (0x17 -> 0x4603B0): refused when the sector hasn't room (a boat for the other side's factories, any
    vehicle for the local player's); the same type as the last entry just counts its repeats up (0x461EE0: 1, 2,
    3, then 4 for ever); otherwise a new entry at the end (0x4604B0 / 0x461B60: one repeat, its whole time,
    working on nothing), not past 32, and the factory made busy if it wasn't (0x4192F0)
  - a construction (0x16 -> 0x460240, its place all 0 from the war map): added as above, then the entry set up
    from the order (0x461C80) and the construction made in the sector at once (0x4620B0 -> 0x43CD10), hearing
    0x80AE. With no place the game finds one for a command building on the battle map (0x4411B0, not in); then
    0x440EA0 for the sector (not traced)
  - Suspend / Resume (0x45FE60): on, with something queued and room for a vehicle at the head, makes it busy;
    off stops it
  - a queue slot clicked, either button (0x4625B0): a repeat less (0x461EB0: for ever goes to 3); at none, an
    entry working on a construction is cancelled (0x462120: from the war map the construction goes from the
    sector, and the entry with it; on the battle map the object hears 0x80A0), any other just goes (0x460080,
    the factory not busy any more once it's empty)
  - a slot's promote button (0x2FC -> 0x4605A0): the entry to the front, the factory made busy
  - cancel all (0x460140, `WarMap::CancelAll`, 0x813D to the object, 0x428F75): for a vehicle (its type's +0x10,
    0x440700, 1) each entry of a copy of its queue cancelled as above (0x462120). Leaving the battle map (0x439ED0)
    sends it to the holder's objects in a sector that had its command building before the owner was worked out
    again (0x43C990, then 0x43CA40) and hasn't after, when the sector's battle flag (+0x124) is set and the
    report's +0x6C is its +0x68 (`WarMap::FightBattle`, after a battle fought on the battle map). Then every sound
    fades over 30 (0x464AB0). *Provisional*: the report's +0x6C / +0x68 test isn't known and is taken as passed

**Can it be ordered** (0x462DC0, an item drawn greyed when not): the sector allows another (0x43C7F0: the type's
faction, record 26, is the sector owner's side, and the side's objects of its kind and type there, plus for a
vehicle those queued in the sector's factories, 0x421C60, are fewer than record 52, "max per sector", 20 by
default); it isn't a constructor vehicle's on the war map (+0x99: the factory in war map mode and its object a
unit; "This item can only be constructed on the BattleMap", 0x494); its prerequisite building (record 54) stands
finished in the sector (0x442420); and the queue has room.

## The Build List window

The window (0x460B00: "Build List", class 0x804A, id 0x5DCA; `warmap/BuildWindow.cpp`) opens from Build. It has
two panels at `[BuildList] Left` (0) and `Top` (70) (0x460A80 / 0x460AC0):
  - **the list** (0x461500): backdrop 0x2FA (`hbuildb1` / `mbuildb1`), scroll buttons 0x2FD at (0x1B, 9) Human /
    (0x1F, 0x15) Martian and 0x2FE at (0x1A, 0x12C) / (0x1F, 0x12B), each one place at a time while there are more
    than five past it (0x4618E0); five items from the scroll position (0x4619A0, the list from 0x45F970), at x 32
    and y from 46, each the last's bottom + 2: the type's counter picture from GType 0x78 (`HU-CNT48` /
    `MA-CNT48`), lit under the mouse, with its time in days ("%0.1f": record 43's days x its battle units, slowed by
    the sector's supply as production is) in font 0x6E at its bottom right (0x462A40). One that can't be ordered
    shows frame + 0x96 and no time. A click orders it (0x460D70)
  - **the queue** (0x460F10) at Left + 0x58, Top + 0x71: backdrop 0x2FB, the head's slot at (8, 6) Human / (8,
    0xD) Martian and four more from (0xA, 0x45) / (8, 0x44), each the last's bottom + 1, showing the entries after
    the head from the queue's scroll position (0x461490); scroll buttons 0x2FF at (6, 0x34) / (6, 0x38) and 0x300 at
    (5, 0xAC) / (6, 0xAA). A slot (0x4621C0 / 0x4624A0) shows the entry's picture from GType 0x76 (`HU-CNT24`),
    lit under the mouse, and with more than one to make the repeat badge (GType 0x6B, `HREPEAT`; frame 1, 2 or 5
    for 2, 3 or for ever, word_4CA7B8) at its bottom right; every slot but the head's has a promote button (0x2FC;
    the engine puts it at the slot's top right, *provisional*: 0x42F020(9) places it). "TTC" (0x461340): when
    the head will be done, its hours left slowed by the supply, as a route's arrival is written (0x41B9A0), centred
    at (0x14, 0x29) in font 0x70.
The tool tips (0x807C): an item's name, "Requires <building>" (0x51E) with a prerequisite, and 0x494 for a
battle-map-only one (0x462B50); a slot's, the name. A click on the window's own background closes it (0x201 /
0x204 in war map mode); elsewhere the map has the mouse.

Still to do: the battle map's side (placement modes 4 / 5 / 6, 0x42E790; 0x80A0); the cursors (0xE / 0xC over
items, 0xF over slots); 0x8131 (an entry's object gone); 0x440EA0; the counts from 0x4F86B8 / 0x4F76F0 in the
room check; where among the world's objects the factories' 0x802D comes (after the research here, *provisional*).

## The computer player

The other side is played by the AI player (0x442890, vtable 0x4AA680, handler 0x442AC0; `warmap/WarMapAi.cpp`).
Every game hour the world's 0x802D (0x419E9D) goes, in order, to the routes, the local player, the AI, both
sides' research (0x458BD0), the pods (0x41A330), then each sector (0x43B6C9: its objects, the factories among them,
then its day count +0x6C, each whole day taking one off its invasion cool-down +0x68). `[Tweak] AI Hours Per Turn`
(5) is read but used nowhere.

**Its building upgrades** (0x111 / 0xB9 from its research finishing a type, 0x442AC0 -> 0x442D80,
`WarMap::AiApplyUpgrade`): for a building type (+0x10, 2), in each sector 1-30 that's the AI's with its command
building finished (0x441A80), every construction there whose type's +0x84 or +0x88 (the type it gives way to,
records 19 / 20; +0x84 is never set) is the new type is retyped in place (+0x20; the saved record with it).
byte_56625C is set while it runs.

On its 0x802D (0x442EC0) the AI adds the hours to its clock (+0x448), lets go of its research item once it's done
or obsolete, or with none picks one (the other side's research, vtable +0x2C, 0x458A60: the first available item
that isn't a trigger and has a trigger among its prerequisites; otherwise from rand() % the item count on, the next
available one that isn't a trigger), then takes **one step** of its cycle (+0x43C):

| Phase | Function | Next |
|---|---|---|
| 0 | gather its combat units (0x443030) | 2 if none, else 1 |
| 1 | the next unit stays or moves (0x443340) | 2 once all have |
| 2 | constructors and the economy (0x443610) | 4 with no constructor, else 3 |
| 3 | a constructor sent to build (0x443CA0) | 4 when done |
| 4 | the front line, defence builders (0x4440B0) | 6 with none, else 5 |
| 5 | a defence builder sent (0x4443D0) | 6 when done |
| 6 | an invasion (0x444690) | 7 |
| 7 | | 8 |
| 8 | its factories' orders (0x444C10) | 9 |
| 9 | | 0 |

What it counts: an object's **strength** (0x441E70) is its battle map units (the group at +0x4C, its +0x24; the
engine counts a saved object's SELO ids after its own) times its type's AI.ojd weight, which is scaled as the file is
read by `[Tweak] AI strength table Human multiplier` (1.0) in Human games or `... Martian multiplier` (2.0) in
Martian ones (0x441625, truncated). Its **class** (AI.ojd's value) says what it is: 1 a combat unit, 2 a constructor,
4 a factory, 6 a resource facility, 7 a defence. F(s) (0x441F00) is the AI's combat units' strength in sector s (with
a target t, only those that can go there: 0x4419E0 lets a boat, base type 19 or 22, only into a sector that isn't
landlocked and has room). E(s) (0x442080) is what it would meet: the other side's combat units and defences there,
and the combat units of every route from or to s, whoever's. Neighbours (0x441AD0 / 0x441B80 / 0x441C50) are
sectors 1-30 in the route table's list with a land link, all of them, the AI's, or the other side's.

- **Phase 0** puts every combat unit in a binary tree by strength, rooted at the one nearest half-way between the
  weakest and strongest (swapped to the front), stronger to one side, the rest to the other, and reads it out
  strongest first: the order they move in.
- **Phase 1**, one unit: staying is worth (F(here) - S) x (1 + rand() x `AI Movement Temperature` (0.25) / 32768),
  doubled (`AI safe sector repeller Mul`) when every neighbour is the AI's, x 0.8 in sector 3 or 30. Each of the
  AI's neighbours it can enter scores that less F(n) / (n's enemy neighbours x `AI Frontline Attractor` (2.0) + 1) x
  `AI Move Threshold` (1.05); it goes to the best above 0.
- **Phase 6**: nothing unless the AI's sectors average at least `AI Invasion Threshold PC` (150). Each sector not
  the AI's is a candidate when what its AI neighbours could send, less `AI Retain Force` (0.2) of it, over its
  cool-down + 1, beats (rand() % 100 + 50) x 0.01 x E x `AI Outnumber Force` (1.1), the force it then wants. It
  scores its AI neighbours x `AI Invader NFriend Mul` (3000) plus what its resources are worth (each kind's spread
  demand - spread supply + `AI Build Resfac Eagerness` (50), times one more facility's gain, at least 0). The best is
  invaded (0x444990) and its cool-down set to rand() % 8 + 10 days. The invasion takes the combat units of the
  neighbours with a command building that can go in: all of them if they fall short of the force wanted + 1, else
  shuffled (4 x their count random swaps) and taken in turn until they make it up.
- **Phase 8**: each finished factory (class 4) with nothing queued orders one thing from what it can make
  (0x45F970, each swapped for the type it gives way to, record 19 / 20, when that's offered too, 0x45F900): the
  first constructor (class 2) of which none is idle or queued in the sectors the factory's sector has a path to;
  failing that a combat unit at random (rand() % their count); added with 0x4603B0.

A move (0xAD, 0x429E80) isn't a route: the object leaves its sector and is put straight into the next (0x429DB0),
both sectors' owners worked out again (0x43CA40). `[Debug] "Enemy Visible"` would also put its counter on the
sector's waypoint. Its 14 `[Tweak]` values: `AI Frontline Attractor for Defences` 2.0, `AI Eff Threshold` 0.6,
`AI Panic Build Threshold` 500, `AI Move Threshold` 1.05, `AI Frontline Attractor` 2.0, `AI Build Resfac Eagerness`
50, `AI Aggression Value` 0.5 (used nowhere), `AI Invasion Threshold PC` 150, `AI Outnumber Force` 1.1, `AI Retain
Force` 0.2, `AI Invader NFriend Mul` 3000, `AI safe sector repeller Mul` 2.0, `AI Minimum Rr` 8.0, `AI Movement
Temperature` 0.25.

**Building.** AI.ojd's sites (0x441580, opcodes 5 / 6) are {type, x, y} or a fence's {type, x1, y1, x2, y2}, in
a list for each side and sector (the side being played at 0x518994, the other at 0x518814: the computer player's).
A site holds the object of exactly its type standing at its place on the battle map (0x440F80: the battle object's
+0x78 / +0x7C truncated, which in a saved construction are the floats 16 and 12 bytes before its last BATP; a
command building matches wherever it stands). The game links them at the start (0x441110), when an AI
construction comes into a sector (0x441420: the first free site of its type takes it and it's moved there, 0x80D1 /
0x80D2) and on some sector events (0x440EA0); the engine works the link out when it's asked (*provisional*: the
same while things stay where they're put). Each sector has a record (0x545BB0, 0x18 bytes): +0 finished factories,
+4 constructors there or heading there, +8-0x10 its network's idle factories' work use (record 47), +0x14 an idle
factory, +0x15 a resource facility going up, +0x16 a factory going up, +0x17 needs its command building. A sector's
network is the sectors its path reaches (0x43C090: the path has more than its start) and itself.
  - **Phase 2** (0x443610) clears the records, counts each sector's constructors (class 2 with a factory) and
    gathers the idle ones that can put up the side's command building (58 or 120, 0x442460). Then for each of
    the AI's sectors not yet in a network: its network's sites with objects mark finished and idle factories
    (class 4), factories and resource facilities (class 6) going up, and whether the command building stands; an
    AI sector without one, finished or on a site, needs it. Each network sector that's the AI's, has its command
    building and no facility going up gets, per kind, a priority (spread demand + the network's use - spread
    supply + `AI Build Resfac Eagerness`) x one more facility's gain, and each above 0 is a candidate (scored by
    the gain), sorted by priority (a bubble sort). Phase 3 follows unless no constructor was gathered.
  - **Phase 3** (0x443CA0), one constructor: unless a factory is going up where it is, the factory site (class 4,
    the first buildable, 0x441310: its research done and its prerequisite standing finished, 0x4423D0) with the
    best efficiency (0x4607C0: 1 / the product of max(1, (spread demand + the type's work use) / spread supply)
    over the side's kinds) in its network's sectors with a command building; above `AI Eff Threshold` (0.6) it's
    built there and the network marked. (A "panic" build would follow, but the game compares the best candidate's
    index with `AI Panic Build Threshold`, 500, so it never happens.) Else a network sector needing its command
    building, its own or one no constructor is in, if its own type runs better than 0.4 where it is. Else the
    first resource candidate with a gain above `AI Minimum Rr` (8.0), no facility going up, its own sector or one
    without a constructor, and a free site for the kind's facility (47-49 Human, 108-110 Martian, dword_4BEDC0).
  - **Phase 4** (0x4440B0): each AI sector with its command building is a front candidate scored mult / (its
    defences' strength (class 7) / (enemy neighbours x `AI Frontline Attractor for Defences` + 1) / c + 1), c 1.4 in
    sector 3 or 30, else 1, and mult 2 on the front (an enemy neighbour) or in sector 3 or 30, 1.5 beside the front,
    else 1; sorted highest first. The constructors are counted again and the idle ones that can't put up a command
    building gathered for phase 5.
  - **Phase 5** (0x4443D0), one of those: down the candidates while they score above 0, the first its path reaches
    (not its own sector) with no constructor there and a buildable defence site (class 7) gets that defence.
  - **Sending one** (0x444510): it moves there (0xAD) if it isn't, the constructor counts kept, and is told to build
    the type at the first free site for its base type (0x4411B0 -> 0x460240); a Communications Centre (120) brings
    its partner (119) when that has a site.

Not yet: `[Debug] "Enemy Visible"`. *Provisional*: a sector's objects are taken in counter order (the game's
lists keep the order they arrived in); the invasion cool-downs aren't saved; a move doesn't stop the object's factory
(+0x54's vtable +0x18, not traced); fences' second point isn't written into a new fence; a new object of a type no
saved object has is copied from another of its kind and category with its type changed (its battle data stays the
copy's).

## Battles

**Who holds a sector** (0x43CA40, whenever a unit goes in or out, 0x429E80, or objects are taken away): the side
that alone has units or constructions there (less units counting themselves out with 0x8116); with both, it stays
as it was; with neither, no one. So a unit setting off can leave its sector to no one, and one arriving where no
one else is takes it.

**Finding battles** (0x41AB00, every tick while none is waiting, dword_4E9B98): the first sector with both sides'
units or constructions in it and no battle flagged (+0x124). If its holder's side (the other side's, for a sector
no one holds) has no units there and no class 7 construction (0x43C8D0), it's UndefendedSectorLost (event 17) and
then all that side's units, constructions and resources there go (0x43A250, the sector worked out again);
otherwise it's PodLanded (if a pod came, +0x64) or BattleInSector, and the battle waits there.

**The battle** (0x8072, the box's Defend / Attack): the world's selected sector becomes the battle's (+0x60 =
sectors[dword_4E9B98], or the box's sector when none is waiting) and it sends itself 0x8133 with 0x8019. Its
commands 0x71, 0x7D, 0x97-0x99, 0x8017 and 0x8019 are deferred: kept in dword_4E9B64 / 4E9B5C and handled one
queue pass later (0x95, 0x419A24).

1. **The transition** (0x8133 → 0x41B210, "Fading up briefing sound over 1 second"): every sound fades out over 30
   ticks, the ambient sound goes, the CD stops and its position is reset (0x412C00, 0x412940), the view gets 0x6E,
   and the briefing sound 0x18E starts at the mouse, faded in over 60 ticks (dword_4E9B84). With the side's disc in
   (0x408B70) it sends 0x8019, else it opens the Insert CD menu (0x4089D0) with it (the engine's check always
   passes). The war map's music and clock wait until it's back.
2. **The Briefing** (0x8019 → 0x41A13C: 0x42DB00(0), the world hidden, `new 0x499930`; layout 0x4999E0, see
   `src/frontend/Briefing.h`). Palette group 0x2EE (hb: HBHB → slot 8, HBHI → 5, HBGI → 9, HMINIT → 21, 0x40BE05),
   backdrop 0x91, font 0x73 with lines its height + 2 apart; columns x 55 / 285 (Human) or 85 / 276 (Martian), the
   first line at y 170 (Human player) or 72 (Martian). The heading (VIEWING 0x497 when the sector hasn't both sides
   in it, 0x43D010; else ATTACKING 0x498 when it isn't the player's, else DEFENDING 0x499) and the sector's name,
   "Local Conditions:", "Date/Time:" with the calendar (0x41B440, "hh:mmap dd mmm yyyy", split into time and date
   lines) or, in Martian games, the game time in days ("%07.2f"), and with the player's forces there "Forces
   Present:", "Units (Vehicles):" (the war map objects, and their battle map units, 0x421F90) and "Facilities:".
   The mini battle map (0x4A1F90: `maps\hmin%02d.spr` / `mmin%02d.spr` for the sector, drawn with shade slot 0x15)
   is centred in x 354-609, y 95-380. Colours: [Colours] "Briefing Sector Name" 17, "Briefing Heading" 17,
   "Briefing Detail" 18. A key or click sends 0x98 at once (0x49A700); otherwise 0x71 comes after `[Briefing]
   "Delay"` (30) ticks.
   The Briefing posts 0x71 to itself, so like a menu click it goes through the Fullscreen View (0x4065E0): the
   fade out, then 5 ticks later on to the world (+0x84); the same for 0x98 and the Debriefing's 0x99 / 0x8017. In
   the game the battle map loads behind the Briefing once 0x71 arrives; the engine has no battle map, so the
   Debriefing follows the fade at once.
3. **The battle map** (0x71, or 0x98 after the queue is flushed, 0x435600 → 0x41A082): 0x44E5B0(1), both
   research objects' vtbl+0x28, 0x42DB00(1), the world hidden, byte_4D2554 = 1, and the sector shown. Its end comes
   back as 0x97 (and leaving a viewed sector, 0x76 to the world, flushes the queues and sends 0x97 too). The engine
   shows the sector's ground (docs/BATTLEMAP.md) until Escape; nothing is fought there yet, so at 0x97 the engine
   settles the battle (*provisional*): with only one side in the sector (a
   sector being viewed) there's none and the Debriefing says RETURNING TO WAR ROOMS; otherwise the side with
   more men in its units there wins (the holder on a tie), the loser's units, constructions and resources there go
   (0x43A250), the sector's flag goes and it and its neighbours are seen (0x418E17), and the game is over when the
   Humans win in sector 3 or the Martians in London (30). The briefing sound is faded out over 120 ticks as the
   battle map opens (0x41B360 from EnterBattleMap: 0x464EB0 with 0x78), and the Debriefing follows at once.
4. **The Debriefing** (0x97 → 0x41A0DC: 0x42DB00(0), `new 0x49A7B0`; layout 0x49A820), on the Briefing's screen:
   "RETURNING TO WAR ROOMS" (0x4A8, sound 0x18E) with no battle fought (the report's +0x54); "DEFEAT" (0x4AA, 0x190)
   when byte_4E9B94 is set or the player has nothing left there (+0x28 + +0x48), else "VICTORY!" (0x4A9, 0x18F); the
   sound faded in over 60 ticks. After a battle, 0x4AB ("... were eliminated") or 0x4AC ("... are occupying this
   sector") wrapped to the columns, three lines on "Forces lost:" (0x4AD: +0x58 (+0x60) and +0x18 − +0x28) and
   "Enemy forces destroyed:" (0x4AE: +0x5C (+0x64) and +0x1C − +0x2C), each with "Units (Vehicles):" and
   "Facilities:", and the mini map. A key or click sends 0x99 five ticks later (0x49B900); otherwise 0x8017 comes
   `[Debriefing] "Delay"` (300) ticks after it's first drawn (0x49B830).
5. **Back on the war map** (0x99 after a queue flush, or 0x8017 → 0x419AA2): no child view, the world shown again,
   every sound faded out over 30 ticks, the ambient sound dropped, 0x8147 after 30 ticks (the music again,
   0x41B070) and, unless the game is over (+0x55), the view sent 0x6E. The view's set-up (0x418980) then sends
   0x808D, the announcement (0x41AED0) of the battle report (0x421210 at 0x4E9B7C: sector +0x50, winner +0x68,
   loser +0x6C, game over +0x55): the player won: EndGameWon (18, with the other side's home sector) if the game is
   over, else sound 0x13C; lost: EndGameLost (19) if over, else 0x140; no winner: 0x13D (the player's report) or
   0x13F. And the queued events go on (0x41E730). The game's end codes (0x4212A0): 0x80FA Human victory / 0x80FD
   Martian defeat, or 0x80FB Human defeat / 0x80FC Martian victory.

What the text objects' +0x3C (set on the section titles) does is unknown: the text drawing (0x40F770) doesn't read
it. The view's 0x6E isn't rebuilt.

**The end** (0x41F400 / 0x41F4B0): byte_4D2530 is set and the world stands still; the game is sent 0x80FE with the
code after the victory sound's length and 30 ticks (the engine waits 6 seconds), or 30 ticks for a defeat. The
front end (0x402B80) shuts the war map down and plays VICTORY (0x80FA / 0x80FC) or DEFEAT, then the credits, then
the main menu (0x80C9).

## Messages and events

The war map's events (a table at 0x4BA0E8 of handler, "is it critical" test and name, 12 bytes each, raised by
0x41E5B0): 0 BattleInSector, 1 ConstructionCompleted, 2 UnitCompleted, 3 ArrivedInSector, 4 ResearchCompleted,
5 ResearchAvailable, 6 ResearchAlert, ConstructorOnline, FacilityOnline, ConstructorIdle, FacilityIdle,
TravelInterrupted, PodLanded, ResourceWarning, ResourceAlert, CommandCentreBuilt, EnteringNeutralSector,
UndefendedSectorLost and End Game (won, lost). 0x41E5B0 asks the event's test: a critical one waits in a queue
(0x4EA064) while a box is up (byte_4EA0A0) and otherwise runs and marks a box up; any other runs at once. When a
box goes (0x8122 to the war map), 0x41E730 runs the next queued event's handler, or clears the flag. The tests are
the `[Event]` options (0x41E160-0x41E3E0, the Critical event options menu, plus "Entered Neutral Sector" which isn't
in it), always for BattleInSector and the endings, and "unless the cheat 0x4D1261" for ResearchAlert, PodLanded and
UndefendedSectorLost. A handler writes its line to the message list and, if its option is on, opens the box. The
flag is set after any critical handler, box or not, so a critical event that put nothing up would leave the queue
waiting until a box does go (ResearchAlert only puts its box up for the local player's research, and only that
research raises it): kept as the game has it. ConstructorOnline,
FacilityOnline and EnteringNeutralSector have empty handlers (nullsub_3), and nothing in the game raises 1, 7, 8, 9
or 16 with a constant id (0x41E5B0's callers raise 0, 2-6, 10-15 and 17-19 that way, and one, in production at
0x45FD91, with a computed one).

**The InfoBox** (0x41E4E0 -> 0x432520, WDialog "InfoBox" 0x801B, z 0x65FE, modal): time stops (0x41A6C0(0)), the
view goes to the sector (0x41E000, which glides; the engine jumps), sound 0x198. The backdrop (GType 0x68, hu_event
/ ma_event) is centred 4 pixels above the bottom; in Martian games ma_scrn (0x69) is drawn at 50% at +0x0B, -0x5E and
the text goes from its corner. The text (font 0x73, wrapped to 320 pixels by 0x4161E0, centred, shade 21) sits
0x2B (Human) or 0x17 (Martian) down, a second line (shade 22, centred too, as the original shows it) 16 pixels
below it. The button (GType 0x6A hu_buton / ma_buton, made by 0x42FB40 like the popup menu's items) is at +0x52,
+0x76 (Human) or +0x87, +0x28 (Martian; the offsets come from `and al, 0CBh` / `and eax, 4Eh` on -1 or 0); its text,
"Press button to continue" (0x47F) or "... ESC to cancel" (0x480) for a box that can be cancelled, is its tool tip,
not drawn on it. Its button (or Enter / Space) sends the box's command
(0x432B30): 0x8122 closes it and tells the war map; any other command (0x8072, a battle) goes to the war map and the
box closes without 0x8122, the queue waiting until the war map comes back (0x418980 -> 0x41E730). Esc on a box that
can be cancelled is 0x8123, closing it too.

The message list (PopupMessageController 0x420A30, at 0x28, 4) keeps at most 10 lines (0x4BA4FC), each a
"Message Item" (0x428810, font 0x72) shown for 5 x 30 updates (0x4BA4F8), drawn top down 16 pixels apart
(0x4BA500). Lines that have gone are kept (they count towards the 10). When a line runs out (0x420CD0) the list
scrolls up a pixel an update; once it has gone up 16 the first run-out line is gone and the list drops back, and it
goes on scrolling while more have run out. M (world 0x6C, a toggle on +0x6C) opens the history with sound 0x193:
every line with less than 5 seconds left gets 5 more (0x421150), so the last 10 show; closing it (0x6D, sound
0x196) takes the 5 seconds off them all. Command 0x80CC gives the last 2 (0x4BA504) another 5 seconds.

The handlers (each event the engine raises so far, and the ones waiting on production and research, which run the
same way once those raise them; their parameter is the object's counter, or the research's name):

| Event | Box (when its option is on) | Line | Sound |
|---|---|---|---|
| BattleInSector (0x41E850) | always: ours "Enemy have attacked" (0x4AF) / "Defend" (0x4B2), else "Invasion commencing" (0x4B0) / "Attack" (0x4B1), command 0x8072 | "Battle in" (0x210) and the sector | 0x138 ours; 0x139, or 0x13A for sector 3 or London |
| ArrivedInSector (0x41EAE0) | "Unit Arrived At Destination": the line, Unit view first | "Vehicle/s arrived in" (0x211) and the sector | 0x120 |
| TravelInterrupted (0x41EF80) | "Travel Interrupted": "Travel interrupted" (0x419), Unit view first | the same | 0x147 |
| PodLanded (0x41F010) | Martian games go to the Unit view; in a Martian sector "Pod landed: <sector>" (0x41E); otherwise a battle (+0x124) with "Pod landed in our territory" (0x420, Human) or "Pod landed in enemy sector" (0x41F, Martian) and Defend / Attack, 0x8072 | the same | 0x1B2 / 0x1B3 |
| UndefendedSectorLost (0x41E960) | "Undefended Sector Lost" (0x49E, ours) or "Undefended sector captured" (0x49F) and the sector, after "Pod landed" on a line of its own if a pod came (clearing the pod flag); Unit view | the same, "Pod landed; " before | 0x1B2 with a pod |
| EndGameWon (0x41F400) | "Victory is ours!" (0x533), Unit view | the same | 0x13E |
| EndGameLost (0x41F4B0) | none; Unit view | none | none |
| ResourceWarning (0x41F280) | "Resource Warning: " (0x41B) and the resource (0x214 + kind) | 0x124, 0x122, 0x123 (Human), 0x125-0x127 (Martian) |
| ResourceAlert (0x41F340) | the same | 0x12A, 0x128, 0x129 (Human), 0x12B-0x12D (Martian) |
| ConstructionCompleted (0x41EDC0) | "Construction Completed": "<name> completed." (the type's name 0x440720, 0x4B4) for its sector, the Construction view for a type of class 2 (unit table +0x10, OBJ.ojd record 17 before its record 15), else the Resource view | the same | 0x12F, not for AI class 7 |
| UnitCompleted (0x41ECD0) | "Unit Manufactured": "<name> unit completed." (0x4B3), Unit view | the same | 0x131 for a type of one battle unit (+0x58, record 55), else 0x1C0 |
| ResearchCompleted (0x41EB90) / ResearchAvailable (0x41EC30) | "Research Completed": "Research completed: <name>" (0x46B) / "New research available: <name>" (0x46C), no sector | "<heading> .. <name>" | the heading's speech (0x414700: TEXT.ojd's record type 4 for it) |
| ResearchAlert (0x41F220) | for the local player's research (its +0x88): always, "No research is currently in progress" (0x421) | none | 0x135 |
| ConstructorIdle (0x41EEA0) / FacilityIdle (0x41EF10) | "Constructor Idle" / "Facility Idle": "Vehicle idle" (0x416) / "Facility idle" (0x417) for the object's sector, Unit / Construction view first | the same | none |
| CommandCentreBuilt (0x41F1C0) | "Command Centre Built": "Command centre built" (0x415) for the sector, Construction view | the same | none |

The sector under the mouse, the panel button's name and a refused route's reason show in a strip at the bottom
left (*provisional*: the engine's own).

## The clock

The calendar object (0x41F550) adds `speed / 30` hours to the game time every update (0x41F640, 0x41F6C0; 30 is "Game
Frequency"), with the speeds at 0x4BA470 the floats 0.0-10.0 (the slider's position). Each whole hour moves the date on (0x41B770),
with February's leap years. The clock document (0x41F550) is made with speed 0, so the war map starts with time
stopped until the slider or a digit sets a speed.

**Time running** (world +0x64, 0x41A6C0): turned on by a digit, which sets the speed first (0x41A450; refused,
nothing changing, while any sector holds units or constructions of both sides, 0x41AEA0 -> 0x43D010); turned off by
Backspace, any press of a mouse button on the map (0x201 / 0x204, a counter too), the research screen (0x7D),
every critical event's box, and a speed of 0 (0x41F640). A change is recorded in the player's command stream
(actions 0xA on / 0xB off, `{01,01,code,04}`) and the opponent is told (vtbl +0x30; the computer, 0x442A80,
records 0xC / 0xD); when the commands are carried out the world gets 0x8050 / 0x8054 for each on, adding one to
+0x68, and 0x8051 / 0x8055 for each off, taking one away and setting the speed to 0. While +0x68 is exactly 1 (one
side waiting for the other, as in a network game) the clock document's +0x38 is set and the calendar flashes:
it isn't drawn, nor its wheels turned, while the tick count's bit 1 is set (0x4271E0). With the computer both
records come together, so what shows is that turning time off stops the clock. A slider move sets the speed
(0 if a sector refuses) without turning time on.

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
| 0-9 | the speed, '0' being 10, and time running on (unless a sector with both sides in it refuses) |
| Home | centre on the selected sector (its rectangle's middle, 0x41E000) |
| PgUp / PgDn | to the top / bottom of the map |
| arrows | a seventh of the map's width or a sixteenth of its height (Ctrl: a third / an eighth) |
| Esc | the war map menu (0x80B5) |
| R | research (0x7D) |
| M | the message history (0x6C / 0x6D) |
| Backspace | turns time running off (+0x64, 0x41A6C0(0)), which stops the clock |

## Scrolling

Besides the keys, the engine scrolls at the screen edges and by clicking or dragging on the mini-map, at
`[Screen] "Scroll Speed"` / 5 pixels a tick (*provisional*: the game's own edge scrolling isn't traced).

## Still to do

The game's scroll rules, the event messages (top left; the engine shows only the latest), the own sectors' shading,
the arrows' exact curve and head, construction (the Build List window, and the construction's side of it, so command posts can be built), the AI's turns, saving, and the
battle map.
