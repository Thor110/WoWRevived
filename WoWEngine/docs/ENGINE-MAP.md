# Engine map (vanilla WoW_patched.exe)

This is a map of the original executable, used to plan the rebuild one subsystem at a time.

The debug strings show the class names Rage used (WGame, WWarMapWorld, WLandscape, DeeDraw and so on). The compiler kept each source file's functions together, so these strings also mark where one subsystem's code ends and the next begins.

## Hand-written assembly

Parts of the exe were written in assembly rather than compiled C++, the blitters especially (sprite rows 0x451235, the pixel writers around 0x45702D, the IOB patch drawing). They don't follow the compiler's calling conventions or stack frames, so IDA's function boundaries and argument lists there can be wrong, and code can jump into the middle of another routine. The engine doesn't copy them: it reproduces what they draw (checked against the original's output), in plain C++.

## Sources

- **IDA listing.** The listing is of the patched vanilla build: it imports `_INMM.dll`, so the winmm calls show up as `_imeGetTime`, `_imeSetEvent` and so on. That build was compiled on 13 Nov 1998, with its code (.text) section running 0x401000–0x4A8A92.
  - The listing's input MD5 (`F29999CB…`) is an older patched copy than the current `WoW_patched.exe`. The section layout is the same, so the addresses still match.
  - The patches have changed a few instructions. Where the rebuild copies behaviour, it should copy the original behaviour or a deliberate fix, not the patch bytes.
- **`docs/ida/functions.tsv`.** One row per function: address, size, how many functions call it, the functions it calls, the Windows APIs it uses and the strings it references.
- **`docs/ida/strings.tsv`.** Every string, and the functions that use it.
- **Regenerating the index:** `python3 tools/ida_index.py IDA-Output.txt docs/ida`.
- **WoW_network.exe.** This is a separate build, compiled 5 Jul 2006, with a larger code section (0x401000–0x506ABF). There's no listing for it yet. The addresses already known for it are in `WoWPatches/Common/GameCommon.h` and `winmm_driver9x.cpp`.

## Start-up and threads

| Address | What it is |
|---|---|
| 0x468070 | CRT entry point, calls WinMain |
| 0x401940 | `WinMain`: settings, single-instance mutex, DirectDraw, starts the threads, message loop |
| 0x402630 | Game initialisation: checks `Human.cd` / `Martian.cd`, loads `Text.OJD`, sets up the world |
| 0x402180 | Game thread, woken by the multimedia timer (`Thread Enable`, `Game Frequency`) |
| 0x4020D0 | Timer callback, sets the game thread's event |
| 0x402230 | Render thread: calls 0x403920 (frame update) and 0x40AF20 (draw) |
| 0x402080 | Sleeper thread (`Sleeper Enable`) |
| 0x4015C0–0x402540 | Small getters for settings (`Debug`, `I Am Human`, `Install Path`, `CD Path`, `Game Memory`…) |
| 0x478680 | Multimedia timer set-up (`timeSetEvent`) |

The rebuild runs everything on one thread at a fixed 30 updates per second. That replaces this timer-and-threads arrangement.

## Subsystems by address

| Range | Subsystem | Notes |
|---|---|---|
| 0x402B80–0x405A70 | WGame | Game over, saving and loading (`SaveGame\%s.%03d`), pause and resume |
| 0x4057B0–0x40A1B0 | Front-end menus | Volume, full-screen view, network host, game session, Game Options, Quit, Display Settings, WFullScreenMenu |
| 0x407990–0x4088B0 | CD Player menu | Constructor 0x407990, destructor 0x408080, message handler 0x4081D0 |
| 0x40AFF0 | Time of day | |
| 0x40B570–0x40B6C0 | Shade tables | `LoadShadeTables`: 13 brightness levels, `dat\cd*.%s`, `dat\F1..F7`, `BM*` |
| 0x40CE00–0x40CF00 | Screen and renderer settings | `BLIT instead of FLIP` |
| 0x40E7A0–0x40EAE0 | RTypeBase and the GType table | Graphics table at 0x4DAFD0, 56 bytes per id; 0x40E980 loads a GType's file on first use |
| 0x40F060–0x40F5E0 | Text styles | Menu Heading, Research Detail, Briefing Heading, Credits Heading… |
| 0x40FEA0 | PolyLib set-up | `dat\b8.shl`, `dat\b16.hsh` |
| 0x410230 | Damage map | |
| 0x410E00–0x411170 | ISOLib | Isometric renderer, `DithDat`, `ISO Brightness` |
| 0x4122F0 | DirectDraw loader | `DDRAW.DLL`, `DirectDrawCreate` |
| 0x412690–0x412940 | CD audio position | Resume and reset the track position |
| 0x414410–0x414680 | WText | `TXT\` |
| 0x4148A0–0x415410 | Memory manager | `MEM_*`, allocation statistics |
| 0x414DE0–0x415200 | File loading | `dat\%s`, `Map\%s`, `FILE_Load` |
| 0x414410 | TEXT.ojd loader | Ported in `src/core/TextOjd.cpp` |
| 0x415030–0x415200 | Archive lookup and FILE_Load | Ported in `src/core/Vfs.cpp` |
| 0x415A80 | FFUH decompression | Ported in `src/core/Ffuh.cpp` |
| 0x4166B0 | Settings (registry) | `Use INI` |
| 0x417BF0–0x417C30 | Debug log | |
| 0x417CD0–0x418220 | WWarMapWorld | Constructor 0x417CD0, destructor 0x418220 |
| 0x419520–0x42B790 | War map | Routes, pods, briefing sound, events, WDocument, clunting (unit group movement), mini-map, calendar, sector limits |
| 0x42C460–0x42DDB0 | Win95DeskTop | Screen mode, app activation, pause and resume around alt-tab |
| 0x42D290–0x42D2D0 | Playback files | Recording and replaying games (`playback.%d`) |
| 0x42FEF0–0x4337B0 | UI widgets | Push buttons, scrollbars, info boxes, tooltips |
| 0x4344C0–0x4346C0 | WFogOfWar | |
| 0x435310 | World tasks | |
| 0x437410–0x437710 | Armour, research and manufacture modifiers | |
| 0x437750 | OBJ.ojd loader | 65 record types: the graphics table and the unit table (docs/UNITS.md) in `src/core/ObjOjd.cpp` |
| 0x439400–0x43A600 | Battle map set-up | Water, `dat\%d.nsb`, battle map selection |
| 0x43C0A0 | Resources | |
| 0x43D760–0x43FBE0 | Object type tables | Unit, building, scenery, tree, explosion and smoke names to object types |
| 0x4401E0–0x4407E0 | WSelectableObject pool | 1024 objects |
| 0x441500–0x444FDA | AI | Reads `AI.ojd` (0x441580), the AI tuning values, the computer player (0x442890) and its war map turn (0x442EC0, phases 0x443030-0x444C10). `warmap/WarMapAi.cpp` |
| 0x445010–0x449840 | WObject, sector stats, Zoomer, WCmdTarget | The battle AI 0x445010-0x445E30 in `src/battlemap/BattleAi.cpp` |
| 0x44A160–0x44A8D0 | DeePlay | DirectPlay networking: send and receive threads, Human and Martian players |
| 0x44AE60–0x44E4B0 | DeeDraw | DirectDraw surfaces, flip and blit, the cursor, error strings |
| 0x44DE20–0x44E060 | FMV | `FMV\*.smk` and `*.fmv` |
| 0x44EE70–0x44F120 | AStar | Pathfinding, shared by both maps; `src/core/AStar.h` |
| 0x44F2C0 | Smacker | Opens movie files |
| 0x457F20–0x45BB90 | WResearch | Research plans, improvement multipliers, obsolete items |
| 0x45D1A0–0x45F770 | Research and dialog screens | Hotspots, proposals, blueprints |
| 0x45F770–0x462EE0 | WBuildList | Each factory's queue: what it can make (0x45F970), the hourly progress (0x45FC90), save (0x460900); the Build List window from 0x460B00. `core/BuildList`, `warmap/Production.cpp` |
| 0x462EF0–0x466910 | SoundFXSystem | `sfx.ojd`, `SFX\SFX.WoW`, `VOX\HUMAN.WoW`, `VOX\MARTIAN.WoW`, volume groups, 128 sound objects |
| 0x466E30–0x4783C0 | C runtime and STL | Not rebuilt; replaced by the C++ standard library |
| 0x4783D0 | Reaper | Fatal error handler |
| 0x47AF10–0x47FC50 | WLandscape | Terrain from `maps\` (`.ils`, `.cls`, `vertInf`, `atm`), lighting, collisions; 0x47AF10-0x47CCF0 in `src/core/Landscape.cpp` (docs/BATTLEMAP.md) |
| 0x421FE0–0x422850 | Deployment | Centres and ring placement; `src/core/Deployment.cpp`, `BattleMap::Deploy` (docs/BATTLEMAP.md) |
| 0x436050 | Entry points | A pair of points for each pair of neighbouring sectors; `src/core/Deployment.cpp` |
| 0x480150–0x484A30 | Battle map | Unit groups, routing (`ROUTE FOUND`) |
| 0x485DE0–0x4862C0 | Battle map menu, music | |
| 0x4868B0–0x4873F0 | Battle map minimap | `BattleMap::SetUpMinimap` / `MakeDots` (docs/BATTLEMAP.md) |
| 0x4862C0–0x4868B0 | Bridge base | The ends (0x80A5 / 0x80A6), grid snap, heading and length; `src/core/Bridge.cpp` |
| 0x48D050–0x48D580 | Bridge kinds | Load (0x48D0A0) and the joint cross-section (0x48D2F0); `src/core/Bridge.cpp` |
| 0x486E80–0x487A50 | Renderers, transient and effect objects | 512 of each |
| 0x489340–0x4916E0 | Vehicles | Steering values (`Vehicles` section), shown on `Fun` / `Debug` toggles |
| 0x490820–0x4935C0 | Ground vehicle | Move order, steering and speed (0x492F40, 0x492DE0, 0x491870), neighbours (0x492C00 / 0x492A90) with separation (0x4920A0) and velocity matching (0x492160), the lookahead (0x492350, objects by 0x47FE10 / 0x47FD40) with avoidance (0x491D90) and braking (0x491C70); `src/battlemap/Driving.cpp` |
| 0x449710–0x449BD0 | WBucketGrid | The battle map's object grid, `[System] ZMapRes` cells; 0x449920 gives a point's cell and its nearer neighbours; `BattleMap::Gather` |
| 0x4802E0–0x480740 | WBMGroup ground | The group's terrain and slope costs from OBJ.ojd records 10 / 11 and a vertex's cost; `BattleMap::SetUpGrounds` / `CellCost` |
| 0x482B00 | Nearest passable node | `BattleMap::NearestPassable` |
| 0x493DF0–0x494960 | WRouteFinder | Node grid, route requests, route building; `src/battlemap/Routes.cpp` |
| 0x48A950 / 0x48A9A0 | Footprints | vt+0xE4 / +0xE8: an object's ground marked blocked (0x47C280) or cleared; `BattleMap::Occupy` |
| 0x4983D0 / 0x4984E0 | Bridge links | The bridge's two route nodes (via 0x494220, joined by 0x494400) and a vehicle's deck height on it; `BattleMap::AddBridgeNodes` / `OnBridge` |
| 0x483590 / 0x488FB0 | Destination flag / WAnimEffect | The "_ACK" flag and the one-shot animated sprites; `BattleMap::SpawnEffect` |
| 0x491450 | Engine sound | `BattleMap::EngineSound` |
| 0x47E1E0 / 0x47DA80 | Object's orders / special order check | The popup's order items for the selection's first member; `BattleMap::AddOrders` / `CanDo` |
| 0x488180–0x488350 / 0x48AB20 / 0x48A5A0 | WExplosion, TakeDamage, HitByExplosion | Explosions, splash damage, health and death; `src/battlemap/Combat.cpp` |
| 0x4949A0–0x495280 / 0x4886A0 / 0x487F50 | Guns | The fire cycle, aim and landing; `src/battlemap/Weapons.cpp` |
| 0x489820–0x489D80 / 0x494FD0 | Turrets | Turning, hardpoints and the guns' arcs; `src/battlemap/Weapons.cpp`, drawn in `BattleMap.cpp` |
| 0x487B00–0x487E54 / 0x495CB0 / 0x496100 / 0x496760 | Area blasts | Their detonation by type and the timed statuses (frozen, stance 0, turned), Infiltrate's 0x80F4 (0x43B920 -> the war map's intel, 0x418E17); `src/battlemap/Combat.cpp` |
| 0x4817C0 / 0x481CA0 / 0x496920 / 0x482570 | Modes and specials | The group's mode, which guns fire in it, the computer's specials; `src/battlemap/Weapons.cpp` |
| 0x43D150 / 0x43D3F0 / 0x480EF0 | Reinforcement points | The points, their markers and Retreat; `src/battlemap/Reinforcements.cpp` |
| 0x43A980 / 0x43A800 / 0x4212F0 | The battle's end | Each side counted, the InfoBox, the report; the losses back to the war map (`BattleMap.cpp`, `WarMap::FightBattle`) |
| 0x489E20 / 0x48A950 / 0x48A9A0 | Damageable objects | Health, bit 7, the footprint marked and freed; the scenery as `props` in `src/battlemap/Combat.cpp` |
| 0x48DD50–0x48E270 | Lights | The light parts and their fade; `src/battlemap/Effects.cpp` |
| 0x487730–0x487890 / 0x47F050 / 0x48AEF0 / 0x48ADD0 | Flying objects | Effects' update, emitters, damage smoke; `src/battlemap/Effects.cpp` |
| 0x4889D0 / 0x488E80–0x488FA0 / 0x47F770 | Mortars | The launch from the emitter list and the shells' flight; `src/battlemap/Weapons.cpp` |
| 0x481F70 / 0x482880 / 0x482960 / 0x496300 / 0x496570 | Targets | Attacking an object, Approach, looking round by stance; `src/battlemap/Weapons.cpp` |
| 0x498D40–0x4993F0 / 0x493600–0x493C80 | Going under | Tunnel and Submerge: the sinkers' height, hiding, marker, surfacing, hit filter; `src/battlemap/Sinking.cpp` |
| 0x491020 / 0x483880 / 0x48CC00 / 0x48CCB0 / 0x4898A0 | Hits and detection | A vehicle's hit told to its group (retaliation, Avoid's run), what can be targeted, the mines' marker; `src/battlemap/Weapons.cpp` |
| 0x499420–0x4998D0 | BatRoute | The battle A*'s cost (line cost 0x4994B0, turn penalty 0x499660, goal 0x499800) and the route container; `BattleMap::LineCost` / `StepCost` |
| 0x495620–0x496D60 | Puppets | Units' sub-objects |
| 0x496D60–0x497890 | Hotspots | An object's click window, its selection brackets and power bar; `BattleMap::ReleaseLeft` / `DrawSelection` |
| 0x49C080–0x49C740 | Drag rectangle | The selection box; `BattleMap::DrawSelection` |
| 0x497890–0x498750 | WBridge | Segments (0x497900) and their stitching (0x497AC0, 0x497E00, 0x498110); `src/core/Bridge.cpp`, `WofInstance::Stitch` (docs/BATTLEMAP.md) |
| 0x499850–0x49A820 | Briefing and debriefing | `src/frontend/Briefing` |
| 0x49B980–0x49BD50 | War map and battle map menus | |
| 0x49BE70–0x49CD40 | RTypeArrow | |
| 0x49DE20–0x49E1E0 | RTypeISO | Isometric objects and lighting |
| 0x49E330 | WOFLib | 3D models (see `ENGINE_LIMITS.md`) |
| 0x4A1CD0–0x4A20A0 | Minimap dots, RTypeMiniBattleMap | `maps\hmin%02d.spr`, `maps\mmin%02d.spr`; `src/battlemap/BattleMap.cpp` |
| 0x4A3920 | Archive reader | `Unable To Read Header` |
| 0x4A3E60 | InfoPage | |

## Known data addresses

| Vanilla | Network | What it is |
|---|---|---|
| 0x4B84C4 | 0x530518 | "I Am Human" (side being played) |
| — | 0x5432D0 | Network player id (non-zero in a network game) |

## Order of work

1. File formats. Archives, FFUH, palettes, sprites, fonts, TEXT.ojd and the OBJ.ojd graphics table are done. Next come the shade tables, the rest of OBJ.ojd, and the sound files.
2. The front end. The menu framework, Main Menu, Game Options and Quit are done; the details are in `FRONTEND.md`.
3. The war map, from WWarMapWorld at 0x417CD0.
4. The battle map: WLandscape at 0x47B900, ISOLib, WOFLib and the vehicles.
