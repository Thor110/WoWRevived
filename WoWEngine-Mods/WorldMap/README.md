# World Map (draft)

A first draft of a world war map for a future WoWEngine mod: the whole world as war map sectors, in the style of the
original Human and Martian maps, with the sector map that sits under them. Nothing here works in the original game or
in the current engine, both of which have the 30 British sectors built in; it is material for later. The ideas for
how it could be used are in [DESIGN.md](DESIGN.md).

## Two scales

- **The draft world map** (below): 8192 x 3443, a whole-world view at about 4.9 km a pixel. Its use now is the
  **mini-map** of the world at the original's scale.
- **The world at the original's scale** (`scale-tile/`, the section after this one): the original war map's own
  scale, about 0.40 km a pixel across and 0.44 down, so that the original British Isles, at their original
  resolution, are a part of the world map. A proof of concept, one 8192 x 8192 tile of it.

## The world at the original's scale (`scale-tile/`)

The world frame is Miller cylindrical, which reaches both poles (Antarctica included; Mercator never reaches
them), scaled separately across and down so that at 54 N a pixel is exactly the original's: 0.40 km across,
0.442 km down (`tools/fit_britain.py`). **The whole world is then 58,823 x 48,404 pixels**, 2.85 billion, about 750
times the original map. Our own engine has no sprite limit to meet; it would draw the map in tiles, only those in
view (about 2 million pixels at 1080p), never holding the whole of it.

`tools/poc.py` builds one 8192 x 8192 tile of it, at world position 23027, 8953 (28 W to 22 E, 37 N to 69 N): Iceland,
the British Isles, Scandinavia, France, Iberia, Italy and central Europe. 6 x 6 such tiles (plus a strip) make the
world. The tile is built from:

- **Coastlines:** GSHHS at full resolution (from the `basemap-data-hires` package; the same data as NOAA's GSHHG),
  about 0.1-0.2 km, finer than a pixel.
- **Heights:** NOAA's ETOPO 2022 at 15 arc-seconds, about 0.46 km, the map's own scale.
- **Height bands by relief, calibrated on Britain:** lowland, upland and highland are cut by how far the land rises
  above its surroundings (50 km about) with its height mixed in, and low coastal plains stay lowland. The cuts are
  set so that Great Britain has the original's shares (43% lowland, 28% upland, 29% highland). Cutting by height
  alone at Britain's levels made the whole continent highland; by relief, the Alps, the Pyrenees, the Massif Central
  and Norway are highland and the plateaux between them upland, as a map of the time draws them.
- **Texture** from the original maps, as for the draft; the Martian sea and weather made, the weather in the sea only.
- A graticule every 5 degrees on the Human sea, as faint as the original's grid.

| File | What it is |
|---|---|
| `HWM-tile.SPR`, `MWM-tile.SPR` | The tile, both sides, in the original maps' colour indices |
| `human-tile-quarter.png`, `martian-tile-quarter.png` | The tile at a quarter size, lit as the game lights it |
| `south-east-england-1to1.png` | 1000 x 600 pixels of both, at full size: London, Kent, the Channel |
| `britain-original-and-ours.png` | **The game's Britain beside ours**, the same size and scale, both 1498 x 2544 |

The comparison shows the two at the same scale: England and Wales agree, and the original's Scotland is drawn
larger and pulled north, freely, as the fit found. Either can be the British Isles of the world map: the game's own
(set into the world at this scale, the world's coasts round it eased to meet it), or ours. The art carries no
coastline (the sector map's coastline pixels draw it in the game); the Human map's lowland green is close to its
sea, so a faint shore tint in the art could be worth adding for anyone editing the bare art.

The full-size PNGs of the tile (23 and 45 MB) are left out of the package; `python3 tools/poc.py` makes them again in
about nine minutes. It needs the 12 ETOPO 2022 15" tiles N75/N60/N45 x W030/W015/E000/E015
(www.ngdc.noaa.gov/thredds/fileServer/global/ETOPO2022/15s/15s_surface_elev_netcdf/) and `gshhs_f.dat` /
`gshhsmeta_f.dat` from `basemap-data-hires`.

Sectors for the world at this scale are left until they can be worked out from maps of the time (DESIGN.md).

## The whole world at the original's scale (`world/`)

`tools/worldtile.py` builds the world at the original's scale, a tile at a time (8 x 6 tiles of 8192 x 8192, the last
column and row smaller): **58,823 x 48,404 pixels, both poles, Antarctica included.** Every part of it is worked out
from world positions, the texture's offsets and the noise of the Martian sea and weather included (hashed lattice
noise that wraps round the world), and the colour scales are fixed once for the whole map, so the tiles join
without seams. `tools/stitch.py` puts them together.

What changed from the proof of concept (`scale-tile/`), after comparing with the original's Britain:

- **The coastline is drawn into the art**, two pixels on the land side, in the original's own line colours, as the
  original's art has it. Without it the land faded into the sea; it was most of what made the colours feel wrong.
- **The bands are coherent and nested**, as the original's are (green lowland, tan round the hills, brown on top):
  the relief is smoothed before it's cut, each band takes the majority of its neighbourhood, and specks of a band
  smaller than about 1.5 km square are folded into the band below.
- **Flat land is lowland**: below 58 m anywhere, and the great plains (within 15 m of their surroundings, under
  350 m) whatever their height, so the plains of northern Europe and Russia are green as a map of the time has them.
- **Calibrated on Britain**: the cuts are set among the land the rules leave open, so that Great Britain has the
  original's shares: 42.5% lowland, 27.6% upland, 29.9% highland against the original's 43.4, 27.6 and 29.0
  (`tools/consts.json`).
- **Heights read against their region**, so every region shows its own lowlands, hills and mountains as the
  original shows Britain's: a pixel's height against the mean and spread of the land round it (about 300 km, from
  `tools/regional.npz`, a coarse grid of the whole world made by `tools/preview_bands.py --regional`, so every tile
  reads the same values and they join), with the local relief added, and the great ranges and plateaux (above
  1,500 m) raised whatever their region. Cutting by Britain's heights alone made the whole world highland brown.
- **Rules before the cuts**: flat plains under 350 m are lowland, flat high plateaux upland, the polar ice sheets
  upland (a pale tint) with their ranges highland.
- **Texture offsets hashed for every 512 pixel square** from its place in the world, so the original's texture
  doesn't repeat in a visible pattern over large areas of one band. **Still to fix:** the squares' edges show
  faintly, most on the Martian land at full size (`places/new-york-martian.png`) and on Antarctica's ice at a
  distance; blending each square's texture into its neighbours' over a few dozen pixels would hide them.
- **The Martian weather systems are larger**, at the world's scale (a few hundred to a thousand kilometres).

| | |
|---|---|
| `world/world-human-1-8.jpg`, `world/world-martian-1-8.jpg` | The whole world at an eighth of the original's scale, 7352 x 6050 |
| `world/world-*-1-16.png` / `.jpg` | At a sixteenth, 3676 x 3025 |
| `world/places/` | Full size crops, 1600 x 900: the Alps, the Nile delta, the Himalaya, Japan, New York, Cape Horn, the Cape of Good Hope, the Antarctic Peninsula |
| `world/britain-original-and-ours.png` | The game's Britain beside the world's, the same scale |

The full size tiles (96 PNGs, about 3.6 GB) are not in the package. `python3 tools/worldtile.py calibrate`, then
`python3 tools/worldtile.py tile <col> <row>` for each of the 48 (about seven and a half minutes each, 4.8 GB of
memory at the most, so two or three at once on 16 GB), then `python3 tools/stitch.py`, make them again; the 288
ETOPO 2022 15" tiles (5.1 GB) go in `data/etopo15`. `python3 tools/preview_bands.py` draws the whole world's
bands in flat colours at an eighth of the scale in two minutes, for trying the band rules.

## The questions

### Can the world be made to the original map's scale?

No. The original map was fitted against Natural Earth's outline of Great Britain (`tools/fit_britain.py`): it is
close to a Transverse Mercator projection, stretched about 10% from north to south, at **0.40 km a pixel across
and 0.44 km a pixel down**, or about 2.4 pixels a kilometre. England and Wales match closely; Scotland is drawn
freely (pulled north, with some of the western isles left out of the sector map).

At that scale the world would be about 100,000 pixels across and 60,000 down: some 6 billion pixels for each of
the two maps, and wider than a sprite can be (a sprite's width and height are 16-bit, 65,535 at most).

### How many sectors would the world need?

At Britain's density (30 sectors for about 230,000 km², one for every 7,700 km²) the world's land, about 131 million
km² without Antarctica, would need **about 17,000 sectors**. The sector map is 8-bit, so it can hold 255 values in
all, sectors and borders together. Even a 16-bit sector map (65,535 values) would not hold 17,000 sectors and the
50,000 or so borders between them.

So this draft keeps the original's rules and fits the world into them: **62 sectors**, each a region of the world
rather than a county. Britain and Ireland are one sector (308,000 km², about 280 pixels square). DESIGN.md describes how the
original's detail could still be kept, with a map for each region.

### How many colours does the sector map need?

The sector map's pixels are numbers, not colours: each value gets a shade level from the game, and the map is
lit through it. Every sector, every border between two sectors and every coastline needs a value of its own, and the
sea is 0. The original uses 116 values: the sea, sectors 1-30, then borders 1-86 as values 32-117 (border 64
unused, between the borders between sectors, 1-63, and the coastlines, 65-86).

This draft follows the same layout:

| Values | What | Count |
|---|---|---|
| 0 | The sea, lakes, and islands left out of the sectors | 1 |
| 1-62 | The sectors | 62 |
| 63 | Unused, as the original leaves 31 | 1 |
| 64-170 | Borders between two sectors | 107 |
| 171 | Unused, as the original leaves border 64 | 1 |
| 172-229 | Coastlines, one for each sector on the sea | 58 |
| | **In all** | **230 of 256** |

26 values are left: room for a few more sectors, but each new sector also brings its borders (about 2.7 on
average here). The sea areas (`CHI.SPR`, below) are numbered 192-233 in their own map, as the original numbers
its 22 from 192.

The original keeps 8 border numbers for each sector (0x4D0C00). Two sectors here have exactly 8 (Tibet, and
Egypt and the Sudan); none has more.

## The map

- **Projection:** Miller cylindrical, centred on 11 degrees east so that the map's edge falls in the Bering Strait,
  from 72 north to 56 south (no Antarctica).
- **Size:** 8192 x 3443 pixels, 4.9 km a pixel at the equator (about twelve times the original's), 28 million pixels
  against the original's 3.8 million. `tools/build.py` takes another width.
- **Sectors** are made of whole countries (Natural Earth, 1:50m), and the large ones are split by whole states and
  provinces (1:10m), so the borders follow lines on the ground. Islands smaller than 300 pixels belong to no
  sector, as the original leaves some of the western isles out. Lakes are 0.
- **The names** are geographic rather than political ("The Western Steppe", "New Granada", "Turkestan"), as the
  borders of 1898 were not today's.

| File | What it is |
|---|---|
| `DAT/HWM.SPR`, `DAT/MWM.SPR` | The Human and Martian maps, 8192 x 3443, in the original maps' colour indices |
| `DAT/HWMHI.SPR`, `DAT/MWMHI.SPR` | The sector map (the same for both sides, as the original's are) |
| `DAT/HMINI.SPR`, `DAT/mmini.spr` | The mini-maps, 256 x 108 (a 32nd; the original's is a 15th, and wouldn't fit the panel at this size) |
| `DAT/HMINIHI.SPR`, `DAT/mminihi.spr` | The mini-maps' sector maps: the sectors only |
| `DAT/CHI.SPR` | The sectors drawn smaller and the 42 sea areas numbered from 192, as the original's CHI.SPR, for placing counters and ships |
| `world.json` | Every sector (name, value, bounds, a waypoint inside it, area, neighbours, borders, sea areas, its countries), every border (its two sectors, or its sector and the sea) and every sea area (its coastal sectors) |
| `layers/` | Each map's layers as palette PNGs, for editing (below) |
| `preview/` | The maps lit as the game lights them, with and without the borders; the sectors numbered and named; the sea areas |
| `tools/` | The scripts that made all of it |

The sprites are written uncompressed (the game's own are FFUH compressed inside Dat.wow; the engine reads loose
files either way).

### The layers

Each map is built in layers, the land cut out first, and the layers are kept:

| Layer | Human | Martian |
|---|---|---|
| 0 | `0-land-mask.png`, the land (both sides) | |
| 1 | `human-1-sea.png`: the paper sea, with a graticule every 15 degrees | `martian-1-sea.png`: the dark sea seen through the telescope |
| 2 | `human-2-land.png`: lowland, upland and highland | `martian-2-land.png` |
| 3 | | `martian-3-weather.png`: weather systems, in the sea only, as the original has them (its swirls never cover the land) |
| 4 | | `martian-4-sky.png`: the red sky along the top, as the original has |
| (optional) | | `martian-optional-weather-over-land.png`: the same weather carried on over the land, not in the map; for a weather system drawn over the map separately, should one be wanted |

The layers are palette PNGs whose palette is the side's colours as the game shows them (level 11 of `HWHW` or
`MWMW`), so an editor that keeps the palette (GIMP in Indexed mode, for one) keeps the game's colour indices.
Index 0 is transparent, as in the sprites. The composite is each layer over the one below.

The maps carry no border or coast lines. The game paints every border pixel of the sector map in a solid colour of
its own (shade levels 59-66: the selected sector, the frontier, and so on), whatever the art has there, so the
borders live in the sector map alone, and the art stays clean. That also means the sectors can be redrawn (to the
empires of 1898, say) without touching the art. The `-with-borders` previews show them as a new campaign would
(level 65, the same owner on either side).

### How the art is made

- **Heights** come from NOAA's ETOPO1 relief (the coloured world image in `basemap-data`, 5400 x 2700), read back
  into three classes: lowland, upland and highland, the three the original Human map draws (green, tan and brown).
- **Texture** is taken from the original maps themselves: every pixel of a class takes the original's pixel of the
  same class at an offset, the offsets changing from region to region so the tiling doesn't show. The Human sea, land
  and the Martian land are the originals' own pixels, rearranged.
- **The Martian sea and weather** are made, not taken (the original's large swirls can't be tiled without seams): a
  swirling field (noise bent by two other noise fields) is given the original Martian sea's own colours, darkest
  to brightest in their own shares; the sea takes the darker part, the weather the brighter. As in the original,
  the weather is part of the sea's picture and stops at the coast.

## Not done

- **The sectors' frames** (`HSECTOR.SPR` / `MSECTOR.SPR`, a frame per sector to light it on the mini-map).
- **The sector links** for routes (the original's 0x4BCEB0 table), beyond neighbours and shared sea areas in
  `world.json`.
- **Battle maps** for the new sectors.
- Anything in the engine: see DESIGN.md.

## Making it again

```
pip install shapely scipy numpy pillow
python3 tools/build.py [width]
```

It needs, in its `data` folder: Natural Earth's `ne_50m_admin_0_countries`, `ne_50m_land`, `ne_50m_lakes` and
`ne_10m_admin_1_states_provinces` GeoJSON (github.com/nvkelso/natural-earth-vector), `etopo1.jpg` from the
`basemap-data` package on PyPI, and the original game's `HWM.SPR`, `MWM.SPR`, `HWMHI.SPR`, `HWHW.SHH` and `MWMW.SHH`
extracted from Dat.wow (`wowdump DAT/Dat.wow <folder>`). The paths are at the top of `build.py`. The sectors
are listed in `tools/sectors.py`. It takes about six minutes.

Sources: Natural Earth (public domain); ETOPO1 (NOAA, public domain); the original game's maps and shade
tables (Rage Software, 1998), from which the textures are taken.

## The sectors

| # | Sector | Area (km²) | Land neighbours | Coastal | Borders |
|---|---|---|---|---|---|
| 1 | The British Isles | 308,000 | 0 | yes | 1 |
| 2 | Iberia | 589,000 | 1 | yes | 2 |
| 3 | France | 553,000 | 5 | yes | 6 |
| 4 | The Low Countries | 69,000 | 2 | yes | 3 |
| 5 | Germany | 356,000 | 5 | yes | 6 |
| 6 | Scandinavia | 795,000 | 3 | yes | 4 |
| 7 | The Baltic | 475,000 | 5 | yes | 6 |
| 8 | The Alps | 128,000 | 6 | no | 6 |
| 9 | Italy | 306,000 | 3 | yes | 4 |
| 10 | Central Europe | 439,000 | 6 | yes | 7 |
| 11 | The Danube | 366,000 | 4 | yes | 5 |
| 12 | The Balkans | 520,000 | 4 | yes | 5 |
| 13 | The Western Steppe | 781,000 | 4 | yes | 5 |
| 14 | Central Russia | 1,139,000 | 5 | yes | 6 |
| 15 | Northern Russia | 1,215,000 | 5 | yes | 6 |
| 16 | The Volga | 1,476,000 | 5 | yes | 6 |
| 17 | The Caucasus | 199,000 | 3 | yes | 4 |
| 18 | Anatolia | 789,000 | 4 | yes | 5 |
| 19 | The Levant | 752,000 | 4 | yes | 5 |
| 20 | Arabia | 2,802,000 | 1 | yes | 2 |
| 21 | Persia | 1,619,000 | 5 | yes | 6 |
| 22 | Turkestan | 3,918,000 | 5 | yes | 6 |
| 23 | Afghanistan | 1,530,000 | 4 | yes | 5 |
| 24 | India | 3,562,000 | 3 | yes | 4 |
| 25 | Western Siberia | 2,412,000 | 6 | yes | 7 |
| 26 | Central Siberia | 3,557,000 | 4 | no | 4 |
| 27 | Eastern Siberia | 5,985,000 | 3 | yes | 4 |
| 28 | Mongolia | 1,561,000 | 4 | no | 4 |
| 29 | Northern China | 2,896,000 | 6 | yes | 7 |
| 30 | Tibet | 4,339,000 | 8 | no | 8 |
| 31 | Southern China | 2,187,000 | 3 | yes | 4 |
| 32 | Korea and Japan | 596,000 | 2 | yes | 3 |
| 33 | Indochina | 2,074,000 | 3 | yes | 4 |
| 34 | The East Indies | 2,816,000 | 0 | yes | 1 |
| 35 | Western Australia | 3,921,000 | 1 | yes | 2 |
| 36 | Eastern Australia | 4,101,000 | 1 | yes | 2 |
| 37 | Egypt and the Sudan | 3,488,000 | 7 | yes | 8 |
| 38 | The Maghreb | 3,010,000 | 2 | yes | 3 |
| 39 | Libya | 1,801,000 | 3 | yes | 4 |
| 40 | The Sahel | 4,742,000 | 5 | yes | 6 |
| 41 | West Africa | 1,666,000 | 2 | yes | 3 |
| 42 | The Niger | 2,039,000 | 4 | yes | 5 |
| 43 | The Horn of Africa | 1,924,000 | 2 | yes | 3 |
| 44 | The Congo | 4,169,000 | 5 | yes | 6 |
| 45 | East Africa | 1,733,000 | 4 | yes | 5 |
| 46 | The Zambezi | 2,026,000 | 3 | yes | 4 |
| 47 | Southern Africa | 3,288,000 | 2 | yes | 3 |
| 48 | Alaska | 1,522,000 | 1 | yes | 2 |
| 49 | Western Canada | 3,847,000 | 4 | yes | 5 |
| 50 | Eastern Canada | 5,177,000 | 3 | yes | 4 |
| 51 | The American West | 3,064,000 | 3 | yes | 4 |
| 52 | The Great Plains | 2,465,000 | 6 | yes | 7 |
| 53 | The Northern States | 1,385,000 | 3 | yes | 4 |
| 54 | The Southern States | 893,000 | 2 | yes | 3 |
| 55 | Mexico | 1,977,000 | 3 | yes | 4 |
| 56 | Central America | 729,000 | 2 | yes | 3 |
| 57 | New Granada | 2,519,000 | 3 | yes | 4 |
| 58 | The Andes | 2,644,000 | 4 | yes | 5 |
| 59 | The Amazon | 4,835,000 | 3 | yes | 4 |
| 60 | Southern Brazil | 4,252,000 | 3 | yes | 4 |
| 61 | Patagonia | 3,530,000 | 2 | yes | 3 |
| 62 | Greenland and Iceland | 1,054,000 | 0 | yes | 1 |

Sea areas (`CHI.SPR`): 192 The North Sea, 193 The Norwegian Sea, 194 The Baltic Sea, 195 The Western Approaches, 196 The North Atlantic, 197 The Labrador Sea, 198 Hudson Bay, 199 The Barents Sea, 200 The Kara Sea, 201 The Western Mediterranean, 202 The Eastern Mediterranean, 203 The Black Sea, 204 The Caspian Sea, 205 The Red Sea, 206 The Arabian Sea, 207 The Bay of Bengal, 208 The Indian Ocean, 209 The South China Sea, 210 The East China Sea, 211 The Sea of Japan, 212 The Sea of Okhotsk, 213 The Western Pacific, 214 The North Pacific, 215 The Bering Sea, 216 The Gulf of Alaska, 217 The Caribbean, 218 The Gulf of Mexico, 219 The Central Atlantic, 220 The South Atlantic, 221 The Gulf of Guinea, 222 The Mozambique Channel, 223 The Coral Sea, 224 The Tasman Sea, 225 The Java Sea, 226 The South Pacific, 227 The Eastern Pacific, 228 The Southern Ocean, 229 Cape Horn, 230 The Great Australian Bight, 231 The Greenland Sea, 232 The Beaufort Sea, 233 The Persian Gulf.
