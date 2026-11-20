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
  player's selects its group (with Ctrl: force fire at the ground under it, packet 0x13) and one of the other
  side's is attacked (packet 0x10).
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
mouse (0x47B3F0) and sends packet 0x0E; the selection's group (0x4812B0 -> 0x481CA0 -> 0x482510 -> 0x804F, 0x482E60)
snaps it to a cell and, when it can go straight there, logs "GOING STRAIGHT THERE!" and sends each member 0x80C2
with it. 0x80C2 (0x490A70): the route goes; the target is the point; the arrival radius the unit's radius (+0xAC, the
type's +0x44 word) x `arrivalRadius` - -20, its growth the group's average war map speed (+0x1D0) x 0.01 x members
x 0.5; then 0x808B(0) is posted (0x4355A0's last argument is the delay): at once if the unit is moving (speed over
0.01), else after ftol(5 x members x (+0xD4 - the group's +0x68) / (+0x6C - +0x68 + 1) + rand() % 8) updates.
Backspace (packet 0x11) sends the members 0x8031: stopped.

**Steering** (0x492F40), every vt+0x148 (3) updates on the phase:
- the accelerations, each its force x weight x maxAcc with its length at most max x maxAcc (0x491B20): arrival
  (0x491FA0: only on a route inside the radius, -velocity), separation, the target (0x491BB0: the target less the
  position, `targAccWeight` / `targAccMax`), avoidance, braking (0x491C70: with bit 3, -velocity unit, halved
  without bit 4), velocity matching and attraction to the route, gated as the survey lists;
- added in that order until their lengths come to maxAcc (the one that crosses it cut to what's left); the sum goes
  on the steering velocity;
- within (1 + 0.625 x sqrt(group size)) x the arrival radius of the target, the radius grows by +0x11C.

Every update: the velocity's heading and length (0x43FDA0); the speed (0x492DE0: at most the type's +0x54 word x 0.02
over the ground's cost / `speedCostDivisor` when that's over 1 and at most the length; going, at least 0.2, rising by
0.05 x it or falling by 0.1 x it; stopped, falling by 0.1 x it to 0); the velocity made the speed long; the turn
toward its heading (0x43FF30) at most min(ftol(8 + 120 x maxAcc x speed), +0x140 x speed), a base type 0x13 more
than 0x400 away turning toward the opposite and driving backwards; the step the speed long along the heading
(0x43FE90), taken by vt+0x54 (0x47F770: the ground's height at the new point); the model turned to the heading.

**Arriving** (0x491870): +0xFC the distance to the target; with no route it has arrived inside the arrival radius
(+ +0x13A). Once (bit 1): the group counts it (0x482450) and 0x808B(1) stops it 9 x (members not arrived) + 60
updates on, so it runs on a little before slowing to a stop.

*Provisional*: only the target behaviour is in (no collision lookahead 0x492350, no neighbours 0x492C00, so
separation, avoidance and velocity matching are missing and a group bunches up); no route finder (every order goes
straight, through buildings); the snap, the stagger's distances, the ground's cost (taken as no slowing) and the
bridges' links are approximations; no destination flag, speech or engine sounds yet.
