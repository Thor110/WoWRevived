# Testing in play

What hasn't been seen working in a real game yet: each was built from the disassembly and tried headless as far as a
script can take it, but needs someone to play it. Started at update 247 (the owner: "aside from proper testing of
everything"). When something here is confirmed, say so and the update it was seen in; when it's wrong, the report
goes to the item's doc.

**What helps a report**: WoWEngine.log (beside the program; the run before is WoWEngine.previous.log), a screenshot,
and the save it happened in (SaveGame), plus the same moment in the original game where it can be had. A difference
from the original is the most useful thing to report: the engine's aim is to play as the original does, and the
Modern options are the only deliberate differences.

## Modern options and files (updates 240-247)

- **PNG sprite overrides** (OVERRIDE.md, README-OVERRIDE.txt). The shading: a PNG is drawn in full colour where the
  game draws the sprite as it is, and matched to the sprite's colours where the game shades or fades it. If
  colours go wrong in the second case (a fading menu, a shaded battle map sprite), the matching may need reworking
  (`textures::Indices` against the sprite's level 0, `Resources::ApplyPngOverride`). Try a menu button, a
  background, a war map counter and a battle map icon, each at 1x and at 2x or 4x.
- **Texture packs** (TEXTURES.md, README-TEXTURE-PACKS.txt): a reworked unit and building through a whole day on the
  battle map (the shading as the light changes); a civil war's other side from `civilwar/`.
- **Unlimited building** (MODERN.md "Modern Gameplay"): more than 12 turret emplacements and more than 20 of one
  unit type in a sector; a factory finishing vehicles in a sector that holds "Max units in sector" already; that
  moving a group into such a sector is still refused; that the computer player keeps the limits; one command
  building per sector.
- **The `.mp4` movies on Linux and macOS** (249; MODERN.md "The movies"): the owner's `.mp4`s with ffmpeg
  installed, on Linux and on a Mac (started from the Finder too); without ffmpeg, the `.smk`s.
- **Bridges stay broken** (249; MODERN.md): a bridge destroyed in a battle, the sector left and fought in again
  (the bridge still down, units going round); a construction vehicle in the sector rebuilding it (the flags at its
  ends) and the bridge back after; a save with a broken bridge passed on with its `.bridges` file.
- **Full control on bridges** (the same, "Bridge control"): a Human vehicle attacked while crossing a bridge returns
  fire; Force Fire given to one on a bridge; with the option off, neither (the original). The new orders were seen
  working headless (249).
- **Custom campaigns** (CUSTOM.md, README-CUSTOM-CAMPAIGNS.txt): each example played for a while; a definition with
  its own start file in the main folder's Campaigns; a Custom.nnn save copied to another install and loaded there;
  a Martian custom campaign beside a Human one (each listed only in its side's game, neither replacing the other's
  save).
- **Unit models** (248; MODELS.md, README-TEXTURE-PACKS.txt "Models"): a model made in a 3D tool (Blender) as OBJ
  and as glTF, with new shapes for some parts, in a battle; an animated unit (a walker) with a changed part; a gun's
  part changed, checking it still fires from its muzzle.
- **The documented WoWRevived.ini** (248; MODERN.md): WoWLauncher on an existing install (every setting documented,
  nothing changed); the old WoWRevived launcher saving a setting after (the comments kept: its C# side wasn't built
  here); an uncommented example taking effect.
- **WoWViewer's sounds** (248): playing, exporting and replacing a stereo sound (a briefing) and a mono one; SFX.ojd
  found beside the game's folder (not built here).
- **Menus**: Modern Gameplay from Game Options and from the war map's menu; Custom Campaigns and its load menu; at
  the larger menu scales.

## A tester's report (update 253)

- **Defence counters** (WARMAP.md "Counters"): turrets, mines and fences built during a game share one counter a
  type and sector with a number, as the ones the start file has do ("2/3" while one is going up).
- **The Build List in a battle** (BATTLEMAP.md): it closes when the construction vehicle is deselected (a click on
  open ground, another unit selected), not while placing; it opens centred down the screen at large resolutions.
  Not tried headless (no scripted battle with a construction vehicle).
- **The research screens**: each page's picture behind its texts, on both sides (missing since update 181; seen
  headless, Martian at 640 x 480 and 1920 x 1080, Human at 640 x 480).

## Campaign authors (update 272)

- Each shipped campaign's briefing headed "Designed by ..." in the game's language; Highlands Alone, Quiet Skies and
  Stranded (no briefing text) show a briefing of that line and Begin Campaign.

## Custom campaigns from the WotW Companion, narration, Auto Resolve (update 270)

- The 14 campaigns (the separate zip's Campaigns folder in the main folder): each listed for its side, started,
  its briefing read, and its war map as the scenario says. "Secondary Line Holds" is a repaired copy (it had been
  through a text-mode copy).
- Narration: the three with an `.mp3` read their briefing out as it's shown, at Master x Speech, and stop on Begin
  Campaign. Not heard headless (no sound device there).
- Auto Resolve Battle, Battle Options' bottom button: in a battle with both sides, the side with more men left takes
  the sector, the debriefing counting the other's objects; with only one side, as Return to Warmap.

## CD Music Volume (update 268)

- Sound Control's new slider under Master: the music (war map, battle map, menus, the CD player) quieter and louder
  as it moves, at once; Master still over it; kept in the ini; its name in each language's install.

## Fences under construction (update 267)

- A fence placed on the battle map (and one loaded unfinished): the first post standing and a flag at every other
  post's place along the line, as in the original; units walk through the flags; once finished (on the war map),
  the next battle has the whole fence. Not tried headless: a new game's construction vehicle has no fences yet.

## After the owner's first look at 265 (update 266)

- **Heat rays on the Human HQ**: Martian rays aimed at the HQ drawn over its corner sandbags, as in the original.
- **Building after a battle**: destroy an enemy HQ and leave the battle at once, before the rest of its base has
  finished exploding: the sector is yours on the war map and you can build there.
- **Enemy bases outlast their HQ** (Modern Gameplay): with it ticked, the enemy's other buildings stay after its HQ
  goes; the sector stays theirs until they're destroyed. Without it, the original.
- **Skip briefings** (Modern Gameplay): no Briefing on the way into a battle, no Debriefing on the way out; the
  result still announced on the war map, and the campaign saved.

## The testers' and the owner's reports (update 265)

- **Installer**: an Italian and a French install from their discs (the manual copied as `wotw-man-italian-v1.pdf` /
  `wotw-man-french-v1.pdf` beside TEXT.ojd); an English install (`wotw-man-english-v1.pdf` from resources); the
  playlist `HumanMartianPlaylist.m3u8` in the main folder either way.
- **Ground impacts**: a shell or rocket landing on open ground shows its impact (the crater, smoke and light parts).
- **Firing effect**: rockets' and mortars' flashes at their record's length, not drawn out to the target.
- **A builder lost**: a construction vehicle killed part way through a building, the building destroyed with it;
  its Build List entries gone and the list closed if it was open.
- **The computer's fences**: they run between their two points, not as a single post.
- **Human research**: the lamp cord under the papers; the Martian picture and the proposals under it see-through.
- **Cheats**: punyhumans takes the Humans, atchooo the Martians, whichever side is played; in a civil war the war's
  own faction's cheat takes the other side and the other does nothing; no cheats in a network game.
- **A new building**: placed and building, with no fire or smoke until it's damaged.
- **The minimap**: no dots for the other side's units under the fog.
- **Research speech**: stops on leaving the screen and changes with the page.
- **Retreat arrows**: red (Human) or purple (Martian), each pointing off its nearest edge.
- **Deployment**: attackers coming in from the side of the sector they came from, in the campaigns and in both civil
  wars (where the sides used to be confused, putting one at the top left); a land unit from a sector whose pair has
  only a boat point comes in near it.
- **War map Build List**: centred at screen sizes over 480 rows; closed by a press on the map.
- **Battle Options**: the last button (Modern Gameplay) on the rounded bottom plate.
- **Saving after a battle**: quit (or end the program) just after a battle and load: the battle's result is there.
- **The Human Civil War**: each side starts with two Construction Vehicles (the Rebels' in the Grampians).
- **CD music**, war map and battle map: against the original, how often it plays. The engine now holds the music off
  once a track has been stopped part way (a menu's Resume, or a step coming before a track's end) until the next
  briefing or the war map's return (WARMAP.md "Music"). If it's now too quiet, say so: a Modern option could bring
  back playing on.

## Custom campaigns' own files (update 264)

- A save made by the original game renamed Custom.nnn: listed under Resume Custom Campaign for its side only, and
  played (seen headless with a copy of Martian.000). To try: saving it, and it loading again as a tagged save.
- A campaign with an intro (.smk seen headless; an .mp4 to try), a briefing of two pages, and a reskin folder (39
  recoloured Martian unit pictures used in a battle, Enhanced assets off, seen headless). To try: the outro movie
  on winning; a resumed save finding them through Custom.nnn.campaign in play (seen headless in the log).

## Battle Options (update 262)

- The battle map's menu (263: in the war map's order, Sound Control among them): Critical Event Options, Sound
  Control, Modern Options and Modern Gameplay, each back to it
  and Resume to the battle. Seen headless (under AddressSanitizer, Enhanced assets toggled in a battle too). To
  try: settings changed in a battle taking effect there (the HUD and menu scales, the window, the cursor).

## Civil war sides and the weed (update 261)

- Choose Side for each civil war: the Rebels and the Resistance start as before; the Loyalists in London, the
  Imperialists in the Grampian Mountains. Seen headless: each starts in its home, and a saved second side loads as
  it (unit test). Back from the slots returns to Choose Side. To play: a whole civil war on the second side,
  including its end (losing the home, taking the other's).
- The red weed: a battle at the Martians' base in the regular Martian game (and the Human one) has red land and red
  weed, the first battle after starting included (it was green: the shade digit was the battle before's). Seen
  headless at noon. At night a Martian game's battle map is green as a whole, units and buildings too: its night
  tables (the Y set, LANDR?Y and the BM tables) are the Martians' night view, left as they are. In the Martian Civil War, the Resistance's sectors green, the Imperialists' red
  (seen headless, from both sides).

## Music playback on the main menu (update 260)

- With Music playback on (Modern Options), the main menu minimised or left for another window: its music goes on.
  Off, it stops and comes back with the focus. In the game (war map, battle) the sounds still stop with the window.

## Crash logs (update 258)

A crash on Windows now lists the calls that led to it in WoWEngine.log ("CRASH: the calls:"), each with its
module and offset, and with WoWEngine.pdb beside the program (a release since 258 has it) the routine's name, file
and line. Keep the .pdb with WoWEngine.exe. Two reports are waiting on such a log:

- **Modern Options opened from the war map's menu** (0.2.8, a tester: "war map: drawing", reading address 0xB418).
  The owner found it: Enhanced assets toggled while playing (update 259). Resources let go of every decoded frame,
  and the war map's map pictures, kept as pointers, were then read freed (AddressSanitizer: heap use after free in
  `WarMap::DrawMap`). Now nothing handed out is freed (`Resources::Retire`), and the war map gets its pictures
  again (`Resources::Generation`). Seen headless under AddressSanitizer: toggled three times from the war map's
  menu, then the map and the research screen, clean. To try: toggling it in a battle, and the new look appearing.

## The crash after a battle (update 256)

- A campaign played on past battles where the computer player's construction vehicles lose vehicles and then
  build again (the tester's log: a Martian campaign, a battle in sector 12, the crash in the next hour). The
  cause is covered by a unit test; the game itself not replayed.
- Turrets, fences and mines built while others of the type stand in the sector: the new one goes up over its
  time, the old ones stay finished.

## Resources (update 255)

- A resource building (iron, coal, oil; the Martians' three) placed on the battle map with a construction vehicle,
  left to finish on the war map: the Resource view's bar for the kind rises in its sector and less in the sectors
  near it; a second one in the same sector adds less than the first (r B / (n + r - 1) each). Seen headless
  through the war map's order and its hours, with the building given 1 health first as a battle hands it back.
- Other buildings placed on the battle map are finished at their full health too (a repair vehicle no longer
  goes to them).

## The window (update 254)

- Play in window (Modern Options), then a smaller or larger resolution in Game Options: the window takes the
  new size at once, the picture unstretched in it (before: only after a restart). The same full screen, then
  Play in window. Alt + Enter both ways after it. Not tried headless (the test driver has no real window).

## The civil wars (updates 236-237)

- The balance of the start layouts (the Human Civil War's draft gives London's side 17 sectors to the player's 3).
- A battle between the two sides played through: their forces start far apart, and the colours (the other side's
  hue turn) were only checked on a regular battle made to count as a civil war.
- The computer player over a long game without pods.

## The battle map (updates 239-245)

- **Building on the battle map** (BATTLEMAP.md, PLAN.md A10): placing each kind of building, turret, mine and fence
  with a construction vehicle; the green and red placement; the construction going up; vehicles ordered from the
  battle map finishing on the war map.
- **Fences** (BATTLEMAP.md "Fences"): built, shot through post by post, the wires; vehicles kept out by them.
- **The special orders** (BATTLEMAP.md "The modes and specials"): Black Dust, the scans and the other specials with
  enemies present (in 245 a group ordered to Black Dust a point with no enemy near set off but wasn't seen to
  arrive or fire; the original didn't fire in the same save either).
- **Bridges** breaking under fire and the units on them (PLAN.md 2.2).
- **The route finder spread over updates** (PLAN.md 2.1): large groups ordered across a big map at once; nothing
  stalls or stutters.
- **The update order** (244): units and groups as the original, newest first: watch for anything that now acts a
  tick early or late (a unit firing, a group arriving).

## The war map

- Repair (WARMAP.md "Repair"), the counters' pips, the Build List cancel-only for construction vehicles.
- A whole campaign won and lost on each side, with the endings.

## Sound (update 247 on)

- **Loose sound files** (247; MODERN.md "Loose sound files", README-SOUNDS.txt): a replaced effect heard (seen
  used in the log headless, never listened to); a replaced voice line in VOX\HUMAN and VOX\MARTIAN (not seen
  yet); one of the stereo war room sounds; a WAV at 44100 Hz and one in stereo for a mono sound;
  Search Libraries First on.
