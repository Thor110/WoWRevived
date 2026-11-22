# Modern options and ideas

Things the engine could offer beyond the original game, gathered from the project's notes. None of these are in
yet unless marked. The original's behaviour stays the default; each option here is opt-in, and set in the
engine's `.ini` where it says so. This is a list to come back to once the original is complete (milestones 1-5),
not a plan with dates.

## Modern fixes (on by default; do not "correct" these back)

These depart from the original's code on purpose, because the original's behaviour is a fault. The owner fixed each
in WoWRevived's patches to the original game (the repository readme's list), and the engine keeps the fix. In the
code each is to be marked `MODERN FIX`. A parity pass must leave them alone. They are the last stage of the work
(docs/PLAN.md, Stage 6), after the original is complete; the table says how the engine stands until then.

| Fix | The original | The engine |
|---|---|---|
| Volume sliders saved | The Sound Control sliders never write the registry, so the volumes are lost | Written to `WoWRevived.ini` |
| Music toggle | The Sound Control check box does nothing | Stops and starts the music (*to do*: today it's only read when music starts) |
| Master volume on the music | Master doesn't reach the music's playback | Master scales the music |
| Resolution applied at once | The slider only takes effect after a restart | *To do*: the engine saves `[Screen] Size` but always draws 640 x 480 |
| Desktop resolution on the first start | (no such thing) | *To do*: with no `[Screen] Size` yet, the desktop's is taken; afterwards the player's choice stands |
| No 8 / 16-bit colour choice | Two radios in Display Settings; 8-bit doesn't work on modern systems | *To do*: the radios taken out (WoWRevived skips their creation) |
| Loose map files | The game marks MAPS.WoW's entries by looking for `Map\<name>` (0x414FAB), a folder it doesn't have, then reads `maps\<name>`: a loose map file is never used | **In** (update 140): `core/Vfs` looks in `maps\`, so a loose file in `MAPS` replaces the archive's, as in `DAT` |
| No "Lights" toggle, lights always on | With lights off, Infiltrate, which makes a light, crashes the game | *To do*: the toggle taken out and `[Options] "Show lights"` forced on at start-up (WoWRevived does both) |

Other changes WoWRevived made to the executable, for reference when reading the listing (`IDA-Output.txt` was made
from `WoW_patched.exe`): the CD Player's "no CD" check skipped, the .smk file check skipped, winmm redirected to
`_inmm.dll`, the resolution slider's creation skipped, and the battle map's drag select border widened from 640 to
2048. Where the listing shows one of these, the retail behaviour is what the docs should describe.

A **Modern Options** menu for the opt-in toggles below is planned (PLAN.md, Stage 6); it can wait.

- **The start files are never saved over**: the game's save writes slot 0 to `Human.000` / `Martian.000`, the
  files every new campaign starts from, so a campaign that reached the war map without a slot replaced them with
  its own state (0x4042D0; docs/ODDITIES.md): what happens in the original whenever a new campaign is started
  with the network version enabled. The engine doesn't save a campaign that has no slot from 1 to 5.

## Options

### Graphics

- **HUD scaling**: a slider, 1x / 2x / 3x / 4x, for the war map's and battle map's panels, menus and cursors. The
  upper limit is an `.ini` setting, so larger steps can be allowed later for bigger screens.
- **Resolution scaling**: render at the original's resolution and scale the picture up, or render the views at a
  larger size (more of the map shown).
- **Windowed mode** and **fullscreen** (with the window's size remembered).
- **Scroll-wheel zoom** on the war map and the battle map: the mouse wheel zooms in and out, by 1x a step by
  default (`.ini` sets the step and the limits).

### Files

- **Every data folder overridable**: a loose file in `DAT`, `MAPS`, `SFX` or `VOX` replacing the copy in that
  folder's archive. The original does this for `DAT` only (a loose `dat\<name>` replaces Dat.wow's entry,
  0x414DE0). For `MAPS` it looks in a folder `Map` that doesn't exist (0x414FAB), so loose map files are ignored;
  the engine has that one fixed already (a modern fix, above). The sound libraries have no such rule; this
  option would give `SFX` and `VOX` the same.
- **Modern formats** accepted for the overriding files (PNG for sprites, WAV / OGG for sounds, and so on), so a
  fix or a mod needn't be encoded in the game's own formats.
- **Enhanced Assets** (an option of the earlier patched game, WoWRevived): every sprite extracted, scaled up and
  back down, which gives cleaner, more vivid colours. Off for parity work: a frame taken with it on doesn't match
  the discs' sprites pixel for pixel (update 158 ran into this on the war map's panel). To offer as an option.
- **Re-encoded sprites** (the owner's idea, a general enhancement): run every `.spr` through WoWViewer's RLE
  encoder (`SprEncoder.cs`), which packs tighter than whatever the game's files were made with and decodes to
  identical pictures, so the data gets smaller at no cost.
- **The owner's fixed files** (`WoWEngine/dat`, `WoWEngine/maps`: the WoWRevived patches for this version of the
  game), as laid out since update 166:
  - `dat` (loose): re-encoded sprites (the owner's RLE encoder packs them better than the original tool did) and
    the Cotton Factory and Airship model fixes. They work today as loose files in the game's `DAT` folder.
  - `dat/warmap`: the alternative war map backgrounds (`MWM`, `TEMP-HWM` / `TEMP-MWM` and their `HI` sector maps,
    `mmini`), to be chosen between by resolution.
  - `dat/extra/<width>x<height>` (640x480 up to 1920x1080): WoWRevived's menu backgrounds for each resolution
    (`humanbd`, `martbd`, `legal1`, `legal2`), the files its resolution switch swaps in.
  - `dat/extra/HD`: the Enhanced Assets set (129 sprites: the war map's panels, buttons, counters, calendar and
    so on, scaled up and back down; docs/ODDITIES.md, update 158).
  - `dat/extra/UNITS`: the 24 pixel counter sheets.
  - `maps`: the sector 22 terrain fix (`LAND22.CLS`) and re-encoded minimaps; loose files in `MAPS` since
    update 140.
  How the engine should offer these (repacked into the archives by the installer, or picked at run time by an
  option) is for this stage; the owner: "we will look into it when we get that far". Nothing in the engine reads
  the `extra` or `warmap` folders yet.

### Controls

- **Keyboard bindings**: every key the game uses can be rebound (the scroll keys, Tab for the minimap, Home,
  Backspace's halt, the menus, the cheats' entry), saved in the `.ini`.
- **War map status strip** (in, off by default): `[Modern] "War map status strip"` = 1. A darkened strip at the war
  map's bottom left with the name of the sector or panel button under the mouse, and the engine's notes (a refused
  route). The original has no such strip (its names are tool tips); before update 133 the engine always drew it.
- **Sticky popup menus** (in, off by default): `[Modern] "Sticky popup menus"` = 1. The original's right-click
  popup (war map and battle map) stays open only while the right button is held, and an item acts as a button is
  let go on it. With the option on, the popup stays open after the right button's let go, an item acts as it's
  clicked, and a click anywhere else closes it. To go on the modern options screen as a toggle
  (`WarMap::SetStickyPopup`, `BattleMap::Setup::stickyPopup`).

- **Group keys** (to do; off by default): Ctrl + a digit keeps the selected groups under that number and the digit
  alone selects them again, a second press centring the view on them, as other strategy games have it. The
  original has nothing of the kind: its digit keys send packet 0x12, which no routine handles (checked in update
  150). Earlier notes listed "the group keys" as a missing parity item; they were an assumption from the genre,
  not something found in the game. Not to be added as parity work.
- **Shift adds to the selection** (to do; off by default): a click or a box with Shift held adds the groups to
  the selection, and Shift-clicking a selected one takes it out. The original clears the selection on every
  click and box (packet 4, 0x4976DA); its Shift path (packet 5, 0x497730) only runs with the debug flag
  byte_4D1279 set (update 150). The same goes for it: a modern option, not parity.

### Sound

- **Working sound controls**: now a modern fix, on by default; see "Modern fixes" above.

### Lights

- Decided: the lights toggle goes and the lights are always on; see "Modern fixes" above.

## Game modes

- **Campaign extras**: Martian against Martian, and Human against Human.
- **Multiplayer** (the original's network game is in docs/TASKS.md; this would be a modern take on it).
- **Auto-resolve a battle**: settle a battle on the war map without playing it on the battle map (the engine
  already does this when a sector's map files are missing).

## Tools

- **Level editor**: the battle maps' ground and scenery, and the war map's sectors.
- **War map creator and new campaigns** (the owner's idea): with the level editor, a tool to make a war map
  (the map, its sectors, links and starting forces), so whole new campaigns can be built and played.
- **Save editor**: the war map's objects, research and resources in a save.

## From the earlier patched game

**The credits' hidden end must be kept** (the owner, update 181; they rate it near the top of this list). The
credits picture (GType 0x2F3, credits.spr) ends with a screen the scroll normally stops short of: the team
outside the St Martins Tavern (Dave, James, Mat, Dobbin, Craig, Charlie, Andy; three of the `[Debug]` flags
are named after them, docs/ODDITIES.md). The scroll ends 960 rows from the picture's bottom, and with the
`englishrose` cheat 480 rows from it, holding there 300 ticks (0x40574C, 0x40577C; `Credits`). The engine shows
it from the original file (seen in update 181). WoWRevived's edited credits picture took that screen off and
put "WoWRevived - Thor110" at the end, not knowing it could be reached. For any edited or enhanced credits
picture: keep the team's screen as the last 480 rows, and put added lines above it, where the normal scroll
ends, so both the line and the hidden end are seen.


The repository's earlier work on the original executable (unit fixes, asset updates and so on; see the readme
and the repository) has much that can be carried over: fixes to units' data and updated assets. To go through
once the engine is complete.

## Ideas from players

### Z-Fighter

1. **The Isle of Man (sector 5)**: let the player send construction vehicles there. Today only the Human AI ever
   gets one there; the island could be a base for Ironclads or Flying Machines. As it is it's worth nothing to
   the Martians, and only to a Human player sending Ironclads there before an attack.
2. **Building and room**:
   - an adjustable turret build limit (12 to 50);
   - more ground the AI may build on (its build places are fixed), if possible;
   - larger coastal sectors: in Southampton (sector 28) only one or a few Formidable-class Ironclads find room,
     and Warrior and Invincible-class Ironclads simply vanish;
   - some of the upgrades that were cut from the game, or new ones, or upgrades also helping turrets (Hover Bombs
     also lengthening the Projectile Launcher turret's range, say).
3. **Scenarios and difficulty**:
   - some sectors starting with turrets already built, as the enemy AI's do at a campaign's start (a central
     sector, or the one nearest London, with six Medium Gun posts: "the six guns we had seen", from the musical);
   - pods sometimes bringing Fighting Machines instead of Scout Machines, for a Hard setting (the Human campaign
     is the easier of the two);
   - on Hard, no cheats (so the player can't clear resource shortages or wipe out enemy units in an attack), and
     the enemy feeling its resource shortages too, so hit-and-run raids on its resource buildings pay off.
4. **The Martian Handling Machine repairing Martian buildings and emplacements**, as the Humans' (Modified) Mobile
   Repair Vehicles do theirs.
