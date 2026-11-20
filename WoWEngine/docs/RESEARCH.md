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

## The screen

`src/frontend/ResearchScreen` (0x45E670, opened by WResearch::Open 0x458150 from the war map's 0x7D: R or the
Research button, with time stopped). A full-screen view: backdrop 0x85 (HU_RSCH / MR_RSCH) in palette group 0x2EF
(hr: {S}R{S}R in slot 7, {S}R{S}I in 5, {S}RGI in 9, 0x40BF79), and in Human games the overlays 0x86 HU_RSCHT,
0x87 RES_LAMP and 0x88 RES_WIRE over the tree. The exit button (GType 0x7C RES_EXIT / MR_EXIT, command 0xBD) is at
(0x1AF, 0x1A4) Human, (0x1B7, 0x1BD) Martian; Esc too. Opening plays 0x19C (the cabinet) and settles the folders
first; closing (0x458380) plays 0x19D and the war map comes back (0x41A10F).

**Plans and pages.** A plan is shown (0x458100 / 0x45C140) if any of its pages is, or it's the "open plan": Human,
always the last plan (the empty one, the wire divider r_divide); Martian, the last plan with a visible page. A page is
visible (0x45CCF0) if any of its items is available or done. From (32, 30) Human or (4, 10) Martian (0x459200):

- each shown plan's folder at (x, y): sprite 0x89 for the open plan, else 0x8A (tab_top / mr_tab), frame 1 under the
  mouse (Human) or, Martian, (frames - 1) x open / full; its name font 0x72 centred at (x + 0x6F, y + 0xB) Human or
  (x + 0x9B, y + 0x10) Martian, as "<<name>>" when the current research is in it; click area (x + 0xE, y) 0xC3 x 0x16
  (none for the open plan) or (x + 0x23, y) 0xF0 x 0x20, command 0xBE. y goes on by 0x16 / 0x20 (and on a Martian
  folder open past 0x20, by the rest).
- each visible page unrolled at all at (x + 3, y) Human or (x + 0x28, y) Martian (0x45E550): sprite 0x8B for the
  open plan's last visible page, else 0x8D (r_fold / pge_top / mr_page), frame 1 under the mouse; its name font 0x72
  at (+6, +6) Human or (+0x1C, +8) Martian, "(name)" when it has the current research, drawn in Martian games only
  once fully unrolled; click area 0x1D6 x 0x14 / 0xF8 x 0x18, command 0xBF. y goes on by its height and, Human, x by
  a tenth of it.
- after the pages y goes on from where the last one started by how far the folder is open (Martian at most 0x20), and
  Human x by 2.

Names are coloured `[Colours] "Research Current"` (25) for the current research's plan / page, `"Research New"` (27)
for one with an item not yet looked at (fresh 0 or 1), else `"Research Normal"` (26). Martian pages are drawn only
when their folder is fully open and every visible page fully unrolled.

**The folders move** (0xBC, a step a tick until nothing moves, 0x458D90): the selected plan opens by `[Research]
"Human Open Rate"` (20) or `"Martian Open Rate"` (10) and every other shown plan closes by it. Opening (0x45C840),
the folder (+0x3C up to 0x16 Human, 0x26 Martian) first, then what's left unrolls the visible pages from the last up
(+0x44 up to 0x18 / 0x14, 0x45D330); closing, the pages roll up from the first, then the folder.

**Clicks.** A folder (0xBE): it's selected (sound 0x199), or unselected if it was (0x19A); the page goes (0x458D10,
sound 0x19B) and the folders move. A page (0xBF): if it isn't the one shown, 0x19B, its items are marked looked at
(fresh 2, 0x45CD70) and its detail shows (0x45D380); its scientist's speech plays 30 ticks on (the page's speech text's TEXT.ojd
sound, 0x414700), and the last page's is faded out over 30 ticks when its page goes (0x45E4A0 -> 0x464EB0), the
screen closing included. An item in the detail (0x84): if it isn't the one picked already, sound 0x132 and it becomes
the current research (0x458A00).

**The detail** (0x45D3F0 Human, 0x45DD50 Martian). The blueprint window (0x45F4B0) at (0x118, 0x16) Human or
(0x143, 0x2F) Martian, the size of the page's picture (OBJ.ojd records 63 / 64: the page table 0x5888C0 gives each
page a name, a picture GType and a speech text): res_det (0x8C, Human) and the picture, then its texts, font 0x72,
each a window of its own placed inside it (w is the picture's width):

| | Human | Martian |
|---|---|---|
| Texts placed | centred in w - 0x4A from 0x36 | centred in the picture (flag 0x10) |
| Title ("(name)" with the current research), "Research Heading" | y 0x12 | y 6 |
| The scientist's text, wrapped, "Research Detail" | to w - 0x4A, at the title's height + 0x18 | to w - 8, at the title's height + 9 |
| After it | its height + 0x12 | its height + 6 |
| "Specifications:" (0x462), if a unit or building there is done (state 2) | then its height + 6 | + 3 |
| each, its record 42 text wrapped, "Research Specification" | its height | its height |
| after the section | 0x12 | 6 |
| "Enhancements:" (0x463) for done enhancements, "Research Enhancement" | the same | the same |

With anything available there, the proposals list (0x45F140): res_prop / mr_prop (0x90) at (0x12A, 0x114) Human or
(0x144, 0x123) Martian, "Proposals For research:" (0x464, "Research Heading") centred at y 0xA, then from 0x10
(Martian 0xD) below that each available item (0x45EEA0) as its name and time (0x45B6D0) centred, one under another,
coloured "Research Current" for the one being researched, "Research New" for one not yet looked at, else "Research
Proposal" (0x45F070). A click on one researches it (0x84); a click elsewhere on the blueprint plays the speech
again (0x45F710). The overlays are drawn over all of it (0x45E930).

**The keyboard** (0x45ECB0, on a folder or page): the mouse takes the focus to whatever it's over (0x20 ->
0x42EBE0), a folder or page showing frame 1 while it has it. Up and Down move the focus to the one before or after
(0x8008 / 0x8007 to their parent; the engine goes round the ends, *provisional*), Enter and Space click it (0x201 ->
0x111 with its command), Esc closes the screen.

**The proposals** (0x45F0E0): a proposal's tool tip asks the research (0x8114 with the item's index): "Makes
Available:" (0x46E) and a line for each item still locked that has it among its prerequisites with every other one
done or obsolete (0x45BA30), by its research name (record 40, 0x45B690); none when there are no such items. The
blueprint window's timer (0x45F320, 0x8129 every 2 ticks, 0x45F3D0) steps the focused proposal's colour level
between `[Colours] "Menu Glow First"` (4) and `"Menu Glow Last"` (9) and back, as the menus do; one the focus leaves
is left at `"Menu Items"` (4), not its own colour, until the detail is built again. Esc there would close the page
(0xBF with -1), but the screen takes Esc first.

## The computer player's choice

The AI player (0x442EC0) keeps its research item (+0x454): once it's done or obsolete it lets go of it, and with none
it asks its research (vtable +0x2C, 0x458A60): the first available item, not a trigger, with a trigger among its
prerequisites (0x45B8B0); otherwise from rand() % the item count on, round, the first available item that isn't a
trigger (none: nothing). It becomes the research (as 0x458A00, and 0x80A3 to the screen if it's shown).

## Still to do

The battle map setting +0xAC (the triggers); the cheat that finishes everything (0x458460, keys
2 14 12 4 1 0 2 10 at 0x4013A0; `Research::CompleteAll` is ready for it).
