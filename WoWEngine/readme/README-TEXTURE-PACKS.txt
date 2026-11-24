WoW Engine - texture packs for the battle map
=============================================

A texture pack gives the battle map's units, buildings and scenery new pictures. It's a folder of PNGs,
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
  show through. A building can't change shape this way.


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
