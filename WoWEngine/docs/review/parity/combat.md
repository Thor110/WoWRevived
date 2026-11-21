# Parity review: combat, weapons and effects (battle map)

Scope: src/battlemap/Weapons.cpp, Combat.cpp, Effects.cpp, Sinking.cpp, the beams/turrets/lights in BattleMap.cpp.
Original routines read in full: 0x487F50, 0x488070, 0x488180, 0x488200, 0x488350, 0x4886A0, 0x488830, 0x4889D0,
0x488CA0, 0x488D20, 0x488D70, 0x488F00, 0x487890, 0x489050, 0x40EF20, 0x487800, 0x489000, 0x488FB0, 0x48DCB0,
0x48DD30, 0x47F050, 0x47F770, 0x47D6F0 (flags), 0x488E80, 0x48A5A0, 0x48AB20, 0x4958B0, 0x491020, 0x48F030,
0x489FA0, 0x47DF20, 0x47EAA0, 0x47F220, 0x48EF80, 0x48A7D0, 0x47FBA0/0x47FC50, 0x487B50, 0x48CDE0, 0x494DB0,
0x494FD0, 0x494D20, 0x494AF0, 0x496200, 0x491110, 0x489B40, 0x43FF30, 0x495180, 0x4884C0/0x4885C0/0x4885F0,
0x48DD50, 0x48DDC0, 0x48DE80, 0x48E050, 0x48E100, 0x48AEF0, 0x48ADD0, 0x4967E0, 0x496890, 0x496570, 0x48F8A0,
0x4826D0, 0x498F20, 0x493640, 0x493C20, 0x495BB0, 0x416500, 0x495620, 0x440060/0x4393B0, 0x489170.
Engine line numbers are as of this review.

---------------------------------------------------------------------------------------------------------------------

## Visible bugs

### 1. Units never pick targets on their own; emplacements keep standing their guns down (HIGH)
- Original: 0x495620 builds a [Tweak] "Audible radius multiplier" float object at 0x59E438 (default 1.0). 0x416500
  stores the value at +0x14 (`fst [esi+14h]`), so the value lives at **0x59E44C**. That is `flt_59E44C`, which
  0x496570 (look round, `fmul flt_59E44C` at 0x4965A3) and 0x48F8A0 (emplacement, 0x48F8CC) multiply the sight
  (word_51A138, type +0x48) by. It is not 0. Nothing else writes it because the tweak object's constructor and
  ini loader fill it.
- Engine: Weapons.cpp:458 `static constexpr float kLookScale = 0.0f;`, used at :479 and :517. With sight x 0 =
  0, AutoTarget (:513) never finds anything. LookRound (:478) stands down an emplacement's whole group on every
  look round whenever its aim point is any distance from it.
- What the player sees: units on stances 2 and 3 never engage enemies that come into sight. Defensive buildings
  and emplacements stop firing every field92+1 (about 91) updates and fire only when a target is re-pushed.
- Fix: read [Tweak] "Audible radius multiplier" (default 1.0) into setup and use it in place of kLookScale.

### 2. Shots that land high use the ground explosion instead of the air-burst table (HIGH)
- Original 0x487F50: `cmp ebp(arg_C = the height), 40h; jle` picks between land and water, otherwise eax=0, which
  is the table at 0x4D0450 (393, 394, 552-589 ...). So the **height over the ground > 64** selects the air-burst
  table. The size only indexes the table (`[eax+edx*4-4]`).
- Engine: Weapons.cpp:1006 `if (size > 64) effect = ... kImpactBig[size - 1]`. It tests the size instead of the
  height. Hits on tall targets (Fighting Machines and other aircraft-height aim points, with z = ground + lift +
  size41 x 1.414) and mortar shells that time out in the air show land explosions. The water/land check is used
  instead.
- Fix: `if (h > 64) effect = kImpactBig[size-1]; else if terrain 1-3 water else land;` (and keep Explode's z =
  max(h, 0)).

### 3. A building's lost part blows up its own building and group (HIGH)
- Original 0x489FA0 case 0x80A0: it calls 0x488070 with arg_10 (the explosion's +0xCC firer) = the parent's id
  (`mov eax,[esi+44h]; mov ebx,[eax+8]`) and arg_14 (+0xC4) = the dying object's type. Detonate 0x488200 then
  skips the firer object itself (`cmp esi,[esp+var_8]`) and every object of the firer's group (0x47EB00 = +0x34),
  unless that object is turned (vt+0xF8).
- Engine: Combat.cpp:97 `Explode(pt->second.field98, ..., z)` with no firer (-1). The part's destruction
  explosion hits the building itself and its group-mates. That damage runs Damage -> LoseParts again, which
  collapses buildings faster than the original and harms neighbouring units of the same side.
- Fix: pass the building's group as the firer and its id as the attacker. Die() can stay as it is, because a
  top-level unit has no parent and its firer is 0.

### 4. A building can lose several parts from one hit in the original; the engine takes one (MEDIUM-HIGH)
- Original 0x48EF80: the loop runs `lose` times. Each pass sends 0x80A0 through vt+0x1C (= 0x47DF20, the
  synchronous handler). 0x489FA0 sends 0x8057 through vt+0x1C, and 0x47DF20 case 0x8057 calls vt+0x78. That is
  0x47EAA0, which calls the parent's vt+0x50 = 0x47F220, which removes the child from +0x50 (0x440A90) at once. So
  each pass takes the new last child, and a big hit removes up to `lose` parts. (The part classes' own vt+0x78 was
  not traced; that override is unverified.)
- Engine: Combat.cpp:62-98 (`u.partsLeft--` once at :76). Its comment says "the child isn't off the list until
  it's gone, so a hit takes one part", which the trace above contradicts.
- Fix: loop `lose` times in LoseParts, taking the last part each time.

### 5. Particles and debris keep their absolute height over slopes instead of hugging the ground (MEDIUM)
- Original: 0x47D6F0 sets +0xB8 bits 8 and 10 on every object (`or ah, 5`). 0x47F770 with bit 10 set moves the
  point and adds vz to +0x84 with **no** ground compensation, so the height stays relative to the ground. Only
  the mortar shells' class 0x488E80 clears bit 10 (`and ah, 0FBh`; the gas clouds are on it too). They alone take
  the "keep the absolute height" branch, ftol(+0x80 - +0x84) - the new ground.
- Engine: Effects.cpp:162-166 applies the ground-change compensation to every flying object. Emitter particles,
  smoke, sparks and other debris that cross a slope sink into rising ground (lift < 0, deleted early) or float
  over falling ground.
- Fix: compensate only for `EffectKind::Shell` (including gas); for the others use `lift += vz`.

### 6. Damaged scenery smokes too early (MEDIUM)
- Original 0x48AB20: when the health crosses from above 40% to below 40% of the most (`max*4/10`, 0x66666667) and
  the object isn't on a list (+0x10 0), it is added to the world's update list (0x440060 on dword_4D2550+0x20).
  WScenery isn't on that list from the start: 0x489170 adds only a scenery object with a looping sound. Its
  update (0x48AEF0, the smoulder) therefore runs only once it is under 40%. The smoke then starts at level 3
  (0x16F-0x171), not at level 1 below 80%.
- Engine: Effects.cpp:367 SmoulderProp runs for every live prop every 15 updates.
- Fix: give Prop an `updating` flag, set it in DamageProp when the health crosses 40% (or for looping-sound
  scenery at load), and smoulder only flagged props. Unverified: whether any other path adds plain scenery to
  the list.

### 7. Smoke and turret sounds reveal fogged enemy units (MEDIUM)
- Original 0x48AEF0: no smoke when vt+0x9C is set (`call [edx+9Ch]; jnz skip`; per Unit::fogged that is +0xB8
  bit 3). The turret 0x489B40 also starts its turning sound only when vt+0x9C is clear.
- Engine: Smoulder (Effects.cpp:334) and TurretTick (Weapons.cpp:186-188) don't check `u.fogged`.
- Fix: return early from Smoulder when `u.fogged`; skip the turret's sound when fogged.

### 8. Some kills vanish with no death explosion in the original (MEDIUM, behaviour check)
- Original 0x4967E0: after the look round (vt+0x104), `vt+0xB4 (health) < 0` calls vt+0x78 and returns. For a
  vehicle that is 0x495BB0: 0x807B to both players, out of the world, with no 0x80A0, no explosion and no splash.
  0x80A0 arrives 10-29 updates after the killing hit (0x48AB20), and the look round comes every +0x92+1 updates.
  So a unit with negative health whose look round comes due first disappears quietly (for the default +0x92 of
  90, roughly 10-30% of kills).
- Engine: LookRound (Weapons.cpp:470) has no such check; every kill goes through Die.
- Fix: in LookRound, after the scan, `if (u.health < 0) { Vanish(index); return; }` (and skip Smoulder that
  update, which the original also skips).

### 9. Mortar shells and plain flying objects are drawn 32 rows too far forward (LOW)
- Original: 0x487800 (every flying object's start) sets the renderer's +0x26 = width/2 + **0x20**. Only
  WAnimEffect's start 0x489000 overrides it to width/2 + 0x40. Shells (0x488EF0) and 0x17B keep +32.
- Engine: Effects.cpp:118 uses `width/2 + 64` for all.
- Fix: depth = width/2 + 32 for Shell and Plain kinds.

### 10. The beam glow's frame runs only while it is shown (LOW)
- Original 0x4885C0 (the shells' class draw) steps the glow sprite (0x40EFB0 with 0x100) on every draw, firing or
  not; 0x495180 draws it only in state 2.
- Engine: BattleMap.cpp:1018 steps flashFrame only when the glow is drawn (state 2), so a new burst always starts
  at the glow's next frame instead of wherever the frame has got to.
- Fix: step flashFrame for every gun of class 0 each draw.

---------------------------------------------------------------------------------------------------------------------

## Behaviour divergences

### 11. The rocket's landing has no height jitter (MEDIUM)
- Original 0x488830 calls rand() twice (x, y), then h = the target's z (+0xC4) less the ground (0x47B120). The
  shell's 0x4886A0 calls rand() three times (x, y, z).
- Engine: Impact (Weapons.cpp:989-1001) jitters z for every class and calls rand() three times. Rockets land at
  the wrong height, and the random stream drifts.
- Fix: for cls 1, `z = g.tz` with no third rand().

### 12. The research damage multipliers are applied to a truncated amount (MEDIUM)
- Original: 0x4958B0 multiplies the **scale** float (arg_4, 1.0) by each multiplier, then 0x48A5A0 does
  ftol((1 - d/reach) x power x scale) in x87 extended precision, a single truncation.
- Engine: Explode truncates (`int(double(hit)*1.0)`, Combat.cpp:242), then Hit multiplies and truncates again
  (Weapons.cpp:619-636). For example 10.9 x 1.5 gives 15 instead of 16, and hits come out lower with research.
- Fix: pass the float scale into Hit/Damage and compute `int(raw * scale)` once. Also keep the Martians' gas
  immunity outside `if (setup.researchMultiplier)` (Weapons.cpp:615/634), because the original returns for gas
  whatever the research.

### 13. Light parts consume a rand() even when no roll applies (LOW)
- Original 0x48DE80: rand() % 100 only when the parent's category (0x440700) is 0-3.
- Engine: BattleMap.cpp:823 always calls Rand(), which shifts the shared random stream for every later roll.
- Fix: call Rand() only inside the category 0-3 branch. The explosion parts' lights (Combat.cpp:221) skip the roll
  entirely. Unverified: whether any explosion type has category 0-3, in which case the roll (and the Martian
  exclusion) should apply there too.

### 14. A dying (health <= 0) unit no longer retaliates or speaks when hit (LOW)
- Original: 0x488200 sends vt+0xC0 to every object in reach. The vehicle's 0x491020 says 0x110 and sends 0x8085
  (retaliation) before 0x4958B0 -> 0x48A5A0, where the health > 0 check is.
- Engine: Combat.cpp:240 `if (u.health <= 0) continue;` before Hit, which skips the speech and retaliation too.
- Fix: move the health check into Damage (it is already there) and drop it from Explode/AreaDetonate.

### 15. The firer is spared by its group even when gone; a turned firer is hit by its own blast (LOW)
- Original 0x488200: it spares only if the firer object still exists (0x440110 on +0xCC). It always skips the
  firer object itself; a turned group-mate is not spared.
- Engine: Combat.cpp:236 compares the group whether or not the firer is alive (a mortar's or gas shell's firer
  may be dead by landing time). A turned firer is not skipped.
- Fix: `const Unit* f = UnitById(attacker); if (f && u.id == f->id) continue; if (f && u.group == f->group &&
  !u.Turned()) continue;`. The same firer lookup applies in AreaDetonate (0x487B50, which spares the group
  outright).

### 16. Flying objects are removed one update early (LOW)
- Original 0x487890: under 0.0 high it **posts** 0x8057 with a delay of 1 (0x4355A0, `push 1`). 0x488F00 posts
  its 0x8057 with a delay of 1 too. The object is drawn for one more frame.
- Engine: Effects.cpp:168/187 and Weapons.cpp:981 remove it at once.
- Fix: mark it `dieAt = ticks + 1` rather than erasing it.

### 17. Underground target: the group's bit 0 and +0x56 aren't cleared (LOW)
- Original 0x4826D0: with the object's +0x84 < 0 it sends 0x805A to each member, clears +0x1D8 bit 0, and sets
  +0x56 to 0 (the bit 0 test after clearing always falls to 0x482799). The target and the point are kept.
- Engine: Weapons.cpp:736-747 stands the guns down but leaves groupBusy and the move-order count.
- Fix: `groupBusy[g] = false;` and reset the group's MoveOrder there.

### 18. An emplacement that isn't aiming doesn't free its group (LOW)
- Original 0x48F8A0: not aiming (+0xE4 bit 1 clear) -> 0x480EB0(0) on its group (bit 0 cleared), then 0x496890.
- Engine: LookRound (Weapons.cpp:478) has no else branch.
- Fix: `else if (IsEmplacement(u.type) && hasGroup) groupBusy[u.group] = false;`.

### 19. A gun takes an update longer to fire after being aimed (LOW)
- Original 0x494D20 (a gun's vt+0xC8) runs 0x494FD0 at once and updates +0xF0 bit 0, so state 0 can go to 1 on the
  next cycle.
- Engine: Aim's `arm` (Weapons.cpp:799-807) doesn't recompute the aimed flag. GunTick recomputes it only after its
  switch, so firing starts one update late.
- Fix: evaluate the range/arc test in `arm`.

### 20. Damage precision (LOW)
- Original 0x48A5A0 is all x87: fild d / fild reach, fsubr 1.0, fimul power, fmul scale, then __ftol (truncate).
- Engine (Combat.cpp:241, 260, 297) rounds through float between steps, so a result just under an integer can
  round up. This is rare.
- Fix: compute in double and truncate once.

### 21. Cascade weapon type (LOW)
- Original 0x48CDE0 passes +0xC4 (the weapon type) on to the next explosion.
- Engine: Effects.cpp:185 leaves Cascade::weapon at 0. The research multipliers in 0x4958B0 key on it; it doesn't
  matter for 0x252-0x255 today.
- Fix: carry e.weapon.

### 22. The fire timings use a hard-coded 30 (LOW)
- Original: dword_4B84C8 is the [?] "Game Frequency" ini value (0x401770 registers it, default 30). The gun
  cycle (0x494DB0) and the light fades (0x48DDC0, 0x48E100) use it.
- Engine: Weapons.cpp:66 and Effects.cpp:261 hard-code 30.
- Fix: read the value once and pass it in.

---------------------------------------------------------------------------------------------------------------------

## Architecture misalignments

### A1. Guns, turrets and shells have no position of their own
- Original: each gun and turret is a child object with its own +0x78/+0x7C/+0x80 and +0x40 heading. The range
  test (0x494FD0), landing spread (0x4886A0/0x488830 measure from the gun), mortar launch point (0x4889D0), turret
  aim (0x489B40), beam origin (0x495180) and aim jitter (0x496200 per child) all use the child's point.
- Engine: the Gun and Turret structs live inside Unit and use the unit's x/y (marked *Provisional* at
  Weapons.cpp:161, 775, 842, 921). Long vehicles and buildings with offset guns get the wrong ranges, spreads and
  launch points.
- Fix: give Gun and Turret a world point each update, computed from the part's offset (or its hardpoint vertex
  via WofInstance::WorldPoint, as BuildBeams already does), and use it everywhere the original reads +0x78/+0x7C.

### A2. The explosion's parts are made before the explosion itself
- Original: the WExplosion object exists first; its start (0x488350) creates the children (0x489000 ->
  0x47EF30), then detonates.
- Engine: Explode recurses into parts and spawns the explosion's own sprite last (Combat.cpp:216-264). A child
  explosion's firer/parent linkage (the parent's vt+0x9C gate in 0x487890) and the draw order of equal keys
  differ.
- Fix: spawn the explosion's effect first and keep a parent index on Effect, so that 0x487890's
  `parent->vt+0x9C` gate can be honoured.

### A3. Building parts and their lights aren't linked
- Original: a part is a child with its own children; a light on a part goes with it (0x47EAA0 -> the children).
- Engine: Attached is a flat list per unit, so LoseParts can't take a part's light with it (the *Provisional*
  at Combat.cpp:61). Make Attached entries carry child indices, or key lights by part.

### A4. Hits and damage take ints
- Damage(int) loses the 0x4958B0 scale factor (item 12). Make Hit take the raw float hit and a float scale.

### A5. No world update list
- Scenery doesn't update until damaged below 40% (item 6). Fading lights and effects join the list
  (0x48DDC0, 0x487800). The engine iterates everything every update. A per-object "on the update list" flag
  would cover it.

---------------------------------------------------------------------------------------------------------------------

## Provisional items resolved

- **kLookScale / flt_59E44C** (Weapons.cpp:456-458): it is the [Tweak] "Audible radius multiplier" float, 1.0 by
  default (0x495620 + 0x416500: tweak object at 0x59E438, value at +0x14 = 0x59E44C). See item 1.
- **Die "gone at once rather than on the next update"** (Combat.cpp:106): the original is immediate too. 0x80A0's
  0x8057 goes through vt+0x1C (= the handler 0x47DF20), and its case 0x8057 calls vt+0x78 synchronously. The
  engine is right; drop the note.
- **LoseParts "a hit takes one part"** (Combat.cpp:55-58): wrong. 0x47F220 removes the child from +0x50 at once,
  so each loop pass takes the next part (item 4).
- **"An effect's frames under 3 never end"** (Effects.cpp:144): 0x40EF20 returns without writing the done flag when
  (frames-1)<<8 <= 0x100, and 0x489050 then tests an uninitialised stack byte (var_2). This is undefined in the
  original, so "never end" is a defensible choice. Note the reason in the comment.
- **The flying object's frame step** (BattleMap.h:171): +0xC0 = 0x80 is set by WAnimEffect's constructor 0x488FB0
  (not only WExplosion's Init 0x488350). This is confirmed for every animated effect.
- **The light's start intensity**: 0x48DDC0 sets +0xC0 to 0, and the renderer's vt+0x28 (0x48E050) then sets it to
  the record-13 float again. The engine's l.current = f (overshooting to 3f for 338) matches.
- **The beam/glow blit mode**: confirmed. 0x40EE10 passes (+0x14 & 1) << 1 as the mode, and WAnimEffect's start
  0x489000 also sets +0x14 bit 0, so explosions take the same mode-2 path. On 16-bit that is the plain draw, as
  docs/BATTLEMAP.md already says for the glow.
- **0x48AB20's "damaged list"** (Combat.cpp:43-44): it is the world's update list (dword_4D2550+0x20, via 0x440060;
  +0x10 is the list owner, 0x4393B0). See item 6.
- **Emplacement stand-down "always, the scale being 0"** (Weapons.cpp:467-468): not always. Only when further than
  its sight x 1.0 from its aim point.

---------------------------------------------------------------------------------------------------------------------

## Missing

- **0x80C1 on being hit** (0x48AEF0 start, LOW-MEDIUM): with +0xC0 bit 3 (set by 0x48AB20) it is cleared, and
  0x80C1 is sent to (vt+0xDC root's, else own) +0x48 -> +0x6C, the war map object's notifier. It is noted as not
  in (Effects.cpp:333). The engine also never keeps +0xC0 bit 3.
- **0x80B1 to the battle** (dword_4E9B78) when a player's unit sees an unarmed enemy (0x496570), LOW: not in
  (Weapons.cpp:512).
- **0x8066 relighting** of the objects in a fading light's reach (0x48E100 tail), LOW: not in (Effects.cpp:302).
- **0x487890's parent gate**: a flying object whose parent says vt+0x9C skips its emitters and step entirely. Not
  in (Effects.cpp:144); needs A2.
- **The player's unit vt+0x100 every dword_4EA614 updates** (0x4967E0): not ported, and not identified here.
- **+0xF0 bit 2** (in range, set by 0x494FD0 separately from bit 0, the arc): not kept. Its readers were not traced.
- **The 0x111 alert for buildings**: the engine plays it only for the player's buildings with an attacker found
  alive. The original also requires the firer to exist (0x440110) and be of another side; that matches. No gap.

## Checked and matching (no action)
The gun cycle states and timings (0x494DB0: rand() % 15, word x 30 / 20), range/arc test (0x494FD0), shell
landing spread (0x4886A0, every 5th count in state 2), mortar launch ballistics (0x4889D0, constants 0.1,
0.0015339808, -1.0), shell landing (0x488F00, -8.0), gas bounce (0x488D70: 10.0, 0.65, 0.25, every 15th, puff
+0x98), area blasts (0x487B50 cases and delays 120/240/360), the cascade (0x48CDE0, Random(8) - 4 y then x),
the scorch (0x488200: under 64, not water, 0x804B), Detonate's reach (0x47FBA0: octagonal <= both radii, kind
4 skipped), smoke levels and the puff (0x48AEF0 5 - 5f, 0xCCC, 0x48ADD0 r = radius/3+1, -1.414), emitters
(0x47F050), light fading (0x48E100), the turret step (+-50, 0x43FF30), the beam frames (0x495180: +0x2C<<4 +
i + phase, -4.0, 16.0), the aim jitter (0x496200), the sinkers (0x498F20, 0x493640, 0x493C20) and the research
range/spread items (0x494AF0).
