# Oddities

Things about the game's own files and behaviour that are odd, surprising or easy to trip over, written down so
nobody has to find them twice. Not engine faults, and not things to "fix" without the owner saying so.

## The start files (`Human.000`, `Martian.000`) by region (update 161)

A new campaign starts from `Human.000` or `Martian.000` in the game folder (0x4043D0 builds `%s.%03d` with slot
0; docs/WARMAP.md, "Saving"). The owner sent the two files from five installs: German, English, Spanish, French
and Italian, and later the English pair from the disc itself.

| File | DE | ES | FR | ITA | EN (the owner's WoWRevived testing folder) |
|---|---|---|---|---|---|
| `Human.000` | 19,312 bytes, SHA-256 `60f7e4a4...`, 26 Oct 1998 18:00 | the same | the same, 17:45 | the same | 19,312 bytes, `cbd252a3...`, **22 Nov 2026 02:32** |
| `Martian.000` | 34,172 bytes, `daa920dc...`, 26 Oct 1998 18:12 | the same | the same, 17:57 | the same | **35,711 bytes**, `7adec20f...`, **22 Nov 2026 01:21** |

**All five releases shipped the same pair.** The four continental ones are byte for byte the same and carry
the discs' date, and the pair the owner then took straight off the English disc (update 162) is the same again:
`60f7e4a4...` and `daa920dc...`. The differing English pair in the table came from the owner's WoWRevived
testing folder, where both files had been written on the day the frames for updates 157 to 159 were taken.

**What wrote them: the game itself. Saving to slot 0 overwrites the start file.** The save routine (0x4042D0,
"Saving game to slot %d") names its file `SaveGame\%s.%03d` for every slot but 0; for slot 0 it writes
`%s.%03d` with 0, that is `Human.000` or `Martian.000` in the game folder, the very file new campaigns start
from. The game saves the campaign to its slot whenever the war map is left or a battle comes back
(docs/WARMAP.md, "Saving"). A campaign started the ordinary way has been given a slot from 1 up (0x404660), so
this never shows; one that reaches the war map with its slot still 0 (a start that skips the menus' slot step,
as a patched or debugging start may) saves over the start file, and every "new" campaign after that begins
from wherever that one stopped. The owner made no edits by hand; this is how the testing folder's pair came to
be a day old. **To put it right, copy the two files from the disc again.** The engine doesn't do this: a
campaign with no slot isn't saved (a `MODERN FIX`, docs/MODERN.md).

How a campaign comes to have slot 0 (updates 163, 164). What the listing says:

- The slot is one global (dword_4D2578), 0 when the game starts, and written in one place only: when a slot
  button is pressed in Select Campaign to Replace (0x4043FA). Those five buttons carry 1 to 5 (0x409110), full
  or empty alike; a full slot only adds the "Replace Existing Campaign?" question. So full slots don't lead to
  slot 0 by themselves.
- The new campaign (0x8139) and the war map's start both read that global. If the war map is started in a run
  of the game where no slot button has been pressed yet, the slot is still 0: the campaign plays from the start
  file and saves back onto it.
- The overwritten `Martian.000` agrees: its name field still reads `SaveGame\Martian.001`, the developers'
  name. A campaign made through the menus has the name typed in Edit Campaign there (0x404660), so this one
  never passed the slot and name steps.

**The way in is the network version's menu** (update 164; the owner's install has it on). With `[Screen]`
"Enable Network Version" set (0x4016F0, byte_4D12FC) the Main Menu doesn't offer New Campaign with its slot
menu (0x8138); it offers Single Player Campaign (0xA0), and that menu's "Start New Campaign" sends 0x8133 with
0x8037 at once: the disc check, the intro and the war map, **without Select Campaign to Replace or Edit
Campaign** (0x4068F0; docs/FRONTEND.md). The slot global is never set, so the campaign has slot 0, plays from
`Human.000` / `Martian.000` and is saved back over it the first time the war map is left or a battle comes
back. From then on that install's new campaigns start from the last one's state, under the developers' name.
It is a fault of the original with the network version on, in an unpatched game as much as a patched one, and
a likely source of "the new campaign started strangely" reports. The engine doesn't save a campaign without a
slot (`MODERN FIX`), so the same menu there leaves the start files alone.

Two other things checked for the owner. A save is written with `fopen(..., "wb")` (0x44EA20), so a shorter save
doesn't leave the tail of a longer one behind. And none of the twelve saves to hand has a section tag out of
place (one `GAME`, `WMAP` and `CALN` each). The owner remembers finding stray `WMAP` tags in saves while
writing the save editor; nothing here shows it, so it wants a file that does before anything is said about it.

Every one of the ten files, the 1998 ones included, carries the name of a slot 1 save inside it
(`SaveGame\Human.001`, `SaveGame\Martian.001`): the developers made the start files by saving a game in slot 1
and renaming it.

### `Human.000`: the testing folder's against the discs'

Three bytes, all in the WMAP block: the view's saved corner is 593, 1948 where the others have 803, 1805.
Nothing else. It makes no difference in play: with a sector selected (London, 30) the view is centred on that
sector's bounds when the map opens (0x4189B9), which gives 803, 1805 either way.

### `Martian.000`: the testing folder's against the discs'

The testing folder's file is the same campaign **one game day later**:

| What | The discs' (all five) | The testing folder's |
|---|---|---|
| Game time (CALN) | 0.0 hours | 24.2667 hours: 91 updates at speed 8 (8 / 30 of an hour each), one update past the 24th hour |
| Date (DtTm) | 1 Sep 1898, 00:00:00 | 2 Sep 1898, 00:00:24 as stored (day 1, hour 0, minute 0, second 24) |
| The Martian clock on screen | 0000 00 | 0001 01 |
| View's saved corner (WMAP) | 159, 298 | 150, 324 (the selected sector's centring, 0x4189B9) |
| Message list (MPAD) | empty | one note, "Pod landed: Sector 6" |
| Pods (after MPAD) | hours 0, none come | hours 24, one come |
| Human research (HRSH) | nothing under way | an item under way (the engine, run the same day, starts "Ship Yard Level 2") |
| Sector 6 | no one's, empty | the Martians' (the pod): two objects, types 62 (Scout Machine Mk1) and 89 (Handling Machine Mk1), four units |
| Sector 3 (the Martian base) | | the same objects; a few bytes of state differ |
| The computer's (Human) forces | | moved, below |

The Human computer player's first day, read off the two files (object type: from → to):

- type 19: 15 → 12, 15 → 18, 17 → 21, 28 → 23, 28 → 27
- type 1: 10 → 9, 12 → 9, 16 → 13, 18 → 22, 19 → 24, 21 → 25
- type 28: 12 → 8
- sector 8: a new construction, type 35

**The engine, run for one day from the continental `Martian.000`, makes exactly these moves, starts the same
construction and the same research** (its log, update 161). That is a check of the war map's computer player
against a save the original wrote, and it passes move for move.

The one difference is the pod: the original's first pod came down in sector 6 carrying types 62 and 89. That
can't be reproduced from the file, and needn't be. The sector is `byte_4B9DE8[rand() & 7]` over 2, 4, 6, 7, 8,
9, 10, 11 and the load `off_4B9E28[rand() % 4]` (0x41A330), with rand() seeded with 1 when the world is built
(0x417DA3), but before the pod's turn rand() has also been drawn from for every counter made (once a counter
when the map opens, again on each change of view; update 165) and by the ambient sound each time it starts
with no CD track playing, which goes by real time. So the first pod's place changes with what the player
looked at and how long they took, as the owner had noticed.

## The player's view changes move the dice (update 165)

Every counter the war map makes draws one number from the game's rand() (0x42B34B, for the timing of its
production icon), and the map makes the view's counters afresh each time the view is changed (0x41C2E0).
The computer player's moves, the pods and a battle's deployment draw from the same sequence. So flicking between
the Unit and Construction views changes what the computer does next. Harmless, and almost certainly not meant.

## Section headers with nothing under them in a save (update 179)

The owner's note from hex-editing saves: section headers seem to be left lying about with their contents gone.
What the 24 saves show is the format, not a fault. Every sector writes all six of its lists every time, each as
its tag (HUNI, AUNI, HCON, ACON, HRES, ARES), then "WMOB" and a count, whether or not it holds anything: 180
list headers a save, and between 94 and 168 of them empty (12 bytes each, the count 0). A sector that has seen
no battle writes "DMGL" with a width and height of 0 the same way, and one whose battle left no mark writes a
whole layer packed down to one byte. When an object goes its list's count drops and its bytes go with it; the
header stays because it was never the object's.

So there is nothing to clean. The check that nothing else is stray: the engine reads each of the 24 saves to
its last byte with every tag where its reader expects one, and written straight back the sectors come out the
same size and, but for a few rounded numbers, the same bytes (update 178). If a save turns up that the reader
stops on, it goes here.

## The developers' flags: Mat, Charlie, Dave and Edit functionality (update 180)

Four on / off settings in `[Debug]`, all off unless set, three of them named for people (0x401580-0x401640).
Each is read straight from its setting's value byte, so setting it to 1 turns it on for good; the `curtains`
cheat sets Mat's byte for the one run (0x401372). None of them opens a menu: each lets through things the game
otherwise refuses. Read from the listing; not tried in the original.

**Mat** (byte_4D1279; `curtains`)
- Battle map: the other side's units take the mouse like the player's own: they show the hot brackets
  (0x4972F0, which otherwise returns at once for an object that isn't the player's) and a click selects them
  (0x497680), one unit at a time and not its whole war map object (0x4976F5), with Shift's state sent with the
  click.
- War map: a click or the popup on a sector the player doesn't hold selects it (0x41D24A, 0x41D430).
- The reinforcement points are made when a battle map is entered with no battle on (0x43D171), and for every
  neighbouring sector, not only the ones that qualify (0x43D2D2).
- With `[Game]` "Time Of Day" not 0, the battle map is lit for that hour (0x40B030).

**Charlie** (byte_4D1261): three of the war map's events stop being critical, so they no longer wait their turn,
stop the clock or put up their box, and only write their line in the messages: ResearchAlert (0x41F220),
PodLanded (0x41F010) and UndefendedSectorLost (0x41E960). Their test in the event table is "Charlie is off"
(0x41E4D0). BattleInSector is always critical.

**Dave** (byte_4D1231): text for a debugger. Some eighty places write a line through OutputDebugString or the
game's own printer (0x403680's frame figures, the CD's calls 0x412690-0x414390, DirectPlay's packets 0x44A1A7,
the sound files asked for 0x466029). The printer is an empty routine in the retail build (nullsub_4), so most of
it says nothing; the OutputDebugString lines ("CD not available") can be seen with a debug viewer.

**Edit functionality** (byte_4D1249): the editor's picking. On the battle map a click and a drag box are sent to
the scenery's list as well as the units' (0x485A1A, 0x485DA9), and an object of any kind can be picked
(0x48A87D: otherwise only kinds 1 to 3); on the war map any sector can be selected, as with Mat; and the
renderer's object pool is made forty times larger (0x40D150).

Any of Mat, Charlie or Dave at start-up also sets one more flag (byte_4D12F4, 0x401998), which makes the sound
loader look for a sound as a loose file before the library (0x466910).

The engine: Mat's time of day is in (179); the rest of Mat is PLAN.md 4.9; Charlie, Dave and Edit functionality
are with the developers' aids, 4.8.

## An upgraded building fights as the building it was (update 185)

When research gives the computer player a better level of a building, every such building it has is changed in
place (0x442D80). What is changed is the war map object's type (+0x20, 0x442E06) and nothing else. The object's
battle map object, made when the building was, keeps the type it was made with, and the save writes each with
its own type: in the owner's saves all 535 upgraded buildings are a higher level on the war map over a first
level battle object. So on the battle map an upgraded building is the old one: its model, its health, its guns.
Whether that was meant isn't known; it reads like a step left out: every level has parts, armour and guns of its
own (update 186), and nothing found passes the new type on to the battle object (its type setter, 0x47EB10, is
only called by itself). The player's buildings go the same way as the computer player's (140 and 395 of the
535). The engine has a modern fix for it, on unless turned off (docs/MODERN.md).

