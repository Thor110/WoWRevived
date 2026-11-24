#!/usr/bin/env python3
"""The whole world's height bands at an eighth of the scale, in flat colours: a quick look at the band rules."""
import json, sys, os
import numpy as np, netCDF4
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import worldtile as w
S = 8
W, H = w.WORLD_W // S, w.WORLD_H // S
meta = [l.split() for l in open(f"{w.D}/data/pkg/gshhsmeta_f.dat")]
img = Image.new("L", (W, H)); d = ImageDraw.Draw(img)
with open(f"{w.D}/data/pkg/gshhs_f.dat", "rb") as dat:
    for m in meta:
        level = int(m[0])
        if level not in (1, 2, 3, 4, 5, 6) or float(m[1]) < 20: continue
        dat.seek(int(m[5])); pts = np.frombuffer(dat.read(int(m[6])), "<f4").reshape(-1, 2).astype(float)
        lon = np.degrees(np.unwrap(np.radians(pts[:, 0])))
        x = (lon - w.LON0 + 180) / 360 * w.WORLD_W / S; _, y = w.world_xy(0, pts[:, 1]); y = y / S
        for sh in (-W, 0, W):
            if len(x) > 2: d.polygon(list(zip(x + sh, y)), fill=0 if level in (2, 4) else 255)
land = np.array(img) > 0
X, Y = np.meshgrid((np.arange(W) + 0.5) * S, (np.arange(H) + 0.5) * S)
lon, lat = w.world_lonlat(X, Y)
hgt = np.zeros((H, W), np.float32)
w.heights(0, 0, 1, 1)
for f, la0, la1, lo0, lo1 in w.ETOPO:
    sel = (lat >= la0) & (lat <= la1) & (lon >= lo0) & (lon <= lo1)
    if not sel.any(): continue
    ds = netCDF4.Dataset(f); la = ds.variables["lat"][:]; lo = ds.variables["lon"][:]; z = np.asarray(ds.variables["z"][:])
    iy = np.clip(np.round((lat[sel] - la[0]) / (la[1] - la[0])).astype(int), 0, len(la) - 1)
    ix = np.clip(np.round((lon[sel] - lo[0]) / (lo[1] - lo[0])).astype(int), 0, len(lo) - 1)
    hgt[sel] = z[iy, ix]; ds.close()
# the regional grid: mean and spread of the land's height over about 300 km (750 pixels of the full map), at 1/32
from scipy import ndimage as ndi
if "--regional" in sys.argv:
    sg = 750 / S
    lf = ndi.gaussian_filter(land.astype(np.float32), sg, mode=("nearest", "wrap"))
    hl = np.where(land, np.maximum(hgt, 0), 0).astype(np.float32)
    m1 = ndi.gaussian_filter(hl, sg, mode=("nearest", "wrap")) / np.maximum(lf, 1e-3)
    m2 = ndi.gaussian_filter(hl * hl, sg, mode=("nearest", "wrap")) / np.maximum(lf, 1e-3)
    sd = np.sqrt(np.maximum(m2 - m1 * m1, 0))
    step = 32
    np.savez(f"{w.D}/tools/regional.npz", mean=m1[::step // S, ::step // S].astype(np.float32),
             std=sd[::step // S, ::step // S].astype(np.float32), step=step)
    print("regional grid", m1[::step // S, ::step // S].shape); sys.exit()
consts = json.load(open(w.CONSTS))
cls = w.height_classes(land, hgt, consts, lat, scale=S, origin=(0, 0))
pal = np.array([(152, 164, 120), (222, 198, 139), (149, 118, 84)], np.uint8)
out = np.full((H, W, 3), (139, 155, 145), np.uint8); out[land] = pal[cls[land]]
Image.fromarray(out).resize((W // 4, H // 4), Image.LANCZOS).save(f"{w.D}/v-bands.png")
print("shares of the world's land:", [round(float((cls[land] == i).mean()), 2) for i in range(3)])
