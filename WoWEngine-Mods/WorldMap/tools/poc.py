#!/usr/bin/env python3
"""The world at the original war map's scale: a proof of concept, one 8192 x 8192 tile centred on the British Isles.

The world frame: Miller cylindrical, the whole world (both poles), scaled separately across and down so that at
54 N a pixel is 0.40 km across and 0.442 km down, as the original map's is (tools/fit_britain.py). The world is then
about 58,800 x 48,400 pixels; this builds one tile of it, from full resolution coastlines (GSHHS 'f', from the
basemap-data-hires package) and ETOPO 2022 15 arc-second heights (NOAA)."""
import glob, math, os, sys
import numpy as np
import netCDF4
from PIL import Image, ImageDraw
from scipy import ndimage as ndi
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import spr

D = "/home/user/worldmap"
O = f"{D}/orig/all/"
OUT = f"{D}/mod/WorldMap/scale-tile"
R = 6371.0
KX, KY, LAT_REF = 0.40, 0.442, 54.0     # the original's km a pixel at 54 N
EX = KX / math.cos(math.radians(LAT_REF))          # km a pixel across at the equator
EY = KY / math.cos(math.radians(0.8 * LAT_REF))    # Miller's y: dy/dphi = sec(0.8 phi)
LON0 = 11.0
WORLD_W = int(round(2 * math.pi * R / EX))
def my(lat):
    return 1.25 * np.log(np.tan(np.pi / 4 + 0.4 * np.radians(lat)))
YTOP = 1.25 * math.log(math.tan(math.pi / 4 + 0.4 * math.radians(89.999)))
WORLD_H = int(round(2 * YTOP * R / EY))
T = 8192
rng = np.random.RandomState(1898)

def world_xy(lon, lat):
    x = ((np.asarray(lon) - LON0 + 180) % 360) / 360 * WORLD_W
    y = (YTOP - my(np.clip(lat, -89.999, 89.999))) * R / EY
    return x, y
def world_lonlat(x, y):
    lon = (np.asarray(x) / WORLD_W * 360 - 180 + LON0 + 180) % 360 - 180
    yu = YTOP - np.asarray(y) * EY / R
    lat = np.degrees(2.5 * np.arctan(np.exp(0.8 * yu)) - 0.625 * np.pi)
    return lon, lat

cx, cy = world_xy(-3.0, 54.6)
X0, Y0 = int(cx - T / 2), int(cy - T / 2)
lon_a, lat_a = world_lonlat(X0, Y0); lon_b, lat_b = world_lonlat(X0 + T, Y0 + T)
print(f"world {WORLD_W} x {WORLD_H}; tile at {X0}, {Y0}: lon {lon_a:.1f} to {lon_b:.1f}, lat {lat_b:.1f} to {lat_a:.1f}")

# ------------------------------------------------------------------ land, from GSHHS full resolution
meta = [l.split() for l in open(f"{D}/data/pkg/gshhsmeta_f.dat")]
dat = open(f"{D}/data/pkg/gshhs_f.dat", "rb")
img = Image.new("L", (T, T))
d = ImageDraw.Draw(img)
counts = {}
for m in meta:
    level = int(m[0]); south, north = float(m[3]), float(m[4]); off, nb = int(m[5]), int(m[6])
    if north < lat_b - 1 or south > lat_a + 1 or level not in (1, 2, 3, 4):
        continue
    dat.seek(off)
    pts = np.frombuffer(dat.read(nb), "<f4").reshape(-1, 2).astype(np.float64)
    lo = pts[:, 0].copy(); lo[lo > 180] -= 360
    if lo.max() < lon_a - 1 or lo.min() > lon_b + 1:
        continue
    x, y = world_xy(lo, pts[:, 1])
    x -= X0; y -= Y0
    if len(x) > 2:
        d.polygon(list(zip(x, y)), fill=255 if level in (1, 3) else 0)
        counts[level] = counts.get(level, 0) + 1
land = np.array(img) > 0
print("polygons by level:", counts, " land", round(land.mean(), 3))

# ------------------------------------------------------------------ heights, from ETOPO 2022 15"
height = np.full((T, T), np.nan, np.float32)
YY, XX = np.mgrid[0:T, 0:T]
LON, LAT = world_lonlat(XX + X0 + 0.5, YY + Y0 + 0.5)
del XX, YY
for f in sorted(glob.glob(f"{D}/data/etopo15/*.nc")):
    ds = netCDF4.Dataset(f)
    la = ds.variables["lat"][:]; lo = ds.variables["lon"][:]
    sel = (LAT >= la.min() - 1e-6) & (LAT <= la.max() + 1e-6) & (LON >= lo.min() - 1e-6) & (LON <= lo.max() + 1e-6)
    if not sel.any():
        continue
    z = ds.variables["z"][:].filled(0) if hasattr(ds.variables["z"][:], "filled") else ds.variables["z"][:]
    iy = np.clip(np.round((LAT[sel] - la[0]) / (la[1] - la[0])).astype(int), 0, len(la) - 1)
    ix = np.clip(np.round((LON[sel] - lo[0]) / (lo[1] - lo[0])).astype(int), 0, len(lo) - 1)
    height[sel] = z[iy, ix]
    ds.close()
missing = np.isnan(height) & land
print("height missing on land:", round(missing.mean() / max(land.mean(), 1e-9), 4))
height = np.nan_to_num(height, nan=0.0)
del LON

# the original's split of Britain's land: green 44%, tan 28%, brown 29% (orig/classes.npy); the thresholds that give
# Great Britain the same split here
gb = land & (LAT > 50.0) & (LAT < 58.7)
orig_cls = np.load(f"{D}/orig/classes.npy")
shares = np.array([orig_cls[i].sum() for i in (1, 2, 3)], float); shares /= shares.sum()
hs = ndi.gaussian_filter(height, 1.5)
# by relief, not height alone: how far the land rises above its surroundings (a 50 km neighbourhood), with the
# height mixed in, so that hills read as hills on a high plateau as well as on a low plain; low coastal plains stay
# lowland everywhere. The two cuts are set so that Great Britain keeps the original's shares.
low = hs.copy(); low[~land] = 0
surround = ndi.gaussian_filter(low, 120) / np.maximum(ndi.gaussian_filter(land.astype(np.float32), 120), 1e-3)
score = (hs - surround) + 0.35 * hs
t1, t2 = np.quantile(score[gb], [shares[0], shares[0] + shares[1]])
plain = np.quantile(hs[gb], shares[0] * 0.6)
print(f"relief score cuts {t1:.0f}, {t2:.0f}; plains below {plain:.0f} m stay lowland (Britain's shares {np.round(shares, 2)})")
hcls = np.where((score < t1) | (hs < plain), 0, np.where(score < t2, 1, 2)).astype(np.uint8)
del low, surround, score
del LAT

# ------------------------------------------------------------------ texture (as tools/build.py)
hw = spr.read(O + "HWM.SPR"); mw = spr.read(O + "MWM.SPR")
H0, W0 = hw.shape
_c = spr.shade(O + "HWHW.SHH", 11).astype(int)[hw]
landlike = ((_c[..., 1] - _c[..., 2]) > 28) | ((_c[..., 0] - _c[..., 2]) > 30)
landlike = ndi.binary_dilation(ndi.binary_opening(landlike, iterations=2), iterations=6)
orig_cls[0] &= ~landlike
mars_sea_ok = orig_cls[0].copy(); mars_sea_ok[:420] = False
offsets = np.array([(int(rng.randint(H0)), int(rng.randint(W0))) for _ in range(16)])

def region_of(X, Y):
    return ((X // 700) * 7 + (Y // 600) * 3) % len(offsets)

def texture(src, cls_masks, target_cls):
    out = np.zeros((T, T), np.uint8)
    for y0 in range(0, T, 512):
        ys = np.arange(y0, min(T, y0 + 512))
        X, Y = np.meshgrid(np.arange(T), ys)
        X = X + X0; Y = Y + Y0            # world pixels, so neighbouring tiles would join up
        tc = target_cls[y0:y0 + len(ys)]
        reg = region_of(X, Y)
        res = np.zeros(X.shape, np.uint8); done = np.zeros(X.shape, bool)
        for kk in range(len(offsets)):
            o = offsets[(kk + reg) % len(offsets)]
            sy = (Y + o[..., 0]) % H0; sx = (X + o[..., 1]) % W0
            for c, m in enumerate(cls_masks):
                want = (tc == c) & ~done
                if not want.any():
                    continue
                hit = want & m[sy, sx]
                res[hit] = src[sy[hit], sx[hit]]; done |= hit
            if done.all():
                break
        for c, m in enumerate(cls_masks):
            left_ = (tc == c) & ~done
            if left_.any():
                vals, cnts = np.unique(src[m], return_counts=True); res[left_] = vals[cnts.argmax()]
        out[y0:y0 + len(ys)] = res
    return out

target = np.where(land, hcls + 1, 0).astype(np.uint8)
human = texture(hw, [orig_cls[0], orig_cls[1], orig_cls[2], orig_cls[3]], target)
martian_land = texture(mw, [mars_sea_ok, orig_cls[1], orig_cls[2], orig_cls[3]], target)

def fractal(h, w, octaves, base, stretch=(1, 1)):
    out = np.zeros((h, w), np.float32); amp = 1.0
    for o in range(octaves):
        cell = max(2, base >> o)
        g = rng.rand(h // (cell * stretch[0]) + 3, w // (cell * stretch[1]) + 3).astype(np.float32)
        out += amp * ndi.zoom(g, (cell * stretch[0], cell * stretch[1]), order=3)[:h, :w]; amp *= 0.5
    return (out - out.mean()) / (out.std() + 1e-6)
def warped(base, octaves, push, stretch=(1, 2)):
    u = fractal(T, T, 4, base * 2, stretch) * push; v = fractal(T, T, 4, base * 2, stretch) * push * 0.66
    f = fractal(T, T, octaves, base, stretch)
    out = np.zeros((T, T), np.float32)
    for y0 in range(0, T, 256):
        ys = np.arange(y0, min(T, y0 + 256))
        Xf, Yf = np.meshgrid(np.arange(T, dtype=np.float32), ys.astype(np.float32))
        out[y0:y0 + len(ys)] = ndi.map_coordinates(f, [np.clip(Yf + v[y0:y0 + len(ys)], 0, T - 1),
                                                       np.clip(Xf + u[y0:y0 + len(ys)], 0, T - 1)], order=1)
    return out
MSH = spr.shade(O + "MWMW.SHH", 11).astype(int)
m_vals, m_cnts = np.unique(mw[mars_sea_ok & ~landlike], return_counts=True)
o_ = np.argsort(MSH[m_vals].sum(1)); m_vals, m_cnts = m_vals[o_], m_cnts[o_]
cool = MSH[m_vals][:, 2] + 12 >= MSH[m_vals][:, 0]
def by_rank(field, vals, cnts, lo, hi):
    ref = np.sort(field[::9, ::9].ravel()); edges = np.cumsum(cnts) / cnts.sum()
    out = np.zeros(field.shape, np.uint8)
    for y0 in range(0, T, 512):
        rank = np.searchsorted(ref, field[y0:y0 + 512]) / ref.size
        rank = np.clip(rank + rng.normal(0, 0.025, rank.shape), 0, 1)   # dithered, so the colours don't step
        rank = lo + (hi - lo) * rank
        out[y0:y0 + 512] = vals[np.minimum(np.searchsorted(edges, rank), len(vals) - 1)]
    return out
# at the original's scale the weather systems are larger in pixels than on the small world map
martian_sea = by_rank(warped(640, 7, 220), m_vals[cool], m_cnts[cool], 0.0, 0.62)
wf = warped(800, 7, 340, (1, 3))
t0, t1w = np.quantile(wf[::9, ::9], [0.70, 0.96])
wmask = (rng.rand(T, T) < np.clip((wf - t0) / (t1w - t0), 0, 1) ** 1.2) & ~land
weather = by_rank(wf, m_vals[cool], m_cnts[cool], 0.55, 1.0)
martian = np.where(land, martian_land, martian_sea); martian[wmask] = weather[wmask]

# a graticule every 5 degrees on the Human sea
sea_vals = np.unique(hw[orig_cls[0]])
HSH = spr.shade(O + "HWHW.SHH", 11).astype(int)
by_light = sea_vals[np.argsort(HSH[sea_vals].sum(1))]
dark = by_light[len(by_light) // 6: len(by_light) // 3]   # a little darker than the sea, as faint as the original's lines
g = np.zeros((T, T), bool)
for lon in range(-180, 180, 5):
    x = int(round(world_xy(lon, 0)[0] - X0))
    if 0 <= x < T: g[:, x] = True
for lat in range(-85, 90, 5):
    y = int(round(float(world_xy(0, lat)[1]) - Y0))
    if 0 <= y < T: g[y, :] = True
g &= ~land
human[g] = rng.choice(dark, g.sum())

# ------------------------------------------------------------------ write
os.makedirs(OUT, exist_ok=True)
spr.write(f"{OUT}/HWM-tile.SPR", human); spr.write(f"{OUT}/MWM-tile.SPR", martian)
HS = spr.shade(O + "HWHW.SHH", 11); MS = spr.shade(O + "MWMW.SHH", 11)
Image.fromarray(HS[human]).save(f"{OUT}/human-tile.png")
Image.fromarray(MS[martian]).save(f"{OUT}/martian-tile.png")
Image.fromarray(HS[human]).resize((2048, 2048), Image.LANCZOS).save(f"{OUT}/human-tile-quarter.png")
Image.fromarray(MS[martian]).resize((2048, 2048), Image.LANCZOS).save(f"{OUT}/martian-tile-quarter.png")
np.save(f"{OUT}/frame.npy", np.array([WORLD_W, WORLD_H, X0, Y0, KX, KY, LAT_REF, LON0]))

# the game's Britain beside this one, the same size: the original's 1498 x 2544, and this tile's crop round the
# same coasts (the crop placed by the fitted origin: the original's land centre on Britain's here)
orig_h = HS[hw]
hi = spr.read(O + "HWMHI.SPR")
oy, ox = np.argwhere((hi >= 1) & (hi <= 30)).mean(0)
gb_land = land & (np.arange(T)[:, None] < T)  # the tile's land near Britain's centre
lab, n = ndi.label(land)
big = lab[int(cy - Y0), int(cx - X0)]
ty, tx = np.argwhere(lab == big).mean(0)
y1 = int(ty - oy); x1 = int(tx - ox)
ours = HS[human][max(0, y1):y1 + H0, max(0, x1):x1 + W0]
pair = Image.new("RGB", (W0 * 2 + 40, H0), (20, 20, 20))
pair.paste(Image.fromarray(orig_h), (0, 0)); pair.paste(Image.fromarray(ours), (W0 + 40, 0))
pair.save(f"{OUT}/britain-original-and-ours.png")
pair.resize((pair.width // 3, pair.height // 3), Image.LANCZOS).save(f"{OUT}/britain-original-and-ours-third.png")
print("done")
