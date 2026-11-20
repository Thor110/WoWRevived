# Research

Both sides' research as the war map keeps it (WResearch 0x457FA0-0x45BCB0), rebuilt in `src/core/Research` with the
two trees in `src/core/ResearchTrees.h`. The screen that shows it (0x45BCC0-0x45F770) is still to come. Anything
marked *provisional* is a stand-in until the game's own code for it is traced.

## The trees

The trees are built in code (0x459540 Human, 0x45A680 Martian), not read from a file. `ResearchTrees.h` is generated
from those constructors' calls, in order: AddTrigger (0x458950), AddPlan (0x458670, the plan's name), AddPage
(0x4586E0, its number), AddUnit / AddBuilding / AddEnhancement (0x458890 / 0x4588F0 / 0x458830: a unit type and up to
four prerequisites). Items are numbered as they're made, triggers first: Human 8 triggers and 78 items in 6 plans
(and an empty seventh plan with no name), Martian 6 triggers and 84 items in 6 plans, 30 pages each. A new tree
writes exactly what Human.000 and Martian.000 hold (`wowdump --research`).

From OBJ.ojd each item takes, by its unit type:

| Record | Unit table | What |
|---|---|---|
| 21 | +0x14 | its name (the item's name, +0x0C) |
| 26 | +0x0C | faction: 0 Human, 1 Martian, 2 neither (the default) |
| 39 | +0x08 | research time in days, kept in hours: days x 24 x `[Tweak] "<HumanFriend, HumanEnemy, MartianFriend or MartianEnemy> Research Modifier"` (0.75 each), by the faction the type has when record 39 comes and the side being played (Friend when they match), x 1.0 for faction 2; truncated to a 16-bit word (0x4383F2) |
| 40 | +0x1C | its name on the research screen |
| 41 | +0x20 | "Researching ..." |
| 42 | +0x24 | what it says once done |

Most types have faction 2 when record 39 comes, so their time is the whole number of days.

## Items

- **Kind** (+0x40): 0 unit, 1 building, 2 enhancement, 3 trigger (time 1, no page, named "Trigger").
- **State** (+0x4C): 0 locked, 1 available (being researched is also 1), 2 done, 3 obsolete.
- **Fresh** (+0x50): 2 when made, 0 when it becomes available, 2 once looked at (0x45BCB0). Pages have their own,
  +0x60, 1 when made.

**Becoming available** (0x45B920): a locked item whose prerequisites are all done or obsolete. With notifications on
(byte_588BA8) and the local player's research, if it takes any time, the research hears 0xB8 (+0x6C, purpose
unknown) and the war map gets ResearchAvailable (event 5) with its name. One that takes no time is done at once.

**Done** (0x45BB90): state 2. Its first prerequisite becomes obsolete if it's the same kind; for units only if the
unit table's +0x00 matches, which is the type itself, so in practice never ("%s Makes %s Obsolete"). With
notifications on, the local player's research hears 0xB9 (the idle count reset, and the current item dropped at the
end of the hour, whichever item it was) and the war map gets ResearchCompleted (event 4). The other side's research
tells its player instead (0xB9 with the type to dword_4E9B78, the AI; *not yet*), notifications or not.

**Triggers** stand for things met in battle: UpdateTriggers (0x45A1C0 / 0x45B320, at the war map view's set-up
0x418D17 and on the way to a battle 0x41A094) marks one done once any of its types has the unit table's +0xAC flag,
which the battle map sets (0x47F630, 0x488C00, 0x492931; so none yet):

| Side | Trigger | Types |
|---|---|---|
| Human | 0 | 0x50-0x52 Scanning Machines |
| | 1 | 0x197-0x199 |
| | 2 | 0x2C2-0x2C4 |
| | 3 | 0x4D-0x4F Flying Machines |
| | 4 | 0x53-0x55 Constrictors |
| | 5 | 0x2BF-0x2C1 |
| | 6 | never set ("Reading Martian symbols" waits on it) |
| | 7 | 0x4A-0x4C Electric Machines or 0x50-0x52 |
| Martian | 0, 3 | never set |
| | 1 | 7-9 Tunnelling Track Layers |
| | 2 | 0x13-0x18 Ironclads and Submersibles |
| | 4, 5 | 0x273-0x276 mines |

Then an unlock pass, and each finished enhancement's improvement multiplier is set from `[Tweak]` (1.0 until then;
read by the battle map's damage and weapon code): Human 0x256 HeatResistantArmour 0.85, 0x257
InsulatedHighEnergyResistantArmour 0.75, 0x259 HighExplosiveRounds 1.15, 0x25A ImprovedPowderCharge 1.15, 0x25B
AdvancedPowderCharge 1.30, 0x25C FineAirFilters 0.85, 0x25D ChemicalFilters 0.75, 0x25E EnhancedSightingApparatus
1.20, 0x266 TunnelReinforcementApparatus 0.75, 0x265 EnhancedDrill 1.25; Martian 0x27B ShockResistantArmour 0.85,
0x27C ImpactReflectingArmour 0.75, 0x27D EnhancedRayFocusMechanism 1.15, 0x27E TightBeam 1.30, 0x289 HoverBombs 1.25,
0x27F SecondaryBeamAmplifiers 1.15, 0x280 MotionTrackingDevice 1.20 (each `<name>Multiplier`), and the Martian
detection upgrades 0x28C / 0x28D (byte_589130 / 589134).

## Every game hour

The war map's 0x802D handler runs the local player's research, then the other side's (0x419EE2 / 0x419EEE), after
the routes and before the pods. Each (0x458BD0): the current item's hours go down (0x45BB00) and at 0 it's done; with
none current, the idle count (+0x64) goes up; unlock passes repeat until nothing changes; a 0xB9 heard on the way
clears the current item; and the local player's research, idle more than 336 hours (0x150) with anything available,
raises ResearchAlert (event 6, "No research is currently in progress", sound 0x135) and starts counting again.
Choosing an item (0x458A00) starts its hours at its whole time if none are left from before and resets the idle count.

The Research button's tool tip (0x45BEB0) is "View Research" and the current item's status (0x45B750: its record 41
text and the time left in `<< >>`), or "No research in progress" (0x461).

**Times** (0x41BDA0): from two days up "<n>days" ("day" for one; Martian "cycle(s)"); below that, in Human games,
"<n>hrs" ("hr" for one); Martian games always count days. Nothing for 0. No space before the unit.

## Saved

HRSH then MRSH, whichever side is played (0x41AD80 / 0x41AC40; the tag, then 0x4585D0 / 0x4584C0):

    u32 selected plan, u32 page, u32 selected item     -1 each in a new game (the screen's)
    u8 +0x6C, u8 +0x6D
    i32 the current item's number, or -1
    u32 plans, then each: u32 pages, then each: u8 fresh, u32 items, then each: u32 state, u32 fresh, u32 hours left

Triggers aren't in any page, so they aren't saved; UpdateTriggers works them out again. Loading reads as many plans,
pages and items as the tree has, whatever the counts say, then checks every item again with notifications off.

## Still to do

The research screen (0x45E670, 0x45D3F0) and choosing what to research; the AI's choice (0x45A400 / 0x45B4E0, its
plan lists) with the war map AI; the battle map setting +0xAC; the cheat that finishes everything (0x458460, keys
2 14 12 4 1 0 2 10 at 0x4013A0; `Research::CompleteAll` is ready for it).
