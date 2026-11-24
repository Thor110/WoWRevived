WoW Engine - replacing the game's sprites with PNG pictures
============================================================

Any of the game's sprites (the .spr files: menus, buttons, backgrounds, panels, icons, the war map's
counters, cursors and so on) can be replaced, one frame at a time, with your own PNG pictures.

The battle map's unit and building textures are done another way, with a texture pack: see
README-TEXTURE-PACKS.txt.


1. TURN ON ENHANCED ASSETS FIRST
--------------------------------

The pictures are only used when "Enhanced assets" is ticked:

    Game Options > Modern Options > Enhanced assets

(or Enhanced assets=1 under [Modern] in WoWRevived.ini). With it off, the game's own sprites are drawn
and your pictures are ignored. It takes effect at once; no restart is needed.


2. NAMING THE PICTURES
----------------------

The names are the ones WoWViewer gives the frames when it exports a sprite, so you can export a sprite,
paint over the frames and put them straight back.

  A sprite with more than one frame:   <sprite name>_frame_<number>.png
      The number has two digits and counts from 00. A sprite with 3 frames has
          bigmnbut_frame_00.png   bigmnbut_frame_01.png   bigmnbut_frame_02.png
      (frame 100 and on use three digits: name_frame_100.png)

  A sprite with one frame:             <sprite name>.png
      for example  humanbd.png  for humanbd.spr

- The sprite name is the file name without .spr. Upper or lower case doesn't matter.
- WoWViewer's frame list shows how many frames a sprite has, and their numbers.
- You only need the frames you change. Any frame without a picture is drawn as the game's own, so you can
  replace just one state of a button and leave the rest.
- A one-frame sprite only takes the plain name; name_frame_00.png is not looked for.


3. WHERE THEY GO
----------------

In the game's DAT folder (in the main folder, where WoWEngine is installed), in the same folder as the
sprite they replace:

    DAT\bigmnbut.spr            ->  DAT\bigmnbut_frame_00.png
    DAT\800\humanbd.spr         ->  DAT\800\humanbd.png
                                    (a sprite made for a screen size has its own folder: 800, 1024 ...)
    DAT\extra\HD\<name>.spr     ->  DAT\extra\HD\<name>_frame_00.png
                                    (the enhanced sprites, used while Enhanced assets is on)

The picture replaces the frame of the file the game is actually using. With Enhanced assets on, a sprite
that has an HD version in DAT\extra\HD is read from there, so its PNG goes beside the HD one and is the
HD frame's size.


4. SIZE
-------

The picture must be exactly the frame's size, or exactly 2, 4 or 8 times it (in both directions). For a
352 x 32 frame:

    352 x 32,   704 x 64,   1408 x 128,   2816 x 256

A larger picture is shrunk to the frame's size as it's read (each 2 x 2, 4 x 4 or 8 x 8 block of pixels
averaged into one), so you can paint at a comfortable size. The game still draws at its own resolution.
A picture of any other size is not used (the log says so: see 7).


5. SEE-THROUGH PIXELS
---------------------

- Fully transparent pixels (alpha 0) are see-through. Partly transparent pixels are blended with what is
  behind them.
- Pure magenta (red 255, green 0, blue 255) is see-through too, as in the PNGs WoWViewer writes.
- Interlaced PNGs can't be read: save without interlacing.


6. COLOURS
----------

Your picture is kept in full colour (any 24 or 32-bit PNG, or 8-bit with a palette) and drawn that way
wherever the game draws the sprite as it is.

Where the game darkens, lights, fades or recolours a sprite through its colour tables (a screen fading
out, the battle map's shading), your picture is matched pixel by pixel to the nearest colours the sprite
has, and that is drawn instead. A pixel that matches the game's own colour there keeps the game's colour,
so a picture exported from WoWViewer and put back unchanged looks exactly as the game's.

Colours far from any the sprite has will shift a little in those places. If a sprite's colours look wrong
while fading or shaded, please report it (with the sprite's name and a screenshot).


7. CHECKING IT WORKED
---------------------

WoWEngine.log (beside the program) names each picture the first time it's used:

    Override: dat\bigmnbut_frame_00.png

or says why it wasn't used:

    Override dat\bigmnbut_frame_00.png is 700x64, not 352x32 (or 2, 4 or 8 times it); the game's is used

If a picture doesn't show and the log says nothing about it, check that Enhanced assets is on and that
the name and folder are right.


8. QUICK RECIPE WITH WOWVIEWER
------------------------------

1. Open the sprite in WoWViewer and export its frames as PNG.
2. Edit the frames you want (keep the size, or work at 2x, 4x or 8x).
3. Put the edited PNGs in the same folder as the .spr (see 3).
4. Tick Enhanced assets and look at the screen that uses the sprite.
