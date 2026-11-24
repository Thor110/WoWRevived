# Texture packs: where the files go

*The layout agreed with the owner (update 238); the engine reads it since update 240 (`core/Png`,
`core/TexturePack`, `battlemap/Textures.cpp`). It is the same as the texture export's (`wowdump --textures`), so an
exported and reworked set can be dropped in as it is.*

**It's read with Enhanced assets on** (Modern Options; `--no-loose` keeps it off), as the battle map loads its
models. WoWEngine.log says where the pack was found ("Texture pack: 482 pictures in ...") and names each picture as
it's used ("Texture pack: human/units/ar_lor_1.png"), or why one isn't ("... is 256x40, not 256x48; the game's is
used"). A change to the pack is seen at the next battle the program starts after it's run again (the folder is
looked through once).

## The folders

Beside the program: `TEXTURES` (or `textures`) beside WoWEngine.exe; a build run where it was made also looks in
the source folder's `textures` (WoWEngine/textures), after the program's own:

```
textures/
  human/
    units/          the Human vehicles' textures        (one PNG per WOF model)
    buildings/      the Human buildings' pictures        (one PNG per IOB model) and their turrets (WOF)
  martian/
    units/          the Martian machines' textures       (WOF)
    buildings/      the Martian buildings' pictures      (IOB) and their turrets (WOF)
  scenery/          towns, houses, trees, the red weed, bridges (both sides see these)
  civilwar/
    human/
      units/        the other side's in a Human Civil War
      buildings/
    martian/
      units/        the other side's in a Martian Civil War
      buildings/
```

- **A file's name is its model's** as the export wrote it: `AIRSHIP.png` for `AIRSHIP.WOF`, `Bc_main.png` for
  `Bc_main.iob`. Case doesn't matter. Only the models a pack changes need a file. Anything missing is drawn as
  the game's own.
- **Which side a model is on** is the export's choice: the unit table says (each unit type's model and its parts'),
  and a model no unit type names goes by its colour table. The export's folders are the answer; a file in the
  wrong side's folder isn't found.
- The `.patches.txt` files the export wrote beside the buildings are for cutting a picture by hand. The engine
  doesn't need them in the pack; it reads the pieces from the game's own IOB.

## Both sides of a civil war

- `human/...` and `martian/...` are each **that faction's**, so in a civil war they change **both** sides (both
  sides are the one faction). That's also what the regular game uses.
- `civilwar/<faction>/...` is **only the other side's**, in that faction's civil war. Where there is a file there,
  the other side's model uses it; where there isn't, the other side uses the faction's (`human/...` or the game's
  own) with the hue turned as now (MODERN.md "The civil wars", Colours). A file in `civilwar/` turns the hue turn
  off for that model, so its colours are drawn as given.
- The player's side in a civil war is always drawn from `human/...` / `martian/...` (or the game's own).

## The pictures

- **PNG**, 8-bit (with a palette) or 24 / 32-bit full colour.
- **The size stays the game's.**
  - A unit's texture is an atlas 256 pixels wide whose height is the model's. Its faces map into it, so it is
    repainted in place: what's where stays where it is. A PNG of another size is left out (the log says so) and
    the game's is drawn.
  - A building's picture is the export's size, the building put back together. Its outline is the game's:
    the engine cuts the picture back into the game's own pieces, so only pixels inside the original's outline
    are read. Paint over the outline and it's cut off; leave a hole inside it and the game's own pixels show
    through there. A building of another shape needs a new model, which this doesn't do.
- **Transparency**: a pixel is see-through (the game's index 0) when its alpha is under 128 (a 32-bit PNG), or
  it is pure magenta (255, 0, 255), or (an 8-bit PNG) it is palette entry 0. The exported PNGs mark index 0 both
  ways (magenta, and transparent in the PNG). Nothing else becomes see-through, so no other colour comes out as
  index 0.
- **Colours: quantised on the way in.** The game draws a model through its side's table of 64 brightness levels
  (shading by the sun); a texture holds index numbers into it, not colours. So each pixel of a pack's PNG is
  turned into the index whose colour is nearest in the palette the export used: the battle map's midday table
  for that colour group at level 44 of 64 (the export's palette; the export's `shade-tables` folder shows
  all the levels). Paint at about the brightness the export shows; the game makes it darker and lighter as the
  light falls.
  - An **8-bit PNG with the export's palette** (any edit that keeps the palette, such as painting with its
    colours) is read by index, exactly, with no rounding to the nearest colour. An exported PNG put back
    unchanged gives the game's texture byte for byte (checked for all 484 of the export's: `wowdump --texcheck
    <game folder> <pack folder>` reads a pack as the engine does and says which pictures differ from the game's),
    and the battle's pictures are the same to the pixel as without the pack.
  - Colours far from any in the palette come out as the nearest one, as they would in any quantising. The
    colour groups are: Human buildings and scenery houses, Human vehicles, Martian vehicles, Martian buildings,
    nature (trees, red weed). A Human building can only use the Human buildings' colours, and so on.
  - Later (the owner's plan, MODERN.md "Tools"): full-colour textures lit by the engine, and the overrides in
    common formats for the rest of the game's files. Packs made now carry over: the same folders and names.
- **No building's picture moves.** The IOB header word that the engine once called "animated" (0x0A) only picks
  how a building's pieces are lit (0x4111E1: a normal for each corner of a piece, so it shades smoothly, or one
  for the piece, flat). What moves on a building is a separate model fixed to it: the turrets and guns (WOF
  parts with atlases of their own, in the same folder as the building) and the effects (smoke, lights). So a
  building's one picture is all there is to paint.
- **Not read**: interlaced PNGs (save without interlacing). A building's dither (the fine pattern of its
  shading) is kept as the game's, whatever the picture. The bridges are read since update 241.

## Not in this layout

- **The war map's counters and the Build List's pictures** (`HU-CNT24/32/48.SPR`, `MA-CNT...`) and the other
  sprites are `.spr` files. They're replaced as the engine's own loose files are (MODERN.md "The engine's own
  files" and "Enhanced assets"): a file of the same name in the engine's `dat` folder. That changes both sides
  of a civil war; separate files for the other side would come later, as above (the owner, update 238).
- **The models' shapes** (WOF / IOB geometry): not covered.
- **Voices** naming the other faction ("Martians sighted"): to be reworked for the civil wars later.
