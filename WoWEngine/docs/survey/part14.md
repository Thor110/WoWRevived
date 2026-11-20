# Part 14: 0x4A14C0 to the end of .text (0x4A8A92), plus a survey of .rdata/.data

## 1. Overview

This piece is small. It holds the **last ~12 KB of game code** (83 functions, 0x4A14C0–0x4A4470), then a few CRT helpers, then compiler-generated exception-unwind funclets that run to the end of `.text`:

| Range | What | Rebuild? |
|---|---|---|
| 0x4A14C0–0x4A1CBF | Building-placement footprint (`sub_4A14C0`, a method of the vtable-0x4B1270 class whose ctor 0x4A13B0 is in part 13) | yes (battle map) |
| 0x4A1CD0–0x4A1F85 | `RTypeDotList`: the dots on the battle-map mini-map | yes (battle map) |
| 0x4A1F90–0x4A2197 | `RTypeMiniBattleMap`: the mini-map's backdrop picture `maps\hmin%02d.spr` / `mmin%02d.spr` | yes (battle map) |
| 0x4A21A0–0x4A2793 | `RTypeLine`: clipped Bresenham lines, 8 bpp and 16 bpp | yes (footprint and debug overlays) |
| 0x4A27A0–0x4A2A89 | Dynamic point light (vehicle light) and its world-position base class | yes (battle map lighting) |
| 0x4A2A90–0x4A31DA | 3x3 fixed-point (x4096) matrix maths for WOFLib: multiply, rotate, transform, tilt to the terrain normal | yes (3D models) |
| 0x4A31E0–0x4A3345, 0x4A3600–0x4A3813 | Two more `Setting<int>` / `Setting<float>` classes, bound to variables (the AI and route-clunting `[Tweak]`s, `[Demo] Playback Files`, `Minimum Audible Volume`) | replace with the engine's INI |
| 0x4A3350–0x4A35FF | `Stack`: a fixed-size stack of free ids (WSelectableObject ids, 32768 of them) | trivial |
| 0x4A3820–0x4A39DD | The `SfxL` sound-library reader (`SFX\SFX.WoW`, `VOX\HUMAN.WoW`, `VOX\MARTIAN.WoW`) | done in `core/Archive` |
| 0x4A39E0–0x4A3E4F | `WSectorPath`: a sector-to-sector A* path, cached per sector, which the resource code and the AI use | yes (war map, AI) |
| 0x4A3BD0–0x4A3CA2 | The war-map vehicle-unit class (save tag `VEHU`), types 0x3E–0x5F | yes (saves) |
| 0x4A3E60–0x4A421D | `InfoPage`: the "more information" popup for an object (text and speech) | yes (war map and battle map UI) |
| 0x4A4220–0x4A435A | Two small widget subclasses (formatted static text, captioned graphic push button) | yes |
| 0x4A4360–0x4A43F3 | An owner slot (a WCmdTarget that owns one child object), used by WResearch | yes |
| 0x4A4400–0x4A4470 | Game-order packets (0x0202 and 0x0707 records) and their "send" thunk | yes (orders, playback, network) |
| 0x4A4480–0x4A477F | CRT: `_qsort` 0x4A4480, `_shortsort` 0x4A4630, `_swap` 0x4A4690, `__eof` 0x4A46C0 | no (but see the qsort note in 2.3) |
| 0x4A478E–0x4A8A92 | 401 `SEH_<func>` unwind handlers (one per C++ function with destructible locals, from WinMain's 0x4A478E to `SEH_4A40A0` at 0x4A8A88), about 95 small `unknown_libname_*` unwind funclets, and a few FLIRT-misnamed "std::ios_base / strstreambuf" chunks | no |

**Hand-written assembly.** None found in this piece. Every function has a normal MSVC frame, stack arguments and SEH prologues. Even the fixed-point matrix routines (0x4A2A90, 0x4A2C50) and the line drawers (0x4A22C0, 0x4A23D0) take their arguments on the stack. Some instruction lists look odd only because of IDA variable names.

**IDA/FLIRT mislabels inside game code.** 0x4A3290 ("std::ios_base::~ios_base", 0x6B bytes, collapsed) is in fact the destructor of the int setting at vtable 0x4B1358. `unknown_libname_136…142, 200–202` are the game's own static-initialiser and `atexit` thunks. Don't treat them as library code.

**The useful finds from the data survey (section 3):**
- the game's RNG table: 1024 `rand()` values, picked by `sub_4025F0`;
- the 0x51A100 object-type table (0xB0 bytes per type);
- the 32768-entry object-id table and its free-id `Stack`;
- the sector adjacency matrix at 0x4EAA18;
- the complete settings inventory: **229 registry/INI values in 22 sections** (section 4).

## 2. Subsystems

Some names below are proposed (marked *proposed*); only `RTypeMiniBattleMap`, `InfoPage` and `Stack` appear as strings in the exe. "slot n" means vtable entry n.

### 2.1 Battle map: building placement footprint (part of the build-list "place building" mode)

The class is built by 0x4A13B0 (in part 13; vtable **0x4B1270**, base RType 0x49C080) with `(WLandscape* land, int objType)`. While the player is choosing where to put a building, it draws the building's outline on the terrain.

| Address | Name | What it does |
|---|---|---|
| 0x4A13B0 *(part 13)* | `PlacementGhost::PlacementGhost(land, type)` | +44 = landscape. Size = `(word_51A136[type*0xB0]*2 + 14) & ~15` (the type table's +0x36 footprint, rounded to 16), stored in +38 (width) and +3C (depth). For types 0x27, 0x28 and 0x29, flag +41 = 1 (passed to the can-build test; probably the waterside buildings, *unclear*). |
| 0x4A1460 *(part 13)* | slot 4, `OnClick` | When +40 (valid) is set, sends message **0x80A4** to the current window `dword_4EA448` (vtable+0x20, payload `{dword_4EA440, x(+30), y(+34), 0xDEADDEAD}`), then posts **0x8137** (leave placement mode). The build list catches 0x80A4 at 0x460E7C. |
| 0x4A14C0 | slot 5, `PlacementGhost::AddToDisplay(list)` | Gets the mouse position (0x42C6B0, 0x42DA10, 0x40E610). Snaps it to the landscape through 0x47B3F0 and averages it with the previous position (`+30/+34 = (old+new)/2`, a smoothing). Asks the landscape whether the type can be built there: `0x47B610(x0,y0,x1,y1, flag+41)` on map cells (world coords <<16 / land+0x30), stored in **+40**. Then walks the four edges of the footprint **every 16 world units**, sampling terrain height (`0x47B170`), and makes an `RTypeLine` (0x4C bytes, ctor 0x4A21F0) per segment. They are drawn **green** (`dword_4DAF80`, palette match of RGB 0,48,0) when it can be built and **red** (`dword_4DAF7C`, 48,0,0) when not. Each segment goes into the display list (`list->vtbl[0x18]`) with depth `+1E = y+16`. |

Projection constants in 0x4A14C0 (the isometric world-to-screen mapping):
- `screen_y = land+0x48 − h·0.69131 + y·0.70582`;
- the height factor is `h<<16 · 0x2C3E63FB >> 46`, i.e. ÷94800·65536;
- the y factor is `y<<16 · 0xB4B10D5D` with an add-back, i.e. ×0.70582 ≈ cos 45°.

Use the same constants as the landscape renderer (0x47B…) when rebuilding.

Globals: `dword_59D940` (the WLandscape instance; +0x24/+0x28 map size in cells, +0x30 cell size, +0x34 half cell, +0x64 cell array of 10 bytes per cell, whose byte 0 is the light level), `dword_4EA448` (the window that takes game commands), `dword_4EA440` (cursor/context value sent with 0x80A4, *unclear*). Settings that feed 0x47B610: `[Tweak] "Max Slope For Building On"` = 6 (0x47B4B1), `"No-build border width (vertices)"` = 16 (0x47B4F1).

### 2.2 Battle map: mini-map picture and unit dots

| Address | Name | What it does |
|---|---|---|
| 0x4A1F90 | `RTypeMiniBattleMap::RTypeMiniBattleMap(WORD map)` | vtable **0x4B12B0** (base RTypeBase 0x40E880). Builds `maps\hmin%02d.spr` (Human game, `sub_404BA0`) or `maps\mmin%02d.spr` with the battle-map number. Loads it whole with `FILE_Load` 0x415200 into a handle (+38, +3C = data). Width and height come from the file's first two words (+2A, +2E). The only caller is 0x48695F (the battle-map menu / mini-map at 0x485DE0–0x4868B0). |
| 0x4A20A0 | `~RTypeMiniBattleMap` | Frees the handle (0x414BC0, with the debug name string). |
| 0x4A2100 | slot 4, `Draw(surface)` | Draws frame 0 (offset at data+6) at the object's position, through the sprite blitter 0x40FD80. Uses shade level `0x40B500(slot 0x15, level 0)` and flag bit 0 of +14 → blend mode 2 (50%). |
| 0x4A1CD0 | `RTypeDotList::RTypeDotList()` | *proposed name*; vtable **0x4B1290**. +38 = count (WORD), +3C = 0x1000-byte buffer of up to **1024 dots**, each `{u8 x, u8 y, u16 colour}`. Built at 0x486A4D. |
| 0x4A1D50 | `~RTypeDotList` | |
| 0x4A1D70 | `Clear()` | count = 0. Called at 0x486CDF and 0x4870DE (each mini-map refresh). |
| 0x4A1D80 | `AddDot(x, y, colour)` | Ignored when full (0x400). Stores `colour | (colour & 0xFF) << 8`: for an 8-bit index that duplicates the byte. A real 16-bit colour gets its low byte ORed into the high byte; that is what the code does, so check what the caller passes. Called by the mini-map handler 0x486E80, message **0x8077**: world 16.16 position × mini-map size / (map cells × cell size). |
| 0x4A1DD0 | slot 4, `Draw(surface)` | Each dot is a **2x2 block** at (obj+x, obj+y), skipped when past the surface's right or bottom edge (no left/top test). 16 bpp writes the word; 8 bpp writes the low byte. |

### 2.3 Battle map renderer helpers: lines, lights, 3D maths

**RTypeLine** (*proposed name*), vtable **0x4B12D0**, 0x4C bytes. The footprint above uses it, and the vehicle code (0x48BEE9–0x48C0E1, ten calls) uses it, almost certainly for the `[Debug] Show Accelerations / Show Radii / Show Arrival` overlays.

| Address | Name | What it does |
|---|---|---|
| 0x4A21A0 | `RTypeLine(colour)` | Blank line with colour +38. |
| 0x4A21F0 | `RTypeLine(x1,y1,x2,y2,colour)` | +3C=x1, +44=y1, +40=x2, +48=y2, +38=colour. Bounding rect normalised by 0x4163A0 into +28..+2E; flag 0x10 set. |
| 0x4A22B0 | dtor | |
| 0x4A2720 | slot 4, `Draw(surface)` | Adds the object and surface offsets, then calls 0x4A24F0. |
| 0x4A24F0 | `ClipAndDrawLine(surf, x1,y1,x2,y2, colour)` | Cohen–Sutherland. Clip rect = surface `+46,+48` to `+4A−1,+4C−1`. Outcodes come from 0x4A26C0 (bit 1 left, 2 right, 4 top, 8 bottom, inferred). Calls the 8 bpp or 16 bpp Bresenham. |
| 0x4A26C0 | `OutCode(&code, x, y, l, t, r, b)` | |
| 0x4A22C0 | `Bresenham8(pixels, pitch, x1,y1,x2,y2, colour)` | Byte writes. |
| 0x4A23D0 | `Bresenham16(...)` | Word writes. The pitch is in pixels (index ×2). |

**Dynamic point light** (*proposed* `RTypeLight` / `VehicleLight`). The base class is vtable **0x4B1318** (ctor 0x4A2A20, base WObject 0x40E910): world position x, y, z WORDs at +38/+3A/+3C, radius +3E (`SetRadius` 0x4A2A80). Other users of the base are the ISO-object lights at 0x49DA75 and 0x49DDC3 (RTypeISO). The derived class is vtable **0x4B12F0**, ctor 0x4A27A0, made by the vehicle code at **0x48E050** (`[esi+4C]`). Its intensity comes from the per-type float `flt_4EC598[type*16]`.

| Address | Name | What it does |
|---|---|---|
| 0x4A2800 | `SetRadius(r)` | Sets +3E and the bounding box `(−r…r, −r…r)`, top `−32−r`; flags 0x18. |
| 0x4A2840 | `SetIntensityA(float)` | +40 = value. Recomputes +48 = `LightLevel(value)` (0x40CB10: `byte_4DA660[clamp(v·256, 0, 255)]`, a table filled by LoadShadeTables 0x40B570), using the terrain cell under the light (cell byte 0, `land+0x64 + (y·w+x)·10`). Called at 0x48E0D3 and 0x48E1CD. |
| 0x4A28E0 | `SetIntensityB(float)` | +44, the same recompute. |
| 0x4A2980 | slot 4 | Recomputes +48 and calls `landscape->ApplyLight(this)` (0x4A0140, part 13: lights the terrain vertices within the radius). |
| 0x4A2A20 / 0x4A2A70 | base ctor / dtor | |

Note that 0x40CB10 only reads its first argument. The callers push three floats (light value, +44, +40); this looks like a leftover from an inlined helper, so only the first matters.

**Fixed-point 3x3 matrices for WOFLib (3D models).** 4.12 format (0x1000 = 1.0); angles are 4096 to the circle (`dbl_4B1340` = 2π/4096). All callers are in WOFLib::WOFLib / draw at 0x49E6F6–0x49EB60.

| Address | Name | What it does |
|---|---|---|
| 0x4A2E00 | `MatrixFromAngles(short ang[3], int out[9])` | Uses `fsin/fcos`, then ×4096 and `_ftol` (truncates). The rotation order is *unclear*: check it against 0x49E729. |
| 0x4A2A90 | `MatMul(int A[9], short B[9], int out[9])` | `>>12` after each dot product. |
| 0x4A2C50 | `MatMul(int A[9], int B[9], int out[9])` | |
| 0x4A2FA0 | `Transform(int M[9], short v[3], int out[3])` | `>>12`. |
| 0x4A3020 | `TiltToTerrain(x, y, out[9], enable)` | When `enable` is 0, out = identity. Otherwise gets the landscape normal at (x,y) (0x47B240; z defaults to 0x1000 when 0) and builds an orthonormal basis (up = the normal, the others by cross products, normalised with `fsqrt` and integer division). Models sit tilted on slopes. |

**qsort.** The only game caller of `_qsort` (0x4A4480) is WOFLib 0x49ED32. It sorts 8-byte `{index, depth}` polygon records by depth (`CompareFunction` 0x49E8E0; culled polygons get depth 1000000). MSVC's qsort is **not stable**. For pixel-identical model drawing with equal depths, the rebuild needs MSVC's exact quicksort (0x4A4480, cut-off 8, then `_shortsort` 0x4A4630; check the pivot choice in the listing) or a deliberate fix.

### 2.4 Settings: variable-bound int and float settings

The game has a family of setting classes. All derive from 0x4166B0 (`Setting` base: +4 flag, +8 key, +C section, vtable 0x4A9A30). On the first use, 0x4166B0 also reads root `"Use INI"`, which picks `HKLM\SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\<section>` (registry, the default) or `WOW.INI` (`GetPrivateProfileString` with a bare file name, so the Windows directory). Read is 0x416770; write is 0x4168D0. This piece adds two classes:

| Address | Name | What it does |
|---|---|---|
| 0x4A31E0 | `IntSetting(int* var, section, key)` | vtable **0x4B1358**. The default is the variable's current value (itoa); reads it back with `atoi` (0x468A60). Users: `[Tweak] "Route clunting width"` (0x42AF34, var 0x4BAA14 = 4), `"Route clunting spacing"` (0x42AF74, 0x4BAA18 = 4), `[Demo] "Playback Files"` (0x42D2A4, 0x4BAAD8 = 4). |
| 0x4A3300 | slot 1 `Reload()` | Re-reads with no default. |
| 0x4A3320 | slot 2 `Save()` | Writes `itoa(*var)`. |
| 0x4A3290 | slot 0 dtor | Collapsed, misnamed `std::ios_base::~ios_base`; *probably* saves on exit like 0x4A3740. |
| 0x4A3600 | `FloatSetting(float* var, section, key)` | vtable **0x4B1388**. Default `sprintf("%f", *var)`, read with `atof`. Its 14 users are all the AI `[Tweak]`s at 0x4424E4–0x442824 (table in section 4). |
| 0x4A36B0 | `FloatSetting(float* var, key)` | Root section. Only user: `"Minimum Audible Volume"` (0x4646CF, var 0x4CA874 = 0.05). |
| 0x4A3740 | dtor | **Writes the value back** (`%f`) at exit (called from the `atexit` thunks `unknown_libname_141…`). So these AI tweaks end up in the registry or INI after the first run. |
| 0x4A37C0 / 0x4A37E0 | slot 1 `Reload` / slot 2 `Save` | |

All the setting classes and their type codes (for the INI rebuild):

| Address | Kind | Default | vtable |
|---|---|---|---|
| 0x411540 | bool | pushed | 0x4A99D8 |
| 0x4115E0 | bool, root | pushed | 0x4A99D8 |
| 0x411660 | bool, root | none | 0x4A99D8 |
| 0x412160 | int | pushed | 0x4A99F8 |
| 0x416500 | float | pushed | 0x4A9A20 |
| 0x4169A0 | string | pushed | 0x4A9A40 |
| 0x411810 / 0x4117A0 / 0x411890 / 0x4118D0 / 0x411910 / 0x411950 | bound int (type 0) / dword (1) / bool (3) / float (4) / size "w,h" (5) / point (6) | the bound variable | 0x4A99E4, reader 0x4119A0 |
| 0x4A31E0 / 0x4A3600 / 0x4A36B0 | bound int / float / float (root) | the bound variable | this piece |

### 2.5 Free-id stack (`Stack`)

| Address | Name | What it does |
|---|---|---|
| 0x4A34C0 | `Stack::Stack(n)` | Named base `"Stack"` (0x435640); vtable **0x4B1378**. +18 = capacity, +14 = `new int[n]`, +10 = top; empty slots are filled with **0xDDDDDDDD**. |
| 0x4A3350 | `IdStack::IdStack(n)` | vtable **0x4B1368**. After clearing, pushes `n−1 … 0`, so ids come out from 0 upwards. The one instance is **0x518748, n = 0x8000** (static init 0x4400C0). It hands out ids for WSelectableObjects: 0x440120 pops an id into `obj+8` and sets **`dword_4F8748[id] = obj`** (32768 pointers, 0x4F8748–0x518747; lookup 0x440110). Ids are pushed back when objects die (0x4402C8, 0x4404DF). |
| 0x4A3590 | `Push(v)` | Fails when full. |
| 0x4A35C0 | `Pop()` | No underflow check. |
| 0x4A35E0 | `Clear()` | |
| 0x4A3420 | `Replace(old, new)` | Linear search. |
| 0x4A3460 | debug check at exit | Builds a byte map of the ids in use and throws it away; it does nothing. |
| 0x4A3570 / 0x4A33E0 | dtors | |

For a faithful rebuild, **ids are allocated lowest-free-first after a fresh start, and LIFO afterwards**. Save files store these ids (`SELO`), so the rebuild has to reproduce the allocation order.

### 2.6 `SfxL` sound-library reader

| Address | Name | What it does |
|---|---|---|
| 0x4A3860 | `SfxLib::Open(path)` | Clears +20..+30, copies the path (0x466910 builds it from `SFX\SFX.WoW`, `VOX\HUMAN.WoW`, `VOX\MARTIAN.WoW`), calls 0x4A3920. The three instances are at **0x589840, 0x5896E8, 0x58FE40** (static init 0x466750). |
| 0x4A3920 | `ReadDirectory()` | `CreateFileA(GENERIC_READ, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS|READONLY)`. 8-byte header: magic **`'SfxL'` (0x4C786653)** and count. Then `count × 16`-byte entries `{char name[8]; u32; u32}`, kept at +24, count at +2C, handle at +20. Errors: "Unable To Read Header", "Unable To Read File Entries" (to 0x4164A0). Returns false when the magic is wrong. |
| 0x4A3820 | `SfxKey(name)` | Uppercases the name up to `.` or 8 characters into two dwords; the rest are zero. |
| 0x4A38C0 | `Find(key)` | Linear search comparing the two dwords; returns the entry pointer or 0. Callers: 0x466819, 0x46685A, 0x46689B (one per library). |

WoWEngine's `core/Archive.cpp` already reads SfxL (entry `{name[8], length, offset}`). The one difference: the game's key stops at the first `.` and is exact after uppercasing (`"ABC.WAV"` → `ABC`), so check that the engine's name lookup strips extensions the same way.

### 2.7 War map: sector-to-sector paths (`WSectorPath`, *proposed*)

Each sector object (the WCmdTarget whose handler is 0x43ADA0, +48 = sector number) keeps **30 cached paths at +0xA0** (`GetPath(n)` 0x43C090 returns `[this + n*4 + 0x9C]`, n = 1..30). When it gets **WM_COMMAND 0xB7**, it deletes them and rebuilds `new WSectorPath(owner, this, n)` for n = 1..30 (0x43AFEF–0x43B04D). The war map (WWarMapWorld) sends 0xB7 to all 30 sectors:
- at once, from 0x418890, called by 0x423B00 and 0x42A980 (after ownership or command-post changes);
- or staggered, from 0x4192A0(false): sector n is told after n ticks (0x4355A0 timer).

| Address | Name | What it does |
|---|---|---|
| 0x4A39E0 | `WSectorPath(side, from, to)` | vtable **0x4B1394**, 0x10 bytes: +4 = `int sectors[count]` (entry 0 left 0), +8 = count (including the start), +C = **float length** (the sum of `sqrt` distances between consecutive sector waypoints, 0x437380 / table 0x4BAE68). Built with the A* 0x44EE70: node table **0x4BD260** (31 nodes), the cost object below, from, to, at most **1000** iterations. Run by 0x44EFA0. When the result's +4 is not 1 or the path has ≤1 node, count stays 0. |
| 0x4A3CB0 | `SectorPathCost(side, from(ignored), to)` | vtable **0x4B13C0** (base 0x44F280): +4 = side, +8 = goal, +C = arg 0. |
| 0x4A3D10 | slot 1 `Cost(a, b)` | Returns 0 when a == b. Otherwise returns −1 (impassable) when **`byte_4EAA18[a*32+b]` bit 0** is clear (no land link). Returns −1 when b's owner (`dword_4EAE1C[b*0xC0]`) is neither `side` nor 2 (no one). Returns −1 when neither a nor b has a finished command building (0x43C990). Returns −1 when b has none and b is not the goal. Otherwise returns `(int)sqrt(dx²+dy²)` between waypoints. |
| 0x4A3E40 | slot 2 `IsGoal(a, b)` | `a == b`. |
| 0x4A3D00 | dtor | |

These are the same rules as the player's route check 0x42ABD0 (already in `WarMap::FindRoute`), **minus** the unit-kind (land/sea) and the "Max units/boats in sector" tests. They also only follow land links (bit 0).

Users of the cached paths, all through 0x43C090:
- the war map's resource / supply computation 0x41A760, which uses `path+8` (sector count) with `flt_4F86B4` and `flt_4F740C`, near `[Tweak] "Distance Crippler VX/VY"` 0.5/0.5 and `"Resource Buffer"` 30;
- **the AI** at 0x4437CF, 0x4439C8, 0x443D1D, 0x443DD2, 0x443F15, 0x444469 and 0x444D8F.

So rebuilding resource flow or the AI needs this.

### 2.8 War map: vehicle-unit object (save tag `VEHU`)

The object factory 0x446950 makes, for war-map object types **0x3E–0x5F** (unit types), a 0x78-byte object: `0x4A3BD0(type)` → base 0x428BB0 (the war-map object, "BMOList"), vtable **0x4B1398**. The overrides:

| Address | What it does |
|---|---|
| 0x4A3C20 | Pass-through to 0x428D70 (tooltip / info text and stats). |
| 0x4A3C40 | Pass-through to 0x429F20. |
| 0x4A3C50 | Pass-through to 0x429FC0. |
| 0x4A3C60 | `Load(stream)`: base 0x42A190, then reads one extra dword (stream vtbl+0x60). |
| 0x4A3C80 | `Save(stream)`: base 0x42A220 (which writes `WMOB` …), then writes **`'VEHU'` (0x55484556)** (stream vtbl+0x34). |
| 0x4A3C10 | dtor → 0x428CB0. |

`core/SaveGame` already lists the `VEHU` tag.

### 2.9 InfoPage (an object's "information" popup with speech)

| Address | Name | What it does |
|---|---|---|
| 0x4A3E60 | `InfoPage::InfoPage(stringId, x, y)` | Window class `"InfoPage"`, created by 0x42F730 as a child of `dword_4EA448` with style 0x10000000 (WS_VISIBLE), vtable **0x4B13D0**, 0x88 bytes. +7C = string id; layer `+44 = 0x65FE`. Size from 0x402970 (the screen size); 0x42E9B0(1) makes it modal or captures input (*unclear*). The text is `TEXT.ojd[stringId]` (0x4146F0), wrapped by 0x4161E0 to **width 256** in **font 0x73**. It goes into a 0x2AC-byte text object (0x49D260, with 0x441A40), coloured `[Colours] "Info Page Text"` (`word_4B9424`, default **15**), with margins 4,4,3,3 (0x4163C0). It is placed at **(x+32, y+32)** and pushed inside the world view rect (`dword_4EA448` +34/+38/+58/+5C, by 0x416400). Then it starts **timer 0x812C** after **15 ticks** (0x4355A0(0x113, 0x812C, 0, 15)). |
| 0x4A4190 | slot 7 `WndProc` | On `WM_TIMER 0x812C`: plays the **speech for the same string id** (0x414700: TEXT.ojd entry +4 = sound id, played by 0x464EE0 at volume 1.0) and keeps it in +84. It **closes** on WM_KEYUP (0x101), WM_LBUTTONUP (0x202) or WM_RBUTTONUP (0x205), through 0x42F2A0. Anything else goes to 0x42E290. |
| 0x4A4160 | slot 9 `Paint(list, …)` | Adds the text object (+80) to the display list, then calls the base 0x42E150. |
| 0x4A40A0 / 0x4A4080 | dtor / deleting dtor | Releases the text, **fades the speech out over 30 ticks** (0x464EB0(0x1E)) and releases it. |

The one creator is **0x4403A0**, the WSelectableObject "info" handler. It opens the page when the object's type has a non-zero info string id `dword_51A108[type*0xB0]` (`type+8`), at the object's screen position (0x440440). 0x440300 dispatches it from message 0x111. The same table's +4 (`dword_51A104`) is the type's name string for tooltip text (message 0x807C, arg 2).

### 2.10 Small UI subclasses and the owner slot

| Address | Name | What it does |
|---|---|---|
| 0x4A4220 | `FormattedLabel(fmt, a, x, y, w, h)` | *proposed*. Formats with 0x416230 (into the 1 KB static buffer 0x4E9258), then builds the static-text control 0x430500; vtable **0x4B1410**. Six callers in the research and blueprint screens (0x45D5EF–0x45E1D0). |
| 0x4A4280 | dtor | Misnamed `ios_base` dtor thunk. |
| 0x4A4290 | `CaptionButton(…, captionStyle)` | *proposed*. WGraphicPushButton (0x42FA20) plus +90 = a text style id; vtable **0x4B1450**. Caller: 0x484159 (battle-map menu). |
| 0x4A4300 | slot 9 `Paint` | Moves the caption object (+80) to the button's x/y + offset, applies style +90 (0x40F000), sets layer = button layer + 2, and adds it to the list. |
| 0x4A42F0 | dtor | |
| 0x4A4360 | `OwnerSlot(parent)` | *proposed*. Base 0x4354C0 (WCmdTarget); vtable **0x4B1490**; +1C = an owned object. |
| 0x4A43D0 | `Set(obj)` | Deletes the old object first. |
| 0x4A43F0 | `Get()` | 12 callers in WResearch 0x458266–0x45E58A. |
| 0x4A43B0 | `Destroy` | Slot 6: base 0x435570, then deletes +1C. |
| 0x4A43A0 / 0x4A4380 | dtors | |

### 2.11 Game-order packets (the command stream)

Every player order is serialised into a small record and appended to the command buffer (0x438C50 copies `size` bytes to `buf+28` and advances `+2C`). That buffer is very likely what goes over DirectPlay and into `playback.%d` (*inferred*). Record header: `[0] type, [1] = type (duplicated), [2] code, [3] size`.

| Address | Record | Payload |
|---|---|---|
| 0x4460A0 *(not this piece)* | base, type 0x0101, size 4 | Sets the target buffer `dword_566260`. |
| 0x4493E0 *(not this piece)* | type 0x0404, size 8 | dword. |
| 0x4A4440 | type **0x0202**, size **8** | `+4 = dword`. Users: the battle map at 0x485CA0 and 0x485CCC, codes **0x13** and **0x0E**, chosen by Ctrl (`GetAsyncKeyState(VK_CONTROL)`) = force fire; puppets 0x497805. |
| 0x4A4400 | type **0x0707**, size **0x20** | 24 bytes copied from arg 3. Built by the build list at 0x4601DE (queue or build order). |
| 0x4A4470 | `Send()` | Thunk to 0x4460C0 → 0x438C50(record, size). 5 callers: 0x42C1C2, 0x43C469, 0x485661, 0x485CA9/CD5, 0x49780E/873. |

## 3. Data tables in .rdata / .data

Layout:
- `.idata` + `.rdata`: 0x4A9000–0x4B7552. Imports, then from 0x4A92B8: vtables, float/double constants, CRT tables and the import directory at 0x4B677C.
- `.data`: 0x4B8000–0x5A0EFC; above about 0x4D1000 most of it is BSS.
- `seg004` at 0x5A1000 (0x94 bytes).

Sizes below are "bytes to the next label" unless the element count is known from code, so treat them as upper bounds.

### 3.1 Tables a rebuild needs (game logic and data)

| Address | Size / layout | What it is | Users |
|---|---|---|---|
| **0x4D14A8** | 1024 × int (to 0x4D24A8) | **The game RNG table.** Filled once with CRT `rand()` at start-up (0x402780 loop, in 0x402630). `Random(n)` = **0x4025F0**: `table[idx++] % (n+1)`, index `dword_4D2580` wrapping at 0x400. Seeds: `srand(timeGetTime())` in WinMain 0x401AE8, **`srand(1)` in the WWarMapWorld ctor** 0x417DA3. Direct `rand()` is also used by the war map (0x41A360, 0x41B17C, 0x422738, 0x42B34B) and the battle-map set-up (0x43A6A2). | 0x4110AF (ISO), 0x48898E, 0x48C6B0–0x48CF73 (vehicles), 0x4953C0 (puppets) |
| **0x51A100** | 0xB0 bytes per object type (index = type id; the WSelectableObject class tables) | **Object type table**, filled by the OBJ.ojd loader 0x437750. Fields seen: +4 name string id (0x440300), +8 info-page string id (0x4403A0), +0x20.. bytes (+0x20 is passed to 0x4354C0 as the type's flags), +0x36 footprint size (placement), +0x4E move class (WARMAP.md), +0x50..+0x64 dwords read by 0x428D70 (stats). | everywhere (167+ xrefs to 0x51A1xx) |
| 0x518B14 | 8 bytes per entry | AI.ojd per-type class (`[i*8] == 7` → defence class), loaded by 0x441580. | 0x41EE69, 0x42061E, 0x42A52A |
| **0x4F8748** | 32768 × ptr | Object id → WSelectableObject*; free ids in `Stack` 0x518748 (2.5). | 0x440110, 0x440120 |
| 0x4DAFD0 / 0x4DAFE8 | 56 bytes per GType | The GType (graphics) table (ENGINE-MAP). 0x40E980 loads on first use ("Can't load file '%s'"). | 0x40E7A0, 0x40E980 |
| **0x4EAA18** | 32 × 32 bytes | **Sector link matrix** `[a*32+b]`. Built by 0x4357E0 (a switch over sector pairs). Bit 0 = land link; whenever bit 0 is set, bits 1–2 are set too (`or al,6`, meaning *unclear*: probably sea/air passable). 0x4356EF tests `&7`. | 0x42ABD0 (player routes), 0x4A3D10 (sector paths) |
| 0x4EAE1C | 0xC0 bytes per sector, 31 entries | Sector table: +0 owner (0 Human, 1 Martian, 2 no one), resources… (WARMAP.md). | 0x43CA30, 0x43C990, 0x4A3D10, 0x43B02E |
| 0x4BAE68 / 0x4BAE78 | 32 bytes per sector | Sector bounds and waypoints (WARMAP.md); waypoint reader 0x437380. | |
| 0x4BCEB0 | ptr per sector → 36-byte links | Route links (WARMAP.md). | 0x4244E0 |
| **0x4BD260** | 31 ptrs → node records (from 0x4BCF2C) | A* node list for sector routing. | 0x424160 (player routes), 0x4356C0, 0x4A39E0 |
| 0x4D0C00 | 8 border numbers per sector | Borders (WARMAP.md). | 0x49BE70, 0x49BF50 |
| 0x4BA0E8 | 12 bytes per event | War-map events: handler, "critical" test, name (WARMAP.md). | |
| 0x4B9200 | per cursor: GType + offset | Cursor table (FRONTEND.md). | 0x40CD30 |
| 0x4B9A30 | 256 bytes | ANSI → CP437 font map (`core/Charset.h`). | 0x414410 |
| 0x4DA660 | 256 bytes | Light level → shade level, built by 0x40B570 (`dat\LANDR%d%s.int`). | 0x40CB10 |
| 0x4DAF7C–0x4DAF98 | 8 dwords | Palette indices for fixed colours, matched by 0x40B3B0 in 0x40B120: red (48,0,0), green (0,48,0), blue (0,0,48), black, white (63), grey 48, 32, 16 (RGB 0–63). Used for the footprint and debug lines. | 0x4A14C0, 0x4A0630 |
| 0x4DAFC8 | dword | Another palette-matched colour from 0x40B120, passed as the text colour by the route arrival labels and InfoPage. | 0x4244E0, 0x4A3F51 |
| 0x4CA858–0x4CA870 | 6 floats + master | `[Sound\Volume]` Ambient, Engine, Explosions, Weapons, Special, Speech, Master: all **1.0**. 0x4CA874 = `Minimum Audible Volume` **0.05**. | 0x4642C0… |
| 0x58FEF5 (0x58FE..) | byte per sound id | Sound table flags, written by the SFX.ojd loader 0x463D10 and read by `PlaySound` 0x464EE0. | |
| 0x5898D0 | ≈26 KB | Sound object pool (0x54-byte sound objects, 0x4648C0). | 0x465490 |
| 0x589840 / 0x5896E8 / 0x58FE40 | SfxLib objects | `SFX`, `VOX\HUMAN`, `VOX\MARTIAN` libraries (2.6). | |
| 0x4EC598 | 16 bytes per unit type, float at +0 | Per-type light intensity for the vehicle light. | 0x48E0B7 |
| 0x4F0538 | float per type (10.5 KB) | Per-type float from OBJ.ojd (0x437A7A), read by the vehicles (0x4900CE) and the route finder (0x49349C); probably the speed (*guess*). | |
| 0x4F2F1E | word per type | OBJ.ojd per-type word (0x437D0A), read by 0x48899D. | |
| 0x5888C8 | ~736 bytes | Written by the OBJ.ojd loader (0x437750+DCC), read by the research screen 0x45D1A0 (*guess*: research items). | |
| 0x51A1A0, 0x51C8D0, 0x51F370 | three blocks | Build-list tables walked by 0x45F970 (a loop from 0x51A1A0 to 0x51F370, *unclear*). | |
| 0x51DC0C, 0x52BACC, 0x53885C | byte tables (6 KB, 35 KB, 48 KB spans) | Research state tables (0x45A1C0, *unclear*: probably per side, item and level). | |
| 0x546230 | dword array (~57 KB span) | AI working table (0x442EC0 / 0x443030 / 0x443610). | AI |
| 0x566280 | dword stack, top `dword_4BEFDC` | A global push/pop stack (0x449B10 push, 0x449B30 pop); purpose *unclear* (in the sector-stats / WCmdTarget block). | |
| 0x4EA614 | dword | Fog-of-war map resolution divisor. | 0x4346C0, 0x43A9B8, 0x48C855 |
| 0x59D9C0 | 256 × ptr | Battle-map object slot table (0x439770 set-up, 0x486490 add/remove; *guess*). | |
| 0x59DE18 / 0x59DFB8 / 0x59E470 | pool headers | "512 WTransientObjects", "256 WPuppetHotSpot", … allocator blocks. | 0x487610, 0x487910, 0x496C40 |
| 0x4D2658 | 16 bytes per entry | Layer / draw-queue table for RTypeBase (0x40E300); *unclear*. | |
| 0x586EB8 | 0x19D8 bytes | Buffer used by the new-campaign / save code 0x4043D0 (*guess*: save header and name). | WGame saves |
| 0x4C0B5F, 0x4C0D80, 0x4C10CC, 0x4C12CC, 0x4C20FC, 0x4BFD8A, 0x4C074E, 0x4C02A6 | lookup tables | Tables used by the hand-written blitters 0x44F…–0x456… (*guess*: blend modes, jump tables, colour splits; not traced). | 0x450836, 0x456CA0, 0x456DD0, 0x4566FE |

### 3.2 vtables defined for this piece's classes

| vtable | Class | Slots of note |
|---|---|---|
| 0x4B1270 | PlacementGhost | 4 = 0x4A1460 (click), 5 = 0x4A14C0 (add to display) |
| 0x4B1290 | RTypeDotList | 4 = draw 0x4A1DD0 |
| 0x4B12B0 | RTypeMiniBattleMap | 4 = draw 0x4A2100 |
| 0x4B12D0 | RTypeLine | 4 = draw 0x4A2720 |
| 0x4B12F0 / 0x4B1318 | VehicleLight / light base | 4 = 0x4A2980, 8 = 0x4A2A80, 9 = 0x4A2800 |
| 0x4B1358 | IntSetting | 1 = 0x4A3300, 2 = 0x4A3320 |
| 0x4B1368 / 0x4B1378 | IdStack / Stack | |
| 0x4B1388 | FloatSetting | 1 = 0x4A37C0, 2 = 0x4A37E0 |
| 0x4B1394 | WSectorPath | |
| 0x4B1398 | WM vehicle unit (VEHU) | 1 = 0x4A3C60, 2 = 0x4A3C80, 4 = 0x4A3C40, 5 = 0x4A3C50, 6 = 0x4A3C20 |
| 0x4B13C0 | SectorPathCost | 1 = 0x4A3D10, 2 = 0x4A3E40 |
| 0x4B13D0 | InfoPage | 7 = 0x4A4190, 9 = 0x4A4160 |
| 0x4B1410 / 0x4B1450 / 0x4B1490 | FormattedLabel / CaptionButton / OwnerSlot | 9 = 0x4A4300 (CaptionButton paint) |

### 3.3 String literals that reveal subsystems

- **Settings**: `SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'` (0x4B84D0, under HKLM), `WOW.INI` (0x4B850C), `Use INI`. All values are in section 4.
- **Class and debug names**, which mark source-file boundaries:
  - `WRouteFinder::` (11 strings), `WGame::`, `DeeSound::DuplicateSoundBuffer failed DSERR_*`, `DeePlay::RECEIVED packet # %d of %d bytes on cycle %d`, `DeeDraw::`, `Win95DeskTop::PlaybackMessage %s`;
  - `WOFLib::WOFLib: Inconsistent number of vertices`, `WLandscape::`, `WFogOfWar::WFogOfWar: no mem for fog map`, `PolyLib::`, `RTypeISO::Change`, `RTypeBase`;
  - `WWarMapRoute::~WWarMapRoute`, `WText::`, `WScrollbar::`, `WResearch::`, `WGraphicPushButton::SendCommand( %d, %d )`, `WDamage::~WDamage`;
  - `Star::~AStar` / `AStar:1`, `AStar:2`, `FindRoute: No memory for route list nodes`, `Heap::~Heap`, `RTypeMiniBattleMap::~RTypeMiniBattleMap`, `RTypeArrow`, `ISOLib::`, `BMOList`, `InfoPage`, `Stack`.
- **Pools**: `Allocating memory block for 1024 RType??? objects` (0x40E700), `… 512 WTransientObjects??? objects` (0x487610), `… 256 WPuppetHotSpot objects` (0x496C40).
- **Files**:
  - `Text.OJD`, `obj.ojd`, `AI.ojd`, `sfx.ojd`, `SFX\SFX.WoW`, `VOX\HUMAN.WoW`, `VOX\MARTIAN.WoW`, `dat\dat.wow`, `maps\maps.wow`;
  - `dat\%d.nsb` (battle map set-ups), `maps\hmin%02d.spr` / `mmin%02d.spr`, `.ils` (landscape);
  - `SaveGame\%s.%03d`, `playback.%-d`, `Playback.1`, `FMV\*.smk` / `.fmv`;
  - the 40-odd shade-table patterns `dat\F1F1.%s` … `dat\BMR%dNA%s.%s`, `dat\LANDR%d%s.int` (0x40B570–0x40B6C0);
  - `Martian.cd` / `Human.cd`.
- **Sound definition parser**: `Ignoring CONSTANTVOL setting in SFX.def`, `LoadSetupData` (0x463D10). SFX.ojd was compiled from a text `SFX.def`.
- **Errors**: `INVALID DATA IN TEXTFILE` (AI.ojd), `LoadShadeTables: Invalid palette ID (%d)`, `Unable To Read Header`, `Unable To Read File Entries`.
- **Network**: `War of the Worlds - session name` (`[Network] Session Name` default).

## 4. All registry / INI settings

These are all the settings found in the exe: 229 values in 22 sections. They were extracted by scanning every constructor call of the setting classes (2.4).
- **Default** is the value pushed at construction or the bound variable's initial value. "bss" means the variable starts at 0 and is set by code.
- **Object / var** is the setting object (`obj`) or, for bound settings, the variable it reads into (`var`).
- **At** is the constructor call.

Kinds: bool, int, float, string; `dword` = colour index or count (type 1); `size` / `point` = two ints `"a,b"`. The base is HKLM `SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\<Section>`, or `WOW.INI` when root `Use INI` is set.

Notes:
- `[Stats]` values are **written** by the game (counters for debugging), not read.
- `Game Frequency` (root, 30) is the tick rate. `Game max/min time (ms)` 25/20 bound the frame time.
- `Charlie`, `Dave` and `Mat` are developer debug switches.
- `[Tweak]` floats at 0x4424E4–0x442824 and the bound ints are written back at exit (2.4).

| Section | Key | Kind | Default | Object / var | At |
|---|---|---|---|---|---|
| (root) | Enable Network Version | bool | 0 (bss, set at run time) | var byte_4D12FC | 0x401701 |
| (root) | Game Frequency | int | 30 | var dword_4B84C8 | 0x401781 |
| (root) | Thread Enable | bool | 1 | obj unk_4D1200 | 0x40183C |
| (root) | Timer Enable | bool | 0 | obj unk_4D1280 | 0x40186C |
| (root) | Sleeper Enable | bool | 0 | obj unk_4D12B8 | 0x40189C |
| (root) | Skip | bool | (no default pushed) | obj unk_4D1350 | 0x40258A |
| (root) | Use INI | bool | 0 (read once, local) | 0x4166B0 local | 0x41670E |
| (root) | Full Screen | bool | 1 | obj unk_4EA350 | 0x42C46C |
| Screen | BPP | dword | 16 | var dword_4BAAC4 | 0x42C554 |
| (root) | Minimum Audible Volume | float | 0.05 | var dword_4CA874 | 0x4646CF |
| BattleMap | Damage reduction divisor | int | 500 | obj unk_4D2588 | 0x404E74 |
| BattleMap | EnableFogOfWar | bool | 1 | obj unk_4EA570 | 0x4344D1 |
| BattleMap | Fog darkness | float | 0.7 | obj unk_4EA5E8 | 0x434514 |
| BattleMap | Fog return rate | int | 5 | obj unk_4EA5A0 | 0x434551 |
| BattleMap | Fog update freq | int | 15 | obj unk_4EA600 | 0x434591 |
| BattleMap | Fog smooth index | int | 128 | obj unk_4EA588 | 0x4345D4 |
| BattleMap | Fog vert dropoff | int | 3 | obj unk_4EA5D0 | 0x434611 |
| BattleMap | Fog damage delta | int | 0 | obj unk_4EA558 | 0x434651 |
| BattleMap | Fog vert damage radius | int | 16 | obj unk_4EA5B8 | 0x434691 |
| Briefing | Delay | int | 30 | obj unk_59E628 | 0x499901 |
| BuildList | Top | int | 70 | obj unk_589160 | 0x460A91 |
| BuildList | Left | int | 0 | obj unk_589140 | 0x460AD1 |
| Colours | Menu Heading | dword | 7 | var dword_4B93EC | 0x40F074 |
| Colours | Menu Items | dword | 4 | var dword_4B93F0 | 0x40F0B4 |
| Colours | Menu Glow First | dword | 4 | var dword_4B93F4 | 0x40F0F4 |
| Colours | Menu Glow Last | dword | 9 | var dword_4B93F8 | 0x40F134 |
| Colours | Menu Static Text | dword | 4 | var dword_4B93FC | 0x40F174 |
| Colours | Research Current | dword | 25 | var dword_4B9400 | 0x40F1B4 |
| Colours | Research Normal | dword | 26 | var dword_4B9404 | 0x40F1F4 |
| Colours | Research New | dword | 27 | var dword_4B9408 | 0x40F234 |
| Colours | Research Heading | dword | 17 | var dword_4B940C | 0x40F274 |
| Colours | Research Proposal | dword | 26 | var dword_4B9410 | 0x40F2B4 |
| Colours | Research Detail | dword | 26 | var dword_4B9414 | 0x40F2F4 |
| Colours | Research Specification | dword | 26 | var dword_4B9418 | 0x40F334 |
| Colours | Research Enhancement | dword | 26 | var dword_4B941C | 0x40F374 |
| Colours | Message Text | dword | 19 | var word_4B9420 | 0x40F3B4 |
| Colours | Info Page Text | dword | 15 | var word_4B9424 | 0x40F3F4 |
| Colours | Messagebox Detail | dword | 21 | var word_4B9428 | 0x40F434 |
| Colours | Messagebox Command | dword | 22 | var word_4B942C | 0x40F474 |
| Colours | Briefing Sector Name | dword | 17 | var word_4B9430 | 0x40F4B4 |
| Colours | Briefing Heading | dword | 17 | var word_4B9434 | 0x40F4F4 |
| Colours | Briefing Detail | dword | 18 | var word_4B9438 | 0x40F534 |
| Colours | Credits Heading | dword | 17 | var unk_4B943C | 0x40F574 |
| Colours | Credits Detail | dword | 6 | var unk_4B9440 | 0x40F5B4 |
| Debriefing | Delay | int | 300 | obj unk_59E648 | 0x49A784 |
| Debug | Charlie | bool | 0 | obj unk_4D1250 | 0x401591 |
| Debug | Dave | bool | 0 | obj unk_4D1220 | 0x4015D1 |
| Debug | Mat | bool | 0 | obj unk_4D1268 | 0x401611 |
| Debug | Edit functionality | bool | 0 | obj unk_4D1238 | 0x401651 |
| Debug | Save File ID | int | 5 | obj unk_4D1418 | 0x4025C1 |
| Debug | Show Mem Dump On Alloc Failure | bool | 0 | obj unk_4E9120 | 0x414751 |
| Debug | Ignore fresher files | bool | 0 | obj unk_4E9208 | 0x414DB1 |
| Debug | Log Debug Output | bool | 0 | obj unk_4E9870 | 0x417C01 |
| Debug | Load scenery | bool | 1 | obj unk_4F73E0 | 0x439451 |
| Debug | Enemy Visible | int | 0 | obj unk_546038 | 0x442861 |
| Debug | Arrow scenery load fix | bool | 1 | obj unk_59D960 | 0x47CD01 |
| Debug | Show Accelerations | bool | 0 | obj unk_59E3E8 | 0x490771 |
| Debug | Show Radii | bool | 0 | obj unk_59E208 | 0x4907B1 |
| Debug | Show Arrival | bool | 0 | obj unk_59E340 | 0x4907F1 |
| Debug | Show Orders | bool | 0 | obj unk_59E420 | 0x496531 |
| Demo | Playback Files | int | 4 | var dword_4BAAD8 | 0x42D2A4 |
| Event | Construction Completed | bool | 0 (bss, set at run time) | var byte_4EA088 | 0x41E174 |
| Event | Unit Manufactured | bool | 0 (bss, set at run time) | var byte_4EA08C | 0x41E1B4 |
| Event | Command Centre Built | bool | 0 (bss, set at run time) | var byte_4EA090 | 0x41E1F4 |
| Event | Constructor Idle | bool | 1 | var byte_4BA0D0 | 0x41E234 |
| Event | Facility Idle | bool | 1 | var byte_4BA0D4 | 0x41E274 |
| Event | Unit Arrived At Destination | bool | 0 (bss, set at run time) | var byte_4EA094 | 0x41E2B4 |
| Event | Travel Interrupted | bool | 1 | var byte_4BA0D8 | 0x41E2F4 |
| Event | Research Completed | bool | 1 | var byte_4BA0DC | 0x41E334 |
| Event | Resource Warning | bool | 0 (bss, set at run time) | var byte_4EA098 | 0x41E374 |
| Event | Resource Alert | bool | 1 | var byte_4BA0E0 | 0x41E3B4 |
| Event | Entered Neutral Sector | bool | 0 (bss, set at run time) | var byte_4EA09C | 0x41E3F4 |
| Fun | Veh Speed | int | 1 | obj unk_59E370 | 0x4916B1 |
| Fun | Veh Rot Speed | int | 32 | obj unk_59E3D0 | 0x4916F1 |
| Game | Time Of Day | int | 0 | obj unk_4DA808 | 0x40B001 |
| Game | ISO Brightness | float | 0.5 | obj unk_4E8BF0 | 0x410E14 |
| Game | WOF Brightness | float | 2.5 | obj unk_59F350 | 0x49E304 |
| Network | I Am Human | bool | 1 | var byte_4B84C4 | 0x401744 |
| Network | Messages Guaranteed | bool | 0 | obj unk_4D1338 | 0x402551 |
| Network | Session Name | string | "War of the Worlds - session name" | obj unk_5862A0 | 0x44A5C4 |
| Options | Show lights | bool | 1 | var byte_4B8808 | 0x4023D4 |
| Options | Show Smoke & particles | bool | 1 | var byte_4B880C | 0x402414 |
| Options | Show gouraud shading | bool | 1 | var byte_4B8810 | 0x402454 |
| Options | Show vehicle shadows | bool | 1 | var byte_4B8814 | 0x402494 |
| Options | Show simple tooltips | bool | 1 | var unk_4B8818 | 0x4024D4 |
| Research | Human Open Rate | int | 20 | obj unk_5888A8 | 0x457F31 |
| Research | Martian Open Rate | int | 10 | obj unk_588890 | 0x457F71 |
| Screen | Use DDraw surfaces | bool | 1 | var byte_4B92D8 | 0x40CE14 |
| Screen | BLIT instead of FLIP | bool | 1 | var byte_4B92DC | 0x40CE54 |
| Screen | Damage map VRAM | bool | 0 (bss, set at run time) | var byte_4DAF78 | 0x40CE94 |
| Screen | Scroll Speed | int | 40 | var dword_4BA058 | 0x41DF54 |
| Screen | Scroll Border | int | 4 | var dword_4BA05C | 0x41DF94 |
| Screen | Top left | point | 0 (bss, set at run time) | var dword_4EA340 | 0x42C4B4 |
| Screen | Size | size | 0 (bss, set at run time) | var dword_4EA370 | 0x42C514 |
| Screen | Support screen size | size | 0 (bss, set at run time) | var dword_4EA3D8 | 0x42C5B4 |
| Screen | Support screen BPP | dword | 16 | var dword_4BAAC8 | 0x42C5F4 |
| Screen | AllowResize | bool | 0 | obj unk_4EA3C0 | 0x42C631 |
| Screen | FixedAspect | bool | 1 | obj unk_4EA328 | 0x42C671 |
| Screen | Quick Tool Tip Delay | int | 15 | var dword_4BACC4 | 0x433784 |
| Screen | Tool Tip Delay | int | 70 | var dword_4BACC8 | 0x4337C4 |
| Screen | Slow Tool Tip Delay | int | 150 | var dword_4BACCC | 0x433804 |
| Screen | Tool Tip Enable | bool | 1 | var byte_4BACD0 | 0x433844 |
| Screen | Use Reaper | bool | 0 | obj unk_586CB0 | 0x44DC71 |
| Screen | Tool Tip Offset | int | 16 | var dword_4D0FD8 | 0x49D444 |
| Sound | Inner border | point | 0 (bss, set at run time) | var dword_589260 | 0x464594 |
| Sound | Outer border | point | 0 (bss, set at run time) | var dword_589230 | 0x4645D4 |
| Sound | Inner Ambient border | point | 0 (bss, set at run time) | var dword_589200 | 0x464614 |
| Sound | Outer Ambient border | point | 0 (bss, set at run time) | var dword_5891F0 | 0x464654 |
| Sound | Search Libraries First | bool | 0 (bss, set at run time) | var byte_58FE78 | 0x465EC4 |
| Sound | Unload Limit | dword | 0 (bss, set at run time) | var dword_58FE7C | 0x465F04 |
| Sound\Volume | CD | bool | 1 | var dword_4B8A88 | 0x4057C4 |
| Sound\Volume | Master | float | 1.0 | var flt_4CA870 | 0x4642D4 |
| Sound\Volume | Ambient | float | 1.0 | var flt_4CA858 | 0x464314 |
| Sound\Volume | Engine | float | 0 | var unk_4CA85C | 0x464354 |
| Sound\Volume | Explosions | float | 0 | var unk_4CA860 | 0x464394 |
| Sound\Volume | Weapons | float | 0 | var unk_4CA864 | 0x4643D4 |
| Sound\Volume | Special | float | 0 | var unk_4CA868 | 0x464414 |
| Sound\Volume | Speech | float | 0 | var unk_4CA86C | 0x464454 |
| Stats | IRQ mem used | int | 0 | obj unk_4D13D0 | 0x402AC1 |
| Stats | Non IRQ mem used | int | 0 | obj unk_4D1430 | 0x402B01 |
| Stats | Routes left | int | 0 | obj unk_4EA110 | 0x4240A1 |
| Stats | WM Groups | int | 0 | obj unk_4EA128 | 0x425B51 |
| Stats | Holding Groups | int | 0 | obj unk_566268 | 0x449421 |
| Stats | BM Groups | int | 0 | obj unk_59D990 | 0x480161 |
| Stats | Scenery objects left | int | 0 | obj unk_59E158 | 0x489351 |
| Stats | Puppets objects left | int | 0 | obj unk_59E450 | 0x4955F1 |
| System | Game max time (ms) | int | 25 | obj unk_4D11C0 | 0x4018D1 |
| System | Game min time (ms) | int | 20 | obj unk_4D1190 | 0x401911 |
| System | Game Memory | int | 2 | obj unk_4D24E8 | 0x4022D1 |
| System | Renderer Memory | int | 5 | obj unk_4D2518 | 0x402311 |
| System | Game Memory Handles | int | 8192 | obj unk_4D1478 | 0x402354 |
| System | Renderer Memory Handles | int | 16384 | obj unk_4D13A0 | 0x402394 |
| System | ZMapRes | int | 400 | obj unk_4F7410 | 0x439704 |
| Tweak | Text Line Spacing | int | 4 | obj unk_4E72A0 | 0x40F5F1 |
| Tweak | Pod Interval (hours) | int | 24 | obj unk_4E9888 | 0x41A301 |
| Tweak | Quick Cancel Timeout | int | 5 | obj unk_4E9F10 | 0x41C5B1 |
| Tweak | Warmap Zoomer Speed | float | 1.5 | obj unk_4E9F28 | 0x41DFD4 |
| Tweak | WM clunting res | int | 36 | obj unk_4EA0C0 | 0x4200F1 |
| Tweak | WM clunting adj | int | 4 | obj unk_4EA0D8 | 0x420131 |
| Tweak | Resource Drop Off Rate | float | 3 | obj unk_4EA0F0 | 0x423F54 |
| Tweak | Res Stack XO | int | 4 | obj unk_4EA140 | 0x426341 |
| Tweak | Res Stack YO | int | 16 | obj unk_4EA158 | 0x426381 |
| Tweak | Clunting Speed | float | 7 | obj unk_4EA180 | 0x428B84 |
| Tweak | Max units in sector | int | 15 | obj unk_4EA1A0 | 0x42AB01 |
| Tweak | Max boats in sector | int | 5 | obj unk_4EA1B8 | 0x42AB41 |
| Tweak | Route clunting width | int | 4 | var dword_4BAA14 | 0x42AF34 |
| Tweak | Route clunting spacing | int | 4 | var dword_4BAA18 | 0x42AF74 |
| Tweak | VisRadMul | float | 1.5 | obj unk_4F7058 | 0x4373E4 |
| Tweak | Scenery Armour Modifier | float | 1.2 | obj unk_4F04E0 | 0x437424 |
| Tweak | HumanFriend Armour Modifier | float | 1.8 | obj unk_4F2E98 | 0x437464 |
| Tweak | HumanEnemy Armour Modifier | float | 1.8 | obj unk_4F04A8 | 0x4374A4 |
| Tweak | MartianFriend Armour Modifier | float | 1.8 | obj unk_4F2E80 | 0x4374E4 |
| Tweak | MartianEnemy Armour Modifier | float | 1.8 | obj unk_4F7070 | 0x437524 |
| Tweak | HumanFriend Research Modifier | float | 0.75 | obj unk_4F6E20 | 0x437564 |
| Tweak | HumanEnemy Research Modifier | float | 0.75 | obj unk_4F04F8 | 0x4375A4 |
| Tweak | MartianFriend Research Modifier | float | 0.75 | obj unk_4F2EF8 | 0x4375E4 |
| Tweak | MartianEnemy Research Modifier | float | 0.75 | obj unk_4F7088 | 0x437624 |
| Tweak | HumanFriend Manufacture Modifier | float | 1 | obj unk_4F2EE0 | 0x437664 |
| Tweak | HumanEnemy Manufacture Modifier | float | 0.85 | obj unk_4F2EB0 | 0x4376A4 |
| Tweak | MartianFriend Manufacture Modifier | float | 1 | obj unk_4EC580 | 0x4376E4 |
| Tweak | MartianEnemy Manufacture Modifier | float | 0.85 | obj unk_4F2EC8 | 0x437724 |
| Tweak | Water twinkles per frame | int | 20 | obj unk_4F76D0 | 0x439411 |
| Tweak | IMap Iteration Count | int | 4 | obj unk_4F8670 | 0x439741 |
| Tweak | Battlemap Zoomer Speed | float | 2.5 | obj unk_4F7438 | 0x43A454 |
| Tweak | Resource Buffer | float | 30 | obj unk_4F73F8 | 0x43C0B4 |
| Tweak | Distance Crippler VX | float | 0.5 | obj unk_4F86A0 | 0x43C0F4 |
| Tweak | Distance Crippler VY | float | 0.5 | obj unk_4F8688 | 0x43C134 |
| Tweak | AI strength table Human multiplier | float | 1 | obj unk_5187F8 | 0x441514 |
| Tweak | AI strength table Martian multiplier | float | 2 | obj unk_51A0D0 | 0x441554 |
| Tweak | AI Frontline Attractor for Defences | float | 2.0 | var flt_4BED88 | 0x4424E4 |
| Tweak | AI Eff Threshold | float | 0.60000002 | var flt_4BED8C | 0x442524 |
| Tweak | AI Panic Build Threshold | float | 500.0 | var flt_4BED90 | 0x442564 |
| Tweak | AI Move Threshold | float | 1.05 | var flt_4BED94 | 0x4425A4 |
| Tweak | AI Frontline Attractor | float | 2.0 | var flt_4BED98 | 0x4425E4 |
| Tweak | AI Build Resfac Eagerness | float | 50.0 | var flt_4BED9C | 0x442624 |
| Tweak | AI Aggression Value | float | 0 | var unk_4BEDA0 | 0x442664 |
| Tweak | AI Invasion Threshold PC | float | 150.0 | var flt_4BEDA4 | 0x4426A4 |
| Tweak | AI Outnumber Force | float | 1.1 | var flt_4BEDA8 | 0x4426E4 |
| Tweak | AI Retain Force | float | 0.2 | var flt_4BEDAC | 0x442724 |
| Tweak | AI Invader NFriend Mul | float | 3000.0 | var flt_4BEDB0 | 0x442764 |
| Tweak | AI safe sector repeller Mul | float | 2.0 | var flt_4BEDB4 | 0x4427A4 |
| Tweak | AI Minimum Rr | float | 8.0 | var flt_4BEDB8 | 0x4427E4 |
| Tweak | AI Movement Temperature | float | 0.25 | var flt_4BEDBC | 0x442824 |
| Tweak | AI Oracle | bool | 0 | obj unk_566248 | 0x442E51 |
| Tweak | AI Hours Per Turn | int | 5 | obj unk_545760 | 0x442E91 |
| Tweak | HeatResistantArmourMultiplier | float | 0.85 | obj unk_588BC8 | 0x4592D4 |
| Tweak | InsulatedHighEnergyResistantArmourMulti… | float | 0.75 | obj unk_588E30 | 0x459314 |
| Tweak | HighExplosiveRoundsMultiplier | float | 1.15 | obj unk_588BB0 | 0x459354 |
| Tweak | ImprovedPowderChargeMultiplier | float | 1.15 | obj unk_588E60 | 0x459394 |
| Tweak | AdvancedPowderChargeMultiplier | float | 1.3 | obj unk_588C10 | 0x4593D4 |
| Tweak | FineAirFiltersMultiplier | float | 0.85 | obj unk_588C28 | 0x459414 |
| Tweak | ChemicalFiltersMultiplier | float | 0.75 | obj unk_588E48 | 0x459454 |
| Tweak | EnhancedSightingApparatusMultiplier | float | 1.2 | obj unk_588BF8 | 0x459494 |
| Tweak | TunnelReinforcementApparatusMultiplier | float | 0.75 | obj unk_588E78 | 0x4594D4 |
| Tweak | EnhancedDrillMultiplier | float | 1.25 | obj unk_588BE0 | 0x459514 |
| Tweak | ShockResistantArmourMultiplier | float | 0.85 | obj unk_588E90 | 0x45A4D4 |
| Tweak | ImpactReflectingArmourMultiplier | float | 0.75 | obj unk_589118 | 0x45A514 |
| Tweak | EnhancedRayFocusMechanismMultiplier | float | 1.15 | obj unk_589070 | 0x45A554 |
| Tweak | TightBeamMultiplier | float | 1.3 | obj unk_589100 | 0x45A594 |
| Tweak | HoverBombsMultiplier | float | 1.25 | obj unk_588EA8 | 0x45A5D4 |
| Tweak | SecondaryBeamAmplifiersMultiplier | float | 1.15 | obj unk_588EC0 | 0x45A614 |
| Tweak | MotionTrackingDeviceMultiplier | float | 1.2 | obj unk_588EF0 | 0x45A654 |
| Tweak | Max Slope For Building On | int | 6 | obj unk_59D948 | 0x47B4B1 |
| Tweak | No-build border width (vertices) | int | 16 | obj unk_59D928 | 0x47B4F1 |
| Tweak | ISO obj col rad mul | float | 0.66 | obj unk_59D978 | 0x47D6C4 |
| Tweak | Audible radius multiplier | float | 1 | obj unk_59E438 | 0x495634 |
| Vehicles | startTurnRate | int | 12 | obj unk_59E1A8 | 0x48FB71 |
| Vehicles | stopTurnRate | int | 8 | obj unk_59E178 | 0x48FBB1 |
| Vehicles | turnAnimRate | float | 0.02 | obj unk_59E190 | 0x48FBF4 |
| Vehicles | maxAcc | float | 1 | obj unk_59E328 | 0x490274 |
| Vehicles | collisionCheckDist | float | 0.5 | obj unk_59E1C0 | 0x4902B4 |
| Vehicles | attributeAccWeight | float | 0.3 | obj unk_59E2B0 | 0x4902F4 |
| Vehicles | attributeAccMax | float | 0.2 | obj unk_59E268 | 0x490334 |
| Vehicles | separationAccWeight | float | 0.2 | obj unk_59E3A0 | 0x490374 |
| Vehicles | separationAccMax | float | 0.1 | obj unk_59E310 | 0x4903B4 |
| Vehicles | avoidanceAccWeight | float | 0.15 | obj unk_59E298 | 0x4903F4 |
| Vehicles | avoidanceAccMax | float | 0.1 | obj unk_59E1D8 | 0x490434 |
| Vehicles | brakingAccWeight | float | 0.1 | obj unk_59E388 | 0x490474 |
| Vehicles | brakingAccMax | float | 0.2 | obj unk_59E250 | 0x4904B4 |
| Vehicles | velMatchAccWeight | float | 0.08 | obj unk_59E238 | 0x4904F4 |
| Vehicles | velMatchAccMax | float | 0.03 | obj unk_59E1F0 | 0x490534 |
| Vehicles | targAccWeight | float | 0.003 | obj unk_59E2F8 | 0x490574 |
| Vehicles | targAccMax | float | 0.15 | obj unk_59E220 | 0x4905B4 |
| Vehicles | attractAccWeight | float | 0.001 | obj unk_59E358 | 0x4905F4 |
| Vehicles | attractAccMax | float | 0.1 | obj unk_59E2E0 | 0x490634 |
| Vehicles | arrivalAccWeight | float | 0.33 | obj unk_59E400 | 0x490674 |
| Vehicles | arrivalAccMax | float | 0.2 | obj unk_59E3B8 | 0x4906B4 |
| Vehicles | arrivalRadius | float | 1.5 | obj unk_59E280 | 0x4906F4 |
| Vehicles | speedCostDivisor | float | 25 | obj unk_59E2C8 | 0x490734 |

## 5. Cross-references

### 5.1 Who calls this piece

| This piece | Called from | Context |
|---|---|---|
| 0x4A14C0 (via vtable 0x4B1270) | the display-list walk | Building placement (the build list sets the mode; 0x80A4 / 0x8137 come back to 0x460E7C) |
| 0x4A1CD0, 0x4A1D70, 0x4A1D80 | 0x486A4D, 0x486CDF, 0x4870DE, 0x487167 | Battle mini-map handler 0x486E80 (message 0x8077 adds a dot) |
| 0x4A1F90 | 0x48695F | Battle-map menu / mini-map set-up |
| 0x4A21A0 | 0x48BEE9 … 0x48C0E1 (10×) | Vehicle debug overlays (0x48BE30) |
| 0x4A21F0 | 0x4A14C0, 0x4A0… (part 13) | Footprint lines |
| 0x4A27A0, 0x4A2840, 0x4A28E0 | 0x48E08F, 0x48E0D3, 0x48E0E1, 0x48E1CD | Vehicle light (0x48E050 / 0x48E100) |
| 0x4A2A20 | 0x49DA75, 0x49DDC3 | RTypeISO light objects |
| 0x4A2A90, 0x4A2C50, 0x4A2E00, 0x4A2FA0, 0x4A3020 | 0x49E6F6 … 0x49EB60 | WOFLib model transform |
| 0x4A31E0 | 0x42AF34, 0x42AF74, 0x42D2A4 | Static initialisers |
| 0x4A3600 (14×), 0x4A36B0 | 0x4424E4 … 0x442824, 0x4646CF | Static initialisers (AI, sound) |
| 0x4A3350 | 0x4400CA | WSelectableObject id stack (0x8000) |
| 0x4A3590 / 0x4A35C0 | 0x4402C8, 0x4404DF / 0x44017A | Free / allocate object id |
| 0x4A3860, 0x4A3820, 0x4A38C0 | 0x46677E–0x4667F0, 0x46680A–0x46689B | Sound libraries |
| 0x4A39E0 | 0x43B037 | Sector object, WM_COMMAND 0xB7 |
| 0x4A3BD0 | 0x446A13 | War-map object factory 0x446950 (types 0x3E–0x5F) |
| 0x4A3E60 | 0x440425 | WSelectableObject info (0x4403A0) |
| 0x4A4220 | 0x45D5EF, 0x45D7F6, 0x45DA2A, 0x45DF01, 0x45E069, 0x45E1D0 | Research / blueprint screens |
| 0x4A4290 | 0x484159 | Battle-map menu |
| 0x4A4360, 0x4A43B0, 0x4A43D0, 0x4A43F0 | 0x458266 … 0x45E58A | WResearch |
| 0x4A4400 | 0x4601DE | Build list order |
| 0x4A4440 + 0x4A4470 | 0x485CA0/CCC (force fire), 0x497805, 0x42C1C2, 0x43C469, 0x485661 | Orders into the command stream |

### 5.2 What this piece calls (outside it)

- **Landscape** (`dword_59D940`, WLandscape):
  - 0x47B170 height at (x,y); 0x47B3F0 snap; 0x47B610 can-build;
  - 0x47B240 normal; 0x4A0140 apply light (part 13).
- **RType / display**: 0x40E880 RTypeBase ctor, 0x40E910 WObject ctor, 0x40EA50 dtor, 0x40E700 RType pool alloc (1024), 0x40EB40 screen offset, 0x40EC30 set colour, 0x40FD80 sprite blit, 0x40B500 shade level, 0x40CB10 light level, 0x40F000 text style, 0x49D260 text object.
- **Files and strings**: 0x415200 FILE_Load, 0x414BC0 free, 0x4170E0 / 0x417330 / 0x417380 path string helpers, 0x4146F0 string by id, 0x414700 speech by string id, 0x4161E0 word wrap, 0x416230 sprintf to a temp buffer.
- **Windows / UI**: 0x42F730 create window, 0x42EDA0 size, 0x42E9B0, 0x42EA00 / 0x42F7C0 destroy, 0x42F2A0 close, 0x42E150 / 0x42E290 base paint / proc, 0x4355A0 set timer, 0x402970 screen size, 0x42C6B0 / 0x42DA10 mouse position.
- **Settings**: 0x4166B0, 0x416760, 0x416770, 0x4168D0.
- **Sound**: 0x464EB0 fade out / stop.
- **War map and AI**: 0x437380 waypoint, 0x419240 sector object by number, 0x43C990 has command building, 0x44EE70 / 0x44EFA0 / 0x44F280 A*, 0x428BB0 / 0x428CB0 / 0x428D70 / 0x429F20 / 0x429FC0 / 0x42A190 / 0x42A220 war-map object base.
- **Orders**: 0x4460A0 / 0x4460C0 / 0x438C50.

### 5.3 Message and command ids seen here

| Id | Meaning |
|---|---|
| 0x80A4 | Place building at {x, y} (to the game window; handled at 0x460E7C) |
| 0x8137 | Leave building-placement mode |
| 0x8077 | Battle mini-map: add a unit dot (0x486E80) |
| WM_COMMAND 0xB7 | Sector: rebuild its 30 cached sector paths |
| WM_TIMER 0x812C | InfoPage: start the speech (15 ticks after opening) |
| 0x101 / 0x202 / 0x205 | Close InfoPage |
| Order records 0x0101 / 0x0202 / 0x0404 / 0x0707 | Command-stream records; codes 0x0E and 0x13 come from the battle map's move / attack (force fire with Ctrl) |

### 5.4 Globals

| Global | What it is |
|---|---|
| `dword_59D940` | WLandscape |
| `dword_4E9B70` | WWarMapWorld (ctor 0x417CD0) |
| `dword_4EA448` | The window that takes game commands |
| `dword_4EA440` | Current context / cursor value |
| `dword_4DAF7C`, `dword_4DAF80` | Red and green palette indices |
| `dword_4DAFC8` | Text colour |
| `word_4B9424` | Info Page Text colour |
| `dword_4D2580` | RNG index |
| `dword_566260` | Command buffer |
| `byte_4EAA18` | Sector links |
| `dword_4EAE1C` | Sector owners |
| `dword_51A104`, `dword_51A108`, `word_51A136` | Type table fields |
| `dword_4F8748` | Object ids |

## 6. Missing in WoWEngine / candidate tasks (prioritised)

WoWEngine today covers the file formats, the front end, the war map display and moves (`src/warmap`), INI settings (`frontend/Settings`) and save reading (`core/SaveGame`). Nothing of the battle map, WOFLib, the AI or orders exists yet. Each task lists the addresses a developer needs.

### P1: needed by work already under way (war map and saves)

1. **Read the war-map `[Tweak]`s the engine still hard-codes.** It reads only `Clunting Speed`, `Max units in sector`, `Max boats in sector`, `Scroll Speed`, `BPP` and `Size`. Missing ones the war map uses:
   - `Pod Interval (hours)` 24 (0x41A301);
   - `Quick Cancel Timeout` 5 (0x41C5B1);
   - `Warmap Zoomer Speed` 1.5 (0x41DFD4);
   - `WM clunting res` 36 (0x4200F1), `WM clunting adj` 4 (0x420131);
   - `Resource Drop Off Rate` 3 (0x423F54);
   - `Res Stack XO` 4 / `YO` 16 (0x426341 / 0x426381);
   - `Route clunting width` 4 / `spacing` 4 (0x42AF34 / 0x42AF74, bound to 0x4BAA14 / 0x4BAA18);
   - `[Screen] Scroll Border` 4 (0x41DF94);
   - `AI Hours Per Turn` 5 (0x442E91);
   - `Distance Crippler VX/VY` 0.5 / 0.5, `Resource Buffer` 30 (0x43C0B4–0x43C134);
   - the tool-tip delays `Quick Tool Tip Delay` 15, `Tool Tip Delay` 70, `Slow Tool Tip Delay` 150, `Tool Tip Enable` 1, `Tool Tip Offset` 16 (0x433784–0x433844, 0x49D444).
2. **Sector paths (0x4A39E0 / 0x4A3D10, message 0xB7 from 0x418890 / 0x4192A0, cache at sector+0xA0, getter 0x43C090).** The engine has the player route check (`WarMap::FindRoute` = 0x42ABD0) but not these cached owner-side paths. The resource flow 0x41A760 and the AI (0x4437CF–0x444D8F) need them. Same command-post rule (`HasCommandPost` = 0x43C990), land links only (`byte_4EAA18` bit 0), no unit limits, length = sum of waypoint distances.
3. **The RNG.** Reproduce the 1024-entry table (0x4D14A8) filled by MSVC `rand()` (LCG `x = x*214013 + 2531011`, output `(x>>16)&0x7FFF`). Seeds: `srand(timeGetTime())` at start-up (0x401AE8) and `srand(1)` when the war map is built (0x417DA3). `Random(n)` = `table[i++ & 1023] % (n+1)` (0x4025F0). Direct `rand()` callers on the war map: 0x41A360, 0x41A382, 0x41B17C, 0x422738, 0x42B34B. Without it, campaigns and AI decisions can't match the original.
4. **InfoPage (0x4A3E60–0x4A4190).** The object info popup: TEXT.ojd string `type+8` (0x51A108), font 0x73, width 256, colour `[Colours] Info Page Text` 15, at the click +32 clamped into the view. Its speech starts 15 ticks later (0x414700) and fades over 30 when it closes. It closes on key-up or a mouse-button release. The war map's counters and buildings need it (opener 0x4403A0).
5. **Object id allocation (0x4A3350 / 0x440120, 32768 ids, lowest-first then LIFO).** Needed to write saves the original can load (`SELO` ids) and to keep ids stable after load.
6. **Save writer: the `VEHU` tag after `WMOB` for unit types 0x3E–0x5F** (0x4A3C80; reader 0x4A3C60 skips one dword). The engine's reader knows the tag; a writer must emit it.
7. **Settings persistence.** The original writes the bound AI floats (and probably the bound ints) back at exit (0x4A3740 / 0x4A3290), so a player's `WOW.INI` / registry contains them. When importing an original install, read these from `HKLM\SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\…` or `WOW.INI` (`Use INI`, 0x4166B0) and map them onto WoWRevived.ini. Also add the remaining sections from section 4: `[Colours]` (only 3 of 20 read), `[Event]` (11 keys, including `Entered Neutral Sector`), `[Options] Show simple tooltips`, `[Sound] Inner/Outer (Ambient) border`, `Search Libraries First`, `Unload Limit`, `Minimum Audible Volume` 0.05, `[Briefing] Delay` 30, `[Debriefing] Delay` 300, `[Research] Human/Martian Open Rate` 20/10, `[BuildList] Top/Left` 70/0.

### P2: the battle map (when it starts)

8. **Building placement footprint** (0x4A13B0 / 0x4A14C0 / 0x4A1460). Footprint `(size*2+14)&~15`, edge lines every 16 units following the terrain, green / red from the can-build test 0x47B610 (`Max Slope For Building On` 6, `No-build border width` 16), smoothed mouse position, and 0x80A4 then 0x8137 on click.
9. **Battle mini-map**: `maps\hmin%02d.spr` / `mmin%02d.spr` backdrop (0x4A1F90, shade slot 0x15) and up to 1024 2x2 unit dots (0x4A1D80 / 0x4A1DD0, message 0x8077 from 0x486E80, scaled world → mini-map).
10. **Line primitive** (0x4A24F0 clip + 0x4A22C0 / 0x4A23D0 Bresenham) for the footprint and the `[Debug] Show Accelerations / Radii / Arrival / Orders` overlays (vehicles 0x48BE30).
11. **Dynamic vehicle lights** (0x4A27A0–0x4A2A80, terrain apply 0x4A0140, per-type intensity 0x4EC598, light-level table 0x4DA660). Gated by `[Options] Show lights`.
12. **WOFLib maths**: 4.12 fixed-point rotation (0x4A2E00, 4096 angle units, truncating `_ftol`), matrix products (0x4A2A90, 0x4A2C50), transform (0x4A2FA0) and terrain tilt (0x4A3020 with the normal from 0x47B240). Use the original fixed point and truncation so models line up pixel for pixel. Polygon order: MSVC `qsort` (0x4A4480, unstable) over `{index, depth}` (0x49ED32).
13. **The order / command stream** (records 0x0101 / 0x0202 / 0x0404 / 0x0707, buffer 0x438C50, send 0x4A4470). This is the base for playback files (`[Demo] Playback Files` 4, `playback.%d`) and network play, so it is worth designing the engine's input → order layer around it from the start.

### P3: research and other UI

14. **Research screens' helpers**: `FormattedLabel` (0x4A4220), `OwnerSlot` (0x4A4360…), and research tables 0x51DC0C / 0x52BACC / 0x53885C (0x45A1C0) once WResearch is rebuilt. `[Tweak]` research multipliers (0x4592D4–0x45A654) and the per-side armour, research and manufacture modifiers (0x4373E4–0x437724) are in section 4.
15. **Captioned graphic button** (0x4A4290 / 0x4A4300) for the battle-map menu (0x484159).
16. **SfxL lookup key**: match the game's "up to `.`, 8 characters, uppercase" key (0x4A3820) in `core/Archive` lookups for `SFX\SFX.WoW` / `VOX\*.WoW`, if not already done.

### Not needed

The CRT (`_qsort` aside), the SEH funclets 0x4A478E–0x4A8A92, the `Stack` debug check 0x4A3460, and the `[Stats]` / `[Debug]` developer values (they could be exposed in a debug overlay).
