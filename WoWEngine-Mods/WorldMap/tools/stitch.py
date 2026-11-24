#!/usr/bin/env python3
"""The whole world from its tiles: overviews at an eighth and a sixteenth of the original's scale for each side, and
full size crops of a few places. python3 stitch.py"""
import os, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import worldtile as w

Image.MAX_IMAGE_PIXELS = None
OUT = w.OUT
cols, rows = -(-w.WORLD_W // w.TILE), -(-w.WORLD_H // w.TILE)
for side in ("human", "martian"):
    for div in (8, 16):
        world = Image.new("RGB", (w.WORLD_W // div, w.WORLD_H // div))
        for r in range(rows):
            for c in range(cols):
                f = f"{OUT}/tiles/{side}-{c}-{r}.png"
                if not os.path.exists(f):
                    print("missing", f); continue
                im = Image.open(f)
                world.paste(im.resize((max(1, im.width // div), max(1, im.height // div)), Image.LANCZOS),
                            (c * w.TILE // div, r * w.TILE // div))
        world.save(f"{OUT}/world-{side}-1-{div}.jpg", quality=90)
        if div == 16:
            world.save(f"{OUT}/world-{side}-1-{div}.png")
        print(side, div, world.size)

# full size crops, 1600 x 900, of places round the world (lon, lat)
PLACES = [("the-alps", 9.5, 46.3), ("the-nile-delta", 31.2, 30.6), ("the-himalaya", 86.9, 28.0),
          ("japan", 139.7, 35.7), ("new-york", -74.0, 40.7), ("cape-horn", -68.0, -55.3),
          ("cape-of-good-hope", 18.5, -34.2), ("antarctic-peninsula", -60.0, -64.5)]
os.makedirs(f"{OUT}/places", exist_ok=True)
for name, lon, lat in PLACES:
    x, y = w.world_xy(lon, lat); x, y = int(x), int(float(y))
    c, r = x // w.TILE, y // w.TILE
    for side in ("human", "martian"):
        f = f"{OUT}/tiles/{side}-{c}-{r}.png"
        if not os.path.exists(f):
            continue
        im = Image.open(f)
        lx, ly = x - c * w.TILE, y - r * w.TILE
        box = (max(0, lx - 800), max(0, ly - 450), min(im.width, lx + 800), min(im.height, ly + 450))
        im.crop(box).save(f"{OUT}/places/{name}-{side}.png")
print("places done")
