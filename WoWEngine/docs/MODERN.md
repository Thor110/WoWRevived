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
| No "Lights" toggle, lights always on | With lights off, Infiltrate, which makes a light, crashes the game | *To do*: the toggle taken out and `[Options] "Show lights"` forced on at start-up (WoWRevived does both) |

Other changes WoWRevived made to the executable, for reference when reading the listing (`IDA-Output.txt` was made
from `WoW_patched.exe`): the CD Player's "no CD" check skipped, the .smk file check skipped, winmm redirected to
`_inmm.dll`, the resolution slider's creation skipped, and the battle map's drag select border widened from 640 to
2048. Where the listing shows one of these, the retail behaviour is what the docs should describe.

A **Modern Options** menu for the opt-in toggles below is planned (PLAN.md, Stage 6); it can wait.

## Options

### Graphics

- **HUD scaling**: a slider, 1x / 2x / 3x / 4x, for the war map's and battle map's panels, menus and cursors. The
  upper limit is an `.ini` setting, so larger steps can be allowed later for bigger screens.
- **Resolution scaling**: render at the original's resolution and scale the picture up, or render the views at a
  larger size (more of the map shown).
- **Windowed mode** and **fullscreen** (with the window's size remembered).
- **Scroll-wheel zoom** on the war map and the battle map: the mouse wheel zooms in and out, by 1x a step by
  default (`.ini` sets the step and the limits).

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
- **Save editor**: the war map's objects, research and resources in a save.

## From the earlier patched game

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
