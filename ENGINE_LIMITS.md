# War of the Worlds (1998, Rage Software) — Engine Limits Reference
*Reverse-engineered from IDA Pro disassembly of the game executable.*

---

## WOF / IOB Format (3D Unit Models)

Both `.WOF` (standalone) and `.IOB` (VOX-archive-embedded) use the same binary
format and are loaded by the same `WOFLib` constructor (`sub_49E330`).

### Per-model limits

| Field | Storage | Hard limit | Evidence |
|-------|---------|-----------|----------|
| `piece_count` | `uint16` at header[0x00] | **32767** (signed) | `movsx ecx, word ptr [esi+64h]` — read as *signed* word, loop `cmp edx, ecx; jl` |
| `total_vert_count` | `uint32` at header[0x08] | **32767** (signed) | `movsx edx, word ptr [esi+66h]` — upper 16 bits silently discarded; treated as signed int16 |
| `total_face_count` | `uint32` at header[0x04] | **32767** (signed) | Same pattern; used in ScrnPts allocation formula |
| Texture atlas width | fixed stride | **256 pixels** | Hardcoded stride `256` throughout renderer and encoder |
| Texture atlas height | `(end_off - tex_off) / 256` | Unlimited (heap) | No bounds check; just a byte array |

### Per-piece limits

| Field | Storage | Hard limit | Evidence |
|-------|---------|-----------|----------|
| `vert_count` | `uint8` at piece[0x11] | **255** | Storage type; face vertex indices are also bytes |
| `face_count` | `uint8` at piece[0x12] | **255** | Storage type |
| `mat_id` in face | `uint8` at face[3] | **254** (0xFF = skip) | `cmp eax, 0FFh; jge skip_face` at `0x49F1A7` |
| BSP children list | `int32[]` in piece, `-1` terminated | **16** | `cmp [arg_0], 10h; jge bail` at `0x49EB2A` |
| Piece record stride | fixed | **97 bytes** (0x61) | `add edi, 61h` at `0x49E485` |
| UV coords in face | `uint8` (offset from mat origin) | **255 pixels** from material top-left | Storage type |

### Material table

| Field | Limit | Evidence |
|-------|-------|----------|
| Material entries | **254 max** (mat_id 0–253 valid) | mat_id=0xFF causes face skip; no upper bounds check on table read — engine does `mat_table[mat_id*4]` blindly |
| Material record size | **4 bytes** each | `(tex_off - mat_off) / 4` |

### Memory allocations (no hard cap — heap only)

| Buffer | Formula |
|--------|---------|
| WorldPts | `5 × piece_count + 12 × total_verts` bytes |
| ScrnPts  | `8 × piece_count + 6 × total_verts` bytes |

### Lighting / shading

| Item | Limit | Evidence |
|------|-------|----------|
| Shade group ID per vert | **255** clamped | `cmp eax, 0FFh` in `sub_49DF40` — brightness result clamped to 0xFF |
| Minimum ambient shade | **8** | Enforced floor: `cmp cl, 8; jnb skip; mov [eax], 8` at `0x49E0B0` |
| Brightness levels | **13** (indices 0–12) | `LoadShadeTables` (`sub_40B6C0`) dispatches on 13-case jump table; `cmp eax, 0Ch` at `0x40B857` |
| SHH shade files | **13** variants (`dat\cdsepia.%s` etc.) | 13 jump-table cases × 1 SHH file per case |

### Practical 256×256 target

For a model with 256 pieces of 255 verts and 255 faces each:
- `total_verts` = 256 × 255 = **65,280** — exceeds the **32,767** int16 limit!
- Safe maximum: **~128 pieces × 255 verts** = 32,640 total verts (just under 32,767)
- Or with fewer verts per piece: **256 pieces × 127 verts** = 32,512 total verts ✓
- WorldPts = 5×256 + 12×32,640 ≈ **393 KB** — fine
- ScrnPts  = 8×256 + 6×32,640 ≈ **198 KB** — fine

> **Key constraint discovered:** `total_vert_count` in the header is read with
> `movsx` (sign-extending word read), not `movzx`. Values above 32,767 wrap
> negative and will corrupt the ScrnPts/WorldPts allocation. Keep
> `total_verts` ≤ **32,767**.

---

## Archive Format (.WoW files)

Used for `SFX\SFX.WoW`, `VOX\HUMAN.WoW`, `VOX\MARTIAN.WoW`.

| Field | Value / Limit | Evidence |
|-------|--------------|----------|
| Magic | `LxfS` (0x4C786653) | Checked at load; rejects if wrong |
| Header size | **8 bytes** | `push 8; ReadFile` at `0x4A3962` |
| Entry count | **uint32** (~4 billion) | Stored as `uint32`; `entry_count × 16` bytes allocated on heap; no cap |
| Entry record size | **16 bytes** | `shl eax, 4` (×16) for both alloc and ReadFile |

---

## Terrain Format (ATM / CLS)

Loaded by `sub_47B900`.

| Field | Limit | Evidence |
|-------|-------|----------|
| Tile type ID | Limited by name-table size | `[ebp+40h] * 64` indexes into `unk_4EAEC8` (64 bytes/entry); table is static — size TBD from data section |
| Tile name length | **64 bytes** (including null) | `shl esi, 6` stride at `0x47B965` |
| Map grid width × height | Effectively unlimited (uint32) | No `cmp` found on dimensions; allocated on heap via `imul` |
| String sentinel | `0xFF` = end-of-string marker | `cmp al, 0FFh` at `0x47BA59`, `0x47BAEF`, `0x47BD49` |

---

## Sprite / Minimap Format (.SPR)

Used for `maps\hmin%02d.spr` and `maps\mmin%02d.spr` (minimap sprites).

| Field | Limit | Evidence |
|-------|-------|----------|
| String/name sentinel | `0xFF` | `cmp al, 0FFh` at `0x41747A` |
| End-of-data marker | `0xFE` | `cmp byte ptr [eax-1], 0FEh` at `0x4174FC` |
| Dimensions | No explicit CMP found | Loaded via `sub_4175B0`; dimensions appear to be read as `uint32` fields with no bounds check |
| Map index format | `maps\hmin%02d.spr` / `maps\mmin%02d.spr` | `%02d` format → indices 0–99; `and eax, 0FFFFh` before sprintf |

---

## Billboard / Screen-space Renderer (sub_49F890)

Used for rendering WOF units as sprites on the battle map.

| Item | Limit | Evidence |
|------|-------|----------|
| Sprite UV / tile coord | **63** (0x3F) clamped | Three `cmp x, 3Fh; jle; mov x, 3Fh` at `0x49F972/983/992/9A3` — all four UV components clamped |
| Animation frame dispatch | 4 explicit cases: **0, 64, 128, 192** | Jump table at `0x49FB67`; default catches all non-multiples-of-64 |
| Distance/LOD scale | 100,000 (0x186A0) | `mov [var], 186A0h` used as initial distance value |

---

## SHH Shade Table Format

Pre-baked normal-direction lighting lookup tables, loaded by `LoadShadeTables`
(`sub_40B6C0`). One SHH file per brightness level / light direction.

| Item | Limit | Evidence |
|------|-------|----------|
| Shade table variants | **13** (0–12) | 13-case jump table; `lea eax, [ebx-2EDh]; cmp eax, 0Ch` at `0x40B857` |
| File naming | `dat\cdsepia.%s` | First case = `cdsepia`; other cases follow similar pattern |
| Entries per table | **256** | One entry per palette index (uint8 palette) |

---

## Key Takeaways for Maximum-Detail Models

1. **Piece count:** up to **32,767** theoretically; limited in practice by WorldPts/ScrnPts memory and BSP traversal depth (16 recursive stack frames).
2. **Verts per piece:** **255 max** (uint8 field).
3. **Faces per piece:** **255 max** (uint8 field).
4. **Total verts across whole model: ≤ 32,767** — this is the critical constraint. The field is stored as uint32 but read as signed int16 (`movsx`). Exceeding 32,767 causes a negative allocation and crash.
5. **Material IDs: 0–253** valid (254 also works; 255 skips the face entirely).
6. **BSP children: 16 per piece** — a flat list (no nesting) of up to 16 pieces.
7. **Brightness: 13 levels** (0–12).
8. **Texture atlas: 256 wide × unlimited tall** (but keep it reasonable — it's a flat raw byte array).

