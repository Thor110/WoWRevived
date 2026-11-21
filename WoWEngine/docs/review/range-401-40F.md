# Review 0x401000-0x40FFFF (WinMain, WGame, front-end screens, shade tables, lighting, renderer, RType, text)

**307 routines: PORTED 100 · PARTIAL 31 · MISSING 14 · LIBRARY 162.**
Some routines appear in two sections: each one is counted once, under its main class. For example, 0x401000 is
counted as PARTIAL and the missing cheat handlers it dispatches to are counted as MISSING.

Port paths are relative to `WoWEngine/src`. "dis" means the disassembly in IDA-Output.txt. The routine notes in
`docs/survey/part01.md` / `part02.md` were spot-checked against the disassembly and hold, except where noted.

## Missing

### Cheats (front end / war map / battle map)
- **0x4011F0, 0x401320, 0x4013A0, 0x401440, 0x4014F0, and the inline cases of 0x401000** (s → `shym`, d → `datsun`):
  the handlers for the other seven typed codes:
  - `youlikeit`: byte_58913C = 1. The port reads that flag as a constant: `warmap/WarMap.cpp:350`, `warmap/Production.cpp:129`.
  - `shym`: byte_4D25A0, which clears the damage layer at 0x405039.
  - `datsun`: byte_4BAD38 = 0, which turns the fog of war off (read at 0x434744).
  - `curtains`: byte_4D1279, the [Debug] Mat flag.
  - `icomeback`: calls 0x458460 on dword_4E9B88.
  - `punyhumans`: Send(0x111, 0x806C) to dword_4D2550, only when dword_4E9B70 != 0.
  - `atchooo`: the same with 0x806D.

  Typing is ignored in network games (byte_4D256C at 0x401000).
  Task: extend `FrontEnd::OnKey` (`frontend/FrontEnd.cpp:1262`) to the 9-state recogniser and wire each flag. The
  research one already exists as `Research` (0x458460, `core/Research.h:136`).
- **0x4011D0** ResetCheats: clears byte_4BAD38 (to 1), byte_4D1138, byte_58913C, byte_4D25A4 and byte_4D25A0. It is
  called from the WWarMapWorld constructor 0x417CD0 and from war map load 0x418500. The port never resets
  `creditsEasterEgg` (`frontend/FrontEnd.h:165`), so `englishrose` typed in the menus survives into a campaign.
  Task: reset the cheat flags in `StartWarMap`.

### Battle map: Damage Layer (scorch carried between battles)
- **0x404EA0 / 0x404F50 / 0x405020 / 0x4050B0 / 0x405100 / 0x4051B0** "Damage Layer" (vtable 0x4A92E8). Each
  sector has a w×h byte grid, saved as the `DMGL` chunk (0x405100: tag, w, h, packed size, FFUH data). On every
  battle-map entry (0x439770):
  - if the grid is empty it is created at the landscape size (0x439883 → 0x404F50);
  - it is decayed by `1000 / [BattleMap] "Damage reduction divisor"(500)` = **2 per byte, clamped at 0**
    (0x439892 push 3E8h → 0x405068 idiv dword_4D259C);
  - with `shym`, it is first zeroed once (0x405040).

  On leaving the battle map it is repacked (0x439FE2 → 0x4050B0).
  The port keeps scorch only inside `Landscape::scorch` (`core/Landscape.h:117-120`), which is rebuilt empty for
  each battle (`core/Landscape.cpp:96`). The sector's DMGL bytes are carried untouched in the save
  (`core/SaveFile.h:81`). So scorch from an earlier battle never shows, and new scorch is never saved.
  Task: load the sector's DMGL into the landscape scorch on entry, decay it by 2, and write it back to the sector's
  DMGL on leaving.

### Cursors
- **0x40CDC0** Cursor::Animate: each tick it advances the cursor sprite (0x40EF20(0x80, 1)) and marks it dirty, so
  cursors such as the wait cursor animate. The port always draws frame 0 (`frontend/FrontEnd.cpp:1253`).
  Task: keep a per-cursor 8.8 frame counter stepped by 0x80 every tick.
- **0x40E680 / 0x40E6E0** WaitCursor guard: saves the cursor, sets cursor 0xD (GType 0xC) for the duration of a long
  operation, then restores it. Used by SaveGame 0x4042D0, the disc check 0x408B70 and war map load 0x418500.
  Nothing in the port does this. Task: show cursor 0xD while saving or loading. Low priority, because the port's
  loads finish within a frame.

## Partial / divergent

- **0x401000** Cheat recogniser. Only `englishrose` is implemented (`frontend/FrontEnd.cpp:1264-1274`). On a wrong
  letter the original sets state = 0 (0x401163 / 0x4012E4) and **does not re-test the letter as a first letter**.
  The port does `letter == Code[0] ? 1 : 0`, so it also accepts input like "eenglishrose". Task: copy the
  original's state machine (see Missing).
- **0x401280** `englishrose`: the port has the code but not the 0x4011D0 reset (see above).
- **0x402B80** WGame::OnMessage:
  - The **0x79** handler (shut down networking and the world, clear the views, then go to the main menu; sent from
    0x43E4EA and 0x45A7D3) is missing. It falls to `NotRebuilt` (`frontend/FrontEnd.cpp:927`).
  - WM_COMMAND ids are deferred one queue pass through 0x95. Not done; already a TASKS item.
  - Quit (0x70) fades every sound over 15 ticks (0x402CA6); the port quits at once. Harmless.

  Task: add 0x79 → `warMap.reset()` and the main menu.
- **0x403680** WGame::Tick. The frame order is ported: pre-tick, tick, queue drain. The player objects' per-tick
  calls are absent: opponent vtbl+0x24 (think), +0x28(player), player +0x2C, opponent +0x2C. The port runs the AI
  hourly from the war map. This depends on the missing Players / command-queue task (TASKS Milestone 2).
- **0x4052B0 / 0x4053E0 / 0x4056F0** Credits:
  - (a) **Hold length** (see Bugs).
  - (b) In the original, the music-finished check (0x40574C) comes *before* the `englishrose` test (0x40576D), so the
    easter-egg version also waits for the music. The port skips the wait when the egg is on
    (`frontend/Credits.cpp:31`).
  - (c) The destructor fades the music over 60 ticks (0x405415 push 3Ch → 0x464EB0). The port `Stop`s it at once
    (`frontend/Credits.cpp:18`).
  - (d) The exit fades. The credits send their exit WM_COMMAND to themselves, and 0x4055C0 passes it to 0x4065E0
    (0x405689), which starts the fade-out and the 5-tick delay. The port dispatches the credits' commands without a
    fade (`frontend/FrontEnd.cpp:1007`, `fromControl = screen->Id() != Credits::Id`).
- **0x405DE0** Splash::OnMessage: a **right-button** press (WM_RBUTTONDOWN, 0x204 at 0x405E6E) skips the splash as
  well as the left button. `SplashScreen` only overrides `OnMouseButton` (`frontend/Splash.h:24`), and the default
  `Screen::OnRightButton` only presses push buttons. Credits has the same gap (0x405672). Task: override
  `OnRightButton` in both.
- **0x40A1B0** Menu::OnMessage, Escape. Escape sends (0x111, 0x8123) to the menu. 0x40A253 then sends
  (0x111, prevCmd) to itself, and that is forwarded to the owner Fullscreen View (`[esi+3Ch]`, 0x40A245). There
  0x4065E0 starts the 50%/black fade and the 5-tick delay like any click. The port sends Escape-back straight on
  with no fade (`frontend/FrontEnd.cpp:1278`, `Dispatch(*command, key != SDLK_ESCAPE)`). Task: fade Escape-back on
  the front-end menus too. The war map and battle map menus are a separate case.
- **0x4081D0** CD Player handler: transport cases 4 and 6 (seek ±0x500 with a "Repeat %d" count) are missing.
  Already a TASKS item (`frontend/CdPlayer.cpp`).
- **0x4089D0 / 0x408AB0** Insert CD: the 0x8134 recheck every 15 ticks and the ejection of the wrong disc
  (0x408B70 → 0x413F70) are not done. By design the disc check always passes (`frontend/FrontEnd.cpp:362-368`).
  No task unless real discs are supported.
- **0x404850** Load menu with network flag (0xA2): the port only shows the menu; network loads come with
  networking.
- **0x406A10, 0x406B30, 0x406C10, 0x406E20, 0x406F00, 0x407110, 0x407200, 0x407260, 0x4073D0** Network menus.
  These are static stand-ins (`frontend/FrontEnd.cpp:404-447`, and 0xA5 / 0xAB go to `NotRebuilt` at
  `FrontEnd.cpp:818-822`). Missing: the DirectPlay service-provider list (≤10 entries), the session poll every 30
  ticks (timer 0x812A), and join → 0xA5. Already a TASKS item (Network game).
- **0x4096D0 / 0x409AD0** Display Settings. The port deliberately offers its own mode list
  (`frontend/FrontEnd.cpp:63-67`, 290-312) and still draws at 640×480. The original offers 512×384…1280×1024, only
  the modes flagged available in 0x4B8C88, as commands 0x8115, 0x810B, 0x810C, 0x8108, 0x8109, 0x810A. Already a
  TASKS item.
- **0x40B6C0** LoadShadeTables:
  - the 0x2F9 slot list is wrong (see Bugs);
  - the original frees all 33 slots and the LANDR table before loading, but `Resources::LoadShadeGroup`
    (`frontend/Resources.cpp:118-140`) only overwrites the group's own slots, so slots from the previous group stay
    in use;
  - the `ShadeGroups.h` header comment still says bm "builds names from unit data". The hour-letter tables are in
    fact loaded in `battlemap/BattleMap.cpp:55-60`.

  Task: clear every slot on a group change, and fix the 0x2F9 entry.
- **0x40CB60 / 0x40CD30** Cursor object. The cursor table is ported (`frontend/FrontEnd.cpp:1244-1253`). Not done:
  the 64×64 clamp, the dirty flag, and animation (see Missing 0x40CDC0).
- **0x40CB10** IntensityLookup: `INT[clamp(f*256, 0, 255)]`, called by RTypeLight 0x4A2980, which stores the result at
  light+0x48. The port has no equivalent. Unclear whether it matters: 0x4A0140 reads light+0x40 (the raw intensity),
  not +0x48, so the terrain lighting is unaffected. Who reads +0x48 was not traced.
- **0x40F3A0** `[Colours] "Message Text"` (19, word_4B9420). It is read by the war map message items 0x428810 and
  0x428940 (`SetShadeLevel(word_4B9420)` at 0x4288A3 / 0x4289A1). The port draws the message list at level 0 (see
  Bugs). "Messagebox Detail" (21) and "Messagebox Command" (22) are hard-coded in `warmap/WarMap.cpp:1919-1921`
  rather than read from the INI. That is harmless unless a user overrides them.
- **0x40FEA0** PolyLib::Setup loads `dat\div_tab.dat` and `dat\b16.hsh` (the 16-bit blend / shade hash). The port
  instead averages in 24-bit (`frontend/Canvas.cpp:52`, `(a>>1 & 0x7F7F7F) + (b>>1 & 0x7F7F7F)`). Its 50%
  translucency may differ slightly from the hash table's. Task (low): compare a translucent sprite pixel against
  b16.hsh.
- **0x4057F0 / 0x405A70** Sound Control: ported (TASKS: live preview). Not checked: the CD checkbox applying 2 ticks
  later (0x812D → 0x412690). Treat as unverified.

## Bugs

1. **Credits easter-egg hold is twice as long.** In the original, 0x40577C pushes 12Ch, the exit is
   Post(0x111, exit, delay 300), and no step timer runs during the wait, so the hold is **300 game ticks**.
   In the port, `frontend/Credits.cpp:24` returns on odd ticks *before* the hold countdown, and
   `Credits.cpp:27-28, 36` count `holdTicks = 300` down once every two ticks, so the hold is **600 ticks**.
   Fix: decrement `holdTicks` every tick, before the `ticks % 2` gate.
2. **Palette group 0x2F9 (the CD Player) is missing two shade slots.** After cd1…cd10 the original also loads
   `dat\%sR%sI` into slot 5 (0x40BC4E string, stored at 0x40BCB3 `mov dword_4DA794, esi`) and `dat\%sRGI` into slot 9
   (0x40BCAD, the shared tail at 0x40C54F `mov dword_4DA7A4, esi`). `core/ShadeGroups.h` lists 0x2F9 as cdsepia and
   cd1-cd10 only (slots 22-32). Together with the non-clearing loader (Partial 0x40B6C0), the CD Player's interface
   and text use the F6 / F7 menu tables that were left loaded, not HRHI / HRGI (or MRMI / MRGI). Fix: add
   `{S}R{S}I`→5 and `{S}RGI`→9 to the 0x2F9 entry.
3. **The war map message list draws at shade level 0.** In the original, the message items set level
   `word_4B9420` = 0x13 = 19 (`[Colours] "Message Text"`, declared at 0x40F3A0; used at 0x4288A3 and 0x4289A1).
   In the port, `warmap/WarMap.cpp:1117` calls `canvas.DrawString(0x72, line.text, {...}, 0)`, so the level
   defaults to 0 (`frontend/Canvas.h:57`). Fix: pass `ini "Colours"/"Message Text"` with a default of 19. Also
   check the flags: the original calls `SetFlags(1)` (0x40F9F0) where the port passes 0.
4. **Escape in front-end menus skips the fade** (detailed under Partial 0x40A1B0): the original's path is
   0x40A1ED → 0x40A272 → 0x40A245 → 0x4065E0; the port's is `frontend/FrontEnd.cpp:1278`.
5. **The credits' exit skips the fade** (Partial 0x4055C0 / 0x4056F0 (d)): the original calls 0x405689 → 0x4065E0;
   the port uses `frontend/FrontEnd.cpp:1007`.

## Library / not needed (162)

- **Static setting objects (atexit initialisers), 55.** Examples:
  - 0x4015C0–0x401900 ([Debug] Dave / Mat / Edit functionality, I Am Human, Game Frequency, Thread / Timer /
    Sleeper Enable, Game max / min time);
  - 0x4022C0–0x4025B0 (memory pools, [Options] Show lights…, Messages Guaranteed, Skip, Save File ID);
  - 0x402AB0 / 0x402AF0 [Stats], 0x404E60 Damage reduction divisor, 0x40AFF0 Time Of Day,
    0x40CE00–0x40CE80 [Screen];
  - 0x40F060–0x40F5E0 [Colours] and [Tweak] Text Line Spacing.

  Their values are read where they are used. "Skip" is a root registry value in the original
  (0x402580 → 0x411660 with section NULL) and `[Main] Skip` in the port (`frontend/FrontEnd.cpp:98`), a reasonable
  mapping.
- **Threads, timer, message loop and quit, 9:** 0x401FF0, 0x402050, 0x402080, 0x4020D0, 0x402180, 0x402230,
  0x403890, 0x403920, 0x404E10. These are replaced by the 30 Hz SDL loop in `main.cpp`.
- **WRenderer, dirty rects and frame list, 25:** 0x40CF00–0x40E650 and 0x40AEB0 / 0x40AED0 / 0x40AF20. The port
  redraws everything each frame. View clamping 0x40E5A0 is ported in `battlemap/BattleMap.cpp:991`.
- **PolyLib asm stubs, 8:** 0x40FD30–0x40FFC0, which the port replaces with `core/Sprite`, `core/Font` and `Canvas`.
- **RType pool and trivial RType methods, 14:** 0x40E700 / 0x40E780, 0x40EAE0, 0x40EB40 / 0x40EB70, 0x40EBA0,
  0x40EBE0, 0x40EDF0, the destructors 0x40ECD0 / 0x40ED90 / 0x40EA50 / 0x40F760, and 0x40FA40 / 0x40FB60 / 0x40FB90
  (text getters).
- **Destructors and stubs of the screens and menus, ~25:** 0x402880, 0x402B30, 0x405CF0, 0x405DD0, 0x406120,
  0x406410, 0x4062E0, 0x406A00, 0x406B20, 0x406C00, 0x406DA0, 0x406DC0, 0x408170, 0x408AA0, 0x408E20, 0x408F10,
  0x4091D0, 0x4092C0, 0x4093F0, 0x409550, 0x4096C0, 0x40A000, 0x40A020, 0x40A9B0, 0x40A9E0.
- **Small getters, ~12:** 0x402950, 0x402970, 0x4029C0, 0x404BA0 / 0x404BB0, the game and UI time functions
  0x4038C0–0x403900 (the port's `stillTicks` covers UI time), and the cursor helpers 0x40CC30 / 0x40CC60 / 0x40CD10 /
  0x40CD20 / 0x40CC70 (the cursor drawer goes to the asm blitter).
- **Dead or unreachable, 5:** the GT logo splash 0x403A40 / 0x405D00 (nothing sends 0x8035) and the picture sequence
  0x404130 / 0x408E30 / 0x408F20 (dword_4D25E8 is never written).

## Ported

- 0x401940 WinMain → `main.cpp:203` (side from `--human/--martian`, language from `language.txt` via the
  installer, SaveGame folder at `frontend/FrontEnd.cpp:455`)
- 0x4025F0 TableRandom → `core/Random.h:34` (matches `idx % 1024`, `% (n+1)`)
- 0x402630 WGame ctor → `main.cpp:211`, `core/Random.h:27`
- 0x402A30 Start, with Skip → `frontend/FrontEnd.cpp:96-99`
- 0x4039D0 legal splash → `frontend/Splash.h`
- 0x405BF0 legal splash → `frontend/Splash.h` (GType 0x36D / 0x36E, group 0x2F5, ticks 90 / 180 / 300 all checked
  against 0x405C31 / 0x405C74 / 0x405C92)
- 0x403AB0–0x403F00, 0x404090, 0x4041B0, 0x404230, 0x4045C0, 0x4047B0, 0x4049C0, 0x404A60, 0x404B00 screen builders
  → `frontend/FrontEnd.cpp:716-930`
- 0x403FA0 disc check → `FrontEnd.cpp:760-766`
- 0x4042D0 SaveGame → `FrontEnd.h:83`, `core/SaveFile`
- 0x4043D0 → `FrontEnd.cpp:748-753`
- 0x404660 NewCampaign → `FrontEnd.cpp:755-758`, `:493`
- 0x4048F0 StartCampaign → `FrontEnd.cpp:774-783` (StartWarMap)
- 0x404BC0 / 0x404C10 pause / resume → `platform/Sound.h:52`, `main.cpp:337`
- 0x404C60 / 0x404CD0 → `core/SaveGame.h`
- 0x405470 / 0x4055C0 credits set-up and handler → `frontend/Credits.cpp` (speed (h−640)/3150 or /4200, start −60,
  end h−960 / h−480, sound 0x191, 60-tick lock-out). Divergences listed above.
- 0x4057B0 / 0x4057F0 / 0x4059A0 / 0x405A70 Sound Control → `FrontEnd.cpp:262-285`
- 0x405ED0 main menu → `FrontEnd.cpp:215-235` (button set and save check for slots 1-5 match 0x40601B-0x40603E;
  sound 0x18C with a 60-tick fade)
- 0x406130 Game Options handler → `FrontEnd.cpp:115-120`
- 0x4061B0 / 0x406310 / 0x406470 / 0x406530 / 0x4065E0 / 0x4067C0 / 0x406860 Fullscreen View and fades →
  `FrontEnd.cpp:148`, `:948-981` (50% then black, command 5 ticks later, 0x8106 backdrop change), and
  `frontend/Screen.*`
- 0x4068F0 Single Player → `FrontEnd.cpp:350-359`
- 0x407990 / 0x4088B0 CD Player → `frontend/CdPlayer.*`
- 0x408B70 → `FrontEnd.cpp:362` (always true, by design)
- 0x408CF0 Game Options → `FrontEnd.cpp:238-247`
- 0x4090A0 / 0x4091E0 / 0x4092D0 / 0x409400 save / replace / edit (name ≤ 36 characters) → `FrontEnd.cpp:202-211`,
  `:381-401`
- 0x409460 / 0x409560 quit / load → `FrontEnd.cpp:192-200`, `:250-259`
- 0x409A90 Display dtor (scroll speed (pos+1)·10) → `FrontEnd.cpp:327-330`
- 0x409DE0 critical events → `FrontEnd.cpp:70-80`, `:336-347`
- 0x40A030, 0x40A060 menu base, 0x40ADC0 glow → `frontend/Menu.cpp:176-192`. The glow was checked against
  0x40AE15-0x40AE90: ping-pong between First and Last, the old control reset to "Menu Items".
- 0x40A320 / 0x40A480 / 0x40A630 / 0x40A7D0 / 0x40A8C0 / 0x40AA10 / 0x40AB00 / 0x40AC80 control factories →
  `frontend/Menu.h`, `frontend/Screen.h`
- 0x40B030 SetTimeOfDay → `core/Landscape.h:40`, `battlemap/BattleMap.cpp:34-55`
- 0x40B570 LANDR intensity → `BattleMap.cpp:49`, `core/Landscape.h:41`
- 0x40B120 / 0x40B3B0 system colours → `battlemap/BattleMap.cpp:158`
- 0x40B500 / 0x40B520 shade level and slot → `core/ShadeTable.h:7`, `frontend/Briefing.cpp:12`
- 0x40C930 PointLight → `core/TerrainView.h:78`
- 0x40CA90 DirectionalLight → `core/IsoObject.cpp:22`, `core/Landscape.h:55`
- 0x40E5A0 SetViewPos → `battlemap/BattleMap.cpp:991`
- 0x40E7A0 / 0x40E880 / 0x40E910 / 0x40E980 GType table and RTypeBase → `core/ObjOjd`, `frontend/Resources.cpp`
  (FindGType / GetFrame)
- 0x40EAF0, 0x40EC30, 0x40EC50 / 0x40ECF0 / 0x40EDA0 sprite set-up, 0x40EE10 sprite render → `core/Sprite.h`,
  `battlemap/BattleMap.cpp:874`, `:1512`
- 0x40EF20 / 0x40EFB0 / 0x40F000 / 0x40F040 animate, frame and brightness → `battlemap/Effects.cpp:80`,
  `frontend/Screen.h:136`
- 0x40F620 / 0x40F6D0 / 0x40F770 / 0x40F9F0 / 0x40FA50 / 0x40FA90 / 0x40FBA0 / 0x40FBF0 RTypeText →
  `frontend/Canvas.*`, `core/Font.*`, `frontend/ResearchScreen.cpp:131`
