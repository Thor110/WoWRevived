# Review: routines 0x410000–0x41FFFF (294 routines)

**Counts:** PORTED 116 · PARTIAL 16 · MISSING 9 · LIBRARY 153 (total 294)

Paths below are relative to `/home/user/WoWRevived/WoWEngine/src`. IDA = `scratchpad/ida/IDA-Output.txt`.
Note: a few doc tables in `docs/survey/part03.md` are slightly off, and the port is right in those cases. For example,
0x41EDC0 picks the Construction view when 0x51A100 == 2, which is the opposite of what the doc row says, and the port does the same as the binary.

## Missing

### Battle map rendering (ISOLib point lights)
- **0x4111B0** `ISO_LightVertices`: adds a point light's contribution, `0x40C930(light, vertex) × scale`, to each vertex or face level of an IOB object. **0x410E00** reads the `[Game] "ISO Brightness"` setting (float, default 0.5, at 0x4E8C04). 0x4113F0 multiplies each light's intensity by it (0x4114BB `fmul flt_4E8C04`).
  The port never does this: `core/IsoObject.h:4` says "(0x4111B0, not in yet)", and nothing reads "ISO Brightness".
  *Task:* add point lights (fires, vehicle lights) to IOB object shading. Use at most `dword_4B8E28` = 2 lights per object (see the 0x4113F0 notes below) and scale each by ISO Brightness.

### Front end – CD player
- **0x413740 / 0x413990** `SeekBack` / `SeekForward`: move the position by a TMSF delta (`0x500` = 5 s per auto-repeat step) with second/minute borrow, clamped to the track. These are CD Player transports 4 and 6.
  The transport enum in `frontend/CdPlayer.cpp:12` has no 4 or 6. This is already listed in TASKS.md (CD player seek).
  *Task:* add the ±5 s seek buttons and their auto-repeat to `CdPlayer` (Music::Seek).

### Files / formats – FFUH encoder
- **0x415520** `Compress`, **0x4157E0** `Encode` (LSB-first bit packing) and **0x415720** `WriteHeader` (csize + 0x410 header and frequency table). `core/Ffuh.h` only decompresses.
  The encoder is needed only for writing a sector's damage map (DMGL, 0x404F50/0x405100/0x4050B0). The port keeps DMGL as raw bytes copied from the original save, and the persistent scorch map is not ported (that code is outside this range).
  *Task:* add `ffuh::Compress` once the battle-map damage map (0x404F50–0x4051B0) is ported.

### War map – view gliding
- **0x41B040** (start or stop the zoomer at world+0x124) and **0x41DFC0** (`[Tweak] "Warmap Zoomer Speed"` = 1.5f): 0x41E000 with `animate=1` makes `Zoomer 0x446690(view, x, y, 1.5)`, which glides the view to the sector. Event boxes (0x41E4E0 → `0x41E000(sector, 1)` at 0x41E575) always ask for the animated form.
  The port jumps instead: `warmap/WarMap.cpp:1889` says "the game glides; the engine jumps".
  *Task:* add an animated CentreOn that uses Warmap Zoomer Speed, and use it from OpenBox.

## Partial / divergent

| Addr | What differs | Port | Task |
|---|---|---|---|
| 0x4113F0 | It picks the nearest lights within `light+0x3E + objRadius`, keeping their distances in increasing order, up to `dword_4B8E28` (=2). For each it calls 0x4111B0 with `intensity(+0x48) × ISO Brightness`, then draws (0x457C60). The port does only the draw. | `core/IsoObject.h:1-5` | See Missing / 0x4111B0. |
| 0x413220 / 0x4134B0 | Next and Prev **wrap**: 0x4132E2 sets `if (cur >= count) next = 1`. The caller, 0x4081D0 (0x4083BF / 0x408351), then selects track 2 (Next) or the last track (Prev) and **stops** play (`+0xC4 = 0`). The port **clamps** to 2..5 and keeps playing. | `frontend/CdPlayer.cpp:73,101-102` | Wrap at the ends (Next past 5 → 2, Prev before 2 → 5) and stop play when it wraps. |
| 0x412690 | CD enable/disable also **saves and restores the play position** ("Saving position %d" / "Resuming position at %d", reset by 0x412940). The port's music restarts from the beginning of the track. | `frontend/FrontEnd.cpp` (music) | Optional: remember the war-map music position across pause/resume. |
| 0x416250 (via 0x4161E0/0x416230) | Word wrap. (a) It breaks when the running width is **≥** the width (0x4162DB `cmp ebp,[width]; jnb`); the port breaks on `>`. (b) After a break the running width is reset to **0** (0x416321), not to the width of the word being carried over, so the next line can run a word too long. (c) Runs of several spaces are kept, and trailing blanks are trimmed (0x4162FA); the port collapses spaces between words. (d) The width is the sum of per-character widths (0x40FB60), not `Font::Measure` of the whole line. | `frontend/Canvas.cpp:117-135` | Port 0x416250 literally to get the same line breaks (briefing, CD credits, popups). |
| 0x41AED0 | The **`byte_4E9B94` (forfeit) branch is missing**. When it is set (by the battle map at 0x48ACEF, in sector 3 or 30 when no battle is on), the original raises event 19 (EndGameLost) with param 30 for Human or 3 for Martian, clears the flag and returns (0x41AED0–0x41AF06). The port only checks winner/loser. | `warmap/WarMap.cpp:1549-1566` | Carry `forfeit` from the battle result into the war map and raise EndGameLost. |
| 0x41F4B0 | With `byte_4E9B94` set, the lParam is **0x80FB (Human) / 0x80FD (Martian)** (0x41F4CB–0x41F4D6), not `EndCode()`. The port always uses EndCode, which returns 0 when `gameOver` is false. With forfeit, the ending FMV code would be lost. | `warmap/WarMap.cpp:1855-1860` | Use 0x80FB/0x80FD when the game was forfeited. |
| 0x41F400 | The delay before 0x80FE is the **victory sound's length (`sound+0x30`) + 30** (0x41F472/0x41F48D). The port assumes 150 + 30. | `warmap/WarMap.cpp:1853` | Use the real length of sound 0x13E from the mixer. |
| 0x41D9B0 | See **Bugs** (enemy sector level in the Construction view). **Also**, for **border colours** each side's owner is taken as "no one" (2) when that sector has no finished command building (0x41DC1A–0x41DC47 call 0x43C990). The port uses the raw owner. | `warmap/WarMap.cpp:1018-1022` | Treat a sector without `HasCommandPost` as owner 2 when colouring borders. |
| 0x41DE70 | Routes are drawn and battle sectors pulse **only in Unit view**: 0x41D9B0 calls 0x41DE70 only when `world+0x70 == 1` (0x41DD35 `sub_42A430` returns +0x70, then `dec eax; jz`). The port's `ShadeSectors` always calls `PulseBattles()`, so battle sectors also pulse in the Construction and Resource views. | `warmap/WarMap.cpp:456` | Call PulseBattles only when `viewMode == 1`. |
| 0x41E000 / 0x41E4E0 | Centring is not animated (see Missing / zoomer). 0x41E4E0 also calls the view's vtbl+0x38 on the sector first (0x41E563); that part is unclear. | `warmap/WarMap.cpp:1299-1305, 1888-1897` | See above. |
| 0x41C2E0 | In mode 3 (Resource view), Activate builds 3 `ROHS` (0x426840, produced/used bars) and 3 `RFHS` (0x426140, facility stacks with tooltips) hot spots per sector into 0x4E9BF8[180]. Nothing in the port matches (no RFHS/ROHS). | none (`warmap/WarMap.cpp`) | Port the Resource-view gauges and hot spots (0x426140/0x426840; survey part04 §26/117). |
| 0x41FDD0 | Route-list Load reads each route **with its progress** (`SaveRoute::progress`, +0x34, path, +0x5E attack flag) and then syncs arrivals (0x41FCF0). The port puts loaded travellers **back at their origin and sends them again**. Progress is lost and the attack flag is not passed to `SendTo`. | `warmap/WarMap.cpp:729-739` | Restore `hoursLeft`/segment from `progress` and the attack flag from `flag5E`. |
| 0x41DF40 | `[Screen] "Scroll Speed"` (default 40) is read, but how it is used (in 0x422C70/0x4096D0) is guessed: `scrollSpeed/5`, marked *provisional*. `"Scroll Border"` (0x41DF80, default 4) is a constant in the port instead of a setting. | `warmap/WarMap.cpp:24,107-108` | Trace the use of 0x4BA058/0x4BA05C in 0x422C70 and read Scroll Border from the INI. |
| 0x41B070 | When the CD reports a non-zero status (0x413EB0), the original calls **0x412960** (it takes the CD over from SJE_CdPlayerClass) and re-polls in 10 s. It does **not** stop the playback. The port stops whatever music is playing. Status: unclear. 0x413EB0's return value is a packed MCI value, and the exact meaning was not traced. | `frontend/FrontEnd.cpp:1176` | Check what 0x413EB0 returns; don't stop CD-player music that the player started. |

## Bugs

1. **Enemy sector shade in the Construction view.** In the original (0x41DAB6–0x41DAEF), a sector that is not ours, when the view is not Resource:
   - owner 2 (no one) → 0xB;
   - otherwise (enemy) → `neg/sbb/and 0Bh` on the view index `var_1C`, which gives **11 when the index ≠ 0 (Construction view) and 0 only in Unit view**.

   The port (`warmap/WarMap.cpp:450-453`) sets `level = 0` for an enemy sector in **both** Unit and Construction views. Its own comment at lines 425-427 describes the original behaviour correctly.
   *Fix:* `else level = viewMode == 1 ? 0 : warmap::NormalLevel;`.
2. **Battle pulse outside Unit view** (0x41DD35–0x41DD43 against `WarMap.cpp:456`). See Partial. The port pulses battle sectors in the Construction and Resource views, and the original never does.
3. **CD Next/Prev clamp instead of wrap** (0x4132E2–0x4132E9 and the caller at 0x4083C2/0x4083E4 against `frontend/CdPlayer.cpp:73`).
4. **Forfeit result not handled** (0x41AED0 at 0x41AED8 and 0x41F4B0 at 0x41F4C9 against `WarMap.cpp:1549`/`1855`): `byte_4E9B94` is never consulted on the war map side.
5. **Word-wrap comparison** (0x4162E2 `jnb` = ≥ against `Canvas.cpp:122` `> width`). It is off by one at the limit and can move line breaks in game text.

Checked and **matching**:
- pods 0x41A330: the `< 8` cap, `hours -= interval`, sectors {2,4,6,7,8,9,10,11}, loads {62},{62,89},{62,91},{62,89}, the rand order, the +0x64 flag against event 12, Pod Interval 24;
- resource warnings at 0x419EFB: 0.3/0.8, >336 / >504, resets, param `sector<<16|k`;
- economy 0x41A760: all five phases, `(count·X − (−1))`, the buffer added to supply only, `max(1,(D−S)/S+1)`, cheat 0x58913C, min/max;
- shade tables 0x4192F0;
- battle detection 0x41AB00;
- result sounds 0x13C/0x13D/0x13F/0x140 and event params 3/30;
- event queue 0x41E5B0/0x41E730/0x41E7D0;
- keys 0x41D640 (h/16, h/8, w/7, w/3);
- date advance 0x41B770 (leap rule);
- duration 0x41BDA0;
- the music step's ambient branch (0xFD+rand%3, fade 60, 10 s).

## Library / not needed (153)

- **PolyLib/ISOLib cdecl thunks** (18): 0x410010–0x410070, 0x410E40–0x410F60 (setters, getters, clip, buffers); 0x411170 ISOLib::FreeMem.
- **Renderer damage map / dirty-rect splitter** (8), replaced by full-frame drawing: 0x410230, 0x410300, 0x410340, 0x410370, 0x410420, 0x410460, 0x410490, 0x4106C0.
- **Registry/INI settings classes** (35): 0x411540–0x4122B0 (Bool/Int/Path/RegVar types 0–6, Load 0x4119A0, Store 0x411CC0, chdir 0x411F20, WithSlash 0x412090) and 0x416500–0x416D10 (Float/String, Use INI back end). They are replaced by `frontend/Settings` + IniStore.
- **DirectX/DInput loader** 0x4122F0.
- **CD audio via MCI** (19), replaced by WAV music files: 0x412660–0x412680, 0x412940, 0x412960, 0x412B50, 0x412C00, 0x412DC0, 0x412F10, 0x412FD0, 0x413C70–0x413F70 (track count/current/length/status/eject), 0x414010, 0x4140F0, 0x414150, 0x414180 (play thread).
- **MEM_\* pool allocator and statistics** (12): 0x414780, 0x4148A0, 0x414BC0, 0x414CE0, 0x415410, 0x416F50–0x417090; debug setting statics 0x414740 and 0x414DA0; WText dtor 0x414680.
- **FFUH plumbing** (7): ctor 0x415470, AttachHeader 0x415780, streamed refill with busy icon 0x415C00, std::list helpers 0x416110/0x416180/0x4161B0, 0x4161D0.
- **Rect helpers and fatal error** (5): 0x416380, 0x4163A0, 0x4163C0, 0x416400, 0x4164A0.
- **WPath std::string helpers** (8): 0x4170E0–0x417A80. **Debug log** 0x417BF0/0x417C30 (already in TASKS.md, optional).
- **War-map object plumbing** (35):
  - broadcasts 0x4188D0, 0x41A3F0;
  - getters 0x4191F0–0x419280;
  - selection objects 0x419750–0x4197F0;
  - DateTime ctor/copy/dtor 0x41B3A0/0x41B3F0/0x41B430;
  - statics 0x41BFD0/0x41BFF0, 0x41E660/0x41E6A0, 0x41F510/0x41F530;
  - WarMap view window ctor/dtor/surface/deactivate 0x41C010/0x41C140/0x41C1F0/0x41C530;
  - 0x41E140 forwarder, 0x41E4D0 nullsub;
  - calendar dtor/activate/deactivate 0x41F5C0/0x41F7A0/0x41F820, 0x41F6B0;
  - route-list ctor/dtor/activate/deactivate/arrow-activate 0x41F990/0x41FA20/0x41FB40/0x41FB90/0x41FF20.

## Ported

- 0x410090, 0x410190 → `frontend/Canvas.h` (textured quad/triangle)
- 0x410F80 → `core/TerrainView.h` (DithDat)
- 0x414410 → `core/TextOjd`; 0x4146F0 → TextTable::Get; 0x414700 → `warmap/WarMap.cpp:1761`, `frontend/ResearchScreen.cpp:205` (speech by text id; all in-range callers covered)
- 0x414DE0, 0x415030, 0x415120, 0x415200 → `core/Vfs`, `core/Archive`
- 0x4155B0, 0x415640, 0x4158E0, 0x415A80, 0x415CA0, 0x415D20 → `core/Ffuh.cpp` (decoding)
- 0x416230 → `Canvas::Wrap` (with the 0x416250 caveats)
- 0x417CD0, 0x418220, 0x418500, 0x4185F0 → `warmap/WarMap.cpp` ctor / `frontend/FrontEnd.cpp:538, 1197` / `warmap/WarMapAi.cpp`
- 0x418890, 0x4192A0 → `WarMap::BuildSectorPaths` (WarMap.cpp:307); 0x418930 → rubber band; 0x418980 → WarMap.cpp:112/1522; 0x419030 → deactivate; 0x4191A0 → `Log`
- 0x4192F0 → `ShadeSectors` (WarMap.cpp:421); 0x419500/0x419520 → `BuildPreview` (WarMap.cpp:2176); 0x419810 → `SetView`
- 0x4198C0 → WarMap/FrontEnd command handling (0x419A4D, 0x419AA2, 0x419E9D `Hourly`, 0x419EFB `ResourceWarnings`, 0x41A082–0x41A17A)
- 0x41A2F0, 0x41A330 → `DropPod` (WarMap.cpp:557); 0x41A450 → keys (WarMap.cpp:1307-1330); 0x41A6C0 → `SetTimeRunning`; 0x41A730/40/50 → `SetView`
- 0x41A760 → `EconomyPhase` (WarMap.cpp:350); 0x41AB00 → `DetectBattles` (WarMap.cpp:1598); 0x41AC40/0x41AD80 → load (WarMap.cpp:701) / `SaveData` (744); 0x41AEA0 → `SpeedChangeAllowed`
- 0x41B210, 0x41B360 → `frontend/FrontEnd.cpp:1124, 1197`; 0x41B440 → `frontend/Briefing.cpp`; 0x41B770, 0x41B9A0, 0x41BF90 → `warmap/WarMapData.h:378-428`; 0x41B8D0/0x41B930 → `core/SaveFile`; 0x41BDA0 → `Research::Duration`
- 0x41C5A0 (Quick Cancel Timeout), 0x41C5E0, 0x41D120, 0x41D300, 0x41D470, 0x41D590, 0x41D640 → `warmap/WarMap.cpp` (mouse, menu, keys); 0x41CD50 → `frontend/FrontEnd.cpp:727`; 0x41DF80 (Scroll Border constant)
- 0x41E160–0x41E3E0 ([Event] options) → `frontend/FrontEnd.cpp:70`; 0x41E420–0x41E4C0 → `WarMap::Critical`
- 0x41E4E0 → `OpenBox`; 0x41E5B0/0x41E730/0x41E7D0 → `RaiseEvent`/`NextEvent`
- 0x41E850, 0x41E960, 0x41EAE0, 0x41EB90, 0x41EC30, 0x41ECD0, 0x41EDC0, 0x41EEA0, 0x41EF10, 0x41EF80, 0x41F010, 0x41F1C0, 0x41F220, 0x41F280, 0x41F340 → `WarMap::RunEvent` (WarMap.cpp:1713-1880)
- 0x41F550, 0x41F640, 0x41F6C0, 0x41F720, 0x41F760 → clock (`ClockTick`, `SetSpeed`, SaveFile CALN/DtTm); 0x41F860 → `frontend/FrontEnd.cpp:1148`
- 0x41FBD0, 0x41FCA0, 0x41FCF0 (`ArriveTogether`), 0x41FEC0, 0x41FF70, 0x41FFB0, 0x41FFF0 → counters/routes in `warmap/WarMap.cpp`
