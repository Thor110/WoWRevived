# Replacing sprites with PNG pictures

MODERN. Any of the game's sprites (`.spr`) can be replaced, frame by frame, with PNG pictures. This is separate from
the battle map's unit and building textures (the `.wof` and `.iob` models), which have their own folders: see
TEXTURES.md.

## Turning it on

The pictures are read only when **Enhanced assets** is on (Game Options, or `[Modern] Enhanced assets=1` in
`WoWRevived.ini`). With it off, or with `--no-loose`, the game's own sprites are drawn.

## Names

The names are the ones WoWViewer gives the frames when it exports a sprite (SprViewer.cs), so a sprite can be
exported, painted over and put back as it is:

| The sprite has | The picture's name | Example |
|---|---|---|
| more than one frame | `<sprite name>_frame_<NN>.png`, NN the frame's number with two digits, from 00 | `bigmnbut_frame_00.png`, `bigmnbut_frame_01.png`, `bigmnbut_frame_02.png` |
| one frame | `<sprite name>.png` | `humanbd.png` |

- The sprite's name is its file name without `.spr`, in any case (`BIGMNBUT_FRAME_00.png` works too).
- Frames are counted from 00, as WoWViewer's frame list shows them. A sprite with 3 frames has frames 00, 01 and 02.
- Any frame can be left out: the frames that have no picture are the game's own. One frame of a button can be
  replaced and the rest left alone.
- A one-frame sprite takes the plain name only; `<name>_frame_00.png` isn't looked for.
- A frame number with three digits (frame 100 and on) is written with all three: `<name>_frame_100.png`.

## Where they go

Beside the sprite the engine reads, under the same folder names, in one of the engine's loose `dat` folders:

1. the `dat` folder beside the engine's program (a release), or the source folder's `dat` (a build run where it
   was made);
2. the game's own `DAT` folder.

The engine's folder is looked in first. The folder inside `dat` is the sprite's own:

- `dat\bigmnbut.spr` → `dat\bigmnbut_frame_00.png`
- a sprite read from a view size's folder (for example `dat\800\humanbd.spr` at 800 x 600) → the PNG goes in that
  folder too: `dat\800\humanbd.png`
- a sprite read from the enhanced sprites' folder `dat\extra\HD` → `dat\extra\HD\<name>...png`

The picture replaces the frame of the file the engine actually reads, so with an HD sprite in `dat\extra\HD` the
PNG goes beside the HD one and its size is the HD frame's.

## Size

The picture is the frame's own size, or exactly 2, 4 or 8 times it in both directions. A larger one is made the
frame's size by averaging each 2 x 2, 4 x 4 or 8 x 8 block of pixels. WoWViewer shows each frame's size; a 352 x 32
frame can be given as 352 x 32, 704 x 64, 1408 x 128 or 2816 x 256. A picture of any other size isn't used, and
the log says so (`Override dat\...png is W x H, not ...`).

## See-through pixels

- A pixel with alpha 0 is see-through; one with alpha between 0 and 255 is blended over what is under it.
- Magenta (255, 0, 255) is see-through too, as in the PNGs WoWViewer writes.

## Colours

Each picture is kept in full colour, and is drawn that way wherever the game draws the sprite as it is (menus,
buttons, backgrounds, the war and battle maps' interface). Where the game draws the sprite through one of its
colour tables (darkened, lit, faded or recoloured: the battle map's shading, a fading screen), the picture is
matched pixel by pixel to the nearest colour of the sprite's own colour table, and that is drawn. A pixel whose
nearest colour is the same as the game's own pixel there keeps the game's colour index, so a picture exported from
WoWViewer and put back unchanged draws exactly as the game's. *Provisional*: a sprite without a colour table of its
own (some of the battle map's) is drawn in full colour where it is drawn as it is, and as the game's frame elsewhere.

## Checking

The log names each picture as it is first used (`Override: dat\bigmnbut_frame_00.png`), or why it wasn't
(a picture it can't read, or the wrong size).
