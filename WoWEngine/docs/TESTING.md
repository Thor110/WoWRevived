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
- **Units on bridges fire back** (the same): a Human vehicle attacked while crossing a bridge returns fire; Force
  Fire given to one on a bridge; with the option off, neither (the original).
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
