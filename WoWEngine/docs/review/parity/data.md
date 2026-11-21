# Parity review: data loading and formats

Scope: src/core/ObjOjd.*, Landscape.*, Scenery.*, Sprite.*, ShadeTable.*, Palette.*, Models.*, SaveFile/SaveGame, AiTable.*,
SoundTable.*, Archive/Vfs/GameFiles, Random.h. Original: OBJ.ojd loader 0x437750 (all 65 cases read), the [Tweak] modifier
statics 0x4373D0-0x437715, GType registration 0x40E7A0, WLandscape 0x47AF10 / CreateTerrain 0x47B900 / LightTheMap 0x47BD80 /
CalcNormals 0x47C100 / HeightAt 0x47B120-0x47B170, the sun 0x40B030, SlopeLight 0x40CA90, LoadShTab 0x40B570, palette colours
0x40B120 / 0x40B3B0, sprite row walker 0x451157, IOB 0x49DF40 / 0x49E1E0, SFX.ojd 0x463D10 / 0x466B40 / 0x466BC0, AI.ojd 0x441580,
FILE lookup 0x415030, the random table 0x4025F0 / 0x402780, the object factory 0x446B30, the scenery classes 0x489380 / 0x4895E0 /
0x48F330-0x48F571.

Checked against the shipped data with throwaway scripts (scratchpad/datachk/): OBJ.ojd record counts, the record 37/38 rounding,
signedness of every record 6/12/30-37/52-57 word, SFX.ojd duplicate samples and group usage, all 395 sprites for zero-length or
overshooting runs, all 30 .nsb scenery lists against their maps.

Overall: the OBJ.ojd unit table, the battle tables, the GType table, terrain creation, lighting, normals and height sampling match
the original field for field (offsets, defaults 0x4377A3, scales, ftol truncation, faction/side modifier selection all verified;
ini key -> address mapping verified: e.g. HumanFriend Armour = 0x4F2E98+0x14 = flt_4F2EAC). The real divergences are in the
scenery's rand() stream, the sound table's sample handling, and a handful of latent (not triggered by the shipped data) precision
and signedness differences.

---

## Visible bugs

### 1. Bridges draw a rand() for "burnt" that the original never draws: which trees are burnt and which lamps stay lit drift (medium)
- Original: only WScenery-derived classes run the burnt-weed roll in their constructor (0x489380: `if count > 3: rand() < (0x8000/7)*(count-3)`).
  The factory 0x446B30 sends every scenery type the .nsb files use to such a class (0xCF-0xD4 via 0x48F330, 0xD5-0x10F, 0x111-0x115,
  0x117, 721-741, 845-904, 973-985 via loc_4478E9 -> 0x489380, 0x110 via 0x48F290, 686-693/940/994-996 via 0x4890A0 -> 0x48B480 -> 0x489380)
  EXCEPT bridges 445-447: 0x447506 -> sub_48D050 -> sub_497890 -> sub_4862C0 -> sub_489E20 (the plain base), no rand().
- Engine: src/core/Scenery.h:82 calls `scenery::Burnt(o.type, enemyCount)` for every object in the list, bridges included.
- Effect: in any sector with more than 3 enemy units/buildings (dword_4F7430 > dword_4BD4F8 = 3) each bridge before a tree shifts
  the rand() stream by one, so every later object's burnt roll differs (different trees shown as red weed), and the street lamps'
  `rand() % 100` light rolls (0x48DE80) taken after loading differ too, up to the first tree circle's srand(). The shipped maps hold
  41 bridges (26 x 445, 4 x 446, 11 x 447).
- Fix: in PlaceScenery only call Burnt for types the original builds as WScenery; simplest exact rule: skip 445-447 (and, to be
  complete for other data, 0x116, 0x118-0x121, 824-844, 905-914 and the other non-0x489380 cases of 0x446B30).

### 2. Sound variations start on the wrong sample (low-medium, audible)
- Original 0x466BC0: the entry's per-group cursor (+0x4C + group*4, zero at load) is advanced BEFORE use:
  `if cursor < count-1: cursor++ else cursor = 0; return sample[cursor]`, so a 3-sample sound plays 1, 2, 0, 1, 2, 0 ...
- Engine: src/platform/Sound.cpp:115-116 `group[cursor++ % group.size()]` plays 0, 1, 2, 0 ...
- Fix: `cursor = cursor + 1 < size ? cursor + 1 : 0; sample = group[cursor];` (cursor starting at 0). The cursor also lives in the
  entry in the original, so opcode 13's copy (0x464005, 0x70 bytes) copies it; harmless to ignore.

---

## Behaviour divergences

### 3. Empty variation group does not fall back to group 0 (low; latent with the shipped SFX.ojd)
- Original 0x466BC0: if the requested group (> 0) is empty and group 0 is not, it uses group 0.
- Engine: src/platform/Sound.cpp:105-106 returns 0 (silent).
- With the shipped data every voiced sound (265-286, the only ones Speak plays with a voice > 0) defines groups 1-3, and the
  results match (checked both sides), so nothing is audible today. Fix: `if (group.empty() && groupIndex > 0) group = def->groups[0];`

### 4. Sample de-duplication is per group in the engine, per sound in the original (low)
- Original 0x466B40 looks the sample up in all three groups; if found it adds nothing and returns the ENTRY pointer, after which
  0x463EA9-0x463EB0 writes +0x15/+0x18/+0x1C into the entry itself (an original bug: it overwrites group 1's first two sample
  pointers with the rate and priority; harmless because group 1's count stays 0). There is no 5-per-group cap.
- Engine: src/core/SoundTable.cpp:184-188 checks only the current group and caps at 5.
- Shipped data: sounds 274-286 repeat "None"/"ALLOO" across groups; with the fallback of item 3 the effective samples are the same.
  Fix together with item 3: dedupe across all groups, drop the cap.

### 5. SFX opcode 0 with a type > 5 keeps the previous priority (low, latent)
- Original 0x463DAB: `ja default` for type > 5 skips the priority store; engine SoundTable.cpp:160 sets 0xC0. Types in SFX.ojd are 0-5 only.

### 6. SFX opcode 1 does not clear the entry (low, latent)
- Original 0x463DE1 sets only +0x58 = 0, type, volume; loop/flags/+0x5C/+0x64/samples stay if an id is defined twice. Engine builds a
  fresh SoundDef. No id is defined twice in SFX.ojd.

### 7. Record 38 (war map speed scale) and record 39 (research hours) round through float; the original keeps x87 precision (low, latent)
- Original 0x438392: `fild word; fmul float; ftol` (no float rounding before truncation). Same for 0x438444 (days*24 x modifier).
- Engine: ObjOjd.cpp:235-236 `int(float(speed) * a)`; ObjOjd.cpp:278 `int(float(days*24) * factor)`. A product just under an integer
  can round up to it in float and truncate one higher (e.g. 100 x 0.7f).
- Shipped data: no record 37/38 pair diverges (checked all 43), and 0.75 is exact. Fix: compute in double: `int(double(v) * double(f))`.

### 8. Record 44 is applied out of order (low, latent)
- Original 0x438595: `+0x5C *= f` at that moment (a later record 43 overwrites it). Engine keeps a separate buildMul applied after
  record 43 always (ObjOjd.cpp:194, 287) and multiplies the muls together first. No record 44 in OBJ.ojd. Fix: store +0x5C directly.

### 9. Record 35 without a record 34 is ignored (low, latent)
- Original 0x4382BA multiplies +0x4A (default 100) whatever came before. Engine Armour() returns 100 when armourFaction < 0 and
  never applies armourMuls (ObjOjd.cpp:294). No record 35 in OBJ.ojd. Fix: start from 100 and apply the muls in both cases.

### 10. Signedness of record 6 / 12 words (low, latent)
- Original readers use movsx: record 12 words (0x494CB6, 0x494E75, 0x4887B1 ...), record 6's first word (0x49DD4B).
- Engine reads them as uint16 (ObjOjd.cpp:323, 326). No value >= 0x8000 in OBJ.ojd. Fix: int16_t.

### 11. [Tweak] "VisRadMul" is never read (low)
- Original 0x4373D0 reads it (default 1.5, flt_4F706C) and record 32 uses it. Engine: FrontEnd.cpp:1177-1181 reads the five armour
  modifiers but not visRadMul, so ArmourModifiers::visRadMul is always 1.5. Fix: add `s.armour.visRadMul = ini.GetFloat("Tweak", "VisRadMul", 1.5f);`.

### 12. A scenery object outside the map goes to (0, 0) in the original (low, latent)
- Original WScenery::Load 0x4895E0: if x >= worldWidth, y >= height x cell, or either < 0, BOTH are set to 0 (not clamped, 0x489653).
- Engine: LoadScenery/PlaceScenery keep the position (Scenery.h:89). All 30 shipped .nsb lists are inside their maps. Fix: replicate.

### 13. Random(n) for n < -1 (low)
- Original 0x4025F0 `idiv (n+1)` (crash for n = -1, a remainder for n < -1). Engine Random.h:39 returns 0 for n < 0. Harmless.

---

## Architecture misalignments

### A. Tree circles are flattened into the world list (medium)
- Original 0x48F410: each tree of a circle is created by the factory, added to the CIRCLE's own child list (`[ebx+50h]`, 0x47D190),
  given the circle as parent (`[esi+44h] = ebx`), placed with vtbl+38(dx, dy, 0) where dx/dy are the offsets from 0x43FE90, then
  activated (vtbl+14). Engine (Scenery.h:118) emits independent PlacedObjects at circle.x + dx.
- Unverified: whether vtbl+38 of the child adds the parent's position (the engine assumes it does). Anything the original does via
  the parent (drawing/sorting the children through the circle, removing them with it, saving them as the circle's) cannot be
  reproduced. Restructure: give PlacedObject a parent index (or keep a circle object with a child vector) and let the battle map's
  object list be hierarchical like WObjectList.

### B. OBJ.ojd is parsed six times by separate loops (low)
- The original is one pass with shared running state (current GType id var_38, current unit var_34, class var_18, scale var_3C, box
  var_49). The engine has LoadGTypes, LoadUnits, LoadBattleTables, LoadArrowTypes, LoadResearchPages, LoadSectorDeposits/Maps, and
  the unused LoadUnitCounters (ObjOjd.cpp:87, no caller; its record 18 copies only when the template has a counter, unlike the
  original's full copy), each repeating the skip logic. Fix: one pass filling one struct; delete LoadUnitCounters.

### C. Modifiers are applied at use, not at load (low)
- The original bakes +0x08 (research hours), +0x4A (armour), +0x48 (vision), +0x5C (build days) into the entry while reading, with
  byte_4B84C4 (the CD/"I Am Human" side) fixed at start-up. The engine keeps raw values plus the faction at the time and recomputes;
  equivalent today, but it is why items 8 and 9 diverge. Storing the baked values like the original removes that class of bug.

### D. Saves are carried as opaque blobs found by tag scanning (medium, robustness)
- SaveFile.cpp:47-66 splits object lists by searching for "SELO" followed 16 bytes later by "WMOB"; BattlePosition/SetType/
  BattlePositions scan for "BATP"/"BAMO"/type+"SELO" byte patterns. The original reads each object through its class's serialiser
  (0x421B80, 0x47D360, 0x489F30, 0x490A40, 0x495710, 0x4A3C80). A float/int field that happens to spell a tag would mis-split or
  patch the wrong bytes, and battle-map state inside the blobs (unit hit points etc.) cannot be edited field by field. Restructure:
  per-class readers for BMOL/BAMO/VEHI/BATP/VEHU/SELO as the original's.

---

## Provisional items resolved

- Scenery.h:48 "*Provisional*: the order of the rand() calls of objects other than trees and lights isn't traced": every object the
  .nsb files use is a WScenery (one rand() at construction when the sector count > 3, 0x489380) except bridges 445-447, which draw
  none (see item 1). The tree circle draws: its own burnt roll (0x48F338 -> 0x489380), then rand() for its seed (0x48F343), which
  Load (0x48F3B0) replaces. At activation (0x48F410): srand(seed), rand() & 0xFFF, then per tree rand() % radius (+0x44), rand() %
  7 or 10, and the child's own burnt roll inside the factory. Engine order matches for these.
- Models.h:16-20 "Several megabytes follow ... what's really there isn't worked out yet": the CLS header 0x30/0x34 and the three
  tables at header 0x38/0x3C/0x40 (offsets from 0x44) are the terrain view's (0x49F2D2-0x49F3CB), already used by
  TerrainView::Load. Only the comment is stale.
- ObjOjd.h names guessed fields; what the original does with them (reader addresses):
  - field28 (+0x28, record 24): the type's graphics GType (0x47EB6B-0x47EDAF renderer pick, 0x48DC79, 0x494B1B, 0x4951A4).
  - field43 (+0x43, record 56, default 1): the battle object's starting stance (0x49575C in the BATP set-up, 0x496797).
  - field50 (+0x50, record 33): read once at 0x48EA67; OBJ.ojd has no record 33, so always 0.
  - field52 (+0x52, record 37 word 1): battle/war map movement class (0x48036B `cmp 7`, 0x47E296, 0x4195B7, 0x422224, 0x42427D).
  - field56 (+0x56, record 37 word 3, scaled by record 38): no reader anywhere in the exe.
  - field4C (+0x4C, record 57): no reader; no record 57 in OBJ.ojd.
  - field92 (+0x92, record 53, default 90): 0x480533, 0x496848.
  - field98 (+0x98, record 60, default 0x250): the explosion type (0x488E65, 0x48A2F9, 0x492916).
  - fieldA0 (+0xA0, record 61 less 1): voice class (0x4391AC, 0x44913D, 0x491060, 0x495E5D); values 0-2 in the data.
  - fieldA1 (+0xA1, record 62, default 0xC0): shade level (0x486685, 0x487846, 0x48852F, 0x48EC7A ...).
  - size40/size41 (+0x40/+0x41): +0x40 is rewritten at run time (0x4874A5 = 0x10, 0x4893B9 = 1) and read at 0x48A6AF; +0x41 is
    the height used by the explosion/turret z (0x47F4EE, 0x48A2C2, 0x4928DD ...).
  - record 19's second word (+0x8C) is dropped by the engine (ObjOjd.cpp:144); nothing reads dword_51A17C, so that is fine.
  - record 8's four skipped words: the loader skips them (0x437B33-0x437B4B), confirmed unused.
  - record 9 keeps only the FIRST character of each set's name (byte_4DA82C, 0x437898); the engine keeps the string and uses [0].
- Landscape.h:61-64 ShadeClass thresholds: dword_4BD4F8 = 3, dword_4BD4FC = 10 are .data constants, not ini keys; engine correct.
- Hour index for the sun: ftol(calendar +0x28) % 24 (0x47AF87-0x47AF95); a debug override (byte_4D1279 && dword_4DA81C) exists only
  in debug builds. Engine FrontEnd.cpp:1125 matches.

---

## Missing

- Debug sun-hour override (0x40B030: byte_4D1279 / dword_4DA81C) - debug only, not needed.
- The fourth float of each day-time set (flt_4DA768) is used (lamp chance, 0x48DED5); the second and third named sets of record 9
  (0x4DA840-0x4DA864) have no reader found in the exe other than the loader; nothing missing there.
- AI.ojd: the running weight total (0x441662 `[edi+4] += weight x mult`, unrounded) is not kept by AiTable/FrontEnd (unverified
  whether the AI reads it; grep for its consumer of [this+4] of the 0x441580 object).

## Verified identical (no action)
OBJ.ojd payload sizes for all 65 record types; unit defaults 0x4377A3; record 15 first-seen rule; record 18 copy (0x2C dwords) and
clears (+0x38, +0x42, +0x78-0x80, emitters kept); records 16, 21-34, 36, 37, 39-43, 45-62, 63/64 offsets, signedness and scaling;
record 2/3/4/5/7 GType registration incl. reset of hotspot/flag after every 2/3/4 (also unwanted ones), flag semantics (0x40E7A0
+0x10: 1 = load on use); all 13 [Tweak] keys and defaults and which address each side/faction picks; BuildHours (0x461BB4);
CreateTerrain (ATM to top-left vertex, flags 1, height x 92682 >> 16, topY); CalcNormals incl. float/extended rounding; LightTheMap
(both shadow directions, compare sense, ambient shadow level, 3x3 /52 smoothing, edge zeroing); SunFor incl. pointLight =
(0.2 + 0.5 sin) x f2 x 64; SlopeLight argument order; HeightAt/HeightAtNearest; LoadShTab level search and Martian L->X / N->Y;
IOB normals (-n0, -n2, -n1); sprite run format (no zero or overshooting runs in any shipped sprite); KAT lookup (binary search with
_strcmpi = case-insensitive find on sorted archives); MSVC rand() and the 1024-entry table.
