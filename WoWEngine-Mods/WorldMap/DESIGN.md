# World Map: design notes

Ideas for a world campaign, gathered while making the draft map (README.md). None of it is planned or built; it is
here so the thinking isn't lost. The original's behaviour stays the default in WoWEngine, so all of this would be a
mod, or a mode of its own.

## One world at the original's scale (the owner's direction)

The aim now is a single world map at the original war map's own scale, so that the original British Isles, at their
original resolution, are a part of it, and the detail is kept everywhere: about 58,800 x 48,400 pixels for the whole
world with Antarctica (README.md, "The world at the original's scale"). Our engine has no sprite limit to meet, so:

- **The map is drawn in tiles**, only those in view; perhaps cut into sections in the end, perhaps not.
- **The draft world map** (8192 wide) is its **mini-map**.
- **The British Isles** can be the game's own, set into the world at this scale with the world eased to meet it, or
  the version made from the data; both are at the same scale (`scale-tile/britain-original-and-ours.png`).
- **Sectors** for the world are to be worked out from maps of the time (below, "The empires of 1898"); their density
  needn't be Britain's everywhere. With the art in layers and the borders only in the sector map, they can be drawn
  and redrawn without touching the art. At this size a sector map wants more than 8 bits: 16-bit values (65,535)
  hold several thousand sectors with their borders.

The rest of this file was written for the smaller draft and still holds where it says so.

## Three maps: world, war and battle

The world can't be drawn at the original's scale (README.md: about 17,000 sectors, and a map 100,000 pixels wide).
A chain of maps keeps the original's detail where the fighting is:

1. **The world map** (this draft): 62 regions, at about 4.9 km a pixel. The strategic layer: where the pods come
   down, which regions each side holds, movement between regions by land, sea and air.
2. **A war map for each region**, like the original's: its own sectors (up to 70 or so, the 8-bit sector map's
   limit, as worked out in README.md), its own art, its own sector map. **The original Human and Martian maps are
   the British Isles' war map, unchanged**, at the original scale: the British Isles are about 280 pixels square on the
   world map, and the original is about nine times that.
3. **The battle maps**, as now: a battle in a war map's sector.

Each step down keeps the 8-bit limits of the step above, so no single map has to hold the world, and the original
campaign stays exactly as it is, as one region of the world.

What it would take in the engine:

- **The war map made from data, not the executable.** The sector tables (bounds, waypoints, links, the eight border
  numbers each, the names) are read from the original's executable into `WarMapData.h` now. A region's war map
  would load them from its own file instead (`world.json` is that file for the world map).
- **Saves with more than 30 sectors**, and a world level above them, in a format of the engine's own, while the
  original's saves still load as the British Isles' war map.
- **The map's size**: 28 million pixels for each map at 8192 wide, about 60 MB for the two maps and their sector
  map, against the original's 11 MB. Fine for the engine; not for the original game.
- **The mini-map panel**, made for a tall map of Britain, needs a wide version.
- **Names**: TEXT.ojd has the 30 sectors' names at 0x003-0x020; a mod's names would come from its own text file in
  each language, as the engine's own captions do (C8 in PLAN.md).
- **Battle maps** for the regions that have none: the 30 existing ones, chosen by terrain, as stand-ins at first.

## Pods: asking where to land

In the original the pods come down where the game decides. A world campaign could let the Martian player **ask for
a landing in a chosen region or sector**, with a limit on how many can be sent, which matches the lore: the
Martians are a dying race on a dying world, and every cylinder they send is a part of what is left.

- **A pod budget**: a number of cylinders for the campaign, or for each launch, and none to waste.
- **Launch windows**: Mars comes closest to Earth at opposition, about every 26 months, and the novel's cylinders
  were fired at an opposition. Pods could only be launched in a window, with a long wait between, so each
  window's choice of targets matters.
- **The flight**: the cylinders take time to arrive, and land with some scatter around the aim: the nearer the
  target to the night side, or to the edge of the disc as seen from Mars, the wider the scatter.
- **The Human side** sees a landing coming, with less warning the further from an observatory (a building, or a
  sector's property, as the original's research gives information).

## Crossing the sea

On a world map the oceans matter. In the original, Humans have ships (Ironclads, the Thunder Child) and the
Martians have none, and that would hold:

- **Humans** cross by sea, which needs a way to carry land units: a transport (a new unit), or the existing ships
  carrying units between coastal sectors that share a sea area (`world.json` lists them).
- **Martians** can't cross the oceans with Fighting Machines. In the novel the Fighting Machines wade into the
  shallows (the Thunder Child fights them off the Essex coast), so narrow, shallow crossings (the Channel, the
  Bosporus, the Strait of Gibraltar) could be open to them as links between sectors, and the oceans not.
- **Flying Machines** could cross the oceans, but they can't build, so a new foothold across an ocean would still
  need a pod (above) or a new unit that can carry a constructor.

## The empires of 1898

The original has one Human side. A world campaign could have the empires of the time:

- **The powers of 1898**: the British, French, German, Russian, Austro-Hungarian and Ottoman Empires, Italy, Spain,
  Portugal, the Netherlands, Belgium (the Congo Free State), the United States, Japan and Qing China, with Persia,
  Siam, Ethiopia and the American republics holding their own.
- **The time itself**: 1898 is the year of the Spanish-American War (Cuba and the Philippines change hands), of
  Fashoda (Britain and France face each other on the Nile), and a year before the Boer War; much of Africa had only
  just been carved up, on lines drawn in Europe. Starting holdings would come from a map of the world in 1898
  rather than from today's countries; the sectors could then follow the empires' and colonies' borders instead of
  today's (README.md: the borders are only in the sector map, so redrawing them doesn't touch the art). One open
  source of historical borders is the *historical-basemaps* project's world in 1900 (on GitHub; its licence to be
  checked before use).
- **Factions**: the player as one empire, with the others run by the computer; or the player in charge of all of
  them, humanity as one side as in the original; and the Martians against any of these.
- **The empires among themselves**: allies of necessity against the Martians, rivals over what's left, or both. At
  the least, an empire's units only enter another's sectors when they are allied.
- **Colours**: the original lights its borders with eight solid colours (shade levels 59-66) for three owners
  (Human, Martian, no one). More factions would need more levels in the war map's shade table, or the engine
  colouring the borders itself.

## Smaller things noticed

- The pods' landing scatter, the observatories and the empires would each want their own entries in the save; the
  original's save format has room for none of them, so a world campaign would be saved the engine's own way.
- The war map's counters, routes and messages would work unchanged on the regional war maps; on the world map they
  would want a coarser look (an army per region, rather than a counter per unit group).
- The original's AI (AI.ojd) knows nothing about sectors beyond its war map; a world AI would be new.
