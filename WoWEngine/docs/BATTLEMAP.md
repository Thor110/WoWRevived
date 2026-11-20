# The battle map

Milestone 5. This page grows as the battle map's parts are worked out; so far it covers the terrain and the first
form of the battle map screen.

## The screen (`src/battlemap/BattleMap`)

After the Briefing (0x71 / 0x98, see docs/WARMAP.md, Battles) the front end opens the battle map for the battle's
sector, as EnterBattleMap (0x439770) begins: the sector's map from OBJ.ojd record 8 (0x437ADD: the sector, then a
name to the sector table's +0xAC, `land03.cls` and so on; `ObjOjd::LoadSectorMaps`), the landscape lit for the
calendar's hour (ftol(+0x28) % 24, 0x47AF10), the shade tables for that hour's letter and the LANDR digit from the
battle before, and the briefing sound stopped (0x41B360). Then it counts the enemy's units and constructions in the
sector for the next battle (0x43983E). The screen uses palette group 0x2F0 (bm).

The window (WBattleMap 0x484A30, "BattleMap", 0x8018) makes the terrain view over the screen (0x484D00, 64 x 64
tiles), sets the world's size to the landscape's (+0x1C, +0x20) and scrolls to the start view. The view's scroll
is kept between 0 and the world's size less the view's less 1 (0x42F6F0 / 0x40E5A0).

- **Start view** (0x439B7E): the player's HQ, looked for among the Human constructions (type 278 in London or 314
  elsewhere in a Human game; 830 in sector 3 or 120 elsewhere in a Martian one) and centred on its units (0x42A2A0
  / 0x42A340); else the deployment's centre (dword_4F742C / 4F7434); else the map's middle, (+0x1C / 2, +0x28 x
  +0x30 >> 16 / 2). It's projected with the height there (0x47B170, `Landscape::ProjectY`) and half the screen is
  taken off. *Not yet*: with no objects or deployment it's always the middle.
- **Edges** (0x484DF0): with the mouse within 4 pixels of an edge (32 in a window), the view moves by the mouse's
  distance from the centre / (half the width / 40), across and down at once.
- **Keys** (0x485610), a step a press: Up / Down the world's height / 16 (Ctrl / 8); Left / Right its width / 7
  (Ctrl / 3); Page Up / Down to the top or bottom; Escape the battle map menu (0x80B8). *Not yet*: the digits
  (select group, packet 0x12), Backspace (halt, 0x11), Tab (0x80C8) and Home (centre on the selection, 0x43A480).
- **Battle map menu** (0x49BD50, "Battlemap menu"): heading 0x3FD "Battle Options", Resume (0x8123, 0x405) and a
  bottom Sound Control (0x3ED, 0x411); no save, no quit. Drawn over the battle map, sound effects at a quarter and
  the world paused while it's open (0x484FB0).
- **Popup** (0x485DE0, the right button held for `[Tweak] "Quick Cancel Timeout"` updates): Cancel (0x73, icon
  0x1C) and, unless the player has a selection or both sides have objects on the map, Return to Warmap (0x76,
  icon 0x2C, 0x47A), then the items of whoever answers 0x805C. Return to Warmap goes to the world, which ends the
  battle map (0x97); with no objects on the map it's always offered. The Debriefing then follows as before.

## The landscape (WLandscape, 0x47AF10-0x47CCF0)

`core/Landscape` (`wowdump --landscape` builds one from a map's CLS and ATM, runs the queries over every vertex and,
given OBJ.ojd, an hour and a shade table, lights it and writes the light as a picture). The game keeps one
landscape at a time (dword_59D940).

### Building it (CreateTerrain, 0x47B900)

It looks for `maps\<name>.ils` first (+0x44 = 1) and then `.cls` (+0x44 = 2); every map on the discs is a CLS.
From the CLS header:

| CLS | WLandscape | |
|---|---|---|
| 0x00 / 0x04 | +0x24 / +0x28 | width and height in vertices |
| 0x08 | +0x2C | vertex count |
| 0x10 / 0x14 | +0x1C / +0x20 | world width; projected height |
| 0x24 | +0x30 | cell size, 16.16 (20.0 on most maps); +0x34 half of it |
| 0x28 | +0x38 | screen row step |
| 0x2C | +0x3C | height scale, 16.16 |
| 0x30 | +0x4C | height offset |
| 0x44 | +0x58 | the height bytes |

+0x48 is the top edge's screen y: (first height x scale >> 16) + header 0x18 + header 0x30.

`vertInf` is ten bytes for each vertex: +0 the lit level, +1 moving lights, +3 to +5 the normal (x 127, signed),
+6 the height as a word, +8 the ATM byte of the cell the vertex is the top left corner of, +9 flags. The ATM byte's
low nibble is the cell type (1-3 water, 8 impassable, 10 no building) and its high nibble the slope less one. The
height is ((byte x scale >> 16) + offset) x 92682 >> 16, so x sqrt 2. Every vertex but the last row and column
gets flags 1; 2 marks a vertex blocked, 4 occupied. CalcNormals then runs over the whole map.

### Queries

All take world positions (vertex x cell), as integers, and work in the game's fixed point:

- **HeightAtNearest** (0x47B120) and **HeightAt** (0x47B170): the vertex's height, or the two-way blend of the
  four round the point with fractions in 1/256ths. **NormalAt** (0x47B240) blends the normals the same way.
- **ScreenYToWorldY** (0x47B3F0): walks the column from the far edge and gives the first row whose projected
  height is at or above the screen row.
- **IsAreaPassable** (0x47B520): not on the outer two vertices; no impassable, water or too steep vertex.
- **CanBuildAt** (0x47B610): the area kept off the border (`[Tweak] "No-build border width (vertices)"`, 16), no
  no-build, impassable, too steep (`"Max Slope For Building On"`, 6), blocked or occupied vertex; a building that
  stands in water needs water and land under it, any other no water.
- **CanBuildLine** (0x47B770): the same border; 2 to 25 vertices long, stepped in floats; over water only when
  allowed (and then it must cross some), never across a blocked vertex.
- **MarkOccupied** / **ClearOccupied** (0x47C280 / 0x47C3A0): set or clear flag bits over a circle measured with
  the quick distance |dx| + |dy| - min / 2 (0x43FF00).
- **Scorch** (0x47C4C0): darkens a byte for each vertex in a radius by (r - d) x (4 + intensity / 128) / r, up to
  255. The game keeps these bytes in the battle world's layer (+0x8C, +0x20) and redraws the screen rectangle the
  blast covers; the engine keeps them in the landscape until the battle world exists.
- **ClampWorld** (0x47C7E0), **SetPointLight** / **ClearPointLight** (0x47C830 / 0x47C8A0) and the 32 x 32 light
  masks the vehicles' headlights lay on the ground (0x47C900 / 0x47CAB0, called from 0x48E383 and 0x48E657).

### Time of day and LightTheMap (0x47BD80)

OBJ.ojd record 9 gives each hour a sun angle in degrees and three named sets of four floats (64 bytes each at
0x4DA828). The hour is the game time's hour (`[Game] "Time Of Day"` overrides it when byte_4D1279, a cheat flag, is
set). 0x40B030 turns the first set into the light: the landscape keeps its first float as the strength (+0x50) and
its second as the ambient (+0x54); sin and cos of the angle weight the flat and the facing-slope parts, and the
point lights get 64 x the third float x (0.2 + 0.5 sin). At the start of a battle (0x40B570) the first letter of
the set's name picks the shade table `dat\LANDR<d><letter>.int` (a Martian game, byte_4B84C4 "I Am Human" clear, uses X for L and Y for N); the last
of its 256 bytes at or under the third float x 64 gives the light level (/ 256).

The digit `<d>` (dword_4DAED0) is 0 for up to 3 (dword_4BD4F8) enemy units and buildings in the sector, 1 for 4 to
9 and 2 for 10 (dword_4BD4FC) or more. The battle map set-up counts them into dword_4F7430 (0x43983E: the Martian
units and buildings in a Human game, the Human ones in a Martian game) only after it has loaded the tables
(0x439812), so the files a battle uses follow the count from the battle before; the first battle of a session uses
0. The same count makes some scenery start out burnt (0x4890C0). `Landscape::ShadeClass`.

LightTheMap then, for every row but the first and last:

1. lights each vertex by its slope to the next vertex across and down (0x40CA90): sqrt of the light direction
   against the normal, x strength, + ambient, x the level, added to a zeroed byte buffer ("ISOTemp");
2. casts shadows along the row: from each vertex (its height smoothed 3 x 3, 0x43FD10) up to 16 vertices away
   from the sun, any vertex the rise over the distance reaches the sun's tangent sets to level x ambient.

The lit level is then the buffer smoothed 3 x 3 for every vertex two in from the edge; the edge vertices go to 0
and the ring one in keeps what it had.

## Drawing the ground (WLandscapeView, 0x49F240-0x4A0370)

`core/TerrainView` (`wowdump --terrain` draws a whole map, or a 640 x 480 view, for an hour).

The ground is not drawn from the heights: a CLS carries, after its heights, a pre-drawn patch for each half cell.
Header 0x38 is a u16 map of patch numbers, (width x 2 - 2) for each row of cells, 0xFFFF for none; header 0x3C a
table of u32 offsets; header 0x40 the patch data (all from 0x44). (An `.ils` map would index the offsets directly;
none ships.) Header 0x34 (0xF8) masks the heights the patches line up on.

A patch is: width (bit 7 picks which side the middle corner is on), height, the row of the middle corner, and a
byte whose low two bits name the corner it hangs from (0 / 1 the top two, at the top; 2 / 3 the bottom two, its
height above them) and whose next three pairs name the corners whose light goes to its first, second and third
point. Then its rows: an x offset, a count (bit 7 for a run of one texel) and the texels.

Each frame (0x49FDA0) every vertex in view gets a shown level: lit level less the scorch byte less the moving
light, times the fog factor (fog byte / threshold blended towards 1 by WFogOfWar +0x90, or 1 at or over the
threshold), kept to 0-255. The LANDR `.INT` table (byte_4DA660) turns it into a shade level, kept to 8-63.

Then row by row from the first row in view (0x49F890), the half cells left to right: a cell is skipped unless its
top left vertex has flag 1; the patch is placed at the cell's x (cell x column) and its corner's projected y,
((row x rowStep - (height & mask) x scale) >> 16) - header 0x30 + the top edge, and drawn (0x4579A4 / 0x45702D)
with the light running down its two edges and across each row in 1/256ths of a level. The colour is level x 256
+ texel in shade slot 20, `dat\LANDR<d><letter>.SHH` (the battle map group 0x2F0 fills slots 1-5, 9-11, 20 and 21
with `BMHB`, `BMHV`, `BMMV`, `BMMB`, `BM?I`, `BMGI`, `BMR<d>NA`, `BMEX`, `LANDR` and `?MINIB`). Drawing stops after
five rows whose highest point is below the view.

Dither: ISOLib (0x410F80) builds 16 sets of four 64-entry tables from `dat\dith.dat` (the first four sets'
ranges set in the code) with the game's random table, so like the game it differs from run to run. Each entry
darkens by 0 or a few levels. Water corners (ATM 0-3) use their own set and land set 8; patches 16, 12 and 8 rows
high or less use denser tables. Each pixel takes the next entry from a phase that moves on four bytes a pixel;
the game starts the phase from the patch's address, the engine from its offset in the file.

*Not yet:* the black fill above and below the map (0x457D44), drawing objects between the rows (0x49F70A), and the
view's place in the battle map screen.

## Scenery (`dat\<sector>.nsb`, `core/Scenery`, `core/IsoObject`)

With `[Debug] "Load scenery"` on (the default, 0x4F73F1), EnterBattleMap loads `dat\<sector>.nsb` into the world's
+0x11C list. It's that list's save (WObjectList 0x47D240 / 0x47D360): u32 0, the tag `BMOL`, a count, and each
object as its u32 type and its class's Load (vtbl+8), without the save file's tags. The classes the 30 files use:
scenery (WScenery 0x4895E0) x, y (kept on the map); tree circles 0xCF-0xD4 (0x48F3B0) x, y and a seed; fences and
walls 445-447 (0x48D0A0) six dwords. `wowdump --scenery <nsb> <OBJ.ojd>` lists a file's types, GTypes and parts;
all 30 read to their ends.

Placing them (`PlaceScenery`), with the game's `rand()`:

- **Burnt trees** (WScenery's constructor 0x489380): with more than 3 enemy units and buildings in the sector
  (dword_4F7430, this battle's count), each scenery object made takes a `rand()`, and a tree (0xD5-0xE5) under
  (0x8000 / 7) x (count - 3) becomes its red-weed kind (0x399-0x3A9).
- **Tree circles** (0x48F410, when they start, after everything's loaded): `srand(seed)`, then 8, 4 or 2 trees
  (types 0xCF / 0xD2, 0xD0 / 0xD3, 0xD1 / 0xD4) of the C kinds (0xD5 + `rand() % 7`) or D kinds (0xDC +
  `rand() % 10`); the first at angle `rand() & 0xFFF` (of 4096), each next 4096 / n further round, at distance
  `rand() % radius` (the unit table's +0x44 word), turned into x and y by 0x43FE90.

*Provisional*: the `rand()` calls the other classes' constructors make aren't traced, so the burnt trees placed
on their own may differ from the game's.

Each object's GType (the unit table's +0x28) has a class that picks its renderer (0x47EB10): 0 a sprite, **1
RTypeISO** (an IOB model), 2 a WOF model, 6 an atlas sprite, 14 a sprite at scale 1. The scenery's trees and
buildings are IOB models; the engine draws those. *Not yet*: the sprites (street lamps), the WOF bridges of the
walls, the ambient sound scenery (940, 994-996), and the other child parts.

**Shadows** (the buildings' part 331, types 0x14B-0x14D, 0x48E2A0, a class 14 GType whose `.raw` is a 32 x 32 mask
of 0-63): when one starts (0x48E460) it reads its parent's renderer and lays the mask on the ground's moving light
(0x47C900), which the ground view takes off the lit level, so it darkens. The rectangle: y from the object's y less
its depth x 362 / 512 to y plus it; across, 2 x the half width + 10 x |cos of the sun| x the part's scale (the part
record's, times its type's), from x + half width leftwards when the cos is over 0, else from x - half width
rightwards and the mask mirrored; the corner it starts from moved 8 up and left. Each mask texel m gives
(255 - 4m) x the point light level / 255 (at most about 11 levels at noon).

**Placing an object** (0x47F960 / 0x47F480): its position is kept a collision radius inside the map (for RTypeISO
max(2 x half width, depth) x `[Tweak] "ISO obj col rad mul"` 0.66); +0x80 is the ground's height there (0x47B170)
plus its own; its renderer's point is (x, the iso projection of y with that height), and its depth (+0x1E) its y.

**RTypeISO** (0x49DDA0 / 0x49E1E0): the model's half width (header 0x0E / 2) is taken off x; its depth +0x26 is
header 0x16 / sqrt 2 (x 0x2D4279A3 >> 38); y gets +0x26 / 2 less header 0x12. Its lights are made once (0x49DF40,
brightness 255): for each patch three levels, from one normal for the patch, or (header 0x0A set) one normal for
each corner named by the patch, lit like the ground (0x40CA90 with (-x, -z, -y)) times ftol(255 x the shade
level), through the LANDR `.INT` table, at least 8. Drawn (0x4113F0 / 0x457C60), every patch goes at its offset
through 0x4579A4 like a ground half cell, with the object's shade slot (buildings 1, `BMHB<letter>`; trees 10,
`BMR<d>NA<letter>`; the bm group loads slots 1-4 and 10 by the hour), dither set 13 (DithDat + 0x3400) and its
kind's span routine: kind 1 (0x4574CD) leaves texel 0 out. Up to two point lights nearer than their radius would
add to the lights (0x4111B0); *not yet*.

**With the ground** (0x49F610): the view's objects are kept in depth order; after each row of ground every object
whose depth + its +0x26 is under the next row's world y is drawn, and the rest after the last row.

