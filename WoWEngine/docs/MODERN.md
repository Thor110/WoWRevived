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
| Volume sliders saved | The Sound Control sliders never write the registry, so the volumes are lost | **In** (marked, 191): Master as it moves, Ambient and Speech as the menu closes, written to `WoWRevived.ini` |
| Music toggle | The Sound Control check box only saves the setting; the music steps read it when they next come round | **In** (191): off stops the track at once, and either way the music step of the map behind the menu is brought forward |
| Master volume on the music | Master doesn't reach the music's playback | **In** (marked, 191): Master scales the music |
| Resolution applied at once | The slider only takes effect after a restart | **In** (192-195): the menus, the other 640 x 480 screens and both maps take the view size at once; the movies are scaled up. See "Resolution" below |
| Desktop resolution on the first start | (no such thing) | **In** (192): with no `[Screen] Size` yet, the view size is worked out from the desktop's and kept; afterwards the player's choice stands |
| No 8 / 16-bit colour choice | Two radios in Display Settings; 8-bit doesn't work on modern systems | **In** (191): not offered; `[Modern]` "Original Display Settings" = 1 puts them and the lights toggle back |
| Loose map files | The game marks MAPS.WoW's entries by looking for `Map\<name>` (0x414FAB), a folder it doesn't have, then reads `maps\<name>`: a loose map file is never used | **In** (update 140): `core/Vfs` looks in `maps\`, so a loose file in `MAPS` replaces the archive's, as in `DAT` |
| No "Lights" toggle, lights always on | With lights off, Infiltrate, which makes a light, crashes the game | **In** (191): the toggle not offered and the lights always on, unless `[Modern]` "Original Display Settings" = 1 |

Other changes WoWRevived made to the executable, for reference when reading the listing (`IDA-Output.txt` was made
from `WoW_patched.exe`): the CD Player's "no CD" check skipped, the .smk file check skipped, winmm redirected to
`_inmm.dll`, the resolution slider's creation skipped, and the battle map's drag select border widened from 640 to
2048. Where the listing shows one of these, the retail behaviour is what the docs should describe.

A **Modern Options** menu for the opt-in toggles below is planned (PLAN.md, Stage 6); it can wait.

- **The start files are never saved over**: the game's save writes slot 0 to `Human.000` / `Martian.000`, the
  files every new campaign starts from, so a campaign that reached the war map without a slot replaced them with
  its own state (0x4042D0; docs/ODDITIES.md): what happens in the original whenever a new campaign is started
  with the network version enabled. The engine doesn't save a campaign that has no slot from 1 to 5.

- **Upgraded buildings fight as their new level** (update 186; `[Modern]` "Upgraded buildings in battle", on;
  `WarMap::FightType`). The game upgrades a building's war map object and leaves its battle object the first
  level's (docs/ODDITIES.md), so a Vehicle Factory Level 3 stands on the battle map as a Level 1. Every level
  has data of its own, checked for all of them: the factories, ship yards, hangars, constructors, suspension
  field sites and training centres add parts and armour by level (the Martian training centre a new main
  model too); each gun post, emplacement, anti-aircraft array, heat ray, turret and launcher level has its
  own model, gun and armour; the fences and mines their own part or marker. The fix uses the war map type in
  a battle when it is the battle type's upgrade, with the saved health scaled to the new level's and back.
  The save is still written the game's way (the battle object's type untouched), so it loads in the original.
  Off, a building fights as the level it was built as, as the original has it.

### The Modern Options menu (update 196)

The engine's own menu, command 0x9001 (no such command in the game), opened by a "Modern Options" button in
Game Options and in the war map's Campaign Options. It is laid out as the critical event menu is, two columns of
check boxes, and each box is a setting in WoWRevived.ini that can be set there as well:

| Box | Setting | Default | Takes effect |
|---|---|---|---|
| Upgraded buildings in battle | `[Modern]` "Upgraded buildings in battle" | on | at once (the next battle) |
| Popup menus stay open | `[Modern]` "Sticky popup menus" | off | at once on the war map, the next battle |
| War map status strip | `[Modern]` "War map status strip" | off | at once |
| Group keys and Shift select | `[Modern]` "Selection keys" | on | the next battle |
| More save slots | `[Modern]` "More save slots" | on | the next time the Load or Save menu opens |
| Battle zoom (+, -, wheel, R) | `[Modern]` "Battle zoom" | on | at once |
| Original display settings | `[Modern]` "Original Display Settings" | off | the next time Display Settings opens |
| Play in a window | `[WoW Engine]` "Windowed" | off | at once |
| Stretch the picture | `[WoW Engine]` "Stretch" | off | at once |
| System mouse pointer | `[WoW Engine]` "System Cursor" | off | at once |
| CD player music continues | `[WoW Engine]` "CD Player Music Continues" | off | at once |

Two more boxes aren't settings: **Record input** and **Play a recording** switch the game's playback recordings
(below) on and off while playing.

The captions are the engine's own English text (there are no strings for them in TEXT.ojd).

### Group keys and Shift select (update 198)

In battle, with `[Modern]` "Selection keys" on (it is unless turned off):

- **Ctrl + a digit** keeps the selection under that digit. **The digit** alone selects what was kept (those
  still on the map), and pressed again with them already selected moves the view to them. In the game the
  digits build a "select group n" packet (0x12) that nothing acts on, so they do nothing there; Ctrl with
  the arrow keys is untouched.
- **Shift** held while picking adds to the selection instead of replacing it: a drag box, a click on one of
  the player's vehicles, or a click on a group icon. It only adds vehicles to vehicles; a building is still
  selected on its own. Shift has no meaning in the game's battles (on the war map it has, and that is as it
  was).

Both work through the game's own selection (0x439000): the war map objects picked become one battle group, as
a drag box over several makes them.

### Battle zoom (update 202)

In battle, with `[Modern]` "Battle zoom" on (it is unless turned off): `+` (or `=`) and the wheel forward go
closer, `-` and the wheel back go further, `R` puts it back. Each step is 1.25 times, about the view's middle.

How it is done: the battle is drawn at a different size and the window scales the picture to the screen
(`FrontEnd::BattlePicture`, `BattleMap::SetViewSize`). Further out draws more of the land in a larger picture;
closer draws less in a smaller one. The limits: never smaller than 640 x 480 (so at a 640 x 480 view size
there is no going closer than the game's own view, only further), never more than twice the view size, never
more than the land. A new battle starts unzoomed, and a menu over the battle is drawn unzoomed.

What it doesn't do yet: the panels (the minimap, the group icons, the popup menu, the end box) are part of the
picture, so they shrink and grow with the land. Keeping them at their size is the HUD scaling of PLAN.md C4.
The wheel isn't written to a playback recording (the keys are).

### Resolution (agreed with the owner, update 192)

- **The view size** (`[Screen]` "Size") is one of the thirteen sizes WoWRevived has menu backgrounds for, 640 x
  480 to 1920 x 1080. Nothing is drawn larger: past 1920 x 1080 everything would be too small to play, so a
  bigger screen gets the finished picture scaled up.
- **The first start** (no "Size" yet): the desktop's own size if it is one of the thirteen; past 1920 x 1080 the
  size nearest the desktop's shape (a 4K screen gets 1920 x 1080 at twice the size); any other, the largest
  that fits, of the desktop's shape if there is one. `FrontEnd::ViewSizeFor`.
- **The picture on the screen**: its shape kept, with bars where it doesn't fill (`[WoW Engine]` "Stretch" = 1
  fills instead); sharp pixels when it goes in a whole number of times, smoothed when it doesn't. Full screen
  unless `[WoW Engine]` "Windowed" = 1; Alt + Enter changes it while playing.
- **Backgrounds are never scaled.** The main menu's and the legal screens' (`humanbd`, `martbd`, `legal1`,
  `legal2`) are read from `DAT\extra\<width>x<height>\` in the game folder when the size has them there
  (`Resources::SetViewSize`), and drawn from the top left at their own size. The briefing, research and CD
  player backgrounds are the owner's 1920 x 1080 ones, loose in `DAT`, drawn from the top left and cut off by
  the picture's edges.
- **The controls are centred** both ways: a 640 x 480 screen is drawn from the middle 640 x 480 of the picture
  (`Canvas::SetOrigin`, `FrontEnd::Origin`), and the mouse is taken from there.
- **The war map** (193) is drawn at the view size: its panels keep to the picture's edges as they do in the
  original, the menus that open over it are centred, and a 640 x 480 screen over it (a briefing box, a movie)
  is drawn in the middle. `WarMap::SetViewSize`, called when the setting changes; the view's middle stays
  where it was. A save holds the camera as the original's 640 x 480 view would (the same middle), so a save
  made at any size opens in the original at the same place.
- **The war map's background**: the original picture (1498 wide) while the view is no wider than it, so there
  is no empty map to scroll into; the extended ones (1921 wide, with the Martians' coast of Europe, and their
  mini-map) only for a view wider than that. Both are 2544 high. The extended ones are read from
  `DAT\warmap\` in the game folder (`TEMP-HWM.SPR`, `TEMP-MWM.SPR`, the two `HI` sector maps, `mmini.spr`;
  `Resources::SetFileOverrides`). They hold the original picture at the left with 423 more columns of sea on
  the right, so sectors, counters and saves need no shifting. If they aren't there, the original picture is
  drawn in the middle of the view with black at its sides.
- **The battle map** (194) is drawn at the view size (`BattleMap::Setup` viewW / viewH, `SetViewSize` for a
  change while it is on): more of the land is seen, nothing is larger. The minimap keeps to the bottom right
  corner and its rectangle is the view's share of the land; the edge scrolling's speed goes by the view's
  width as the original's does (0x484DF0); the group icons wrap at the view's width; the end box is at the
  bottom middle; the Battle Options menu is in the middle. The sound's listener is the view's middle, and the
  borders are scaled as the original scales them for a larger screen (0x464B10, ODDITIES.md).
- **The briefing, debriefing, research and CD player** (195) are the exception to the centring: they are drawn
  at the top left. The owner's 1920 x 1080 backgrounds for them have the original picture's frames there (the
  briefing's two panels, the CD player's boxes, the empty drawer the research cabinet goes in), with the rest
  of the room drawn around them, so any view size shows the right part. `FrontEnd::Origin`.
- **The movies** are drawn at 640 x 480 and scaled up to fill the screen, their shape kept.
- **Where the files go**: the engine looks for `extra\<size>\` and `warmap\` inside the game's `DAT` folder,
  so the installer has to put `WoWEngine/dat/extra` and `WoWEngine/dat/warmap` there (PLAN.md D2).

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

### Playback recordings (the owner, update 183)

The original's self-running demo (docs/PLAN.md 4.6, `core/Playback.h`) can't be switched on in the retail game.
The engine has it as the game does, the files and their format the game's own, and for now switches it from the
command line: `--record` writes the session's input to `Playback.1` in the game folder, `--playback` plays
`playback.1` to `playback.N` (`[Demo]` "Playback Files", 4), one file each time, and drops real input until the
file ends. Since update 197 both can also be switched from the Modern Options menu: "Record input" starts
writing `Playback.1` with the next input and stops when unticked; "Play a recording" plays the next file. A
recording made from the menu begins with the menu open, so it plays right when started from the same menu in
the same place. Escape ends a playback (the engine's own; the game drops every key until the file ends). Two things the engine adds, because a
recording is only raw input by frame number: with either switch the random numbers start from 1, so a recording
meets the same game it was made in; and a key the game has no virtual key for is kept as its own code with the
top bit set. A recording is only good from the state it was made from (the same start, the same save).

### Buildings the game names and never offers (the owner, update 186)

To be restored as options once the rest is done. What the data holds:

- **Matter Transfer Station** (type 122, Martian). A whole building in the data, never offered:
  - *It has*: its model `dat\MT_Main.iob` (GType 499) and six more parts (`MT_leg1`-`4`, `MT_mainb`,
    `MT_maint`, all in Dat.wow); armour 1500; 30 days to build; 80 days of research; its own war map counter
    (frame 124); its own standing sound (the type's +0xA2, sound 50, as the Repair Facility beside it has 49);
    an information page (text 600: "Matter transfer stations allow the rapid distribution of materials over
    great distances.") with its spoken version (sound 390); its name on the research screen (521) and
    "Researching Matter Transfer Station" (522).
  - *It lacks*: a builder (records 49 to 51), so nothing can be told to make it; and its "research done" text
    is string 523, which reads "Unused": the developers' own mark that it was dropped.
  - *From the owner* (update 187): it can't be made buildable in the original, but hex-edited into a save it
    exists; it doesn't seem to do anything. Their understanding of what it was for: helping the economy by
    evening out or linking resources between sectors, which is what the page's text says.
  - *Not yet looked at*: whether it is anywhere in the research tree's data; whether any code names type
    122; what the economy's distance cost (`[Tweak]` Distance Crippler VX / VY, the spread of supply between
    sectors) would have to do for a station to "distribute over great distances".
  - *To do, after everything regular works*: the constructors as its builders behind an option; its research
    item; an effect on the economy designed and agreed with the owner; its "research done" line written.
- **Railway Platform** (type 59, Human). Little more than a name: the strings 404 to 407 (its name twice,
  "Researching Railway Platform", "Completed research for Railway Platform"), a war map counter frame (95),
  and a page's text that nothing points to (567: "Construction of a temporary rail depot helps to speed up
  transportation."; the type's own page is 0, so no spoken version is tied to it). In the unit table it has
  no side, no model, no armour, no building or research time and no builder, and there are no railway tracks
  in the data. It needs a model made before it can be anything; until then it stays behind a testing option.
  By its text it would have sped up the Humans' movement between sectors, as the station was to speed up the
  Martians' materials.

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

## Ideas from the owner (noted at update 199; not planned in detail yet)

- **More save slots.** *In (updates 200 and 201), as pages*: the game has five slots; the Load and Save menus
  keep their five rows and get "Next Saves" and "Previous Saves" rows, up to slot 999 (the three digits of
  `Human.001`), so the menu is the same at every view size. The pages go by what there is, not by the slots'
  numbers (the owner, update 201: a save dropped in as `.999` beside a `.001` must not be 199 pages away):
  - *Load* lists the slots that have saves, lowest first, five to a page. With no more than five saves it is
    the game's menu as it was.
  - *Save* (the slot for a new campaign) lists the game's five, empty or not, then the slots past them that
    have saves, and one "Empty": the lowest free slot past the five. So there is always room for one more
    campaign, and its first page is the game's with a "Next Saves" row.
  The files are the game's own, numbered on: the original game only lists the first five.
  `FrontEnd::MenuSlots`, `AddSlotPageButtons`, command 0x9002.
- **New strings added to TEXT.ojd by the installer.** "Modern Options" and every other caption the engine adds
  are plain English text in the code now. Better: the install routine appends them to the end of each
  language's TEXT.ojd, so each language can have its own. To settle when it is done: the ids they get (past
  the last the game uses, so nothing of the game's moves), the engine falling back to its built-in English
  when a TEXT.ojd doesn't have them, and who supplies the other languages' wording.
- **Zoom controls.** *In (update 202)*: `+` and `-` or the mouse wheel to zoom the battle map, `R` to reset
  ("Battle zoom" above).
- **A trailer for the project.** The very last thing: once the engine is at 1:1 parity and the modern options
  are in, and everything else is done.
- **The log.** Whether WoWEngine.log should only be written when there is a crash. *Decided for now*: it stays
  on always. A log written only at the crash would hold the fault's address and nothing of what led to it,
  and the lead-up (which screen, which commands, the last battle or save) is what makes a report usable; a
  stall (the watchdog's STALL line) isn't a crash at all and would leave nothing. It costs little: a few
  kilobytes, started afresh each run. Since update 199 the run before's log is kept as
  `WoWEngine.previous.log`, so starting the game again after a crash doesn't lose it.

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
