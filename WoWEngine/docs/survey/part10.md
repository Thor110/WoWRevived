# Part 10: 0x466300–0x47F480 (sound samples, C runtime, Reaper FMV codec, WLandscape, object lists, the battle-map object base class)

The address ranges and class names below come from the listing. Anything marked *(guess)* is an inference, and *unclear* means the listing didn't settle it.

## 1. Overview

The piece holds five unrelated blocks, in address order:

| Range | Block | Rebuild relevance |
|---|---|---|
| 0x466300–0x466E18 | **SoundFXSystem, second half.** Sample load and unload, archive lookup, loose-file lookup, the SFX table entry (0x70 bytes) with variation groups and round-robin, and the intrusive list of playing sounds | Medium. The engine has most of this; the differences are listed in section 6 |
| 0x466E30–0x477D00 | **MSVC 6 C runtime and STL** (std::string throws, `_rand`/`_srand`, stdio, heap, EH, locale, `start` at 0x468070). About 360 collapsed library functions | Not rebuilt, except `_rand`, whose exact LCG matters (see 6) |
| 0x477DC0–0x47AEF0 | **Reaper '95 v1.30** ("(c) Paul Hughes", 0x4CE744). A third-party FMV codec and player for `FMV\*.fmv`: `!Reaper!` files, `Rimg`/`Raud` chunks, an ADPCM audio stream through its own DirectSound buffer, and its own mm-timer. The decoder core is **hand-written assembly** | None. It only runs when `[Screen] "Use Reaper"` is set (default 0), and no `.fmv` files ship (the CD has only `.smk`) |
| 0x47AEF2–0x47AF0A | Import thunks: DirectSoundCreate, DirectPlayEnumerateA, DirectPlayCreate, DirectDrawCreate, RtlUnwind | — |
| 0x47AF10–0x47CCF0 | **WLandscape**, the battle-map terrain. It loads `maps\<name>.cls` and `.atm`, builds per-vertex info (height, normal, light, terrain attributes, occupancy), lights the map from the sun direction (`lightTheMap`), and answers queries: height, normal, screen picking, passability, build-site, wall and bridge lines, explosion scorch, point lights | **High.** This is the core of the battle map |
| 0x47CCF0–0x47D6D6 | **WObjectList** (vtable 0x4AD5F0): a refcounted `std::list` of battle objects, with broadcast, update, save/load (`BMOL`), counting by side, and a staggered message broadcast. Also two Tweak settings | High |
| 0x47D6F0–0x47F47F | **Battle-map object base class** (vtable 0x4AD620; I call it *WMapObject*, a guess at the name). It derives from WSelectableObject. Covers construction, flags, renderer creation by GType class, child "puppet" objects attached to model vertices, smoke and particle emitters, the **unit command panel**, the **special-ability table**, the message handler, and kill/cleanup | **High.** This is gameplay |

Hand-written assembly in this piece is limited to the Reaper decoder: 0x479D43, 0x479F60, 0x47A0FB, 0x47A25E, 0x47A372 and 0x47A5F8. These have odd, unaligned start addresses, pass arguments in registers, have no stack frame, and keep bit-reader state in globals 0x5A1000–0x5A100C. 0x479EC0 (an RLE unpacker taking its arguments in ecx/edx) also looks hand-written. Everything from WLandscape onwards is ordinary compiled C++ (thiscall, x87, `__ftol`).

---

## 2. Subsystems

### 2.1 SoundFXSystem, second half (0x466300–0x466E18)

The first half (0x462EF0–0x4662xx: sfx.ojd loader 0x463D10, sound objects, volume groups) is in the previous piece.

| Addr | Name (mine) | What it does |
|---|---|---|
| 0x466300 | `SfxSample::EnsureLoaded` | If `+0x20` (the IDirectSoundBuffer) is null, builds the path `+8` + (`+0x28`==0 ? ".snd" : ".WoW"), resolves it with 0x466910, and gets the size (`+0x2C`) from the loose file if it's 0. It creates the buffer with 0x465D10(&buf, size, rate `+0x18`, `+0x1C`, channels/stereo byte `+0x15`) and locks it (vtbl+0x2C, flags 2). It then reads from the shared archive handle `+0x24` at offset `+0x28` (SetFilePointer), or from the loose file, and returns `size == bytesRead`. In Debug it logs "Loading SFX %s" through nullsub_4 |
| 0x466630 | `SfxSample::AddRef` | `+0x30`++ (lifetime play count) and `+4`++ (refcount) |
| 0x466640 | `SfxSample::Release` | `+4`--. At 0 and if size `+0x2C` > `[Sound] "Unload Limit"` (dword_58FE7C, default 0), releases the buffer (vtbl+8) and logs "Unloading SFX %s" |
| 0x466750 | `SfxSample::FindInLibraries` | Opens the three libraries once (static-init guard byte_5896E0, readers from 0x4A3860): `SFX\SFX.WoW` (0x589840), `VOX\HUMAN.WoW` (0x5896E8) and `VOX\MARTIAN.WoW` (0x58FE40). **Both VOX libraries are searched whichever side is played.** It looks up the name (from `+0xC`, after the "SFX\"/"VOX\" prefix; key made by 0x4A3820, lookup 0x4A38C0). On a hit it sets `+8` = "SFX\SFX" / "VOX\HUMAN" / "VOX\MARTIAN", `+0x28` = the entry offset (entry+0xC), `+0x2C` = the size (entry+8), and `+0x24` = the library's file handle |
| 0x466910 | `ResolveLooseSoundPath(path)` | Only active when byte_4D12F4 is set (= Debug 0x4D1231 \| 0x4D1261 \| 0x4D1279, from WinMain 0x4019A6); otherwise it returns `path`. Order: `path` itself if it exists → `SFX\HUMAN\<name>` or `SFX\MARTIAN\<name>` (by side, 0x404BA0) → `VOX\<Language>\<name>` (String1, default "English") → `VOX\<Language>\HUMAN\<name>` or `\MARTIAN\` → otherwise the static buffer 0x589720 (last candidate). `<name>` is `path+4`, which skips "SFX\" |
| 0x466A50 / 0x466A80 | static ctor/dtor | `eh vector constructor` for **450 (0x1C2) SFX entries × 0x70 bytes at 0x58FE88** |
| 0x466AB0 | `SfxDef::SfxDef` | vtbl 0x4AAF34. Zeroes 3 groups × 5 sample pointers (`+4`, stride 0x14), counts `+0x40[3]` and cursors `+0x4C[3]`, and `+0x58/+0x5C/+0x60/+0x64/+0x66`. Sets `+0x68` = 1.0f (volume) |
| 0x466B30 | `SfxDef::~SfxDef` | resets the vtable |
| 0x466B40 | `SfxDef::AddSample(name, group)` | Gets or creates the sample (0x465F90: table 0x5898B0, 0x34 bytes each, count dword_58FE80, matched by `_strcmpi`) and appends it to `group` unless it's already in any group. Called by the loader 0x463D10+0x18C |
| 0x466BC0 | `SfxDef::NextSample(group)` | **Round-robin**: if `group` is empty, falls back to group 0. Advances cursor `+0x4C[g]` (wrapping to 0 after count−1) and returns that sample. The sound-object ctor 0x4648C0+0x8B passes the group |
| 0x466C30 | `SfxDef::SetStereo(b)` | sets sample `+0x15` on every sample (opcode 15) |
| 0x466C70 | `SfxDef::SetRate(r)` | sets sample `+0x18` on every sample (opcode 11) |
| 0x466CC0 | `SoundList::UpdateAll` | walks the singly linked list (`next` at +0x10) and calls vtbl+0x10 on each. Used by 0x464A50 (listener moved) on the 6 type lists |
| 0x466CE0 | `SoundList::PushFront` | Pushes onto the front; `+4` word refcount++, list count `+4`++ |
| 0x466D00 | `SoundList::PushUnique` | Like PushFront, but refuses when a node with the same sound id `+0x14` is already present |
| 0x466D40 | `SoundList::Remove(obj)` | unlinks the node, drops its refcount (deletes at 0), count-- |
| 0x466DB0 | `SoundList::Clear` | releases all nodes and zeroes head and count |
| 0x466DF0 | `SoundList::StopAll(fade)` | fade ? 0x464EB0 (fade out) : 0x464E30 (stop). Used by 0x464AB0 "Fading out all current (%d) sounds" |

**Sample record** (0x5898B0 + 0x34·i, count 0x58FE80):

| Offset | Meaning |
|---|---|
| +4 | refcount |
| +8 | path: first "SFX\name" or "VOX\name"; after a library hit, the library base path |
| +0xC | bare name |
| +0x15 | stereo byte |
| +0x18 | rate (default 22050) |
| +0x1C | *unclear* (passed to 0x465D10) |
| +0x20 | IDirectSoundBuffer* |
| +0x24 | library HANDLE (0 = loose file) |
| +0x28 | offset in the library (0 = loose) |
| +0x2C | byte size |
| +0x30 | play counter |

**Playing-sound lists:** 0x5893E8, 0x589388, 0x589258, 0x589360, 0x589370 and 0x5894A0 (six lists, one per volume type, *(guess)*). Each is `{head, count}`, and dword_5894A8 is the total.

### 2.2 C runtime (0x466E30–0x477D00)

This is the standard MSVC 6 multithreaded CRT, statically linked; IDA collapsed most of it. Game code relies on these entries:

- 0x466E30: `std::_Xlen` ("string too long"). 0x4672E0: `_Xran` ("invalid string position"). 0x466F70, 0x467180 and 0x467430 are `logic_error`, `length_error` and `out_of_range` constructors (vtables 0x4AAF64/44/74).
- **0x467B20 `_srand`, 0x467B30 `_rand`.** These use the MT CRT, so the seed lives **per thread** in `_getptd()->_holdrand`: `holdrand = holdrand*214013 + 2531011; return (holdrand>>16)&0x7FFF`. Game logic (game thread 0x402180) and rendering (0x402230) have separate rand streams.
- 0x467B60 `_access` (the loose-file probes), 0x468000 `_sprintf`, 0x468070 `start`, 0x4682E0 `_ftol`, 0x468830 `_clock`, 0x469140 `_strcmpi`, 0x469B19 `_pow`, 0x469D94 `_asin`, plus `fopen`/`fread`/`fseek` and so on.
- 0x467730 and 0x468C20 are the locale paths of `_tolower` and `_toupper`. 0x46A1F0 is `__crtLCMapStringA`.
- 0x4783D0 `Reaper_FatalError(msg)`: MessageBoxA(msg, "Reaper Fatal Error"). It belongs to Reaper, not the CRT.

### 2.3 Reaper '95 FMV player (0x477DC0–0x47AEF0)

`[Screen] "Use Reaper"` (bool, default 0) is registered at 0x44DC60 and its value byte is 0x586CC1. The movie launcher 0x44DCA0+0x146 starts thread **0x44E060** (Reaper, `FMV\<name>.fmv`) when the flag is set, and otherwise 0x44DE20 (Smacker). The rebuild can skip Reaper and should only note that the setting exists.

| Addr | Name | What it does |
|---|---|---|
| 0x478130 | `Reaper_Open(file, ddsurf, desc[11 dwords])` | calloc 0x378. Copies the 44-byte surface descriptor to `+0x20..+0x4B`; `+0x70` = DD surface; `+0x74/78/7C` = desc fields. Calls 0x4783F0 (audio init). Opens with `_open` (0x473EC0, O_BINARY) and reads a 0x1C-byte header into 0x59C958: `"!Reaper!"`, u32 header size (0x59C960), s16 → `+0xC`, s16 → `+8`, s16 table size (0x59C968), s16 audio flag → `+0`. It then RLE-unpacks (0x479EC0) the rest of the header into the malloc'd table dword_59C9E0. If there's audio it pre-reads 16 audio chunks (0x477EF0) and builds the colour tables (0x4799F0). Error code dword_59C974: 1 open, 2 signature, 3 memory |
| 0x477EF0 | `Reaper_ReadChunk` | Reads a big-endian length and tag. Tag `'Raud'` (0x64756152) is audio: decoded into the ring `+0xAC[32]` (8-byte entries) by ADPCM 0x479D43. Tag `'Rimg'` (0x676D6952) is a video frame: stored into the ring `+0x1AC[32]` (12-byte entries, index `+0xA8`). Returns 1 for a frame, 2 for audio, 0 at EOF |
| 0x477DC0 | `Reaper_NextFrame` | Reads until a frame is queued. Decodes the frame at the ring head `+0xA4` (0x479BB0). If `+0x4C` (frames to skip) ≤ 1, blits it: `+0x6C` (pixel format) 15 or 16 → 0x4790E0, otherwise → 0x479430. Frame counter `+0x84`. Starts the timer (0x478680) the first time. Frees the slot, advances `(+0xA4+1)&31`, and InterlockedIncrement `+0x88`. Returns 0 at end of movie (and stores `_clock()` in `+0x374`) |
| 0x4780E0 | `Reaper_ShouldWait` | For movies with audio: returns 1 while the decoder is more than one frame ahead of the audio clock (`+0x88` − `+0x8C` ≥ 1); otherwise sets the skip count `+0x4C` |
| 0x478300 | `Reaper_Close` | Closes the file, stores the play time in `+0x36C`, frees the rings, kills the timer (0x478AB0), and releases DirectSound (0x478630) and the colour tables (0x479B70) |
| 0x4783F0 | `Reaper_InitAudio` | DirectSoundCreate (0x59C9E8), SetCooperativeLevel, primary buffer, then a secondary streaming buffer (0x59C9EC). Format from the header |
| 0x478630 | `Reaper_FreeAudio` | releases both buffers |
| 0x478680 | `Reaper_StartTimer` | Fills and plays the stream buffer, then `timeGetDevCaps`, `timeBeginPeriod` and **`timeSetEvent`(period `+0x368`, 0x478880)**. Errors go to 0x4783D0 |
| 0x478880 | `Reaper_TimerProc` | multimedia-timer callback. Refills the DirectSound ring from the decoded audio queue (Lock/Unlock) and advances the audio clock `+0x8C`. Re-entry guard `+0x360` (InterlockedExchange) |
| 0x478AB0 | `Reaper_StopTimer` | timeKillEvent, then timeEndPeriod |
| 0x478B40, 0x478C70 | `Reaper_BuildColourTables` | three 128-entry tables (0x59D224, 0x59D424, 0x59D624), packing clamped ±31/±63 components into 555 or 565 according to `+0x6C` (YUV→RGB lookup, *(guess)*) |
| 0x4799F0 | `Reaper_InitFrameBuffers` | calloc's two w·h·4 buffers (`+0x10`, `+0x14`), queries the DD surface pixel format (vtbl+0x58/+0x54), derives the mask shifts, and calls 0x478B40 |
| 0x479B70 | `Reaper_FreeFrameBuffers` | |
| 0x4799E0 | `Reaper_SwapBuffers` | swaps `+0x10` and `+0x14` |
| 0x479BB0 | `Reaper_DecodeFrame` | parses the frame's block list and calls 0x4797D0 for each 256 blocks |
| 0x4797D0 | `Reaper_DecodeBlocks` | block type 0xC → 0x47A5F8 (transform-coded block); 0xB → copy (0x478F20); otherwise motion or skip *(guess)* |
| 0x478F20 | `Reaper_CopyBlock` | copies or moves blocks between the frame buffers |
| 0x4790E0 / 0x479430 | `Reaper_Blit16` / `Reaper_BlitOther` | Lock the surface (vtbl+0x64), convert through the tables, Unlock |
| 0x479D43 | `Reaper_DecodeADPCM` (**asm**) | 4-bit ADPCM, clamped to ±0x7FFF, step limit 0x1600 |
| 0x479EC0 | `Reaper_UnRLE` (**asm-like**) | escape-byte RLE |
| 0x479F60, 0x47A0FB, 0x47A25E, 0x47A372, 0x47A5F8 | (**asm**) | bit reader / Huffman (state 0x5A1000–0x5A100C), 8-point row pass (×8, tables 0x59CA04…), 8-point column pass (×8), output; 0x47A5F8 is the block driver. Looks like an 8×8 DCT-style codec *(guess)* |

### 2.4 WLandscape, the battle-map terrain (0x47AF10–0x47CC46)

The battle-map world calls the constructor from 0x439770+0x88 (`new` 0x6C; arg = world `+0x48` = battle-map index). The global is `dword_59D940`, with about 130 xrefs.

| Addr | Name | What it does |
|---|---|---|
| 0x47AF10 | `WLandscape::WLandscape(mapIndex)` | WCmdTarget base ("WCmdTarget", 0x4354C0). Zeroes fields; vtbl 0x4AD590; `+0x40` = mapIndex. Sets the time of day from `(int)calendar(dword_4E9B80)+0x28 % 24` through 0x40B030 (that fills the sun vector). Copies the sun direction flt_4DA778 → `+0x50` and flt_4DAE28 → `+0x54`. Calls `CreateTerrain` (0x47B900). Sets dword_59D940 = this. Creates **WRouteFinder** (0x54 bytes, 0x493DF0) → dword_59E418 |
| 0x47B010 | scalar deleting dtor | (vtbl slot 0) |
| 0x47B0D0 | `WLandscape::OnMessage(msg, w, obj)` | Only handles **0x804B** (explosion scorch). Calls `Scorch(x=obj+0x78, y=obj+0x7C, radius=obj+0xAC, intensity=(s16)obj+0xCA)`. Sent by the explosion object at 0x488200+0x12F unless 0x47FFE0 says the point is water |
| 0x47B900 | `WLandscape::CreateTerrain` | Name = `"maps\\"` + the battle-map table name at **0x4EAEC8 + 0xC0·mapIndex** (filled by the OBJ.ojd loader 0x437750+0x3BA). It searches for the file (path helper 0x4171E0/0x417380), and the extension sets `+0x44`: **`.ils` → 1, `.cls` → 2**. It loads the whole file into `new(size+0x80)` → `+0x5C`; `+0x68` = header. It allocates **vertInf** (MEM tag "vertInf", 10 bytes × vertex count, through 0x4148A0, pool dword_4D1234) → `+0x60` handle, `+0x64` data. It copies the header fields (see 3.2), swaps the extension to **`.atm`** (0x4175B0) and loads it (MEM tag "atm"; on failure "WLandscape::CreateTerrain: couldn't load…"). Then per **cell** (w−1)×(h−1): vertex`+8` = ATM byte, vertex`+9` = 1, vertex`+1` = 0. Per vertex: `+6` = ((hByte·hdr0x2C>>16) + hdr0x30) · 92682 >> 16, i.e. **height × √2** (iso projection). Finally `CalcNormals(0,0,w,h)` |
| 0x47BD80 | `WLandscape::LightTheMap` | Called from LoadShadeTables 0x40B570+0x138, so it reruns whenever the time-of-day shade tables reload. Allocates the "ISOTemp" byte map (w·h). Pass 1, per vertex: light = `ftol(ambient flt_4DA764·256)` · 0x40CA90(dz along x, dz along y, cell, sunX `+0x50`, sunY `+0x54`) (a Lambert-style factor, *(guess)*). Pass 2, **shadows**: per vertex it takes the 3×3-smoothed height (0x43FD10) and walks up to **16 vertices** toward the sun. The side depends on the sign of flt_4DAEBC; the elevation is flt_4DAEC0/flt_4DAEBC. Any vertex whose height-difference slope is under the sun elevation gets light = ambient·sunY (shadow). Pass 3: vertex`+0` = 3×3 average of ISOTemp (0x43FD10). Border vertices (outer rows and columns) get 0. Frees ISOTemp |
| 0x47C100 | `WLandscape::CalcNormals(x0,y0,x1,y1)` | n = (−cell·(h[x+1]−h), −cell·(h[y+1]−h), cell²), normalised and ×127 → s8 `+3,+4,+5` |
| 0x47B120 | `HeightAtNearest(x,y)` | world (x,y) → vertex `(x<<16)/cell`, clamped → `(s16)+6` |
| 0x47B170 | `HeightAt(x,y)` | **bilinear** height in 8-bit fixed fractions: `h00 + fx·(h10−h00) + fy·(h01−h00) + fx·fy·(h11−h10−h01+h00)`. Used everywhere: vehicles 0x48D30D…, effects 0x4889FF, groups 0x481529…, building placement 0x4A0839…, the next piece 0x47F7A7/0x47FA52 |
| 0x47B240 | `NormalAt(x,y,out[3])` | bilinear interpolation of the three s8 normal components; result `>>3 & ~31` per component. Called from 0x4A3020 |
| 0x47B3F0 | `ScreenYToWorldY(worldX, screenY)` | **Mouse picking on the iso terrain.** For column `(x<<16 + half)/cell + 1` it scans rows from the bottom up, computing the projected y = `(row·rowStep − h·hScale)>>16 − base + yOff` from the raw heights `+0x58`, and returns `(row·cell − half)>>16` for the first row whose projection ≤ screenY. Callers: battle-map input 0x485A88/0x485C77, 0x4977E9, placement 0x4A04D7… |
| 0x47B520 | `IsAreaPassable(x0,y0,x1,y1)` | Every vertex in the rectangle must have type ≠ 8, slope ≤ MaxSlope, and type ∉ 1..3 (no water). Called by 0x48F620 (vehicles) |
| 0x47B610 | `CanBuildAt(x0,y0,x1,y1, waterSide)` | The rectangle must lie inside the **no-build border** (dword_59D93C). Every vertex: type ≠ 0xA, type ≠ 8, slope ≤ MaxSlope, flags `+9 & 6` == 0. It records whether water (types 1..3) and land were seen. If `waterSide`, it needs **both** water and land (docks, shipyards); otherwise it needs no water. Callers: 0x422624 (war-map side, *(guess)*: AI site picking), 0x4A072D/0x4A15C0 (placement) |
| 0x47B770 | `CanBuildLine(x0,y0,x1,y1, overWater)` | A **line structure** of 2..25 vertices (euclidean length), inside the border. Steps along the line: type ≠ 8, slope ≤ MaxSlope, `+9 & 2` clear. Water vertices are allowed only if `overWater`, and then at least one is required. These are bridges (types 0x1BC–0x1BF); fences and walls (0x1C3–0x1C8) use `overWater` = 0 *(guess)*. Caller 0x4A0796 |
| 0x47C280 | `MarkOccupied(x,y,r,bits)` | vertex`+9 \|= bits` for every vertex within world radius r (distance 0x43FF00). Callers 0x48A950 and 4 others: buildings and objects marking their footprint |
| 0x47C3A0 | `ClearOccupied(x,y,r,bits)` | `+9 &= ~bits` |
| 0x47C4C0 | `Scorch(x,y,radius,intensity)` | Invalidates the screen rectangle of the area (0x40AED0, iso projection with 1/√2 constants). For each vertex within the radius, the byte at **battle world (dword_4D2550)+0x8C→+0x20** [y·w+x] += `(radius−dist)·(4 + intensity/128)/radius`, clamped to 255. These are burn marks *(guess)*: that byte map is probably the terrain darkening/damage layer |
| 0x47C7E0 | `ClampWorld(&x,&y)` | clamps x to [0, `+0x1C`−1] and y to [0, (h·cell>>16)−1]; returns whether it clamped |
| 0x47C830 | `SetPointLight(x,y)` | vertex`+1` = `ftol(flt_4DA76C)` (night light level from the time of day). Caller 0x4896D0 (vehicle lights, *(guess)*) |
| 0x47C8A0 | `ClearPointLight(x,y)` | vertex`+1` = 0. Caller 0x489710 |
| 0x47C900 | `StampLightMask(mask32x32, x0,y0,x1,y1, mirror)` | Samples a 32×32 byte mask across the vertex rectangle (mirrored in X if `mirror`). Where `mask·4 < 255`: vertex`+1` = (255 − mask·4)·nightLight/255. Building light pools. Caller 0x48E460 |
| 0x47CAB0 | `ClearLightMask(...)` | same sampling; sets `+1` = 0. Caller 0x48E330 |

Settings registered here (static initialisers in the 0x4B83C0 init table):

- 0x47B4A0: `[Tweak] "Max Slope For Building On"`, int, **default 6** (object 0x59D948, value dword_59D95C).
- 0x47B4E0: `[Tweak] "No-build border width (vertices)"`, int, **default 16** (object 0x59D928, value dword_59D93C).

### 2.5 WObjectList (vtable 0x4AD5F0)

This is a list of battle objects. It is created by WWarMapWorld 0x417CD0 (3 lists at +0x110…, *(guess)*: per-sector unit, construction and resource lists), by 0x428C3D, and by 0x47FC9D (next piece). Layout: `+0x1C` byte flag (ctor arg 2), `+0x20` std::list head (node {next, prev, obj}), `+0x24` count, `+0x28` owner (ctor arg 1).

| Addr | Slot | Name | What it does |
|---|---|---|---|
| 0x47CD30 | ctor | `WObjectList(owner, flag)` | |
| 0x47CDA0 / 0x47CDC0 | 0 | dtor | releases every member (refcount word `+4`; deleted at 0) and frees the nodes (0x4211D0) |
| 0x47D240 | 2 | `Load(stream)` | Reads the tag (vtbl+0x60), the count (+0x5C), then per object the u32 type (+0x68), creates it (**0x446B30 = object factory by type id**) and calls obj vtbl+8 (Load). Then: type **0x3AC (940)** → 0x48AA30 (unclear). If class (0x440700) ≠ 0 → owner(`+0x28`)→0x42A090(obj), i.e. handed to the war-map sector. If position x ≤ 0 and y ≤ 0 → 0x48AA30. Otherwise push_back |
| 0x47D360 | 3 | `Save(stream)` | Writes tag **'BMOL'** (0x4C4F4D42), the count of members whose type ≠ **0x3DB (987, a transient effect object, 0x4873F0)**, then per member its u32 type (+0x2C) and the object's own save (obj vtbl+0x0C) |
| 0x47D030 | 5 | `ActivateAll` | for each member with x+y+z ≠ 0: vtbl+0x24 (Place/spawn children), +0x14 (Start); if vtbl+0x64 (dead?) → +0x78 (Remove) |
| 0x47D110 | 6 | `ShutdownAll` | each member vtbl+0x18 |
| 0x47CEE0 | 7 | `OnMessage(msg,w,l)` | **0x80A0** is re-posted to every member through 0x4355A0 with growing delays: d = 30, then d += 30 + rand()%30 each time, so they don't all fire on the same tick. Every other message is forwarded synchronously (member vtbl+0x1C) |
| 0x47D0B0 | 9 | `StartAll` | members with `+0x18`==0: vtbl+0x24, +0x14 |
| 0x47D150 | 10 | `RemoveAll` | each member vtbl+0x78 |
| 0x47CFA0 | – | `RegisterAll` | each member: dword_4D2550→0x43C770(obj) (adds it to the battle world) |
| 0x47CFE0 | – | `MoveAllTo(dest)` | dest→0x4480F0(obj) for each member, then dest→0x4802E0 |
| 0x47D190 / 0x47D1E0 | – | `PushBack` / `PushFront` | addref |
| 0x47D430 | – | `ForEach_4C(arg)` | each member vtbl+0x4C(arg) |
| 0x47D480 | – | `ForEachAlive_68(arg)` | each member where vtbl+0x9C returns false: vtbl+0x68(arg) |
| 0x47D4E0 | – | `RemoveAllSafe` | addref self, vtbl+0x78 on each member, release self |
| 0x47D540 | – | `ClearPositions` | each member: x = y = z = 0 (`+0x78/+0x7C/+0x84`) |
| 0x47D590 | – | `CommandAll5` | for each member builds a command (0x447D80(5, shift = GetAsyncKeyState(VK_SHIFT)) on obj+8) and issues it (0x447DB0). Callers 0x428D70+0x6CF and one other. Probably select-all with Shift adding to the selection *(guess)* |
| 0x47D600 | – | `Front` | |
| 0x47D610 / 0x47D660 | – | `CountSide0` / `CountSide1` | count members whose type-table side (0x4406E0, 0x51A0FC) is 0 or 1. Used by the **battle-end check** 0x43A980 (the world's `+0x118` list; side 0 = Human, side 1 = Martian, picked by 0x404BA0) |
| 0x47D6B0 | – | static init | `[Tweak] "ISO obj col rad mul"`, float, **default 0.66** (0x3F28F5C3; value flt_59D98C) |
| 0x47CCF0 | – | static init | `[Debug] "Arrow scenery load fix"`, bool, default 1 (object 0x59D960). **No code reads it** |

### 2.6 Battle-map object base (vtable 0x4AD620, *WMapObject*)

It derives from WSelectableObject (ctor 0x440120, dtor 0x440270, base OnMessage 0x440300). The constructor 0x47D6F0(type) is called by 0x487730/0x487A38 (transient and effect objects), 0x489E42 (vehicles) and 0x4949A8, so it is the base of most battle objects.

Constructor 0x47D6F0:

- The embedded child list (`+0x50`, a WObjectList-like object built by 0x449B60; its std::list head is at `+0x70`).
- Position `+0x78` x, `+0x7C` y, `+0x80` (height above ground / attachment z), `+0x84` z.
- `+0x88..+0xA0` = 0, scale `+0xA4` = 1.0, `+0xA8` = 0.
- `+0xAC` = collision radius = type table word 0x51A134[type].
- `+0xB0` attachment spec, `+0xB4` emitter array.
- 0x47FB20 (next piece), then the flags `+0xB8`:

| Bit | Meaning |
|---|---|
| 0 | class (0x51A100) == 1 |
| 1 | class ∈ {0, 2, 3} |
| 2 | type in 0xCF..0xE5 (*unclear*: a run of scenery types) |
| 4 | cleared in Start |
| 5 | set by 0x47D940 |
| 8, 10 | set (|= 0x500) |
| 7, 9 | cleared |
| 9 | 0x47F320 sets it |
| 11 | "weapon" child |

Vtable (offset: function):

| Offset | Function |
|---|---|
| 0x00 | 0x47D9B0 deleting dtor (0x47D9D0 → 0x449BD0 child list, 0x440270) |
| 0x08 / 0x0C | 0x4404A0 / 0x440550 (SELO load / save, base) |
| 0x14 | 0x47EF30 Start |
| 0x18 | 0x47F260 Shutdown |
| 0x1C | 0x47DF20 OnMessage |
| 0x24 | 0x47EE10 Place |
| 0x28 | 0x47EB10 SetType |
| 0x2C | 0x47D930 (`fild [this+0x40]`) |
| 0x30 | 0x47FAC0 |
| 0x34 | 0x47F480 |
| 0x38 | 0x47F8D0 |
| 0x3C | 0x47F900 |
| 0x40 | 0x47F940 |
| 0x44 | 0x47F850 |
| 0x48 | 0x47F340 PoseChildren |
| 0x4C | 0x47F630 |
| 0x50 | 0x47F220 OnChildRemoved |
| 0x54 | 0x47F770 |
| 0x58 | 0x47FEB0 |
| 0x60 | 0x47F570 |
| 0x78 | 0x47EAA0 Remove |
| 0x7C | 0x47F960 |
| 0x80 | 0x47F740 |
| 0xA0 | 0x47FFC0 |
| 0xA4 | 0x47D940 SetFlag5 |
| 0xB4 | 0x47D960 (ret 0) |
| 0xBC | 0x47D970 (ret 1) |
| 0xC0 | 0x47D980 (ret 0) |

The 0x47Fxxx and 0x48xxxx entries are in the next piece.

| Addr | Name | What it does |
|---|---|---|
| 0x47EB10 | `SetType(type)` | `+0x20` = type. Releases the old renderer `+0x4C` (0x40EBE0). GType = type table dword 0x51A118[type]; its class (graphics table 0x4DAFD0, 56 bytes per GType) picks the renderer: **0 → sprite** (0x44 bytes, 0x40EC50); **1 → RTypeISO** building (0x54 bytes, 0x49DDA0), whose collision radius = max(2·iso`+0x2A`, iso`+0x26`) × "ISO obj col rad mul" → `+0xAC` and type table word 0x51A134 (also 0x51A136 if it has a parent `+0x44` or vtbl+0xB8); **2 → WOF 3D model** (0x5C bytes, 0x49DA50), with model dimensions `+0x3C/40/44` multiplied by the scale `+0xA4`; **6 →** 0x50 bytes (0x49D710, args 0x10, 0); **14 →** 0x40 bytes (0x49D9C0). Other classes get no renderer |
| 0x47EE10 | `Place` (slot 0x24) | scale ×= type table float 0x51A198. vtbl+0x28, then vtbl+0x40(x,y,z). Then **creates the child puppets** from the type table list 0x51A128[type]. Node: `+0` next, `+4` child type, `+8` scale, `+0xC/+0xE/+0x10` s16 offset xyz, `+0x12..+0x18` 4 × s16 attachment (piece and vertex indices, only if `+0x12` ≥ 0) → child`+0xB0`. Each child is made by the factory, pushed into the child list, gets child`+0x44` = parent, then vtbl+0x38(offset), the scale, and vtbl+0x24 |
| 0x47EF30 | `Start` (slot 0x14) | vtbl+0xA0, clears flag 0x10, 0x47FB20. `+0x3C` = number of children with flag bit 11 (**weapons**). Starts the children (vtbl+0x14). Allocates `+0xB4` = an array of {emitterDef*, count, accumulator} for each node in the type table list 0x51A12C[type] (smoke and particle emitters), null-terminated. Then vtbl+0xA4 |
| 0x47F050 | `TickEmitters` | Only when `[Options] "Show Smoke & particles"` (byte_4B880C, default 1, registered 0x402400). Per emitter: accumulator += def`+0x18` (rate). For each whole unit: a random angle `rand()%def+8` and a random radius `def+0xC + rand()·(def+0x10 − def+0xC)/32767` (flt_4AD700), converted to an offset by 0x43FE90 (polar → xy), plus a second random 0..0xFFF angle. It creates the particle type def`+4` (factory), gives it velocity vtbl+0x80, places it at the parent position (vtbl+0x40), sets `+0x94` = def`+0x14` (lifetime *(guess)*), vtbl+0x24, and adds it to the world render list (dword_4D2550+0x90, vtbl+0x14). Count++. If def`+0xA` (max) ≠ 0 and the count exceeds it, **all** emitters are freed (`+0xB4` = 0). Caller 0x47F050 ← 0x4??? (one caller, the object tick) |
| 0x47F340 | `PoseChildren` (slot 0x48) | For 3D (WOF) objects: vtbl+0x98 and vtbl+0x2C(0,0,x,y,z) give the pose, then 0x49E690 transforms the model. Renderer `+0x26` = (model maxY − minY)/2. For each child with an attachment spec, it reads the transformed **model vertex** (piece `[0]`, vertex `[2]`) and positions the child there (vtbl+0x38 relative, vtbl+0x7C absolute, `+0x80` = vertex z). This is how turrets and weapons ride on animated models |
| 0x47F220 | `OnChildRemoved(child)` (slot 0x50) | removes the child from the list (0x440A90). If the object has no renderer, no children and no emitters, it removes itself (vtbl+0x78) |
| 0x47F260 | `Shutdown` (slot 0x18) | children vtbl+0x18 and detach; clears the list (0x449CF0); scale = 1; base 0x440470; frees `+0xB0`; releases the renderer; 0x4393D0; frees `+0xB4` |
| 0x47EAA0 | `Remove` (slot 0x78) | addref; vtbl+0x18; parent`+0x44`→vtbl+0x50(this); if class ≠ 4, removes it from the selection (dword_4E9B70+0x58 → 0x440CB0); group `+0x34`→0x448210; release |
| 0x47EB00 | `GetGroup` | returns `+0x34` (13 callers) |
| 0x47DA20 | `TypeWord13A` | (s16) type table 0x51A13A[type], *unclear* (17 callers, including the war map) |
| 0x47DA40 | `ReleaseRenderer` | |
| 0x47D940 / 0x47F320 | | set flag bit 5 / bit 9 |
| **0x47DF20** | `OnMessage(msg, w, l)` | See below |
| **0x47E1E0** | `BuildCommandPanel(menu)` | Fills the unit or building command panel with 0x432E90(menu, group, cmd, iconGType, textId). Details in section 4 |
| **0x47DA80** | `HasAbility(cmd)` | Hard-coded table of type id × research → special commands (see 4) |
| 0x47E810 | `QueryCommand(cmd)` | for msg 0x8130: **0x8131** → mobile (type table s16 0x51A142 ≠ 0); **0x8132** → armed (`+0x3C` ≠ 0); 0x80D5–0x80EF, 0x80F0 → HasAbility; anything else → false |

`OnMessage` (0x47DF20) handles:

- **0x111** with `w` = 0x813E: `l` = 0x8065 resets the build queue (owner `+0x48`→`+0x54`: `+0x20`=0, then vtbl+0x14(`+0x28`→`+0x3C`)); `l` = 0x813F toggles suspend (0x45FEF0/0x45FE60).
- 0x8057 → Remove.
- **0x805C** → BuildCommandPanel(l), then the base handler.
- **0x8066** → sets renderer flag bit 4 and calls 0x47F6F0 on each child (*(guess)*: becomes dead or wreck).
- 0x80A7 → vtbl+0x3C, then the base handler.
- **0x80D1** → position = ((s16)l, (s16)(l>>16)).
- **0x8130** → QueryCommand(w).
- Anything else → WSelectableObject::OnMessage 0x440300.

---

## 3. Data structures and globals

### 3.1 Globals

| Address | Meaning |
|---|---|
| 0x59D940 | WLandscape* (current battle map) |
| 0x59E418 | WRouteFinder* |
| 0x4D2550 | battle world (`+0x48` map index, `+0x8C`→`+0x20` scorch byte map, `+0x90` render list) |
| 0x4E9B80 | calendar (`+0x28` hours, float) |
| 0x4E9B70 | game (`+0x58` selection, `+0x118` battle object list) |
| 0x4E9B7C | (`+0x54` flag gating Retreat) |
| 0x4F76E8 | (non-zero needed for Retreat; zeroed by 0x439770) |
| 0x4EAEC8 | battle-map table: 0xC0 bytes per map; name at +0 |
| 0x51A0F0-ish, stride 0xB0 | **object type table** (from OBJ.ojd, 0x437750). See the field list below |
| 0x4DAFD0 | GType table (56 bytes per id; dword +0 = class) |
| 0x4DA764 | ambient |
| 0x4DA76C | night or point light level |
| 0x4DA778 / 0x4DAE28 | sun direction x / y |
| 0x4DAEBC / 0x4DAEC0 | sun elevation terms (set by 0x40B030 from the 24-entry hour table at 0x4DA828, 64 bytes per hour) |
| 0x4B880C | Show Smoke & particles |
| 0x58FE88 | SFX table |
| 0x5898B0 | samples |
| 0x58FE80 | sample count |
| 0x58FE78 | `[Sound] "Search Libraries First"` (bool, default 0) |
| 0x58FE7C | `[Sound] "Unload Limit"` (int, default 0) |
| 0x588CDC, 0x588F28, 0x588F2C, 0x588FA0, 0x588FB4, 0x588FD0, 0x588FD4, 0x588FD8, 0x588FDC, 0x588FF8, 0x588FFC, 0x589000 | research items tested with 0x45B890 (`IsResearched`). Their names are in the research piece |

Object type table fields (absolute address of entry 0; add `type·0xB0`):

| Address | Field |
|---|---|
| 0x51A0FC | side (0 Human, 1 Martian) |
| 0x51A100 | class |
| 0x51A118 | GType |
| 0x51A128 | child-puppet list |
| 0x51A12C | emitter list |
| 0x51A134 | s16 collision radius |
| 0x51A136 | s16 second radius |
| 0x51A13A | s16 *unclear* |
| 0x51A142 | s16 "mobile" (non-zero → can move or roam) |
| 0x51A198 | float scale |

### 3.2 WLandscape layout (0x6C bytes)

| Offset | Meaning |
|---|---|
| +0x00 | vtbl 0x4AD590. Slots: 0 dtor 0x47B010, 1 0x4356A0, 2/3 0x4356B0, 4 0x435590, 5 0x435560, 6 0x435570, 7 OnMessage 0x47B0D0, 8 nullsub |
| +0x1C, +0x20 | CLS hdr +0x10 / +0x14: world width, and the projected world height (5001 and 3536 on LAND01 = 250·20 and 250·20/√2) |
| +0x24, +0x28 | grid width and height in vertices (hdr +0, +4; 251×251 on LAND01) |
| +0x2C | vertex count (hdr +8) |
| +0x30 | **cell size, 16.16** (hdr +0x24 = 0x140006 ≈ 20.0) |
| +0x34 | half cell |
| +0x38 | screen row step, 16.16 (hdr +0x28 ≈ 14.145 = 20/√2) |
| +0x3C | **height scale, 16.16 (hdr +0x2C ≈ 1.174)** |
| +0x40 | battle-map index |
| +0x44 | file kind (1 .ils, 2 .cls) |
| +0x48 | screen y offset = (h[0]·scale>>16) + hdr+0x18 + hdr+0x30 |
| +0x4C | hdr +0x30 |
| +0x50, +0x54 | sun direction |
| +0x58 | raw height bytes (CLS +0x44) |
| +0x5C | file buffer |
| +0x60 / +0x64 | vertInf handle / data |
| +0x68 | CLS header |

The CLS header words +0x0C (triangles), +0x1C, +0x20, +0x34, +0x38, +0x3C and +0x40 are **not used** by CreateTerrain. The data after the height bytes (several MB) is not touched in this piece. The renderer (ISOLib 0x410E00 / 0x49DE20) presumably uses it; *unclear*.

### 3.3 vertInf (10 bytes per vertex, row-major, w×h)

| Offset | Type | Meaning |
|---|---|---|
| +0 | u8 | static light (LightTheMap, smoothed) |
| +1 | u8 | dynamic light (point and mask lights, night) |
| +2 | u8 | *unclear* |
| +3..+5 | s8 ×3 | normal ×127 |
| +6 | s16 | height in world units (×√2) |
| +8 | u8 | ATM attribute: low nibble = type (0 land; **1–3 water**; 8 impassable; 0xA no-build; others plain land), high nibble = slope − 1 |
| +9 | u8 | flags: 1 set at load, bit 1 = blocked (footprint and line checks), bit 2 = occupied |

Measured over all 30 ATMs: low-nibble counts are 1: 152k, 2: 70k, 3: 130k, 4: 432k, 5: 290k, …, 8: 17k, 10: 60k. High nibble 0..7.

---

## 4. Messages, commands, strings and tuning

### 4.1 Messages

| Id | Handled by | Meaning |
|---|---|---|
| 0x804B | WLandscape | scorch (explosion) |
| 0x80A0 | WObjectList | staggered broadcast (30 + rand%30 ticks apart) |
| 0x8057 | WMapObject | remove |
| 0x805C | WMapObject | build the command panel |
| 0x8066 | WMapObject | see 2.6 |
| 0x80A7 | WMapObject | vtbl+0x3C |
| 0x80D1 | WMapObject | set position from packed s16 x/y |
| 0x8130 | WMapObject | query a command (0x8131 mobile, 0x8132 armed) |
| 0x111 | WMapObject | WM_COMMAND, panel buttons 0x813E/0x8065 and 0x813E/0x813F |

### 4.2 Command panel (0x47E1E0)

Each entry is (group, cmd, icon GType, text id).

- **Build**: (0x813E, 0x8065, 0x20, 0x470 "Build"). Only if the owner has a build panel (`+0x48`→`+0x54`), 0x43D010(owner `+0x48`) is true, and the object is class 1 or 0x42AA70 is true. If the panel's `+0x128` is set, also add (0x813E, 0x813F, 0x22, 0x495 "Suspend building tasks") when `+0x12C` is set, otherwise (…, 0x23, 0x496 "Resume building tasks").
- **Stances**, group 0x80CE, w = stance. Shown only if armed (`+0x3C`):

| Stance | Icon | Text | Condition |
|---|---|---|---|
| 3 | 0x34F | 0x474 "Roam and Attack" | mobile only |
| 2 | 0x34E | 0x473 "Attack" | |
| 1 | 0x34D | 0x472 "Retaliate" | |
| 0 | 0x34C | 0x471 "Avoid" | mobile only; shown even when unarmed |

- **Retreat**: (0x80CE, 0x80B7, 0x2D, 0x479). Mobile, dword_4E9B7C+0x54 set, 0x43D010(owner) false, and dword_4F76E8 ≠ 0.
- **Abandon**: (0x80CE, 0x8067, 0x34B, 0x46F). Always.
- **Force Fire**: (0x80F3, 0x80ED, 0x1E, 0x48A). Armed only.
- **Specials**, group 0x80F3, each shown only if `HasAbility`. Three command ids per ability are the Mk1/Mk2/Mk3 strength. Type ids follow the factory table at 0x43D760…:

| Cmd | Text (icon) | Allowed when |
|---|---|---|
| 0x80EF | Submerge 0x48C (0x349) | type 0x16–0x18 SubmersibleMk1–3 |
| 0x80EE | Tunnel 0x48B (0x34A) | 7–9 TunnellingTrackLayerMk1–3 |
| 0x80D5 | Black Dust 0x482 (0x25) | 0x42, 0x43 FightingMachineMk2/3; 0x44, 0x45 TempestMk1/2; 0x47 BombardingMk1 |
| 0x80D6 | Black Dust | 0x46 TempestMk3; 0x48 BombardingMk2 |
| 0x80D7 | Black Dust | 0x49 BombardingMk3 |
| 0x80D8 / 0x80D9 / 0x80DA | Shock Bomb 0x483 (0x26) | research 0x588FA0, and type 0x45/0x47, 0x46/0x48, 0x49 respectively |
| 0x80DB / 0x80DC / 0x80DD | Explosives 0x484 (0x351) | 0x20 MortarBikeMk1 / 0x11, 0x21 (SappersLorryMk1, MortarBikeMk2) / 0x12, 0x22 |
| 0x80DE / 0x80DF / 0x80E0 | Infiltrate 0x485 (0x352) | {0x11, 0x12, 0x20, 0x50, 0x56} / {0x21, 0x51, 0x57} / {0x22, 0x52, 0x58} (sappers, bikes, ScanningMachine, XenoTelepath) |
| 0x80E1 / 0x80E2 / 0x80E3 | Pulse 0x486 (0x353) | research 0x588FB4 and ElectricMachine 0x4A/0x4B/0x4C; or research 0x588FD8 and Scanning 0x50/0x51 |
| 0x80E4 | Freeze 0x487 (0x354) | research 0x588FDC and Scanning 0x50–0x52; or 0x588FF8 and XenoTelepath 0x56; or 0x588CDC and SelfPropelledGun 0x0B/0x0C |
| 0x80E5 / 0x80E6 | Freeze | 0x588FF8 and 0x57 / 0x58 |
| 0x80E7–0x80E9 | Scare 0x488 (0x355) | 0x588FFC and XenoTelepath 0x56/0x57/0x58 |
| 0x80EA–0x80EC | Control 0x489 (0x356) | 0x589000 and 0x56/0x57/0x58 |
| 0x80F0 | Scan 0x48D (0x358) | (0x588FD0 or 0x588FD4) and Scanning 0x50–0x52; or (0x588F28 or 0x588F2C) and the object is the owner's comms building: type 0x33E "Martian Base" if the owner's side value (`+0x48`→`+0x48`→`+0x48`) == 3, else 0x78 CommunicationsCentre |

### 4.3 Type ids used here

From the factory name tables at 0x43D760–0x43E2D0:

| Ids | Types |
|---|---|
| 1–3 | ArmouredLorry |
| 4–6 | ArmouredTrackLayer |
| 7–9 | TunnellingTrackLayer |
| 0xA–0xC | SelfPropelledGun |
| 0xE–0x10 | MobileAA |
| 0x11–0x12 | SappersLorry |
| 0x13–0x15 | Ironclad |
| 0x16–0x18 | Submersible |
| 0x19–0x1B | ObservationBalloon |
| 0x1C–0x1D | ConstructionWorkers |
| 0x1E–0x1F | MobileRepair |
| 0x20–0x22 | SpecialForcesMortarBike |
| 0x23–0x3C | Human buildings |
| 0x3E–0x40 | ScoutMachine |
| 0x41–0x43 | FightingMachine |
| 0x44–0x46 | Tempest |
| 0x47–0x49 | BombardingMachine |
| 0x4A–0x4C | ElectricMachine |
| 0x4D–0x4F | FlyingMachine |
| 0x50–0x52 | ScanningMachine |
| 0x53–0x55 | Constrictor |
| 0x56–0x58 | XenoTelepath |
| 0x59–0x5A | HandlingMachine |
| 0x5B–0x5C | DiggingMechanism |
| 0x5D–0x5F | Drone |
| 0x60–0x7A | Martian constructions (0x78 CommunicationsCentre) |
| 0x116 | Parliament |
| 0x1BC–0x1BF | Bridges |
| 0x1C3–0x1C8 | Fences and walls |
| 0x33E | Martian Base |
| 0x3AC | special-cased on load |
| 0x3DB | transient effect, not saved |

### 4.4 Strings

"Loading SFX %s", "Unloading SFX %s", "SFX\SFX.WoW", "VOX\HUMAN.WoW", "VOX\MARTIAN.WoW", "SFX\", "HUMAN\", "MARTIAN\", "VOX\", "\HUMAN\", "\MARTIAN\", ".snd", ".WoW", "maps\", ".ils", ".cls", "atm", "vertInf", "ISOTemp", "lightTheMap", "WLandscape::CreateTerrain: couldn't load…", "WLandscape::CreateTerrain", "WCmdTarget", "!Reaper!", "Reaper Fatal Error", "timeGetDevCaps failed!", "timeSetEvent failed...", "timeKillEvent failed to kill timer...".

### 4.5 Settings

| Setting | Type | Default |
|---|---|---|
| `[Tweak] Max Slope For Building On` | int | 6 |
| `[Tweak] No-build border width (vertices)` | int | 16 |
| `[Tweak] ISO obj col rad mul` | float | 0.66 |
| `[Debug] Arrow scenery load fix` | bool | 1 (unused) |
| `[Sound] Search Libraries First` | bool | 0 |
| `[Sound] Unload Limit` | int | 0 |
| `[Screen] Use Reaper` | bool | 0 (registered in 0x44DC60) |
| `[Options] Show Smoke & particles` | bool | 1 (read here; registered in 0x402400) |

### 4.6 Constants

| Constant | Where | Use |
|---|---|---|
| 450 | SFX table | entries |
| 3 × 5 | SFX entry | sample variation groups |
| 32 | Reaper | frame and audio ring size |
| 16 | LightTheMap | vertices of shadow ray |
| 2..25 | line builds | vertices |
| ×√2 (92682/65536) | CreateTerrain | heights |
| ×127 | CalcNormals | normals |
| 4 + i/128 | Scorch | factor |
| 255 | Scorch | clamp |
| 1/32767 | emitters | radius random |
| 30 + rand%30 | 0x80A0 broadcast | stagger |

---

## 5. Cross-references to other pieces

- **Previous piece (sound):** loader 0x463D10 → 0x466B40/0x466C30/0x466C70. 0x4660A0 → 0x466750. Sound object ctor 0x4648C0 → 0x466BC0. 0x464A50/0x464AB0/0x4641xx/0x4652B0/0x4653A0 → the list functions. 0x465D10 creates buffers; 0x465F90 is the sample registry; 0x465B20 is called before loading.
- **FMV:** 0x44DCA0 picks 0x44E060 (Reaper) or 0x44DE20 (Smacker) by `Use Reaper`. 0x44E060 → 0x478130 / 0x4780E0 / 0x477DC0 / 0x478300.
- **Time of day and shade:** 0x40B030 (hour table) feeds `+0x50/+0x54` and flt_4DA76C. LoadShadeTables 0x40B570 → LightTheMap.
- **Battle-map setup:** 0x439770 creates WLandscape and sets dword_4D2550. 0x43A980 counts sides for the end of battle. 0x439C0D uses HeightAt.
- **Next pieces:** 0x47F480–0x47FFxx (WMapObject's remaining virtuals, 0x47FB20, "Collisions" 0x47FC50). 0x480150 "BM Groups". 0x485930 battle input (picking). 0x486E80+ effects (0x488200 sends 0x804B). 0x489340+ vehicles (lights, footprints, 0x48F620 passability). 0x493DF0 WRouteFinder. 0x49D710–0x49E690 renderers (RTypeISO, WOF). 0x4A0630+ construction placement (CanBuildAt, CanBuildLine, picking). 0x4A3020 NormalAt.
- **Earlier pieces:** WSelectableObject 0x440120–0x440CB0. Object factory 0x446B30. Command objects 0x447D80/0x447DB0. Research 0x45B890. Panel 0x45FE60/0x45FEF0. Menu builder 0x432E90. Path and file helpers 0x4171E0–0x4177F0, FILE_Load 0x415200, MEM 0x4148A0/0x414BC0. Settings classes 0x411540 (bool), 0x411890 (bool, external storage), 0x4117A0 / 0x412160 (int), 0x416500 (float). War-map sector hand-off 0x42A090.

---

## 6. Missing in WoWEngine / candidate tasks (in priority order)

WoWEngine has no battle map yet: `src/` has only the core formats, the front end and the war map. Everything in 2.4–2.6 is missing, and the format readers have a few errors.

1. **Fix the CLS reader (`core/Models.cpp` Terrain::Load).** It uses `heightScale = U32(cls, 0x3C)`, but the game uses **header +0x2C** (0x47BB92 → `+0x3C`) as the 16.16 height scale. Header +0x24 is the cell size (16.16), +0x28 the screen row step, and +0x10/+0x14 the world extents. World height per vertex = `((byte·hdr[0x2C]>>16) + hdr[0x30]) · √2`. *Address: 0x47B900.*
2. **WLandscape model.** vertInf (3.3) built from CLS + ATM (ATM goes per cell into the top-left vertex's +8), normals (0x47C100), and the height, normal and picking queries (0x47B120, 0x47B170, 0x47B240, 0x47B3F0). These underpin all battle-map movement, placement and rendering. Use the integer and fixed-point arithmetic exactly (16.16, 8-bit fractions, `sar` rounding) to match.
3. **Terrain lighting.** LightTheMap 0x47BD80: Lambert term (0x40CA90), 16-step shadow walk toward the sun, 3×3 smoothing (0x43FD10), zeroed border. Rerun on every time-of-day shade change (0x40B570+0x138), with sun values from 0x40B030.
4. **Build and move rules** with the Tweak defaults: IsAreaPassable 0x47B520 (no water, no type 8, slope ≤ 6); CanBuildAt 0x47B610 (16-vertex border, type 0xA/8, flags 6, water-side needs both water and land); CanBuildLine 0x47B770 (2–25 vertices, bridges need water). Footprints through 0x47C280 / 0x47C3A0.
5. **Unit and building command panel** (0x47E1E0) and the **ability table** (0x47DA80 / 0x47E810, msg 0x8130): exact items, ordering, icons, text ids and research gates are in 4.2. This is also the source of truth for the AI or UI asking "can this unit do X".
6. **Battle object framework:** WMapObject ctor flags (0x47D6F0), SetType → renderer by GType class (0x47EB10) including the ISO collision radius × 0.66, child puppets from the type table (0x47EE10) posed on WOF model vertices (0x47F340), Start / Shutdown / Remove (0x47EF30 / 0x47F260 / 0x47EAA0), and the message handler (0x47DF20).
7. **Smoke and particle emitters** (0x47F050), gated by `[Options] Show Smoke & particles`: rate accumulator, random polar spawn, max-count shutdown.
8. **WObjectList and BMOL save/load** (0x47D240 / 0x47D360). SaveGame.h only names BMOL. Implement the format: tag, count excluding type 0x3DB, then per object u32 type + object data. On load, class ≠ 0 goes to the sector and position (0,0) goes to 0x48AA30. Also the staggered 0x80A0 broadcast (0x47CEE0) and the side counts used for the end of battle (0x47D610 / 0x47D660 ← 0x43A980).
9. **Night and point lights and explosion scorch:** 0x47C830 / 0x47C8A0 / 0x47C900 / 0x47CAB0 (vertex +1 using flt_4DA76C), and Scorch 0x47C4C0 through msg 0x804B (radius obj+0xAC, intensity obj+0xCA, factor 4 + i/128, clamp 255).
10. **Sound sample selection is different.** The engine (`platform/Sound.cpp` Play) picks a **random** sample from **group 0**. The game takes the group from the caller (0x4648C0 arg) and picks **round-robin** within it (0x466BC0, cursor per group, falls back to group 0 when the group is empty). Fix both.
11. **Sound lookup order is different.** The engine always tries a loose `SFX\` / `VOX\<name>.snd` first. The game searches **libraries first** and uses loose files only when they aren't in a library, or, in debug builds (byte_4D12F4), when a side- or language-specific override exists (`SFX\HUMAN\`, `VOX\English\`, `VOX\English\HUMAN\`; 0x466910). It also searches both VOX libraries regardless of side. Minor; matters only for mods.
12. **MSVC `rand()`.** Several gameplay paths here use `_rand` (emitters, the 0x80A0 stagger) and so do many elsewhere. The engine uses `std::rand`. For faithful behaviour and for replaying `playback.%d` recordings (0x42D290), implement the MSVC LCG (`x = x·214013 + 2531011; return (x>>16) & 0x7FFF`). Remember the original kept a **separate state per thread** (game versus render thread).
13. *(Skip, document only)* Reaper `.fmv` playback: `Use Reaper` defaults to 0 and no `.fmv` files ship. The engine's Smacker/MP4 path is correct; at most, read and ignore the setting.
14. *(Low)* `[Sound] Unload Limit` and `Search Libraries First` settings: the engine caches everything, so only the order in task 11 is observable.

Open questions for the next pieces:

- What 0x48AA30 does with objects at (0,0) and with type 0x3AC.
- What type-table word 0x51A13A means.
- The meaning of ATM types 4–7, 9 and 11–15.
- What the multi-MB tail of the CLS file is (the renderer's data?).
- Which of the six playing-sound lists maps to which type.
