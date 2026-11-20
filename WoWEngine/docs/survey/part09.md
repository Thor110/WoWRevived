# Part 09: 0x45B690–0x466300 (research tail, research screens, build list, SoundFXSystem)

Sources: `pieces/part09.txt`. Functions IDA left collapsed (0x462FF0, 0x4644C0, 0x464520, 0x465700) were
disassembled from `game/WoW.exe` with objdump. Its bytes match the listing at these addresses.
Field offsets are hex. The term "tick" means one game update; there are 30 a second (`dword_4B84C8` = 0x1E).
Anything that is a guess is marked *(guess)*.

## 1. Overview

The piece has four blocks:

| Range | Block | Size |
|---|---|---|
| 0x45B690–0x45BCB0 | End of **WResearch**: the research-node state machine, obsolescence, and the name and tooltip strings | ~15 small functions |
| 0x45BCC0–0x45F770 | **Research screen UI**: the "View Research" button, the research tree, the category and item nodes, the research detail panel (Specifications / Enhancements / Proposals), the "Research Hotspot", the "Proposals" list, the "Blue print" window, and the scientist's dialog speech | ~75 functions |
| 0x45F770–0x462EE0 | **Build list** (production queue): the per-sector `buildList` WObject, its queue entries, production timing, the "Build List" window (available items, queue slots, TTC) and the item tooltips | ~70 functions |
| 0x462EF0–0x4662FB | **SoundFXSystem / DeeSound**: SFX.ojd loader, the 0x54-byte sound objects (pool of 128), six per-type lists plus a speech queue, 8 hardware voices with per-type limits and stealing, positional attenuation and panning, fades, ducking, the DirectSound primary and secondary buffers, the volume curve, and the sample cache | ~75 functions |

No hand-written assembly here. Every function has a normal MSVC prologue, SEH frames and `__thiscall`. Two
points to watch:
- The x87 `fyl2x` dB table in 0x465B20 is the compiler's inline `log10`.
- Several small functions (0x462FF0, 0x4644C0, 0x464520, 0x465700 and 0x465720) are collapsed in the IDA listing,
  so their code has to come from the exe.

---

## 2. Subsystems

### 2.1 WResearch tail (0x45B690–0x45BCB0)

A research node is the object these functions run on (`this`). Its layout is given after the table.

| Addr | Name | What it does |
|---|---|---|
| 0x45B690 | Research::Name | Returns "Trigger" when kind (+40) is 3, else the text of `table[+44].+1C` (via 0x4146F0) |
| 0x45B6D0 | Research::NameWithTime | Builds `"<name> (<time>)"`. The time is +54 (remaining), or +2C (total) when +54 is 0, formatted by 0x41BDA0. Uses buffer 0x4E8D20 |
| 0x45B750 | Research::StatusText | For state 2/3 (done/obsolete): text `table[+44].+24`. Otherwise `"<table.+20 text> <<time>>"` |
| 0x45B810 | Research::DoneText | Text `table[+44].+24`, or "Trigger" |
| 0x45B850 | Research::StartTimer | Sets +54 = +2C if +54 is 0 |
| 0x45B860/870/890/8A0 | IsInProgress (state 1), IsDoneOrObsolete (2/3), IsDone (2), IsTrigger (kind 3) | |
| 0x45B8B0 | AnyPrereqIsTrigger | Any of the four prerequisite pointers +30..+3C has kind 3 |
| 0x45B920 | TryMakeAvailable | If state is 0 and every non-null prerequisite is done or obsolete: state=1, +50=0. If the global byte 0x588BA8 (research notifications on *(guess)*) and +48 (player's side) are set and there is time to do, it posts 0x111/0xB8 to itself and raises war-map event 5 **ResearchAvailable** (0x41E5B0). If +2C is 0 it completes immediately (0x45BB90) |
| 0x45BA30 | PrereqsMetExcept(x) | The same test, but treats x as done |
| 0x45BB00 | Advance(dt) | When in progress: +54 -= dt. At 0 or below it calls Complete |
| 0x45BB30 | ForceAvailable | state=1. Sends the same notification as above |
| 0x45BB90 | Complete | state=2. If prerequisite +30 is of the same kind, or (kind 0) its table entry +0 (category) matches, that prerequisite becomes **obsolete** (state 3). The debug message is `"%s Makes %s Obsolete"`. Then it posts 0x111/0xB9 and raises event 4 **ResearchCompleted** (player side), or sends 0x111/0xB9 with +44 to `dword_4E9B78` (the other side / AI *(guess)*) |
| 0x45BC70 | CompleteTrigger | For a state-0 trigger: state=2 and posts 0xB9 |
| 0x45BCB0 | MarkSeen | +50 = 2 |

Research node fields:
- +0C name
- +20 parent window
- +2C total time
- +30..+3C four prerequisite pointers
- +40 kind (3 = trigger)
- +44 index into the object/item table 0x51A0F0 (0xB0 bytes a type)
- +48 byte: belongs to the player's side
- +4C state: 0 locked, 1 available or in progress, 2 done, 3 obsolete
- +50 sub-state
- +54 remaining time

The item table text fields it reads:
- +1C name text id
- +20 "in progress" text id
- +24 "done" text id
- +00 category

### 2.2 Research screen and dialogs (0x45BCC0–0x45F770)

This is the full-screen research view opened from WResearch (0x458150 calls 0x45E670). It holds a tree of
category nodes and item nodes, built on the generic tree widget base 0x4A4360, 0x4A43A0, 0x4A43D0 and 0x4A43F0
(another piece). Pressing a node opens a detail panel with the scientist's speech.

| Addr | Name | What it does |
|---|---|---|
| 0x45BCC0 | ResearchChild::OnCommand | Forwards message 0x111 to +24 |
| 0x45BCF0 / 0x45BD30 | Research::Load / Save | Stream vfunc 0x5C reads and 0x30 writes +4C, +50, +54. This is the research part of the save (HRSH/MRSH, see SaveGame.h) |
| 0x45BD70 | ViewResearchButton ctor | Push button 0x42FA20, id 0x341, vtable 0x4AAB00. Text object (0x2A8 bytes, font 0x72). +98 is the flash timer |
| 0x45BE80 | dtor | |
| 0x45BE90 | thunk to 0x433D00 | |
| 0x45BEB0 | ViewResearchButton::OnMessage | 0x807C tooltip: text 0x47D "View Research" + "\n" + current research `StatusText` or 0x461 "No research in progress". Messages 0x20, 0x800A and 0x80A3 set flash +98 = 60 ticks. 0x80D9 returns the position (x, y-0x18) |
| 0x45BFA0 / 0x45C050 | ResearchTreeNode ctor / dtor | vtable 0x4AAB40, base 0x4A4360 (title text). +40 = 0x16 Human / 0x26 Martian (`0x404BA0` = Human game). +48 and +4C are graphics |
| 0x45C110 / 0x45C140 | FindSelectedChild / AnySelected | |
| 0x45C180 | IsCurrentResearch | 0x458100 returns the current research |
| 0x45C1A0 | ResearchTreeNode::Draw | Icon at +34,+38. Title at +6F,+0B (Human) or +9B,+10 (Martian). Colour 0x4B9400 for the current research, 0x4B9408 if any child is "on", else 0x4B9404. Martian progress icon frame = (frames-1)·+3C/+40. Draws its open children (0x45CDD0) |
| 0x45C3C0 | ResearchTreeNode::Open | Icon GType 0x89 (current) or 0x8A. Title "<<name>>" for the current research. Hotspot (0x45EC00, id 0xBE) is 0xC3×0x16 (Human) or 0xF0×0x20 (Martian). Help ids 0x468C / 0x4682 |
| 0x45C660 | Close | |
| 0x45C6E0 | OnMessage | 0x80A3 tooltip ("<<name>>"), 0x111 forward |
| 0x45C7D0, 0x45C840, 0x45C9A0 | Scroll and layout of the children | |
| 0x45CAD0 / 0x45CB10 | Category node Load / Save | |
| 0x45CB70 / 0x45CC50 | ResearchCategoryNode ctor / dtor | vtable 0x4AAB68. Text from the category table 0x5888C0 (12 bytes an entry, filled by the OBJ.ojd loader 0x437750+0xDB4) |
| 0x45CCF0, 0x45CD30, 0x45CD70, 0x45CDB0 | Small state getters and setters (is open, has progress, mark children seen) | |
| 0x45CDD0 | ResearchItemNode::Draw | |
| 0x45CF10 | ResearchItemNode::Open | GTypes 0x8B / 0x8D. Text "(name)". Hotspot 0xBF with a 0x1D6-wide rect |
| 0x45D140 | Close | |
| 0x45D1A0 | ResearchItemNode::OnMessage | 7 / 8 = focus on / off (+40). 0x20 resets the cursor (0x42E780(0)). 0x80A3 tooltip. **0x808F (click)**: stops the previous dialog speech `dword_589138` (0x464E30), then plays the category's speech, text id `dword_5888C8[cat*3]`, through 0x414700 (TEXT.ojd entry sound id → 0x464EE0 at the screen centre, volume 1.0). It sets the sound object's +24 (start tick) = 30, so the speech starts 1 s later. If the speech is missing it logs "SFX missing for dialog". Other 0x111 commands go to the parent |
| 0x45D330 | ClampScroll(delta) | |
| 0x45D380 | OpenDetail | Marks the children seen, then calls 0x45D3F0 (Human) or 0x45DD50 (Martian). Sets +41 |
| 0x45D3F0 / 0x45DD50 | BuildDetailPanel Human / Martian | Builds: a blueprint window (0x45F4B0, 0x118×0x16); a title (0x430500, font 0x72, "(name)" when current); the category speech (as above); the category text; then three sections: **0x462 "Specifications:"** (done items of kind 0/1), **0x463 "Enhancements:"**, **0x464 "Proposals For research:"** (a 0x45F140 list). Text colours are set with 0x8128 from the style table 0x4B940C..0x4B941C. Human and Martian layouts differ (+0x36/+0x4A versus +4/+8 offsets) |
| 0x45E4A0 | CloseDetail | Releases the panels. **Fades the dialog speech out over 30 ticks** (0x464EB0(0x1E)) |
| 0x45E550 | SetPos | |
| 0x45E5D0 / 0x45E620 | Load / Save of the screen state | |
| 0x45E670 | ResearchScreen ctor | Full-screen view (0x4061B0, GType 0x85, palette group 0x2EF), vtable 0x4AAB90. Human only: overlays 0x86, 0x87, 0x88 (help ids 0x4696, 0x46C8, 0x46A0). Close button 0xBD with sprite 0x7C at (0x1AF, 0x1A4) Human or (0x1B7, 0x1BD) Martian, help 0x46D2 |
| 0x45E930 | ResearchScreen::Draw | Draws the backdrop, the tree (0x458420) and the overlays |
| 0x45E9E0 | ResearchScreen::OnMessage | WM_KEYDOWN Esc (0x1B) → 0x111/0xBD (close). 0x8007 = focus the next hotspot (flags bit 16 and bit 28) |
| 0x45EC00 / 0x45ECA0 | ResearchHotspot ctor / dtor | "Research Hotspot", vtable 0x4AABD0, flags 0x10000 |
| 0x45ECB0 | ResearchHotspot::OnMessage | Keys: Enter / Space → 0x201 (click). Esc → 0x8123. Up → 0x8008, Down → 0x8007 (focus prev / next). Mouse 0x200 / 0x201 → sends 0x111 with its id |
| 0x45EEA0 / 0x45F060 | ResearchInfoBox ctor / dtor | vtable 0x4AAC10, texts 0x44–0x48 and 0x104 |
| 0x45F070, 0x45F0E0 | Draw (colours 0x4B9400, 0x4B9408, 0x4B9410) / OnMessage (tooltip 0x807C) | |
| 0x45F140 / 0x45F260 | "Proposals" list ctor / dtor | vtable 0x4AAC50 |
| 0x45F2D0, 0x45F320, 0x45F3D0 | Proposals Draw / OnMessage (0x113 timer scroll, 0x800C, 0x8129 → 0xBF) / colour cycling (0x8128) | |
| 0x45F4B0 / 0x45F5D0 / 0x45F5F0 | "Blue print" window ctor / dtor | vtable 0x4AAC90, GType 0x8C |
| 0x45F690, 0x45F710 | Blueprint Draw / OnMessage (0x201 → 0x111/0x808F, which replays the speech) | |

### 2.3 Build list / production (0x45F770–0x462EE0)

#### WBuildList object

Created by 0x45F770 (`"buildList"` WObject, vtable 0x4AACD0, one per owner (a sector's factory or constructor)).

Fields:
- +1C window
- +20 byte: war-map mode *(guess: when set, constructions are queued straight away; when clear, a battle-map placement cursor is used)*
- +24 count of available items
- +28[] available item ids
- +A8[32] queue of entry pointers
- +128 queue count (**maximum 32**)
- +12C "busy" flag (mirrored to the war map by `dword_4E9B70->0x4192F0(0)`, the sector-shading refresh)
- +130 owner object

The owner's fields used here:
- +08 id
- +20 side
- +24 byte: AI-owned (no player events)
- +48 sector; sector +48 is the sector number

#### Queue entry

0x3C bytes, vtable 0x4AADE0, ctor 0x461A80 / 0x461B60.

- +10 build list
- +14 target object id; 0xDEADDEAD = none
- +18 item type (row of 0x51A0F0)
- +1C..+28 placement x, y, x2, y2 (default: the screen centre `dword_4E9250`, `dy`)
- +30 repeat count: 1–3, or 4 = forever
- +34 hours remaining (float)
- +38 byte

**Build time** = `trunc(item.+5C (float) × 24.0)` hours. For units (item +10 kind == 1) it is also multiplied by the
word item.+58 (units per batch *(guess)*). See 0x461B60, 0x461C80 and 0x461E60.

| Addr | Name | What it does |
|---|---|---|
| 0x45F770 / 0x45F860 | BuildList ctor / dtor | The dtor deletes the entries and clears the busy flag |
| 0x45F7D0, 0x4606D0, 0x460740 | ProductionMultiplier | m = sector[+90+k]·[+94+k]·[+98+k], where k = 3 if `0x4406E0(owner)` else 0 (sector table 0x4EAE20, 0xC0 bytes a sector). The cheat flag `byte_58913C` (cheat 0x13 at 0x401233) forces m = 1. 0x45F7D0 returns head.+34·m (time left, for TTC). 0x460740 returns 1/m |
| 0x4607A0 / 0x4607C0 | ResourceSlowdown(side, item, sector) | For each of the three resources r: f_r = max(1, (sector.+48[r] + item.cost[r] (+6C/+70/+74)) / sector.+78[r]). Returns 1/(f0·f1·f2). Called from 0x443D66, 0x443F44 and 0x444D07 (sector code, which presumably fills the multipliers above) |
| 0x45F900 | ReplaceUpgradedItems | Replaces list entries with their upgrade (item +84 / +88) |
| 0x45F970 | BuildAvailableList | Walks types 1..1007. A type is allowed if the side may build it (0x442460(side, type)). If the sector **has** its command building (0x43C990) then Command Post (0x3A) and Comms Centre (0x78) are excluded. If it hasn't, **only** its command building type is allowed: Command Post 58, London 278, Comms Centre 120, landing site (sector 3) 830, and only when `0x42A410(owner)` is false. Category 0x27 is excluded when `0x4373C0(sector)` is true |
| 0x45FA80 | OpenWindow | `new 0x460B00` (0x88 bytes) |
| 0x45FB00 | CloseWindow | |
| 0x45FB20 | BuildList::OnMessage | **0x8026** add: arg4 == 0 → 0x460240(cmd), else 0x4603B0(type). **0x802D** (hours elapsed, from the calendar) → 0x45FC90. **0x80C4** window closed. **0x812F** remove an entry by id |
| 0x45FC90 | Progress(hours) | dt = hours/m. While dt > 0 and the queue isn't empty: dt = head.Advance(dt). When the head reaches 0: a unit (kind 1) is finished by 0x4620B0 and raises event **2 UnitCompleted**; anything else raises event **1 ConstructionCompleted** (both only for the player's side). Then 0x461C30(0xDEADDEAD) and Pop (0x45FF00). If the queue empties: busy=0 and event **9 ConstructorIdle** (when 0x440700(owner)==1) or **10 FacilityIdle** |
| 0x45FE30 | CountOfType | |
| 0x45FE60 | SetBusy(on) | A unit at the head only starts if 0x43AC10(sector, type, 1) (can afford / has room *(guess)*) |
| 0x45FF00 | PopHead | A repeating entry (+30 > 0) has its time reset (0x461E60) and is **re-queued at the back**. Returns whether work continues |
| 0x460080 | RemoveEntry(ptr) | |
| 0x460140 | CancelAll | Only for owner type 1. Copies the queue, then calls 0x462120 on each entry |
| 0x4601C0 / 0x460200 | SendBuildCommand | Build command 0x16 (with a 0x20-byte placement block) or 0x17. Packet built by 0x4A4400 / 0x447D80 (header 0x707) and sent through the command target `dword_4E9B74` (0x447DB0), so it is network-synchronised |
| 0x460240 | AddFromCommand(cmd) | Appends a type (0x4603B0). For a command building with no position given it picks the sector's spot (0x4411B0). Fills the entry (0x461C80). A non-unit gets 0x4620B0 *(guess: starts the site)*. Then 0x440EA0 and the UI refresh |
| 0x4603B0 | AddType(type) | Items with +4E == 3 need 0x43AC10. Adding a unit equal to the last queued one just bumps its repeat (0x461EE0). Otherwise 0x4604B0 |
| 0x4604B0 | InsertAt(type, pos) | Allocates an entry and inserts it. Refused at 32. Sets busy |
| 0x4605A0 | MoveToFront(entry) | |
| 0x4608D0 | SetBusyOn | |
| 0x460900 / 0x460960 | Save / Load | count, busy, entries (entry vfunc 0xC) |
| 0x460A40 | HeadName | Text item.+14 |
| 0x460A80 / 0x460AC0 | Settings | `[BuildList] Top` (default **70**) and `Left` (default **0**) |
| 0x460B00 | BuildListWindow ctor | "Build List", class 0x804A, id 0x5DCA, vtable 0x4AAD10. Left panel 0x461500 at (Left, Top). Right panel 0x460F10 at (Left+0x58, Top+0x71). A "Scroll" bar 15 px wide. `byte_589158` = window open |
| 0x460CF0 / 0x460D60 | dtor (sends 0x80C4 to the owner) / IsOpen | |
| 0x460D70 | BuildListWindow::OnMessage | 0x111 with an item id: a unit → command 0x17. A construction: in war-map mode → command 0x16 straight away; otherwise placement mode 0x42E790(mode, id) with mode 5 for item 0x1BC, 6 for 0x1C3–0x1C8, 4 otherwise (0x42E790 sets the global cursor mode 0x4EA43C/0x4EA440 and sends 0x20 to the active view). 0x201 / 0x204 close it in war-map mode. 0x80A4 refreshes and forwards 0x80A4 to the view `dword_4EA448`. 0x812F forwards to the owner |
| 0x460F10 / 0x4612A0 | QueuePanel ctor / dtor | vtable 0x4AAD50, background GType 0x2FB. Slot 0 = current item (0x4621C0, sprite 0x76, at 8,6 Human or 0xD,6 Martian). Four more slots below (0x0A,0x45 or 8,0x44, stacked). Buttons 0x2FF at (6,0x34/0x38) and 0x300 at (5,0xAC)/(6,0xAA). **"TTC"** time-to-complete text, font 0x70, at (0x14,0x29) |
| 0x461340 | QueuePanel::Draw | TTC from 0x460740 × head time, shown as a date with 0x41B9A0 |
| 0x4613F0 | QueuePanel::OnMessage | 0x2FF → 0x7E23 |
| 0x461490 | RefreshQueueSlots | Calls 0x462420 for each slot |
| 0x461500 / 0x4617D0 | AvailablePanel ctor / dtor | vtable 0x4AAD90, background 0x2FA, scroll buttons 0x2FD / 0x2FE, builds the list (0x45F970) |
| 0x461890, 0x4618E0 | AvailablePanel Draw / OnMessage | 0x8144, 0x2FD → 0x7E25 |
| 0x4619A0 | RebuildItemButtons | Uses 0x462770 |
| 0x461C30, 0x461C80 | Entry::SetTarget / Init | Sends **0x80AE** (started) to the target object when it is type 2 or 3 |
| 0x461D60 | Entry::Advance(dt) | Re-initialises the time at 0. Tells the target **0x80AC** (still going) or **0x80AD** (finished). Decrements the repeat count when ≤3. Returns the leftover dt |
| 0x461E60 | ResetTime | |
| 0x461EB0 / 0x461EE0 | RepeatDown / RepeatUp | 1→2→3→4 (∞). Down from ∞ goes to 3 |
| 0x461F00 / 0x461FA0 | Entry Save / Load | |
| 0x462080, 0x4620A0 | Helpers | |
| 0x4620B0 | Entry::Finish | 0x43CD10 creates the object in the sector. Sends 0x80AE |
| 0x462120 | Entry::Cancel | Sends 0x80A0 to the target. Refund / clean-up through 0x47D600 *(guess)*. 0x440EA0 |
| 0x4621C0 / 0x462380 | QueueSlot ctor / dtor | 0x98 bytes, vtable 0x4AADF0, GType 0x2FC, flags 0x10010000 |
| 0x462420, 0x4624A0, 0x4625B0 | QueueSlot SetItem / Draw (icon item.+2C, progress colour `word_4CA7B8`) / OnMessage | 0x807C tooltip = item name. Click: decrement the repeat or remove (0x461EB0 / 0x460080). Right click: cancel (0x462120). Cursor 0xF |
| 0x462770 / 0x4629C0 | ItemButton ctor / dtor | 0x9C bytes, vtable 0x4AAE30. Shows the build time "%0.1f" |
| 0x462A40 | ItemButton::Draw | Icon frame item.+2C, or +0x96 when disabled |
| 0x462B50 | ItemButton::OnMessage | 0x20 sets the cursor (0xE enabled / 0xC disabled). 0x201–0x205 click → 0x111 item id to the parent. 0x807C tooltip: `"%s"` name (item +14), then `"\n%s %s"` with 0x51E "Requires" and the item at +94, then `"\n%s"` with 0x494 "This item can only be constructed on the BattleMap" when +99 is set |
| 0x462DC0 | ItemButton::UpdateEnabled | `0x43C7F0(sector, type)` && !battleMapOnly && `0x442420(sector+54, type)` *(guess: resources / prerequisites)* |
| 0x462E50, 0x462EA0, 0x462EB0 | Small button (vtable 0x4AAE70) | Sets the cursor 0x10 on hover |
| 0x462EE0 | (sound) | Reads the last listener position 0x589178 |

### 2.4 SoundFXSystem (0x462EF0–0x4662FB)

#### Objects

- **SoundDef** table at 0x58FE88: 450 (0x1C2) entries of 0x70 bytes, constructed at 0x466A50. Fields:
  - +04..+3F: three variation groups of up to five sample pointers each
  - +40[3] group counts
  - +4C[3] round-robin cursors
  - +58 / +5C / +64: set by opcodes 8, 9 and 10. **Nothing in the exe reads them** (grep: written only at 0x463ED1, 0x463EED, 0x463F3F)
  - +60 type
  - +66 loop word (opcode 7)
  - +68 volume float
  - +6C byte "queued speech" (opcode 6)
  - +6D byte "cull off-screen" (opcode 18)
- **Sample** cache at 0x5898B0: 500 (0x1F4) entries of 0x34 bytes, count `dword_58FE80`, vtable 0x4AAF30. Fields:
  - +04 refcount
  - +08 name
  - +15 stereo flag
  - +18 rate
  - +1C DSBCAPS flags
  - +20 master IDirectSoundBuffer
  - +28 "from a .WoW library" flag
  - +2C byte size
  - +30 use count
- **Sound object** (0x54 bytes, vtable 0x4AAEE0: dtor 0x464820, Tick 0x464CB0). Allocated from a free-list pool (0x464710: blocks of **128**, "Allocating memory block for 128 SFX objects"; all freed when the live count reaches 0 and there is more than one block, "UNLOADING ALL SFX OBJECT MEMORY"). Fields:
  - +04 refcount (word)
  - +10 next in list
  - +14 sound id
  - +18 sample
  - +1C voice
  - +20 age in ticks
  - +24 start tick
  - +28 fade-in end tick (0 = none)
  - +2C fade-out start (-1 = none)
  - +30 end tick (-1 = loops)
  - +34/+38 world x, y
  - +3C/+40 position relative to the listener
  - +44 byte "don't follow the listener"
  - +48 volume
  - +4C fade factor
  - +50 byte "keep alive after fade"
- **Voice** (0x24 bytes, vtable 0x4AAEF8: dtor 0x465700/0x465720, Update 0x465940). Fields:
  - +04 refcount
  - +10 sound object
  - +14 duplicated IDirectSoundBuffer
  - +1C last DS volume (initially -10000)
  - +20 last pan
- **Lists** (head +0, count +4):
  - 0x5894A0 Ambient (type 0)
  - 0x589370 Engine (1)
  - 0x589388 Explosions (2)
  - 0x589360 Weapons (3)
  - 0x589258 Special (4)
  - 0x5893E8 Speech (5)
  - 0x5891E0 **speech queue** (type 5 with opcode 6): no duplicate ids (0x466D00)
  - `dword_5894A8` = number of live sounds
- **Voice slots**: `dword_589180[16]`. Count `dword_4CA7E8` = **8**, capped at 16 by the ctor. Per-type counts are in `dword_5891C0[6]`; the total in use is in `dword_5891DC`. Global critical section `stru_5893CC`.

#### Functions

| Addr | Name | What it does |
|---|---|---|
| 0x462EF0 | SoundFXSystem ctor | WObject "SoundFXSystem", vtable 0x4AAEB0. Gets DeeSound (0x465B20). Caps the voices at 16 |
| **0x462FF0** | SoundFXSystem::Update (once a tick, from 0x4038A4) | Collapsed in IDA. **Listener** = the centre of the current view's rect (`dword_4EA448` +2C..+38, through view vfunc 0x2C), else (320,240); calls 0x464A50. **Speech queue**: if no speech voice is playing and the queue isn't empty, it starts the head (0x4632B0). **Engine**: if a voice is free and fewer than **4** engine voices play, the loudest silent engine sound with volume·attenuation above the minimum gets one. If all voices are busy, the same happens when engine voices are ≤4 and the listener moved. **Ambient**: when no ambient voice plays, or the listener moved, 0x463C30. Then every voice's Update. Stores the listener in 0x589178/0x58917C |
| 0x463280 | Rect::Set(x, y, w, h) | |
| 0x4632B0 | AcquireVoice(obj) | Allocator 0x463640 → slot. new Voice (0x4655F0). Counts it. **A speech voice sets the duck factor `dword_4CA998` = 0.5.** Starts it (0x4657B0) |
| 0x4633F0 / 0x4634C0 / 0x4635A0 | ReleaseVoice by voice / by object / by slot | Stops the buffer (0x465920), uncounts it. When the last speech voice goes, the duck factor goes back to 1.0 |
| **0x463640** | AllocateSlot(obj) | Fails (-1) if obj.+48·attenuation < the minimum audible volume. Then by type: **Ambient** (limit 2): a free slot; at the limit, steal the quieter ambient voice. **Engine** (limit 4): a free slot, or evict an ambient voice; at the limit, steal the quietest engine voice. **Explosions** and **Weapons** (limit 4 each): a free slot or evict a lower type; at the limit, steal the *oldest* of that type (largest +20 age). **Special** (limit 2): a free slot or evict a lower type; at the limit it fails. **Speech** (no limit): first sets every playing speech voice's fade to **0.7**, then a free slot or evicts the lowest-type voice. So the priority is Speech > Special > Weapons > Explosions > Engine > Ambient |
| 0x463C30 | PickAmbient | The loudest ambient sound above the minimum. If it has no voice: every playing ambient voice fades out over **150 ticks**, the new one gets fade-in 150, then AcquireVoice. The voice ctor then overrides the fade-in to **15 ticks** (0x4655F0). So the old ambient fades out over 5 s and the new one comes in over 0.5 s |
| 0x463D10 | LoadSetupData | SFX.ojd parser, already ported (`core/SoundTable`). Corrections are in §4 |
| 0x4640D0–0x4642A0 | Static ctors / atexit | The seven lists (IDA calls them `_ReaderWriterLock`; they are intrusive lists) |
| 0x4642C0–0x464440 | Settings | `[Sound\Volume]` Master, Ambient, Engine, Explosions, Weapons, Special, Speech (floats, default **1.0**) at 0x4CA870 and 0x4CA858[6] |
| 0x464480 / 0x464490 / 0x4644A0 / 0x4644B0 | Get/SetTypeVolume, Set/GetMaster | Called from the Sound Control menu 0x405844–0x405AFC |
| 0x4644C0, 0x4644F0, 0x464520, 0x464550 | Default rects (collapsed / small) | Inner (-320,-200,320,200). Outer (-640,-400,640,400). Inner Ambient (-640,-400,640,400). Outer Ambient (-960,-600,960,600) |
| 0x464580–0x464640 | Settings | `[Sound]` "Inner border", "Outer border", "Inner Ambient border", "Outer Ambient border" (rect settings 0x411950) |
| 0x4646C0 | Setting | `[Sound] "Minimum Audible Volume"`, float, default **0.05** (0x4CA874) |
| 0x464710 / 0x4647A0 / 0x464820 | Pool alloc / free / deleting dtor | |
| 0x4648C0 | SoundObject ctor(id, x, y, vol, group) | Picks the sample with 0x466BC0(group): **round-robin** through the group's samples, not random; an empty group falls back to group 0. Fade = 1. End = `bytes·30 / (rate·2 [mono] or ·4 [stereo]) + 3` ticks, or -1 if it loops; 0 if the sample is missing |
| 0x4649F0 | SoundObject dtor | Releases the voice |
| 0x464A50 | SetListener(x, y) | Then ticks every object in the six lists (the queue is not ticked) |
| 0x464AB0 | FadeOutAll(n) | Calls 0x464EB0(n) on each of the six lists; n = 0 → stop. "Fading out all current (%d) sounds". **Queued speech is not faded** |
| 0x464B10 | ScaleBorders(w, h) | Called at 0x402A45, 0x42C87F and 0x42C8F9 on a mode change. Copies the rects, then if w ≠ `dword_4EA3D8` scales inner.left, outer.right, innerAmb.left and outerAmb.right by w/ref; the same for top/bottom with h/`cy`. It only scales one side of each rect, which looks like a bug. No effect at 640×480 |
| 0x464CB0 | SoundObject::Tick | Sound off (`byte_4CA7EC`, constant 1) → remove. Age ≥ end: if +50 is set it becomes endless (end and fade-out = -1), else it is removed (0x4653A0). Age ≥ start: relative position = world − listener (unless +44 or the position is 0,0). **At the start tick, types 2–4 always try for a voice; type 5 does if not queued; Ambient and Engine never do here** (the system update hands them out). Fade-in factor = (age−start)/(fadeInEnd−start); fade-out factor = (end−age)/(end−fadeStart). age++ |
| 0x464E20 | IsFinished | age ≥ end |
| 0x464E30 | Stop | Volume 0, release the voice, unlink |
| 0x464E60 | FadeIn(n) | fadeInEnd = age+n, fade = 0 |
| 0x464E80 | FadeOutKeep(n) | Same as 0x464EB0 and sets +50 |
| 0x464EB0 | FadeOut(n) | fadeOutStart = min(itself, age); end = age+n if that is sooner |
| **0x464EE0** | PlaySound(id, x, y, vol, group) → obj | Called from 47 places. If opcode 18 is set and (x,y) isn't 0,0 and the point relative to the listener is **outside the outer border**, it returns 0 (never created). Otherwise allocate, construct, add to the type list (0x4652B0). Returns the object, or 0 if it couldn't be added (sound off, or zero length) |
| **0x464FF0** | Attenuation(obj) → 0..1 | A position of 0,0 or a sample without CTRLVOLUME → 1. Caches the last position (0x589368) and result (0x4CA878). Ambient uses the ambient rects; the others use the normal ones. Outside outer → 0. Inside inner → 1. In between, per axis: `1 − d/(outer−inner)` on the side it's on. If the x factor < the minimum → 0. The result is x·y. The cache is keyed only on position, not type, so an ambient and a non-ambient sound at the same spot share the last result (quirk) |
| 0x4652B0 / 0x4653A0 | AddToList / RemoveFromList | Speech with opcode 6 → the queue. Counts in 0x5894A8 |
| 0x465490 | ClearAll | |
| 0x4654E0 | ResumeOffset | (age−start)·rate·2(·2 stereo)/30 mod size. A voice regained later starts at the logically correct sample position: virtual voices |
| 0x465540 | AttachVoice | The sample's refcount (0x466630 / 0x466640) |
| 0x465570, 0x465590, 0x4655B0, 0x4655D0 | Def getters | type, loops, volume, queued |
| 0x4655F0 | Voice ctor | Duplicates the sample's master buffer (0x465DF0). SetFrequency if CTRLFREQUENCY (never set from SFX.ojd). **Ambient: fade-in 15 ticks** |
| 0x4657B0 | Voice::Start | SetCurrentPosition(ResumeOffset), SetVolume, SetPan, Play (loops if the sound loops) |
| 0x465920 | Voice::Stop | |
| **0x465940** | Voice::Update (each tick) | A queued-speech owner is ticked here. Finished → stop and release. If positional and volume·attenuation < the minimum: ambient → FadeOutKeep(150); others → release ("Shutting down quiet sound"; the object lives on and can be voiced again: engines through Update, but **explosions, weapons and special only at their start tick**). Otherwise volume and pan (below) |
| 0x465B20 | DeeSound::Instance | Builds a 128-entry volume table once (see the volume curve below) |
| 0x465B90 | atexit | Releases IDirectSound |
| 0x465BB0 | DeeSound::Init(hwnd) | Called from 0x42CB13. DirectSoundCreate, SetCooperativeLevel(**DSSCL_PRIORITY**=3), primary buffer, Play looping |
| 0x465C40 | CreatePrimary | DSBCAPS_PRIMARYBUFFER. SetFormat PCM **2 ch, 22050 Hz, 16-bit** (88200 B/s, align 4) |
| 0x465D10 | CreateSecondary(pp, bytes, rate, flags, stereo) | flags \| DSBCAPS_STATIC (8). PCM 16-bit, 1 or 2 channels |
| 0x465DF0 | Duplicate(src, pp) | DuplicateSoundBuffer, with the DSERR strings |
| 0x465EB0 / 0x465EF0 | Settings | `[Sound] "Search Libraries First"` (bool, 0x58FE78, bss default 0) and `"Unload Limit"` (int, 0x58FE7C, default 0); used by 0x4660A0 and 0x466640 |
| 0x465F30 / 0x465F60 | Sample array ctor / dtor | |
| 0x465F90 | FindOrAddSample(name) | Case-insensitive |
| 0x4660A0 | VerifySamples | After loading: for each sample, if not "search libraries first", a loose `<name>.snd` (0x466910) wins; else the library lookup (0x466750). Missing → "Couldn't find Sfx %s in Library" |
| 0x466120 / 0x4661D0 / 0x466250 / 0x466270 | Sample ctor / unload ("Unloading SFX %s") / GetBuffer (loads through 0x466300 on demand) / GetSize | GetSize opens `<name>.snd` or `.WoW` with CreateFile and GetFileSize; it is cached at +2C |

#### Sound rules (what the rebuild needs)

- **Voices**: 8 at once. Limits per type: Ambient 2, Engine 4, Explosions 4, Weapons 4, Special 2, Speech unlimited.
  Priority by type number (Speech highest). Stealing works as in 0x463640.
- **Volume**: `v = def.volume · obj.fade(+4C) · attenuation · obj.volume(+48) · typeVolume[type] · master`, and also
  `· duck` for types 0–4.
  - duck (`dword_4CA998`) = 1.0 normally, **0.5 while any speech voice plays**, and **0.25 while the war map menu 0x80B5 is
    open** (0x41CFAC; back to 1.0 on 0x8123 Resume, 0x41CF6C).
  - When a new speech starts over another, the older speech voices' fade drops to 0.7.
- **Volume curve**: DS volume = `table[trunc(127·v)]`, where `table[i] = trunc(4745.61156 · log10((i+1)/128))`. That runs
  −10000 (silence) at i=0 to 0 at i=127, i.e. 10000/log10(128) hundredths of a dB per decade.
- **Pan** = `relX · 4` in DSBPAN units (±320 px → ±1280, about 12.8 dB). Only x counts. Speech buffers have no CTRLPAN
  (flags 0x80), so speech is always centred.
- **Update rate**: the DS volume is updated only on even game frames and the pan only on odd ones (`dword_4D2558` & 1;
  the first value is always set).
- **Attenuation** is in screen space around the listener (the view centre).
  - Normal sounds: full volume within ±320×±200, falling linearly to 0 at ±640×±400.
  - Ambient: full within ±640×±400, 0 at ±960×±600.
  - Each axis is clamped by the 0.05 minimum before multiplying.
- **Culling**: an opcode-18 sound outside the outer rect at play time is never created. A voiced sound that drops below
  0.05 loses its voice (except ambient, which fades out over 5 s).
- **Fades** count in ticks.
  - The main menu fades in over 60, and FadeOutAll is called with 30 or 60 ticks.
  - Dialog speech starts 1 s late and fades out over 30 ticks when the research detail panel closes.
  - Ambient crossfade: 150 out, 15 in.
- **Variations**: round-robin per group. Groups are selected by the caller (the 5th argument of 0x464EE0).
- **Durations**: computed from the byte size, plus 3 ticks of slack.

---

## 3. Data structures and globals (quick reference)

| Address | What |
|---|---|
| 0x51A0F0 | Object/item type table, 0xB0 bytes a type, about 1008 types (0x51A1A0 is type 1, up to 0x5456A0). This piece uses: +00 category, +10 kind (1 unit, 2/3 construction), +14 name text, +1C/+20/+24 research texts, +2C icon frame, +4E word (3 = needs 0x43AC10), +58 word batch, +5C float build days, +6C/+70/+74 resource costs, +84/+88 upgrade links, +94 "requires" type, +99 byte battle-map only |
| 0x5888C0 / 0x5888C8 | Research category table, 12 bytes: name text id, ?, dialog text id (from OBJ.ojd) |
| 0x588BA8 | byte: research notifications enabled *(guess)* |
| 0x589138 | Current research-dialog sound object |
| 0x4E8D20, 0x59EF50 | Shared string buffers (tooltips) |
| 0x4EAE20 | Sector table, 0xC0 bytes a sector. +48..+50 resource use, +78..+80 resource supply *(guess)*, +90..+A4 production multipliers (two sets of three) |
| 0x58913C | Cheat: production multiplier = 1 |
| 0x4E9B70 / 0x4E9B74 / 0x4E9B78 | War map document / command target (network) / the other side's research owner |
| 0x4EA43C, 0x4EA440, 0x4EA438, 0x4EA444, 0x4EA448 | Cursor mode and parameter, cursor shape, focused view, game view |
| 0x589140/0x589154, 0x589160/0x589174 | `[BuildList] Left` / `Top` settings objects and their values |
| 0x589158 | Build List window open |
| 0x58FE88 | SoundDef[450] |
| 0x5898B0 | Sample[500]; count 0x58FE80 |
| 0x589180[16] | Voice slots |
| 0x5891C0[6], 0x5891DC | Voices in use per type / in total |
| 0x5891E0 | Speech queue list (+4 = count, read as 0x5891E4) |
| 0x5894A0, 0x589370, 0x589388, 0x589360, 0x589258, 0x5893E8 | Lists per type |
| 0x5894A8 | Live sound objects |
| 0x589210/0x589214 | Listener |
| 0x589178/0x58917C | Listener last tick |
| 0x4CA7E8 | Voices = 8 |
| 0x4CA7EC | Sound on (constant 1) |
| 0x4CA7F0..0x4CA800 | Limits 2, 4, 4, 4, 2 |
| 0x4CA858[6], 0x4CA870 | Type volumes and master |
| 0x4CA874 | Minimum audible 0.05 |
| 0x4CA878 | Attenuation cache |
| 0x4CA998 | Duck factor |
| 0x589200/0x5891F0/0x589260/0x589230 | Inner ambient / outer ambient / inner / outer rects (settings) |
| 0x5893B0, `rc` (0x5892A0), 0x589378, 0x589350 | Working copies of the rects |
| 0x5894C0[128] | dB table |
| 0x5896C0 | IDirectSound* |
| 0x5894B0 | Primary buffer |
| 0x5894B4 | hwnd |

---

## 4. Messages, commands, strings and tuning

**Messages handled or sent (0x111 = command with id in wParam):**
- Research
  - 0xB8 research available and 0xB9 research done (sent as 0x111)
  - 0xBD close research screen; 0xBE / 0xBF research hotspot ids; 0x341 "View Research" button
  - 0x808F play the dialog speech; 0x80A3 tooltip refresh; 0x807C tooltip text request
  - 0x8007 / 0x8008 focus next / prev; 0x8123 Esc / resume; 0x8128 set text colour; 0x80C6; 0x8129; 0x800C; 0x113 timer
- Build list
  - 0x8026 add; 0x802D hours elapsed (calendar); 0x80C4 window closed; 0x812F remove
  - 0x80A4 refresh view; 0x80A0 cancel → target; 0x80AE start / 0x80AC progress / 0x80AD done → target object
  - 0x8144; 0x7E23 / 0x7E25 scroll; 0x804A window class
  - command packets 0x16 / 0x17 through 0x4E9B74
- War map events raised (0x41E5B0): 1 ConstructionCompleted, 2 UnitCompleted, 4 ResearchCompleted, 5 ResearchAvailable,
  9 ConstructorIdle, 10 FacilityIdle.

**Text ids:**

| Id | Text |
|---|---|
| 0x461 | No research in progress |
| 0x462 | Specifications: |
| 0x463 | Enhancements: |
| 0x464 | Proposals For research: |
| 0x47D | View Research |
| 0x494 | This item can only be constructed on the BattleMap |
| 0x51E | Requires |

**Literal strings:** "Trigger", " (", ")", " <<", ">>", "<<", "%s Makes %s Obsolete", "SFX missing for dialog",
"Research Hotspot", "Proposals", "Blue print", "buildList", "Build List", "Scroll", "TTC", "%0.1f", "%s", "\n%s %s",
"\n%s", "SoundFXSystem", "Ignoring CONSTANTVOL setting in SFX.def", "LoadSetupData", "Fading out all current (%d)
sounds", "Shutting down quiet sound", "Allocating memory block for 128 SFX objects", "UNLOADING ALL SFX OBJECT MEMORY",
"Couldn't find Sfx %s in Library", "Unloading SFX %s", and DeeSound::DuplicateSoundBuffer failed DSERR_* (x5).

**GTypes:**
- Research: 0x85 (screen), 0x86–0x88 (Human overlays), 0x7C (close), 0x89/0x8A/0x8B/0x8D (nodes), 0x8C (blueprint)
- Build list: 0x2FA (available panel), 0x2FB (queue panel), 0x2FC (slot), 0x2FD/0x2FE (scroll), 0x2FF/0x300 (queue buttons), 0x76 / 0x78 (slot and button frames)
- Palette group 0x2EF for the research screen.

**Tuning and settings** (registry or INI sections as the game names them):

| Setting | Default |
|---|---|
| `[Sound\Volume]` Master, Ambient, Engine, Explosions, Weapons, Special, Speech | 1.0 |
| `[Sound]` Inner border | -320,-200,320,200 |
| `[Sound]` Outer border | -640,-400,640,400 |
| `[Sound]` Inner Ambient border | -640,-400,640,400 |
| `[Sound]` Outer Ambient border | -960,-600,960,600 |
| `[Sound]` Minimum Audible Volume | 0.05 |
| `[Sound]` Search Libraries First | 0 |
| `[Sound]` Unload Limit | 0 |
| `[BuildList]` Top | 70 |
| `[BuildList]` Left | 0 |

Hard-coded values: 8 voices (maximum 16); limits 2/4/4/4/2; 128-object pool blocks; 500 samples; 450 sound ids;
build queue 32; build time = days × 24 h; repeat 1–3 or ∞; dialog delay 30 ticks; crossfade 150/15 ticks; dialog
fade-out 30; ducking 0.5 / 0.25 / 0.7.

**Corrections to `core/SoundTable.h`:**
- The `priority` field (0xC0, or 0x80 for speech) is really the **DSBCAPS flags**: CTRLPAN 0x40 | CTRLVOLUME 0x80.
  Speech has no pan control.
- `field58`, `field5C` and `field64` are never read by the game.
- Opcode 6 (`flag6`) means "queued speech": it is played one at a time from a FIFO with no duplicate ids.
- Opcode 18 means "don't start if off-screen" (outside the *outer* rect, not the screen).
- Variation picking is round-robin, not random.

---

## 5. Cross-references to other pieces

- **Calls out:**
  - 0x4146F0 text, 0x414700 (play a text's speech)
  - 0x41E5B0 war map events, 0x4192F0 sector shading refresh, 0x41BDA0 / 0x41B9A0 time strings
  - 0x42DE00 / 0x42DFE0 / 0x42EDA0 / 0x42EDC0 / 0x42FA20 / 0x433B60 / 0x430500 / 0x4308A0 window and widget bases (UI piece 0x42FEF0–0x4337B0)
  - 0x40E700 / 0x40EC50 / 0x40EC30 / 0x40F000 / 0x40F6D0 / 0x40F9F0 graphic and text objects (RTypeBase)
  - 0x435640 / 0x435690 / 0x4354C0 WObject
  - 0x4406E0, 0x440700, 0x440110, 0x440EA0, 0x4411B0 selectable objects and sector
  - 0x43AC10, 0x43C990, 0x43C7F0, 0x43CD10 sector resources and construction
  - 0x442420, 0x442460 AI and availability; 0x4373C0
  - 0x447D80 / 0x447DB0 / 0x4A4400 command packets
  - 0x458100 / 0x458420 research tree; 0x4A4220 / 0x4A4360 / 0x4A43A0..0x4A43F0 tree widget base
  - 0x466300–0x466DF0 sample loading and lists (next piece); 0x415120 / 0x415200 / 0x414BC0 file loading
- **Called from:**
  - 0x4038A4 (SoundFXSystem::Update every frame); 0x4027DC (LoadSetupData), 0x402802 (ctor), 0x42CB13 (DeeSound::Init)
  - 0x402A45 and 0x42C87F / 0x42C8F9 (ScaleBorders)
  - 0x405844–0x405AFC (Sound Control menu)
  - 0x41CD50 (war map menu duck); 0x458150 (open the research screen); 0x443D66 / 0x443F44 / 0x444D07 (resource slowdown)
  - 0x464EE0 has 47 callers: front-end, war map (0x41E4E0–0x41F860), research (0x458150–0x458E50), battle map (0x480EF0–0x4833F0), vehicles (0x489FA0, 0x48F030, 0x491020), puppets (0x495CB0), briefings (0x49A820)
  - 0x464E30 stop: 16 callers; 0x464EB0 fade-out: 10; 0x464E60 fade-in: 8; 0x464AB0 fade-all: 8 (0x402B80, 0x4052B0, 0x407990, 0x417CD0, 0x418220, 0x4198C0, 0x41B210, 0x439ED0)

---

## 6. Missing in WoWEngine: candidate tasks (in priority order)

WoWEngine has a single-mixer `platform/Sound` (Play / Stop / FadeIn / FadeOutAll / SetVolumes; every sound plays with
no limit, centred and unattenuated, with a random variation from group 0). It has no research screens, no build
list and no production. The war map's "Still to do" lists construction and research.

1. **Sound voice manager (P1)**: 8 voices; per-type limits 2/4/4/4/2/∞; priority stealing (oldest
   explosion/weapon, quietest ambient/engine); virtual voices that resume at the right offset (0x4654E0); Engine
   and Ambient voiced only by the per-tick update (0x462FF0, 0x463640, 0x463C30, 0x464CB0, 0x465940).
2. **Positional sound (P1)**: a listener at the view centre; inner and outer rect attenuation per axis with the 0.05
   floor; separate ambient rects; pan = 4·dx in DSBPAN (convert to linear L/R); no pan for speech; the opcode-18
   off-screen cull at play time; voices dropped when quiet (0x464FF0, 0x464EE0, 0x4657B0, 0x465940). Needs
   `Play(id, x, y, vol, group)` and a SetListener API.
3. **Volume curve and ducking (P1)**: the 128-step log table (0x465B20) instead of linear gain; duck non-speech to 0.5
   while speech plays and to 0.25 while the war map menu 0x80B5 is open (0x41CFAC / 0x41CF6C); older speech at 0.7.
4. **Speech queue (P1)**: opcode-6 sounds queue FIFO without duplicate ids and play one at a time (0x4652B0, 0x466D00,
   0x462FF0). FadeOutAll leaves the queue alone (0x464AB0).
5. **Fix the SoundTable semantics (P2)**: rename `priority` to DSBCAPS flags; round-robin variation with group
   fallback (0x466BC0); end time = bytes·30/(rate·2·ch)+3 ticks; FadeOut takes the earliest end (0x464EB0);
   FadeOutKeep for ambient (0x464E80); the ambient voice's 15-tick fade-in (0x4655F0); a start delay (+24) as used by the
   dialog speech.
6. **Sound settings (P3)**: `[Sound]` borders, Minimum Audible Volume, Search Libraries First (loose `.snd` overrides,
   0x4660A0), Unload Limit.
7. **Build list / production model (P1 for the war map)**: the per-owner queue (32), entries with the build time
   item.+5C×24 h (×batch for units), progress on 0x802D hours ÷ the sector multiplier (0x4606D0) with the resource
   slowdown (0x4607C0), repeat 1–3/∞ re-queued at the back, completion events 1/2 and idle events 9/10, owner
   notifications 0x80AE/0x80AC/0x80AD/0x80A0, network command packets 0x16/0x17 (0x45FB20–0x462120). Needs the OBJ.ojd
   unit-table fields listed in §3.
8. **Available-items rules (P1)**: 0x45F970: without a command building only the command building can be built
   (58 / 278 London / 120 / 830 sector 3); with one, those are hidden; side availability 0x442460; category 0x27 rule;
   enabled state 0x462DC0; the "Requires" / "BattleMap only" tooltip (0x462B50).
9. **Build List window (P2)**: 0x460B00 / 0x461500 / 0x460F10: the left list with scroll, the right queue (current
   slot plus 4), TTC text, `[BuildList] Top/Left`, slot clicks (repeat / remove / cancel), cursors 0xC/0xE/0xF/0x10,
   placement mode 0x42E790 (modes 4/5/6) on the battle map.
10. **Research state machine (P1)**: TryMakeAvailable / Advance / Complete with obsolescence and triggers
    (0x45B920–0x45BC70), events 4/5, and save fields +4C/+50/+54 (0x45BCF0).
11. **Research screen (P2)**: 0x45E670 (GType 0x85, palette 0x2EF, close 0xBD, Esc), the tree of category and item
    nodes (0x45BFA0, 0x45CB70, 0x45C1A0, 0x45C3C0, 0x45CF10), keyboard hotspot focus (0x45ECB0), the "View Research"
    button tooltip with flashing (0x45BEB0).
12. **Research detail panel and scientist speech (P2)**: 0x45D3F0 (Human) and 0x45DD50 (Martian) layouts with the
    Specifications / Enhancements / Proposals sections, the blueprint window, the category dialog speech delayed 1 s
    with click-to-replay (0x808F) and a 30-tick fade-out on close (0x45D1A0, 0x45E4A0).
13. **Minor (P3)**: ScaleBorders only matters at resolutions other than 640×480 (and is asymmetric there; keep it for
    1:1); the attenuation cache quirk (0x464FF0); volume and pan on alternate frames.

Unclear items to trace further:
- 0x43AC10, 0x43C7F0 and 0x442420 (affordability / prerequisites)
- What +4406E0 selects (constructor versus facility)
- The meaning of the build list's +20 flag
- 0x42A410
- 0x4373C0 (category 0x27)
- Category table field +4
- The item table fields +84/+88 (upgrades)
