# Battle-map rendering parity review (rendering.md)

Scope: BattleMap::Draw and its view objects/keys (src/battlemap/BattleMap.cpp), AddRenderer / AddPartRenderers /
PoseParts, effect/beam/light drawing, the re-keying in Update, TerrainView, IsoObject, WofView, Bridge, ShadeTable.
Original: WLandscapeView 0x49F240-0x4A0370, the view list 0x40D750, RTypeWOF 0x49DA50/0x49DBD0, RTypeISO
0x49DDA0/0x49DE90/0x49E2C0, sprite 0x40EC50/0x40EE10/0x40F040, glue 0x47EB10/0x47F340/0x47F480/0x47F630, WBridge
0x497900/0x498110/0x4984C0, shade loading 0x40B6C0 (0x40C08D), 0x40B500.

Data lookups used below come from OBJ.ojd record 6 (`2 x word_4F0532[gtype*12]`) and record 62 (+0xA1, default 0xC0
from 0x4377D6) and the dat\*.SHH files (level 0 = darkest: BMHB level 0 mean ~25/255, level 63 ~208; BMHI and BMGI
have only 30 levels, BMHI levels 28-29 all black).

## How the original sorts and draws (reference)

- The view rebuilds its list every frame (0x40D750): for each dirty rectangle it walks the grid buckets in view and
  inserts each renderer into a singly linked list in ascending **key = word[r+0x1E] + word[r+0x26]** (inserted after
  all equal keys; duplicates found by pointer). 0x49F610 then draws ground row by row and after each row draws every
  renderer whose key < `(rowY + cell) >> 16` (vt+0x10), and the rest after the last row. The engine's
  `TerrainView::Render` + `between(limit)` + stable sort by key reproduce this (TerrainView.cpp:317-321,
  BattleMap.cpp:2020). Ties: original = bucket walk order, engine = creation order (low, not worth changing).
- **+0x1E** = the object's world y, ftol(+0x7C), written by 0x47F480 (vt+0x34 "place") at 0x47F516. Default 0x4650
  (18000) from 0x40E95F for a renderer nobody places.
- **+0x26** (depth over y) per renderer kind:
  - sprite 0x40EC50: 1; WEffect start 0x487800: `(+0x2A - +0x28)/2 + 0x20` = width/2 + 32 (0x487855-0x487868);
  - RTypeWOF 0x49DA50: 2 x record 6 d at creation (0x49DB1C) **but every pose 0x47F340 overwrites it with
    `(model+0x72 - model+0x6C) / 2`** (0x47F392-0x47F3A3): half the posed model's world-y (depth) extent;
  - RTypeISO: the model's depth (header 0x16 / sqrt 2) - engine IsoObject::Depth matches;
  - light 0x4A2800: `-32 - range` - matches BattleMap.cpp:1384/1090;
  - turrets (WTurret vt+0x4C 0x489AE0): with +0x192 set, +0x26 = parent's +0x26 + 1 and +0x1E = parent's +0x1E + 1
    (key = hull key + 2); else +0x26 += 4;
  - vehicle shadows 0x48E850: +0x26 and +0x1E each = parent's - 1 (key = hull key - 2), only while the parent's
    +0x84 >= 0.0 (flt_4AF790);
  - wakes/splashes 8 (0x48FDAB, 0x4935B3, 0x43A7BD); mine/sinker markers 0x20 (0x48CD69, 0x499320, 0x493DCF);
    gun beam quads 0x40 (0x495162) with +0x1E stepped per quad (0x495503); 0x487490 500.
  - deck riders: vehicle pose 0x491730 = 0x47F340 then, if +0x18C (bridge link) != 0, link->vt+0xFC = 0x4984C0:
    `+0x1E += 0x30; +0x26 += 0x40` (key +0x70).
- **Renderer level (+0x34)** is set every object update: 0x48B3D0 (generic update) calls vt+0x5C (0x48B456), which
  takes vt+0x58 = 0x48B050 (the fog brightness at the object: 1, or b/smooth + (1-b/smooth) x darkness) and calls the
  renderer's vt+0x18 with ftol(f x 255) (0x48B2A0 base, 0x4897D0 WScenery, 0x48B2D0 units, which also hides) or
  ftol(f x type+0xA1) (0x48D000 type 0x1C0, 0x48F2E0 street lamp). WEffect's vt+0x5C is nullsub_3 (0x4ADACC), so
  effects keep the +0xA1 set at start (0x487846). The level is then used as:
  - sprite: vt+0x18 = 0x40F040 stores `arg >> 2` → shade level for 0x40B500 (no clamp: `table + 1 + level<<9`);
  - WOF: vt+0x18 = 0x40EC30 stores arg; 0x49DBD0 passes `+0x34 / 255` (flt_4B11B4) to 0x49EC00 → 0x49ED90, which
    multiplies **the sun term** (`SlopeLight x arg_18 x dword_4DA774 x 256`, 0x49EE1D) and the point lights by it;
  - ISO: vt+0x18 = 0x49E2C0: when the level changes it re-runs 0x49DF40(level) (the base patch lights at that
    brightness) and 0x49DE90 passes `+0x34 / 255` (flt_4B11DC) to 0x4113F0 (the point-light cap 63 x it).

## Visible bugs

### 1. Units on bridges drawn behind the deck — HIGH (confirmed; also found by the movement reviewer)
- Original: 0x491730 → 0x47F340, then `dword_59D9C0[+0x18C]->vt+0xFC` = 0x4984C0 adds 0x30 to +0x1E and 0x40 to +0x26
  (key + 0x70 = 112). Bridge segments are child objects posed by 0x47F340 in 0x498110 (0x4981B0) so their key is
  segY + half the posed segment's y-extent (~32-40), not 64.
- Engine: unit key `u.y + 2*six.d(hull)` (BattleMap.cpp:1133, re-keyed at 1708), segment key `y + 2*six.d(bridge)`
  (BattleMap.cpp:1488) with no link bonus anywhere (`drive.link` is only used for driving/shadows). Bridge_1 (GType
  0x105) d = 32 → 64; Armoured Lorry hull (0x26C) d = 18 → 36: a lorry on the segment it stands on keys 28 lower than
  that segment, so the deck draws over it.
- Fix: in the re-key (BattleMap.cpp:1704-1717 and Driving.cpp:637) add 0x70 to a unit's own WOF view object while
  `u.drive.link != 0`. Note the original bumps only the hull's renderer; the shadow follows (it copies the hull's
  +0x1E/+0x26 less 1) and so do turrets (parent + 1), so implement item 2 or bump those too.

### 2. Turrets keyed independently of their hull (drawn under big hulls) — HIGH
- Original: WTurret 0x489AE0 sets turret +0x26 = hull +0x26 + 1 and +0x1E = hull +0x1E + 1 each update (when +0x192,
  set by 0x489980 unless the parent's kind is 4-9): the turret always draws just after its hull.
- Engine: AddPartRenderers keys a turret `u.y + 2*six.d(turret gtype)` (BattleMap.cpp:878-913), re-keyed the same at
  1708. Ironclad hull GType 0x27E d = 162 → 324, its turret 0x2B7 d = 18 → 36: the turret keys 288 earlier and the
  hull's deck is drawn over it. Same for every hull with a large d (e.g. 0x281-0x286 d 72-75, 0x28E-0x296 d 75-112)
  carrying d-18 turrets.
- Fix: give turret view objects `key = hull key + 2` (store depth as hull depth + 1 and +1 on y), recomputed whenever
  the hull's key is.

### 3. Objects are never dimmed by the fog (renderer level ignored) — HIGH
- Original: every object update sets the renderer level from the fog at the object (see reference; 0x48B3D0 →
  vt+0x5C). WOF faces' sun light x level/255 (0x49EE1D), ISO base lights rebuilt at the level (0x49E2C0 →
  0x49DF40), sprites at (level >> 2).
- Engine: WOF Draw uses brightness 1.0 (WofView.cpp:298-300 "x the brightness (1)"), ISO BaseLights at 0xFF once
  (BattleMap.cpp:776/1255/1394; IsoObject.h), sprite level 0. Fog.cpp:87-90 already marks this *Provisional*
  ("the brightness (level x 255 to the renderer, 0x48B2A0) isn't applied"). Visible in the London screenshot:
  Parliament / Big Ben / trees fully lit on fog-darkened ground.
- Fix: per update, per object, level = ftol(FogLevel x 255) (lamps/0x1C0: x +0xA1); pass level/255 into
  WofInstance::Draw (multiply the sun term and the point lights), rebuild an ISO model's lights with
  BaseLights(level) when its level changes (needs per-instance light arrays, see Architecture 2) and use cap
  63 x level/255 in the ISO point lights; sprites at level >> 2.

### 4. Scenery sprites and construction corners drawn at shade level 0 (nearly black) — HIGH
- Original: street lamps (type 0x110, GType 0x15A stlamp.spr, slot 1 BMHB) get ftol(0xC4 x f) >> 2 = 49 (0x48F2E0 →
  0x40F040); other WScenery sprites (CP/reinforcement markers 0x3DB, HEXITARR) ftol(255 x f) >> 2 = 63 (0x4897D0);
  building corners (type 0x17B, GType 0x11 bld_mark.spr, +0xA1 = 180) 180 >> 2 = 45, set at 0x48EC7A.
- Engine: Draw computes a level only for `o.effect >= 0` (BattleMap.cpp:2046-2053); lamps (LoadScenery 1366),
  reinforcement markers (Reinforcements.cpp:53) and corners (AddCorners → AddRenderer 1150, though its comment
  says "drawn at its type's +0xA1 level") all draw at level 0 (BMHB level 0 mean ~25 vs ~145 at 49).
- Fix: store a level on each sprite ViewObject (scenery 63 x f, lamp/0x1C0 (A1 x f) >> 2, corners A1 >> 2) and use it.

### 5. Move-order marker (0x17C, hove-ack.spr) is a black circle — HIGH (cause found by the UI reviewer; confirmed here)
- 0x17C → GType 0x1A, slot 5 (BMHI, 30 levels). The engine draws effects at (A1 >> 2) = 48 (BattleMap.cpp:2051),
  ShadeTable::Level clamps to 29, and BMHI levels 28-29 are all black. The UI reviewer found the original sets this
  marker's level to 0 (0x483590) after its start, which gives BMHI level 0 (mean ~119). Not contradicted by anything
  here: 0x487846 sets A1 at start, WEffect's vt+0x5C is a nullsub, so a later explicit vt+0x18(0) stands. Fix: level 0
  for this effect. Also: 0x40B500 never clamps, so the engine's clamp in ShadeTable::Level hides other out-of-range
  levels; log them instead.

### 6. Effects keyed 32 too far forward — MEDIUM
- Original 0x487855-0x487868: WEffect +0x26 = width/2 + 0x20.
- Engine Effects.cpp:118: `image->width / 2 + 64` (no citation). Explosions/smoke draw over objects up to 32 world-y
  in front of them. Fix: + 32.

### 7. Vehicle shadows keyed parent - 1 from a fixed depth — LOW
- Original 0x48E883-0x48E893: shadow key = hull key - 2, recopied every update, and only while the hull's lift >= 0
  (a sinking hull's shadow freezes in place).
- Engine BattleMap.cpp:908-913: depth = hull view object's creation depth - 1; key = u.y + that. Fix with item 2's
  "follow the hull's key" mechanism (- 2), and stop updating it while `u.drive.lift < 0`.

### 8. Mine / sinker markers placed and keyed differently — MEDIUM
- Original: the marker renderer's +0x1A/+0x1C are copied from the object's +0x2C/+0x30 (0x48CBD6, 0x4990AB), which
  0x47F480 sets to (x, screen y - type byte +0x41) (0x47F4EE-0x47F4FA → 0x4406A0); its +0x1E is never set (0x4650),
  so key = 18000 + 0x20: drawn after the whole ground and every object, i.e. always on top.
- Engine Weapons.cpp:577-585 / BattleMap.cpp:1709-1715: at the unit's ground point (no +0x41 lift), key = unit key + 1,
  so terrain rows and objects in front can cover it. This settles the *Provisional* at Weapons.cpp:556. Fix: key
  0x4650 + 0x20, y = projected y - size41.

### 9. Turret gun beams/flash sprite level — LOW (unverified detail)
- Beam quads: 0x4950D0 calls vt+0x18 (= 0x40F040 for RType 0x4B1150) with +0xA1 → level A1 >> 2: engine matches
  (BattleMap.cpp:956). Beam keys (gy + 40 stepped a quarter) match 0x495162/0x495503 in shape; the exact +0x1E per
  quad was not re-derived.

## Behaviour divergences

### 10. WOF depth never recomputed from the posed model — MEDIUM
- Original 0x47F340: +0x26 = (bound max depth - min depth) / 2 after each pose; this is why a long hull turned N-S keys
  further forward than turned E-W.
- Engine: fixed `2 x six.d` for hulls, parts and bridge segments (BattleMap.cpp:878-913, 1119-1133, 1468-1488).
  WofInstance has the depth bounds (boundMin/Max[2]) but no accessor. Fix: expose `(boundMax[2]-boundMin[2])/2` after
  Transform and re-key with it in PoseParts/Update; bridge segments: compute after their (unstitched) pose, before
  Stitch, as 0x4981B0 does.

### 11. Point-light caps and WOF brightness assume level 255 — MEDIUM (part of 3)
- ISO: 0x49DE90 passes +0x34/255 to 0x4113F0 (cap 63 x it); engine BattleMap.cpp:2096 cap 63. WOF: 0x49EE3F
  multiplies the point lights by arg_18 x "WOF Brightness"; engine x 1.0 (WofView.cpp:300).

### 12. Piece order inside a WOF: qsort vs stable_sort — LOW
- 0x49ED32 uses CRT qsort (unstable); engine std::stable_sort (WofView.cpp:398). Equal "nearest" keys can order
  differently. Only matters for coplanar pieces; leave unless a model shows z-fighting differences.

### 13. View list ties — LOW
- Original ties by bucket walk (0x40D96A-0x40D9ED), engine by creation order (stable_sort). Leave.

## Architecture misalignments

1. **Keys are flat `u.y + depth` per view object, not per renderer `+0x1E` + `+0x26` with parent links.** The
   original's turret/shadow/bridge-rider rules all copy or add to another renderer's +0x1E/+0x26 each update. Give
   ViewObject separate `y` (+0x1E) and `depth` (+0x26) fields plus an optional `parent` view-object index, and compute
   `key = y + depth` after the parent; re-key all objects each update (not only when `moved`, BattleMap.cpp:1702),
   since turning changes +0x26 (item 10).
2. **ISO lights are per model, not per instance.** `IsoModel::lights` is shared by every object using the model
   (BattleMap.cpp:776, 1255, 1394), but 0x49E2C0 rebuilds the lights per renderer at its own level. Needs per-instance
   base lights (or a cache keyed by level) for fog dimming.
3. **No renderer level on ViewObject.** Add `int level` (+0x34) set by the per-object vt+0x5C equivalent each update;
   Draw then uses it for all three renderer kinds instead of special-casing effects.

## Provisional items resolved

- Fog.cpp:87-90 "the brightness (level x 255 to the renderer, 0x48B2A0) isn't applied": the original applies it every
  update via 0x48B3D0 → vt+0x5C (0x48B456); see item 3 for exactly how each renderer kind uses it.
- Weapons.cpp:556 "the renderer's +0x26 (0x20) isn't known": +0x26 = 0x20, +0x1E stays 0x4650 → drawn on top of
  everything, at the object's +0x2C/+0x30 (item 8).
- AddPartRenderers "the parts' own parts aren't drawn"/turret order: turrets follow the hull at key + 2 (0x489AE0).
- The shade slots of 0x40C08D (engine BattleMap.cpp:77-84): confirmed BMHB→1, BMHV→2, BMMV→3, BMMB→4, BM{S}I→5,
  BMGI→9, BMR<d>NA→10, BMEX→11, LANDR→20, {S}MINIB→21 (dword_4DAE3C.. = 0x4DAE38 + slot x 4, read by 0x40B500).
- Effects' depth "width/2 + 64": the original is width/2 + 32 (0x487865).

## Missing

- Bridge-rider key bonus (item 1), turret/shadow parent-linked keys (items 2, 7), per-pose WOF depth (10), fog level
  on all renderers (3), sprite levels for scenery/corners (4), marker on-top drawing (8).
- WOF renderer +0x74 (the tunnelling track layers' sink offset, Sinking.cpp:63 *Provisional*) - not drawn.
- The sinker's text label renderer (+0x198, Sinking.cpp:47) - not drawn.

## Checked and matching

Row/object interleave limit `(row+1)*cell >> 16` (0x49F71F vs TerrainView.cpp:317); light key -32-range (0x4A2827);
ISO depth/offsets (0x49E1E0, 0x49DE90 x = +0x1A + +0x28, y = +0x1C + +0x2C); sprite key +1 (0x40ECA7) and hot-spot
placement (0x40EAF0); wake depth 8; beam quad level A1>>2; bridge cross-sections (Bridge.cpp vs 0x48D2F0).

Runtime note: a headless London run (scratchpad/rtools/shots/mv.png) shows item 3 (bright buildings on fogged
ground). I could not get the scripted move onto the bridge to run, so items 1-2 are from the code and data only.
