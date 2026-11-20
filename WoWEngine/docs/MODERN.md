# Modern options and ideas

Things the engine could offer beyond the original game, gathered from the project's notes. None of these are in
yet unless marked. The original's behaviour stays the default; each option here is opt-in, and set in the
engine's `.ini` where it says so. This is a list to come back to once the original is complete (milestones 1-5),
not a plan with dates.

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

### Sound

- **Working sound controls**: the Sound Control menu's sliders applied to the mixer at once (master, effects,
  speech, ambient, music), and kept in the `.ini`.

### Lights

- Either remove the lights toggle (`[Options] "Show lights"`) or fix what made it a problem in the earlier
  patched game (the Infiltrate skill crashing with the lights off). The engine's Infiltrate (0x80F4) doesn't
  touch the lights, so the crash shouldn't carry over; this is to check once both are in.

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
