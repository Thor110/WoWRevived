WoW Engine - texture packs for the battle map
=============================================

A texture pack gives the battle map's units, buildings and scenery new pictures, and units new shapes (8). It's a folder of PNGs,
laid out the same way the engine's texture export writes them, so an exported set can be reworked and
dropped straight back in.

The interface's sprites (menus, buttons, panels, icons, the war map's counters) are replaced another way,
frame by frame: see README-OVERRIDE.txt.


1. TURN ON ENHANCED ASSETS FIRST
--------------------------------

The pack is only read when "Enhanced assets" is ticked:

    Game Options > Modern Options > Enhanced assets

A change to the pack is seen in the next battle after the game is started again (the folder is looked
through once each run).


2. WHERE IT GOES
----------------

A folder called TEXTURES in the main folder, beside WoWEngine.exe:

    TEXTURES\
      human\
        units\          the Human vehicles               (one PNG per unit model)
        buildings\      the Human buildings              (one PNG per building) and their turrets
      martian\
        units\          the Martian machines
        buildings\      the Martian buildings and their turrets
      scenery\          towns, houses, trees, the red weed, bridges
      civilwar\
        human\units\  human\buildings\       the other side's, in a Human Civil War
        martian\units\  martian\buildings\   the other side's, in a Martian Civil War

- A picture is named after its model: AIRSHIP.png for AIRSHIP.WOF, Bc_main.png for Bc_main.iob. Upper
  or lower case doesn't matter.
- Only the models you change need a picture. Anything missing is drawn as the game's own.
- A picture must be in its side's folder (the one the export put it in); in the other side's it isn't
  found.
- The .patches.txt files the export writes beside the buildings are only a guide for painting. The
  engine doesn't need them.

Civil wars: human\ and martian\ are each that faction's, so in a civil war they change both sides. A
picture in civilwar\<faction>\ changes only the other side's look, in that faction's civil war. Without
one there, the other side is drawn with its colours turned, as usual.


3. GETTING THE GAME'S PICTURES TO START FROM
-------------------------------------------

The engine's tool wowdump writes every unit's and building's picture in this layout:

    wowdump --textures <game folder> <output folder>

It writes 8-bit BMPs; save them as PNG (keeping the palette is best) and edit from there. wowdump is built
with the engine from its source code and isn't in the release, so an exported set is usually passed
around ready-made; any set in this layout will do.


4. SIZE
-------

The picture must be the game's size, or exactly 2, 4 or 8 times it in both directions. A larger one is
shrunk to the game's size as it's read, so you can paint at a comfortable size. Any other size is left
out (the log says so: see 7).

- A unit's picture is a texture sheet 256 pixels wide that the model's faces are mapped onto. Repaint it
  in place: what's where must stay where it is.
- A building's picture is the whole building as the game shows it. Only the pixels inside the game's own
  outline are used: paint outside it and it's cut off; leave a hole inside it and the game's own pixels
  show through. A building can't change shape this way. (A building in the game isn't a 3D model but a
  picture drawn beforehand and cut into pieces, so its picture is the only thing there is to change.)


5. SEE-THROUGH PIXELS
---------------------

A pixel is see-through when it's mostly transparent (alpha under 128), or pure magenta (red 255, green 0,
blue 255), or palette entry 0 in an 8-bit PNG. The exported pictures mark these both ways. No other
colour is see-through.


6. COLOURS
----------

The game shades the battle map by the time of day, so a texture holds the game's colour numbers rather
than colours. Each pixel of your picture is turned into the nearest of its side's colours at about the
brightness the export shows: paint at that brightness and the game makes it darker or lighter as the
light changes.

- An 8-bit PNG that keeps the export's palette is read exactly. An exported picture put back unchanged
  draws exactly as the game's.
- Colours far from any the side has come out as the nearest one. Each kind of model has its own set:
  Human buildings, Human vehicles, Martian vehicles, Martian buildings, nature.
- Interlaced PNGs can't be read: save without interlacing.


7. CHECKING IT WORKED
---------------------

WoWEngine.log (beside the program) says where the pack was found and names each picture as it's used:

    Texture pack: 482 pictures in ...\TEXTURES
    Texture pack: human/units/ar_lor_1.png

or why one wasn't used:

    ... is 256x40, not 256x48; the game's is used


8. MODELS: NEW SHAPES FOR UNITS
-------------------------------

A unit's shape can be changed too, with a 3D model beside its picture in the pack:

    TEXTURES\human\units\AIRSHIP.obj      (or AIRSHIP.gltf, or AIRSHIP.glb)

OBJ, glTF and GLB are read (a .gltf must have its data inside it; Blender's "glTF Embedded" does that, or use
.glb). Buildings can't be given new shapes: they aren't 3D models in the game.

- Start from the game's own model: the engine's tool wowdump writes every unit as OBJ ("wowdump --obj"; like
  --textures it's built from the source, so an exported set is usually passed around ready-made). Each part of
  a unit (body, turret, legs, ...) is a group named as the game names it. Keep those names: the game moves and
  turns the parts by them, and a part your model doesn't name keeps the game's shape.
- Each part holds at most 255 points and 255 triangles. A part with more is left as the game's (the log says so).
- OBJ files can't be reordered: they must keep their original structure (the parts, each part's points and
  triangles in the order the export wrote them), or the parts' animation and the points the game finds by
  number (where a gun fires from, a walker's feet) go wrong. New points go after a part's own.
- Blender reorders OBJ files unless told not to: its import and export settings likely need adjusting to keep
  the order (look for the options that keep the vertex order and split by object, and leave out anything that
  cleans up, merges or validates the mesh). Don't use "Merge by Distance": the game's models have points at the
  same place on purpose. Keep the axes and scale the same on import and export.
- glTF is less fussy about order (points at the same place are joined up again), but export with normals so
  sharp edges stay sharp, and use "glTF Embedded" or .glb.
- The texture is the unit's picture (the same PNG as above, 256 wide): the model's texture coordinates place each
  triangle on it. The materials are named material_0, material_1 ... as the export names them; keep each
  triangle's.
- The model is in the game's own units and the export's place: change the shape around where it is.

The log names each model used ("Texture pack: model human/units/AIRSHIP.obj") and anything it couldn't use.
