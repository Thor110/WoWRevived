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
scenery (WScenery 0x4895E0) x, y (kept on the map); tree circles 0xCF-0xD4 (0x48F3B0) x, y and a seed; bridges
445-447 (0x48D0A0) six dwords: x, y, then the two ends. `wowdump --scenery <nsb> <OBJ.ojd>` lists a file's types, GTypes and parts;
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
buildings are IOB models; the engine draws those, the street lamps and the bridges (below). *Not yet*: the
ambient sound scenery (940, 994-996), and the other child parts (the lamps' 334, below).

**Sprites** (class 0, RType 0x40EC50; in the scenery only the street lamp, type 272, `dat\stlamp.spr`, in sectors
11, 15 and 30 among others): the renderer's +0x28 / +0x2C are the GType's hot spot negated (0x40EAF0) and +0x2A /
+0x2E the sprite's width and height less it; +0x26 is 1. Drawn (0x40EE10) at its point less the hot spot, frame
+0x38 >> 8 (0), through 0x40B500 with the GType's palette slot (+0x18, the battle map's table for the hour) at
level 0. Its shadow part takes the half width from +0x28..+0x2A (half the sprite's width) and depth 1. The lamp
(0x48F290) is WScenery with its own vtable; its other part, 334, is its light (below).

**Lights** (types 334-346, 0x48DD50, each with an OBJ.ojd record 13 entry, 0x4EC598: a float and three words; the
lamp's 334 is 0.4, 0, 0, 0; those with words set fade in and out, 0x48E100, *not yet*):

- **Start** (0x48DE80): for a parent of category 0 to 3, `rand() % 100` (taken as the objects start, in the list's
  order with the tree circles) must be at most 100 x flt_4DA768, the hour's fourth float (0x40B030: 1 at night,
  0.8 to 0 round dawn and dusk, 0 by day, so one lamp in a hundred is lit even at noon), and the parent not
  Martian (+0x0C 1); then `[Options] "Show lights"` (byte_4B8808, default 1) must be on, else the light goes. One
  that stays is raised 16 (+0xA0 += 16, so 32 over the ground for the lamp's part at 16).
- **Renderer** (RTypeLight 0x4A27A0, 0x4A2800 / 0x4A2840): its range is the type's +0x44 word (50 for 334), its
  intensity the record's float; its +0x26 is -32 - range, so it comes before the rows it covers.
- **Draw** (0x4A2980 -> 0x4A0140): when the view reaches it, every vertex in the window within range/cell
  vertices of it gets `ftol(intensity x 256 x PointLight)` added to its shown level (+2), at most 255.
  PointLight (0x40C930): from the vertex to the light (dx, dy, dz: the light's height less the vertex's) at
  distance d, with the ground's rise to the next vertex across (hx) and down (hy) over a cell, it's 0 when d is
  out of range or the facing (hx, hy, cell)/|..| . (dx, dy, dz)/d is 0 or less, else (1 - d / range) x
  sqrt(facing). The lamp's light covers a vertex or so round it.

**The spot under each object** (WScenery's start 0x4896D0, after its parts): an object whose type is category 0,
2 or 3 (+0x10; 0x47D828 sets +0xB8 bit 1, and the scenery is all category 0) puts the sun's point level on the
vertex it stands on (SetPointLight 0x47C830), which the ground view takes off: a small dark spot. The tree circles
(0x48F410) do it at their centres. It's set after the shadows, so it wins on that vertex. *Provisional*: done for
the drawn objects and the circles; the ambient sound objects' classes aren't checked.

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

## 3D models (WOF; RTypeWOF 0x49DA50, WOFLib 0x49E330-0x49F230; `core/WofView`)

`wowdump --wof <OBJ.ojd> <gtype> <dat folder> <map.CLS> <map.ATM> <out.bmp> [heading]` poses a model at a map's
middle at noon and draws it.

**Set-up** (0x49DCA0 / 0x49E330): the scales start at 1 and are multiplied by the GType's OBJ.ojd record 6 float
(an ironclad's is 750, so a point is (p x 750) >> 16); the record's first word (+0x60) says whether the model tilts
to the ground. From the file: piece count (header 0), the root piece (0x10), the vertex section (0x0C), the
material table (0x24, a u32 each: the texture offset, its top bit marking a masked material), the texture (0x28)
and the smooth-shading groups (0x2C, a u32 offset for each piece). A piece record (0x61 bytes from 0x30): flags
(+0x10: low two bits the damage variant, 8 drawn first, 0x10 last, 0x20 smooth), vertex and face counts, its pivot
(+0x13), vertex offset (+0x19), face offset (+0x1D), up to 16 children (+0x21). A piece whose variant is neither 0
nor the current one (+0x5E, 1 at first) is hidden.

**Posing** (0x49E690), with 4.12 fixed-point 3 x 3 matrices (column major, 0x4A2FA0 applies one, 0x4A2C50 joins
two, 0x4A2E00 builds one from three angles of 4096 a turn): the camera's turn from angles (-110, 0x400 - heading,
0x800); the root's pivot (x, -y, z) through it is the translation; then 0x4A3020's turn is put after it: from the
model's axes (y up) to the world's (z up), swapping y and z, or for a ground-following GType a frame built from the
ground's normal there (0x47B240). The root's pivot's y and z, scaled, are taken off the anchor's y and z. Each
piece (0x49E900) takes every point through the matrix plus the translation, scales it ((v x scale) >> 16) and adds
the anchor: its world point; its screen point is (x, the iso projection of y with z as height, y as depth); the
nearest depth is the piece's. Each child adds its pivot (as it is, not negated) through the matrix to the
translation, and the walk goes on down.

**Drawing** (0x49EC00): the pieces in depth order (qsort), flag 8 first, 0x10 last. For each piece (0x49EEF0) each
face's light (0x49ED90) comes from the normal of its three world points (cross product), lit as the ground
(0x40CA90) times the brightness (1), the day set's third float and 256, at most 255 (point lights would add here,
times `[Game] "WOF Brightness"`); a smooth piece averages its faces into each group's points, the others give
each face's light to its first point (the table, byte_59F768, is shared and kept between pieces, as in the game).
A face is drawn when its screen winding is negative, with its material (255 none), its corners in the order
first, third, second, and flat when its three lights agree or the piece isn't smooth.

*Provisional*: the triangle filler. The game's (0x456DD0, 48 span routines: textured with gouraud through
`shade[level x 256 + texel]`, the level the light's top six bits) isn't traced; the engine fills an affine
textured triangle shaded the same way, refusing ones taller than 250 rows as the game does. The rotation is
computed in doubles; the game keeps a few of its values as floats.

**With the ground**: a WOF object's +0x26 (0x49DB1C) is twice the second word of its GType's record 6 entry
(0x4F0532: the record is read as two floats then two words, kept as word, word, float, float), so it is drawn
after the row past its y plus that.

## Bridges (WBridge 0x497890-0x498750 on 0x4862C0-0x4868A0; `core/Bridge`)

Scenery types 445 (Country), 446 (City) and 447 (Railway) are bridges (0x48D050, 0x48D510 and their neighbour; the
three differ only in their part: 461 bridge_1.WOF, 462 bridge_2.WOF, 448 bridge_4.WOF). The sector files have one
to four each in 21 of the 30 sectors.

- **Load** (0x48D0A0): x, y (the position), then end A's x, y and end B's x, y. An end at or past the map's width
  or the rows' extent becomes one less than it; one under 0 becomes 0; if any end moved, the position is (0, 0).
  The ends go to the object as 0x80A5 / 0x80A6 (x and y as words), the position through vtbl+0x40.
- **Start** (0x486490): each end is moved onto the nearest vertex (((v << 16) + half a cell) / cell x cell >> 16);
  0x43FDA0 gives the heading and length from A to B: asin(dy / length) as 4096 a turn, 2048 less it when dx is
  negative, kept to -2048..2047. Then (0x497900) the segment count is `ftol(length x 1/64 - -3.0)` (the constant
  is -3.0 and it's subtracted, so three more, not fewer); the bridge's one part is the first segment and count - 1
  more of its type are made; segment i stands at A + (i + 1/2) steps (a step being (B - A) / count), kept as
  floats relative to the position.
- **Joints** (0x48D2F0, vtbl+0x10C, every update from 0x498110): joint i lies at i x (length / count) along the
  line (the distance added up as a float). Its point P on the line, and the ground's height there; the deck is the
  straight line between the ends' ground heights or that ground, whichever is higher. Six points, 40 either side
  of P across the line: the ground, the deck (deck + 8) and the rail's top (deck + 16); at the two end joints (0 and
  count) the deck is the ground and the rail's top 4 above it. Each is projected (0x498300 / 0x49E630).
- **Stitching**: every segment model has nine pieces of four points (the deck, its two sides from the ground up,
  the rails), and all 36 are set from the joints: joint i is segment i's start (0x497E00) and segment i - 1's end
  (0x497AC0), world and screen points alike; the segment's bounds widen to them. So the segments' own geometry
  only gives the faces and textures, and the posed points only give the pieces' depth order.

The engine builds the joints once, as the battle map loads; the game redoes them every update (they only change
when the ground does). *Provisional*: the segments' place among the view's objects (each by its own y plus the
GType's +0x26), not checked against the game.

## Deployment (0x421FE0 / 0x4222C0; `core/Deployment`, `BattleMap::Deploy`)

EnterBattleMap (0x439770) gives each war map object of the sector its battle units (0x4216E0: the object's +0x4C
list, also put in the battle world's list), then deploys the six lists in the order HCON, HRES, ACON, ARES, HUNI,
AUNI (sector +0x54, +0x5C, +0x58, +0x60, +0x4C, +0x50). The save keeps each object's battle units after its BMOL:
each one's type, SELO and id, two dwords, then BAMO with five values, its place (+0x78 / +0x7C) being the second
and third, and BATP and its class's own tags (VEHI / VEHU for a vehicle). A fresh unit's place is 0, 0.

**The centre** (0x421FE0), for each object in the list:
- the command building's place, if one stands among the battle world's objects: the last found of type 830 in
  sector 3 or 120 elsewhere (Martian), or 278 in sector 30 or 314 elsewhere (Human), either side's;
- a Martian pod's place (type 179 in the scenery list, made when a pod landed among the Humans, sector +0x64);
- if the list's faction (its first object's +0x0C) isn't the sector's owner (0x43CA30): the pod for Martians,
  else the entry point from the sector the object came from (+0x58); if it is: the command building, else the
  pod for Martians, else the entry point if it came from somewhere, else the map's middle.
- the entry point (0x436050): a hard-coded pair of points for each pair of neighbouring sectors (a switch on
  previous x 100 + sector, only for previous's neighbours in 0x4BD260), boats (+0x52 5 or 6) at the second;
  (0, 0) when there's none (some pairs have one only for boats). The 158 pairs are in `core/Deployment.cpp`.
- the player's objects (+0x24 0) leave their centre in dword_4F742C / 4F7434, which the start view uses.

The war map object's +0x58 is the sector a route last brought it from (0x4254A7) or the one an AI move took it out
of (0x4219DC); the engine keeps it on the counter (not saved, so 0 after loading).

**Placing** (0x4222C0): n units, size the first's type's +0x46 word, k = ftol(sqrt(n) + 0.5), the block k x size
either way of its centre. They keep their saved places unless the first stands at (0, 0), or it's a vehicle
(+0xB8 bit 0: category 1) that's the computer's, or the player's within `[Tweak] "No-build border width
(vertices)"` of an edge (or with +0x188 bit 13, a fixed place at +0xF4 / +0xF8; or +0x18C). Then rings round the
centre, 0.7075 x k x size apart (an int), radius r from 0, each with ftol(r / step) + 1 spots from angle 0 at
2pi / ftol(r / step) apart (floats): a spot is good when the area e vertices round it (e = (k x size << 16 + half a
cell) / cell) is passable and each unit's place in the block clear of the battle world's objects (0x47FE10) for a
vehicle, or a building could go there (0x47B610) for anything else. Past the map's width and height the middle is
used. The units then stand in the block row by row from (spot + size - k x size), 2 x size apart, each with a
heading of `rand() & 0xFFF`, and are told to gather at the spot (0x80C3).

**Start view** (0x439B7E): the player's command building in HCON (Human 278 / 314, Martian 830 / 120 by sector),
the middle of its units' extent ((max - min) / 2 + min, 0x42A2A0 / 0x42A340); else 4F742C / 4F7434 unless their
sum is 0; else the map's middle.

The units are drawn by their GType's class like the scenery: a WOF model (2) posed at its place and heading, an IOB
model (1) or a sprite (0).

*Provisional*: the vehicles' ground check (each vertex through the unit's mover, 0x480740) is taken as the
landscape's IsAreaPassable; the object check as circles (the unit size against each unit already placed, at its
IOB collision radius or its own size), without the scenery; +0x188 / +0x18C aren't read; there are no pods yet;
the order of the objects in a list is the counters' order; and where these `rand()` calls fall against the
scenery's isn't checked. The units don't move, fight or hide in the fog yet.

## Minimap ("BattleMap miniMap view" 0x4868B0, RTypeMiniBattleMap 0x4A1F90, dots 0x4A1CD0)

EnterBattleMap makes it with the sector (+0x8C). Its parts:
- **The picture**: `maps\hmin<sector>.spr` (`mmin` in a Martian game, 0x404BA0), the same file the Briefing shows;
  drawn (0x4A2100) through the bm group's slot 21, `{S}MINIB` (0x40C436), at level 0. The window is the picture's
  size at the bottom right of the view, `[Screen] "Scroll Border"` (4) in from each edge.
- **The view's rectangle** (0x49C740 / 0x49C7C0, an outline, its last row and column inside): the picture's width x
  640 / the world's width (landscape +0x1C) by its height x 480 / the world's height (+0x20). On every scroll
  (0x8004, the view's top left) it moves to the scroll's share of the picture, at least 0 and kept inside it.
- **The dots** (0x4A1CD0: up to 1024 of x, y as bytes and a colour; each drawn 2 x 2, 0x4A1DD0): made again every
  16 frames (+0x88) by asking every battle object (0x47D480 over the world's +0x118) for its dot (0x8077: x x the
  picture's width / the world's width, y x its height / the rows' extent), then those of type 987's list
  (0x487380, colour dword_4DAFB4).
- **Colours** (system colours 0x40B120, each a 6-bit RGB made by 0x40B3B0: the nearest palette entry in 8-bit,
  565 or 555 packed twice in 16-bit): the rectangle dword_4DAF9C 47, 0, 0 (Human) or 12, 42, 4 (Martian). A dot
  (0x480070, every object's): dword_4DAFA0 for a category 1 object (a vehicle) else dword_4DAFA4, and dword_4DAFA8
  for the other side's (+0x24 not 0): Human 0, 63, 0 / 32, 63, 0 / 0, 0, 63; Martian 63, 0, 0 / 63, 32, 0 /
  0, 63, 0. Types 694 / 695 (0x48C870) use dword_4DAFAC / 4DAFB0 (Human 0, 48, 0 / 0, 0, 48) and show none while
  their vtbl+0x9C says so.
- **Mouse** (0x486E80): the left button down on it scrolls the view to the point's share of the world less half
  the view (0x7A) and keeps the mouse (0x42E9B0); moving with the button held does the same.
- **Tab** (0x80C8): hides it, or makes the dots again and shows it.

*Not yet*: the type 987 and 694 / 695 dots, and the fog (every unit shows).

## Selecting (0x484FB0 / 0x485930 / 0x485D00, the drag rectangle 0x49C080, hotspots 0x496D60-0x497890)

The selection is by group: a war map object's battle units are one group (WBMGroup), and every way of selecting
sends 0x8088 to an object's group (+0x48), which selects all its units.

- **The drag rectangle** (0x49C080, vtable 0x4B1070): the left button down in cursor modes 0 and 1 (0x201) starts
  it at the mouse (0x49C230) and the view keeps the mouse. Moving with the button held (0x485D00 -> 0x49C250) moves
  its far corner; once that's more than 4 pixels from the start either way (+0x18 / +0x1C) it's a box (+0x17),
  with four GType 6 (`ho-selec.spr`) corner sprites, frames 0-3. While it's a box every move asks the objects which
  are inside (0x8010 -> 0x48A4C0, counting them in world +0x114).
- **Drawn** (0x49C3E0), normalised: where a side is 23 or longer, two lines 12 short of each corner: dword_4DAF9C
  on the edge, dword_4DAFB8 one pixel in (Human 40, 0, 0; Martian 5, 39, 4); the corners at 7 in.
- **Let go** (0x485930): a box (0x49C350) with at least one object inside clears the selection (packet 4) and sends
  0x8011 with the box: each object (0x48A530) that is the player's (+0x24 0), whose screen point (+0x2C, +0x30) is
  in the box (PtInRect) and that is a vehicle (+0xB8 bit 0) or has +0x3C set, selects its group. Otherwise the
  click goes by cursor mode (1 orders, 3 building; nothing yet).
- **Hotspots** (0x496D60, made by 0x48A660 for objects whose vtbl+0xF0 says so): a square window 2 x the type's
  +0x40 byte wide over the object. Let go on it in cursor mode 0 (0x497770 -> 0x497680): nothing if it's selected
  already or not the player's; with Ctrl only a vehicle; else the selection is cleared and its group selected.
  (With Ctrl in mode 1 it's force fire at the ground under it, packet 0x13.)
- **Shown** (0x8012 -> 0x4972F0 / 0x497000): a selected object of the player's gets GType 5 (`hselec.spr`)
  frames 0-3 at its hotspot's corners 8 in, and GType 7 (`hpower.spr`) at the middle of its bottom edge 8 up, its
  frame ftol(frame count x (1 - damage / maximum)), at most the last (0x497290 keeps it up to date).

- **Cursor mode** (dword_4EA43C, set by 0x42E790): every change of the selection (0x800F to the world and the
  view, 0x43B682 / 0x4853A0) makes it 1 (orders) when something is selected, else 0. In mode 1 a hotspot of the
  player's selects its group, one of the other side's is attacked (packet 0x10, "Attacking an object" below), and
  with Ctrl either is force fire at the ground under the mouse (packet 0x13).
- **Cursors** (WM_SETCURSOR, numbers into the cursor table: 1 `hselcurs`, 4 `h-move`, 5 `h-attack`): over the
  minimap 0; over a hotspot (0x497110) in mode 0 1 for the player's, in mode 1 5 for the other side's or with Ctrl,
  else 1; elsewhere (0x485F60) mode 0 4 with something selected else 0, mode 1 5 with Ctrl else 4.
- **Cancel**: the right button let go before the popup comes (0x205) drops the drag rectangle and sends 0x73,
  which clears the selection, as the popup's Cancel does. The popup only offers Return to Warmap with nothing
  selected and one side without units on the map.
- **Home** (0x43A480): the view centred on the selection: the middle of its members' extent (0x4836A0, halves
  rounded to 0), projected with the nearest vertex's height (0x47B120), less half the view.

*Provisional*: the hotspot is taken as centred on the object's screen point (the ground under it); +0x3C isn't read
(only vehicles count for the box); the frontmost hotspot is the one lowest on screen; no damage yet, so the power
bar is full. The orders themselves (move, attack, force fire), Shift adding to the selection (packet 5) and the
group keys aren't in.

## Orders (the popup's selection items 0x47E1E0, special orders 0x47DA80, stances 0x4817C0 / 0x80B2)

The popup (0x485DE0) asks whoever answers 0x805C after its own Cancel and Return to Warmap: the selection, a WBMGroup
("BattleMapSelection"), whose 0x4482D0 asks its first member (unless the members are in different sectors, when
0x448500 builds a combined panel); the member's 0x47DF20 calls **0x47E1E0**, which adds (0x432E90: command, param,
icon GType, tool tip text):
- Build (0x813E 0x8065, 0x20, 0x470) and Suspend / Resume (0x813F, 0x22 / 0x23, 0x495 / 0x496) for a builder, only
  while its sector is quiet (0x43D010), so never on a battle's map;
- the stances (0x80CE): Roam and Attack (3, 0x34F, 0x474) when armed and moving, Attack (2, 0x34E, 0x473) and
  Retaliate (1, 0x34D, 0x472) when armed, Avoid (0, 0x34C, 0x471) when moving, then Retreat (0x80B7, 0x2D, 0x479)
  when moving, a battle is on, the sector isn't quiet and there are reinforcement points (dword_4F76E8);
- Abandon (0x8067, 0x34B, 0x46F);
- Force Fire (0x80F3 0x80ED, 0x1E, 0x48A) when armed;
- each special order 0x47DA80 allows, in order: Submerge 0x80EF (0x349, 0x48C), Tunnel 0x80EE (0x34A, 0x48B), Black
  Dust 0x80D5-D7 (0x25, 0x482), Shock Bomb D8-DA (0x26, 0x483), Explosives DB-DD (0x351, 0x484), Infiltrate DE-E0
  (0x352, 0x485), Pulse E1-E3 (0x353, 0x486), Freeze E4-E6 (0x354, 0x487), Scare E7-E9 (0x355, 0x488), Control EA-EC
  (0x356, 0x489), Scan 0x80F0 (0x358, 0x48D).

*Moving* is the type's +0x52 word (the move class) not 0; *armed* is +0x3C, the children with +0xB8 bit 11 counted
as the object starts (0x47EF30): the turrets (0x489820: types 0x2E6-0x313, 0x389-0x38C) and the shot classes on
0x4949A0 (0x488930: 0x197-0x199, 0x2BF-0x2C4, 0x3AA, 0x3EB-0x3ED; 0x4887E0: 0x2B9-0x2BB, 0x320-0x322, 0x3B5-0x3B7;
0x488460: 0x3AB; 0x48DBE0: 0x19A-0x19C). The Armoured Lorry's turret 0x2E6 makes it armed.

**0x47DA80** by type, some needing a research item done (0x45B890: state exactly 2; the items are the side's tree's
in order, the globals at 0x588C40 Human and 0x588F20 Martian, four bytes each): Submerge 0x16-0x18; Tunnel 7-9;
Black Dust 0x42/0x43/0x47/0x44/0x45, 0x48/0x46, 0x49; Shock Bomb the same with Martian item 32 (minus the first
three types); Explosives 0x20, 0x11/0x21, 0x12/0x22; Infiltrate 0x11/0x12/0x20/0x56/0x50, 0x21/0x57/0x51,
0x22/0x58/0x52; Pulse 0x4A (M37) or 0x50 (M46), 0x4B (M37) or 0x51 (M46), 0x4C (M37); Freeze 0x50-0x52 (M47), 0x56
(M54) or 0xB/0xC (Human 39), 0x57 (M54), 0x58 (M54); Scare 0x56-0x58 (M55); Control 0x56-0x58 (M56); Scan 0x50-0x52
(M44 or M45) or the battle's command building (0x33E where its kind is 3, else 0x78) with M2 or M3 (triggers).

**Stances** (packet 0x15 -> 0x4817C0 -> 0x80B2): each member gets the stance (+0xD8) and the group keeps it at +0x54
(1 as a group is made; 0x4802E0 averages the members'); Roam and Attack also takes the group's centre as its home
(0x483AE0). Abandon (0x8067) sends each member 0x80A0, its end; Retreat runs 0x480EF0; Force Fire and the specials
set the group's mode (+0x1DC) for the next click.

**The digits** build packet 0x12 (select group n, 0 as 10) but the selection's 0x4812B0 has no case for it: they do
nothing in the game.

*Provisional*: the stances and Abandon act (the stances are kept per group, for the AI and weapons to come); Force
Fire and the specials are listed but not acted on; Retreat is listed with reinforcement points (below).

## Damage and death (0x48AB20, 0x48A5A0, 0x80A0 in 0x489FA0; WExplosion 0x488180 / 0x488350 / 0x488200; `battlemap/Combat.cpp`)

**Health** (+0xC4) starts at the maximum (0x47DA20): the type table's +0x4A, which is record 34 times the armour
modifier for its faction and the side played, then record 35's (`[Tweak]` "Scenery / HumanFriend / HumanEnemy /
MartianFriend / MartianEnemy Armour Modifier", 1.2 and 1.8; the survey's "armour"). The Armoured Lorry has 161, the
Parliament 17,999. Every object on 0x489E20 (vehicles, buildings, scenery) can be hit (+0xB8 bit 7; the turrets clear
it).

**Taking damage** (vt+0xE0, 0x48AB20): +0xC0 bit 3 (hit this update), the health less the amount, and at 0 or under
0x80A0 to itself 10 + rand() % 20 updates on (0x4355A0); the status bar is refreshed and, the first time it's under
40 %, the object goes on the world's damaged list.

**Dying** (0x80A0, 0x489FA0): it posts itself 0x8057 (gone) and makes its type's destruction effect (+0x98) with
0x488070 at its point, ftol(+0x84 + its +0x41 byte x 1.414) up, the firer its parent (none for a unit). The battle's key
building going (the Martian Base 0x33E where the kind is 3, else the Communications Centre 0x78; Parliament 0x116
where it's 0x1E, else 0x13A) speaks 0x112 if it was the player's, else 0x13B. Abandon (0x8067) sends 0x80A0 at once.

**Explosions** are the types the factory makes as WExplosion (0x488180, and Death_Explosion 6-8's 0x48CD80 on it):
0x221-0x243, 0x247-0x255 and its switch's 715-717, 946-948, 986, 997-999, 1006-1008. +0xC8 is the type's +0x44 (the
radius; the survey had it as the damage) and +0xCA its +0x4A (the power, armour modifiers and all). Added (0x488350):
its sound (+0xA2) at its point; its parts started, each a child at the part's offset (an explosion part going off
first: Death_Explosion_2 (591, the Armoured Lorry's: radius 40, power 24, sound 205) has part 997, an explosion with
sprite GType 191, radius 0 and power 100); its radius (+0xAC) set; then **Detonate** (0x488200): everything within
reach (0x47FC50: the octagonal distance from whole positions at most its radius + the other's +0xAC; kind 4 and itself
left out), except the firer and, unless vt+0xF8 says otherwise, the firer's side, gets vt+0xC0 with 1.0 (0x48A5A0:
health over 0 and +0xB8 bit 7, then ftol((1 - distance / (both radii)) x the power x 1.0) to vt+0xE0); and under 64
up, not on water (0x47FFE0: terrain 1-3), 0x804B to the landscape scorches the ground (0x47C4C0 with the radius and the
power). Then it plays as a WAnimEffect, its sprite if it has one.

In sector 30, Abandon on the lorry group: each lorry went up (591 and 997), the others nearby losing 5 to 11 of 161 and
the Parliament 4.

**The scenery is damaged too.** WScenery's constructor (0x489380) is on the damageable object 0x489E20, which sets
+0xC4 to the type's health (0x47DA20) and +0xB8 bit 7 (the hit's 0x48A5A0 needs it); the base 0x47D6F0 clears it,
and the units' 0x495668 and the bridges' 0x48D800 set it too. Its vtable (0x4AE5B0) has the same hit (vt+0xC0),
damage (vt+0xE0, 0x48AB20), death (0x80A0 in 0x489FA0: gone, its destruction effect, the key buildings' speech) and
update (+0x10 = 0x48AEF0, the damage smoke: not a vehicle, so every level up to its own). Gone (0x8057), any
object of category 0, 2 or 3 but the trees frees its footprint (vt+0xE8 = 0x48A9A0: 0x47C3A0 with 2), as it marked
it added (0x48A950). Here the scenery is a list of its own (`props`, each with its view object and light): every
explosion's detonation goes over it after the units with the same reach (its radius the type's +0x44 word) and
hit; of the area blasts only 818-820's hit reaches it (never under the ground or Martian; the statuses' messages
aren't a WScenery's). Rockets force-fired into the trees west of the start hit them 3 to 9 a time (60 health each)
and four went in 290 updates.

*Provisional*: an object goes at once rather than on the next update; the scenery is hit after the units (the game
takes both from one list, 0x47FC50, by its buckets: the `rand()` order of their deaths may differ); the damaged list
isn't in; the kind is taken as the sector; the status bars aren't in. (The light parts and emitters are in: "Lights",
"Flying objects and emitters".)

## Weapons (WProjectile base 0x4949A0 and its classes, 0x494DB0 / 0x494FD0, 0x4886A0 / 0x488830 -> 0x487F50; the group's target 0x4826D0; `battlemap/Weapons.cpp`)

**The guns** are a unit's weapon children (+0xB8 bit 11), its own parts or a turret's (0x489820: a vehicle of its own,
types 0x2E6-0x313 and 0x389-0x38C). The factory (0x446B30, its switch on 0x314 on decoded from the byte table at
0x447C94) makes them as: WShell (0x488410: 700-702, 803-811), WProjectile (0x488460: 0x1AB-0x1B0, 0x1B5-0x1B7, 939)
and 0x48DBE0 (0x19A-0x19C), all three with the shell's update (0x4886A0); WRocket (0x4887E0: 0x19D-0x1A2, 0x1B8-0x1BB,
697-699, 800-802, 949-951, update 0x488830); WMortar (0x488930: 0x197-0x199, 0x1A3-0x1AA, 0x1B1-0x1B4, 0x1C9-0x1CB,
703-708, 938, 1003-1005), which lobs shells (update 0x488CA0: the cycle, then 0x4889D0 as it goes to 1; see "Mortars"). (449 and 450 are no
weapons.) The Armoured Lorry's turret 742 holds a WRocket, 414.

**OBJ.ojd record 12** (0x4F2F10, 16 bytes a type): +0 the range, +2 the wind-up and +4 the reload (each as ftol(the word
x 30 / 20) updates, dword_4B84C8 = 30), +6 the impact size. 414: range 240, wind-up 1, reload 3, size 2.

**The fire cycle** (0x494DB0, each update; +0xD8 the state, +0xD4 the count): 0 counts down, then, armed (+0xD0) and
aimed (+0xF0 bit 0), goes to 1 if its owner lets it (vt+0x84, 0x496920: an ordinary gun needs the group's mode +0x1DC
0; vt+0x108 not frozen), else drops aimed; 1: the count ftol(+2 x 30 / 20) + rand() % 15, its sound (+0xA2) at its
point, then 2; 2 counts down to 3; 3: its owner told (vt+0xD0, 0x496BC0: the rescan reset, a mode's target cleared),
then 4; 4: the count ftol(+4 x 30 / 20) + rand() % 15, then 0. Then **aimed** (0x494FD0): the point under its range x
+0xC8 (1.0; research raises it, 0x494AF0) from it, and inside its hardpoint's arc (+0xB0: +4 the middle, +6 the half
width) if it has one. The **landing**: the shell's update once in state 2 when the count is a multiple of 5 (+0xF0 bit
1 marks it), the rocket's in state 3: the spread s = ftol(the octagonal distance from the gun / 32 / +0xCC (1.0) + 1),
x, y and height each moved by rand() % 2s - s, the height less the ground at the vertex there (0x47B120); then
0x487F50 with +6 as the size: on water (terrain 1-3) 0x4D0340[size - 1], else 0x4D0230[size - 1], over 64 0x4D0450;
the effect made there with the firer (the gun's root object) and the gun's type. The effects for sizes up to 40 are
all WExplosions (381-394, 484-589), 41-43 too (715-717), 44-49 and 50-61 area blasts (718-720, 788-790, 812-823), the
rest WExplosions. An explosion spares the firer's group (0x47EB00) unless the other says otherwise (vt+0xF8).

**Aiming** (vt+0xC8, 0x491110): not under the ground (+0x84 < 0) or on a bridge link, the point kept on the map, then
0x496200: the goal kept and its children all aimed at one point jittered by up to the distance / 16 + 1 each way
(drawn once: rand() % twice that, less it, for x, y and height); a turret passes its own jittered point on the same way
to its guns, whose vt+0xC8 (0x494D20) keeps it and arms them (one in flight, state 2, goes to 3). 0x805A (0x494D80)
disarms a gun, sends a cycle under way to its reload and sets aimed.

**The group's target** (+0x2C; -1 a point, +0x40..+0x48): each update (0x483B70 -> 0x4826D0), a target that's moved more
than 80 from where it was last aimed (+0x34..+0x3C, -1 at first) is aimed at again: the group moves there (0x804F, the
route's radius its engagement range +0x4E in vertices: the least, from 100000, of each member's sight +0x48 and its
longest gun range, 10000 for none; 240 for the lorries) and each member gets vt+0xC8 (a point) or vt+0xC4 (an object,
Approach 0x496300). **Force fire**: Ctrl with the left button on the ground (packet 0x13), or the popup's Force Fire
(0x80ED: mode 1) then a click: speech 0x10C and the target the point at its ground height. A move order clears the
target and disarms the guns.

**Mortars** (0x4889D0): a mortar's launch list (+0xB4) is its type's emitters (+0x3C, record 29, copied by 0x47EF30 into
12-byte entries: the emitter, a count, 0): each a shell's type (+4), the spread (+8), a limit (+0xA, 0 for none) and
the gravity (+0x14). As the cycle goes to 1: h = ftol(its height less the ground's, 0x47B170); the heading and
length to the aim (0x43FDA0); the reach R = the length less (its height less the aim's), at least 0.1. Each entry
makes one shell: s = ftol(spread / +0xCC (1.0)); heading rand() % 2s + heading - s, elevation rand() % 2s - s + 0x200
(45 degrees), as radians x 0.0015339808; the speed v = sqrt(-(R x gravity) / (2 sin a cos a)), split by 0x43FE90 into
across and up; the updates ftol(length / across - -1.0). The shell (0x446B30) gets the across part along the heading
and the up part as its velocity (vt+0x80, +0x88..+0x90), the mortar's point (vt+0x40), h (+0x84), the gravity
(+0x94), record 12's +6 (+0xC0), the updates (vt+0xE0: +0xC2) and the root's id (+0xC4); byte_51A19C of the mortar's
type is set. The entry's count goes up; past a limit (not 0) the list is freed and the mortar fires no more. The
mortars' entries are all spread 32, no limit, gravity -0.3: shell 396 for most, 915-917 for 407-409, 1002 for
1003-1005.

**The shell** (0x488E80 on 0x487730; the factory's 0x18B-0x196, 713, 714 and 1002), each update (0x488F00): first
0x487890 (its emitters, 0x47F050, its step vt+0x54 = 0x47F770: the point moved by the velocity, the height kept
level across the ground's change (ftol(+0x80 - +0x84) less the ground at the new point, plus the height and the
upward speed; with +0xB8 bit 10 just the height plus the speed), then the upward speed plus the gravity; under 0.0
high it's gone, 0x8057). Then at or under 0.0 high, or with +0xC2 counted out (decremented only while above the
ground), it lands: 0x487F50 with +0xC0, its point, ftol(the height - -8.0), the firer and its type; with +0xC2 below
0 it's gone too. With a mortar (706, range 600) given to the five lorries for the test, force fire at a point about
300 away launched five shells at 8.7-10.6 a step that came down within about 30 of their aims, some on the ground
and some in the air as their counts ran out. Shell 396's sprite (GType 259) is 5 x 4.

**Attacking an object** (a click on the other side's hotspot in mode 1, packet 0x10 -> 0x4812B0 -> 0x481F70), for each
selected group: type 0x3DB is a case of its own (0x481170, else speech 0x121); any other: each member 0x80B9, then
0x482880 (the target +0x2C, its id +0x30, the last aim -1), +0x1D8 bit 0 set (0x480EB0), vt+0x38, and speech 0x10C
for a player's group (bit 4) whose first member is a vehicle; then bit 3. Each update the group's target follows the
object (+0x78..+0x80, 0x4826D0); one under the ground (+0x84 below 0.0) has its guns stood down (0x805A) and bit 0
cleared. **Approach** (vt+0xC4, 0x496300): d = ftol(the distance - -1.0) and r the object's radius (+0xAC) / 3 for a
category 0, 2 or 3 object (+0xB8 bit 1), else 0; the aim is the object's point less (it - the unit) x r / d, its
height plus its +0x41 byte x 1.414 (dbl_4B0AA8 is -1.414, subtracted): the near side of a building, a vehicle's middle.

**Looking round** (the unit's update 0x4967E0): +0xDA counts the updates; past its type's +0x92 word (record 53) it
calls vt+0x104 (0x48F8A0: with +0xE4 bit 1 and further than its sight x flt_59E44C from its post, the group's guns are
stood down; then 0x496890: on stance 2 or 3 with its group not attacking (+0x1D8 bit 0 clear or bit 2 set) it picks a
target, 0x496570), and +0xDA goes back to 0. **0x496570**: within its sight (+0x48) x flt_59E44C (octagonal, whole
positions), not under the ground itself, of the other side's objects that can be targeted (vt+0x6C, +0xC0 bit 0) the
first armed one (+0x3C) is taken at once; else the battle's key building (0x33E / 0x78, 0x116 / 0x13A) if seen, else
the last seen. The group gets 0x80B1 (0x4807F0's case: a player's group and a vehicle target, rand() under 5000 says
0x10F; then 0x482960, as 0x482880 with bit 0 set). (0x482570, every 8th update of a group with +0x1D8 bit 7, tries
one of 27 special orders at random through CanDo and 0x4817C0: the computer's, for the battle AI.)

**flt_59E44C is never written**: it sits in uninitialised data and nothing in the listing stores to it (two reads,
0x48F8CC and 0x4965A3, no other reference), so it stays 0.0, the sight is 0 and the scan finds nothing: units don't
pick their own targets. It's kept as `kLookScale` in `Weapons.cpp`. *Provisional*: if something unseen sets it, the
scan works (checked with 1.0: the lorries on Attack took the tower as their target unasked).

In sector 30, Force Fire from the popup on a point west of the start: the group drove into range (a 4-node route) and
the five lorries' rockets landed round their jittered aims, a few units out, each an effect 382 with a scorch. With
the tower (278) made the other side's for the test, a click on it set it as the target: the lorries' rockets landed on
its east side, the side they were on, a few units in from its middle, and took its 18000 health down 16-22 a hit.

*Provisional*: the shells' frame (0 is drawn), the mortar's place on its
unit (the shells start at the unit's point), type 0x3DB's case, 0x80B9 and vt+0x38, the posts (+0xE4), the battle
told of an unarmed object seen, the guns' place on their unit, the research
multipliers and the "CAN'T GET TO WITHIN ATTACK DISTANCE!" check aren't in; a target that's gone clears the group's
target (the game's tidy-up of 0x8057 isn't traced); WProjectile and 0x48DBE0 fly no visible missile.

**Turrets** (0x489820 on the vehicle 0x490820: 0x2E6-0x313 and 0x389-0x38C; update 0x489B40). Each part may have a
**hardpoint** (+0xB0, made by 0x47EE10 from the part's record 27 box when box[0] is 0 or more: four words, +4 the
middle, +6 the half width). The Armoured Lorry's turret 742 has middle 0, half 4096 (all round); its gun 414 has
middle 0, half 256 (a sixteenth of a turn each way). Each update a turret copies its unit's status (vt+0x8C ->
vt+0x88, vt+0x94 -> vt+0x90), then wants the heading to its point (+0xC8 / +0xCC) if it's aiming (+0xE4 bit 1, set by
its aim, 0x491110 / 0x496200, cleared with the group's target), else its hardpoint's middle plus its unit's heading.
d = 0x43FF30(wanted, its heading) (its heading less the wanted): under -50 it turns +50, over 50 -50, else not.
Aiming, the turn is taken only while |0x43FF30(+0x190 + the step, the middle)| stays under the half width; not
aiming, always. +0x190 is its turn from its unit; its heading (vt+0x30, 0x489D00) is its unit's plus that. Turning
with no sound yet (+0xE8), a type +0xA2 that isn't 0 and vt+0x9C clear, its sound starts (0x4883C0 / 0x4883E0);
stopped, a sound playing stops (vt+0x13C). Then the vehicle's own update (0x4913B0). A gun's **arc** (0x494FD0, with
a hardpoint): |0x43FF30(ftol(vt+0x2C + the middle), the heading to its point)| under the half width, vt+0x2C
(0x47D930) being its own heading (+0x40, its parent's) and leaving its argument on the stack as 0x43FF30's second;
aimed is in range and in the arc. The turrets' WOF models (742: WOF 688) are drawn with the unit (each part a child
object with its renderer, 0x47EE10), posed at the unit's point plus the part's offset, turned with the turret.

In sector 30, Force Fire west of the lorries: each turret turned 50 an update toward the point, holding on it as the
lorry itself turned under it, and settled within 50 (d -12 to -37 as the lorry drove); the guns were aimed (in range
and arc) from update 160 and fired, the barrels drawn pointing west.

*Provisional*: a turret's place is its unit's point plus the part's offset (x along the heading); the vehicle update
it runs after isn't; the lorries' other WOF part (324, WOF 739: drawn solid it shows as a pale diamond under each
lorry, a shadow drawn some other way) and the turrets' guns' sprites (GType class 6) aren't drawn.

**Area blasts** (0x487B00 on WExplosion: the factory's 718-720, 788-790 and 812-823; impact sizes 44-61): made as a
WExplosion (sound, parts, radius +0x44, power +0x4A), but their detonation (vt+0xE0 = 0x487B50) replaces WExplosion's
0x488200: no splash for everything and no scorch. Each object within reach (0x47FC50) but the firer's side
(0x47EB00) gets, by the blast's type:

| Types | What |
| --- | --- |
| 718-720 | a hit (vt+0xC0 with 1.0, 0x48A5A0) only if it's under the ground (+0x84 below 0.0) |
| 788-790 | a hit only if it's Martian (0x4406E0 = 1) |
| 812-814 | 0x80F4 with the firer, 1-3 and 120 / 240 / 360 (to the battle's key buildings: taking them over) |
| 815-817 | 0x80F5 at 1-3: frozen |
| 818-820 | 0x80F6 at 1-3 and a hit |
| 821-823 | 0x80F7 at 1-3 |

(The switch also sends 0x80F5 for 0x1F8-0x1FA, 0x21E-0x220 and 0x244-0x246, which the factory makes as WExplosions,
whose own detonation runs instead: those cases are never reached.) **The statuses** (0x495CB0): 0x80F5-0x80F7 set the
timer (vt+0x88: +0xDC) to (level + 2) x 150 updates and the status (vt+0x90: +0xE0, 0x496760); 0x80F6 first stands
its group's guns down (0x805A) and sets the group's stance to 0 (0x80B2); leaving 0x80F6, the group's stance goes
back to the type's +0x43 byte. The unit's update (0x4967E0) counts the timer down and at 0 sets the status 0.
Frozen (vt+0x108 = 0x489860: timer running, status 0x80F5) keeps its guns from firing (0x496920). So the mortars
706-708 (sizes 44-46) blast with 718-720 and hurt only what's under the ground, and 938 (53) freezes (815).

With the tower made the other side's and the lorries given mortar 938 for the test, its shells' 815 blasts
(radius 60, power 84) froze the tower for 450 updates at a time (level 1), the lorries' own group spared.

**What the statuses do.** Frozen (0x80F5; vt+0x108 = 0x489860, the timer running with that status): its guns don't
fire (0x496920) and, a vehicle, it doesn't drive (0x4909D0). 0x80F6: its group's guns stood down and the group's
stance 0 (0x80B2 to the group, each member taking it at +0xD8); leaving it (0x496760), the stance given back is
the type's +0x43 byte (record 56, each unit's own stance from the start, 0x495740). Turned (0x80F7; vt+0xF8 =
0x489880): as it arrives, before its status is set, 0x496100 has it approach (vt+0xC4) each vehicle of its own side
(0x4406E0) within its sight (+0x48, octagonal, under it) where neither is under the ground, the last one's aim
standing; while it lasts it takes no other aim (0x496300 checks vt+0xF8 first) and its own side's explosions hit it
(0x488200 spares the firer's side only where vt+0xF8 is clear); leaving it, the group's target goes (0x482880(0)).
0x80F4 goes to the key buildings (0x495CB0: the Martian base 0x33E or the Communications Centre 0x78, Parliament
0x116 or 0x13A, by the kind), which pass it to the battle (0x43B920: +0x128 the time, dword_4E9B60 the firer's
side) and speak 0x115: control of the sector, for the battle's end.

With lorry 1 turned and lorry 2 frozen at update 40 for the test: lorry 2 stayed where it was through 440 updates
while the others drove off on an order; lorry 1 aimed at its own unit 5 and fired on it six times in 120 updates,
none doing damage, unit 5 being of the firer's own group (spared unless it's turned itself).

*Provisional*: 0x80F4 (the battle's control of the sector) isn't in; frozen holds a vehicle still (its steering
and step not run) rather than as 0x4909D0's callers do.

## The battle's end (the battle world's update 0x43A980, the report 0x43A800; `battlemap/BattleMap.cpp`, `warmap/WarMap.cpp`)

**On the battle map**, each update with the battle on (the report's +0x54: both sides had something there) and no
box up yet (dword_4F7450): each side's count is its objects in the battle (0x47D610 / 0x47D660: the world's list,
those whose side, 0x4406E0, is 0 or 1; the player's taken as its own by 0x404BA0) plus its war map objects waiting
off the map (+0x54 / +0x58, 0x4216D0). The player's at 0: the report is filled (0x43A800) and the InfoBox
(0x432520) says "Sector Lost" (0x49D) if the player held the sector (the report's +0x6C), else "Attack Failed"
(0x49B), its button 0x76 (back to the war map). The other side's at 0: the report filled, and "Invaders Repelled!"
(0x49C) with 0x8024 if the player held it, else "Attack Successful!" (0x49A) with 0x76 if that ends the game (+0x55:
sector 3 taken by the Humans with no Martian base left, dword_4EB05C, or London by the Martians, dword_4EC49C), else
0x8024. 0x8024 only closes the box: the battle map goes on, its popup offering Return to Warmap (one side gone).
(byte_4E9B94, set, sends 0x76 after 150 updates.) **The report** (0x43A800): the sector (+0x50), each side's lists
counted (0x421740 / 0x4216D0 / 0x421790 / 0x421F90 into +0x10..+0x4C), the winner (+0x68) the sector's holder
afterwards (0x43CA40, 0x4EAE1C), the losses (0x4212F0: +0x58..+0x64 the counts before less those after), the game's
end (+0x55); every view told vt+0x78.

**Back on the war map** (0x97), the battle map's losses go to it: each battle group stands for one war map object
(its index in the save's table), and an object whose battle units are all gone is taken out of its counter (a
counter left empty goes from the sector); the side with nothing left in the sector lost, the holder winning if
both or neither have; the Debriefing counts the objects (units and facilities) and the crews lost each side. A
sector the war map sees with one side only is still no battle ("RETURNING TO WAR ROOMS"); without a battle map
(no map files) the war map settles it as before.

In sector 30, with the tower made the other side's and at a hundredth of its health for the test: the lorries
destroyed it, and at update 135 the box said "Invaders Repelled!" (the player holding London), its button closing
it; the popup then had Return to Warmap, and the war map (seeing only the player's objects there) gave
RETURNING TO WAR ROOMS.

*Provisional*: the units are what's counted (not every object with a side); the objects waiting off the map
(reinforcements) aren't in; the report's +0x6C is taken as the side holding the sector as the battle began; the
game's end as sector 3 won by a Human player or 30 by a Martian; the war map's taking of the losses isn't checked
end to end (no save with a contested sector to hand); 0x80F4's control (+0x128, dword_4E9B60, used by the war
map's 0x418980 on the research) isn't in.

## Reinforcement points and Retreat (0x43D150, 0x43D3F0, 0x480EF0; `battlemap/Reinforcements.cpp`)

**The points** (0x43D150, from EnterBattleMap with the battle on or the debug flag byte_4D1279; the table at
dword_4F7458, 0x18 bytes each, dword_4F76E8 of them): for each sector 1-30 with its holder's command building there
finished (0x43C990: a Human holder's Parliament 0x116 in London or 0x13A elsewhere, a Martian's base 0x33E in sector
3 or 0x78 elsewhere, in either construction list, 0x42AA70), for each link kind 7, 5 and 1 the point units from it
come in by (0x436050; none, (0, 0), from a sector that isn't a neighbour). A point nearest an edge is put 100 in
from it. With the player holding that sector (0x4EAE1C against the player's side; or the debug flag) and room
there (0x43AC40, by the move class of type 19 for kind 5, else type 1), the point goes in the table (x, y, the
sector, the kind, held, 0) and a marker of type 0x3DB (a sprite, GType 862; +0xCC the sector, +0xC8 the kind) is
made there. So only the player has points, and only from a neighbour holding a finished command building.

**Retreat** (in the popup, 0x80CE 0x80B7, for a moving type with the battle on, the sector not quiet and points in
the table; 0x4817C0 -> 0x480EF0): the nearest point (0x43D3F0) to the first member's whole position whose kind
suits the type's move class (+0x52: 2-4 and 7 kind 1, 5 and 6 kind 5, 0 and 1 as they are), held, by 0x43FF00;
with none (or +0x1D8 bits 8 and 9 both set) speech 0x121 and the leader told (0x4842D0). Otherwise the members get
0x80B9, the target is cleared and the guns stood down (0x805A), +0x1D8 bit 3 and bit 0 set, the group sent there
(0x804F), and each member's war map object told 0x80BB with the point's sector. A unit there leaves the map; its
war map object, once none of its units are left, goes to that sector as the war map comes back.

In sector 30, with sector 24 taken to have its command building for the test (none of London's neighbours has one
in the save, so there are no points), the points were (100, 350), kinds 7 and 1 (24's entry (65, 350) brought in
from the left edge); Retreat sent the lorries along a 47-node route and all five left there by updates 3164-3236.

*Provisional*: the room checks (0x43AC40) aren't in; kinds 7 and 1 share 0x436050's first point (the engine's
EntryPoint); what 0x80BB does isn't traced: a unit within 256 (|dx| + |dy|) of its point leaves the map (the group
spreads as it arrives); the retreated object is taken out of its counter into a new one in that sector, arriving
from the battle's sector (AddCounter, Arrive).

## Flying objects and emitters (0x487730, 0x487890, 0x47F050, 0x47F770; 0x48AEF0, 0x48ADD0; `battlemap/Effects.cpp`)

**The classes** on the flying object 0x487730 (its update 0x487890): WAnimEffect (0x488FB0, update 0x489050: the
factory's 0x15B-0x16E, 0x174-0x17A, 918-920, 952-972, 988-993, 1000 and 0x17C) with WExplosion on it (0x488180); the
smoke (0x48DCB0: 0x16F-0x173, update 0x48DD30: +0x84 less -1.5, rising, then 0x489050); the mortars' shells (0x488E80,
0x488F00); 0x17B the plain object. An animated one with a renderer moves its frame on by +0xC0 (0x80, 8.8) an update
(0x40EF20) and goes past its last frame (vt+0x78); otherwise, each update, 0x487890 runs its emitters (0x47F050) and
its step (vt+0x54 = 0x47F770: unless +0xB8 bit 10, the point moved by the velocity (+0x88 / +0x8C) and the height
(+0x84) kept level over the ground's change: ftol(+0x80 - +0x84) less the ground at the new point (0x47B170), plus the
height and the upward speed (+0x90); then +0x90 plus the gravity (+0x94)), and under 0.0 high it's gone (0x8057).

**Emitters** (0x47F050; the list +0xB4 is the type's emitters, record 29, put in 12-byte entries by 0x47EF30): only
with [Options] "Show Smoke & particles" (byte_4B880C, 1 to start with). For each entry (+4 the type, +8 the spread,
+0xA the limit, +0xC / +0x10 the slowest and fastest, +0x14 the gravity, +0x18 the rate): the entry's fraction (+8)
goes up by the rate and its whole part n comes off; n are made: rand() % spread as the angle up, the speed rand() x
0.000030601626 x (fastest - slowest) + slowest, split by 0x43FE90 into across and up, across along rand() & 0xFFF
(three rand() calls a particle); each at the emitter's point and height, with that velocity and the gravity (vt+0x80,
vt+0x40, +0x94, vt+0x24). Then the entry's count (+4) goes up, and past a limit that isn't 0 the list is freed. Shell
396 trails smoke 367 (spread 1, still, rate 0.2: one every 5 updates); explosions 484-500 throw debris 350 / 352 (the
whole circle, speed 1-3, gravity -0.02, 5 an update for 4 updates); 915 throws 918.

**Damage smoke** (0x48AEF0, from the object's update 0x4967E0 after its look round): every 15th update (dword_4D2558
% 15), with the option on, the level ftol(5 - the health fraction (vt+0xF4 = 0x48ADA0: +0xC4 / 0x47DA20) x 5.0), at
most 4; above 0 (under 80% health): rand() under 0xCCC (one in ten) puffs 1000, then a vehicle (+0xB8 bit 0) puffs
0x16F + level - 1 (367-370) and anything else each of 0x16F up to it. A puff (0x48ADD0): r = +0xAC / 3 + 1, at the
point rand() % 2r - r each way (added as floats, ftol'd) and rand() % r over the height plus its +0x41 byte x 1.414,
still. (Called with +0xC0 bit 3 set first: that's cleared and 0x80C1 goes to the parent's +0x48's +0x6C.)

With the lorries at 40% health (level 3) and armed with a mortar for the test, each lorry gave off a 369 every 15
updates (about 12 alive at a time), the shells trailed 367 in flight, and 1000 came now and then; over 600 updates
the list held 57 slots, 35-50 alive. A rocket's hit (382) is a light (338) and 350, neither with a sprite: it shows as the
light's flash on the ground and the scorch.

**Lights** (0x48DD50 on 0x487A30: types 334-346; record 13 at 0x4EC598: a float and three times a, b, c): made as an
explosion's part (or the scenery's, LoadScenery), at the part's point, height plus 16 (0x48DE80: +0xA0 less -16; gone
at once with "Show lights" off; a parent of category 0-3 also rolls against the hour). Added (0x48DDC0): with any
time set it fades: +0xC0 0, the count (+0xD0) Ticks(a) (a x 30 / 20), the step (+0xC4) the float / that, phase
(+0xCC) 0, and it joins an updated list (dword_4D2550 +0x20); else +0xC0 is the float. Then 0x47EE10 makes its
renderer (vt+0x28 = 0x48E050: RTypeLight 0x4A27A0 with its range, the type's +0x44 word) and sets +0xC0 to the float
again, so a fading light starts from it, not 0. **Each update** (0x48E100): with the count under 0 the phase moves
on: 0 to 1, the step 0 and the count Ticks(b); 1 to 2, the count Ticks(c) and the step -(+0xC0 / that); after 2
it's gone (0x8057). Then the renderer takes +0xC0 (0x4A2840), +0xC0 goes up by the step, the count down by 1, and
the objects within reach (0x47FC50) get 0x8066. 334-336 are steady (0.40, 0.38, 0.34; range 50, 60, 70); 337-346 are
0.35 with a 1-2, b 1-12, c 12-24, range 30-120. A rocket's 338 (1, 1, 16; range 40) gave 0.35, 0.70, 1.05 three
updates, then down to 0 over 25 and gone: the grass round the aim lit yellow at the peak and dark again after.
*Provisional*: 0x8066 (the IOB objects lit again) isn't in; a light follows no moving parent (48DF50).

*Provisional*: an object with no sprite is made only if its type has emitters, and goes once they're spent; a dead
one's slot is taken again; one made in an update first moves in the next; frames under 3 never end; the parent's
vt+0x9C check, bit 3's 0x80C1 and the option's setting on the options screen aren't in.

## Driving (the ground vehicle 0x490820-0x4935C0; the group's move 0x482E60; `battlemap/Driving.cpp`)

A vehicle (category 1) keeps its driving state at +0x88..+0x18C: the step (+0x88 / +0x8C), the target (+0xF4 / +0xF8)
and its distance (+0xFC), the heading (+0x100, 4096 a turn), speed (+0x104), the steering velocity (+0x108 /
+0x10C), the arrival radius (+0x118) and its growth (+0x11C), the turn rate (+0x140: 64 for move classes 0-4, 8 for
5 / 6, 16 for 7), the update phase (+0x146, rand() at first) and the flags (+0x188). Added to the world (0x490E70)
its flags are 0x801 and its heading 0, whatever the model was turned to.

**Flags**: bit 0 is *stopped* (0x8031 sets it, 0x808B sets it to its lparam; when it goes, bit 1 goes too), 1 arrived,
2 arriving, 5 and 8 held, 13 the opposite of bit 0. The survey's "moving" for bit 0 is the wrong way round: the
update (0x4913B0) steers while vt+0x144 (0x4909D0) is true, which it is when bit 0 is clear (or 5 / 8 / a link, or
vt+0x108), and while the speed is over 0.01.

**The order**: the left button let go in cursor mode 1 on the ground (0x485A53) picks the world point under the
mouse (0x47B3F0) and sends packet 0x0E; each selected group (0x4812B0 -> 0x481CA0 -> 0x482510 -> 0x804F, 0x482E60)
then, while another group's route search runs, posts itself the order again 10 updates on; else it measures each
member's distance to the point (0x43FF00 from whole positions, +0xD4; members are vehicles, +0xB8 bit 0, not on a
link or with bit 8), the nearest (+0x68) and the farthest (+0x6C), whose position is the start. Both ends go to
vertices (kept on the map); within 3 of each other it's "ALREADY AT DESTINATION" (when the first member is a vehicle
without bit 9). Each end goes to the nearest node the group can stand on (0x482B00, below), and a ground vehicle
(vt+0x70 and vt+0x118 both no) asks the route finder (0x494440 with radius 4 and limit 200, "Routes" below). When it
says to go straight, "GOING STRAIGHT THERE!": each member gets 0x80C2 with where it says (back in the world: v x cell
- half >> 16). 0x80C2 (0x490A70): the route goes (+0xEC, +0xF0 0); the target is the point; the arrival radius the
unit's radius (+0xAC, the type's +0x44 word) x `arrivalRadius` - -20, its growth the group's average war map speed
(+0x1D0) x 0.01 x members x 0.5; then 0x808B(0) is posted (0x4355A0's last argument is the delay): at once if the
unit is moving (speed over 0.01), else after ftol(5 x members x (+0xD4 - the group's +0x68) / (+0x6C - +0x68 + 1) +
rand() % 8) updates. The offset (+0x13A) is left as it was.
Backspace (packet 0x11) sends the members 0x8031: stopped.

**The ground** (WBMGroup 0x4802E0, 0x480680): each group works out from its members, by move class (the type's +0x52
word) and OBJ.ojd records 10 (17 costs a class, 0x4F6E38) and 11 (max slope and slope base, 0x4F04C0 / 0x4F0510):
airborne (+0x1D8 bit 5) when every member is of class 7; bit 6 when their side holds the sector; for each of the 16
terrain types the largest cost (compared unsigned, so -1, can't cross, wins); the max slope (+0x4C) the smallest; the
class (+0x52) the one with the largest slope base (over -1000); and the slope costs (+0xB4) ftol(pow(base, s)), s 1
to 8, for all 8 classes. A vertex's cost (0x480680, strict or not) reads its ATM byte (terrain the low nibble, slope
the high + 1): 0 for a terrain cost of -1 or a slope over the max, and, unless airborne, for flags bit 1, bit 2 when
strict, or bit 3 in its own sector; else the slope cost x the terrain cost. In sector 30 the group of five came to max
slope 6, class 2, terrains 0-3 closed and 4-7 costing 3, 4, 9 and 5; the ground under them cost 1 to 8.

**Steering** (0x492F40), every vt+0x148 (3) updates on the phase:
- the accelerations, each its force x weight x maxAcc with its length at most max x maxAcc (0x491B20): arrival
  (0x491FA0: only on a route inside the radius, -velocity), separation, the target (0x491BB0: the target less the
  position, `targAccWeight` / `targAccMax`), avoidance, braking (0x491C70: with bit 3, -velocity unit, halved
  without bit 4), velocity matching and attraction to the route, gated as the survey lists;
- the order and gates (0x492F40, the slots of vtable 0x4AE6B0): vt+0x158 (0x491EA0) when the lookahead 0x492350
  finds something; the neighbours gathered (0x492C00); vt+0x168 (0x491FA0, arrival) when neither arrived (bit 1)
  nor bit 5; vt+0x154 (0x4920A0, separation) with no bridge link (+0x18C) and not bit 5; vt+0x150 (0x491BB0, the
  target) always; vt+0x15C (0x491D90, avoidance: with bit 3, away along the +0x110 vector, `avoidanceAcc*`) with no
  link; vt+0x160 (0x491C70) without bit 5; vt+0x16C (0x492160, velocity matching) with no link and not bit 5;
  vt+0x164 (0x492240) the same and +0xEC set;
- **neighbours** (0x492C00): +0x120..+0x144 cleared, then every object in four cells of the bucket grid (WBucketGrid
  at the battle map's +0x118, cells `[System] ZMapRes` square, default 400, over the world's width and the rows'
  extent): the unit's own cell and, for q 1 to 3 (0x449920), the cell beside it across, up or down, and diagonally,
  on the sides it's nearer (the remainder against half a cell). A neighbour clamped back onto the unit's own cell
  gives nothing for q 1 and 2, but q 3 then returns the other one again, so at a map edge one neighbour cell is
  counted twice. Each object (0x492A90) that isn't the unit, isn't category 4, passes vt+0xBC, is within 64 in
  height and isn't type 0x273-0x276 adds the difference over gap³ to the push (+0x120 / +0x124; the gap is the
  distance less both radii, +0xAC, at least 0.001) and is counted (+0x144); one of the same group (0x47EB00) that's a
  vehicle (+0xB8 bit 0) and going (vt+0x144) also adds its step (+0x88 / +0x8C) to +0x128 / +0x12C and its position
  to +0x130 / +0x134, counted in +0x138;
- **lookahead** (0x492350), first: bit 3 cleared; four points looked at, the unit's position and then each a step on
  along the heading (max(speed x 0.5, 1) x `collisionCheckDist` x the radius; dword_4D05C4 = 4 of them). At each,
  with a ground group (+0x34) and not bit 7, the vertices (kept on the map, 0x492950) a radius ahead, left and right
  are checked in turn for ground it can't cross (0x480680 giving 0, off a bridge link): +0x110 / +0x114 toward that
  vertex ((v x cell - half) >> 16 less the point), bit 3 set and 4 cleared,
  and at the first point a type of move class 5 or 6 returns true (then vt+0x158, 0x491EA0, steers away from it with
  `attributeAccWeight` / `attributeAccMax`). Then the object there (0x47FE10: the cells of 0x449920 in turn until
  one has one; in a cell (0x47FD40) the nearest, by the larger difference plus half the smaller, under its radius +
  the unit's, passing vt+0xBC) within 64 in height: +0x110 the way to it, bit 4 set when it's of the same group and
  not a vehicle going, bit 3 set, and the look ends. When that's at the unit's own position, an object that can't
  be crushed (vt+0xD8) has the unit put (vt+0x40) (distance less both radii) / 8 along the way to it, so out of it
  by an eighth each time; one that can is crushed 1 in 4 (rand() < 0x2000) with its type's effect (0x488070);
- **avoidance** (0x491D90): with bit 3, away along +0x110 (its unit vector negated), `avoidanceAccWeight` /
  `avoidanceAccMax`; **braking** (0x491C70): with bit 3, against the step (its unit vector negated), halved unless
  bit 4, `brakingAccWeight` / `brakingAccMax`;
- **arrival** (0x491FA0): on a route inside the arrival radius, bit 2 and against the step (+0x88 / +0x8C, not made a
  unit), `arrivalAccWeight` / `arrivalAccMax`; otherwise bit 2 cleared;
- **cohesion** (0x492240, the survey's "attraction to the route"): on a route, with any matched neighbours, toward
  their average position (+0x130 / +0x134 made the average), `attractAccWeight` / `attractAccMax`;
- **separation** (0x4920A0): with any counted, the push x the count, `separationAccWeight` / `separationAccMax`;
  **velocity matching** (0x492160): with any matched, their average step, `velMatchAccWeight` / `velMatchAccMax`;
- added in that order until their lengths come to maxAcc (the one that crosses it cut to what's left); the sum goes
  on the steering velocity;
- within (1 + 0.625 x sqrt(group size)) x the arrival radius of the target, the radius grows by +0x11C.

Every update: the velocity's heading and length (0x43FDA0); the speed (0x492DE0: at most the type's +0x54 word x 0.02
over the ground's cost / `speedCostDivisor` when that's over 1 and at most the length; going, at least 0.2, rising by
0.05 x it or falling by 0.1 x it; stopped, falling by 0.1 x it to 0); the velocity made the speed long; the turn
toward its heading (0x43FF30) at most min(ftol(8 + 120 x maxAcc x speed), +0x140 x speed), a base type 0x13 more
than 0x400 away turning toward the opposite and driving backwards; the step the speed long along the heading
(0x43FE90), taken by vt+0x54 (0x47F770: the ground's height at the new point); the model turned to the heading.

**Arriving** (0x491870): +0xFC the distance to the target (whole). On a route short of its last node, the next
waypoint (0x491790) comes once inside the arrival radius, or once the node after is nearer than this one (off a
link); at the last node, or with no route, it has arrived inside the arrival radius + the offset (+0x13A), and the
route is let go. Once (bit 1): the group counts it (0x482450) and 0x808B(1) stops it 9 x (members not arrived) + 60
updates on, so it runs on a little before slowing to a stop. (A second target at +0xC8 / +0xCC, when set and not the
target already, is taken first.)

*Provisional*: each group's ground is set up once at the start, counting every member as +0xB8 bit 0 (in a test the
five of a group travelled 34-54 apart and stopped 20-39 apart, their radii 14). Only units are in the grid (the game's
holds every battle map object that places itself), vt+0xBC lets all through, and the cells are worked out from where
they are each time rather than where they were put in. Crushing, bit 5, bit 7, bit 9, the bridges' links, the second
target, the attack range ("CAN'T GET TO WITHIN ATTACK DISTANCE!"), the order re-posted while another search runs,
the destination flag (type 0x17C), speech (0x10A, 0x10B) and engine sounds aren't in yet.

## Routes (WRouteFinder 0x493DF0-0x494960, BatRoute 0x499420-0x4998D0; `battlemap/Routes.cpp`)

**The nodes** (0x493DF0, when the landscape is set up): one every 4 vertices from 4 each way, (width - 4) / 4 across
and (height - 4) / 4 down (62 x 61 for sector 30's 251 x 251), each joined to those round it in the order left,
right, up, down, up-left, down-right, up-right, down-left. Nothing is checked here; a flag byte each (the link byte)
starts at 0. The finder is set up with the landscape, before anything is placed.

**More nodes** (0x494220, x, y, link byte; up to 0x400 of them): the new node's own list is the four grid nodes of its
cell (((v - 4) / 4) kept to 1 .. count - 2: top left, right, below, below right), and it's added to each of theirs:
each list is cut at its first entry of 0 or less (its -1 end; a list holding node 0 is cut there too) and a -2 leads
on to a chain of added nodes, the newest first, which the A* walks after the list (0x44EFA0).

**Bridges** (vt+0x104, 0x4983D0, as a bridge starts): a node 48 out beyond each end along the bridge's heading
(0x43FE90), at the vertex nearest, with the bridge's link number (+0xDC, from dword_59D9B8: 1 for the battle's first
bridge, counting up as they're made in 0x486490; dword_59D9C0 holds them by it), and the two joined (0x486820 ->
0x494400: each put in the other's own list after its four). A step between two nodes with the same link byte costs
the straight distance x the group's +0x5A, the members' record 10 17th values (dword_4F6E78) averaged, -1 once any
is (so a group that can't use bridges can't take that step). In sector 30, across the Thames from vertex 156,100 to
144,180: 11 nodes over the second bridge.

**Footprints** (vt+0xE4, 0x48A950, as each object is added to the world, 0x48A8B0): an object whose type is of class
0, 2 or 3 (+0xB8 bit 1, 0x47D828) and not a tree (0xCF-0xE5, bit 2) marks the vertices within its type's +0x46 word of
it (0x47C280: QuickDistance in whole cells from the vertex nearest it) with flag 2, which the ground's cost refuses.
Since buildings and scenery do, an order west from the start in sector 30 now finds a 4-node route round them.

**The nearest node** (0x482B00, both ends of a move): the vertex goes to the node at or before it (((v - 4) / 4) x 4 +
4) and squares 0, 4, 8 ... under 200 out are tried at 8 points each (right, right-down, down, left-down, left,
left-up, up, right-up), kept on the map, until the group can stand there (0x480680 strict). An order onto the Thames
stops on the embankment.

**The request** (0x494440: start and goal in vertices, the radius, the limit, the group): with a search already
running it waits. Each end becomes its node ((v - 4) / 4 kept to 1 .. count - 1). The radius over the float distance
(0x43FED0), over 1, makes the goal the start: no route. Else the goal is pulled back the radius along the line from
the start and when that line can be crossed (0x4994B0) that's the goal: no route. Otherwise a BatRoute cost
(0x499420) and an A* (0x44EE70, the war map's, `core/AStar.h`) are started, which the group's update polls
(0x483B70 -> 0x483270, 0x44EFA0 trying 200 neighbours an update, giving up only when nothing's left to try).

**The cost** (0x499660, from a to b with the goal): the line cost (0x4994B0: Bresenham over the vertices, each one's
strict 0x480680 added up, -1 at the first that can't be crossed, else the whole length x the sum / the count) times
the turn penalty from 0x4D09F8 (8 x 8: 1, 3, 6, 10, 16 by how far apart the ways are) for the way to the goal against
the way to b, x 5 >> 4, + 1. A way is 0x4D0AF8[(dy's sign x 3 + dx's sign) x 3 + (|dx| - |dy|)'s sign] (signs as 0,
1, 2; 8 for no difference, which indexes on into 0x4D0AF8 itself). Two nodes sharing a link byte cost their straight
distance x the group's +0x5A instead. The A*'s h is the same cost to the goal, so it's a real estimate; the goal
(0x499800) is any node within the radius (0x43FF00).

**The route** (0x494690 when found): a node per step (vertices, the link byte); each between the ends moves to the
average of it and the two either side (whole numbers, toward 0) when the lines from the one before and to the one
after can both be crossed; the headings (0x43FDA0, less 0x400) from each node's neighbours (the first's to the next,
the last's from the one before). "ROUTE FOUND" (0x483270): each member gets 0x802E(ftol(sqrt(members) + 1) x its
radius, the route); "ROUTE NOT FOUND" (0x4833F0): 0x8031 to each, the group stops (speech 0x10B).

**Following** (0x802E, 0x4911B0): the route kept (+0xEC, counted); the offset (+0x13A) rand() % (the value + 1); the
waypoint (+0xF0) the nearest node by 0x43FF00 (the last of equals); 0x491790 sets the target to it, with the arrival
radius the radius + 20 and no growth at a link node, else the radius x `arrivalRadius` + 20 and the growth as 0x80C2's;
then 0x808B(0) after the same stagger as 0x80C2's.

In sector 30, from vertex 152,100 to 80,94 the search ran along the river's north edge, 19 nodes, its corners
smoothed. With the straight-line check forced off, a group of five followed a 21-node route to the west and stopped
at its end, 1600 updates on.

*Provisional*: the search is run to its end at once rather than 200 tries an update; an RTypeISO type's +0x46 is
taken from OBJ.ojd (0x47EB10 overwrites it with the model's radius only for an object with a parent or vt+0xB8), and
nothing takes its footprint off again yet (vt+0xE8); node removal (0x494370) isn't in.

**On a bridge** (0x492F40 after the step): on a route whose waypoint has a link byte, the bridge's vt+0x100
(0x4984E0) is asked and +0x18C becomes the link; else, on a link, it's asked and +0x18C cleared when it says no.
0x4984E0: within the bridge's length (+0xD8) of both ends the vehicle is on it: at the point on the line its distance
from the first end along, the deck is the straight line between the ends' heights, or that point's ground when
higher, + 8; over the first and last segment (+0xF4, the length / the segments) it ramps from that point's ground up
to it; +0x84 is the deck less the ground under the vehicle (at least 0), which its position's height adds (0x47F770).
Off it, +0x84 is 0. On a link: the speed is 1 (0x492DE0), the ground check and the skip to a nearer waypoint are off,
and separation, avoidance, velocity matching and cohesion aren't asked; a broken bridge's link (+0xEC 0) over water
kills the vehicle (0x80A0) and stops its group. In the test the group crossed 24 over the water at speed 1, then went
on along the route on the south bank.
*Provisional*: broken bridges aren't in.

## Sounds and the flag (0x483590, 0x481770, 0x4913B0 / 0x491450; the mixer `platform/Sound`)

**The listener** is the view's centre (dword_4E9250 / dy) in the screen space the objects' points (+0x2C / +0x30) are
in. A sound played at a point (0x464EE0's x, y; 0, 0 for none) is at full volume within 320 x 200 of it, falling off
each way to nothing at 640 x 400 (ambient: 640 x 400 to 960 x 600), 0 when the across factor is under 0.05, the two
multiplied (0x464FF0); it pans by its x less the listener's x 4 (DSBPAN, not speech); an opcode 18 sound beyond the
outer border isn't made. The 5th argument picks the variation group: for speech, the voice.

**The flag** (0x483590): a player's move by a player's group (+0x1D8 bits 3 and 4) makes type 0x17C ("_ACK", category
4, GType 26), a WAnimEffect (0x488FB0): its sprite at the point (going straight, the goal + 2 vertices; with a route,
its last node), its depth half the width + 64 (0x489000), its frame counter (8.8) going on 0x80 an update (0x40EF20)
and gone past its last frame (7 frames: 12 updates). Then, unless the group has a target, it speaks **0x10A** in the
group's voice (0x449120: its first member's type's +0xA0; the three groups of 0x10A are the three voices, Sc / Sb /
Sa). "ROUTE NOT FOUND" (0x4833F0) speaks **0x10B** the same way. Speech plays at the listener (0x481770), and the leader's
portrait flashes (0x4842D0, not in).

**Engine sounds** (0x4913B0): an update in which a vehicle is neither going (vt+0x144) nor faster than 0.01 sets
+0x188 bit 11 and, if bit 10 was set, clears it and calls vt+0x124 (0x491590); any other sets bit 10 and, from bit 11,
calls vt+0x120 (0x491570). Both end in 0x491450: the sound at +0xE8 stopped, then (not dead, vt+0x9C) the type's
+0xA2 while going or +0xA4 while not, played at its screen point. For the Armoured Lorry those are 0x33 (TRKLOTS,
Engine, 0.6, looping) and 0x7A (TRUCKt01, 0.5). A battle's start (flags 0x801) has bit 11 set, so nothing plays until a
vehicle first moves.

*Provisional*: the engine sound stays where it started; vt+0x110 / vt+0x114 (the ground patch +0x13C / +0x13E) and
the selection voice (0x109, 0x439000) aren't in; a quiet positional voice isn't let go.
