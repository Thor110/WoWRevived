# Review: 0x490000-0x4A4FFF (403 routines, 0x490030-0x4A4470)

**Counts:** PORTED 187 · PARTIAL 24 · MISSING 70 · LIBRARY / not needed 122 (total 403).

Method: every `proc near` in the range was listed, its callers, callees and strings extracted, and matched against
address citations in `WoWEngine/src` and `docs/` (incl. `docs/survey/part12-14.md`). Cited routines got a quick
logic spot-check; the large ones (0x492DE0, 0x492F40 turn and reverse, 0x4994B0, 0x497900, 0x49DCA0/0x49E330,
0x49EC00, 0x497110/0x497680/0x497770, 0x4929A0, 0x4958B0) were read in full. Port paths are relative to
`WoWEngine/src`. "Equivalent" means no address is cited but the port does the same job (e.g. Briefing screens).

## Missing

### Vehicles: subclasses and per-vehicle behaviour (0x490030-0x493D70)
- **0x490030, 0x490040, 0x490070**: the tripod (walker): vt+0x11C runs the footprints (0x48FDD0), then steering, then the
  leg animation rate from speed (`startTurnRate` 12, `turnAnimRate` 0.02, table flt_4F0538). The port has no walker
  class: tripods drive like ground vehicles and have no leg animation. *Task:* add a walker subclass (animation rate,
  footprints 0x3C7; 0x48FCB0/0x48FDD0 are just before this range).
- **0x490120, 0x490160, 0x490170**: the flier: steering, then it keeps its height above the ground (0x47B120) and bobs
  (0.8, 4.0, 0.01, 5e-5, scaled by OBJ+0x54). *Task:* add a flier subclass (height and bob).
- **0x493490**: vt+0x130 makes the model wobble or bob (flt_4F0538). **0x4934F0**: vt+0x134 spawns a 0x3C8 dust or wake
  trail behind a vehicle whose move class is not 5 or 6 and whose speed is over 8. *Task:* add the trail effect and the bob.
- **0x493870-0x493B50**: the Ironclad class: its own vt+0x14 and stop test, braking, avoidance, arrival and separation
  scaled by 0.5 or 0.25 (0x4938E0, 0x4939A0, 0x493A40, 0x493AD0), a random 0x80C1 in its tick (0x493B50), and 0x493B40 returning 1.0.
  **0x493700, 0x4937C0, 0x493860**: the Submersible's scaled braking and avoidance. *Task:* give these per-class
  behaviour overrides in `Steer` (battlemap/Driving.cpp:323).
- **0x493D00**: the Tunnelling Track Layer's top speed is OBJ+0x54 × 50/65 once research dword_588CC8 is done (0x45B890).
  **0x493D70**: the mound sprite GType 0x35A (vt+0x174). *Task:* add both to `battlemap/Sinking.cpp`.
- **0x4915B0 / 0x491630** (vt+0x110 / +0x114): a stopped vehicle marks its footprint (0x47C280, scaled ×1.25 by
  dbl_4B01E0) and clears it when it moves again. The port says this is not in (Driving.cpp:243-244). *Task:* occupy and
  free the footprint on the stop and start edges.
- **0x4929A0** (vt+0xBC, "does this object block me"): no for objects with +0xB8 bit 2. For walkers (OBJ+0x42 kind 3 or 4),
  crushable objects (vt+0xD8) lower than 0x20 or 0x30 px are ignored. Units (+0xB8 bit 0) always block. On a bridge
  link, bridges 0x1BC-0x1BF and 0x1CC-0x1CE are ignored and 0x1C0 never blocks. Anything else blocks by +0xB8 bit 1.
  The port lets every object through (Driving.cpp:525, `ObjectAt`). *Task:* implement this filter in `ObjectAt` and `Gather`.
- **0x490F10**: vt+0x18, removing a vehicle (vt+0x13C, vt+0x114, 0x4393D0, 0x495830). **0x4910B0 / 0x491180 / 0x496440**:
  0x808A halt (+0x188 |= 0x20) and resume, and the puppet's Stop (each member vt+0xCC, +0xD4 = 1000000, goal cleared).
  The port does not use flag bit 5 at all ("bit 5 isn't in", Driving.cpp:322). *Task:* add the halt flag (bit 5) and
  the 0x808A path.

### Routes and fences
- **0x494370**: `WRouteFinder::RemoveNode`, called from 0x486850 (bridge) and 0x498CE0 (fence). TASKS already lists it
  ("node removal"). *Task:* remove a bridge's or fence's extra nodes when it goes.
- **0x4987F0-0x498D30**: WFence, for types 0x1C3-0x1C8 (wire fences, walls, electric, laser and plasma fencing). The
  missing pieces are save/load (0x4987F0, 0x498890), snapping the ends (0x498900), the segments rebuilt as type 0x17B
  as the fence is damaged (0x4989F0), its messages 0x80A5-0x80A7 and 0x80D1/0x80D2 (0x498B70), and blocking or
  unblocking the route nodes (0x498CB0, 0x498CE0). Nothing in `src` handles these types as live objects. *Task:* add a
  WFence object (damage segments, route blocking).

### Projectile and gun visuals (base 0x4949A0)
- **0x4950D0** (vt+0xE0) makes an atlas-sprite renderer (0x49D710) with alpha OBJ+0xA1. **0x495180** (vt+0x4C) places
  it (−4, 16, frames from OBJ+0x2C/+0x28). **0x495280** (vt+0xE4) draws a beam or trail as a textured quad (0x49D820)
  from the record 12 words +0x1A/+0x1C and 0.25. All three are also used by the 0x19A-0x19C class (0x48DC30). Guns in
  `battlemap/Weapons.cpp` have no visual of their own. *Task:* port the shot renderer: atlas sprite **0x49D710/0x49D7E0**
  and textured quad **0x49D820/0x49D930** (both MISSING).
- **0x494C50 / 0x494CC0**: the shot's remove and message handler (vt+0xC8/+0xCC). Low priority.

### Puppet
- **0x4964B0** (vt+0x100): fog reveal around a friendly unit (0x434A80, radius = sight OBJ+0x48), called every
  "Fog update freq" ticks from 0x4967E0. *Task:* comes with fog of war, which TASKS lists as to do.

### Rendering
- **0x49E2C0**: `RTypeISO::SetBrightness` relights an IOB object (0x49DF40) when its brightness changes. This is what
  the lights' 0x8066 messages drive; Effects.cpp:227 says 0x8066 is not in. *Task:* relight IOB objects inside a light's
  reach.

### Battle UI: the build list's placement mode (cursor modes 4-6)
- **0x4A13B0 / 0x4A1430 / 0x4A1440 / 0x4A1460 / 0x4A14C0** (vtable 0x4B1270, cursor mode 4) and **0x4A03A0 / 0x4A0430 /
  0x4A0490 / 0x4A0570 / 0x4A0590 / 0x4A05B0 / 0x4A0630** (vtable 0x4B1240, modes 5 and 6, two clicks):
  - The build window 0x460D70 sets these cursor modes: mode 4 for one building; mode 5 for type 0x1BC (Bridge Military);
    mode 6 for 0x1C3-0x1C8 (fences and walls, drawn from an anchor point to an end point).
  - The footprint is (OBJ+0x36 or +0x34 word × 2 + 14) & ~15. ShipYards (0x27-0x29) set flag +0x41.
  - The ghost outline follows the terrain (0x47B170) every 16 units and is drawn green or red as 0x47B610 (can build)
    answers; the mouse position is smoothed.
  - The click posts 0x80A4 {type, x, y[, x2, y2], 0xDEADDEAD} to 0x4EA448 and then 0x8137.
  - TASKS line 128 says "left for the battle map: placement modes".
  - *Task:* add the placement modes (ghost, can-build test, 0x80A4).

### Other UI
- **0x4A3E60, 0x4A4080, 0x4A40A0, 0x4A4160, 0x4A4190**: InfoPage, an object's info text (TEXT.ojd id from
  dword_51A108[type]). Font 0x73, wrapped at 256 px, placed at (x+32, y+32) and kept inside the view, colour
  `[Colours] "Info Page Text"` 15. After 15 ticks timer 0x812C plays the string's speech; a key-up or button-up closes
  it, fading the speech over 30 ticks. Opened by 0x4403A0 from message 0x111. Nothing similar is in `src`.
  *Task:* add InfoPage.
- **0x49BAA0, 0x49BBF0, 0x49BCC0**: the debug "Create..." menu (CreationMenu, 344 types, cmd 0x8027). Debug only.
  *Task (low):* optional, a dev cheat.
- Debug overlays `[Debug] Show Accelerations / Show Radii / Show Arrival / Show Orders` (registered at 0x490760-0x4907E0
  and 0x496520, drawn with RTypeLine 0x4A21F0 from 0x48BEE9-0x48C0E1) are not in. *Task (low):* debug overlay.

## Partial / divergent
- **0x490A70** (the vehicle's messages): 0x80C3 (face and drive toward (x, y) at speed 0.2: the deployment rally),
  0x808A and 0x80B9 (sets bit 9) are not handled. 0x80C2 and 0x802E are ported (Driving.cpp:36-216, Routes.cpp:339).
  *Task:* add 0x80C3 (TASKS "the 0x80C3 rally"), 0x808A and 0x80B9.
- **0x492DE0** (speed control): the game picks the accelerate or brake branch with vt+0x144 (0x4909D0). That test is
  "going" when bit 0 is clear **or bit 5, bit 8 or the link byte +0x18C is set**, and not when frozen (vt+0x108).
  The port tests only `!(d.flags & 1)` (Driving.cpp:730), so a stopped vehicle on a bridge link slows down where the
  game keeps it at 0.2 or more. *Task:* use the same `active` test as `Drive` (Driving.cpp:272).
- **0x4913B0 / 0x491870 / 0x492350 / 0x492C00 / 0x4911B0**: ported but marked provisional in the code:
  - bit 5 and broken bridges are not in;
  - the grid holds units only, not every placed object;
  - crushing (0x488070) is not in;
  - the second target (+0xC8/+0xCC), 0x80B9, the sector stats and the selection refresh are not in.

  See Driving.cpp:322, 465, 525, 564 and 745. *Task:* follow up on those notes.
- **0x4958B0** (WPuppet TakeDamage): the research armour and weapon multipliers are missing:
  - by weapon type: 0x1AB-0x1B0 and 0x1B5-0x1B7 ×dword_4CA180/4CA184; 0x393-0x395 ×4CA194/4CA198 (and they do nothing
    to non-friendly targets); 0x2C8-0x2CA ×4CA1A0; ×4CA35C;
  - for friendly targets: ×4CA348/4CA34C and ×4CA188.

  `Hit` → `Damage` (Weapons.cpp:473, Combat.cpp:43) applies the raw amount. *Task:* apply the multipliers once research
  is done (0x45B890 on dword_588D6C … 589050).
- **0x494AF0** (shot launch): research upgrades +0xC8 / +0xCC are "not in" (Weapons.cpp:672). *Task:* same research hook.
- **0x49EC00 / 0x49ED90** (WOFLib render and face light): before sorting, 0x49EC00 picks for each of dword_4B8E28 (2)
  light slots the light from the list (arg_8) that is within its radius + 64 (0x43FF00) with the smallest distance ×
  its +0x40 under 1e6. 0x49ED90 adds those point lights × "WOF Brightness" (2.5). The port lights by the sun only
  (WofView.cpp:250, "point lights would be added here"). *Task:* pass the battle's lights to `WofInstance::Draw`.
- **0x49E8E0 / 0x49EC00**: the pieces are sorted with MSVC `_qsort` (0x49ED32), which is not stable. The port uses
  `std::stable_sort` (WofView.cpp:321), so pieces of equal depth can come out in a different order (a visible z-fight).
  *Task:* port MSVC's qsort (0x4A4480 / `_shortsort` 0x4A4630) or accept the difference knowingly.
- **0x497680** (hotspot click-select): see Bugs (an already-selected group). Also: with Shift, command 5 adds the
  object to the selection (0x447D80 with GetAsyncKeyState(16)); vt+0xEC decides between 0x8088 (group) and packet 5
  (object). The port selects groups only (BattleMap.cpp:239 notes it). *Task:* Shift adds to the selection.
- **0x4A3350 / 0x4A34C0 / 0x4A3590 / 0x4A35C0 / 0x4A35E0 / 0x4A3420** (IdStack, n = 0x8000, at 0x518748): object ids
  start from 0, lowest free first, and are reused last-in first-out as objects die. The battle map gives `nextId++`
  from 1 (BattleMap.h:252, BattleMap.cpp:456). The ids appear in SELO saves and in packets 0x10 / 5. *Task:* add one
  id allocator shared with the war map.
- **0x499040 / 0x4991D0** (sinker draw and label): the name label (RTypeBoxedText, font 0x6F) is not drawn (Sinking.cpp:47).
- **0x49F4E0** (WLandscapeView::UpdateViewport): it tells 0x434BD0 (the minimap and fog) the visible cell rectangle.
  The port draws the minimap's view rectangle, but the fog side is not in. Unclear beyond that.
- **0x49D9C0** (a landscape object's sprite with scale 1.0, made from 0x47ED7C): the port draws scenery sprites through
  its own path. Not verified.
- **0x4A1D80** (AddDot): stores `colour | (colour & 0xFF) << 8`. The port computes its own colours, and type 987 and the
  own colours of 694 / 695 are "Not yet" (BattleMap.cpp:196-198).

## Bugs
1. **Click-select of an already-selected group (0x497680 vs BattleMap.cpp:294).** In 0x497680, after the checks for
   +0xB8 bit 6, the side (+0x24 / byte_4D1279) and Ctrl (+0xB8 bit 0), the game *always* issues packet 4 (clear the
   selection, 0x4976DA-0x4976F0) and then reselects (0x8088 or packet 5). The port returns early when
   `selected[best->group]` is already set (`if (!best->players || selected[size_t(best->group)]) return;`). So with
   several groups selected, clicking one of them keeps all of them selected in the port, where the game narrows the
   selection to that group. *Fix:* drop the `selected[...]` early return.
2. **Speed control's going test (0x492E8E `call [edx+144h]` vs Driving.cpp:730):** see Partial. With the link byte set
   and bit 0 set, the game takes the going branch (minimum 0.2, accelerate ×0.05); the port takes the stopped branch
   (decelerate ×0.1 to 0).
3. **Unstable vs stable piece sort (0x49ED32 `_qsort` vs WofView.cpp:321):** see Partial. The output differs only
   where depths are equal.

Checked and found to match (no bug): segment count `length × 0.015625 + 3` (0x497916-0x49791C, flt_4B0C14 = −3.0
subtracted; the survey's "−3" is wrong); speed constants 0.02 / 0.2 / ±0.05 / 0.1 and the link speed 1.0 (0x492DB0-0x492F28);
turn limit `8 + 120 × maxAcc × speed` against `+0x140 × speed` and reversing for base type 0x13 at |Δ| > 0x400
(0x493209-0x4932C2); the arrival growth `(1 + 0.625 √n) × r`; LineCost's −1 and `len × sum / n` (0x4995B6, 0x49963F);
WOFLib scales starting at int 1 and multiplied by flt_4F0534 with truncation (0x49E335, 0x49DD1B-0x49DD45); the cursor
choice in 0x497110 for modes 0 and 1; the hotspot's mode-1 actions in 0x497770 (Ctrl → packet 0x13 at the ground
point; friendly → packet 4 + 0x8088; enemy → packet 0x10); the debriefing's heading rules.

## Library / not needed (122)
- 32 static setting registrations: `[Vehicles]` ×21 (0x490260-0x490720), `[Debug]` ×4 (0x490760-0x4907E0,
  0x496520), `[Fun]` ×2 (0x4916A0, 0x4916E0), `[Stats] "Puppets objects left"` (0x4955E0), `[Tweak] "Audible radius
  multiplier"` (0x495620), `[Briefing]/[Debriefing] "Delay"` (0x4998F0, 0x49A770), `[Screen] "Tool Tip Offset"`
  (0x49D430), `[Game] "WOF Brightness"` (0x49E2F0). Their values are used by the port (setup.vehicles, Briefing delays, the tooltip).
- Setting classes 0x4A31E0-0x4A37E0 (int and float bound settings; note that 0x4A3740 writes the AI `[Tweak]` floats
  back to the INI on exit).
- Constructors, destructors, thunks and vtable stubs (about 45, e.g. 0x490A10, 0x4956D0, 0x4978F0, 0x498DA0,
  0x49BA90, 0x49CD40, 0x49DE20, 0x4A20A0, 0x4A22B0).
- The WPuppetHotSpot memory pool (0x496C40, 0x496CE0, 0x496E60) and WRouteFinder::UnSetup (0x4941A0).
- RTypeLine and Bresenham (0x4A21A0-0x4A2720): the port draws lines itself. Needed only for the placement ghost and the
  debug overlays.
- The owner slot (0x4A4360-0x4A43F0) and the scaled-sprite blit for the DeeDraw cursor (0x49D620 / 0x49D670).
- Game-order packet builders 0x4A4400 / 0x4A4440 / 0x4A4470: the command stream for network play and `playback.%d`.
  The port calls actions directly. Needed only for network or replay.
- Battle-object save chunks VEHI (0x490A20 / 0x490A40) and BATP (0x4956F0 / 0x495710): the battle map cannot be saved
  (its menu has no save, 0x49BD50), so these are not needed. `core/SaveGame.h` only lists the tags.
- 0x4984C0: moves sprite +0x1E by +0x30 and +0x26 by +0x40. Unclear; tiny.

## Ported
Each line is address (or range of consecutive ported routines) -> the port file that cites it. "Equivalent" means
uncited but implemented: Briefing and Debriefing (frontend/Briefing.cpp), the war map sector highlight 0x49C000-0x49C990
(WarMap shading), the drag box helpers 0x49C340-0x49C720 (BattleMap::DrawSelection), RTypeRect 0x49C740, BoxedText
0x49D2C0, WOF/ISO draw 0x49DBD0/0x49DE90, the light helpers 0x4A2800-0x4A2A80 (Effects.cpp), MatMul 0x4A2A90, the
SfxL reader 0x4A3820-0x4A3920 (core/Archive), WSectorPath cost 0x4A3CB0/0x4A3E40 (WarMap.cpp:302), VEHU 0x4A3BD0-0x4A3C80
(core/SaveFile), the caption button and label 0x4A4220-0x4A4300 (frontend Menu / ResearchScreen), and the sinkers'
init, cleanup and release 0x498DB0-0x4993A0 (Sinking.cpp).

- 0x490820 -> battlemap/BattleMap.cpp
- 0x4909D0 -> battlemap/Combat.cpp
- 0x490E70 -> battlemap/BattleMap.cpp
- 0x491020 -> battlemap/Weapons.cpp
- 0x4910C0 -> equivalent (see notes)
- 0x491100-0x491110 -> battlemap/Weapons.cpp
- 0x491450 -> battlemap/Driving.cpp
- 0x491530 -> equivalent (see notes)
- 0x491570-0x491590 -> battlemap/Driving.cpp
- 0x491730 -> battlemap/Driving.cpp (link leave)
- 0x491770 -> equivalent (see notes)
- 0x491790 -> battlemap/Routes.cpp
- 0x491B20-0x492240 -> battlemap/Driving.cpp
- 0x492950 -> battlemap/Driving.cpp
- 0x492A90 -> battlemap/Driving.cpp
- 0x492DB0 -> battlemap/Driving.cpp
- 0x492F40 -> battlemap/Driving.cpp
- 0x4935C0 -> battlemap/Driving.cpp
- 0x4935E0-0x493600 -> battlemap/Sinking.cpp
- 0x493640-0x493680 -> battlemap/Sinking.cpp
- 0x493B90 -> battlemap/Sinking.cpp
- 0x493C20-0x493C80 -> battlemap/Sinking.cpp
- 0x493DF0 -> battlemap/Routes.cpp
- 0x494220 -> battlemap/Routes.cpp
- 0x494400 -> battlemap/Routes.cpp
- 0x494440 -> battlemap/Driving.cpp
- 0x494690-0x494960 -> battlemap/Routes.cpp
- 0x4949A0 -> battlemap/BattleMap.cpp
- 0x494CB0 -> core/ObjOjd.h
- 0x494D20-0x494FD0 -> battlemap/Weapons.cpp
- 0x495740 -> battlemap/BattleMap.cpp
- 0x495850 -> battlemap/BattleMap.cpp
- 0x495BB0 -> battlemap/Sinking.cpp
- 0x495C10 -> battlemap/Weapons.cpp
- 0x495CB0-0x496100 -> battlemap/Combat.cpp
- 0x496200 -> battlemap/Weapons.cpp
- 0x496300 -> battlemap/Combat.cpp
- 0x496570 -> battlemap/Weapons.cpp
- 0x496760-0x4967E0 -> battlemap/Combat.cpp
- 0x496890 -> battlemap/Weapons.cpp
- 0x496920 -> battlemap/Combat.cpp
- 0x496BC0 -> battlemap/Weapons.cpp
- 0x496D60 -> battlemap/BattleMap.h
- 0x497000-0x4972F0 -> battlemap/BattleMap.cpp
- 0x497770 -> battlemap/BattleMap.cpp
- 0x497890 -> core/Bridge.h
- 0x497900 -> battlemap/BattleMap.cpp
- 0x497AC0-0x497E00 -> core/WofView.h
- 0x498110 -> battlemap/BattleMap.cpp
- 0x498300 -> core/Bridge.cpp
- 0x4983D0 -> battlemap/Routes.cpp
- 0x4984E0 -> battlemap/Routes.cpp
- 0x498750 -> core/Bridge.h
- 0x498D40 -> battlemap/Sinking.cpp
- 0x498DB0-0x498DE0 -> equivalent (see notes)
- 0x498E90 -> battlemap/Driving.cpp
- 0x498EE0 -> battlemap/Weapons.cpp
- 0x498F20 -> battlemap/Sinking.cpp
- 0x499010 -> equivalent (see notes)
- 0x4990D0 -> battlemap/Sinking.cpp
- 0x499120 -> battlemap/Driving.cpp
- 0x499150 -> battlemap/Sinking.cpp
- 0x4992D0 -> battlemap/Sinking.cpp
- 0x499350-0x4993A0 -> equivalent (see notes)
- 0x4993F0 -> battlemap/Weapons.cpp
- 0x499420 -> battlemap/Routes.cpp
- 0x4994B0-0x499800 -> battlemap/Routes.cpp
- 0x499850 -> battlemap/BattleMap.h
- 0x4998D0 -> battlemap/Routes.cpp
- 0x499930 -> frontend/FrontEnd.cpp
- 0x4999E0 -> warmap/WarMap.cpp
- 0x49A5A0-0x49A700 -> frontend/Briefing.cpp
- 0x49A7B0 -> frontend/FrontEnd.cpp
- 0x49A820 -> frontend/Briefing.h
- 0x49B750-0x49B900 -> frontend/Briefing.cpp
- 0x49B980 -> frontend/FrontEnd.cpp
- 0x49BD50 -> frontend/FrontEnd.cpp
- 0x49BE50 -> equivalent (see notes)
- 0x49BE70 -> warmap/WarMap.h
- 0x49BF50 -> warmap/WarMap.cpp
- 0x49C000-0x49C070 -> equivalent (see notes)
- 0x49C080 -> battlemap/BattleMap.h
- 0x49C230-0x49C250 -> battlemap/BattleMap.cpp
- 0x49C340 -> equivalent (see notes)
- 0x49C350-0x49C3E0 -> battlemap/BattleMap.cpp
- 0x49C630-0x49C740 -> equivalent (see notes)
- 0x49C7C0 -> battlemap/BattleMap.cpp
- 0x49C8D0-0x49C990 -> equivalent (see notes)
- 0x49CAC0 -> warmap/WarMap.cpp
- 0x49CDB0 -> warmap/WarMap.cpp
- 0x49D260 -> battlemap/Sinking.cpp
- 0x49D2C0 -> equivalent (see notes)
- 0x49D300 -> frontend/FrontEnd.cpp
- 0x49D470 -> frontend/FrontEnd.cpp
- 0x49DA50 -> core/WofView.h
- 0x49DBD0 -> equivalent (see notes)
- 0x49DCA0 -> battlemap/BattleMap.cpp
- 0x49DDA0 -> core/IsoObject.h
- 0x49DE90 -> equivalent (see notes)
- 0x49DF40-0x49E1E0 -> core/IsoObject.h
- 0x49E330 -> core/WofView.h
- 0x49E630 -> core/Bridge.cpp
- 0x49E690 -> core/WofView.h
- 0x49E900 -> core/WofView.cpp
- 0x49EEF0 -> core/WofView.cpp
- 0x49F240 -> battlemap/BattleMap.h
- 0x49F610 -> battlemap/BattleMap.cpp
- 0x49F770-0x49FDA0 -> core/TerrainView.cpp
- 0x4A0140 -> core/TerrainView.h
- 0x4A1CD0 -> battlemap/BattleMap.h
- 0x4A1D70 -> equivalent (see notes)
- 0x4A1DD0 -> battlemap/BattleMap.cpp
- 0x4A1F90 -> battlemap/BattleMap.h
- 0x4A2100 -> frontend/Briefing.cpp
- 0x4A27A0 -> battlemap/BattleMap.h
- 0x4A2800 -> equivalent (see notes)
- 0x4A2840 -> battlemap/Effects.cpp
- 0x4A28E0 -> equivalent (see notes)
- 0x4A2980 -> battlemap/BattleMap.cpp
- 0x4A2A20 -> equivalent (see notes)
- 0x4A2A80-0x4A2A90 -> equivalent (see notes)
- 0x4A2C50-0x4A3020 -> core/WofView.cpp
- 0x4A3820-0x4A3920 -> equivalent (see notes)
- 0x4A39E0 -> warmap/WarMap.cpp
- 0x4A3BD0 -> equivalent (see notes)
- 0x4A3C20-0x4A3C60 -> equivalent (see notes)
- 0x4A3C80 -> core/SaveGame.h
- 0x4A3CB0 -> equivalent (see notes)
- 0x4A3D10 -> warmap/WarMap.cpp
- 0x4A3E40 -> equivalent (see notes)
- 0x4A4220 -> equivalent (see notes)
- 0x4A4290 -> equivalent (see notes)
- 0x4A4300 -> equivalent (see notes)
