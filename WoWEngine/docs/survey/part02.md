# Part 02: 0x40D210 – 0x417A80

Survey of the IDA listing `pieces/part02.txt` (239 functions, ~19.3k lines). Addresses are vanilla `WoW_patched.exe`.
"Guess" marks things inferred from code shape rather than proven; "unclear" marks what I could not settle.

## 1. Overview

This piece is almost entirely **engine infrastructure**: it has no game rules. It sits between the front-end menus
(0x4057B0–0x40A1B0, part 01) and WWarMapWorld (0x417CD0, part 03). In address order it holds:

| Range | Subsystem |
|---|---|
| 0x40D210–0x40E650 | **WRenderer** (rest of the class built at 0x40CF00): dirty-cell renderer with a wrap-around (toroidal) scroll buffer, z-sorted object lists, per-frame redraw of damaged rectangles only |
| 0x40E680–0x40E6E0 | Busy cursor ("wait cursor") RAII object |
| 0x40E700–0x40E780 | RType object pool allocator (1024 × 0x2AC bytes per chunk) |
| 0x40E7A0–0x40F040 | **RTypeBase / WObject / RTypeSprite** (GType table 0x4DAFD0, sprite frame, animation, bounds, shade level) |
| 0x40F060–0x40F5E0 | Static initialisers for `[Colours]` text levels and `[Tweak] Text Line Spacing` |
| 0x40F620–0x40FBF0 | **RTypeText** (text objects: up to 16 lines, alignment flags, measuring, drawing, printf) |
| 0x40FD30–0x410190 | **PolyLib** C stubs around hand-written asm blitters/polygon fillers; PolyLib::Setup (shade hash tables) |
| 0x410230–0x410D9F | **DamageMap** (byte grid of dirty cells) and the dirty-rectangle splitter |
| 0x410E00–0x4113F0 | **ISOLib** (isometric/3D object lighting & dither tables; stubs to asm at 0x457xxx) |
| 0x411540–0x4122B0 | **Settings**: registry-bound variables (`RegVar`), Bool/Int/Float/String/Path settings, chdir helpers |
| 0x4122F0 | DirectX version probe (DDraw/DInput) |
| 0x412660–0x414180 | **CD audio** through MCI (`_ciSendCommandA`): open/close, play thread, stop, pause, next/prev track, ±seconds seek, status queries, save/resume position |
| 0x414410–0x414740 | WText / TEXT.ojd (already ported), `GetText(id)`, `PlayTextSpeech(id)` |
| 0x414780–0x415410 | **MEM_*** heap manager (4 heaps, named blocks, stats) |
| 0x414DE0–0x415200 | Archive open (`dat\dat.wow`, `maps\maps.wow`), lookup, `FILE_Size`, `FILE_Load` (already ported) |
| 0x415470–0x416180 | **Huffman ("FFUH") codec class**: compress + decompress (memory and streamed file) |
| 0x4161E0–0x416400 | Word wrap, rect helpers |
| 0x4164A0 | Fatal error (`printf` + MessageBox + `exit(1)`) |
| 0x4166B0–0x416D10 | Settings base class: registry vs `WOW.INI` back end (`Use INI`) |
| 0x416F50–0x417090 | MEM block-list primitives |
| 0x4170E0–0x4178E0 | **WPath** (std::string path; find file locally else on the CD path) |

Already ported in WoWEngine (per docs): TEXT.ojd loader 0x414410, archive lookup/FILE_Load 0x415030/0x415200,
FFUH decompression 0x415A80, sprite frame choice 0x40EE10, font measure/draw 0x40FBF0/0x40F770, word wrap 0x4161E0.

---

## 2. Subsystems

### 2.1 WRenderer (scrolling dirty-rectangle renderer) — 0x40CF00 class, vtable 0x4A9920

Constructor 0x40CF00 (previous piece, string `"Renderer"`) is called from the full-screen view 0x405470
(args: pool 0x400, view rect, cell 16 × 8) and 0x406310. Each frame `0x40AF20` (previous piece) walks the list of
active renderers `dword_4D2638[]` (count `dword_4DA648`) and calls vtable +0x2C, +0x20, +0x24, +0x28.

Vtable 0x4A9920: +0 dtor (0x40D1F0→0x40D210), +4/+8/+C WObject base (0x4356A0/0x4356B0), +0x14 BeginFrame
0x40E0D0, +0x18 AddObject 0x40E240, +0x20 SetupBlitTarget 0x40E1E0, +0x24 Render 0x40E380, +0x28 ClearObjects
0x40E4C0, +0x2C MarkGlobalRects 0x40E300, +0x30 ClearBuffer 0x40E650, +0x38 DrawRegion 0x40D610.

| Address | Name (proposed) | What it does |
|---|---|---|
| 0x40D210 | `WRenderer::~WRenderer` | Releases cursor object 0x4DAF74, back-buffer surface (+0x56, DDraw Release) or memory (+0x38), the 4 DamageMaps (+0x70..+0x7C), the object list (+0x5C, under `CriticalSection`), the node pool (+0x68), the cell grid (+0x6C) |
| 0x40D330 | `MarkCells(l,t,r,b, damageMap, obj, damage)` | Clamps a world rect to the visible area, converts to cells; for each cell optionally pushes `obj` onto the cell's object list (cell = 12 bytes {head, firstObj, count}) and, if `damage` (or no obj), sets the cell in `damageMap` (vtbl+8 Set) |
| 0x40D4E0 | `TrackObject(damageMap, obj)` | Compares an object's position/z with last frame's (+0x1A/+0x1C/+0x1E vs +0x20/+0x22/+0x24). If moved/dirty (flag bit4)/being destroyed (refcount==1) or screen-fixed during a full refresh: damages old rect, stores new pos, clears bit4. Always registers the object over its new rect (damage only if flag bit1 = "always redraw") |
| 0x40D610 | `DrawRegion(objList, x, y, w, h)` (vtbl+0x38) | Converts a cell rect to back-buffer coordinates (wrapping at the buffer size +0x3C/+0x40), sets the clip rect for PolyLib (0x40FE10), ISOLib (0x410EF0) and the poly filler (0x410030), stores draw offsets +0x4E/+0x52, then calls `obj->Render(this)` (vtbl+0x10) for each object in z order |
| 0x40D750 | `RenderDamage(dmgCopy, dmgNow)` | Handles scrolling: compares view cell pos (+0x9C/+0xA0) with last frame (+0x8C/+0x90), clamps to ±view size, damages the newly exposed strips (0x410370). ORs object damage with the persistent map, builds dirty rectangles (0x410490), then for each rect builds a z-sorted object list (insertion sort on key `z(+0x1E)+layer(+0x26)`, stable) and calls DrawRegion. Unlocks DDraw surface at the end (`byte_4B92D8`) |
| 0x40DAC0 | `BlitRect(dst, srcSurf, …)` | Copies one rectangle from the back buffer to the screen: DDraw `BltFast` path (0x44D110) when `Use DDraw surfaces`, else asm copy 0x4502F3 |
| 0x40DBE0 | `BlitWrapped(x0,x1,y,w,h)` | Copies a cell span to the screen, splitting it into up to 2–4 blits where it crosses the wrap-around edge of the back buffer |
| 0x40DDB0 | `Present(dmgCopy, dmgNow)` | Gets the screen surface (0x44C580 lock or `dword_586EA0`), sets clip from screen size (`dword_586E98`/`dword_586DEC`). If a full refresh is pending (+0x30 > 0, decremented) copies the whole view as four wrapped quadrants; otherwise walks cells and copies runs of damaged cells (0x40DBE0). Unlocks (0x44C620) |
| 0x40E0D0 | `BeginFrame` (vtbl+0x14) | Adds itself to the active renderer list; from view position +0xCC/+0xD0 derives wrap offsets (+0xA4/+0xA8), aligned cell origin (+0xAC/+0xB0, +0xB4/+0xB8), cell index (+0x9C/+0xA0), visible world rect (+0xB4..+0xC0); if the view moved since last frame sets full-copy counter +0x30 = 2 |
| 0x40E1E0 | `SetupBlitTarget` (vtbl+0x20) | Locks DDraw surface if used (0x44C4F0), points PolyLib at the back buffer (0x40FFC0, 0x40FE50) |
| 0x40E240 | `AddObject(obj)` (vtbl+0x18) | AddRef; culls against the visible rect unless flag bit2 (screen-fixed). Objects still at the sentinel z 0x4650 get z = 0x4650 + running counter +0x34 (so default objects draw in submission order, on top of explicitly z-ordered ones). Pushes onto list +0x5C |
| 0x40E300 | `MarkGlobalRects` (vtbl+0x2C) | Damages every rect queued by `0x40AED0` (global double-buffered list `unk_4D2658`, counts at `unk_4DA654 - dword_4DA658*4`); a negative count means "everything" → fill damage map +0x70 with 1 |
| 0x40E380 | `Render` (vtbl+0x24) | Per frame: clear/force-damage map +0x78 (forced for the first +0x28 frames, initially 2); collect flag-bit3 objects into +0x60 (purpose unclear); TrackObject for each; RenderDamage; Present; clear the cell grid; remember view pos (+0xD4/+0xD8); frame counter +0x20++ |
| 0x40E470 | `SetCursorObject(obj)` | Global refcounted object `dword_4DAF74`, added first to every renderer each frame (used by 0x433870 tooltips — guess: drag/tooltip sprite) |
| 0x40E4B0 | `GetCursorObject` | |
| 0x40E4C0 | `ClearObjects` (vtbl+0x28) | Releases list +0x5C (under `CriticalSection`), resets z counter +0x34 = 1 |
| 0x40E530 | `SetClip(l,t,r,b)` (static-ish) | Writes +0x46..+0x4C and sets clip for PolyLib, ISOLib and poly filler |
| 0x40E580 | `SetWorldSize(w,h)` | +0xDC/+0xE0. Callers: full-screen view 0x405470, war map 0x41C1F0, battle map 0x484D00 |
| 0x40E5A0 | `SetViewPos(&pt)` | Clamps to [0, world − view − 1], stores +0xCC/+0xD0. Caller 0x42F6F0 (scrolling) |
| 0x40E610 | `GetViewPos(&pt)` | Callers 0x420C10, 0x42F6C0, 0x4A0630, 0x4A14C0 |
| 0x40E630 | `SetDamageVRAM(b)` | +0x2C = b \| `[Screen] Damage map VRAM` (byte_4DAF78); when +0x2C is 0 every frame is a full copy |
| 0x40E650 | `ClearBuffer` (vtbl+0x30) | memset back buffer |

The 4 DamageMaps per renderer (built in 0x40CF00 with cell 16×8): +0x70 = view-size map used for
dirty rects, +0x74 = screen-copy map, +0x78 = object damage this frame, +0x7C = previous frame's (copied via 0x410340).
Node pool +0x68 (0x44ED50 pool allocator, 8-byte nodes {next, obj}, size 0x400×10, ×50 more with `[Debug] Edit functionality`).

### 2.2 Busy cursor, RType pool — 0x40E680–0x40E780

| Address | Name | What it does |
|---|---|---|
| 0x40E680 | `WaitCursor::WaitCursor` (vtbl 0x4A995C) | Saves current cursor id (`dword_4DAED4`+0x44), sets cursor 0xD (wait, GType 0xC) via 0x40CD30. Used around long operations: 0x4042D0, disc check 0x408B70, war-map load 0x418500 |
| 0x40E6E0 | `WaitCursor::~WaitCursor` | Restores the saved cursor |
| 0x40E700 | `RType::operator new(size)` | Sizes ≤ 0x2AC come from a free list (`dword_4E7088`, link at +0x10); refills with chunks of 1024 (0xAB000 bytes, "Allocating memory block for 1024 RType??? objects"); larger → `operator new`. 70 callers |
| 0x40E780 | `RType::operator delete` | Pushes back on the free list |

### 2.3 RTypeBase / WObject / RTypeSprite — 0x40E7A0–0x40F040

| Address | Name | What it does |
|---|---|---|
| 0x40E7A0 | `GType_Register(id, class, palette, file, loadEarly, hotX, hotY)` | Fills GType table entry (0x4DAFD0 + id×56): +0 class, +4 palette slot, +8 refcount, +0xC/+0xE hotspot, +0x10 loadEarly, +0x14 data handle, +0x18 file name. Load-early entries are FILE_Loaded immediately ("Can't load file '%s'" fatal). Called by OBJ.ojd loader 0x437750 |
| 0x40E880 | `RTypeBase::RTypeBase()` (vtbl 0x4A9968) | WObject name "RTypeBase"; zeroes geometry, z = 0x4650 (sentinel), prev pos = −1, layer +0x26 = 1 |
| 0x40E910 | `RTypeBase::RTypeBase(gtype)` | Same, name "WObject", sets +0x16 GType |
| 0x40E980 | `RTypeBase::LoadGType` | Sets +0x0C name ptr, +0x18 palette, +0x30/+0x32 hotspot; for non-load-early GTypes loads the file on first use (refcount +8). Returns data pointer (handle+0x14) |
| 0x40EA50 | `RTypeBase::~RTypeBase` | Drops GType refcount, frees data at 0 ("RTypeBase::UnloadGTypeData") |
| 0x40EAE0 | `RTypeBase::Render` | Fatal "Attempting to Render a non render object" |
| 0x40EAF0 | `ApplyHotspot` | Bounds left/top := −hotspot, keeping width/height; sets dirty bit4 |
| 0x40EB40 / 0x40EB70 | `ScreenFixedOffset(renderer,&x,&y)` | If flag bit2 add the renderer's current (+0xEC/+0xF0) / previous (+0xD4/+0xD8) scroll position |
| 0x40EBA0 | `SetBoundsOrigin(l,t)` | Moves bounds, dirty |
| 0x40EBE0 | `Invalidate` | Queues the object's screen rect into the global dirty list (0x40AED0). 20 callers (war map, landscape, vehicles…) |
| 0x40EC30 | `SetShadeLevel(n)` (vtbl+0x18) | +0x34, dirty if changed. 31 callers |
| 0x40EC50 | `RTypeSprite::RTypeSprite(gtype)` (vtbl 0x4A9988) | Loads GType, bounds = sprite w/h, frame +0x38 = 0, offset +0x40/+0x42 = 0, hotspot applied. 43 callers |
| 0x40ECF0 | `RTypeSprite::RTypeSprite(gtype, x, y)` | Same plus position +0x1A/+0x1C |
| 0x40ECD0 / 0x40ED90 | sprite deleting dtor / dtor | |
| 0x40EDA0 | `RTypeSprite::SetGType(id)` | Reload graphic and bounds |
| 0x40EDF0 | `RTypeSprite::SetDrawOffset(dx,dy)` | +0x40/+0x42 (used by 0x426590) |
| 0x40EE10 | `RTypeSprite::Render` (vtbl+0x10) | Frame = +0x38 >> 8 (offset table at sprite+6), shade colours from 0x40B500(palette +0x18, level +0x34), blend mode = (flags bit0)×2 (50 % translucent), position + renderer offset +0x4E/+0x52 + draw offset; calls 0x40FD80 → asm 0x451235. Already ported |
| 0x40EF20 | `Animate(step, loop, &frameChanged, &ended)` | 8.8 frame step through `frames` (sprite+4); past the end: loop → wrap, else step back and set `ended`. Callers: 0x40CDC0 (cursor), 0x4312D0 (UI), 0x489050 (vehicles) |
| 0x40EFB0 | `AnimateLoop(step)` | Always wraps. Callers 0x44E5C0 (DeeDraw cursor), 0x4885C0 |
| 0x40F000 | `SetFrame(n)` | n ≥ frames → 0; dirty if changed. 27 callers |
| 0x40F040 | `SetBrightness(v)` (vtbl+0x18 of sprite/text) | `SetShadeLevel(v >> 2)` |

### 2.4 Text settings & RTypeText — 0x40F060–0x40FBF0

Static initialisers 0x40F060–0x40F5A0 each build a `RegVar` (0x4117A0, int) in section `[Colours]` bound to a
global; 0x40F5E0 builds an `IntSetting` `[Tweak] "Text Line Spacing"` = 4 (value at `word_4E72B4`). Full list in §4.

| Address | Name | What it does |
|---|---|---|
| 0x40F620 | `RTypeText::RTypeText(font, text, x, y)` (vtbl 0x4A99A8) | Free-standing text: lines 0, line spacing = `Text Line Spacing`, proportional (+0x3D = 1), flags +0x44 = 6 (centre X+Y) |
| 0x40F6D0 | `RTypeText::RTypeText(font, text)` | Menu text: spacing 2, flags 6; NULL text → " ". 25 callers (all UI widgets) |
| 0x40F760 | dtor | |
| 0x40F770 | `RTypeText::Render` | Per line: x from alignment (flag 8 right, 2 centre within bounds), y centred if flag 4; glyph = char−32, width table at font+0xA; proportional or fixed cell (font+8, glyph centred) — the fixed-cell path (+0x3D = 0) is never selected anywhere in the exe; draws glyphs through 0x40FD30 → asm 0x450B38 with shade level +0x34. Ported |
| 0x40F9F0 | `SetFlags(f)` | +0x44, re-measures bounds |
| 0x40FA40 | `Length` | lstrlen of buffer +0x48 |
| 0x40FA50 | `Printf(fmt, …)` | vsprintf into 256-byte buffer then SetText |
| 0x40FA90 | `SetText(s)` | Copies into +0x48 (0x200 bytes), splits on '\n' into ≤ 16 lines (ptrs +0x268, skipping leading blanks), re-measures |
| 0x40FB60 | `CharWidth(c)` | width + font spacing (font+6); 0 for c < 32 |
| 0x40FB90 | `FontHeight` | font+2 |
| 0x40FBA0 | `StringWidth(s, n)` | |
| 0x40FBF0 | `Measure(&rect)` | Line widths (+0x248), max width, total height = lines×height + (lines−1)×spacing (+0x3A); applies flags 8/2/4 to produce the bounds rect. Ported |

### 2.5 PolyLib (C stubs to hand-written asm) — 0x40FD30–0x410190

All of these load arguments into registers and call **hand-written assembly** (no frames, register calling
convention; IDA drops the pushes). The asm itself lives in parts covering 0x4502F3–0x4514F5 and 0x456DD0.

| Address | Name | Target asm | Notes |
|---|---|---|---|
| 0x40FD30 | `Poly_DrawGlyph(13 args)` | 0x450B38 | text glyph blit |
| 0x40FD80 | `Poly_DrawSprite(13 args)` | 0x451235 | sprite RLE rows (ported as `core/Sprite`) |
| 0x40FDD0 | stub | 0x450DA4 | used by 0x49BF50 (RTypeArrow) — unclear |
| 0x40FDF0 | `Poly_SetTable(i, v)` | writes `dword_4C074E[i]` | used by 0x49C990 |
| 0x40FE00 | stub | 0x450E16 | 0x49C990 |
| 0x40FE10 | `Poly_SetClip(l,t,r,b)` | 0x4507C5 (8-bit) and 0x450DEA (16-bit) | |
| 0x40FE50 | `Poly_SetTarget(buf, w, h)` | 0x450754 / 0x450D44 | |
| 0x40FE80 | stub (1 arg, both depths) | 0x4507B4 / 0x450DD9 | called by 0x40B120 (shade set-up) — guess: set active shade table |
| 0x40FEA0 | `PolyLib::Setup(bpp)` | — | Loads `dat\div_tab.dat` once (`dword_4E72C8`, data ptr → `dword_4C0BB4`), then by depth 8 → `dat\b8.shl`, 15 → `dat\b15.hsm`, 16 → `dat\b16.hsh`, copies it into `unk_4C1FFC` (shade/blend hash), frees it. Errors are fatal (`PolyLib::Setup: Couldn't load…`, `Invalid colour depth (%d bits pp)`) |
| 0x40FFC0 | `Poly_SetSurface(buf, pitch, …)` | 0x4514F5 | stores `dword_4E72B8..C4`, clip 0..640×480 |
| 0x410010 | `Poly_SetMode(v)` | `dword_4C0BA8` | used by RTypeArrow 0x49CDB0/0x49D930/0x49DBD0 — guess: blend/translucency mode |
| 0x410020 | stub | 0x4514E4 | 0x40B120 |
| 0x410030 | `Poly_SetClipRect(l,t,r,b)` | 0x4514F5 | |
| 0x410070 | `Poly_Draw(…)` | 0x456DD0 | used by 3D renderer 0x49EEF0 |
| 0x410090 | `Poly_TexturedQuad` | 0x456DD0 ×2 (two triangles) | arrows; engine has `DrawTexturedTriangle` |
| 0x410190 | `Poly_TexturedTriangle` | 0x456DD0 | arrows |

### 2.6 DamageMap & dirty-rectangle splitter — 0x410230–0x410D9F

| Address | Name | What it does |
|---|---|---|
| 0x410230 | `DamageMap::DamageMap(heap, w, h, cellW, cellH, fill)` (vtbl 0x4A99C8) | cols = ceil(w/cellW) (+0xC), rows (+0x10), cellW/H (+0x14/+0x18), byte grid via MEM_Alloc "DamageMap" (+0x1C) |
| 0x410300 | `Fill(v)` | |
| 0x410340 | `CopyFrom(other)` | |
| 0x410370 | `FillRect(x, y, w, h, v)` | handles negative w/h and wraps x/y around the grid (toroidal) |
| 0x410420 | `Get(x,y)` (vtbl+4) | 0 outside |
| 0x410460 | `Set(x,y,v)` (vtbl+8) | |
| 0x410490 | `BuildRects(scrollX, scrollY, viewW, viewH)` | Splits the view into 1, 2 or 4 rects at the wrap point, then repeatedly refines each with 0x4106C0 until no rect changes. Output table at `0x4E72D8`, entries 0x64 bytes (state 1 = to do / 2 = empty / 3 = final, x, y, w, h, density, 33 row counts, column counts), max index `dword_4E72D0` (room for 64) |
| 0x4106C0 | `SplitRect(i, map)` | Counts dirty cells per row/column, shrinks the rect to the dirty bounding box; if empty → state 2; if density ≥ 0xCCCD/65536 (**80 %**, `dword_4B96C8`) → state 3 (blit as one); else splits at the longest clean run (row or column) into two rects. Large (0x72F bytes) but plain compiled code |

### 2.7 ISOLib — 0x410E00–0x4113F0

| Address | Name | What it does |
|---|---|---|
| 0x410E00 | init `[Game] "ISO Brightness"` = 0.5 (FloatSetting at `unk_4E8BF0`) | |
| 0x410E40 | `ISO_GetDitherTable` | `dword_4E8BE0` (= DithDat + 0x100) |
| 0x410E50 | `ISO_SetShadeTable(p)` | `dword_4CA04C`; also selects routine table `off_4CA0C0[is8bit]` → `dword_4CA0BC` |
| 0x410E70/0x410E80 | set/get `dword_4CA0E0` | unclear (used by 0x49F890) |
| 0x410E90–0x410EC0 | getters `dword_4CA01C/020/014/018` | unclear (used by 0x49FDA0) — guess: current clip/projection values |
| 0x410ED0/0x410EE0 | setters `dword_4CA0D0`, `dword_4CA0DC` | unclear |
| 0x410EF0 | `ISO_SetClip(l,t,r,b)` | asm 0x457004 |
| 0x410F10 | `ISO_Draw…(5 args)` | asm 0x457D44 with `dword_4CA0A8` (0x49FDA0) |
| 0x410F40 | `ISO_…(4 args)` | asm 0x45798A (0x49F890) |
| 0x410F60 | `ISO_SetBuffers(a, b)` | `dword_4CA0AC/4CA0A8` (0x49F5D0) |
| 0x410F80 | `ISOLib::Setup(bpp)` | Loads `dat\dith.dat` (0x140 bytes = 16 × 20-byte dither descriptors) into `unk_4B96F0`, overrides some descriptor fields (8, 0x800, 5, 0x1000, 2, 0x2800…), allocates "DithDat" (0x4100 bytes) and fills random dither offsets per brightness band with `rand()%0x7FFF` (0x4025F0), scaled by 0x100 (8-bit) or 0x200; allocates "objLBuff" (0xC00 bytes = 1024 RGB light values) |
| 0x411170 | `ISOLib::FreeMem` | |
| 0x4111B0 | `ISO_LightVertices(obj, buf, x, y, z, range, light, scale)` | For every vertex (or face, depending on obj+0xA) adds `0x40C930(light, vertex)` × scale to the 3-byte light value, clamped to max(8, brightness × 63.0) |
| 0x4113F0 | `ISO_RenderLit(obj, …, lights)` | Copies base light values into objLBuff, then picks up to `dword_4B8E28` (= **2**) nearest point lights in range (distance via 0x43FF00, < 1,000,000) and lights the object with each, then draws with asm 0x457C60. Caller 0x49DE90 (RTypeISO) |

### 2.8 Settings — 0x411540–0x4122B0, 0x4166B0–0x416D10

Two independent mechanisms:

**A. `Setting` objects** (base vtbl 0x4A9A30, ctor 0x4166B0(section, key), +4 "default was written" flag, +8 section, +0xC key;
virtual Reload/Save). Back end chosen once by `Use INI` (read in 0x4166B0 via a BoolSetting with no section):
`byte_4B9DA4` = 1 → registry `HKLM\SOFTWARE\Rage\Jeff Wayne's 'The War Of The Worlds'\<section>` (string values),
0 → `GetPrivateProfileString(section or "Default", key, "", WOW.INI)`.
`0x416770 Read(default)`: returns value, or writes `default` back if missing. `0x4168D0 Write(value)`.

| Address | Class / function |
|---|---|
| 0x411540 | `BoolSetting(section, key, default)` (vtbl 0x4A99D8; value +0x11, original +0x10; '1'/'0') |
| 0x4115E0 | `BoolSetting(key, default)` (no section) |
| 0x411660 | `BoolSetting(key)` (no default) |
| 0x4116D0 | `~BoolSetting` – saves if changed or default was written |
| 0x411750 / 0x411770 | Bool Reload / Save |
| 0x412160 | `IntSetting(section, key, default)` (vtbl 0x4A99F8; value +0x14, original +0x10). 49 callers |
| 0x412210 / 0x412290 / 0x4122B0 | Int dtor(save) / Reload / Save |
| 0x416500 | `FloatSetting(section, key, default)` (vtbl 0x4A9A20, "%f"). 66 callers |
| 0x4165B0 / 0x416640 / 0x416660 | Float dtor / Reload / Save |
| 0x4169A0 | `StringSetting(section, key, default)` (vtbl 0x4A9A40, std::string at +0x10) |
| 0x416AE0 / 0x416B80 / 0x416D10 | String dtor / Reload / Save |
| 0x411EE0 / 0x411F10 | `PathSetting(key, default)` (vtbl 0x4A99E8) – used for `Install Path` (0x4017B0) and `CD Path` (0x4017F0), default `\0WOW\` |
| 0x411F20 | `PathSetting::ChangeTo()` | `_chdrive` to "X:" then `_chdir` (adding a trailing `\`). Called by WinMain 0x401940 |
| 0x412090 | `PathSetting::WithSlash()` | path with trailing `\` (used by 0x402630, disc check 0x408B70, WPath 0x417380, FMV 0x44DE20/0x44E060) |

**B. `RegVar`** (vtbl 0x4A99E4, ctors 0x4117A0…0x411950): binds an *existing global* to a value under the same
registry key, **registry only** (ignores `Use INI`). Type (+4): 0/1 int (REG_DWORD or string→atoi), 2 string
(fixed buffer), 3 bool (DWORD≠0 or char≠'0'), 4 float (`%f`), 5/6 point (`%d,%d`), 7 rect (`%d,%d,%d,%d`).
`0x4119A0 Load` creates the value with the current global as default if missing (`0x411CC0 Store`).
Users: `[Colours]` (0x4117A0, type 1), 0x411810 type 0 (0x401770, 0x41DF40…, 0x433770…, 0x49D430), 0x411850 type 2
(0x401680), 0x411890 type 3 (bools: `[Screen] Use DDraw surfaces`/`BLIT instead of FLIP`/`Damage map VRAM`
0x40CE00–0x40CE80, 0x4057B0, 0x41E160…), 0x4118D0 type 4 (sound volumes 0x4642C0–0x464440), 0x411910 type 5
(0x42C500/0x42C5A0 window pos — guess), 0x411950 type 6 (0x42C4A0, 0x464580–0x464640).

### 2.9 DirectX probe — 0x4122F0

`GetDXVersion(&version, &platform)` (DirectX SDK sample shape). Platform 2 = NT (NT < 4 → 0; NT4 → DInput check
gives 0x200/0x300); otherwise loads `DDRAW.DLL`, `DirectDrawCreate`, QI IDirectDraw2 (→ 0x200), `DINPUT.DLL`
`DirectInputCreateA` (→ 0x300), creates a primary surface and QIs IDirectDrawSurface3 (→ 0x500). Debug strings
"Couldn't …". Called once by WinMain 0x401940. Not needed by an SDL rebuild.

### 2.10 CD audio (MCI) — 0x412660–0x414180

Globals: `dword_4E8D0C` MCI device id (0 = no CD), `dword_4E8D10` notify window (receives `MM_MCINOTIFY` 0x3B9),
`dword_4E8D14` saved position, `dword_4B99A4` CD enabled (default 1), `byte_4B99A8` "CD was on before pause",
`byte_4D1231` = `[Debug] "Dave"` (debug output). All errors print `_ciGetErrorStringA` text or "Generic Error"/"CD not
available" to the debugger only. MCI commands: 0x803 OPEN (flags 0x3100 = TYPE|TYPE_ID|SHAREABLE, type 0x204
cdaudio), 0x804 CLOSE, 0x806 PLAY, 0x807 SEEK, 0x808 STOP, 0x809 PAUSE, 0x80D SET (0x400 time format, 0xA = TMSF),
0x814 STATUS (items 1 length, 2 position, 3 tracks, 4 mode, 6 time format, 8 current track).

| Address | Name | What it does |
|---|---|---|
| 0x412660 / 0x412670 | `GetCdWasOn` / `SetCdWasOn(b)` | `byte_4B99A8`, used by WGame::Pause/Resume 0x404BC0/0x404C10 and app activation 0x42C9B0 |
| 0x412680 | `IsCdEnabled` | |
| 0x412690 | `SetCdEnabled(on)` | **On**: closes any `SJE_CdPlayerClass` window (WM_QUIT), opens cdaudio, TMSF, logs "Resuming position at %d", seeks to the saved position and starts the play thread. **Off**: saves the position ("Saving position %d"), stops, re-seeks, closes. Callers: 0x402630, pause/resume, Sound menu 0x4059A0/0x405A70, CD Player 0x407990, disc check 0x408B70 |
| 0x412940 | `ResetCdPosition` | "Resetting CD position" (0x418220 war-map dtor, 0x41B210, battle 0x484BF0) |
| 0x412960 | `OpenCd(notifyHwnd)` | Re-opens the device (closes the old one, quits SJE player), TMSF. Returns 1 even when no CD. Callers: CD Player, disc check, war map ctor 0x417CD0, 0x41B070, battle-map music 0x4861E0 |
| 0x412B50 | `CloseCd` | |
| 0x412C00 | `StopCd` | Stop, then seek back to the start of the current track |
| 0x412DC0 | `PauseCd` | If playing → pause; returns whether it was already stopped |
| 0x412F10 | `IsCdPlaying` | mode == 0x20E |
| 0x412FD0 | `SelectTrack(n)` | stop, seek to track n (TMSF), verify current track == n, restore time format |
| 0x413220 | `NextTrack` | current+1, wraps to 1 after the last |
| 0x4134B0 | `PrevTrack` | current−1, wraps to last |
| 0x413740 | `SeekBack(tmsfDelta)` | Position − delta with second/minute borrow, stop + seek. CD Player transport 4 with `0x500` (= 5 s) per auto-repeat |
| 0x413990 | `SeekForward(tmsfDelta)` | Position + delta clamped to track length. Transport 6, 5 s |
| 0x413C70 | `GetTrackCount` | |
| 0x413D30 | `GetCurrentTrack` | |
| 0x413DF0 | `GetTrackLength(n)` | seconds + 1 (from MSF) |
| 0x413EB0 | `GetPosition` | TMSF position; 0x41B070 treats min/sec == 0 as "not playing" |
| 0x413F70 | `SetTimeFormat` | (disc check 0x408B70) |
| 0x414010 | `SendMci(cmd, flags, parms)` | wrapper: fills dwCallback with notify hwnd when MCI_NOTIFY |
| 0x4140F0 | `ReportMciError(err)` | |
| 0x414150 | `PlayTrackAsync(track)` | `_beginthreadex(StartAddress, track)` |
| 0x414180 | `StartAddress` (thread) | If track ≠ 0: SelectTrack-like seek. Then plays from the current position **to the start of the next track** (flags 9 = NOTIFY\|TO) so the window gets `MM_MCINOTIFY` when the track ends; on the last track plays to the end |

How it is used elsewhere: war map music `0x41B070` (part 03) — when `[Sound\Volume] CD` (`dword_4B8A88`) is on:
if the CD is idle it alternates between two tracks (`ArgList[dword_4B9DE0 ^= 1]`), `PlayTrackAsync`, logs "Playing
track %d for %d seconds" and sets timer 0x813C for length × 30 ticks; if the CD is busy it reopens and re-checks in 10 s
(300 ticks). When CD music is off it plays a random ambient sound 0xFD–0xFF (`rand()%3`) and fades it in over 60 ticks.

### 2.11 Text table helpers — 0x4146F0–0x414740

| Address | Name | What it does |
|---|---|---|
| 0x414410 / 0x414680 | `WText` ctor/dtor (TEXT.ojd, `TXT\`) | Ported (`core/TextOjd`) |
| 0x4146F0 | `GetText(id)` | `dword_4E8D18[id].text` (8-byte entries). 68 callers |
| 0x414700 | `PlayTextSpeech(id, …)` | If entry +4 (sound id) ≠ 0, plays it with 0x464EE0 at the listener position (`dword_4E9250`, `dy`), volume 1.0. Callers: 0x41EB90/0x41EC30 (war-map messages), research 0x45D1A0/0x45D3F0/0x45DD50, 0x4A4190 |
| 0x414740 | init `[Debug] "Show Mem Dump On Alloc Failure"` (byte_4E9131) | |
| 0x4161D0 | `ResetListener` | zeroes `dword_4E9250`/`dy` (sound listener position — guess) |

### 2.12 Memory manager (MEM_*) — 0x414780–0x415410, 0x416F50–0x417090

Up to 4 heaps (0x34-byte records at 0x4E9138…): +0 used, +4 peak, +8 base, +0xC size, +0x10 block-pool, +0x14/+0x18
counters. Blocks are 0x24-byte descriptors {next, prev, size, flags, heap, data, name[9]}; flags 0x100 = allocate
from the high end (permanent), 0x200 = outside the heap, 0x400 = free, 0x800 = used; the low byte of the flags is the
fill byte. Requests > 0x32000 bytes bypass the heap (`operator new`).

| Address | Name |
|---|---|
| 0x414780 | `MEM_CreateHeap(size, maxBlocks)` → id (−1 when 4 exist) |
| 0x4148A0 | `MEM_Alloc(heap, size, flags, name)` → descriptor; best fit; failures log "#1/#2 Attempting to allocate %d as %s" and optionally dump |
| 0x414BC0 | `MEM_Free(&desc, who)` – merges free neighbours; fatal "MEM_Free: attempting to free block twice: %s" |
| 0x414CE0 | `MEM_Dump(heap)` – "System/Free/Used" table to the debug log |
| 0x414DA0 | init `[Debug] "Ignore fresher files"` (byte_4E9219) |
| 0x415410 | `MEM_LogDelta` – "CHANGE: … TOTAL: … PEAK:" via 0x417C30 when `Dave` |
| 0x416F50–0x417090 | descriptor pool sizing, init, take, give back, iterate |

A rebuild should use the C++ allocator; nothing here is visible to the player.

### 2.13 Files and archives — 0x414DE0–0x415200, 0x4170E0–0x4178E0

| Address | Name | What it does |
|---|---|---|
| 0x414DE0 | `FILE_OpenArchives` | Opens `dat\dat.wow` (index of 44-byte entries, name at +0xC) and marks entries with a loose `dat\%s` override; finds `maps\maps.wow` through WPath (so it can come **from the CD path**) and marks loose overrides in **`Map\%s`** (not `maps\`). Override marking skipped when `[Debug] Ignore fresher files`. Ported (Vfs) |
| 0x415030 | `FILE_Find(name, &stream)` | archive lookup. Ported |
| 0x415120 | `FILE_Size(name)` | uncompressed size from FFUH header, archive entry or ftell. Callers: TEXT, OBJ, AI, SFX loaders, WLandscape 0x47B900 |
| 0x415200 | `FILE_Load(heap, name, memFlags, buffer, bufferSize)` | Ported |
| 0x4170E0 / 0x4171E0 / 0x417330 | `WPath` ctor(char*) / copy / dtor (vtbl 0x4A9A4C, std::string) | |
| 0x417380 | `WPath::Find()` | if `_access(name)` fails, tries `CD Path` + name; updates the string; returns found |
| 0x4175B0 / 0x4177F0 / 0x4178E0 | `WPath` extension/dir manipulation (string surgery around '.') | used by WLandscape 0x47B900 and minimap 0x4A1F90 — exact semantics unclear (guess: replace extension / strip directory) |

### 2.14 Huffman codec ("FFUH") — 0x415470–0x416180

Class (vtbl 0x4A9A18, ctor 0x415470, 0x48 bytes): +0x0C/+0x10 frequency table (256 dwords), +0x14 decode lookup
(256 × 6 bytes), +0x18 output, +0x1C uncompressed size, +0x20 input, +0x24 compressed size, +0x38 bit count
(0 = single repeated byte), bit cursor +0x3C/+0x40/+0x44.

| Address | Name | What it does |
|---|---|---|
| 0x415520 | `Compress(src, len)` | counts byte frequencies, builds tree (0x415D20, encode mode), allocates output, encodes (0x4157E0) |
| 0x4155B0 | `Decompress(header)` | header `'FFUH'`, size, csize, bits, freq[256], data at +0x410 |
| 0x415640 | `DecompressFile(FILE*, out)` | streamed: reads header, then 64 KB input chunks (0x415A80 / refill 0x415C00) |
| 0x415720 | `WriteHeader(&buf, &len)` | allocates csize + 0x410, writes header + freq table |
| 0x415780 | `AttachHeader(header, data)` | |
| 0x4157E0 | `Encode` | LSB-first bit stream |
| 0x4158E0 | `DecodeMemory` | 8-bit lookup table then tree walk |
| 0x415A80 | `DecodeStream` | Ported in `core/Ffuh.cpp` |
| 0x415C00 | `Refill(n, FILE*)` | keeps the last 3 bytes, reads the next 64 KB, then calls **0x44E5C0** (DeeDraw: animates the cursor during long loads) |
| 0x415CA0 / 0x415D20 | build decode table / build tree (std::list based; helpers 0x416110/0x416180/0x4161B0 shared with 0x4211D0, 0x428100) | |

Save games use it: 0x404F50/0x4050B0 compress, 0x405020/0x4051B0 decompress, 0x405100 (DMGL chunk writer)
writes the header — i.e. the **DMGL** (damage) chunk of a save is Huffman-compressed (guess for the exact chunk:
0x405100 is the DMGL writer per `SaveGame.h`).

### 2.15 Word wrap, rects, fatal error — 0x4161E0–0x4164A0

| Address | Name | What it does |
|---|---|---|
| 0x4161E0 | `WrappedText(src, width, font)` | allocates strlen+0x7F, calls 0x416250 |
| 0x416230 | `WrapToStatic(src, width, font)` | into `unk_4E9258` |
| 0x416250 | `WrapText(dst, src, width, font)` | breaks at the last space when the running width ≥ width, trims trailing blanks, keeps existing '\n'. Ported (`Canvas::Wrap`) |
| 0x416380 | clear rect `dword_4E9658..64` | unclear |
| 0x4163A0 | `Rect::Normalise` | |
| 0x4163C0 | `Rect::Inflate(l,t,r,b)` | |
| 0x416400 | `Rect::KeepInside(&offset, bounds)` | OffsetRect so the rect fits inside `bounds` (tooltips/info boxes 0x4163C0, 0x4A3E60) |
| 0x4164A0 | `Fatal(fmt, …)` | `printf("\n%s\n")`, `MessageBoxA(0, text, NULL, 0x2030)`, `exit(1)`. 22 callers |

---

## 3. Data structures & globals

### RType object (base of every drawable; size up to 0x2AC)
| Off | Type | Meaning |
|---|---|---|
| +0x00 | vtbl | 0x4A9968 base, 0x4A9988 sprite, 0x4A99A8 text |
| +0x04 | u16 | refcount (renderer lists AddRef; ==1 → being removed) |
| +0x0C | char* | GType file name |
| +0x14 | u16 | flags: bit0 50 % translucent, bit1 always redraw, bit2 screen-fixed (follows scroll, never culled), bit3 unclear (collected into renderer +0x60), bit4 dirty |
| +0x16 | u16 | GType id |
| +0x18 | u16 | palette slot |
| +0x1A/+0x1C | s16 | x, y |
| +0x1E | s16 | z / draw order (0x4650 = unassigned → insertion order) |
| +0x20/+0x22/+0x24 | s16 | last frame x, y, z (−1 initially) |
| +0x26 | s16 | layer added to z for sorting (1) |
| +0x28..+0x2E | s16 | bounds l, r, t, b relative to x,y |
| +0x30/+0x32 | s16 | hotspot |
| +0x34 | u16 | shade level |
| Sprite +0x38 | u32 | frame, 8.8 fixed |
| Sprite +0x3C | ptr | sprite data |
| Sprite +0x40/+0x42 | s16 | draw offset |
| Text +0x38 | u16 | line count (≤16) |
| Text +0x3A | u16 | line spacing |
| Text +0x3D | u8 | proportional (always 1) |
| Text +0x40 | ptr | font data |
| Text +0x44 | u32 | flags (2 centre X, 4 centre Y, 8 right) |
| Text +0x48 | char[0x200] | text |
| Text +0x248 | s16[16] | line widths |
| Text +0x268 | char*[16] | line starts |

### GType table 0x4DAFD0 (56 bytes per id)
+0 class, +4 palette, +8 refcount, +0xC/+0xE hotspot, +0x10 loadEarly, +0x14 MEM handle (data = handle+0x14), +0x18 name.

### WRenderer (0xF4 bytes)
+0x18/+0x1C screen origin, +0x20 frame no., +0x24 heap, +0x28 forced-full-redraw frames (2), +0x2C damage-map-VRAM,
+0x30 full-copy frames, +0x34 z counter, +0x38 back buffer, +0x3C/+0x3E/+0x40 buffer w/h (rounded to cells),
+0x42/+0x44 bytes per pixel/pitch (from 0x42DB30/0x42DB50), +0x46..+0x4C clip, +0x4E/+0x52 draw offsets, +0x56 DDraw
surface, +0x5C object list, +0x60 bit3 list, +0x64 cell count, +0x68 node pool, +0x6C cell grid, +0x70..+0x7C damage
maps, +0x84 pool size, +0x8C/+0x90 last cell pos, +0x94/+0x98 sub-cell offset, +0x9C/+0xA0 cell pos, +0xA4/+0xA8 wrap
offset, +0xAC..+0xC0 visible rect, +0xCC/+0xD0 view pos, +0xD4/+0xD8 last view pos, +0xDC/+0xE0 world size,
+0xE4/+0xE8 view size, +0xEC/+0xF0 scroll for screen-fixed objects.

### Other globals
| Address | Meaning |
|---|---|
| 0x4D2638[], 0x4DA648 | active renderers this frame, count |
| 0x4D2658, 0x4DA654/0x4DA658 | global invalidated-rect lists (double buffered) |
| 0x4DAF74 | cursor/attached object |
| 0x4DAF50 | screen blit descriptor (w/h at 0x4DAF5E..64, surface 0x4DAF6E) |
| 0x4E7088 | RType pool free list |
| 0x4E72D0, 0x4E72D8 | dirty rect table (64 × 0x64) |
| 0x4E8BD8/0x4E8BDC/0x4E8BE0 | DithDat handle / data / dither table |
| 0x4E8BE4/0x4E8BE8 | objLBuff data / handle |
| 0x4E8C08, 0x4E8D08 | registry key path buffer, end pointer |
| 0x4E8D0C/10/14 | MCI device / notify hwnd / saved position |
| 0x4E8D18 | text table (8-byte entries: text, speech sound id) |
| 0x4E9138… | MEM heaps |
| 0x4E921C/24, 0x4E9228/2C/30 | dat.wow FILE*/index, maps.wow FILE*/count/index |
| 0x4B9DA4 | settings use registry (= !Use INI); 0x4B84A0 → "WOW.INI"; 0x4B84D0 registry key |
| 0x4B92D8 | `[Screen] Use DDraw surfaces` (default 1) |
| 0x4DAF78 | `[Screen] Damage map VRAM` |
| 0x4D1231 | `[Debug] Dave`; 0x4D1249 `[Debug] Edit functionality` |
| 0x4B8E28 | max point lights per ISO object = 2 |
| 0x4B96C8 | dirty-rect density threshold 0xCCCD (80 %) |

---

## 4. Messages, commands, strings, tuning

**Settings defined in this piece**

| Section | Key | Default | Storage | Used by |
|---|---|---|---|---|
| Colours | Menu Heading | 7 | 0x4B93EC | CD Player 0x407990, menus 0x40A883 |
| Colours | Menu Items | 4 | 0x4B93F0 | menu buttons 0x40AA10…, research 0x45F3F1 |
| Colours | Menu Glow First | 4 | 0x4B93F4 | 0x40A0CA, glow animation 0x40ADC0, research 0x45F198 |
| Colours | Menu Glow Last | 9 | 0x4B93F8 | 0x40ADC0, 0x45F439 |
| Colours | Menu Static Text | 4 | 0x4B93FC | CD Player, option labels 0x40A42D… |
| Colours | Research Current | 25 | 0x4B9400 | 0x45C1A0, 0x45CDD0, 0x45F070 |
| Colours | Research Normal | 26 | 0x4B9404 | 0x45C1A0, 0x45CDD0 |
| Colours | Research New | 27 | 0x4B9408 | 0x45C1A0, 0x45CDD0, 0x45F070 |
| Colours | Research Heading | 17 | 0x4B940C | research screen 0x45D3F0… |
| Colours | Research Proposal | 26 | 0x4B9410 | 0x45D3F0, 0x45E42F, 0x45F0B3 |
| Colours | Research Detail | 26 | 0x4B9414 | 0x45D3F0 |
| Colours | Research Specification | 26 | 0x4B9418 | 0x45D3F0 |
| Colours | Research Enhancement | 26 | 0x4B941C | 0x45D3F0 |
| Colours | Message Text | 19 | 0x4B9420 | war-map messages 0x428810/0x428940 |
| Colours | Info Page Text | 15 | 0x4B9424 | InfoPage 0x4A3E60 |
| Colours | Messagebox Detail | 21 | 0x4B9428 | message box 0x432520 |
| Colours | Messagebox Command | 22 | 0x4B942C | message box 0x432520 |
| Colours | Briefing Sector Name | 17 | 0x4B9430 | briefing 0x4999E0/0x49AA5C |
| Colours | Briefing Heading | 17 | 0x4B9434 | briefing |
| Colours | Briefing Detail | 18 | 0x4B9438 | briefing |
| Colours | Credits Heading | 17 | 0x4B943C | **never read** |
| Colours | Credits Detail | 6 | 0x4B9440 | **never read** |
| Tweak | Text Line Spacing | 4 | 0x4E72B4 | free text 0x40F620 |
| Game | ISO Brightness | 0.5 | 0x4E8C04 (object 0x4E8BF0 + 0x14) | ISOLib lighting max |
| Debug | Show Mem Dump On Alloc Failure | 0 | 0x4E9131 | MEM_Alloc |
| Debug | Ignore fresher files | 0 (guess) | 0x4E9219 | archive overrides |
| (root) | Use INI | — | → 0x4B9DA4 | settings back end |

Note: the `[Colours]` values are `RegVar`s, i.e. registry only even with `Use INI`.

**Messages**: `WM_QUIT` (0x12) to window class `SJE_CdPlayerClass`; MCI `MM_MCINOTIFY` (0x3B9) delivered to the
notify window (CD Player handler 0x4081F3, also 0x42D181, 0x42DC25); timer command 0x813C (war-map CD music
re-check, set by 0x41B070). No menu command ids are handled in this piece.

**Strings** (debug/fatal): "Renderer", "RTypeBase", "WObject", "Can't load file '%s'", "RTypeBase::UnloadGTypeData",
"RTypeBase::Render - Attempting to Render a non render object", "Allocating memory block for 1024 RType??? objects",
"PolyLib::Setup: …", "DamageMap", "DithDat", "objLBuff", "ISOLib::FreeMem", "%d,%d", "%d,%d,%d,%d", "%f",
"Default", "WOW.INI", "SJE_CdPlayerClass", "Resuming position at %d", "Saving position %d", "Resetting CD position",
"CD not available", "Generic Error", "StringLib", "TXT\", "INVALID DATA IN TEXTFILE", "WText::~WText()",
MEM_* messages, "dat\dat.wow", "dat\%s", "maps\maps.wow", "Map\%s", FILE_Load warnings, "\n%s\n".
Files: `dat\div_tab.dat`, `dat\b8.shl`, `dat\b15.hsm`, `dat\b16.hsh`, `dat\dith.dat`.

---

## 5. Cross-references to other pieces

- **Callers into this piece**: WinMain 0x401940 (DX probe, PathSetting::ChangeTo); setting statics 0x4014F0–0x402540;
  game init 0x402630 (SetCdEnabled, WithSlash, OpenArchives); WGame pause/resume 0x404BC0/0x404C10; save/load
  Huffman 0x404F50–0x4051B0; full-screen view 0x405470 (renderer, SetWorldSize); Sound menu 0x4059A0/0x405A70;
  CD Player 0x407990/0x4081D0/0x4088B0 (all CD functions); disc check 0x408B70; menus 0x40A060–0x40AB00 (text objects,
  colours); frame loop 0x40AF20; cursor 0x40CD30/0x40CDC0; war map 0x417CD0/0x418220/0x418500/0x41B070/0x41C1F0;
  UI widgets 0x42FA70–0x433B60 (text/sprite objects, Animate); app window 0x42C9B0; OBJ.ojd 0x437750 (GType_Register);
  research 0x45C1A0–0x45F070 (colours, PlayTextSpeech); SFX 0x463D10/0x4642C0; battle map 0x484BF0/0x484D00/0x4861E0;
  vehicles 0x489050; arrows 0x49BE70–0x49DBD0 (PolyLib quads); RTypeISO/WOFLib 0x49DE90–0x49FDA0 (ISOLib);
  minimap 0x4A1F90; InfoPage 0x4A3E60.
- **Callees outside**: shade lookup 0x40B500, dirty rect queue 0x40AED0, light value 0x40C930, DeeDraw
  0x44C3A0/0x44C4F0/0x44C580/0x44C620/0x44D110/0x44E5C0, node pool 0x44ED50/0x44EE00/0x44EE30, asm blitters
  0x4502F3–0x4514F5, polygon 0x456DD0, ISO asm 0x457004–0x457D44, sound 0x464EE0, WObject base 0x435640/0x435690,
  rand 0x4025F0, debug log 0x417C30, CRT 0x466E30+.

---

## 6. Missing in WoWEngine / candidate tasks (prioritised)

WoWEngine draws everything each frame into a 32-bit buffer (`frontend/Canvas`), so the dirty-rectangle machinery,
MEM heaps, DX probe and MCI layer don't need copying; what matters is the **visible behaviour** they produce.

1. **Draw order and object model for the war/battle maps.** The engine has no general drawable-object list.
   Rebuild a renderer list with the original ordering rule: sort by `z(+0x1E) + layer(+0x26)` ascending, stable;
   objects without an explicit z (0x4650) draw after all z-sorted ones in submission order; screen-fixed objects
   (flag bit2) ignore scrolling and are never culled; 50 % translucency from flag bit0; shade level per object.
   Addresses: 0x40E240, 0x40D750 (insertion sort), 0x40EB40, 0x40EE10. Needed before the battle map.
2. **View scrolling clamp.** `SetViewPos` clamps to `[0, world − view − 1]` (0x40E5A0) — note the extra −1; world
   size set at 0x41C1F0 (war map) and 0x484D00 (battle map). Check WarMap's scroll clamp matches.
3. **Sprite animation helpers.** `Animate(step 8.8, loop)` with the "ended" / "frame changed" outputs (0x40EF20),
   `AnimateLoop` (0x40EFB0), `SetFrame` wrap-to-0 (0x40F000). Used by UI widgets 0x4312D0, cursor 0x40CDC0 and
   vehicles 0x489050; also the busy cursor animates during long loads (0x415C00 → 0x44E5C0).
4. **Busy (wait) cursor** around long operations: cursor 0xD (GType 0xC) while the disc check, campaign set-up
   (0x4042D0) and war-map load (0x418500) run (0x40E680/0x40E6E0). Engine cursor table exists; add the scoped switch.
5. **In-game CD music on the war map and battle map.** Engine only plays music in the CD Player. Original
   (0x41B070, 0x4861E0 using 0x412960/0x413DF0/0x413EB0/0x414150): when `[Sound\Volume] CD` is on, alternate between two
   tracks (table `ArgList`, index `dword_4B9DE0`), each played to the start of the next track, re-checking with timer
   0x813C after length × 30 ticks (10 s when busy); when off, a random ambient loop 0xFD–0xFF faded in over 60 ticks.
   Also pause/resume the music with the game (0x404BC0/0x404C10, `byte_4B99A8`) and save/resume the track position when
   CD music is toggled (0x412690, "Resuming/Saving position").
6. **CD Player rewind / fast-forward.** The handler 0x4081D0 supports transport 4 (rewind) and 6 (fast-forward)
   with auto-repeat, 5 s per step (0x413740 / 0x413990, clamped to the track); Stop rewinds to the start of the
   current track (0x412C00); Next/Prev wrap around (0x413220/0x4134B0). Engine has 0,1,2,3,5 only — check the
   constructor 0x407990 for which buttons send 4/6 (possibly prev/next when held).
7. **All `[Colours]` levels.** Engine reads only Menu Heading/Items/Static Text. Add Menu Glow First/Last (needed
   for the menu glow animation 0x40ADC0, listed as not done in FRONTEND.md), Research * (8 values), Message Text,
   Info Page Text, Messagebox Detail/Command, Briefing Sector Name/Heading/Detail with the defaults in §4.
8. **Text objects details.** 16-line limit and leading-whitespace skip per line (0x40FA90), `Printf` 256-byte limit
   (0x40FA50), right-align flag 8 producing a negative-left bounds (0x40FBF0). Fixed-cell (monospace) mode exists but is
   never used — don't implement.
9. **Speech for text ids.** `PlayTextSpeech(id)` (0x414700): TEXT.ojd entries carry a sound id at +4; war-map messages,
   research screens and 0x4A4190 play it. Make sure `core/TextOjd` keeps the sound id and expose a play call.
10. **ISO object lighting** (battle map): per-vertex light from up to 2 nearest point lights, max level
    `max(8, ISO Brightness × 63)`, `[Game] ISO Brightness` default 0.5; dither table from `dat\dith.dat` + random
    offsets (0x410F80, 0x4111B0, 0x4113F0). Needed with RTypeISO/WOFLib (0x49DE90).
11. **Polygon/shade tables for 16-bit.** `PolyLib::Setup` loads `dat\div_tab.dat` and `dat\b16.hsh` (or b8.shl /
    b15.hsm) (0x40FEA0); the asm polygon filler 0x456DD0 uses them for shaded/translucent polygons (arrows 0x49CDB0,
    3D 0x49EEF0, mode `dword_4C0BA8` via 0x410010). Engine's textured triangles are unshaded; check against the original
    when the battle map/3D models are rebuilt.
12. **Huffman *compression* for saves.** Engine can only decompress FFUH. Writing saves in the original format needs
    the encoder (0x415520 freq/tree, 0x4157E0 bit packing, 0x415720 header) — at least for the DMGL chunk (0x405100).
    Loading a save's compressed chunk uses 0x4155B0/0x4051B0.
13. **File lookup fall-backs** (low; engine runs from a full copy): `maps\maps.wow` may come from `CD Path`
    (0x417380); loose overrides for maps are looked up in `Map\%s` (0x414DE0) — confirm Vfs uses `Map\`, not `maps\`.
14. **Settings back end** (deliberately different): original uses registry `HKLM\SOFTWARE\Rage\Jeff Wayne's 'The War
    Of The Worlds'\<section>` or `WOW.INI` (`Use INI`), writing defaults back when a value is missing (0x416770); the
    `[Colours]` RegVars are registry-only. Engine uses WoWRevived.ini — keep, but consider writing defaults back so
    users can find the keys.
15. **Fatal-error behaviour** (low): missing GType files / PolyLib tables abort with a message box (0x4164A0); the engine
    should fail with a clear message rather than crash.

Not worth rebuilding: the toroidal back buffer and dirty-rect splitter (0x40D330–0x40E380, 0x410230–0x410D9F; only a
1998 speed trick — but note scrolling exposes only new strips, so the original never shows tearing artefacts the engine
needs to imitate), MEM_* (0x414780–0x415410), DX probe (0x4122F0), MCI plumbing (0x414010/0x4140F0), RType pool
(0x40E700).
