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
| HUD scale (a slider) | `[Modern]` "HUD scale" | 100 | at once |
| Menu scale (a slider, 231) | `[Modern]` "Menu scale" | 100 | at once |
| Fade between menus | `[Modern]` "Menu fade" | off | at once |
| Discord rich presence | `[Modern]` "Discord rich presence" | on | the next start |
| Enhanced assets | `[Modern]` "Enhanced assets" | off | at once (since 231; the next start before) |
| Original display settings | `[Modern]` "Original Display Settings" | off | the next time Display Settings opens |
| Play in a window | `[WoW Engine]` "Windowed" | off | at once |
| Stretch the picture | `[WoW Engine]` "Stretch" | off | at once |
| System mouse pointer | `[WoW Engine]` "System Cursor" | off | at once |
| Music playback | `[Sound\Volume]` "CD-Focus" (WoWRevived's own setting) | off | at once |

Two more boxes aren't settings: **Record input** and **Play a recording** switch the game's playback recordings
(below) on and off while playing.

**Music playback** (update 218) is WoWRevived's launcher's option of that name, with its setting and its tool
tip: on, the music plays on while the window is out of focus or minimised (the game pauses it, and the engine
does unless this is on). Until 218 the box in its place was "CD player music continues" (`[WoW Engine]` "CD
Player Music Continues": the music carrying on after the CD player's menu is left), which is not what was
asked for; that setting still works from the ini but has no box.

Every row has a tool tip (`Button::tipText`, the engine's own English; to go into TEXT.ojd with the captions).

The captions have two spaces before them, as WoWRevived's TEXT.ojd has before the game's own check box
captions, so they clear the tick or cross. They are the engine's own English text (there are no strings for them in TEXT.ojd).

### Modern Gameplay (update 246)

A menu of the rules of play the engine can change, under Modern Options in Game Options and in the war map's
menu (command 0x9004, `FrontEnd::ModernGameplay`), laid out as Display Settings' boxes are. Its rows:

| Box | Setting | Default | Takes effect |
|---|---|---|---|
| Upgraded buildings in battle | `[Modern]` "Upgraded buildings in battle" | on | the next battle (also in Modern Options) |
| Unlimited building | `[Modern]` "Unlimited building" | off | at once |
| Full control on bridges | `[Modern]` "Bridge control" (246-248: "Bridges fire back") | off | the next battle |
| Bridges stay broken | `[Modern]` "Bridges stay broken" (249) | off | the next battle |

**Unlimited building.** The game keeps a sector to so many of each type (OBJ.ojd record 52, "max per sector":
20, the turret emplacements 12, the command buildings 1). An item is offered in a factory's or construction
vehicle's list only while the side's objects of that type in the sector (and, for a vehicle, those queued in the
sector's factories) are fewer (0x43C7F0, `WarMap::ItemEnabled`). A factory also won't start or finish a vehicle
while the sector has no room for it: "Max units in sector" or "Max boats in sector" (`[Tweak]`, 15 and 5) of the
player's units there or on their way (0x43AC10, 0x45FF00, 0x45FE60, 0x4604B0). With the option on, for the
player's factories and construction vehicles, on the war map and the battle map:

- the per-sector limit isn't applied, except the command buildings' (58 and 120, limit 1: the war map takes a
  sector's command building as its only one, 0x80AC); turrets, mines, fences, every building and vehicle can be
  built without limit;
- a factory makes vehicles whatever the sector's room.

Still kept: a queue holds 32 entries (0x4604B0), and moving a group into a sector keeps to "Max units in sector"
and "Max boats in sector" (0x42ABD0's refusals 7 and 8; they can be raised in `[Tweak]`). A sector that has more
units than that from its factories keeps them; others can't join it until there are fewer. The computer player's
factories keep every limit. (The setting is `WarMap::unlimitedBuilding`, read at the war map's start and when
the box changes.)

**Full control on bridges** (`[Modern]` "Bridge control"; update 246 as "Bridges fire back", which is still read
when "Bridge control" isn't set; the orders since 249, a player's report). A vehicle's bridge link (+0x18C) is set
as soon as its route's next waypoint is a bridge's node (0x493374), so from heading for a bridge until it's off the
other end. While it's set, the original:
- doesn't aim: the aim (vt+0xC8, 0x491110 for the vehicles) returns at once (0x491123), so the unit neither returns
  fire nor takes Force Fire, whatever hits it;
- doesn't take a move order: the group's move (0x482E60) leaves out every member with the link (0x482F37: their
  distance to the point isn't counted), and with all of them on it, as a group crossing is, nothing is counted and
  the order is dropped (0x482F98).

With the option on, both tests are left out (`BattleMap::Aim`, `OrderGroupMove`; `BattleMap::Setup::bridgeControl`):
the unit aims and fires from the deck, and a new order is taken where it is. The other aim test, a unit under the
ground (+0x84 under 0), stays; on a bridge the height is the deck's, never under 0 (`OnBridge`). *Seen (249)*: the
London battle's lorries sent to the south bank over Westminster Bridge and ordered back to Big Ben's lawn while
crossing: with the option off the second order is dropped and they go on south (one route found); with it on they
turn back (a second route) and stop on the lawn. The firing hasn't been seen with an enemy in range.

**Bridges stay broken** (update 249, the owner's idea). In the original a bridge destroyed in a battle is whole
again at the sector's next battle: the bridges are the sector's scenery, loaded from `dat\<sector>.nsb` by each
battle (EnterBattleMap, with `[Debug] Load scenery` on, the default), and the scenery isn't saved (the scorch is:
the Damage Layer). Nothing in the original repairs on the battle map; the war map's repair is the Mobile Repair
Vehicles' and workshops' (WARMAP.md "Repair": the most damaged unit, each hour, no rand()), and construction
vehicles only build. With the option on:
- **Kept**: a battle's end gives the war map the places of its sector's bridges down (`BattleMap::BrokenBridges`:
  those not built because they were down, and those destroyed in it); the war map keeps them by sector
  (`WarMap::SetBrokenBridges`), with the hours of rebuilding each has had.
- **Rebuilt**: each hour of a sector (after its factories' production), every construction vehicle there (a unit
  with a factory, either side's, not travelling) with nothing queued puts its hours into the sector's first broken
  bridge (`RebuildBridges`); at 96 hours (the Military Bridge's, type 444, 4 days; *provisional*, the scenery
  bridges have no build time of their own) it's taken off the list, so the next battle builds it. A vehicle with
  anything to build does that first: the bridges are its last job.
- **In battle**: a broken bridge isn't built at all (no segments, no route nodes: units go round, or can't cross);
  one with rebuilding hours has the building site's corner flags (0x17B, the marker a construction shows) at its
  two ends (`PlaceBridge`).
- **Saved** beside the save, not in it: `SaveGame\<save>.bridges` (`Human.001.bridges`, `Custom.003.bridges`), one
  line a bridge, `sector x y hours`, written with the save and removed when there are none, read as the campaign
  loads; a new campaign in a slot removes the slot's. So the save stays the original's form, and the original game
  (which doesn't read the file) simply has its bridges back.
- *Seen (249)*: London's Westminster Bridge given as broken: not built, the flags at its north end when it has
  hours (the one difference from no hours); a broken bridge in Oxford rebuilt by its construction vehicle on the
  war map ("Bridge at ... rebuilt" in the log) and the `.bridges` file written again at the save without it.
  *Not yet*: a bridge destroyed in a real battle and seen down at the next one.

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

### Battle zoom and HUD scale (updates 202 and 205)

In battle, with `[Modern]` "Battle zoom" on (it is unless turned off): `+` (or `=`) and the wheel forward go
closer, `-` and the wheel back go further, `R` puts it back. Each step is 1.25 times, about the view's middle.
The zoom is the land's only. The panels (the group icons, the minimap, the popup menu, the information page,
the end box) keep their size, which is the **HUD scale**: a slider in Modern Options, 50% to 300% in quarters
(`[Modern]` "HUD scale", a percentage, 100 unless set), taken at once.

How it is done (`FrontEnd::BattleView`, `BattleMap::SetView`, `BattleMap::Draw`): the picture stays the view
size. The battle map has three sizes: the picture's, the land's view (the picture's over the zoom) and the
panels' space (the picture's over the HUD scale). The land is drawn at its view's size and scaled to the
picture; the panels are drawn in their space and laid on it (below). The mouse comes in the
picture's pixels and is turned into the land's or the panels' as each is hit. With no zoom and the HUD at
100% it is the game's one pass straight onto the picture, the same picture as before.

The limits: the land's view is never less than half the picture nor more than twice it, nor more than the
land; the panels' space is never less than 640 x 480, which they are made for (so at a 640 x 480 view size the
HUD can be made smaller but not larger; at 1920 x 1080 it goes up to 225%). A new battle starts unzoomed.

*Waiting on the testers* (the owner, update 206): the selection's brackets over a unit are drawn with the land,
so they grow and shrink with the zoom and look a little smudged when scaled. That is probably right (kept
small they would sit on top of a zoomed unit); to be decided when a build has been tried.

**The war map** (update 207) follows the HUD scale too: its panels (the calendar, the view buttons and
mini-map, the messages, the Build List, the popup menu, the information page, the boxes) are laid out in the
panels' space (`WarMap::SetHudSize`) and the map, its counters, routes and rubber band stay at the picture's
size. The war map keeps the mouse in the panels' space and turns it into the picture's pixels where it meets
the map (`MapX` / `MapY`: the sector and the counter under it, the rubber band, edge scrolling).

**How the panels are laid on** (`frontend/PanelLayer.h`, both maps): the picture under them is scaled into a
buffer of the panels' size, the panels are drawn on that as the game draws them (so their see-through parts mix
with what is under), and only the pixels the panels changed are put back on the picture, scaled. The rest of
the picture is untouched, so the map or the land is not softened by the HUD scale.

**The maps' tool tips** (update 232; the owner's question at 231) follow the HUD scale too: a sector's name, a
counter's, the panels' buttons' and the Build List's, on both maps, are laid on with the panels (`FrontEnd::Draw`,
`LayPanels`) where the mouse is in the panels' space. Until 232 they were drawn at the original size whatever the
HUD scale. A menu's tool tips go with the menu, at the menu scale.

What it doesn't do yet: the battle's drag box is drawn with the land, so it thickens and thins with the zoom.
The wheel isn't written to a playback recording (the keys are).

### Key bindings (update 209)

The two maps' keys can be changed in WoWRevived.ini, section `[Keys]`: an entry for an action with the key's
name (SDL's names: `Up`, `Backspace`, `F1`, `R`, `=`, `Keypad +` ...). An action with no entry keeps the
game's key. The game's key for an action that has been given another does nothing, unless another action is
given it. `FrontEnd::BoundKey`. There is no menu for it (the owner: the ini is enough for now).

| War map action | Game's key | Battle action | Game's key |
|---|---|---|---|
| War map scroll up / down / left / right | the arrows | Battle scroll up / down / left / right | the arrows |
| War map top / bottom | Page Up / Page Down | Battle top / bottom | Page Up / Page Down |
| War map centre on sector | Home | Battle centre on selection | Home |
| War map unit view / construction view | F1 / F2 | Battle previous unit / next unit | `,` / `.` |
| War map resource view 1 / 2 / 3 | F3 / F4 / F5 | Battle halt | Backspace |
| War map next view | Space | Battle minimap | Tab |
| War map research | R | Battle zoom in / zoom out / zoom reset | `=` / `-` / R |
| War map stop time | Backspace | | |
| War map message history | M | | |
| War map panel | Tab | | |

Not bindable: Escape, Enter, the digits (the war map's speeds, the battle's group keys), Ctrl and Shift, and
the menus' keys.

### The engine's own files (update 209)

The engine's loose files (`WoWEngine/dat` and `WoWEngine/maps`: the owner's fixed and enhanced files, the
backgrounds for each view size in `dat/extra`, the wider war map in `dat/warmap`, the larger minimaps) are
found without being copied by hand:

- **A release** (`tools/make_release.py`) has them as `DAT` and `MAPS` in the release folder. The game is
  installed into that same folder, so they sit beside the game's own files and are read as the game's loose
  files are (a loose file replaces the archive's copy; in MAPS too, the engine's fix).
- **A build run where it was made** looks in the source folder it was built from (`WOWENGINE_SOURCE_DIR`),
  and any build looks in `dat` / `maps` beside the program, when the game is installed somewhere else
  (`GameFiles::AddOverlay`: paths beginning DAT or MAPS look there first).
- **Which background for which view size** needs no swapping of files (WoWRevived's launcher copies the
  size's files over the ones in `DAT`; the engine doesn't): for the view size in use the engine reads
  `DAT\extra\<width>x<height>\<name>` when it is there, and the plain `DAT\<name>` otherwise
  (`Resources::FileFor`), and changes over at once when the size is changed in Display Settings. So the
  files in `DAT` itself can stay the 640 x 480 ones. Repacking them into Dat.wow is for later (the owner).
- `--no-loose` on the command line leaves them out (the game's files only), which is what the parity checks
  use. The log says which folder is used ("Loose files:") and, for each view size, which of its backgrounds
  were found ("View size ...: backgrounds in ...").

### Discord rich presence (update 212)

As WoWRevived has it (`WoWPatches/_inmm/winmm_driver9x.cpp`), in `platform/Discord.*`: `discord-rpc.dll` is
loaded if it is next to the program (or anywhere Windows finds it), and the presence says where the player
is, in the game's language (`[WoW Engine]` "Language"), with the time the session began:

| Where | State / details (English) | Image |
|---|---|---|
| The menus | Deciding the Fate of Earth / Main Menu | `cover` |
| A Human campaign (its war map exists, whatever is over it) | Defending Earth / Human Campaign | `human` |
| A Martian campaign | Invading Earth / Martian Campaign | `martian` |
| The CD player | Listening to the Score / CD Player, the track's name | the track's (`cd_bd1`-`cd_bd7`) |
| A network game (when there is one) | Battling for Dominance / Multiplayer Skirmish, the side as the small image | `network` |

The same application id (1527400154154402022), texts (five languages) and image keys as WoWRevived, and the
same rule for sending: when the state changes, when the CD player's track changes, when Discord (re)connects,
and every 16 seconds otherwise. `[Modern]` "Discord rich presence" = 0 turns it off. The log says whether it
started ("Discord: ...").

- **The DLL must be the 64-bit `discord-rpc.dll`** (the `win64-dynamic` one of discord-rpc's release). The engine
  is a 64-bit program; WoWRevived's copy is 32-bit, for the 32-bit game, and won't load. The DLL is
  `resources/discord-rpc.dll`, with discord-rpc's licence (MIT, Discord, Inc. 2017) beside it as
  `resources/discord-rpc-LICENSE.txt`; the build copies both next to WoWEngine.exe and the release script puts
  them in the release (update 217). The one in the repository is the 64-bit one since update 218 (the owner
  replaced the 32-bit one first added), and the owner has seen it working.
- Windows only. Not tried: there is no Discord here to try it against; the code builds and does nothing
  without the DLL.

### Menu scale (update 231)

The menus are 640 x 480 and the original never scales them: at 1920 x 1080 they sit small in the middle of the
picture. `[Modern]` "Menu scale" (a percentage, a quarter a step, a slider in Modern Options under the HUD
scale's) draws them larger. The slider goes from 100% up to the largest the view size takes (update 233: 225% at
1920 x 1080, the largest of the sizes; 200% at 1600 x 900 to 1680 x 1050; 150% at 1024 x 768 to 1366 x 768;
125% at 800 x 600; at 640 x 480 it can't move), and a larger value in the ini is drawn at that largest. 200%
doubles every pixel; the others mix the four pixels round each point, as the HUD scale does.

- **What is scaled**: the menus (every `Menu`: the main menu and its sub-menus, Load and Save, the war map's and
  the battle map's menus). Not the briefing, debriefing, research and CD player, which are laid out against
  backgrounds made for each view size (MODERN.md "Resolution"), nor the movies.
- **How** (`FrontEnd::Draw`, `FrontEnd::MenuArea`): the part of the picture the menu will cover is taken out, the
  menu is laid on it at 640 x 480 with the HUD's `LayPanels` (frontend/PanelLayer.h: the picture under it scaled
  down, the menu drawn on that so its see-through parts mix with what is under them, and only the pixels the menu
  changed scaled back up), and it is put back. The background behind a menu (the main menu's room, the war map)
  stays at the view size, unscaled.
- **The mouse** (`FrontEnd::ToScreen`): a point on the picture is taken back into the menu's 640 x 480 for the
  controls; the pointer itself is drawn where the mouse is, at its own size.

### The Modern Options menu's slider row (updates 210-214)

The two-column menus' slider is two sprites, both 520 x 32: `hugeslda.spr` (the see-through backing, 2 frames:
plain, lit) and `hugesld.spr` on top of it (the row's outline and the knob, 22 frames: the knob's eleven places
plain, then lit). Their full rows (`hugebut.spr` / `hugebutb.spr`) are 540 wide. The HUD scale row is drawn
540 wide from those same two sprites, both widened the same way (`Canvas::DrawSpriteWidened`, asked for by
`Menu::WidenLast`): a run of 20 columns is put in twice, and the run chosen is the nearest to the middle
where that leaves no seam (the columns after the run are the same as the run's own). So the ends, the
outline, the knob and the dithering are the sprites' own, with nothing stretched and no file to make.

The backing is the owner's own `hugeslda.spr` (update 215), a loose file in the engine's `dat` folder that
replaces the one in Dat.wow: the same size and two frames, made from the owner's pictures (frame 0 the dark
middle with dithered ends, frame 1 the even grey), with the palette entries the game's file uses (10, 12, 14,
17, 52). With `--no-loose` the game's own is used.

(A backing sprite that is itself wider than the control on it is centred under it, should one ever be made.)

### Enhanced assets (update 225)

WoWRevived's Enhanced Assets: the interface's sprites (the war map's panels, calendar and buttons, the popup
menus' icons, the research pictures, the CD player's pictures, the unit counters: 129 files) upscaled,
brought back to their size and re-encoded, which gives them a little more colour depth. They have the game's
file names and sizes and are in `dat/extra/HD`. With `[Modern]` "Enhanced assets" on (a box in Modern Options,
WoWRevived's launcher's name and tool tip; off unless turned on, as WoWRevived ships), a sprite is read from
`dat\extra\HD\<name>` when it is there (`Resources::FileFor`: a named override, the view size's folder,
then this, then the game's). Taken at once since update 231: the box calls `Resources::SetEnhancedAssets`, which
lets go of each sprite whose file changes (`Refresh`), so it is read again from the other place on its next use;
nothing that holds a sprite past a frame (the war map's picture and sector map, the battle's mini picture) is
among the 129 files. Not with `--no-loose` (`Resources::AllowEnhancedAssets(false)`).

WoWRevived's other asset option, **Larger Minimaps**, is not wanted in the engine (the owner, update 226): it
was the only way to a bigger minimap in the original, and the HUD scale does that here. `WoWEngine/maps` has
the regular minimaps; `dat/extra/UNITS` (the 32 x 32 battle unit counters that went with the larger ones)
is unused.

### The game's text (updates 219 and 220)

WoWRevived's updated TEXT.ojd files, one for each language and each tested in WoWRevived, are in `resources`
(`TEXT-EN.ojd`, `TEXT-FR.ojd`, `TEXT-DE.ojd`, `TEXT-IT.ojd`, `TEXT-ES.ojd`); a build and a release have them in
a `resources` folder beside the programs. **WoWLauncher** copies the one for the install's language in as the
game's `TEXT.ojd` (`install::CopyLanguageText`): at the end of an install, by the language the discs' marker
gave, and at its start when the game is installed already. Only that one file is copied, over the disc's;
no copy of the disc's is kept (the owner's decision, update 220). Nothing is copied when the language isn't
known. Once the copy is in, the launcher deletes the five files and their `resources` folder beside the
programs (update 227; the owner: less clutter), so they are only there until the first install or start.
A later reinstall from the discs therefore has no updated text to copy and leaves the disc's (put the
`resources` folder back from the release to get it again). The source's `resources`, which a build run
where it was made uses, is never deleted. The copy goes over the game's file under whatever case its name has (the Italian disc's is
`Text.ojd`); nothing is renamed (the renaming of updates 221 and 222 was taken out again in 223).
The engine itself reads whatever `TEXT.ojd` the install has and copies nothing (update 219 had it do
the copying, with a backup; both taken out).

### The engine's own text (update 226)

The strings the engine adds (the Modern Options menu's title, rows and tool tips, "Next Saves" and "Previous
Saves": 37 of them) are TEXT.ojd strings like the game's own, with ids 1400 to 1437, past the game's last
(1332). `core/EngineText.h` has them in English, French, German, Italian and Spanish.

- **WoWLauncher** appends the install's language's to the game's `TEXT.ojd` when it copies WoWRevived's file in
  (`install::CopyLanguageText`): one more record at the file's end for each id the file doesn't have, in
  Latin-1 as the rest of the file is. The files in `resources` are not touched.
- **The engine** reads them from the install's `TEXT.ojd` (`Resources::String`), and uses its built-in English
  for any that aren't there (an install whose text the launcher hasn't been over, or `--no-loose`'s plain
  disc text).
- A row's tool tip is its caption's id + 20. Tool tips have no line breaks in the file: the menu wraps them
  at 46 characters (`Menu::SetLastTip`). The captions keep to about 30 characters, the width of a column.
- **Not for the original game**: it has room for 1333 ids (0x414410) and would write past its table on these.
  Only the engine's install gets them; WoWRevived's own files stay as they are.
- "Music playback", "Enhanced assets" and their tool tips are WoWRevived's launcher's in every language. The
  rest of the French, German, Italian and Spanish is the engine's own translation and should be read over by
  someone who speaks each.

### The install's language (update 216)

The installer writes the language of the discs into WoWRevived.ini, `[WoW Engine]` "Language" ("English",
"French", "German", "Italian", "Spanish"), with the other settings. It used to be a file of its own,
`language.txt`: an install that has one has it taken into the ini and removed, by the installer or by the
engine's next start.

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
- **The window's size** (update 208): in a window, the size the player last dragged it to (`[WoW Engine]`
  "Window Size", width,height, written as the window is resized, not while it is maximised or fills the
  screen). With none kept yet the window opens at the view size, or as much of it as the desktop has room
  for at the same shape, in the middle of the screen. The picture is scaled to whatever size the window has.
  A new resolution chosen in Game Options while in a window (update 254, the owner's report) sizes the window
  as a start would: the kept size is dropped and the window takes the new view size (`main.cpp` `fitPicture`);
  while filling the screen, it's sized so the next time it goes into a window.
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
- **The movies**: the game's own are drawn in a 640 x 480 picture and scaled up to fill the screen, their shape
  kept. A larger movie (WoWRevived's 1920 x 1080 `.mp4`s) is decoded at its own size, up to 1920 x 1080, and
  shown in a picture of that size (updates 203 and 204), so it reaches the screen at its own shape and
  detail. Before, the decoder shrank it to fit 640 x 480 and it played as a small strip.
  `Mp4Movie` (the size it decodes to), `MovieScreen::PictureW` / `PictureH`.
  - **Where the `.mp4`s come from** (the owner, update 248): they ship separately from the engine (about 1 GB),
    and the game's `.smk` movies (about 384 MB: FMV 41, FMV-Human 152, FMV-Martian 191) are kept and installed as
    now. The engine plays an `.mp4` where there is one and the `.smk` otherwise (`Movie::Open`).
  - **On macOS and Linux** (update 249; `src/platform/FfmpegMovie.cpp`): through the `ffmpeg` program where it's
    installed (on the PATH, or in /opt/homebrew/bin, /usr/local/bin or /opt/local/bin, where a Mac app started from
    the Finder doesn't look). `ffprobe` is asked first for the picture's size and frame rate and the sound's; if
    it isn't there or can't read the file, the `.smk` is played (logged: "... ffprobe isn't installed or can't read
    it; trying the .smk"). The picture comes from one ffmpeg process (decoded and scaled to the size shown, up to
    1920 x 1080, as BGRA down a pipe) and the sound from another (16-bit, the file's rate and channels), read on
    their own as Media Foundation's are. Nothing of FFmpeg is linked or shipped, so the engine starts without it.
    *Seen*: a 1920 x 1080 H.264 / AAC test movie as `FMV\TITLE.mp4` played on Linux, colours right; without
    ffprobe on the PATH, the `.smk` played instead. *Not yet*: the owner's own `.mp4`s, a Mac. AVFoundation (every
    Mac has it, as Windows has Media Foundation) would serve Macs without ffmpeg: a later step, needing a Mac to
    build on.
  - **Removing the `.smk`s** (the owner, 248-249): later, and only where the engine checks first that it can play
    the matching `.mp4` (on Windows, or with ffmpeg); until then the `.mp4`s ship separately and the `.smk`s stay.
- **The menus' fade** is off (MODERN FIX, update 204; `[Modern]` "Menu fade" = 1 puts it back). The game
  covers the view with bigBlack for a few ticks at each change of menu and holds a control's command for five
  ticks meanwhile (0x4065E0). bigBlack comes out dark red in the Human menus' palette and another colour in the
  Martians', and on a modern display the change reads as a flash of colour between menus. Without it a
  control's command is carried out at once. With it on, bigBlack (640 x 480) is laid side by side to cover a
  larger picture (update 203).
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
  the engine has that one fixed already (a modern fix, above). *SFX and VOX in (update 247)*: "Loose sound files"
  below.
- **Modern formats** accepted for the overriding files (PNG for sprites, WAV / OGG for sounds, and so on), so a
  fix or a mod needn't be encoded in the game's own formats. *In*: PNG for sprites (246, OVERRIDE.md) and the
  battle map's textures (240, TEXTURES.md), WAV for sounds (247), OGG (248, with stb_vorbis), units' models from
  OBJ, glTF and GLB (248, docs/MODELS.md; glTF with cgltf). **Buildings stay as they are** (the owner, 248): an IOB isn't a model the game draws
  but a picture rendered beforehand from the game's one view of the battle map, cut into run-length patches (one
  for each lit triangle of a model the developers had) and lit by those triangles' normals. Replacing one with a
  3D model would need the engine to render buildings, which the game never did; their look is reworked with
  texture packs and PNGs instead (TEXTURES.md), whose pictures are cut back into the game's own patches, so a
  building keeps its outline.

### The documented WoWRevived.ini (update 248)

The owner (247): the ini should document itself, with every option in it. Every setting the engine reads (209 of
them: the original's it uses and the engine's own) has its default and a description in `core/SettingsTable.h`.

- **What's written**: under each section a line on what the section is, and for each setting its description
  (where one helps; a list such as `[Keys]` or `[Colours]` only has its section's) and, while it isn't set, its
  default as a commented example: `; Battle zoom=1`. A commented example does nothing: the engine and the patched
  original game read their own defaults, as before. Several defaults aren't fixed values but worked out at run
  time (no `[Screen] Size` means the desktop's resolution on the first start; the sound borders scale with the
  view; the window's size), so writing them as values would change things; as examples they can't.
- **Who writes it**: WoWLauncher, at the end of an install and each time it starts on an installed game
  (`DocumentSettings`, `settingstable::Document`). A setting already set keeps its value and gets its description
  above it; nothing is added twice. The engine itself doesn't write the documentation (its screenshot runs leave
  the ini as it is). The release has `WoWRevived.default.ini` beside the programs: the same with nothing set
  (`wowdump --default-ini`, kept the same as the table by a unit test), not in place of a player's ini.
- **Keeping it**: `IniStore.h` (WoWPatches/Common, shared by the engine, WoWLauncher and the patch DLLs) and
  WoWRevived's C# launcher's `Shared/GameSettings.cs` now keep the file as written: comments and blank lines where
  they are, a changed value changed in its own line, a new value in place of its commented example, or after its
  section's last value. A file in the old form (no comments) is written back byte for byte (a unit test). Before,
  every save rewrote the file without its comments. *The C# side wasn't built here* (no .NET in the container).
- **The list** was made from what the engine reads in headless runs (`WOWENGINE_SETTINGS_LOG=<file>` writes every
  setting read and its default as the program ends; `frontend/Settings.cpp`) and the reads outside `Settings`, and
  checked against the source: every setting the engine reads is in the table and nothing else is. The original's
  settings the engine doesn't read (docs/review/SETTINGS.md: the platform's, the network game's, the developers'
  logging) aren't listed; a new setting the engine reads goes in the table too.

### Loose sound files (update 247; PLAN D1)

A sound sample can be replaced by a loose file, as a DAT file can (README-SOUNDS.txt is the players' version):

- **Where**: `SFX\<name>.wav` for an effect; `VOX\HUMAN\<name>.wav` (`VOX\MARTIAN\` in a Martian game), then
  `VOX\<name>.wav`, for a voice. `.snd` is read too: the libraries' own form, 16-bit mono PCM at the sample's rate
  (the engine read `SFX\<name>.snd` / `VOX\<name>.snd` before). In the game's folders and the engine's own `sfx` /
  `vox` beside the program or in the source folder (`GameFiles` overlays, which now take SFX and VOX paths as
  well as DAT and MAPS; a release copies them as `SFX` / `VOX`). No name is in more than one library (checked:
  SFX.WoW's 202, HUMAN.WoW's 153 and MARTIAN.WoW's entries share none), so the name alone picks the sample.
- **The name** is the sample's in SFX.ojd, as `wowdump --sfx` prints it (`[waroom1@22050]`) or WoWViewer lists
  the libraries' entries. `[Debug] Dave=1` puts "Dave: sound <name>" in the log as each is first used (0x466029),
  which is the easy way to find what a sound is called.
- **OGG** (update 248): `<name>.ogg` in the same places, after the `.wav` and before the `.snd`; Ogg Vorbis,
  decoded by stb_vorbis (`third_party/stb_vorbis.c`, public domain, unchanged; `DecodeOgg`) and then made the
  sample's form as a WAV is. Seen used in a battle headless (every SFX entry as an OGG).
- **WAV** (`core/Wav`): 8, 16, 24 or 32-bit PCM or 32-bit float, any channels and rate; made the sample's rate
  (every sample in the game is 22050 Hz) by straight-line interpolation, and mono, or interleaved stereo for the
  sounds the table marks stereo (opcode 15; 0x465D10 gives those a two-channel buffer: 18 samples, the war room's
  ambients, the briefings, the title, victory and defeat music, the epilogues and OPTION), a mono file doubled.
  For a stereo sound a mono WAV at exactly twice its rate is read as the interleaved stereo it is (248: how
  WoWViewer exported the long ones). A 16-bit WAV at 22050 Hz of the right channels is read exactly.
  The log names each loose file as it's used ("Sound file: ...") or why one isn't.
- **The order**: the loose file first, the library's entry when there's none, as the original does with its
  developers' flag on. `[Sound]` "Search Libraries First" (0x465EB0, byte_58FE78; off unless set) puts the library
  first, the loose file only for a sample no library has (0x4660D4).
- **The original's own loose sounds** (0x466910) are looked for only with a developers' command-line flag
  (byte_4D12F4, from WinMain) and in other places (the name as given, `SFX\HUMAN\` / `SFX\MARTIAN\`,
  `VOX\English\` and `VOX\English\HUMAN\` / `MARTIAN\`, each `<name>.snd`); not copied, as nothing ships in them.
- *Seen working*: effects replaced in a battle headless (every SFX entry as a WAV, each logged as used). *Not yet
  seen*: a voice line (none played in the short test battle); it's the same code.
- **WoWViewer's export** (fixed in 248): it wrote each entry as `NAME.wav` with a mono header and a rate guessed
  from the length (44100 for over 30 s at 22050). Every entry that long is one of the stereo samples, so those came
  out the right length (stereo at 22050 is 44100 samples a second) and the engine reads them back exactly (above).
  The short stereo ones (the briefings, victory and defeat, OPTION) came out as mono at 22050, half speed. Since
  248 WoWViewer reads SFX.ojd (`WoWViewer/SfxTable.cs`, a port of `SoundTable::Load`'s opcodes) and writes, plays
  and draws each entry at its own rate and channels, and its Replace checks a WAV against them. *Not built here*
  (no .NET in the container): to be built and tried on Windows.
  *PNG for sprites is in (update 246)*: with Enhanced assets on, any `.spr` frame is replaced by a PNG beside it,
  named as WoWViewer exports the frames (`<name>_frame_NN.png`, or `<name>.png` for a one-frame sprite), at the
  frame's size or 2, 4 or 8 times it; drawn in full colour where the game draws the sprite as it is, and matched to
  its colour table elsewhere (`Resources::ApplyPngOverride`, `Canvas::DrawColour`). The rules are in docs/OVERRIDE.md.
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

- **Campaign extras**: Martian against Martian, and Human against Human. *Designed with the owner (update 236)*:
  "The civil wars" below.
- **Multiplayer** (the original's network game is in docs/TASKS.md; this would be a modern take on it). The owner
  (update 238): in the long run the network game should be just another menu option, not something that needs
  the game started with the network version enabled (and the menus that changes, such as Single Player Options).
- **Auto-resolve a battle**: settle a battle on the war map without playing it on the battle map (the engine
  already does this when a sector's map files are missing).


### The civil wars (designed with the owner, built in update 236)

Two new campaigns, each one faction against itself (PLAN.md F1).

- **Human Civil War**: the Humans divided, on the Human war map. **Martian Civil War**: set after the Humans have
  lost the Earth, the Martians divided, on the Martian war map. **No pods** in either (`WarMap::DropPod`): both
  sides grow by their own construction and production. Tried headless for a fortnight of game time each: the
  Martian computer player builds a Communications Centre, makes units and invades its neighbours without pods.
- **Where they are chosen**: the main menu has "Human Civil War" (under Start Human Game) or "Martian Civil War"
  (under Start Martian Game) under Start New Campaign (command 0x9003), then the slots and the name as for a new
  campaign, and straight to the war map (no intro: the faction's tells of the Martians' coming).
- **The start** is made when a civil war begins, from the side's own start file (`core/CivilWar`,
  `MakeCivilWarStart`), so no start files of the engine's are shipped. The split mirrors the regular game's, the
  player starting from the opposite side to the regular game's. *A first draft, to be evened out with the owner*:
  - Human Civil War, from Human.000: the Martians taken out; the Human objects and sectors in Scotland (sectors 1-4
    and 6-9) stay the player's, every other one goes to the other side (London's: 17 sectors to the player's 3);
    the player gets a copy of London's objects in the Grampian Mountains (3).
  - Martian Civil War, from Martian.000: the Humans taken out; the Martians' base in the Grampians becomes the
    other side's, and the player gets a copy of it in London. Every other sector is no one's.
  - A copied HQ takes its new sector's form (London's Command Post 278 is a 58 elsewhere, the Grampians'
    Communications Centre 830 a 120); `WarMap::HasCommandPost` takes either form in a civil war.
  - **The layout is a text file** (update 237): `dat\CivilWar.txt`, shipped in the engine's `dat` folder (DAT in a
    release), says for each civil war which sectors' objects stay the player's, go to the other side or are taken
    out, which sector's objects the player's home gets a copy of, and where they go (`CivilWarLayout`). It's read
    when a civil war begins, so a change is tried with a new campaign and no rebuild; lines it can't read are noted
    in WoWEngine.log ("CivilWar.txt line n: ..."). Without the file (or with `--no-loose`) the same draft is built
    in (`CivilWarLayout::Default`; a unit test checks the two agree). The homes can't be moved by it: the war map,
    the battle map and the endings know them as the Grampians (3) and London (30) by the campaign.
- **Sides and factions** (`core/Campaign.h`): the save's H lists are always the player's side and the A lists the
  other's, whatever their factions, so the engine already thought in sides. What changed is every place that took
  the other side to be the other faction, which now asks the campaign (`WarMap::OtherIsHuman`): the other side's
  research tree, command building, the computer player's rules (AI.ojd's sites for its faction), its facilities, the
  Martian "infestation" count, the research and build times' Friend / Enemy modifiers (`WarMap::ModifierHuman`),
  the battle's research (asked by side in a civil war: `BattleMap::ResearchDone` / `ResearchMultiplier`), and the
  homes and endings (`WarMap::EndsCampaign`, `PlayerHome`, the battle's end box). In the regular game each gives
  what it gave before; the two parity battles are unchanged.
- **The economy**: each sector's six stock slots were the Humans' 0-2 and the Martians' 3-5. In a civil war the
  player keeps its faction's slots and the other side has the other three (`WarMap::SlotBase`), while its facility
  types and deposits stay its faction's (`FactionBase`, `FactionKind`), so the two sides never share a stock.
- **Each side's unit table** (update 237): the war map keeps a second copy of the unit table for the other side
  (`ObjOjd::OtherSideUnits`, `WarMap::Units(players)`, the battle setup's `otherUnits`), each type marked as the
  other side's, which takes its faction's Enemy armour modifier ([Tweak] "HumanEnemy / MartianEnemy Armour
  Modifier") whatever the side played. Only the full-health lookups choose the table, by the object's side
  (`WarMap::UnitMaxHealth`, `BattleMap::MaxHealth`); everything else reads the one table as before. In the regular
  game the copy gives the same numbers (the other side's types are the other faction's, which take the Enemy
  modifiers already); in a civil war the other side's units now take the Enemy ones where they took Friend. The
  copy is also where per-side data would go: the other side's textures for a texture pack, say.
- **Damage**: the game gives a hit the target's armour from the target's faction and the attacker's improvements
  from the other faction's tree. In a civil war the attacker is the same faction, so there: the target's armour
  from the target's side and the attacker's faction's improvement from the attacker's side.
- **Colours**: the war map's borders and mini-map already go by side (the player's and the other's), so they tell
  the two apart unchanged. On the battle map the other side's units are drawn through their shade tables with the
  hue turned 150 degrees (`ShadeTable::HueRotated`, `BattleMap::OtherSideHue`): blue-violet against the player's.
- **Saves**: `HumanCW.00n` / `MartianCW.00n` in SaveGame, with the engine's tag at the start (`WOWE`, the tag's
  version, the campaign; core/Campaign.h) and then the original's layout, the research holding the player's side's
  block and then the other's. The original game lists only `Human.00n` / `Martian.00n`, so it never meets one. The
  Load menu lists the regular saves and then the civil war's, shown with "(Human Civil War)" / "(Martian Civil
  War)" after the name (the stored name is the player's); Save and Select Campaign to Replace list the kind being
  played or started.
- **Endings** (update 237): no movie. The regular victory and defeat movies are about London falling or the
  Martians being driven from Scotland, so a civil war's end goes straight to the credits (FrontEnd, 0x80FE).
- **Texts**: the debriefing's "The Martians / Humans in this sector were eliminated" and "... are occupying this
  sector" say "the enemy" in a civil war (engine strings 1454, 1455). The menu entries and tags are 1450-1453.
  The French, German, Italian and Spanish are the engine's own wording.

**Not done yet, or to decide with the owner:**
- The start layouts' balance (the Human Civil War's draft gives London's side 17 sectors to the player's 3).
- No movies at either end (the owner: none for now).
- The other side looks the same on the war map's counters (the owner: maybe retextured later) and is told apart
  on the battle map only by the hue turn, until a texture pack gives each side its own textures (below).
- Voice lines that name the other faction ("Martians sighted" and the like) are the regular game's: to be reworked
  for the civil wars later (the owner).
- The Single Player Options menu (with the network version enabled) has no civil war entry yet (moot once network
  play is a menu option, above).
- `CivilWar.txt` says only which sectors' objects go where and where the player's copied base goes. It can't yet
  set how many units or buildings a sector has, or add any: `add` / `remove` lines are the obvious next step.
- `ReceiveSpecial`'s Martian research (items 132 / 133) is asked of the receiving unit's side: *provisional*.
- A battle between the two sides hasn't been played through headless (their forces start far apart); the colours
  were checked on the regular pod battle made to count as a civil war.

## Tools

- **Level editor**: the battle maps' ground and scenery, and the war map's sectors.
- **War map creator and new campaigns** (the owner's idea): with the level editor, a tool to make a war map
  (the map, its sectors, links and starting forces), so whole new campaigns can be built and played.
- **Save editor**: the war map's objects, research and resources in a save.
- **Texture packs** (the owner, update 237; planned; the folders and the rules for the files are in docs/TEXTURES.md,
  update 238): new textures for both sides' units and buildings, which would
  also give the civil wars' two sides looks of their own (the other side's from its unit table, above). The first
  step is done: `wowdump --textures <game folder> <outdir> [level]` writes every unit's and building's texture as
  an 8-bit BMP into human/units, human/buildings, martian/units, martian/buildings and scenery, keeping the
  game's colour indices, with the battle map's midday shade table at a level (44 of 64 unless given) as the
  palette. Which side a model is on comes from the unit table (each type's model, +0x28, and its parts', which
  are unit types of their own); a model no type names goes by its shade slot (1 Human buildings, 2 Human
  vehicles, 3 Martian vehicles, 4 Martian buildings, 10 nature).
  - A unit (WOF) has one texture atlas 256 pixels wide that its faces map into: repainted in place, its layout
    kept.
  - A building (IOB) isn't textured. It's a picture rendered beforehand from the game's one view, cut into
    run-length patches, one per lit triangle of its model, each placed from the building's anchor and lit by its
    triangle's normals. `IobModel::Picture` puts them back together; the export writes each building's
    `<name>.patches.txt` beside it, with each patch's pixel bounds in the picture, so a reworked picture can be cut
    back into the same patches as long as the outline stays. Patches can overlap (the later drawn over the
    earlier), which is a likely reason building edits have gone wrong in other tools.
  - For now (the owner, after 237): the pack is made in any colours and quantised back to the game's indices and
    shade tables on the way in, so it works with the engine as it is (and the original). The export's palettes
    are what to quantise against: each side's midday table at the level the export used.
  - **Read since update 240** (with Enhanced assets on): PNGs in `TEXTURES` beside the program, laid out as the
    export, quantised on the way in; a unit's atlas replaced in its WOF and a building's picture painted back into
    its IOB's patches (`IobModel::Repainted`); the civil war's other side from `civilwar/<faction>/` without the
    hue turn. The rules for the files are in docs/TEXTURES.md. The engine reads PNG itself (`core/Png`: inflate and
    the PNG format, no library). This is the first of the common-format overrides below, for the textures only.
  - Later (the owner's plan): **overrides in common formats**. Game files overridden by loose files in ordinary
    formats (PNG / BMP for sprites and textures, OBJ for models, and the like) read in place of the archive's,
    so packs and mods need no conversion. Not designed yet; the engine's loose-file overlay (`GameFiles`,
    "The engine's own files" above) is where it would hang. Full-colour textures lit by the engine would come
    with it. *Sprites since update 246*: docs/OVERRIDE.md.

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

- **Unlimited building** (the owner, update 245). *In (update 246)*: "Modern Gameplay", below.
- **Custom campaigns** (the owner, update 245). *In (update 246)*: docs/CUSTOM.md. A text file each in
  `Campaigns` in the main folder (update 247; `dat\Campaigns` in 246) (kind, name, start file, pods, a civil war's layout lines), the main menu's "Custom Campaigns"
  under the civil war, saves `SaveGame\Custom.001` on with the engine's tag and its flags, and a Load menu of
  their own ("Resume Custom Campaign").
- **Human units fire back from bridges** (the owner, update 238). *In (update 246)*: "Modern Gameplay", above
  (0x491123: the aim's bridge link test).

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
- **A separate UI scale for the menus** (noted at update 230). *In (update 231)*: "Menu scale" below.
  The note as it was: The HUD scale slider sizes the two maps' panels;
  the menus (and the other 640 x 480 screens) are drawn at their own size, centred in the view and never scaled
  (MODERN.md "Resolution"), so at 1920 x 1080 they are small. A second slider in Modern Options would scale them
  (say 100% to 225%, as the HUD's, or whole steps for sharp pixels), with the mouse turned into the menus' space as
  the HUD's is. To settle: whether the menus' backgrounds scale with them or stay the view size's own.
- **Enhanced assets without a restart** (noted at update 230). *In (update 231)*: "Enhanced assets" above.
  The note as it was: The box takes effect at the next start now. The
  pieces are mostly there: `Resources::SetEnhancedAssets` already empties the sprite cache (`Refresh`), so the
  next frame reads each sprite again, from `dat\extra\HD` where a file of that name is there and the game's
  otherwise (`Resources::FileFor` checks which files exist); only `main.cpp` reads the setting, once, at the
  start. Calling it when the Modern Options box changes may be all it takes; to check, the sprites something holds
  on to past a frame (the war map's and battle map's own copies, if any).

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
