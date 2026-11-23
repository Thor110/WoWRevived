#!/usr/bin/env python3
"""Fit Great Britain from Natural Earth onto the original war map's sector map (HWMHI.SPR), to find the map's
projection and scale. Tries an equirectangular projection (x = lon * cos(lat0)) and Transverse Mercator about 2 W
(the Ordnance Survey's central meridian), each with a uniform scale and an offset."""
import json, math, sys
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, "/home/user/worldmap/tools")
import spr

D = "/home/user/worldmap"
hi = spr.read(f"{D}/orig/all/HWMHI.SPR")
H, W = hi.shape
land = ((hi >= 1) & (hi <= 30)) | ((hi >= 32) & (hi <= 117))
K = 4  # work at a quarter size
small = np.array(Image.fromarray(land.astype(np.uint8) * 255).resize((W // K, H // K), Image.BILINEAR)) > 127

# Great Britain and its islands (not Ireland, not Man, which the sector map leaves out or keeps as islands)
gj = json.load(open(f"{D}/data/ne_10m_land.geojson"))
polys = []
for f in gj["features"]:
    g = f["geometry"]
    parts = g["coordinates"] if g["type"] == "MultiPolygon" else [g["coordinates"]]
    for p in parts:
        ring = np.array(p[0])
        lo, la = ring[:, 0].mean(), ring[:, 1].mean()
        if -8.0 < lo < 2.0 and 49.8 < la < 61.0 and not (lo < -5.4 and la < 55.5 and lo > -10.6):
            polys.append(ring)
print(len(polys), "polygons")

R = 6371.0
def tm(lon, lat, lon0=-2.0):
    """spherical transverse Mercator, km"""
    lam = np.radians(lon - lon0); phi = np.radians(lat)
    B = np.cos(phi) * np.sin(lam)
    x = 0.5 * R * np.log((1 + B) / (1 - B))
    y = R * np.arctan2(np.tan(phi), np.cos(lam))
    return x, y

def eq(lon, lat, lat0=54.0):
    return R * np.radians(lon) * math.cos(math.radians(lat0)), R * np.radians(lat)

def raster(proj, s, ox, oy):
    """km -> pixels: px = x * s + ox, py = oy - y * s, at the quarter size"""
    im = Image.new("L", (W // K, H // K))
    d = ImageDraw.Draw(im)
    for ring in polys:
        x, y = proj(ring[:, 0], ring[:, 1])
        pts = list(zip((x * s + ox) / K, (oy - y * s) / K))
        if len(pts) > 2:
            d.polygon(pts, fill=255)
    return np.array(im) > 127

def iou(a, b):
    return (a & b).sum() / max(1, (a | b).sum())

def fit(proj):
    # the land's extent gives a first guess: the map's land spans these rows and columns
    ys, xs = np.nonzero(land)
    allx, ally = [], []
    for ring in polys:
        x, y = proj(ring[:, 0], ring[:, 1]); allx += list(x); ally += list(y)
    allx, ally = np.array(allx), np.array(ally)
    s = (ys.max() - ys.min()) / (ally.max() - ally.min())
    ox = xs.min() - allx.min() * s
    oy = ys.min() + ally.max() * s
    best = (iou(raster(proj, s, ox, oy), small), s, ox, oy)
    step = [0.02 * s, 40.0, 40.0]
    for it in range(40):
        improved = False
        for k in range(3):
            for sign in (-1, 1):
                p = list(best[1:])
                p[k] += sign * step[k]
                v = iou(raster(proj, *p), small)
                if v > best[0]:
                    best = (v, *p); improved = True
        if not improved:
            step = [x / 2 for x in step]
    return best

for name, proj in (("equirectangular at 54 N", eq), ("transverse Mercator about 2 W", tm)):
    v, s, ox, oy = fit(proj)
    print(f"{name}: overlap {v:.3f}, {s:.4f} px a km ({1 / s:.3f} km a px), origin {ox:.0f}, {oy:.0f}")
    np.save(f"{D}/orig/fit-{'eq' if proj is eq else 'tm'}.npy", np.array([v, s, ox, oy]))
