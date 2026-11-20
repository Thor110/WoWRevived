# Part 08: 0x44E4B0 – 0x45B690

## 1. Overview

This piece covers the end of DeeDraw, a few small engine utilities, the generic A* route finder, the Smacker player, the hand-written assembly rasterisers (most of the piece by line count), and the first half of WResearch, which is the research system.

| Range | Subsystem | Notes |
|---|---|---|
| 0x44E4B0–0x44E5C0 | DeeDraw busy icon | An animated "wait" sprite drawn straight onto the front buffer during disk reads |
| 0x44E7D0–0x44EC20 (+ 0x44F830–0x44F880) | Save-file stream classes | stdio `FILE*`-backed serialiser used for every save chunk; the `GAME` header |
| 0x44ED50–0x44EE30 | "HeepHeep" free-list pool | Fixed 8-byte node allocator for the renderer's draw lists |
| 0x44EE70–0x44F273 | AStar | Generic A* over a node graph; the war map uses it for sector routes |
| 0x44F280 | Cost-function base class ctor | |
| 0x44F2C0–0x44F82B | Smacker player | SMACKW32 calls, centring, the scaler with line doubling |
| 0x44F890–0x450130 | **ASM** blitter A: RLE sprites (cursor copy) | Same algorithm as blitter C, with its own globals. It is used only by the software cursor (0x40CC70) |
| 0x450130–0x450527 | **ASM** raw rectangle copy | Blitter D (0x4502F3): 4 modes |
| 0x450528–0x450752 | **ASM** solid rectangle fill | Used by RTypeArrow 0x49C7C0 |
| 0x450754–0x450D41 | **ASM** blitter B: unpacked 8-bit image through a colour table | The font glyph blitter (0x40F94E); 8 modes |
| 0x450D44–0x4514E3 | **ASM** blitter C: RLE sprites | The main sprite drawer: 0x451235 and its span routines, plus pixel probe 0x450DA4 |
| 0x4514E4–0x457003 | **ASM** PolyLib triangle rasteriser | 0x456DD0 plus 48 span routines (flat, gouraud and textured; 8- and 16-bit; clipped and unclipped) |
| 0x457004–0x457F14 | **ASM** ISOLib | Isometric RLE patch / object drawing (0x457C60, 0x4579A4) and a solid fill (0x457D44) |
| 0x457F20–0x45B640 | WResearch | `[Research]`/`[Tweak]` settings, the research manager, the hard-coded Human and Martian research trees, trigger items, improvement multipliers, the AI research plans, save/load |

Everything from 0x44F890 to 0x457F14 is **hand-written assembly**. The signs are `pusha`/`popa`, register calling conventions (`eax`/`edx`/`ebx`/`esi`/`edi`/`ebp` used as arguments), self-modifying-style globals instead of a stack frame, and `call dword_X` through function-pointer globals. The span routines inside it take their arguments in registers, so IDA's argument lists for them are meaningless. The engine should reproduce what they draw, not how they are written (see ENGINE-MAP.md).

---

## 2. Subsystems

### 2.1 DeeDraw: the busy ("wait") icon

| Addr | Name | What it does |
|---|---|---|
| 0x44E4B0 | `DeeDraw_CreateBusyIcon` | Creates a 64x64 offscreen system-memory surface (DDSD flags 7, caps 0x840). It queries for IDirectDrawSurface3 into `dword_586E14` (errors "Create surface Failed" / "Create Surface 3 Failed" via 0x42E750), sets a source colour key of 0 (vtbl+0x74, flag 8), points the sprite object `dword_586E30` at GType **0x0C** (`hwaitcur.spr` / `mwaitcur.spr`, class 1 = animated, via 0x40EDA0) and clears the enable flag `byte_586EB0`. Called from DeeDraw init (0x44B654, 0x44B770). |
| 0x44E5B0 | `DeeDraw_SetBusyIcon(bool) -> old` | Swaps `byte_586EB0`. Turned on by the save game (0x40490D/0x404998), the war map's start (0x41A084) and the battle map's load (0x439E92). |
| 0x44E5C0 | `DeeDraw_StepBusyIcon` | Called for every chunk read by the file reader 0x415C00 (0x415C95). If the icon is enabled, it locks the 64x64 surface (retrying on DDERR_SURFACELOST 0x887601C2 by restoring the front `586DE8` and back `586EA0` surfaces, and on DDERR_WASSTILLDRAWING 0x8876021C), draws the sprite's current frame into it (0x49D670), then Blts it to the **front buffer** at (8,8) with DDBLT_WAIT\|DDBLT_KEYSRC (0x1008000). It then advances the frame by 0x100 (one frame in 8.8, 0x40EFB0). So during loading and saving an animated icon spins in the top-left corner, bypassing the normal frame. |

### 2.2 Save-file streams (serialiser)

There is a two-level class hierarchy. Each object's writer calls these virtuals; the chunk layout is documented in `core/SaveGame.h`.

* Base `WFileStream` (vtable 0x4AA920; ctor 0x44E950 calls 0x44F830 → 0x435640). Fields: +0x10 version u32, +0x14 `FILE*`, +0x18 path (lstrcpy), +0x36 writing flag, +0x37 reading flag. The "pure" base 0x4AA9D8 has ctor 0x44F830, dtor 0x44F850/0x44F870, and 0x44F880 `SetVersion` (stores +0x10, returns false).
* Derived `WSaveFile` (vtable 0x4AA8A8; ctor 0x44E7D0, dtor 0x44E890) adds +0x38 `char name[40]` (the campaign name).

| Addr | vtbl | Name | What it does |
|---|---|---|---|
| 0x44E7D0 | – | `WSaveFile::WSaveFile(path)` | Opens for read, reads the version and the tag, then reads the name block into +0x38. If any step fails, the name becomes the path itself (`lstrcpyn` 41). Closes the file. The load/save menus use it to read slot names (callers 0x4042D0). |
| 0x44E8A0 | +0x10 | `OpenWrite(version)` (derived) | Calls 0x44EA20(version), then writes the tag `'GAME'` (0x454D4147) and a 0x28-byte block holding the name. |
| 0x44E8F0 | +0x40 | `OpenRead` (derived) | Opens, reads the version, the tag u32 (+0x60) and the name block (+0x48). |
| 0x44E930 | – | `SetName(str)` | `lstrcpyn(+0x38, s, 41)`. Used by 0x404660 (Edit Campaign). |
| 0x44EA20 | +0x10 | `OpenWrite(version)` (base) | `fopen(path,"wb")`, sets +0x36, writes the version u32. |
| 0x44EA60 | +0x40 | `OpenRead` (base) | `fopen(path,"rb")`, sets +0x37, reads the version u32 into +0x10. |
| 0x44EA10 | +0x70 | `OpenWriteDefault` | Calls vtbl+0x10 with 0xF00DF00D as the version. No caller found (unclear). |
| 0x44EAA0 / 0x44EAE0 / 0x44EAC0 | +0x20.. | `WriteU32` / `WriteU16` / `WriteU8` | Used for u32, tag, float, u16 and byte writes. |
| 0x44EB00 | +0x1C | `WriteString` | Writes strlen+1 as a u32, then the bytes including the NUL. |
| 0x44EB60 | +0x14 | `WriteBlock(buf,len)` | Writes len as a u32, then the bytes. |
| 0x44EB40 | +0x18 | `Write(buf,len)` | Calls WriteBlock. |
| 0x44EC20 / 0x44EC80 / 0x44EC50 | +0x50.. | `ReadU32` / `ReadU16` / `ReadU8` | Return whether the read succeeded. |
| 0x44ECC0 | +0x44 | `ReadBlock(buf)` | Reads a u32 length, then that many bytes. |
| 0x44ECB0 | +0x48 | `Read(buf)` | Calls ReadBlock. |
| 0x44ED20 / 0x44E9D0 | +0x6C | `Close` / base dtor | |
| 0x44EBB0 | +0x74 | `CopyRest(fromStream)` | Copies the rest of another open stream in 2 KB chunks. 0x404660 uses it to make a new campaign: it copies `Human.000`/`Martian.000` into the slot under the typed name. |

Strings: "wb", "rb".

### 2.3 "HeepHeep" node pool

| Addr | Name | What it does |
|---|---|---|
| 0x44ED50 | `NodePool::NodePool(heap,count,size)` | Allocates count×size bytes through MEM 0x4148A0 (tag "HeepHeep", flags 0x100) and threads a free list through it with an **8-byte stride**. Fields: +4 used, +8 free, +0xC capacity, +0x10 mem handle, +0x14 free head. Vtable 0x4AA998. |
| 0x44EE00 | `NodePool::Alloc` | Pops a node, zeroes its 2 dwords, used++, free--. Returns 0 when the pool is empty. |
| 0x44EE30 | `NodePool::FreeChain(head)` | Walks a `next`-linked chain, splices it back onto the free list and adjusts the counts. |

Callers: 0x40D181 (ctor), and 0x40D2E3…0x40E509, all in the renderer's draw-list code (0x40CF00–0x40E5xx).

### 2.4 AStar (the generic A* route finder)

The object is 0xC4 bytes (allocated by the caller). Vtable 0x4AA99C.

| Addr | Name | What it does |
|---|---|---|
| 0x44EE70 | `AStar::AStar(adj, maxNodes, graph, start, goal, maxSteps)` | Stores +8 = adjacency table, +0xC = +0xC0 = maxNodes, +0x10 = graph object (cost/goal interface), +0x14 = start, +0x18 = goal, +0x1C = maxSteps. Allocates the node arena (maxNodes×64 bytes, MEM tag "nodeList", flags 0x1100, heap `dword_4D11F8`); allocation failure is fatal: "FindRoute: No memory for route list nodes" (0x4164A0). The first node is the start: g=0, h=graph->Cost(goal,start) (vtbl+4), f=g+h, id=start. It becomes the open list (+0x70). +0x24=2, +0xBC=1 (nodes used), +0xB0 closed list = 0, +0x2C route = 0, +4 status = 0. |
| 0x44EFA0 | `AStar::Run` | Main loop. Pops the head of the sorted open list (+0x70) onto the closed list (+0xB0) and calls it +0xB8 "best". If `graph->IsGoal(best.id, goal)` (vtbl+8) is true, it builds the route. Otherwise it walks `adj[best.id]`: an int list ending at -1, or at -2 followed by a pointer to a `{id,next}` linked list, and calls GenerateSucc for each. A step counter grows by (number of successors − 1) per expansion; at ≥ maxSteps it stops with **status 0** (gave up). An empty open list gives **status 2** (no route; frees the arena with tag "AStar:1"). When the goal is found, it counts the parent chain, allocates a (n+1)×4 route (MEM tag "routeList"; failure is fatal "FindRoute: No memory for route list result"), fills it start→goal, sets +0x28 = length (including the start), +0x20 = goal g (cost), **status 1**, and frees the arena ("AStar:2"). |
| 0x44F120 | `AStar::GenerateSucc(id)` | c = graph->Cost(best.id, id) (vtbl+4). **c < 0 means the step is refused.** g' = best.g + c. If id is already in the open or closed list, the node is added to best's first free child slot (10 slots at +0x14..+0x38), and its g, parent and f are updated if g' is lower. Otherwise it takes a new node from the arena; running out is fatal "GenerateSucc: Out of nodes". Then g=g', h=graph->Cost(id, goal), f=g+h, parent=best, and the node is inserted into the open list **before the first node with f ≥ new f** (so among equal f the newest comes first), then added to best's children. |
| 0x44F280 | `AStarGraph::AStarGraph` | Base vtable 0x4AA9A0 (pure). The war map derives 0x42AB70 (vtable 0x4A9F10: +4 Cost 0x42ABD0, +8 IsGoal 0x4A3E40). |

The 64-byte node is: +0 f, +4 g, +8 h, +0xC id, +0x10 parent, +0x14..+0x3B children[10], +0x3C next (open/closed list link).

The only caller is the war map's 0x424160. It uses 31 nodes (0x1F), the adjacency table at 0x4BD260 and max steps 1000 (0x3E8). The route is accepted only if status==1 and length>1. When the target sector belongs to the enemy or to no one (`+0x5E`), the length must be exactly 2 (one hop).

### 2.5 Smacker movie player (SMACKW32.DLL)

All state is in a static block at `byte_586EB8`: +0x30 SmackBlit handle, +0x34 movie height, +0x38 pointer to a placement struct `{u32 flags, s32 x, s32 y}` (= `dword_586E58`), +0x3C line buffer (`malloc(width*2)`), +0x40.. column-step table (the scaler), +0x1940 DDSURFACEDESC (0x6C), +0x19B0 width, +0x19B4 `Smack*`, +0x19B8/+0x19BC/+0x19C0 the DirectDraw, front and back surfaces, +0x19C4 "scale table built", +0x19C8 SmackDDSurfaceType, +0x19CC "SmackSoundUseDirectSound done", +0x19D0 "scaled mode".

| Addr | Name | What it does |
|---|---|---|
| 0x44F2C0 | `Smk_Play(path, ddraw?, dd2, dsound, surf, placement)` | Calls SmackSoundUseDirectSound once, SmackUseMMX(2), then SmackOpen(path, 0xFE000, -1). On failure it logs "Could not open smacker file %s" (nullsub). Placement flags: bit 8 centres vertically, `y += (screenH − h)/2`; bit 4 centres horizontally, `x += (screenW − w)/2` (screen size is the floats `flt_4BFCDC/E0`). It allocates a line buffer and, if the flags & 0x60, opens SmackBlit. Then it loops: `SmackWait`, and when ready a frame is decoded (0x44F4A0). It quits when **0x44E3E0** (skip check) is true or at the last frame (`smk->frames−1`). Then SmackBlitClose, SmackClose, free. Caller 0x44DE20 (FMV). |
| 0x44F4A0 | `Smk_DoFrame` | Normal mode: Lock(back), SmackToBuffer(at x,y, pitch), SmackDoFrame, Unlock, SmackNextFrame. In scaled mode (+0x19D0) it decodes to a 640-wide buffer, then calls 0x44F5E0 to stretch it to the screen. |
| 0x44F5E0 | `Smk_Scale(screenW, screenH)` | 16-bit only. On first use it builds a column-step table for 640→screenW (the steps are the differences between `round(i*640/W)` values). When screenH ≥ 2×movieH it writes the movie lines to **every other screen line** (an interlaced look) with a top offset of (H−2h)/2; otherwise it writes contiguous lines with a top offset of (H−h)/2. The source pitch is 0x500 (640×2). |
| 0x44E3E0 (DeeDraw, adjacent) | skip check | Skips when `dword_586E64` byte 2 (the "mouse skips" setting) is set and a mouse click is counted (0x4038C0), or when byte 1 is set and **Escape** (VK 27) is down (`GetAsyncKeyState`, the address WoWLauncher patches). |

The engine already plays these (`core/Smacker`, `platform/Movie`, `frontend/MovieScreen`).

### 2.6 Hand-written assembly blitters (screen side)

All the drawing primitives keep their state in .data globals and are wrapped by tiny cdecl thunks at 0x40FD30–0x410070 (BLIT library) and 0x410E50–0x4113F0 (ISOLib). Bit-depth handling is the same everywhere: a "set target(ptr, pitch, bpp)" routine sets a 16-bit flag when bpp is 15 or 16, plus a 50% mask (0x3DEF for 555; 0xFBEF for 565 as written. The usual mask would be 0x7BEF, and 0xFBEF lets the top bit of green leak, which may be a real quirk worth copying). The draw call's mode is `(modeArg | is16bit) & 7` and indexes an 8-entry span-routine table. Bit 0 is 16-bit, bit 1 is translucent (8-bit uses a 64 KB blend table `[src<<8|dst]`; 16-bit uses `(a>>1 & m) + (b>>1 & m)`), and bit 2 is the "shade map" variant.

#### Blitter A: RLE sprite, cursor copy (0x44F890–0x45012C)

| Addr | Name | What it does |
|---|---|---|
| 0x44F890 | `CurBlit_SetTarget(ptr,pitch,bpp)` | Globals 4BFD70/74/78, mask 4BFD88. |
| 0x44F967 | `CurBlit_SetClip(x0,y0,x1,y1)` | 4BFD60..6C. |
| 0x44FE7F | `CurBlit_DrawRLE(dst, pitch, frame, srcX, srcY, w, h, ?, ?, x, y, mode, colourTable)` | Clips, then calls the span routine per row (`call dword_4BFD4C`). Modes ≥4 also walk a second RLE stream (`dword_4C018E`), the shade map. |
| table 0x4C019B | span routines | 0 loc_44FC05 8-bit opaque through the LUT; 1 0x44FC81 16-bit opaque; 2 0x44FD0F 8-bit translucent (blend table 4BFD7C); 3 0x44FDA1 16-bit 50%; 4/6 0x44F9A4 8-bit shade-mapped; 5/7 0x44FA57 16-bit shade-mapped. 0x44FB18 fetches the next shade-map run (count in `byte_4C019A`, colour-table pointer from `dword_4BFD8A[shade]`). |

The only caller is 0x40CC70, which draws the cursor sprite (frame `[obj+0x38]>>8`, shade level via 0x40B500) into the cursor surface for DeeDraw's software cursor (0x44CA4A, 0x44CD49). **Guess:** it is a duplicate of blitter C with separate globals because the cursor is drawn on another thread.

#### Blitter D: raw rectangle copy (0x450130–0x450527)

0x4502F3 `Blit_CopyRect(srcSurfDesc, dstSurfDesc, sx, sy, w, h, dx, dy, mode)` clips against the destination descriptor's clip words (+0xE..+0x14). Span table `funcs_450503` (0x4C021C): 0 0x450130 8-bit copy, 1 0x450171 16-bit copy, 2 0x4501B5 8-bit skipping colour 0, 3 0x450272 16-bit skipping 0. Caller 0x40DAC0, called 7 times from 0x40DCC9–0x40E033 (back-buffer and overlay copies in the renderer).

#### Solid fill (0x450528)

0x450528 `Blit_FillRect(surfDesc, x, y, w, h, colour)` fills a clipped rectangle with a byte, word or dword pattern. It reads `dword_4C01FC`, which it never sets (harmless, unused). Caller 0x49C7C0 (RTypeArrow).

#### Blitter B: unpacked 8-bit image through a colour table (0x450754–0x450D41), the font blitter

| Addr | Name | What it does |
|---|---|---|
| 0x450754 | `ImgBlit_SetTarget` | 4C0294/98/9C, mask 4C02A4. Wrapped with blitter C's 0x450D44 by 0x40FE50, which the renderer calls (0x40E22D) to set both targets. |
| 0x4507B4 | `ImgBlit_SetBlendTable` | 4C02A0. Wrapped by 0x40FE80 together with 0x450DD9 (blitter C), called from the shade-table loader (0x40B391). |
| 0x4507C5 | `ImgBlit_SetClip` | Has a dead "is the clip the whole screen" test. Wrapped by 0x40FE10 together with 0x450DEA. |
| 0x450B38 | `ImgBlit_Draw(dst,pitch,src,sx,sy,w,h,?,srcPitch,x,y,mode,colourTable)` | Row loop calling the table at 0x4C06A6. Modes: 0 loc_4508DD 8-bit LUT; 1 0x450949 16-bit LUT; 2 0x4509D7 8-bit LUT skipping 0; 3 0x4509FD 16-bit LUT skipping 0; 4 0x450A2A 8-bit LUT+blend; 5 0x450A59 16-bit LUT 50%; 6 0x450AAD 8-bit blend skipping 0; 7 0x450AE0 16-bit 50% skipping 0. Here **bit 1 = skip colour 0 and bit 2 = translucent**, the opposite order to the sprite blitters. |

Caller 0x40FD30, called from 0x40F94E (font glyph drawing 0x40F770: the glyph atlas is a plain 8-bit bitmap drawn through the text's shade level).

#### Blitter C: RLE sprites, the main sprite drawer (0x450D44–0x4514E3)

| Addr | Name | What it does |
|---|---|---|
| 0x450D44 | `SprBlit_SetTarget` | 4C0734/38/3C, mask 4C074C. |
| 0x450DD9 | `SprBlit_SetBlendTable` | 4C0740 (8-bit translucency). |
| 0x450DEA | `SprBlit_SetClip` | 4C0724..30. |
| 0x450E16 | `SprBlit_SetShadeMap(rle)` | 4C0B52: the second RLE stream for modes 4–7. Called via 0x40FE00 only from RTypeArrow 0x49C9FA (the war map arrows' lit body). |
| 0x450DA4 | `Spr_PixelAt(frameData, x, row)` | Walks the RLE row to x and returns the colour byte. Via 0x40FDD0 from 0x49BF9A: this is how the war map reads the **sector number under a pixel** of CHI.SPR / the sector map. |
| 0x451235 | `SprBlit_DrawRLE(...)` (13 args) | Clips, then calls the table at 0x4C0B5F per row: 0 loc_450FBB 8-bit opaque LUT; 1 0x451037 16-bit; 2 0x4510C5 8-bit translucent; 3 0x451157 16-bit 50%; 4/6 0x450E27 8-bit shade-mapped; 5/7 0x450EDA 16-bit shade-mapped (0x450F9B fetches the shade run). Callers via 0x40FD80: 0x40EF04 (RTypeSprite draw, i.e. every sprite), 0x49CAA1 (arrows), 0x4A2188 (mini battle map). |

#### PolyLib triangle rasteriser (0x4514E4–0x457003)

| Addr | Name | What it does |
|---|---|---|
| 0x4514F5 | `Poly_SetTarget(ptr, pitch, height, clipX0, clipW, clipY0, clipH, bpp)` | Called via 0x40FFC0 with (0,0,640,480) clipping: y is stored flipped as `height−y`. 15/16-bit selects the second half of the table (+0x10) and the masks 0x1F/0x0A/0x3DEF or 0x3F/0x0B/0xFBEF. |
| 0x4514E4 | `Poly_SetShadeTable` | 4C0B80. Via 0x410020 from the shade loader 0x40B388. |
| 0x456DD0 | `Poly_DrawTriangle(verts, texture, mode)` | The vertex struct is SoA: x[3] at +0, y[3] at +0x10, u[3] +0x20, v[3] +0x30, shade[3] +0x40 (the shade's top bits, `&0xFC <<6`, make the base level). It flips y, sorts the vertices, and **rejects triangles taller than 250 px** (0xFA) or off-screen. It computes outcodes and a "needs clipping" flag, builds edge tables with 0x456CA0 (per-edge x step in 16.16, u/v in 8.8, shade, using the reciprocal table `word_4C10CC`), then dispatches `off_4C1EFC[clip + mode + bppOffset]`. Caller 0x410070 ← 0x49F1FD (WOFLib model drawing). |
| 0x456CA0 | `Poly_SetupEdge` | Fills one edge's interpolants; sets `dword_4C0BBC` if an edge is ≥ 252 rows (overflow abort). |
| table 0x4C1EFC (48 entries) | span fillers | Pairs of (unclipped, clipped). 8-bit: 0x4522B5/0x452691 and 0x452AEC/0x452EEE are **textured + gouraud** (texel `[esi + v<<8 + u]`, then `shadeTable[shade<<8 \| texel]`); 0x4515B2/0x451BB1 and 0x4518A0/0x451F20 have no shade step (**guess:** flat-lit textured); 0x455276/0x4555B8 ×4 (guess: flat or gouraud colour). 16-bit equivalents: 0x45411A/0x454523, 0x4549AB/0x454DD1, 0x45336F/0x4539C2, 0x453680/0x453D54, 0x45597E/0x455D76 (uses the 50% mask 0x4C0D08 heavily, so **translucent**), 0x4561F2/0x456436, 0x4566FE/0x45698D (no texture refs, so flat/gouraud colour). The exact per-mode meaning needs the WOFLib caller (0x49F1FD) to say which modes it asks for. Unclear. |

#### ISOLib (0x457004–0x457F14)

| Addr | Name | What it does |
|---|---|---|
| 0x457004 | `Iso_SetClip(x0,x1,y0,y1)` | 4CA014..20 (via 0x410EF0 from 0x40D702/0x40E564). |
| 0x45798A → 0x4579A4 | `Iso_DrawPatch(patch, x, y, lightRow)` | An RLE patch: byte0 width&0x7F (bit 7 is a flag), byte1 height, byte2 … rows of (skip, count\|0x80 flag). It does trivial clip rejection and picks a lighting-table bank from the patch height (≤8, ≤12, ≤16 add 0x100 each), then calls the span routine in `dword_4CA0BC`. Via 0x410F40 ← 0x49FBE9. |
| 0x457C60 | `Iso_DrawObject(iob, lightBase, x, y)` | Iterates the object's parts (count at +6, part table at +0x22+[+0x1E]; each part has dx/dy at +4/+6, kind at +3, data offset at +0xE). For each it picks the span routine `off_4CA0C0[kind*2 + bpp]` and calls 0x4579A4. Via 0x4113F0 ← 0x49DF26 (RTypeISO). These are the "IOB patches" in Models.h. |
| table 0x4CA0C0 | patch span routines | 0x45702D, 0x457298, 0x4574CD, 0x45774D. They interpolate a light value along the row (`dword_4CA084→4CA08C`), XOR in a per-row dither (`4CA0D8 ^ 4CA0B4`), and write `table[(light&0xFF00) \| texel]`, 16-bit. |
| 0x457D44 | `Iso_FillRect(dst,pitch,x,y,w,h,colour)` | Solid fill (via 0x410F10 ← 0x49FDD7/0x49FE23). |

### 2.7 WResearch: the research system

`WResearch` (0x90 bytes) derives from the world-task base (0x435310 ctor; base methods 0x435430/0x4354B0/0x435560/0x435570/0x4355A0). The war map builds two of them in its constructor (0x417CD0+0x343..0x407), "Human Research" (0x459540) and "Martian Research" (0x45A680). The local player's one goes in **`dword_4E9B88`** and the other side's in **`dword_4E9B8C`** (the order depends on the side, `[player+0x24]`).

Vtables: base 0x4AAA48, Human 0x4AAA78, Martian 0x4AAAA8. Slot order: [0] dtor, [1] 0x4356A0, [2] Load, [3] Save, [4] 0x435430, [5] Open(view), [6] Close, [7] OnMessage, [8] Broadcast, [9] 0x4354B0, [10] UpdateTriggers, [11] AIChooseResearch.

| Addr | Name | What it does |
|---|---|---|
| 0x457F20 / 0x457F60 | static init | `[Research] "Human Open Rate"` = **20** (object 0x5888A8) and `"Martian Open Rate"` = **10** (0x588890). Registered through 0x412160 (int settings); the value is at +0x14. It is the speed at which plan folders open and close (see 0x458D90). |
| 0x4592C0–0x459500, 0x45A4C0–0x45A640 | static init | Seventeen `[Tweak]` float multipliers through 0x416500 (see §4). |
| 0x457FA0 | `WResearch::WResearch(side, name, parent)` | Base ctor: plans +0x48/+0x4C = 0, pages +0x50/+0x54 = 0, +0x58/+0x5C/+0x60 = −1, +0x64 idle = 0, +0x68 view = 0, +0x6C/+0x6D = 0, +0x70..+0x7C layout = 0, items +0x80/+0x84 = 0, +0x8C current = 0. Sets **+0x88 = (side == player side `[dword_4E9B74+0x24]`)**. |
| 0x459540 | `WHumanResearch::WHumanResearch` | Layout +0x70=32, +0x74=30, +0x78=2, +0x7C=22. Creates **8 trigger items** (0x588C40..5C), then the 6 plans + pages + 78 items listed in §2.7.1, then an empty 7th plan (text 0). Ends with 0x458CC0 (unlock pass) and sets `byte_588BA8=1`. |
| 0x45A680 | `WMartianResearch::WMartianResearch` | Layout 4, 10, 0, 32. **6 triggers** (0x588F08..1C), 6 plans, 84 items (§2.7.2). |
| 0x45A130 / 0x45B2E0 | dtors | |
| 0x458100 | `GetOpenPlan` | On the Human side, the last plan. Otherwise the highest-index plan whose 0x45C140 (is-open) is true, or 0. Caller 0x45C180. |
| 0x458150 | `Open(arg)` (vtbl[5]; Human/Martian thunk 0x45A140) | Plays sound **0x19C** ("fcab1", filing cabinet open, via 0x464EE0). Runs 0x458D90 until the folders settle. Creates the view object (0xA8 bytes, 0x45E670, ref-counted +4) at +0x68. Registers the items of open plans with the view (item vtbl+0x14; the first one gets focus via 0x4A43F0→0x42EBE0). Lays the plans out from (+0x70,+0x74) using 0x45C9A0. Shows the current page (+0x5C) with 0x45D380. Then base Open (0x435560), view->Open, and posts **0x80A3** (refresh) to the view. |
| 0x458380 | `Close` (vtbl[6]) | If visible (+0x18): hides the page (0x45E4A0, 0x45CD70), closes every plan (vtbl+0x18) and the view, releases the view, base close. Plays **0x19D** ("fcab2", cabinet close). |
| 0x458420 | `ForEachPlan(a,b,c)` | Calls plan 0x45C1A0(a,b,c) for every plan. Caller 0x45E930 (view; guess: hit-test or draw). |
| 0x458460 | `Cheat_CompleteAll` | With `byte_588BA8` cleared, for every item: state 0 → 0x45BB30 (make available), then 0x45BB90 (complete). Caller: the cheat-key sequence machine 0x4013A0 (keys 2,14,12,4,1,0,2,10 → `dword_4E9B88`). |
| 0x4584C0 | `Load(stream)` (vtbl[2]; 0x45A160 reads the tag first) | Reads +0x58, +0x5C, +0x60 (u32), +0x6C, +0x6D (u8), the current item index (−1 = none), then a plan count, then each plan (vtbl+8). With events off, re-checks every non-trigger item (0x45B920). Settles the folders. |
| 0x4585D0 | `Save(stream)` (vtbl[3]; 0x45A190 writes tag **`HRSH`** 0x48535248, 0x45B2F0 writes **`MRSH`** 0x4853524D) | Mirrors Load: 3×u32, 2×u8, current item's +0x20 index or −1, plan count, then each plan's save (vtbl+0xC). |
| 0x458670 | `AddPlan(&out, textId)` | `new 0x50` plan (ctor 0x45BFA0(index, this, textId)), appended to +0x48/+0x4C. |
| 0x4586E0 | `AddPage(&out, n)` | `new 0x64` page (0x45CB70(pageIndex, lastPlan, n)), appended to +0x50/+0x54 and added to the plan (0x45CDB0). |
| 0x458770 | `AddItem(&out, time, p1, p2, p3, p4)` | `new 0x58` item (0x45B5A0), parent = the last page, appended to +0x80/+0x84 and to the page. |
| 0x458830 / 0x4588F0 / 0x458890 | `AddEnhancement` / `AddBuilding` / `AddUnit(&out, unitType, p1..p4)` | time = **`word_51A0F8[type*0xB0]`** (the unit table's research time, OBJ.ojd record 39 × 24 × a multiplier, see §3). Sets the item kind +0x40 = 2 / 1 / 0, names it (0x45B640), stores the item in the unit table **`dword_51A0F4[type*0xB0]`**, and runs the availability check 0x45B920. |
| 0x458950 | `AddTrigger(&out)` | An item with kind **3**, time 1, no prerequisites, named "Trigger" (`off_4CA494`). |
| 0x458A00 | `SetCurrent(item)` | If it differs from +0x8C: cancels the old one (nullsub_3), sets +0x8C, starts the item (0x45B850: remaining = total if 0), resets idle, and posts 0x80A3 if visible. |
| 0x458A60 | `AIChooseResearch` (vtbl[11] base) / random fallback | First prefers an available (state 1) non-trigger item that has a trigger item among its prerequisites (0x45B8B0). Otherwise it starts at a `rand() % count` item and takes the next available non-trigger one. Calls SetCurrent and returns the item (or 0). |
| 0x458BD0 | `AdvanceResearch(hours)` | Called from the war map's hour tick 0x4198C0 (0x419EE2 for the player, 0x419EEE for the other side). Clears +0x6C/+0x6D. If an item is current, it progresses (0x45BB00(hours)); else idle++. Then it repeats unlock passes (0x45B920 over every non-trigger item) until nothing changes (debug "WResearch::AdvanceResearch - Additional research pass"). If +0x6D was set (by message 0xB9), current = 0. **Research alert:** on the local player's side, when idle > **0x150 (336)** and at least one item is available, it raises war map event **6 ResearchAlert** (0x41E5B0(6,this); text 0x46D "Research Alert") and resets idle. |
| 0x458CC0 | `UnlockPass` | Runs 0x45B920 on every non-trigger item and returns whether any changed. |
| 0x458D10 | `ShowPage(n)` | Plays **0x19B** ("page2", page turn), hides the old page, shows the new one, posts 0x80A3. |
| 0x458D90 | `AnimatePlans(selectedPlan, rate)` | For each plan that is open (0x45C140), or on the Human side the last plan: the selected plan steps by +rate and the others by −rate (0x45C840, the folder's open amount). Returns true while any is still moving. |
| 0x458E50 | `OnMessage(msg, wParam, lParam)` (vtbl[7]) | See §4. Anything unhandled is forwarded to the parent `[+0x1C]` (vtbl+0x1C). |
| 0x459190 | `Broadcast(a,b,c,flag)` | Plan vtbl+0x1C(a,b,c) for every plan, plus vtbl+0x20 when the flag is set. |
| 0x459200 | `LayoutPlans(&pt)` | Places each visible plan from (+0x70,+0x74) through 0x45C9A0 and returns the end point. |
| 0x45A1C0 | `WHumanResearch::UpdateTriggers` (vtbl[10]) | Unlocks Human triggers from unit-type flags `unitTable[type].+0xAC` (set in battle code 0x47F630/0x488C00/0x492931; **guess:** "this enemy type has been encountered"). T0←Martian 0x50–0x52 (Scanning Machine); T3←0x4D–0x4F (Flying Machine); T4←0x53–0x55 (Constrictor); T7←0x4A–0x4C or 0x50–0x52; T1←types 0x197–0x199 (no OBJ name; guess: Black Smoke); T2←0x2C2–0x2C4; T5←0x2BF–0x2C1. **T6 (0x588C58), the prerequisite of "Reading Martian symbols", is never set here** (unclear; maybe by the item index through message 0xB9 from elsewhere). Then an unlock pass, then **applies the improvement multipliers** for completed enhancements (§4). |
| 0x45B320 | `WMartianResearch::UpdateTriggers` | T1←Human 7–9 (Tunnelling Track Layer); T2←0x13–0x18 (Ironclads, Submersibles); T4 and T5←0x273–0x276 (mines). T0 and T3 are never set. Then multipliers, plus flags `byte_589130` (0x28C Enhanced metals detection done) and `byte_589134` (0x28D Detect subterrain excavations done), read by 0x495F8D/0x495FAF. |
| 0x45A400 / 0x45B4E0 | `AIChooseResearch` (Human / Martian, vtbl[11]) | Called from the AI at 0x442F10 for the computer side. On first use it picks one of the side's **AI research plans** at random (Human `off_4CA600`: 2 lists; Martian `off_4CA740`: 2 lists; index in `dword_4CA1A8` / `dword_4CA364`, −1 at start, so it is not saved). Takes the first item in the list that is available (state 1). Debug logs "Randomly chose … Plan %d" / "Starting … research Plan %d Item %d". If none is available, falls back to 0x458A60. |
| 0x45B5A0 | `WResearchItem::WResearchItem(isHuman, index, page, parent, time, p1..p4)` | Base 0x4A4360 ("Research Item"). +0x20 index, +0x24 parent, +0x28 page, +0x2C total time, +0x30..+0x3C prerequisites, +0x48 isHuman, +0x4C state 0, +0x50 = 2, +0x54 remaining 0. Vtable 0x4AAAD8 (methods at 0x45BCC0–0x45BD30, in part 09). |
| 0x45B630 | item dtor | |
| 0x45B640 | `WResearchItem::SetType(type)` | +0x44 = type. +0x0C name = "Trigger" for kind 3, else string `unitTable[type].+0x14` (OBJ.ojd record 21) through 0x4146F0. |

Item state (+0x4C), from the helpers just past the piece: **0 locked, 1 available (0x45B860), 2 complete (0x45B890), 3 also accepted as done by the prerequisite test 0x45B920** (guess: obsolete or abandoned). An item becomes available when every non-null prerequisite is in state 2 or 3. Kind (+0x40): 0 unit, 1 building, 2 enhancement, 3 trigger (0x45B8A0 tests for 3). Trigger unlock 0x45BC70 sets state 2 and posts `0x111 / 0xB9 / index` to its parent.

#### 2.7.1 The Human research tree (0x459540)

These are the order of creation, the item kind, the research time (OBJ.ojd record 39, in days; see §3) and the prerequisites. A prerequisite written `Tn` is trigger item n (§2.7). Page numbers are the global page index. The tree is **hard-coded in the exe**, not read from a data file; only the names and times come from OBJ.ojd.

- **Plan** 0x269 'Control Facilities'
  - page 0
    - 0x3A Command Post [bldg, 0 d]
  - page 2
    - 0x3C Repair Workshop [bldg, 0 d]
- **Plan** 0x267 'Manufacturing'
  - page 3
    - 0x23 Vehicle Factory Level 1 [bldg, 0 d]
    - 0x24 Vehicle Factory Level 2 [bldg, 100 d] <- Vehicle Factory Level 1
    - 0x25 Vehicle Factory Level 3 [bldg, 200 d] <- Vehicle Factory Level 2
  - page 4
    - 0x26 Training Centre [bldg, 0 d]
  - page 5
    - 0x27 Ship Yard Level 1 [bldg, 0 d]
    - 0x28 Ship Yard Level 2 [bldg, 100 d] <- Ship Yard Level 1
    - 0x29 Ship Yard Level 3 [bldg, 200 d] <- Ship Yard Level 2
  - page 6
    - 0x2A Aircraft Hangar Level 1 [bldg, 50 d]
    - 0x2B Aircraft Hangar Level 2 [bldg, 90 d] <- Aircraft Hangar Level 1
    - 0x2C Aircraft Hangar Level 3 [bldg, 130 d] <- Aircraft Hangar Level 2
  - page 7
    - 0x2D Munitions Factory [bldg, 60 d] <- Ship Yard Level 1
  - page 8
    - 0x2E Construction Labs [bldg, 50 d] <- Ship Yard Level 1
  - page 9
    - 0x2F Steel Refinery [bldg, 0 d]
  - page 10
    - 0x30 Coal Mine [bldg, 0 d]
  - page 11
    - 0x31 Oil Refinery [bldg, 0 d]
- **Plan** 0x270 'Offensive Vehicles'
  - page 12
    - 0x1 Mark I Armoured Lorry [unit, 0 d] <- Vehicle Factory Level 1
    - 0x2 Mark II Armoured Lorry [unit, 40 d] <- Mark I Armoured Lorry; Vehicle Factory Level 2
    - 0x3 Mark III Armoured Lorry [unit, 80 d] <- Mark II Armoured Lorry; Vehicle Factory Level 3
  - page 13
    - 0x4 Mark I Armoured Track Layer [unit, 40 d] <- Mark I Armoured Lorry; Vehicle Factory Level 1
    - 0x5 Mark II Armoured Track Layer [unit, 80 d] <- Mark I Armoured Track Layer; Vehicle Factory Level 2
    - 0x6 Mark III Armoured Track Layer [unit, 120 d] <- Mark II Armoured Track Layer; Vehicle Factory Level 3
  - page 14
    - 0x7 Mark I Tunnelling Track Layer [unit, 40 d] <- Mark I Armoured Track Layer; Vehicle Factory Level 1; Construction Labs
    - 0x8 Mark II Tunnelling Track Layer [unit, 80 d] <- Mark I Tunnelling Track Layer; Vehicle Factory Level 2
    - 0x9 Mark III Tunnelling Track Layer [unit, 120 d] <- Mark II Tunnelling Track Layer; Vehicle Factory Level 3
    - 0x265 Enhanced Drill [enh, 20 d] <- Mark I Tunnelling Track Layer
    - 0x266 Tunnel reinforcing apparatus [enh, 20 d] <- Mark I Tunnelling Track Layer; T2
  - page 15
    - 0xA Mark I Self Propelled Gun [unit, 35 d] <- Mark I Armoured Lorry; Vehicle Factory Level 1; Munitions Factory
    - 0xB Mark II Self Propelled Gun [unit, 65 d] <- Mark I Self Propelled Gun; Vehicle Factory Level 2
    - 0xC Mark III Self Propelled Gun [unit, 100 d] <- Mark II Self Propelled Gun; Vehicle Factory Level 3
    - 0x267 not used High Energy Capacitor Shell [enh, 30 d] <- Mark II Self Propelled Gun; T0
  - page 16
    - 0xE Mark I Mobile Anti-Aircraft Array [unit, 15 d] <- Mark I Armoured Lorry; T3; Vehicle Factory Level 1
    - 0xF Mark II Mobile Anti-Aircraft Array [unit, 40 d] <- Mark I Mobile Anti-Aircraft Array; Vehicle Factory Level 2
    - 0x10 Mark III Mobile Anti-Aircraft Array [unit, 80 d] <- Mark II Mobile Anti-Aircraft Array; Vehicle Factory Level 3
- **Plan** 0x266 'Support Vehicles'
  - page 17
    - 0x11 Sappers Lorry [unit, 0 d] <- Vehicle Factory Level 1; Training Centre
    - 0x12 Modified Sappers Lorry [unit, 20 d] <- Sappers Lorry; Vehicle Factory Level 2
  - page 18
    - 0x13 Formidable Class Ironclad [unit, 0 d] <- Ship Yard Level 1
    - 0x14 Warrior Class Ironclad [unit, 85 d] <- Formidable Class Ironclad; Ship Yard Level 2
    - 0x15 Invincible Class Ironclad [unit, 160 d] <- Warrior Class Ironclad; Ship Yard Level 3
  - page 19
    - 0x16 Model A Submersible [unit, 35 d] <- Formidable Class Ironclad; Ship Yard Level 1; Construction Labs
    - 0x17 Model B Submersible [unit, 70 d] <- Model A Submersible; Ship Yard Level 2
    - 0x18 Model C Submersible [unit, 130 d] <- Model B Submersible; Ship Yard Level 3
  - page 20
    - 0x19 Observation Balloon [unit, 0 d] <- Aircraft Hangar Level 1
    - 0x1A Powered Observation Balloon [unit, 30 d] <- Observation Balloon; Aircraft Hangar Level 2
    - 0x1B Powered Observation Air Ship [unit, 60 d] <- Powered Observation Balloon; Aircraft Hangar Level 3
  - page 21
    - 0x1C Construction Vehicle [unit, 0 d] <- Vehicle Factory Level 1; Training Centre
    - 0x1D Modified Construction Vehicle [unit, 30 d] <- Construction Vehicle; Vehicle Factory Level 2
  - page 22
    - 0x1E Mobile Repair Vehicle [unit, 15 d] <- Construction Vehicle; Vehicle Factory Level 1; Training Centre
    - 0x1F Modified Mobile Repair Vehicle [unit, 40 d] <- Mobile Repair Vehicle; Vehicle Factory Level 2
  - page 23
    - 0x20 Mark I Mortar Bike [unit, 30 d] <- Construction Vehicle; Vehicle Factory Level 1; Training Centre
    - 0x21 Mark II Mortar Bike [unit, 50 d] <- Mark I Mortar Bike; Vehicle Factory Level 2
    - 0x22 Mark III Mortar Bike [unit, 90 d] <- Mark II Mortar Bike; Vehicle Factory Level 3
- **Plan** 0x268 'Defences'
  - page 24
    - 0x273 Mines [enh, 12 d]
    - 0x274 Wire triggered mines [enh, 28 d] <- Mines
    - 0x276 Enhanced mines [enh, 40 d] <- Wire triggered mines; T7
  - page 25
    - 0x1C3 Wire fence [enh, 0 d]
    - 0x1C4 Wire wall [enh, 15 d] <- Wire fence
    - 0x1C5 Mined Wire wall [enh, 40 d] <- Wire wall
  - page 26
    - 0x32 4.6 Inch Medium Gun Post [unit, 0 d]
    - 0x33 6 Inch Medium Gun Post [unit, 20 d] <- 4.6 Inch Medium Gun Post
  - page 27
    - 0x34 6 Inch Heavy Gun Emplacement [unit, 30 d] <- 4.6 Inch Medium Gun Post
    - 0x35 9.2 Inch Heavy Gun Emplacement [unit, 60 d] <- 6 Inch Heavy Gun Emplacement
    - 0x36 12 Inch Heavy Gun Emplacement [unit, 120 d] <- 9.2 Inch Heavy Gun Emplacement
  - page 28
    - 0x37 1 Pdr Anti Aircraft Array [unit, 10 d] <- 4.6 Inch Medium Gun Post; T3
    - 0x38 2 Pdr Anti Aircraft Array [unit, 40 d] <- 1 Pdr Anti Aircraft Array
    - 0x39 3 Pdr Anti Aircraft Array [unit, 80 d] <- 2 Pdr Anti Aircraft Array
- **Plan** 0x265 'General Enhancements'
  - page 29
    - 0x256 Heat resistant armour [enh, 30 d]
    - 0x257 Insulated high energy resistant armour [enh, 60 d] <- Heat resistant armour
    - 0x259 High explosive rounds [enh, 40 d]
    - 0x25A Improved powder charge [enh, 25 d]
    - 0x25B Advanced powder charge [enh, 45 d] <- Improved powder charge
    - 0x25C Fine air filters [enh, 15 d] <- T1
    - 0x25D Chemical filters [enh, 30 d] <- Fine air filters
    - 0x25E Enhanced sighting apparatus [enh, 35 d]
  - page 30
    - 0x260 Reading Martian symbols [enh, 20 d] <- T6
    - 0x261 Martian signal methods [enh, 55 d] <- Reading Martian symbols
    - 0x262 Martian language [enh, 110 d] <- Martian signal methods
- **Plan** 0 (text 0, empty), added last. On the Human side the last plan is always treated as open (0x458100, 0x458150, 0x458D90).

#### 2.7.2 The Martian research tree (0x45A680)

- **Plan** 0x26E 'Control Sites'
  - page 31
    - 0x77 Power Plant [bldg, 0 d]
  - page 32
    - 0x78 Communications Centre [bldg, 0 d]
    - 0x2AB Motion scanner [enh, 25 d] <- T1
    - 0x2AC Advanced motion scanner [enh, 45 d] <- Motion scanner
  - page 33
    - 0x79 Repair Facility [bldg, 0 d]
- **Plan** 0x26D 'Production Plants'
  - page 35
    - 0x60 Constructor Level 1 [unit, 0 d]
    - 0x61 Constructor Level 2 [unit, 100 d] <- Constructor Level 1
    - 0x62 Constructor Level 3 [unit, 200 d] <- Constructor Level 2
  - page 36
    - 0x63 Energy Weapon Plant [bldg, 50 d] <- Constructor Level 1
  - page 37
    - 0x64 Suspension Field Site Level 1 [bldg, 80 d] <- Constructor Level 1
    - 0x65 Suspension Field Site Level 2 [bldg, 120 d] <- Suspension Field Site Level 1
    - 0x66 Suspension Field Site Level 3 [bldg, 180 d] <- Suspension Field Site Level 2
  - page 38
    - 0x67 Telepathic Training Centre Level 1 [bldg, 120 d] <- Constructor Level 1
    - 0x68 Telepathic Training Centre Level 2 [bldg, 160 d] <- Telepathic Training Centre Level 1
    - 0x69 Telepathic Training Centre Level 3 [bldg, 230 d] <- Telepathic Training Centre Level 2
  - page 39
    - 0x6A Biochemical Plant [bldg, 60 d] <- Constructor Level 1
  - page 40
    - 0x6B Explosives Plant [bldg, 50 d] <- Constructor Level 1
  - page 41
    - 0x6C Human Farm [bldg, 0 d]
  - page 42
    - 0x6E Heavy Element Plant [bldg, 0 d]
  - page 43
    - 0x6D Copper Forge [bldg, 0 d]
- **Plan** 0x26B 'Attack Machines'
  - page 44
    - 0x3E Scout Machine - 1st Gen [unit, 0 d] <- Constructor Level 1
    - 0x3F Scout Machine - 2nd Gen [unit, 40 d] <- Scout Machine - 1st Gen; Constructor Level 2
    - 0x40 Scout Machine - 3rd Gen [unit, 70 d] <- Scout Machine - 2nd Gen; Constructor Level 3
  - page 45
    - 0x41 Fighting Machine - 1st Gen [unit, 0 d] <- Constructor Level 1
    - 0x42 Fighting Machine - 2nd Gen [unit, 50 d] <- Fighting Machine - 1st Gen; Constructor Level 2
    - 0x43 Fighting Machine - 3rd Gen [unit, 110 d] <- Fighting Machine - 2nd Gen; Constructor Level 3
  - page 46
    - 0x44 Tempest - 1st Gen [unit, 65 d] <- Fighting Machine - 2nd Gen; Constructor Level 1
    - 0x45 Tempest - 2nd Gen [unit, 100 d] <- Tempest - 1st Gen; Constructor Level 2
    - 0x46 Tempest - 3rd Gen [unit, 160 d] <- Tempest - 2nd Gen; Constructor Level 3
  - page 47
    - 0x47 Bombarding Machine - 1st Gen [unit, 35 d] <- Constructor Level 1; Explosives Plant
    - 0x48 Bombarding Machine - 2nd Gen [unit, 80 d] <- Bombarding Machine - 1st Gen; Constructor Level 2
    - 0x49 Bombarding Machine - 3rd Gen [unit, 120 d] <- Bombarding Machine - 2nd Gen; Constructor Level 3
    - 0x286 Shock bombs [enh, 20 d] <- Bombarding Machine - 1st Gen; T1
    - 0x289 Hover bombs [enh, 20 d] <- Bombarding Machine - 1st Gen; T2
  - page 48
    - 0x4A Electric Machine - 1st Gen [unit, 35 d] <- Constructor Level 1; Energy Weapon Plant
    - 0x4B Electric Machine - 2nd Gen [unit, 70 d] <- Electric Machine - 1st Gen; Constructor Level 2
    - 0x4C Electric Machine - 3rd Gen [unit, 110 d] <- Electric Machine - 2nd Gen; Constructor Level 3
    - 0x28A High energy shock wave [enh, 25 d] <- Electric Machine - 1st Gen; T5
  - page 49
    - 0x4D Flying Machine - 1st Gen [unit, 0 d] <- Suspension Field Site Level 1
    - 0x4E Flying Machine - 2nd Gen [unit, 46 d] <- Flying Machine - 1st Gen; Suspension Field Site Level 2
    - 0x4F Flying Machine - 3rd Gen [unit, 100 d] <- Flying Machine - 2nd Gen; Suspension Field Site Level 3
- **Plan** 0x26C 'Specialist Machines'
  - page 50
    - 0x50 Scanning Machine - 1st Gen [unit, 30 d] <- Constructor Level 1; Energy Weapon Plant
    - 0x51 Scanning Machine - 2nd Gen [unit, 70 d] <- Scanning Machine - 1st Gen; Constructor Level 2
    - 0x52 Scanning Machine - 3rd Gen [unit, 130 d] <- Scanning Machine - 2nd Gen; Constructor Level 3
    - 0x28C Enhanced metals detection [enh, 20 d] <- Scanning Machine - 1st Gen; T4
    - 0x28D Detect subterrain excavations [enh, 25 d] <- Scanning Machine - 1st Gen; T1
    - 0x28E Energy pulse generator [enh, 22 d] <- Detect subterrain excavations; T5
    - 0x28F Neutralise power source [enh, 25 d] <- Energy pulse generator
  - page 51
    - 0x53 Constrictor - 1st Gen [unit, 30 d] <- Constructor Level 1; Biochemical Plant
    - 0x54 Constrictor - 2nd Gen [unit, 60 d] <- Constrictor - 1st Gen; Constructor Level 2
    - 0x55 Constrictor - 3rd Gen [unit, 110 d] <- Constrictor - 2nd Gen; Constructor Level 3
  - page 52
    - 0x56 Xeno Telepath - 1st Gen [unit, 0 d] <- Telepathic Training Centre Level 1
    - 0x57 Xeno Telepath - 2nd Gen [unit, 55 d] <- Xeno Telepath - 1st Gen; Telepathic Training Centre Level 2
    - 0x58 Xeno Telepath - 3rd Gen [unit, 110 d] <- Xeno Telepath - 2nd Gen; Telepathic Training Centre Level 3
    - 0x294 Awe [enh, 30 d] <- Xeno Telepath - 1st Gen
    - 0x295 Fear [enh, 60 d] <- Awe
    - 0x29E Lunacy [enh, 90 d] <- Fear
  - page 53
    - 0x59 Handling Machine [unit, 0 d] <- Constructor Level 1
    - 0x5A Advanced Handling Machine [unit, 30 d] <- Handling Machine; Constructor Level 2
  - page 54
    - 0x5B Digging Mechanism [unit, 0 d] <- Constructor Level 1
    - 0x5C Advanced Digging Mechanism [unit, 25 d] <- Digging Mechanism; Constructor Level 2
  - page 55
    - 0x5D Drone - 1st Gen [unit, 20 d] <- Constructor Level 1; Explosives Plant
    - 0x5E Drone - 2nd Gen [unit, 40 d] <- Drone - 1st Gen; Constructor Level 2
    - 0x5F Drone - 3rd Gen [unit, 70 d] <- Drone - 2nd Gen; Constructor Level 3
- **Plan** 0x268 'Defences'
  - page 56
    - 0x1C6 Electric field fencing [enh, 0 d]
    - 0x1C7 Laser fencing [enh, 30 d] <- Electric field fencing
    - 0x1C8 Plasma fencing [enh, 80 d] <- Laser fencing
  - page 57
    - 0x6F 12 KrK Rapid Heat Ray [unit, 0 d]
    - 0x70 18 KrK Rapid Heat Ray [unit, 40 d] <- 12 KrK Rapid Heat Ray
  - page 58
    - 0x71 46 KrK Heat Ray Turret [unit, 30 d] <- 12 KrK Rapid Heat Ray
    - 0x72 62 KrK Heat Ray Turret [unit, 60 d] <- 46 KrK Heat Ray Turret
    - 0x73 80 KrK Heat Ray Turret [unit, 120 d] <- 62 KrK Heat Ray Turret
  - page 59
    - 0x74 102 DnO Projectile Launcher [unit, 25 d] <- 12 KrK Rapid Heat Ray
    - 0x75 130 DnO Projectile Launcher [unit, 50 d] <- 102 DnO Projectile Launcher
    - 0x76 165 DnO Projectile Launcher [unit, 100 d] <- 130 DnO Projectile Launcher
- **Plan** 0x265 'General Enhancements'
  - page 60
    - 0x27B Shock Resistant Armour [enh, 30 d]
    - 0x27C Impact Reflecting Armour [enh, 60 d] <- Shock Resistant Armour
    - 0x27D Enhanced Ray Focus Mechanism [enh, 40 d]
    - 0x27E Tight beam lenses [enh, 80 d] <- Enhanced Ray Focus Mechanism
    - 0x27F Secondary beam amplifiers [enh, 40 d]
    - 0x280 Motion tracking device [enh, 50 d]
  - page 61
    - 0x283 Mars based optical array [enh, 20 d]
    - 0x284 Mars based telescope [enh, 40 d] <- Mars based optical array
    - 0x285 Mars based long range scanner [enh, 80 d] <- Mars based telescope

#### 2.7.3 The AI research plans

The AI goes down these lists in order and starts the first item it finds available (0x45A400 / 0x45B4E0). The ids are unit types.

* Human plan A (0x4CA4F8): 1C4 37 4 2D E A 34 24 1D 5 273 256 33 B 25A 35 274 25 6 12 36 C F 259 10 1C5 25E 257 25B 2 38 3 39
* Human plan B (0x4CA580): 37 28 14 34 1C4 2A 4 20 273 256 24 38 2 1D 35 259 39 29 15 2 21 25E 12 33 1C5 25A 25 3 22 25B 36
* Martian plan A (0x4CA610): 28A 286 289 6B 47 71 63 4A 61 42 1C7 74 44 27B 5D 48 72 5A 27F 5C 4B 75 1C8 45 27D 62 43 280 5E 49 27C 70 3F 4C 46 27E 73 76
* Martian plan B (0x4CA6B0): 28A 64 74 27B 1C7 63 4A 71 67 56 65 4E 61 5A 27F 75 4B 3F 27D 5C 68 57 66 4F 76 27E 1C8 62 4C 69 58 70 280 40 27C

Each list ends with a NULL; the table of lists (0x4CA600 / 0x4CA740) ends with a NULL too.

---

## 3. Data structures and globals

**WResearch** (0x90 bytes):

| Off | Meaning |
|---|---|
| +0x18 | visible (base task) |
| +0x1C | parent (message forwarding) |
| +0x48/+0x4C | plans[] / count |
| +0x50/+0x54 | pages[] / count |
| +0x58 | selected plan (−1 = none) |
| +0x5C | current page (−1) |
| +0x60 | selected item (UI) (−1) |
| +0x64 | idle counter (hours with no research) |
| +0x68 | view (0xA8, 0x45E670, refcount at +4) |
| +0x6C | flag set by command 0xB8 (unclear) |
| +0x6D | "clear current" flag set by 0xB9 |
| +0x70,+0x74 | plan layout origin |
| +0x78,+0x7C | plan layout step |
| +0x80/+0x84 | items[] / count |
| +0x88 | belongs to the local player |
| +0x8C | current item |

**WResearchItem** (0x58 bytes; vtable 0x4AAAD8; base 0x4A4360):

| Off | Meaning |
|---|---|
| +0x0C | name (`char*`) |
| +0x20 | index in items[] (saved as the current item's id) |
| +0x24 | parent (page) |
| +0x28 | page |
| +0x2C | total time (hours) |
| +0x30..+0x3C | prerequisites[4] |
| +0x40 | kind (0 unit, 1 building, 2 enhancement, 3 trigger) |
| +0x44 | unit type |
| +0x48 | isHuman |
| +0x4C | state (0 locked, 1 available, 2 done, 3 done-alt) |
| +0x50 | = 2 (unclear) |
| +0x54 | remaining time |

**Plan** (0x50 bytes, 0x45BFA0) and **Page** (0x64 bytes, 0x45CB70) are implemented in part 09. Plan methods used here: 0x45C140 IsOpen, 0x45C1A0, 0x45C840 StepOpen(rate), 0x45C9A0 Layout, vtbl +8 Load, +0xC Save, +0x18 Close, +0x1C/+0x20 broadcast. Page methods: 0x45D380 Show(view), 0x45E4A0 Hide, 0x45CD70, 0x45CDB0 AddChild.

**Unit-type table** (0x51A0F0, 0xB0 bytes a type; the fields this piece uses):

| Off | Meaning |
|---|---|
| +0x04 | `WResearchItem*` for this type (`dword_51A0F4`), read by 0x4423D0 / 0x44248C (construction or AI can-build checks) |
| +0x08 | research time `word_51A0F8` = record 39 × 24 × a factor (by +0x0C category and side: `flt_4F6E34`/`flt_4F050C` for category 0, `flt_4F709C`/`flt_4F2F0C` for 1; 0x437750 case 39) |
| +0x14 | name text id (record 21) |
| +0xAC | per-type "encountered" flag (guess; writers 0x47F630, 0x488C00, 0x492931; reader 0x4424B0) |

**AStar node**: see §2.4. AStar object fields: +4 status, +8 adj, +0xC/+0xC0 maxNodes, +0x10 graph, +0x14 start, +0x18 goal, +0x1C maxSteps, +0x20 route cost, +0x24 counter, +0x28 route length, +0x2C route mem handle (data at handle+0x14), +0x30 node-arena handle, +0x70 open list, +0xB0 closed list, +0xB4 arena base, +0xB8 best, +0xBC nodes used.

**Busy icon**: `dword_586E14` surface, `dword_586E30` sprite, `byte_586EB0` enabled. DirectDraw objects: `dword_586DF4` DirectDraw, `586DE8` front, `586EA0` back.

**Research globals**:

| Global | Meaning |
|---|---|
| `dword_4E9B88` | local player's research |
| `dword_4E9B8C` | other side's research |
| `byte_588BA8` | research events enabled (0 while building or loading the tree; also cleared during the cheat) |
| `dword_588C40..5C` | Human triggers T0–T7 |
| `dword_588F08..1C` | Martian triggers T0–T5 |
| `dword_588C60..588D94` | Human item pointers |
| `dword_588F20..58906C` | Martian item pointers |
| `dword_4CA1A8`, `dword_4CA364` | AI plan choice (−1) |
| `byte_589130`, `byte_589134` | Martian detection upgrades |
| `byte_4E8D20` | tooltip buffer |

**Blitter globals**: A 0x4BFD04–0x4C01B7; B 0x4C022C–0x4C06C6; C 0x4C06CC–0x4C0B7B; Poly 0x4C0B80–0x4C1FCC (edge buffer `unk_4C12CC`, reciprocals `word_4C10CC`); ISO 0x4CA00C–0x4CA114.

---

## 4. Messages, commands, strings and tuning

**WResearch::OnMessage (0x458E50)** handles `0x111` (WM_COMMAND-style, wParam low word = id, lParam = argument):

| Id | Action |
|---|---|
| 0x84 | Select item `lParam`: if it changed, +0x60 = lParam, sound **0x132** (speech, VOX G63/G64), SetCurrent(items[lParam]), i.e. "research this" |
| 0xB8 | +0x6C = 1 (unclear) |
| 0xB9 | An item completed or a trigger fired: idle = 0, +0x6D = 1 (current cleared in the next AdvanceResearch). Posted by 0x45BC70 and (guess) by item completion in part 09 |
| 0xBC | Folder animation step: if 0x458D90(selectedPlan, openRate) is still moving, re-post 0xBC (0x4355A0); then lay out the plans (0x459200) |
| 0xBD | 0x42DB00(1): posts 0x8084(1) through the desktop (guess: pause/unpause, or close) |
| 0xBE | Click a plan folder `lParam`: toggles the selection; sound **0x199** (open) or **0x19A** (close) ("foldr1-3"); ShowPage(−1); posts 0xBC |
| 0xBF | Turn to page `lParam`: sound **0x19B** ("page2"), hide the old page, show the new one, post 0x80A3 |
| 0x8114 | Tooltip query for the item in wParam's high word: text **0x46E "Makes Available:"** followed by "\n" and the name of every non-trigger item that lists it as a prerequisite (0x45BA30, 0x45B690). Result pointer `*lParam` = `byte_4E8D20`, or NULL if there are none |

Sent: **0x80A3** to the view (refresh). War map event **6 ResearchAlert** (0x41E5B0). Other sounds: **0x19C** cabinet open, **0x19D** cabinet close. Texts: plan names 0x265–0x270 (General Enhancements, Support Vehicles, Manufacturing, Defences, Control Facilities, Attack Machines, Specialist Machines, Production Plants, Control Sites, Offensive Vehicles), 0x46D "Research Alert", 0x46E "Makes Available:", 0x41A "Research completed" (event 4, raised in part 09).

**Settings** (registry or INI):

| Section | Key | Default | Global (value at +0x14) | Applied when completed | Multiplier global → reader |
|---|---|---|---|---|---|
| Research | Human Open Rate | 20 | 0x5888A8 | folder open speed | |
| Research | Martian Open Rate | 10 | 0x588890 | | |
| Tweak | HeatResistantArmourMultiplier | 0.85 | 0x588BC8 | 0x256 Heat resistant armour | 0x4CA180 → 0x4958B0 (damage taken) |
| Tweak | InsulatedHighEnergyResistantArmourMultiplier | 0.75 | 0x588E30 | 0x257 | 0x4CA184 → 0x4958B0 |
| Tweak | HighExplosiveRoundsMultiplier | 1.15 | 0x588BB0 | 0x259 | 0x4CA188 → 0x4958B0+0x1DD |
| Tweak | ImprovedPowderChargeMultiplier | 1.15 | 0x588E60 | 0x25A | 0x4CA18C → 0x494AF0 (weapon) |
| Tweak | AdvancedPowderChargeMultiplier | 1.30 | 0x588C10 | 0x25B | 0x4CA190 → 0x494AF0 |
| Tweak | FineAirFiltersMultiplier | 0.85 | 0x588C28 | 0x25C | 0x4CA194 → 0x4958B0 |
| Tweak | ChemicalFiltersMultiplier | 0.75 | 0x588E48 | 0x25D | 0x4CA198 → 0x4958B0 |
| Tweak | EnhancedSightingApparatusMultiplier | 1.20 | 0x588BF8 | 0x25E | 0x4CA19C → 0x494AF0 |
| Tweak | TunnelReinforcementApparatusMultiplier | 0.75 | 0x588E78 | 0x266 | 0x4CA1A0 → 0x4958B0 |
| Tweak | EnhancedDrillMultiplier | 1.25 | 0x588BE0 | 0x265 | 0x4CA1A4 → 0x493D39 (route finder or tunnelling, guess) |
| Tweak | ShockResistantArmourMultiplier | 0.85 | 0x588E90 | 0x27B | 0x4CA348 → 0x4958B0 |
| Tweak | ImpactReflectingArmourMultiplier | 0.75 | 0x589118 | 0x27C | 0x4CA34C → 0x4958B0 |
| Tweak | EnhancedRayFocusMechanismMultiplier | 1.15 | 0x589070 | 0x27D | 0x4CA350 → 0x494AF0 |
| Tweak | TightBeamMultiplier | 1.30 | 0x589100 | 0x27E | 0x4CA354 → 0x494AF0 |
| Tweak | HoverBombsMultiplier | 1.25 | 0x588EA8 | 0x289 | 0x4CA358 → 0x494AF0 |
| Tweak | SecondaryBeamAmplifiersMultiplier | 1.15 | 0x588EC0 | 0x27F | 0x4CA35C → 0x4958B0+0x175 |
| Tweak | MotionTrackingDeviceMultiplier | 1.20 | 0x588EF0 | 0x280 | 0x4CA360 → 0x494AF0 |

All the multiplier globals start at 1.0 (0x3F800000). They are set, never reset, on each UpdateTriggers call once the item is done. They are not saved directly; they are re-derived from item states.

**Other constants**:

| Value | Meaning |
|---|---|
| 0x150 (336) | idle hours before a Research Alert |
| 31 | AStar nodes used by the war map |
| 1000 | AStar max steps used by the war map |
| 10 | AStar child slots per node |
| 64 | AStar node size |
| 250 | maximum triangle height (px); taller triangles are rejected |
| 0x500 | Smacker scaled-buffer pitch (640×2) |
| 0xFE000 | SmackOpen flags |
| 2 | SmackUseMMX |
| (8,8) | busy icon position on the front buffer |

**Debug strings** (nullsub_4 / fatal 0x4164A0): "FindRoute: No memory for route list nodes" / "…result", "GenerateSucc: Out of nodes", "AStar:1", "AStar:2", "nodeList", "routeList", "HeepHeep", "Could not open smacker file %s", "Create surface Failed", "Create Surface 3 Failed", "WResearch::AdvanceResearch - Additional research pass", "Randomly chose Human research Plan %d", "Starting Human research Plan %d Item %d", "Randomly chose martian research Plan %d", "Starting Martian research Plan %d Item %d", "Research Item", "Trigger", "Human Research", "Martian Research" (names passed in by 0x417CD0).

---

## 5. Cross-references to other pieces

| From this piece | To | Why |
|---|---|---|
| 0x44E4B0 | ← 0x44B654/0x44B770 (DeeDraw init) | busy icon set-up |
| 0x44E5B0 | ← 0x40490D, 0x404998 (save), 0x41A084 (war map), 0x439E92 (battle map load) | |
| 0x44E5C0 | ← 0x415C95 (file read loop); → 0x49D670, 0x40EFB0 | |
| 0x44E7D0.. | ← 0x4042D0, 0x4043D0, 0x404660 (save/load menus, new campaign) | `GAME` chunk |
| 0x44ED50.. | ← 0x40CF00/0x40D210/0x40D330 (renderer draw lists) | |
| 0x44EE70/0x44EFA0 | ← 0x424160 (war map route); graph 0x42AB70 (Cost 0x42ABD0, IsGoal 0x4A3E40), adjacency 0x4BD260 | |
| 0x44F2C0 | ← 0x44DE20 (FMV); → 0x44E3E0 skip | |
| Blitters | ← 0x40CC70 (cursor), 0x40DAC0, 0x40F94E (font), 0x40EF04 (sprites), 0x49C9FA/0x49CAA1 (RTypeArrow), 0x4A2188 (mini battle map), 0x49BF9A (sector pixel lookup), 0x49F1FD (WOFLib), 0x49DF26 (RTypeISO), 0x49FBE9/0x49FDD7, 0x40B388/0x40B391 (shade loader), 0x40E216/0x40E22D (renderer target set-up) | |
| WResearch ctors | ← 0x417CD0 (WWarMapWorld) | |
| Update triggers (vtbl+0x28) | ← 0x418D17, 0x41A094 | |
| Advance | ← 0x419EE2/0x419EEE (war map hour tick, after 0x802D broadcasts) | |
| Save/Load | ← 0x41ACDB / 0x41AE27 (WMAP children) | |
| AI choose (vtbl+0x2C) | ← 0x442F10 (AI) | |
| Cheat | ← 0x4013A0 | |
| Unit table +4 item pointer | read by 0x4423D0 / 0x44248C (can-build checks) | |
| Items, plans, pages, view | → part 09: 0x45B690–0x45E930 (0x45B850, 0x45B860, 0x45B890, 0x45B8A0, 0x45B8B0, 0x45B920, 0x45BA30, 0x45BB00, 0x45BB30, 0x45BB90, 0x45BC70, 0x45BFA0, 0x45C140, 0x45C1A0, 0x45C840, 0x45C9A0, 0x45CB70, 0x45CD70, 0x45CDB0, 0x45D380, 0x45E4A0, 0x45E670) | |
| Events | → 0x41E5B0 (event 6) | |
| Sounds | → 0x464EE0 | |
| Multipliers read by | 0x493D39, 0x494AF0, 0x4958B0, 0x495F8D/0x495FAF (battle map combat / vehicles) | |

---

## 6. Missing in WoWEngine: candidate tasks (in priority order)

WoWEngine currently has the movie player, sprite/font/translucent blitting, an affine textured triangle for the arrows, a war map route finder (Dijkstra) and the save header/tag scan. It has **no research at all** (WARMAP.md "Still to do: … research"), no AI, no chunk-by-chunk save reading, and no busy icon.

1. **Research data model (high).** Port WResearchItem / Plan / Page and the two hard-coded trees (§2.7.1 and §2.7.2, 0x459540 and 0x45A680), including the trigger items, kinds, states 0–3, the prerequisite rule (every prerequisite in state 2 or 3, 0x45B920 in part 09), and the research time from OBJ.ojd record 39 × 24 × the category/side factor (0x437750 case 39, `flt_4F6E34`/`4F050C`/`4F709C`/`4F2F0C`). Extend `ObjOjd::UnitData` with records 21 (name) and 39 (time). Addresses: 0x458670–0x458950, 0x45B5A0, 0x45B640.
2. **Research progression on the war map's hour tick (high).** 0x458BD0 is called for both sides from 0x419EE2/0x419EEE; repeated unlock passes; the 336-hour idle Research Alert (event 6, text 0x46D) for the local player; current-item handling via 0xB9 (0x458E50). Also ResearchCompleted (event 4) and ResearchAvailable (event 5), raised from item code in part 09 and gated by `byte_588BA8`.
3. **Trigger items (high).** 0x45A1C0 / 0x45B320: unlocks from the unit-type flag `+0xAC` (needs the battle map to set it: 0x47F630 / 0x488C00 / 0x492931). Until the battle map exists, provide a stub API. Note Human T6 and Martian T0/T3 are never set by this code; check part 09 and the events for another setter.
4. **Improvement multipliers (high, for the battle map).** The 17 `[Tweak]` floats with their defaults (§4), applied to the globals 0x4CA180–0x4CA1A4 and 0x4CA348–0x4CA360 when the matching enhancement is done, plus the Martian detection flags 0x589130/0x589134. Expose them for the combat code (0x4958B0 damage taken, 0x494AF0 weapon output, 0x493D39).
5. **Research screen UI (high, together with part 09).** The filing-cabinet screen: open/close sounds 0x19C/0x19D; plan folders that animate open at `[Research] Human/Martian Open Rate` (20/10) per 0xBC step (0x458D90, 0x45C840); folder click 0xBE with sounds 0x199/0x19A; page turn 0xBF with 0x19B; item select 0x84 (speech 0x132) to start research; tooltip 0x8114 "Makes Available:" listing dependants; refresh 0x80A3; layout origin and step (Human 32,30 / 2,22; Martian 4,10 / 0,32). Human has an extra empty last plan that is always open. Addresses 0x458150, 0x458380, 0x458D10, 0x458E50, 0x459200.
6. **AI research (medium).** 0x45A400 / 0x45B4E0 with the four plan lists (§2.7.3) and a random plan pick per session; fallback 0x458A60 (prefer available items that depend on a trigger, else a random start index). Called from the AI at 0x442F10.
7. **Research save/load (medium).** HRSH (0x45A190) and MRSH (0x45B2F0) chunks: version, tag, 3×u32 (+0x58, +0x5C, +0x60), 2×u8 (+0x6C, +0x6D), current item index or −1, plan count, then the plans' own data (part 09). `SaveInfo` only scans tags today. Also derive the multipliers after loading (UpdateTriggers).
8. **Save-file writer/reader parity (medium).** The serialiser semantics in §2.2: length-prefixed strings include the NUL (0x44EB00); blocks are u32 len + bytes; OpenWrite writes the version first; the `GAME` header is a 0x28 block. "New campaign" copies `Human.000` behind a fresh GAME header (0x44EBB0 used by 0x404660). If the name can't be read, the slot name falls back to the file path (0x44E7D0).
9. **Route finder fidelity (low to medium).** WoWEngine's `WarMap::FindRoute` is a Dijkstra. The original A* (0x44EFA0/0x44F120): h = Cost(node, goal), which is −1 (refused) except for neighbours of the goal; ties are LIFO (insertion before the first node with f ≥ new f); a node already in the open list whose g improves is **not re-sorted** (a quirk that can change the route in rare cases); give-up after 1000 accumulated successor steps (status 0); a 31-node arena; 10 children per node. The caller 0x424160 also requires a route of exactly 2 nodes when the target is the enemy's or no one's. Checking ties and the no-re-sort quirk against the engine's results is a cheap task.
10. **Busy icon (low).** Show the animated GType 0x0C wait sprite at (8,8) while saving, starting the war map, or loading the battle map (0x44E4B0/0x44E5B0/0x44E5C0). The engine loads too fast for it to matter much, but a 1:1 rebuild could show it for a frame.
11. **Blitter modes not yet copied (low to medium, needed by the war map arrows and the battle map).** The sprite blitter's **shade-map modes 4–7** (0x450E27/0x450EDA with 0x450E16; used by the war map arrows 0x49C9FA). The font/image blitter's transparent and translucent variants (0x450B38 table 0x4C06A6), if fonts are ever drawn translucent. The 16-bit 50% mask is 0xFBEF in 565 mode (not 0x7BEF), so check whether that leak is visible. Solid fill 0x450528. RLE pixel probe 0x450DA4 (already effectively used for CHI.SPR lookups).
12. **Battle map renderers (later, part of the battle-map milestone).** The PolyLib triangle rasteriser (0x456DD0, 48 span modes: affine textured + gouraud via `shadeTable[shade<<8|texel]`, flat, translucent 16-bit; 250 px height limit; y flipped against the clip height; 640×480 clip) for WOF models (0x49F1FD). The ISOLib patch renderer (0x457C60/0x4579A4 with spans 0x45702D…0x45774D: per-row light interpolation, a dither XOR and a height-based light-table bank) for IOB buildings (0x49DF26). Models.h already notes the IOB patch addresses.
13. **Movie scaling detail (low).** 0x44F5E0 interlaces only when the screen is at least twice the movie height, and otherwise draws contiguous lines; centring uses placement flags 4 and 8; skip-on-click depends on the setting byte `dword_586E64+2`. The engine's MovieScreen already centres and handles Escape; check the mouse-skip option.
