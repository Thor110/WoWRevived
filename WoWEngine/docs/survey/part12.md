# Part 12: 0x48A4C0 to 0x4956D0 (battle map objects, vehicle steering, WRouteFinder)

## 1. Overview

The piece has about 300 functions, all compiled C++ with SEH frames. Nearly all of it is the **battle map object classes**: the methods of the objects that the object factory **0x446B30** (`CreateObject(objectType)`) builds.

| Range | Contents |
|---|---|
| 0x48A4C0–0x48B3D0 | The shared base methods: selection, splash damage, TakeDamage, death, damage smoke, fog visibility, moving between cells. They sit in the vtables of every unit, building and scenery class. |
| 0x48B480–0x48F750 | The small object classes: CP marker, a "line-linked" object (0x128, 0x1D0–0x1D3, 0x2B6–0x2B7), mines, chained death explosions, bridges, fences and walls, a projectile type, smoke, lights, shadows, ground light decals, buildings, street lamps, tree circles, Martian pods and gun emplacements. |
| 0x48F970–0x4935C0 | The **vehicle** classes: drone, tripods (Martian walkers), fliers, and the generic ground vehicle `0x490820`. This includes the Reynolds-style **steering** (the `[Vehicles]` ini section), waypoint following, collision probing and the start/stop sounds. |
| 0x4935E0–0x493D70 | The overrides for the Ironclad, the Submersible and the Tunnelling Track Layer. |
| 0x493DF0–0x494960 | **WRouteFinder**, the battle map's node grid and route requests. It sits on top of AStar 0x44EE70. |
| 0x4949A0–0x495280 | The **projectile/weapon shot** base class (vtable 0x4B0890): research multipliers, flight and impact. |
| 0x4955E0–0x4956D0 | The start of the Puppets module: the stat `"Puppets objects left"`, the tweak `"Audible radius multiplier"`, and the WPuppetObject base ctor 0x495660. |

There is **no hand-written assembly** in this piece. Every function has a normal prologue and epilogue and normal `thiscall` use. Two IDA artefacts to know about:

- Every vtable's first slot is a `loc_` scalar-deleting-destructor stub (for example `loc_48B600` calls `sub_48B620`). IDA did not make functions of these.
- Several one-instruction functions are shared by unrelated classes because the compiler folded identical code:
  - `0x48D7C0` and `0x48B580` are `return false`.
  - `0x48FB30` is `return 1`.
  - `0x48FB40`, `0x48DBD0`, `0x493860` and `0x4938B0`-style functions return a float constant.

Object type ids below come from the name→type tables at 0x43D760–0x43FBE0 (`push name; push id; push 8027h; call 0x49BBF0`) and the factory's switch.

### Object factory (0x446B30): ids built by classes in this piece

| Object types (hex) | Names | Ctor (alloc size) | vtable |
|---|---|---|---|
| 1–6, 0xA–0x12, 0x1C–0x22 | Armoured Lorry, Armoured Track Layer, Self-Propelled Gun, Mobile AA Array, Sappers Lorry, Construction Workers, Repair Vehicle, Mortar Bike | **0x490820** generic ground vehicle (0x190) | 0x4B0258 |
| 7–9 | Tunnelling Track Layer Mk1–3 | 0x493B90 (0x1A0), base 0x498D40 | 0x4B06F0 |
| 0x13–0x15 | Ironclad Mk1–3 | 0x493870 (0x190) | 0x4B0568 |
| 0x16–0x18 | Submersible Mk1–3 | 0x4935E0 (0x1A0), base 0x498D40 | 0x4B03C8 |
| 0x19–0x1B, 0x4D–0x4F | Observation Balloon, Flying Machine | 0x490120 flier (0x190) | 0x4B0030 |
| 0x3E–0x4C, 0x50–0x5C | Scout, Fighting, Tempest, Bombarding, Electric, Scanning, Constrictor, Xeno Telepath, Handling Machine, Digging Mechanism | 0x48FC20 tripod/walker (0x194) | 0x4AFEA8 |
| 0x5D–0x5F | Drone Mk1–3 | 0x48F970 (0x194) | 0x4AFD30 |
| 0x32–0x39, 0x6F–0x76 | Medium Gun, Heavy Gun, AA Array, Rapid Heat Ray, Heat Ray Turret, Projectile Launcher | 0x48F750 emplacement (0x190), derived from 0x490820 | 0x4AFBB8 |
| 0x23–0x31, 0x3A–0x3C, 0x60–0x6E, 0x77–0x7A, 0x116, 0x33E… | All the factories and buildings, Parliament, Martian Base | 0x48E8A0 building (0xF0) | 0x4AF798 |
| 0xB3 | Pod | 0x48F590 (0xC8), base 0x489380 | 0x4AFAB8 |
| 0xCF–0xD4 | C/D tree circle 1–3 | 0x48F330 (0xCC), base 0x489380 | 0x4AF9B8 |
| 0x110 | StreetLamp | 0x48F290 (0xC8), base 0x489380 | 0x4AF8B8 |
| 0x13B–0x14A | vehicle shadows (gated by `[Options] Show vehicle shadows`) | 0x48E680 (0xC0, pool 0x487910) | 0x4AF698 |
| 0x14B–0x14D | ground-projected light/shadow decal (*guess*) | 0x48E2A0 (0xD4, pool 0x487910) | 0x4AF5A8 |
| 0x14E–0x15A | lights (gated by `[Options] Show lights`) | 0x48DD50 (0xD8, pool 0x487910) | 0x4AF4B8 |
| 0x16F–0x173 | Smoke 1–5 | 0x48DCB0 (0xC4, pool 0x487610), base 0x488FB0 | 0x4AF3D0 |
| 0x19A–0x19C | a projectile type (names unknown) | 0x48DBE0 (0xF8), base 0x488460 / 0x4949A0 | 0x4AF2E8 |
| 0x1BC | Bridge Military | 0x48D510, base 0x497890 | 0x4AF0A0 |
| 0x1BD–0x1BF | Bridge Country, City and Railway | 0x48D050, base 0x497890 | 0x4AEF80 |
| 0x1C0 | unknown (falls between the bridges and the wires) | 0x48CFB0 (0xC8), base 0x489E20 | 0x4AEE80 |
| 0x1C3–0x1C8 | Wire Fence, Wire Wall, Mined Wire Wall, Electric, Laser and Plasma Fencing | 0x48D780 (0x118), base 0x498750 | 0x4AF1B8 |
| 0x253–0x255 | Death_Explosion 6–8 | 0x48CD80 (0xD4, pool 0x487610), base 0x488180 | 0x4AED98 |
| 0x273–0x276 | Mines, Wire-Triggered, Undetectable, Enhanced Mines | 0x48C8E0 (0xEC) | 0x4AEC80 |
| 0x128, 0x1D0–0x1D3, 0x2B6, 0x2B7 | unnamed "line-linked" object | 0x48B590 (0x104) | 0x4AEB70 |
| 0x2B8 | CP MARKER | 0x48B480 (0xC8), base 0x489380 | 0x4AEA70 |

The other cases go to ctors outside the piece: 0x489E20 (static objects), 0x489380 (scenery and houses), 0x488FB0 (explosion parts and smoke), 0x488180 (death explosions 1–5), 0x487B00, 0x488410, 0x4887E0 and 0x488930 (more shot types), 0x489820 (types 0x389–0x38C) and 0x4873F0.

## 2. Subsystems

### 2.1 Shared object base methods (the WSelectableObject / WObject layer)

These sit in nearly every battle object vtable. The vtable slot meanings below are inferred from use across classes.

| Address | Name | What it does |
|---|---|---|
| 0x48A4C0 | `SelectIfInRect(RECT*)` | Selects the object if it's ours (`+0x24==0`), its screen point (+0x2C/+0x30) is in the rect, and it is selectable (`+0x3C` or `+0xB8` bit 0). Bumps the selection count (`[0x4E9B70]+0x114`) and calls 0x440680. Called from the base message handler 0x489FA0. |
| 0x48A530 | `GroupSelectIfInRect` | The same test, then sends **0x8088** to the object's group (`+0x48`). |
| 0x48A5A0 | vt+0xC0 `HitByExplosion(expl, float mult)` | Splash damage. Uses an octagonal distance between the two positions: `max(dx,dy) + min/2`. Damage = `(1 - d/(r1+r2)) * expl.power(+0xCA) * mult`, passed to vt+0xE0. Only when health > 0 and `+0xB8` bit 7 is set. Called by 0x4958B0. |
| 0x48A660 | `ShowStatusPuppet` | When vt+0xF0 (targetable) says yes, makes a 0x9C-byte **WPuppet** (pool 0x496C40, "256 WPuppet" blocks; ctor 0x496D60, a health bar or status widget) with size `OBJ+0x40 * 2`. Attaches it (0x4405C0), positions it, and sends **0x8125** to every child in the `+0x70` list. |
| 0x48A780 | `HideStatusPuppet` | Detaches the puppet and sends **0x8126** to the children. |
| 0x48A7D0 | `DamageFrame(n)` | The picture frame for the damage state: `nframes-1 - (n+1)*health/maxHealth`. Used by buildings and the minimap. |
| 0x48A830 | vt+0xF0 `IsTargetable` | Type 0x3DB is always targetable. Otherwise needs a model (`+0x4C`); the object's class (0x440700) must be 1–3, or the cheat flag `byte_4D1249` must be set, or class 4 with `+0xB8` bit 7. |
| 0x48A8B0 | vt+0x14 `AddToWorld(arg)` | Every 30th object (`dword_59E170 % 30`) pumps 0x44E5C0, presumably to keep the loading screen alive. Calls 0x48A660 and the base init 0x47EF30. Sets `+0xC0` bit 2 for types 0x1D0–0x1D3 and 0x2B6/0x2B7, then calls vt+0xE4 (adds to the occupancy map). |
| 0x48A950 / 0x48A9A0 | vt+0xE4 / +0xE8 | Add or remove the object's footprint (`OBJ+0x46` cells) in the landscape occupancy map (0x47C280 / 0x47C3A0 on `dword_59D940`). Only when `+0xB8` bit 1 is set and bit 2 is clear. |
| 0x48A9F0, 0x48AA30 | vt+0x18 `RemoveFromWorld` | Takes the object out of the spatial grid cell `+0xBC` (0x449900 on `[0x4D2550]+0x118`) and sets `+0xBC = -1`. |
| 0x48AA70 | vt+0x3C | Thunk to 0x47F900. |
| 0x48AA80 | vt+0x40 `SetPosition(x,y,z)` | 0x47F940, then moves the object to its new grid cell (0x449850 / 0x4498A0 / 0x449900). |
| 0x48AB20 | vt+0xE0 `TakeDamage(n)` | Sets `+0xC0 |= 8` (hit this tick) and `health(+0xC4) -= n`, and updates the status puppet's bar (0x497290). At or below 0 it posts **0x80A0 (die)** to itself after `10 + rand%20` ticks (0x4355A0). When health first falls below 40 % of max (0x47DA20), the object is added to the world's damaged list (`[0x4D2550]+0x20`, 0x440060). |
| 0x48AC50 | vt+0x78 `OnDestroyed` | **Victory and defeat hook.** If the destroyed type is the battle's key building it sends **0x80AA** to the battle owner (`+0x48 → +0x48`): the Martian Base 0x33E, or 0x78 (Communications Centre), when the battle kind (`[group]+0x48`) is 3; Parliament 0x116, or 0x13A, when the kind is 0x1E. For kinds 3 and 0x1E it also sends **0x8136** ("DESTROYING VEHICLES", debug string) and sets `byte_4E9B94=1` unless `[0x4E9B7C]+0x54`. Then vt+0xE8 and 0x47EAA0. |
| 0x48AD40 | `NotifyOwnerDestroyed` | Sends **0x812F**(id `+8`) to the owner side's list (`[+0x48]+0x4C` for ours, `+0x50` for the enemy) unless 0x42AA70 says otherwise. Called by the destroy handlers of bridges and fences. |
| 0x48ADA0 | vt+0xF4 `HealthFraction` | `health / maxHealth`. |
| 0x48ADD0 | `SpawnAt(type)` | Creates an object of `type` at a random offset within `radius/3` (+0xAC), at height `z - 1.414*OBJ+0x41`. Uses vt+0x40, vt+0x24 and vt+0x14(`[0x4D2550]+0x90`). |
| 0x48AEF0 | vt+0x10 `Tick` (static and scenery objects) | When hit this tick (`+0xC0` bit 3), sends **0x80C1** to the owner's `+0x6C` (an "under attack" alert). Every 15 ticks (`dword_4D2558 % 15`), if alive and `[Options] Show Smoke & particles` (`byte_4B880C`) is on: smoke level = `5 - 5*HealthFraction` clamped to 1–4. It spawns smoke 0x16F–0x172 (and 0x3E8 one time in eight, `rand < 0xCCC`); a large object (`+0xB8` bit 0) spawns only the top level, others every level up to it. Then the base tick 0x435590. |
| 0x48B050 | vt+0x58 `GetFogVisibility` | Samples the fog grid (`[0x4D2550]+0x84`: data +0x18, stride +0x84, max +0x9C, darkness +0x90) at the object's four corners, offset by its radius. Returns their average (0–1). |
| 0x48B2A0 | vt+0x5C (static objects) | Model brightness = visibility × 255 (model `+0x4C`, its vt+0x18). |
| 0x48B2D0 | vt+0x5C (units, through thunk 0x48C5E0) | The same, plus showing and hiding enemies. Above `[BattleMap] "Fog darkness"` (`dword_4EA5FC`) the unit becomes visible: vt+0xA0(0), status puppet 0x48A660, `+0xB8 |= 0x10`. Otherwise it is hidden: vt+0xA0(1), the render object is released (0x40EBE0), the puppet is detached, and the children get 0x8126. Only for `+0x24 != 0` (enemy). |
| 0x48B3D0 | vt+0x4C `Draw` (scenery) | Clips the object, then vt+0xA4, vt+0x5C, vt+0x9C and 0x47F630. |

### 2.2 CP marker (0x2B8) and the "line-linked" object (0x128, 0x1D0–0x1D3, 0x2B6, 0x2B7)

| Address | Name | What it does |
|---|---|---|
| 0x48B480 / 0x48B4D0 | CP marker ctor / dtor | Base 0x489380. Clears `+0xB8` bit 0. |
| 0x48B4E0 | CP marker vt+0x14 | 0x4896D0, then removed from the spatial grid (markers aren't hit-tested). |
| 0x48B520 | vt+0x18 | Thunk to 0x489710. |
| 0x48B530 | CP marker vt+0x1C | On **0x80A0** calls vt+0x78. Everything else goes to 0x4895C0. |
| 0x48B560 | | 0x47F960 wrapper. |
| 0x48B590 / 0x48B620 | line object ctor / dtor | Calls 0x495660 (puppet object). Fields `+0xE8` flags, `+0xEC` linked object, `+0xF0..+0xFC` four line primitives, `+0x100` word. |
| 0x48B630 / 0x48C500 | Release lines | Releases the four line primitives (ref-counted at +4) and clears bit 2 of +0xE8 and +0xC0. |
| 0x48BE30 | vt+0x14 `Init` | 0x495740. If the linked object is active, builds 3–4 line primitives (0x4A21A0, 0x4C bytes, vtable 0x4B12D0) in colours that depend on the type: black `dword_4DAF88` for 0x128, 0x1D0 and 0x2B6/0x2B7, and `dword_4DAFBC`/`C0`/`C4` for 0x1D1–0x1D3. The colours are palette indices from 0x40B120 (0x40B3B0 nearest colour). Then 0x48B700. |
| 0x48B700 | `UpdateLines` | Projects its own and the linked object's world positions to the screen. The isometric projection is `sy = (y*0xB4B10D5D>>16) - (z*0x2C3E63FB>>14) + land+0x48` and the same for x. It then sets each line's end points (`+0x3C..+0x48`), offset by -16 or -4 pixels, through the helpers 0x48BD40, 0x48BD60, 0x48BD90 and 0x48BDC0 (point packing, clipping and bounds, using 0x4163A0). |
| 0x48C5F0 | vt+0x4C `Draw` | 0x496560, then 0x48C650, then the line primitives are queued (`renderer->vt+0x18`). |
| 0x48C650 | Tick and positions | Keeps the lines following both objects. Plays its loop sound `OBJ+0xA2` (0x4883C0 / 0x4883E0). |
| 0x48C570 | vt+0x100 `RevealFog` | Calls 0x434A80 on the fog (`[0x4D2550]+0x84`) with the sight radius `OBJ+0x48`, in cells. |
| 0x48C840 | vt+0x10 | For our objects only, every `[BattleMap] "Fog update freq"` (`dword_4EA614`) ticks it calls vt+0x100. |
| 0x48C870 | vt+0x68 `DrawOnMiniMap(target)` | Sends **0x8077**(colour, `x | y<<16`) to the minimap. The colour is `dword_4DAFAC` for ours and `dword_4DAFB0` for the enemy's. |

The purpose of these types is **unclear**. They are lines drawn between two objects, with fog reveal and a minimap blip, so they could be tethers, cables or beam weapons (a guess).

### 2.3 Mines (0x273–0x276)

| Address | Name | What it does |
|---|---|---|
| 0x48C8E0 / 0x48C930 | ctor / dtor | Base 0x495660. `+0xE8` holds the sprite render object. |
| 0x48C940 | vt+0x14 | 0x495740. Sets `+0xC0` bit 2 if bit 0 is set, then vt+0xBC(0). |
| 0x48C970 | vt+0x18 | Releases the sprite, then 0x495830. |
| 0x48C9B0 | vt+0x1C | On **0x80F8** (triggered): vt+0xE0 on itself, then spawns smoke 0x16F at a random offset (the same scheme as 0x48ADD0). Other messages go to 0x495CB0. |
| 0x48CB10 / 0x48CB60 | vt+0xE4 / +0xE8 | Occupancy add and remove. |
| 0x48CBB0 | vt+0x4C | Draw. |
| 0x48CC00 | vt+0x10 | Shown only to the owner, or once detected (vt+0x8C, vt+0x90, vt+0x10C, `+0xDC`). Creates or releases the sprite through 0x48CCD0. |
| 0x48CCB0 | vt+0x6C | |
| 0x48CCD0 | `MakeSprite` | Render object (0x40E700, 0x44 bytes) with GType **0x359**, `word +0x26 = 0x20`. |

### 2.4 Effects: chained death explosions, smoke, lights, shadows, decals, unknown 0x1C0

| Address | Class | What it does |
|---|---|---|
| 0x48CD80 / 0x48CDD0 | Death_Explosion 6–8 ctor / dtor | Base 0x488180. |
| 0x48CDE0 | vt+0x10 | When the animation frame reaches 11 it spawns the next smaller explosion: 0x255→0x254→0x253→0x252. The new one goes at a random offset of up to 8 (0x4025F0 = `rand%n`), through 0x488070 / 0x488400. This is a **cascading explosion**. |
| 0x48CFB0 / 0x48CFF0 | type 0x1C0 | Base 0x489E20. |
| 0x48D000 | type 0x1C0 vt+0x5C | Brightness from the visibility and `OBJ+0xA1` (`byte_51A191`, record 62). |
| 0x48DCB0 / 0x48DCF0 | Smoke 1–5 | Base 0x488FB0. |
| 0x48DD00 | Smoke vt+0x14 | 0x489000. |
| 0x48DD20 | Smoke vt+0x18 | Does nothing. |
| 0x48DD30 | Smoke vt+0x10 | `z += -1.5` per tick: the smoke rises. |
| 0x48DD50 / 0x48DDB0 | Light (0x14E–0x15A) ctor / dtor | Base 0x4879B0 / 0x487A30. |
| 0x48DDC0 | Light vt+0x24 | Reads the per-type timing table **0x4EC598** (OBJ.ojd record 13, 16 bytes a type: float size, then grow, hold and fade times as words). Times are scaled by `Game Frequency` (`dword_4B84C8`) / 10, so they are tenths of a second. |
| 0x48DE80 | Light vt+0x14 | If the creator's class is 0–3 and `rand%100 >= daylight*100` (`flt_4DA768` from the time of day, 0x40B030), or the owner is Martian, the light is destroyed. If `[Options] Show lights` (`byte_4B8808`) is off, it is destroyed too. Otherwise `+0xA0 += 16` (height). |
| 0x48E100 | Light vt+0x10 | A three-phase scale ramp (`+0xCC` state, `+0xD0` countdown, `+0xC0` scale, `+0xC4` step), applied to the sprite (0x4A2840). Each tick it sends **0x8066** to every object under it (0x47FC50); a guess is "lit up, so visible at night". Phase 2 posts **0x8057** to itself (end). |
| 0x48DF30, 0x48DF50, 0x48E050, 0x48E270 | Light | Remove (0x4393D0, 0x47F260); vt+0x7C collision (also sends 0x8066); sprite set-up (0x4A27A0/0x4A2840/0x4A28E0, size from `flt_4EC598`); draw. |
| 0x48E680 / 0x48E6C0 | Shadow (0x13B–0x14A) ctor / dtor | |
| 0x48E6D0 | Shadow vt+0x14 | Destroys itself if `[Options] Show vehicle shadows` (`byte_4B8814`) is off. |
| 0x48E700 | Shadow vt+0x28 | Darkness from `flt_4DA770` (time of day). |
| 0x48E740, 0x48E750 | Shadow vt+0x98 / +0x7C | Follows its parent `+0x44`: position plus `height*(1/12)` with a -6 offset (ground projection). |
| 0x48E850 | Shadow vt+0x4C | Draw. |
| 0x48E2A0 / 0x48E300 | Decal (0x14B–0x14D) ctor / dtor | |
| 0x48E310, 0x48E330, 0x48E3A0 | Decal vt+0x14 / +0x18 / +0x1C | |
| 0x48E460 | Decal draw | Projects corners with the isometric constants and draws through 0x47C900 with `flt_4DA770` darkness. Exact use is unclear (a light pool, *guess*). |

### 2.5 Bridges (0x1BC–0x1BF) and fences and walls (0x1C3–0x1C8)

| Address | Name | What it does |
|---|---|---|
| 0x48D050 / 0x48D090 | Country/City/Railway bridge ctor / dtor | Base 0x497890 (bridge base). |
| 0x48D0A0 | vt+0x08 | Re-links up to six span pieces (vt+0x5C). Sends **0x80A5** / **0x80A6** to neighbouring objects (bridge destroyed or rebuilt, *guess*). |
| 0x48D230 | vt+0x0C | Sets the heading (vt+0x30) of the six pieces. |
| 0x48D2D0 | vt+0x18 | |
| 0x48D2E0 | vt+0x78 | |
| 0x48D2F0 | vt+0x10C `PlaceOnLandscape` | Reads terrain heights (0x47B170) along the span with constants 40, -8 and -4, then 0x498300 (route links). |
| 0x48D510 / 0x48D550 | Military bridge ctor / dtor | |
| 0x48D560, 0x48D570 (→0x498110), 0x48D580 | Military bridge methods | |
| 0x48D5F0 | Military bridge placement | Constant 40. |
| 0x48D590 | Bridge vt+0x78 `OnDestroyed` | Sends **0x807B** to both player objects (`dword_4E9B74` Human, `dword_4E9B78` Martian), then 0x48AD40, then 0x42A0B0 (updates the war map's sector record), then 0x4895A0. |
| 0x48D5E0 | vt+0x4C | Draw. |
| 0x48D780 / 0x48D7F0 | Fence ctor / dtor | Base 0x498750. |
| 0x48D800 | Fence vt+0x24 `BuildSegments` | Walks from one end point to the other in steps of 1/64 (`flt_4AF2D8`) and creates a segment object for each step (0x446B30, 0x47D190), keeping them in `+0x70`. Uses 0x498900. |
| 0x48D9A0 | vt+0x14 | 0x4989F0. |
| 0x48D9B0 / 0x48D9C0 | vt+0x1C | On **0x80A0** the death is passed on to every segment, a tick apart. Otherwise 0x498B70. |
| 0x48DA40 | vt+0x78 | Like 0x48D590, then 0x495BB0 and **0x43C750** (resource recount). |
| 0x48DAA0 | vt+0x50 | Per segment: health from the total, the line effects (0x48B630), and spark objects **0x17B** for the electric types. |
| 0x48DB80 | vt+0x68 | Draws the segments on the minimap. |

### 2.6 Buildings (0x48E8A0 class) and scenery

| Address | Name | What it does |
|---|---|---|
| 0x48E8A0 / 0x48E900 | Building ctor / dtor | Base 0x495660. `+0xE8` sound handle; the dtor stops it with 0x464E30 and calls 0x4956D0. |
| 0x48E9D0 | vt+0x14 `AddToWorld` | Sets health from the construction state. Flattens and samples the terrain under the footprint (`OBJ+0x50` = record 33, 0x47B170). Places four **0x17B** corner objects (scaffolding, *guess*) with alpha `OBJ+0xA1`. For the **PowerPlant (0x77)** and other types with an ambient sound `OBJ+0xA2` it starts the loop (0x465590 / 0x4883C0 / 0x4883E0). Adds itself to the world list (0x440060). Large function. |
| 0x48E990 | vt+0xF4 | Health fraction, with a -0.1 offset. |
| 0x48EEE0 | vt+0x18 | Remove: stops the sound, then 0x495830. |
| 0x48EF80 | vt+0xE0 `TakeDamage` | Uses the damage picture frame (0x48A7D0) and `OBJ+0x42`, and posts 0x80A0. Ends in 0x48F030. |
| 0x48F030 | vt+0xC0 | Hit by explosion. With a 1/64 chance (`rand < 500`) it plays sound **0x111** (`dword_4E9250` SFX, 0x464EE0). Owner checks use 0x4406E0, then 0x4958B0. |
| 0x48F130 | vt+0x10 `Tick` | 1 % chance a tick (`rand < 100`) to (re)start the ambient sound, then the puppet tick 0x4967E0. |
| 0x48F1C0 | vt+0xA0 | Hide or show: stops or restarts the ambient sound. |
| 0x48F290 / 0x48F2D0 | StreetLamp ctor / dtor | Base 0x489380. |
| 0x48F2E0 | StreetLamp vt+0x5C | Brightness from `OBJ+0xA1`. |
| 0x48F330 / 0x48F380 | Tree circle ctor / dtor | `+0xC8` = rand. |
| 0x48F390, 0x48F3B0, 0x48F3E0 | Tree circle | Handler and draw thunks. |
| 0x48F410 | Tree circle vt+0x14 | `srand(+0xC8)`, then 8, 4 or 2 trees for circle sizes 1, 2 and 3 (0xCF/0xD2, 0xD0/0xD3, 0xD1/0xD4). Each is a random C tree (0xD5+) or D tree (0xDC+) within radius `OBJ+0x44`. The spawner then removes itself. |
| 0x48F590 / 0x48F5D0 | Pod ctor / dtor | |
| 0x48F5F0 | Pod vt+0x5C | |
| 0x48F620 | Pod vt+0x14 | Lands the pod at a random spot (0x47B520), then spawns effect **0x3DA** twice (0x488070 / 0x4880F0, with sizes 300 and 300). |

### 2.7 Vehicles: generic ground vehicle 0x490820 and its derived classes

Vehicle fields beyond the base object:

| Field | Meaning |
|---|---|
| +0xE8 | formation or group helper (0x489290) |
| +0xEC | current route, ref-counted; nodes at +0x10, 16 bytes each, count at +0x14 |
| +0xF0 | waypoint index |
| +0xF4 / +0xF8 | target point |
| +0xFC | turn rate or desired heading delta |
| +0x100 | heading (int, 4096 a turn) |
| +0x104 | speed |
| +0x108 / +0x10C | velocity |
| +0x110 / +0x114 | avoidance sum |
| +0x118 | arrival radius = `radius * arrivalRadius - (-20)` |
| +0x11C | arrival slow-down |
| +0x120–+0x138 | neighbour sums for separation, velocity matching and attraction |
| +0x13A | random offset |
| +0x13C / +0x13E | occupancy cell |
| +0x140 | max turn per tick: 64 for move classes 0–4, 8 for classes 5–6, 16 for class 7, from `OBJ+0x52` |
| +0x142 | count in the acceleration buffer |
| +0x144 | neighbour count |
| +0x146 | tick phase (starts random) |
| +0x148.. | acceleration buffer (float pairs) |
| +0x188 | movement flags: bit 0 moving, 1, 5 halted, 7 class 7, 8, 9, 10/11 start and stop edges, 13 |
| +0x18C | byte: index of the bridge or special link object the vehicle is on (`dword_59D9C0[]`) |
| +0x190+ | subclass data |

The save chunk tag is **'VEHI'** (0x49484556, written by vt+0x34 in 0x490A40, loaded by vt+0x60 in 0x490A20).

| Address | Name | What it does |
|---|---|---|
| 0x490820 / 0x490A10 | Vehicle ctor / dtor | Clears all the fields above and sets them up from the OBJ move class. |
| 0x4909D0 | vt+0x144 `IsHalted` | |
| 0x490E70 | vt+0x14 `AddToWorld` | Resets movement. `flags = (flags & ~0x77E) | 0x801`, `+0xA8 = 2.0`. |
| 0x490F10 | vt+0x18 | Remove. Calls vt+0x13C and vt+0x114, removes the target and 0x4393D0, then 0x491020-related clean-up and 0x495830. |
| **0x490A70** | vt+0x1C `HandleMessage` | Handles the movement messages listed in the table after this one; anything else goes to the puppet handler 0x495CB0. |
| 0x4911B0 | `FollowRoute(spread, route)` | Takes the route (ref-counted) and starts at the waypoint nearest the vehicle's position. Sets the arrival radius, then posts 0x808B after a stagger worked out from the vehicle's position in its group (0x47EB00 group, fields +0x68/+0x6C) plus `rand%8`. |
| 0x491790 | `SetWaypoint(i)` | Target = node i, with its link byte at +0xC. Sets the arrival radius, smaller at the last node. |
| **0x4913B0** | vt+0x10 `Tick` | Checks halted (vt+0x144) and speed. On the stopped edge it calls vt+0x124 (stopped sound); on the started edge vt+0x120. Then vt+0x11C (steering), 0x491870 (waypoints) and 0x4967E0 (puppet tick). |
| **0x491870** | `AdvanceWaypoints` | Moves to the next node when inside the arrival radius. At the end of the route it notifies the group (0x482450), re-posts 0x808B, and sends **0x80B9** to the group. It also calls 0x448210 (sector stats), 0x43C750 (resources) and 0x440D10 (selection or UI refresh). This is where "Unit Arrived At Destination" comes from (*guess*). |
| 0x491020 | vt+0xC0 `HitByExplosion` | With a 6 % chance (`rand < 2000`) plays hit sound 0x110 / `OBJ+0xA0`, then 0x483880 (group under attack), then 0x4958B0. |
| 0x4910B0 / 0x491180 | vt+0x128 / +0xCC | Halt (`flags |= 0x20`) and resume (clears 0x20, sets bit 0), then 0x496440. |
| 0x4910C0 | vt+0xD0 | 0x496BC0, depending on the flags. |
| 0x491450 | vt+0x138 `UpdateEngineSound` | Plays the idle loop `OBJ+0xA2` (record 58) or the moving loop `OBJ+0xA4` (record 59) through 0x4883C0 / 0x4883E0. Handle at +0xE8 of the sound object. |
| 0x491510, 0x491530 | | Sound helpers. |
| 0x491570, 0x491590 | vt+0x120 / +0x124 | Start and stop edges: vt+0x114 or vt+0x110, then vt+0x138. |
| 0x4915B0 / 0x491630 | vt+0x110 / +0x114 | Add or remove the occupancy footprint, scaled by 1.25 (`dbl_4B01E0`). |
| 0x491720 | vt+0x98 | Returns `+0x18C`. |
| 0x491730 | vt+0x48 | 0x47F340, then the link object's vt+0xFC(this) to leave the bridge. |
| 0x491770 | vt+0x4C | Draw: 0x496560 and 0x47F480. |
| 0x491100 / 0x491110 | vt+0xC4 / +0xC8 | 0x496300 / 0x496200, height-aware. |

`0x490A70` handles these messages:

| Message | Effect |
|---|---|
| **0x80C2** | Move to a point: `lprc` holds x and y as two shorts. Drops the route, then posts **0x808B** after a delay. |
| **0x802E** | Follow a route object, through 0x4911B0. |
| **0x8031** | Set "moving". |
| **0x808B** | Start or stop: sets bit 0 of +0x188 and bit 13. |
| **0x808A** | vt+0x128 (halt). |
| **0x80B9** | Sets bit 9. |
| **0x80C3** | Face and drive toward (x, y) at speed 0.2: 0x43FDA0 / 0x43FE90, then vt+0x11C. |

#### Steering (vt+0x11C, 0x492F40) and the behaviours (vt+0x150..+0x16C)

0x492F40 runs every `vt+0x148()` ticks (`+0x146 % n`):

1. 0x492350 does the collision lookahead, checking `dword_4D05C4 = 4` steps of `collisionCheckDist * speed` (at least 1.0) along three probe lines (0x492950) against terrain (0x480680) and objects (0x47FE10). Move classes 5 and 6 (water) stop there. On contact with another group's unit it calls vt+0x14C on both and repositions them. One time in four (`rand < 0x2000`) it spawns the type's crush or dust effect `OBJ+0x98` (record 60) and sets `OBJ+0xAC`. It returns "blocked".
2. If blocked: vt+0x158 (`attribute`, obstacle push).
3. 0x492C00 / 0x492A90 gather neighbours from the spatial grid (0x449920). Mines 0x273–0x276 are ignored. The results go into the +0x120..+0x144 sums.
4. The remaining behaviours, gated by the halted flag and the bridge byte:
   - vt+0x168 arrival
   - vt+0x154 separation
   - vt+0x150 target
   - vt+0x15C avoidance
   - vt+0x160 braking
   - vt+0x16C velocity matching
   - vt+0x164 attraction to the route, only when there is a route
5. Each behaviour pushes `weight * clamp(force, max)` with 0x491B20. The sum is accumulated in priority order up to the **maxAcc** budget (Reynolds' prioritised acceleration allocation).
6. The sum is added to the velocity. Speed control (0x492DE0) uses `speedCostDivisor` and the terrain cost (0x480680), and the slow-down near the goal is `(1 - 0.625*sqrt(..))*radius`.
7. Heading change is clamped to `min(+0x140*speed, 8 - 120*maxAcc*speed)`. Units whose `OBJ+0x00 == 0x13` drive **in reverse** when the turn is over 90° (`|Δ| > 0x400`, heading + 0x800).
8. The velocity is built back with 0x43FE90, then vt+0x54 (move), the group update 0x489290, and vt+0x30 (set the model heading).
9. Bridge handling: when the waypoint's link byte is set, it calls `dword_59D9C0[link]->vt+0x100(this)`. If the link object is broken (`+0xEC == 0`) and the vehicle is in water (0x47FFE0), it posts **0x80A0** (dies) and sends **0x8031** to its group.
10. vt+0x130 (animation: walker legs) and vt+0x134 (wake or dust trail, 0x4934F0).

| Behaviour | Function | ini weight / max |
|---|---|---|
| target (seek) | 0x491BB0 | `targAccWeight 0.003` / `targAccMax 0.15` |
| braking | 0x491C70 | `brakingAccWeight 0.1` / `brakingAccMax 0.2` |
| avoidance | 0x491D90 | `avoidanceAccWeight 0.15` / `avoidanceAccMax 0.1` |
| attribute (obstacle) | 0x491EA0 | `attributeAccWeight 0.3` / `attributeAccMax 0.2` |
| arrival | 0x491FA0 | `arrivalAccWeight 0.33` / `arrivalAccMax 0.2` |
| separation | 0x4920A0 | `separationAccWeight 0.2` / `separationAccMax 0.1` |
| velocity matching | 0x492160 | `velMatchAccWeight 0.08` / `velMatchAccMax 0.03` |
| attraction to the route | 0x492240 | `attractAccWeight 0.001` / `attractAccMax 0.1` |
| total budget | all | `maxAcc 1.0` |

Other vehicle functions:

| Address | Name | What it does |
|---|---|---|
| 0x491B20 | `AddAcceleration` | Writes to +0x148[+0x142]. |
| 0x492950 | probe point | |
| 0x492DB0 | vt+0x12C `MaxSpeed` | `OBJ+0x54 * 0.02`. `OBJ+0x54` is the same word the war map uses as its speed. |
| 0x4929A0 | vt+0xBC `CanUseLink(obj)` | Bridges 0x1BC–0x1BF, 0x1C0 and 0x1CC–0x1CE are allowed by move class (`OBJ+0x52`, bits 0x20 and 0x30). Otherwise vt+0xD8. |
| 0x4934F0 | vt+0x134 trail | For move classes other than 5 and 6, when speed > 8, spawns **0x3C8** (dust or wake) behind the vehicle. |
| 0x493490 | vt+0x130 | Wobble or bob of the model through `flt_4F0538`. |
| 0x4935C0 | vt+0xA0 | 0x495820, then vt+0x138. |

#### Vehicle subclasses

| Class | Functions |
|---|---|
| **Gun emplacement** 0x48F750 / 0x48F7B0 | 0x48F780 alloc/dtor (with `+0xB8` bit). 0x48F7C0 vt+0x14: owner checks `OBJ+0x10 == 1`, then 0x490E70. 0x48F890 thunk to the tick. 0x48F8A0 vt+0x104: finds units within `OBJ+0x48 * [Tweak] "Audible radius multiplier"` (0x43FF00) and sends **0x805A** (heard firing, alert *guess*), or 0x480EB0, then 0x496890. 0x48F940 vt+0x40: 0x47F320 and SetPosition. |
| **Drone** 0x48F970 / 0x48F9C0 | 0x48F9D0 vt+0x14. 0x48F9F0 vt+0xBC: on reaching its target (+0x190) within 64 units (`flt_4AFEA0`) posts 0x80A0, which makes it **explode on contact**. 0x48FAC0 vt+0x10: the same check against the order target, then the tick. 0x48FA80, 0x48FAA0 height overrides. 0x48FB30 steering every tick. 0x48FB40 returns 1.0. 0x48FB50 returns 150. |
| **Tripod / walker** 0x48FC20 / 0x48FC60 | 0x48FC70 vt+0x14. 0x48FCA0 vt+0x18. 0x490040 vt+0x11C: 0x48FDD0 footprints, then steering, then 0x490070. 0x490070 vt+0x130: leg animation rate from speed, `turnAnimRate 0.02`, `startTurnRate 12`. 0x48FDD0: per type, the puppet joints that leave footprints (Scout 1–3, Fighting 6/9, Bombarding 9–14, Scanning 8–11, Constrictor 9–12…). 0x48FCB0 spawns footprint object **0x3C7** at the joint's ground cell when the terrain type (landscape +0x64 cell byte +8) is between 1 and 4. 0x490030 draw. |
| **Flier** 0x490120 / 0x490160 | 0x490170 vt+0x11C: steering, then holds altitude above the ground (0x47B120) and bobs with 0.8, 4.0, 0.01 and 5e-5, scaled by speed `OBJ+0x54`. |
| **Ironclad** 0x493870 / 0x4938B0 | 0x4938C0 vt+0x14. 0x4938D0 halted. Weaker braking, avoidance, arrival and separation (0x4938E0 and 0x4939A0 scale by 0.5 and 0.25; 0x493A40, 0x493AD0). 0x493B40 returns 1.0. 0x493B50 vt+0x10: random 0x80C1. |
| **Submersible** 0x4935E0 / 0x493630 | 0x493600 alloc and dtor. 0x493640 vt+0x10: random dive, via 0x498F20. 0x493680 vt+0x170: **dive** splash 0x3DC plus sound 0x1B9. 0x493700 and 0x4937C0 scaled braking and avoidance. 0x493860 returns 1.0. More in 0x498Dxx–0x4993xx (next piece). |
| **Tunnelling Track Layer** 0x493B90 / 0x493BF0 | 0x493BC0 alloc and dtor. 0x493C00 vt+0x14. 0x493C10. 0x493C20 vt+0x10: burrow and surface logic via 0x498F20 and 0x47FFE0. 0x493C80 vt+0x170: **burrow** dust 0x3DE plus sound 0x1B8. 0x493D00 vt+0x12C: speed `OBJ+0x54`, multiplied by 50/65 (`flt_4B0880` / `flt_4B0884`) when research item `dword_588CC8` is done (0x45B890). 0x493D70 vt+0x174: mound sprite GType **0x35A**. |

### 2.8 WRouteFinder (0x493DF0–0x494960)

This is the battle map's route finder (ENGINE-MAP: "WRouteFinder 0x493DF0–0x494370"). It is set up when a map loads (WLandscape 0x47AF10).

| Address | Name | What it does |
|---|---|---|
| 0x493DF0 | `Setup` | Lays a node grid every **4** landscape cells (`+0x38 = 4`): `nx = (W-4)/4`, `ny = (H-4)/4` (land +0x24/+0x28). It allocates through 0x4148A0 with tag 0x100: `connDat` (40 bytes a node), `connList` (4 bytes a node + 0x1000), `nodeXCMem` and `nodeYCMem` (word coordinates, plus 0x400 spare nodes), and `nodeFMem` (a flag byte each, plus 0x400). A node's coordinate is `i*4 + 4`. It then builds the 8-neighbour connections, checking passability. Errors: "WRouteFinder::Setup: No memory for conn…/node…". |
| 0x4941A0 | `UnSetup` | Frees the five blocks (0x414BC0). Debug tags "WRouteFinder::UnSetup:1".."5". |
| 0x494220 | `InsertNode(x, y, …)` | Adds a spare node (at most 0x400, "insertNode: out of extra node space") and connects it. Used by bridges (0x4983D0, the bridge vt+0x104) to add their crossings. |
| 0x494370 | `RemoveNode` | "WRouteFinder::RemoveNode". Called by 0x486850 and 0x498CE0. |
| 0x494400 | `ClearFlags` / reset | Called by 0x486820 and 0x498CB0. |
| 0x494440 | `RequestRoute(...)` | Called from the battle map's group movement 0x482E60. It frees any old route, then makes a route object (0x24 bytes, ctor 0x499420). If start and goal are close (0x43FED0 distance), the route is direct (0x4994B0). Otherwise it creates an **AStar** search (0xC4 bytes, 0x44EE70, the same A* the war map uses) over this node graph. |
| 0x494690 | `BuildRouteNodes` | Called from 0x483270. Turns the A* result into the route's 16-byte waypoint nodes (x, y, …, link byte at +0xC) through 0x4994B0 and 0x499850. |
| 0x494960 | `FreeSearch` | Called from 0x482E60, 0x483270, 0x4833F0 and 0x483B70. |

### 2.9 Projectiles and weapon shots (base 0x4949A0, vtable 0x4B0890)

These are used by object types 0x19A–0x19C (0x48DBE0), 939, 697–699 and 703–708 (built outside the piece by 0x488460, 0x4887E0 and 0x488930).

| Address | Name | What it does |
|---|---|---|
| 0x4949A0 / 0x494A60 | ctor / dtor | Base 0x47D6F0. Damage multiplier +0xC8 = 1.0, multiplier +0xCC = 1.0, state at +0xD0..+0xF0. The dtor stops sound +0xDC (0x464E30). |
| 0x494AF0 | vt+0x14 `Launch(arg)` | Adds itself to the world list, then 0x47EF30. If `OBJ+0x28` (record 24) is set, calls vt+0xE0. **Research upgrades**: for Human shooters (owner side 0, from the owner vt+0xDC), research items `dword_588D78` / `588D7C` / `588D88` set +0xC8 / +0xCC to `dword_4CA18C` / `4CA190` / `4CA19C`. For Martian shooters (side 1), types 0x1AB–0x1B0 use items `589054` / `589058` (values `4CA350` / `4CA354`), and all of them use `588FA4` (`4CA358`) and `589060` (`4CA360`). |
| 0x494C50 | vt+0x18 | Remove (0x40EBE0, 0x47F260). |
| 0x494CB0 | vt+0xB0 | Returns `word_4F2F10[type*16]` (speed). |
| 0x494CC0 | vt+0x1C | Handler: vt+0xC8 / vt+0xCC. |
| 0x494D20, 0x494D80 | vt+0xC8 / +0xCC | |
| 0x494DB0 | vt+0x10 `Tick` | A five-state machine on `+0xD0`: 0 wait, 1 flight time `rand(word_4F2F12)` plus a start sound `OBJ+0xA2`, 2, 3 owner vt+0xD0, 4 `rand(word_4F2F14)`. It calls 0x494FD0. |
| 0x494FD0 | `Steer` | Toward the target (0x43FDA0, 0x43FF30), turn-limited by `word_4F2F10`. |
| 0x4950D0 | vt+0xE0 | Creates the render type (0x49D710, 0x50 bytes) with alpha `OBJ+0xA1`. |
| 0x495180 | vt+0x4C | Positions the render (0x49D7E0) with -4 and 16, and `OBJ+0x2C`/`+0x28` frames. |
| 0x495280 | vt+0xE4 | Trail or impact drawing (0x49D820) with `word_4F2F1A`/`1C` and 0.25. Also used by 0x48DC30. |
| 0x48DBE0 / 0x48DC20 / 0x48DC30 / 0x48DC60 | 0x19A–0x19C class | ctor, dtor, vt+0xE4 (0x495280) and vt+0x14 (0x494AF0 with `OBJ+0x28`). |

The **projectile table at 0x4F2F10** comes from OBJ.ojd **record 12** (0x437C87): 8 words per object type. +0 is the speed or turn, +2 and +4 are random times, +0xA and +0xC are trail parameters, and the rest are unknown.

### 2.10 Settings registered here (static initialisers)

All are `0x412160` (int), `0x416500` (float) or `0x411540` (bool) setting objects. The value is at the object + 0x14, and the registry or INI is read through 0x4166B0.

| Init | Section / key | Default | Value address | Used by |
|---|---|---|---|---|
| 0x48FB60 | Vehicles / startTurnRate | 12 | 0x59E1BC | 0x490070 |
| 0x48FBA0 | Vehicles / stopTurnRate | 8 | 0x59E18C | (no reader found) |
| 0x48FBE0 | Vehicles / turnAnimRate | 0.02 | 0x59E1A4 | 0x490070 |
| 0x490260 | Vehicles / maxAcc | 1.0 | 0x59E33C | all the behaviours |
| 0x4902A0 | Vehicles / collisionCheckDist | 0.5 | 0x59E1D4 | 0x492350 |
| 0x4902E0 / 0x490320 | attributeAccWeight / attributeAccMax | 0.3 / 0.2 | 0x59E2C4 / 0x59E27C | 0x491EA0 |
| 0x490360 / 0x4903A0 | separationAccWeight / separationAccMax | 0.2 / 0.1 | 0x59E3B4 / 0x59E324 | 0x4920A0, 0x493AD0 |
| 0x4903E0 / 0x490420 | avoidanceAccWeight / avoidanceAccMax | 0.15 / 0.1 | 0x59E2AC / 0x59E1EC | 0x491D90 |
| 0x490460 / 0x4904A0 | brakingAccWeight / brakingAccMax | 0.1 / 0.2 | 0x59E39C / 0x59E264 | 0x491C70 |
| 0x4904E0 / 0x490520 | velMatchAccWeight / velMatchAccMax | 0.08 / 0.03 | 0x59E24C / 0x59E204 | 0x492160 |
| 0x490560 / 0x4905A0 | targAccWeight / targAccMax | 0.003 / 0.15 | 0x59E30C / 0x59E234 | 0x491BB0 |
| 0x4905E0 / 0x490620 | attractAccWeight / attractAccMax | 0.001 / 0.1 | 0x59E36C / 0x59E2F4 | 0x492240 |
| 0x490660 / 0x4906A0 | arrivalAccWeight / arrivalAccMax | 0.33 / 0.2 | 0x59E414 / 0x59E3CC | 0x491FA0 |
| 0x4906E0 | Vehicles / arrivalRadius | 1.5 | 0x59E294 | 0x490820, 0x490A70, 0x491790 |
| 0x490720 | Vehicles / speedCostDivisor | 25.0 | 0x59E2DC | 0x492DE0 |
| 0x490760 / 0x4907A0 / 0x4907E0 | Debug / Show Accelerations, Show Radii, Show Arrival | false | 0x59E3FC / 0x59E21C / 0x59E354 | debug overlays (no readers in this piece) |
| 0x4916A0 / 0x4916E0 | Fun / Veh Speed, Veh Rot Speed | 1 / 32 | 0x59E384 / 0x59E3E4 | (no readers here) |
| 0x4955E0 | Stats / Puppets objects left | 0 | 0x59E464 | ++ in 0x495660 |
| 0x495620 | Tweak / Audible radius multiplier | 1.0 | 0x59E44C | 0x48F8A0 |

Settings used here but registered elsewhere:

- `[BattleMap] "Fog darkness"` (0x4EA5FC) and `"Fog update freq"` (0x4EA614), set up at 0x4344C0.
- `[Options] "Show lights"` (0x4B8808), `"Show Smoke & particles"` (0x4B880C) and `"Show vehicle shadows"` (0x4B8814), from 0x4023C0..0x402480. Their menu checkboxes are at 0x409930 with command 0x8120.
- `Game Frequency` (0x4B84C8).

## 3. Data structures and globals

### Battle object (common to all classes; offsets from `this`)

| Field | Meaning |
|---|---|
| +0x00 | vtable |
| +0x04 | ref count (word) |
| +0x08 | id |
| +0x10 | damaged-list link |
| +0x18 | active |
| +0x20 | **object type** (index into the OBJ table) |
| +0x24 | **byte: enemy or not the local player's** (0 = ours) |
| +0x28 | status puppet |
| +0x2C / +0x30 | screen x / y |
| +0x34 | group or animation data (`+0x1D0` read) |
| +0x3C | selectable |
| +0x44 | parent or owner object |
| +0x48 | group or owner |
| +0x4C | model or render object |
| +0x50 | frame data (0x4216D0 frame count) |
| +0x70 | list of children: puppets and segments |
| +0x78 / +0x7C / +0x80 | world x / y / z (floats) |
| +0x84 | height |
| +0x88 / +0x8C | velocity |
| +0xA0 | height offset |
| +0xA8 | speed scale |
| +0xAC | radius (int) |
| +0xB8 | flags: bit 0 large or selectable, 1 occupies cells, 2 no cells, 4 visible, 7 damageable, 0x800 |
| +0xBC | spatial grid cell (word, -1 = none) |
| +0xC0 | flags: bit 0, bit 2 linked, bit 3 hit this tick |
| +0xC4 | **health** |
| +0xCA | explosion power (word) |
| +0xDC | owner (shots) |

The vtable layout (from vehicle vtable 0x4B0258) is listed in §2.1 and §2.7. The key slots: +0x10 Tick, +0x14 AddToWorld, +0x18 Remove, +0x1C HandleMessage, +0x30 SetHeading, +0x34 Save, +0x40 SetPosition, +0x4C Draw, +0x54 Move, +0x58 FogVisibility, +0x5C ApplyFog, +0x60 Load, +0x68 DrawMiniMap, +0x78 OnDestroyed, +0x9C IsDead, +0xA0 Hide, +0xBC CanUseLink, +0xC0 HitByExplosion, +0xDC Owner, +0xE0 TakeDamage, +0xE4/+0xE8 occupancy, +0xF0 IsTargetable, +0xF4 HealthFraction, +0x100 RevealFog; for vehicles +0x110..+0x16C as in §2.7.

### OBJ.ojd unit table 0x51A0F0 (0xB0 bytes a type): fields read in this piece

The record numbers come from the loader 0x437750 (cases listed in brackets).

| Offset | Record | Use here |
|---|---|---|
| +0x00 | 15 | Class; 0x13 means it may reverse. |
| +0x10 | 15 | Compared to 1: owner and emplacement checks (48AC50, 48F7C0). |
| +0x28 | 24 | Projectile flag or frame count. |
| +0x2C | 23 | Counter frame (war map); here an animation frame. |
| +0x40 | 30 | Status puppet size. |
| +0x41 | 30 | Effect height: offset `-1.414 × value`. |
| +0x42 | 18 / 28 | Building damage. |
| +0x44 | 30 | Scatter radius for tree circles. |
| +0x46 | 30 / 31 | Footprint in cells. |
| +0x48 | 32 | **Sight radius**, for the fog and for audible alerts. |
| +0x50 | 33 | Building flatten size. |
| +0x52 | 37 | **Move class**: 0–4 land, 5–6 water, 7 special; sets the turn limit; bits 0x20/0x30 control bridges. |
| +0x54 | 37 / 38 | **Speed**: battle `× 0.02`, war map `× 0.05`. |
| +0x90.. | 52–54 | |
| +0x98 | 60 | Collision dust effect type. |
| +0xA0 | 61 | Hit sound or flag. |
| +0xA1 | 62 | Alpha or brightness. |
| +0xA2 | 58 | Ambient / idle loop sound. |
| +0xA4 | 59 | Moving loop sound. |
| +0xAC | | Collision effect flag (written). |

### Other tables and globals

| Address | What it is |
|---|---|
| 0x4F2F10 | Projectile table, 16 bytes a type (OBJ record 12). |
| 0x4EC598 | Light and effect timing, 16 bytes a type (OBJ record 13). |
| 0x4D2550 | The battle world. +0x20 world and damaged lists, +0x84 fog of war, +0x90 render list, +0x118 spatial grid. |
| 0x4D2558 | Tick counter. |
| 0x59D940 | WLandscape. +0x24/+0x28 size in cells, +0x30 cell size (16.16), +0x34 rounding, +0x48 screen origin, +0x64 cell info (10 bytes, terrain type at +8). |
| 0x59D9C0 | Table of link objects (bridges), built at 0x439770. |
| 0x4E9B70 | Selection manager (+0x114 count). |
| 0x4E9B74 / 0x4E9B78 | Human and Martian player objects (receive 0x807B). |
| 0x4E9B7C | Battle settings (+0x54). |
| 0x4E9B94 | "Vehicles destroyed" flag. |
| 0x4E9250 | SoundFX system. |
| 0x4DAF7C–0x4DAFC4 | Palette colour indices: black, grey, red, green… (0x40B120). |
| 0x4DA768 / 0x4DA770 | Daylight and darkness factors (time of day 0x40B030). |
| 0x4D1249 | Cheat: all targetable. |
| 0x59E170 | Load counter. |
| 0x59E464 | Puppet count. |
| 0x59E468 | WPuppet free list. |
| 0x59E41C | Route finder instance. |
| 0x4D05C4 | Number of collision probe steps (4). |
| 0x4D11F8 | Memory heap. |

## 4. Messages, commands, strings and constants

Messages handled or sent in this piece. The names are guesses from use.

| Id | Meaning | Where |
|---|---|---|
| 0x802E | Follow route (arg = route object) | 0x490A70 → 0x4911B0 |
| 0x8031 | Set moving; sent to the group when a vehicle drowns | 0x490A70, 0x492F40 |
| 0x8057 | Effect finished | 0x48E100 |
| 0x805A | Alert or heard firing | 0x48F8A0 |
| 0x8066 | Lit by, or affected by, an effect | 0x48E100, 0x48DF50 |
| 0x8077 | Minimap dot (colour, packed x, y) | 0x48C870 |
| 0x807B | Structure destroyed, sent to both players | 0x48D590, 0x48DA40 |
| 0x8088 | Select group | 0x48A530 |
| 0x808A | Halt | 0x490A70 |
| 0x808B | Start or stop moving (self timer) | 0x490A70, 0x4911B0, 0x491870 |
| 0x80A0 | **Die**, posted with a delay | 0x48AB20, 0x48B530, 0x48D9C0, 0x48F9F0, 0x48FAC0, 0x492F40 |
| 0x80A5 / 0x80A6 | Bridge pieces | 0x48D0A0 |
| 0x80AA | Key building destroyed (ends the battle) | 0x48AC50 |
| 0x80B9 | Arrived (to the group) | 0x490A70, 0x491870 |
| 0x80C1 | Under attack (to the owner's panel) | 0x48AEF0, 0x493B50 |
| 0x80C2 | Move to point | 0x490A70 |
| 0x80C3 | Drive toward a point | 0x490A70 |
| 0x80F8 | Mine triggered | 0x48C9B0 |
| 0x8125 / 0x8126 | Show / hide child puppets | 0x48A660, 0x48A780, 0x48B2D0 |
| 0x812F | Object destroyed, to the owner list | 0x48AD40 |
| 0x8136 | Destroy all vehicles (battle lost) | 0x48AC50 |

**Sounds**: 0x110 (vehicle hit), 0x111 (building hit), 0x1B8 (tunneller burrows), 0x1B9 (submersible dives), and per-type loops `OBJ+0xA2`/`+0xA4`.

**Object types spawned**:

| Type | What it is |
|---|---|
| 0x16F–0x172 | Damage smoke |
| 0x3E8 | Rare extra smoke or particle |
| 0x17B | Building corner and fence spark objects |
| 0x252–0x255 | Death explosions |
| 0x3C7 | Tripod footprints |
| 0x3C8 | Vehicle trail |
| 0x3DA | Pod impact |
| 0x3DC | Dive splash |
| 0x3DE | Burrow dust |
| 0xD5..0xE3 | Trees |

**GTypes**: 0x359 (mine sprite) and 0x35A (tunneller mound).

**Strings**:

- "DESTROYING VEHICLES" (debug).
- The WRouteFinder errors and tags: `connDat`, `connList`, `nodeXCMem`, `nodeYCMem`, `nodeFMem`, `newSNode`, "insertNode: out of extra node space", "WRouteFinder::RemoveNode", "WRouteFinder::UnSetup:1-5".
- The ini keys in §2.10.

The piece uses no TEXT.ojd strings (sub_4146F0 is not called).

**Constants**:

| Constant | Value |
|---|---|
| Damage-smoke check | Every 15 ticks |
| Smoke appears below | 40 % health; level `5 - 5f` |
| Die delay | 10–29 ticks |
| Heading units | 4096 a turn |
| Max turn per tick | 64, 8 or 16 by move class |
| Arrival radius offset | -20 |
| Effect height factor | 1.414 |
| Collision probes | 4 |
| Collision effect chance | 25 % |
| Hit sound chance | 6 % |
| Building sound restart chance | 1 % a tick |
| Building hit sound chance | 1/64 |
| Route grid step | 4 cells |
| Spare route nodes | 1024 |
| Tunneller research speed factor | 50/65 |
| Drone detonation range | 64 |
| Shot speed multiplier | `× 0.02` |

## 5. Cross-references to other pieces

**Callers into this piece:**

| Caller | Calls |
|---|---|
| Object factory 0x446B30 | All the ctors |
| Base object handler 0x489FA0 | 0x48A4C0, 0x48A530 |
| Puppets | 0x4958B0 → 0x48A5A0; 0x4967E0 → 0x48AEF0; 0x495740 → 0x48A780, 0x48A8B0; 0x496560 → 0x48B3D0 |
| Battle map 0x4865A0 | 0x48A7D0, 0x48A8B0 |
| Battle map group movement and routing 0x482E60 / 0x483270 / 0x4833F0 / 0x483B70 | 0x494440, 0x494690, 0x494960 |
| WLandscape 0x47AF10 | 0x493DF0 (route finder set-up) |
| Map unload (destructor chain 0x4941A0 via `std::ios_base` xref) | 0x4941A0 |
| Bridges 0x4983D0, 0x498CB0, 0x498CE0 and 0x486820 / 0x486850 | Node insert and remove |
| Shot types 0x488460, 0x4887E0, 0x488930 | 0x4949A0 |
| 0x4884C0, 0x4885C0, 0x4885F0, 0x488570 | Projectile methods |
| Underwater and underground base 0x498D40 and its overrides 0x498DB0–0x4993A0 | 0x490E70, 0x490F10, 0x491020, 0x4913B0 |
| 0x418500 (war map) | 0x48B580 (`return false`) |
| 0x47D240 | 0x48AA30 |

**Calls out of this piece:**

| Area | Functions |
|---|---|
| Messaging (WCmdTarget) | 0x4355A0 post with delay, 0x435620 cancel, 0x435590 base tick, 0x4356A0 |
| World lists | 0x440060 |
| Side and class of an object | 0x4406E0, 0x440700 |
| Max health | 0x47DA20 |
| Group | 0x47EB00 |
| Maths | 0x43FDA0, 0x43FE90, 0x43FF00, 0x43FF30, 0x43FED0 |
| Terrain | 0x47B120, 0x47B170, 0x47B520, 0x480680 passability or cost |
| Occupancy | 0x47C280, 0x47C3A0 |
| Collisions | 0x47FC50, 0x47FE10 |
| Fog | 0x434A80 |
| Spatial grid | 0x449850–0x449920 |
| Research | 0x45B890 |
| Sound | 0x464E30, 0x464EE0, 0x465590, 0x4883C0, 0x4883E0 |
| Rendering | 0x40E700, 0x40EC50, 0x40EBE0, 0x4A21A0 lines, 0x4A27A0–0x4A28E0 sprite scale, 0x49D710–0x49D820 shot render, 0x47C900 |
| Effects | 0x488070, 0x4880F0, 0x488400 |
| Resources | 0x43C750 |
| War map sector record | 0x42A0B0 |
| Selection | 0x440680, 0x440D10 |
| Group arrived / under attack | 0x482450, 0x483880 |
| Sector stats | 0x448210 |
| AStar | 0x44EE70 |
| Route objects | 0x499420, 0x4994B0, 0x499850 |
| Memory | 0x4148A0, 0x414BC0 |
| Fatal error | 0x4164A0 |

## 6. Missing in WoWEngine: candidate tasks (prioritised)

WoWEngine has **no battle map at all** today. Its `src/` holds only core formats, the front end, the installer, the launcher and the war map. `Models.h` reads WOF, IOB and CLS but says "drawing them belongs to the battle map". Everything in this piece is therefore missing. The tasks, in a sensible build order:

1. **Battle object model and factory.** Port the object type → class mapping (0x446B30 switch, the table in §1) and the common object layout (§3). Use virtual methods matching the vtable slots: Tick, AddToWorld, Remove, HandleMessage, SetPosition, TakeDamage, HitByExplosion, OnDestroyed, Draw, DrawMiniMap, RevealFog, FogVisibility. The OBJ.ojd loader (`src/core/ObjOjd.cpp`) needs extending to the fields in §3: records 12, 13, 18, 24, 30–33, 37 and 58–62, plus the 0x4F2F10 and 0x4EC598 tables.
2. **Vehicle steering.** 0x492F40 with the eight behaviours, 0x491B20, 0x492350, 0x492C00/0x492A90 and 0x492DE0. Read the `[Vehicles]` ini values with the defaults in §2.10, and port the turn clamp (move-class table in the ctor 0x490820), reversing for class 0x13 and the bridge link handling. Copy the order and arithmetic exactly, because unit movement feel depends on it.
3. **Movement orders and waypoint following.** Messages 0x80C2, 0x802E, 0x808B and 0x808A (0x490A70), staggered starts (0x4911B0), waypoint advance and the arrival events (0x491870, 0x80B9), and the start/stop engine sounds (0x491450, `OBJ+0xA2`/`+0xA4`).
4. **WRouteFinder.** The 4-cell node grid (0x493DF0), insert and remove nodes for bridges (0x494220 / 0x494370), route requests (0x494440) through the AStar already understood for the war map (0x44EE70 / 0x44EFA0), and route node building (0x494690).
5. **Damage model.** TakeDamage with delayed death (0x48AB20), octagonal-distance splash damage (0x48A5A0), the damage picture frame (0x48A7D0), damage smoke gated by `Show Smoke & particles` (0x48AEF0, 0x48ADD0), and the cascading death explosions (0x48CDE0).
6. **Win and lose conditions.** 0x48AC50: destroying the Martian Base or Communications Centre (battle kind 3) or Parliament (kind 0x1E) sends 0x80AA, and 0x8136 destroys all vehicles. These must hook into the battle result and back to the war map.
7. **Fog of war on objects.** Corner sampling (0x48B050), showing and hiding enemy units with the status puppet (0x48B2D0, 0x48A660/0x48A780, `Fog darkness`), and periodic reveal by own units (0x48C840/0x48C570, `Fog update freq`, sight `OBJ+0x48`).
8. **Selection.** Box select (0x48A4C0) and group select (0x48A530); own units only.
9. **Minimap blips.** 0x48C870 (0x8077, side colours 0x4DAFAC/0x4DAFB0) and the per-class vt+0x68 overrides (fences 0x48DB80).
10. **Unit subclasses:**
    - tripod leg animation and footprints (0x490070, 0x48FDD0/0x48FCB0)
    - flier altitude (0x490170)
    - drone detonation (0x48F9F0/0x48FAC0)
    - submersible dive and tunneller burrow, with effects and sounds (0x493680, 0x493C80, 0x493D00 research speed, 0x493D70); the rest is in 0x498Dxx
    - Ironclad scaled behaviours (0x4938E0–0x493AD0)
    - emplacement alerts (0x48F8A0, `Audible radius multiplier`)
11. **Projectiles.** Base 0x4949A0: research damage multipliers (0x494AF0), flight state machine (0x494DB0/0x494FD0) and rendering (0x4950D0–0x495280).
12. **Static world objects.** Buildings (0x48E9D0 terrain flattening, corner objects, ambient sounds 0x48F130), bridges (0x48D2F0/0x48D5F0 placement, 0x48D590 destroy messages to both players and the war map via 0x42A0B0), fences (0x48D800 segment building, 0x48D9C0 death propagation), tree circles (0x48F410: seeded scatter of 8, 4 or 2 trees), pods (0x48F620), street lamps, mines (0x48C8E0 class, trigger 0x80F8, visibility to the owner, GType 0x359), and CP markers (0x48B480).
13. **Visual options.** `Show lights` (lights 0x48DD50 class with the timing table 0x4EC598 and time-of-day chance), `Show vehicle shadows` (0x48E680 class) and `Show Smoke & particles`. The Display Settings menu (0x409930, command 0x8120) already exists in the front end. Check that these keys are persisted and passed through to the battle map.
14. **Debug and Fun settings** (`Show Accelerations`, `Show Radii`, `Show Arrival`, `Veh Speed`, `Veh Rot Speed`). They are low priority, and nothing reads them in this piece. Implement them only if a reader turns up elsewhere.
15. **Unresolved, needing follow-up:**
    - the identity of types 0x128, 0x1D0–0x1D3 and 0x2B6/0x2B7 (the line-linked class 0x48B590)
    - type 0x1C0
    - decal types 0x14B–0x14D
    - shot types 0x19A–0x19C, 697–708 and 939
    - what messages 0x8066 and 0x805A do on the receiving side
