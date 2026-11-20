# The battle map

Milestone 5. This page grows as the battle map's parts are worked out; so far it covers the terrain.

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

*Provisional:* the digit `<d>` is 0, 1 or 2 by comparing dword_4F7430 with dword_4BD4F8 / dword_4BD4FC, which are
not named yet; `wowdump --landscape` takes the table file to use.

LightTheMap then, for every row but the first and last:

1. lights each vertex by its slope to the next vertex across and down (0x40CA90): sqrt of the light direction
   against the normal, x strength, + ambient, x the level, added to a zeroed byte buffer ("ISOTemp");
2. casts shadows along the row: from each vertex (its height smoothed 3 x 3, 0x43FD10) up to 16 vertices away
   from the sun, any vertex the rise over the distance reaches the sun's tangent sets to level x ambient.

The lit level is then the buffer smoothed 3 x 3 for every vertex two in from the edge; the edge vertices go to 0
and the ring one in keeps what it had.
