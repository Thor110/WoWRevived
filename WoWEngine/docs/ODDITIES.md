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

The one difference is the pod: the original's first pod came down in sector 6 carrying types 62 and 89, the
engine's in sector 10. The sector is `byte_4B9DE8[rand() & 7]` over 2, 4, 6, 7, 8, 9, 10, 11 and the load
`off_4B9E28[rand() % 4]` (0x41A330), with rand() seeded with 1 when the world is built (0x417DA3). The owner
says the first pod's place varies somewhat from game to game, so something that isn't the same every run draws
from rand() before the 24th hour. Open; in docs/PLAN.md.
