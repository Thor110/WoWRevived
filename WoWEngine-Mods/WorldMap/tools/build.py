#!/usr/bin/env python3
"""Builds the world war map draft: the sector map, both sides' maps, the mini-maps, the sea areas, the data file
and previews. python3 build.py [width]"""
import json, math, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage as ndi
from shapely.geometry import shape, box
from shapely.ops import unary_union

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import spr
from sectors import SECTORS, SEAS

D = "/home/user/worldmap"
OUT = f"{D}/mod/WorldMap"
O = f"{D}/orig/all/"
WD = int(sys.argv[1]) if len(sys.argv) > 1 else 8192
LAT_N, LAT_S, LON0 = 72.0, -56.0, 11.0
CUT = LON0 - 180.0  # -169: the map's left and right edge, in the Bering Strait's open sea
MIN_PART = 300       # sector parts smaller than this many pixels are left out of the sector map (islands)
LINE = 2             # border lines reach this many pixels either side of a boundary
rng = np.random.RandomState(1898)

def my(lat):
    return 1.25 * np.log(np.tan(np.pi / 4 + 0.4 * np.radians(lat)))
YN, YS = my(LAT_N), my(LAT_S)
HD = int(round(WD * (YN - YS) / (2 * np.pi)))
print(f"map {WD} x {HD}")

def project(lon, lat, east_piece):
    lon = np.asarray(lon, float); lat = np.clip(np.asarray(lat, float), -85, 85)
    x = lon - LON0 + (360.0 if not east_piece else 0.0)
    return (x + 180.0) / 360.0 * WD, (YN - my(lat)) / (YN - YS) * HD

def unproject(px, py):
    lon = px / WD * 360.0 - 180.0 + LON0
    lon = (lon + 180.0) % 360.0 - 180.0
    y = YN - py / HD * (YN - YS)
    lat = np.degrees(2.5 * np.arctan(np.exp(0.8 * y)) - 0.625 * np.pi)
    return lon, lat

def pieces(geom):
    """the geometry split at the map's edge: (piece, east) pairs"""
    out = []
    for b, east in ((box(CUT, -90, 180, 90), True), (box(-180, -90, CUT, 90), False)):
        g = geom.intersection(b)
        if not g.is_empty:
            out.append((g, east))
    return out

def polygons(g):
    if g.geom_type == "Polygon":
        return [g]
    if hasattr(g, "geoms"):
        return [p for q in g.geoms for p in polygons(q)]
    return []

def draw(img, geom, value):
    d = ImageDraw.Draw(img)
    for g, east in pieces(geom):
        for p in polygons(g):
            x, y = project(*np.array(p.exterior.coords).T, east)
            if len(x) > 2:
                d.polygon(list(zip(x, y)), fill=value)
            for hole in p.interiors:  # the Caspian is a hole in Eurasia
                x, y = project(*np.array(hole.coords).T, east)
                if len(x) > 2:
                    d.polygon(list(zip(x, y)), fill=0)

# ------------------------------------------------------------------ the sector map
countries = {}
for f in json.load(open(f"{D}/data/ne_50m_admin_0_countries.geojson"))["features"]:
    code = f["properties"]["ADM0_A3"]
    g = shape(f["geometry"])
    countries[code] = unary_union([countries[code], g]) if code in countries else g

provinces = {}
for f in json.load(open(f"{D}/data/ne_10m_admin_1_states_provinces.geojson"))["features"]:
    pr = f["properties"]
    provinces.setdefault(pr["adm0_a3"], []).append((pr["adm1_code"], pr["longitude"], pr["latitude"], f["geometry"]))
used_prov = set()
def provinces_in(code, clip):
    """the country's provinces whose representative point is in the box, each used once"""
    lo0, la0, lo1, la1 = clip
    gs = []
    for pc, lo, la, g in provinces.get(code, []):
        if pc not in used_prov and lo0 <= lo <= lo1 and la0 <= la <= la1:
            used_prov.add(pc); gs.append(shape(g).buffer(0))
    return unary_union(gs) if gs else None
used = set()
sector_geoms = []
for name, members in SECTORS:
    parts = []
    for m in members:
        code, clip = (m, None) if isinstance(m, str) else m
        if code not in countries:
            print("  no such country:", code); continue
        g = countries[code]
        if clip:
            g = provinces_in(code, clip)
            if g is None:
                print("  no provinces for", code, clip); continue
        else:
            used.add(code)
        if not g.is_empty:
            parts.append(g)
        used.add(code)
    sector_geoms.append(unary_union(parts))
N = len(SECTORS)
left = sorted(c for c in countries if c not in used)
print("countries in no sector:", ", ".join(left))

S = Image.new("L", (WD, HD))
order = sorted(range(N), key=lambda i: -sector_geoms[i].area)
for i in order:
    draw(S, sector_geoms[i], i + 1)
landimg = Image.new("L", (WD, HD))
for f in json.load(open(f"{D}/data/ne_50m_land.geojson"))["features"]:
    draw(landimg, shape(f["geometry"]), 255)
lakes = Image.new("L", (WD, HD))
for f in json.load(open(f"{D}/data/ne_50m_lakes.geojson"))["features"]:
    draw(lakes, shape(f["geometry"]), 255)
lake = np.array(lakes) > 0
S = np.array(S)
land = (np.array(landimg) > 0) | (S > 0)
land &= ~lake
S[~land] = 0
near_d, near_i = ndi.distance_transform_edt(S == 0, return_indices=True)
gap = land & (S == 0) & (near_d <= 6)
S[gap] = S[near_i[0][gap], near_i[1][gap]]
del near_d, near_i
# small parts (islands) leave the sector map, as the original leaves out some of the western isles
for i in range(1, N + 1):
    lab, n = ndi.label(S == i)
    if n > 1 or n == 1:
        sizes = ndi.sum(np.ones_like(lab), lab, range(1, n + 1))
        for k, sz in enumerate(sizes, 1):
            if sz < MIN_PART:
                S[lab == k] = 0
present = [i for i in range(1, N + 1) if (S == i).any()]
assert len(present) == N, f"sectors with no land: {[SECTORS[i-1][0] for i in range(1, N+1) if i not in present]}"

# sea areas: a flood from each named point through the sea, at an eighth of the size
q = 8
sea_small = ~land[::q, ::q]
zones = np.zeros(sea_small.shape, np.int16)
from collections import deque
dq = deque()
for j, (name, lon, lat) in enumerate(SEAS):
    x, y = project(lon, lat, not (lon < CUT))
    yy, xx = int(y) // q, int(x) // q
    yy = min(max(yy, 0), sea_small.shape[0] - 1); xx = min(max(xx, 0), sea_small.shape[1] - 1)
    if not sea_small[yy, xx]:
        ys, xs = np.nonzero(sea_small)
        k_ = np.argmin((ys - yy) ** 2 + (xs - xx) ** 2); yy, xx = ys[k_], xs[k_]
    zones[yy, xx] = j + 1; dq.append((yy, xx))
Hs, Ws = sea_small.shape
while dq:
    yy, xx = dq.popleft()
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        ny, nx = yy + dy, (xx + dx) % Ws
        if 0 <= ny < Hs and sea_small[ny, nx] and zones[ny, nx] == 0:
            zones[ny, nx] = zones[yy, xx]; dq.append((ny, nx))
zones_full = np.kron(zones, np.ones((q, q), np.int16))[:HD, :WD]
ocean = (~land) & (zones_full > 0)

# ------------------------------------------------------------------ borders
k = 2 * LINE + 1
mx = ndi.maximum_filter(S, size=k)
mn = ndi.minimum_filter(S, size=k)
other = np.where(mx != S, mx, np.where((mn != S) & (mn > 0), mn, 0))
internal = (S > 0) & (other > 0)
# a coastline is a shore on the sea (the named sea areas' water), not on a lake
coast = (S > 0) & (other == 0) & (mn == 0) & ndi.maximum_filter(ocean, size=k)
a = np.minimum(S, other)[internal].astype(np.int32); b = np.maximum(S, other)[internal].astype(np.int32)
pairs, inv = np.unique(a * 256 + b, return_inverse=True)
pair_list = [(int(p // 256), int(p % 256)) for p in pairs]
coastal = sorted(int(s) for s in np.unique(S[coast]))
# drop border pairs that only touch at a corner (fewer than 6 pixels)
counts = np.bincount(inv)
keep = counts >= 6
remap = -np.ones(len(pairs), int); remap[keep] = np.arange(keep.sum())
pair_list = [p for p, kp in zip(pair_list, keep) if kp]
BI, BC = len(pair_list), len(coastal)
FIRST_BORDER = N + 2                      # value N + 1 is left unused, as the original leaves 31
coast_first = FIRST_BORDER + BI + 1       # and one between the two kinds, as the original leaves border 64
total_values = coast_first + BC
print(f"{N} sectors, {BI} borders between sectors, {BC} coastlines: values 0-{total_values - 1} "
      f"({total_values} of 256)")
assert total_values <= 256, "too many sector map values"
HI = S.copy()
r = remap[inv]
HI_int = np.where(r >= 0, FIRST_BORDER + r, S[internal])
HI[internal] = HI_int
cidx = {s: coast_first + j for j, s in enumerate(coastal)}
lut = np.zeros(256, np.uint8)
for s, v in cidx.items():
    lut[s] = v
HI[coast] = lut[S[coast]]
borders_of = {i: [] for i in range(1, N + 1)}
for j, (p, q) in enumerate(pair_list):
    borders_of[p].append(FIRST_BORDER + j); borders_of[q].append(FIRST_BORDER + j)
for s, v in cidx.items():
    borders_of[s].append(v)

# ------------------------------------------------------------------ height classes, from etopo1.jpg
eto = np.array(Image.open(f"{D}/data/pkg/etopo1.jpg").convert("RGB"))
EH, EW = eto.shape[:2]
def heights():
    cls = np.zeros((HD, WD), np.uint8)
    for y0 in range(0, HD, 256):
        ys = np.arange(y0, min(HD, y0 + 256))
        X, Y = np.meshgrid(np.arange(WD), ys)
        lon, lat = unproject(X + 0.5, Y + 0.5)
        ex = np.clip(((lon + 180) / 360 * EW).astype(int), 0, EW - 1)
        ey = np.clip(((90 - lat) / 180 * EH).astype(int), 0, EH - 1)
        c = eto[ey, ex].astype(int)
        r_, g_, b_ = c[..., 0], c[..., 1], c[..., 2]
        mxc = np.maximum(np.maximum(r_, g_), b_); mnc = np.minimum(np.minimum(r_, g_), b_)
        sat = (mxc - mnc) / np.maximum(1, mxc)
        # green lowland; yellow and light tan upland; brown, grey and white highland and ice
        low = (g_ >= r_ - 5) & (g_ > b_ + 10)
        high = ((r_ > g_ + 18) & (mxc < 200)) | ((sat < 0.18) & (mxc > 150))
        k_ = np.where(low, 0, np.where(high, 2, 1)).astype(np.uint8)
        water = (b_ > r_ + 15) & (b_ > g_)
        k_[water] = 0
        cls[y0:y0 + len(ys)] = k_
    # smooth the classes, then break their edges up a little
    oh = np.stack([(cls == i).astype(np.float32) for i in range(3)])
    noise = ndi.gaussian_filter(rng.rand(HD // 8 + 1, WD // 8 + 1).astype(np.float32), 1.5)
    noise = ndi.zoom(noise, 8, order=1)[:HD, :WD]
    noise = (noise - noise.mean()) / (noise.std() + 1e-6)
    for i in range(3):
        oh[i] = ndi.gaussian_filter(oh[i], 3.0)
    oh[0] += 0.12 * noise; oh[2] -= 0.12 * noise
    return oh.argmax(0).astype(np.uint8)
hcls = heights()
print("height classes:", [round(float((hcls[land] == i).mean()), 2) for i in range(3)])

# ------------------------------------------------------------------ texture, from the original maps
orig_cls = np.load(f"{D}/orig/classes.npy")   # sea, green, tan, brown, border (the Human map's)
hw = spr.read(O + "HWM.SPR"); mw = spr.read(O + "MWM.SPR")
_c = spr.shade(O + "HWHW.SHH", 11).astype(int)[hw]
landlike = ((_c[..., 1] - _c[..., 2]) > 28) | ((_c[..., 0] - _c[..., 2]) > 30)
landlike = ndi.binary_dilation(ndi.binary_opening(landlike, iterations=2), iterations=6)
orig_cls[0] &= ~landlike
del _c
H0, W0 = hw.shape
# the Martian map's red sky is its top rows: its sea comes from below them, and the sky is laid along the top
mars_sea_ok = orig_cls[0].copy(); mars_sea_ok[:420] = False
offsets = [(int(rng.randint(H0)), int(rng.randint(W0))) for _ in range(16)]

def texture(src, cls_masks, target_cls, region_of):
    """for every pixel of each class, the source's pixel of the same class at the first of the offsets that has
    one (the order of offsets changes from region to region, so the tiling doesn't show)"""
    out = np.zeros((HD, WD), np.uint8)
    for y0 in range(0, HD, 512):
        ys = np.arange(y0, min(HD, y0 + 512))
        X, Y = np.meshgrid(np.arange(WD), ys)
        tc = target_cls[y0:y0 + len(ys)]
        reg = region_of(X, Y)
        res = np.zeros(X.shape, np.uint8)
        done = np.zeros(X.shape, bool)
        for kk in range(len(offsets)):
            oy, ox = offsets[(kk + reg) % len(offsets)][..., 0], offsets[(kk + reg) % len(offsets)][..., 1]
            sy = (Y + oy) % H0; sx = (X + ox) % W0
            for c, m in enumerate(cls_masks):
                want = (tc == c) & ~done
                if not want.any():
                    continue
                hit = want & m[sy, sx]
                res[hit] = src[sy[hit], sx[hit]]
                done |= hit
            if done.all():
                break
        # what's left takes the class's commonest index
        for c, m in enumerate(cls_masks):
            left_ = (tc == c) & ~done
            if left_.any():
                vals, cnts = np.unique(src[m], return_counts=True)
                res[left_] = vals[cnts.argmax()]
        out[y0:y0 + len(ys)] = res
    return out

offsets = np.array(offsets)
def region_of(X, Y):
    return ((X // 700) * 7 + (Y // 600) * 3) % len(offsets)
# ------------------------------------------------------------------ the layers
# Each map is built as layers, the land cut out first: the sea (all of the map), the land (on the land mask), and
# for the Martians' telescope view the weather over both and the red sky along the top. The art carries no border
# or coast lines: the game paints every border pixel of the sector map in a solid colour of its own (levels 59-66),
# whatever the art has under it, so the sectors' borders live in the sector map alone.
target = np.where(land, hcls + 1, 0).astype(np.uint8)
all_sea = np.zeros((HD, WD), np.uint8)
human_sea = texture(hw, [orig_cls[0], orig_cls[1], orig_cls[2], orig_cls[3]], all_sea, region_of)
human_land = texture(hw, [orig_cls[0], orig_cls[1], orig_cls[2], orig_cls[3]], target, region_of)
martian_land = texture(mw, [mars_sea_ok, orig_cls[1], orig_cls[2], orig_cls[3]], target, region_of)

def fractal(h, w, octaves, base, stretch=(1, 1)):
    """smooth noise, the sum of octaves, each a random grid scaled up"""
    out = np.zeros((h, w), np.float32)
    amp = 1.0
    for o in range(octaves):
        cell = max(2, base >> o)
        gh, gw = h // (cell * stretch[0]) + 3, w // (cell * stretch[1]) + 3
        g = rng.rand(gh, gw).astype(np.float32)
        up = ndi.zoom(g, (cell * stretch[0], cell * stretch[1]), order=3)[:h, :w]
        out += amp * up
        amp *= 0.5
    return (out - out.mean()) / (out.std() + 1e-6)

def warped(base, octaves, push, stretch=(1, 2)):
    """a field bent by two others (domain warping): the swirl of weather systems"""
    u = fractal(HD, WD, 4, base * 2, stretch) * push
    v = fractal(HD, WD, 4, base * 2, stretch) * push * 0.66
    f = fractal(HD, WD, octaves, base, stretch)
    out = np.zeros((HD, WD), np.float32)
    for y0 in range(0, HD, 256):
        ys = np.arange(y0, min(HD, y0 + 256))
        X, Y = np.meshgrid(np.arange(WD, dtype=np.float32), ys.astype(np.float32))
        fy = np.clip(Y + v[y0:y0 + len(ys)], 0, HD - 1); fx = np.clip(X + u[y0:y0 + len(ys)], 0, WD - 1)
        out[y0:y0 + len(ys)] = ndi.map_coordinates(f, [fy, fx], order=1)
    return out

MSH = spr.shade(O + "MWMW.SHH", 11).astype(int)
msea_px = mars_sea_ok & ~landlike
m_vals, m_cnts = np.unique(mw[msea_px], return_counts=True)
m_order = np.argsort(MSH[m_vals].sum(1))
m_vals, m_cnts = m_vals[m_order], m_cnts[m_order]
m_cool = MSH[m_vals][:, 2] + 12 >= MSH[m_vals][:, 0]

def by_rank(field, vals, cnts, lo=0.0, hi=1.0):
    """each value of the field given a colour, darkest to brightest, in the colours' own shares, from the lo to the
    hi part of the range"""
    ref = np.sort(field[::9, ::9].ravel())
    edges = np.cumsum(cnts) / cnts.sum()
    out = np.zeros(field.shape, np.uint8)
    for y0 in range(0, HD, 512):
        rank = np.searchsorted(ref, field[y0:y0 + 512]) / ref.size
        rank = lo + (hi - lo) * rank
        out[y0:y0 + 512] = vals[np.minimum(np.searchsorted(edges, rank), len(vals) - 1)]
    return out

# the Martian sea: the dark water under the weather, slow swirls in the darker two thirds of the original's colours
martian_sea = by_rank(warped(256, 6, 90), m_vals[m_cool], m_cnts[m_cool], 0.0, 0.62)

# the Martian weather, over land and sea: bright swirling cloud where the field is high, thinning at its edges
wfield = warped(320, 6, 140, (1, 3))
t0, t1 = np.quantile(wfield[::9, ::9], [0.70, 0.96])
cover = np.clip((wfield - t0) / (t1 - t0), 0, 1) ** 1.2
weather_mask = rng.rand(HD, WD) < cover
# as in the original, the weather is in the sea's picture and never covers the land; weather over the land is kept as
# a layer of its own (land_weather_mask), for a weather system that could be drawn over the map separately
land_weather_mask = weather_mask & land & (rng.rand(HD, WD) < 0.65)
weather_mask &= ~land
weather = by_rank(wfield, m_vals[m_cool], m_cnts[m_cool], 0.55, 1.0)

# the Martian sky along the top: the original's top rows, mirrored side by side, fading out below
sky_h = 420
sky = mw[:sky_h]
tiles = [sky if t % 2 == 0 else sky[:, ::-1] for t in range(WD // W0 + 2)]
sky_layer = np.zeros((HD, WD), np.uint8)
sky_layer[:sky_h] = np.concatenate(tiles, 1)[:, :WD]
sky_mask = np.zeros((HD, WD), bool)
sky_mask[:sky_h] = rng.rand(sky_h, WD) < (np.linspace(1, 0, sky_h)[:, None] ** 1.5)
sky_mask[:sky_h] &= ~land[:sky_h]

# a graticule every 15 degrees on the Human sea, in the sea's darker shades
def graticule(img, src_sea_vals):
    vals, cnts = np.unique(src_sea_vals, return_counts=True)
    shade = spr.shade(O + "HWHW.SHH", 11).astype(int)
    dark = vals[np.argsort(shade[vals].sum(1))][: max(3, len(vals) // 12)]
    g = np.zeros((HD, WD), bool)
    for lon in range(-180, 180, 15):
        x = int(round(((lon - LON0 + 180) % 360) / 360 * WD))
        g[:, max(0, x - 1):x + 1] = True
    for lat in range(-45, 75, 15):
        y = int(round((YN - my(lat)) / (YN - YS) * HD))
        if 0 <= y < HD:
            g[max(0, y - 1):y + 1, :] = True
    img[g] = rng.choice(dark, g.sum())
graticule(human_sea, hw[orig_cls[0]])

# the composites
human = np.where(land, human_land, human_sea)
martian = np.where(land, martian_land, martian_sea)
martian[weather_mask] = weather[weather_mask]
martian[sky_mask] = sky_layer[sky_mask]

# ------------------------------------------------------------------ the mini-maps, the sea areas
MINI = 32
def mini(img):
    return img[MINI // 2::MINI, MINI // 2::MINI].copy()
def mini_hi(sm):
    h, w = sm.shape[0] // MINI, sm.shape[1] // MINI
    blocks = sm[:h * MINI, :w * MINI].reshape(h, MINI, w, MINI).transpose(0, 2, 1, 3).reshape(h, w, -1)
    out = np.zeros((h, w), np.uint8)
    for yy in range(h):
        for xx in range(w):
            vals, cnts = np.unique(blocks[yy, xx], return_counts=True)
            if (vals > 0).any():
                vv, cc = vals[vals > 0], cnts[vals > 0]
                if cc.max() * 4 >= MINI * MINI:
                    out[yy, xx] = vv[cc.argmax()]
    return out

CHI = np.where(S > 0, 0, 0).astype(np.uint8)
eroded = ndi.minimum_filter(S, size=9)
CHI[(eroded > 0) & (eroded == S)] = S[(eroded > 0) & (eroded == S)]
seaz = (~land) & (zones_full > 0)
CHI[seaz] = (191 + zones_full[seaz]).astype(np.uint8)
assert 191 + len(SEAS) <= 255

# ------------------------------------------------------------------ write
os.makedirs(f"{OUT}/DAT", exist_ok=True); os.makedirs(f"{OUT}/preview", exist_ok=True)
files = {
    "HWM.SPR": human, "MWM.SPR": martian, "HWMHI.SPR": HI, "MWMHI.SPR": HI,
    "HMINI.SPR": mini(human), "mmini.spr": mini(martian), "HMINIHI.SPR": mini(S), "mminihi.spr": mini(S),
    "CHI.SPR": CHI,
}
for name, img in files.items():
    spr.write(f"{OUT}/DAT/{name}", img)
    print(f"  {name:12s} {img.shape[1]} x {img.shape[0]}  {os.path.getsize(f'{OUT}/DAT/{name}') / 1e6:.1f} MB")

# previews, lit as the game lights them: the art at level 11, and with the sector map's borders as a new campaign
# would show them (level 65, the same owner either side), and the layers as palette PNGs that keep the game's
# colour indices (index 0, the sprites' transparent colour, outside a layer)
HS = spr.shade(O + "HWHW.SHH", 11); MS = spr.shade(O + "MWMW.SHH", 11)
HB = spr.shade(O + "HWHW.SHH", 65); MB = spr.shade(O + "MWMW.SHH", 65)
def save_preview(arr, name, scale=4):
    Image.fromarray(arr).resize((WD // scale, HD // scale), Image.LANCZOS).save(f"{OUT}/preview/{name}")
bord = HI >= FIRST_BORDER
for side, art, lvl, blv in (("human", human, HS, HB), ("martian", martian, MS, MB)):
    clean = lvl[art]
    save_preview(clean, f"{side}-warmap.png")
    withb = clean.copy(); withb[bord] = blv[art][bord]
    save_preview(withb, f"{side}-warmap-with-borders.png")
os.makedirs(f"{OUT}/layers", exist_ok=True)
def layer(arr, mask, pal, name):
    a_ = np.where(mask, arr, 0).astype(np.uint8)
    im = Image.fromarray(a_, "P")
    im.putpalette(pal.astype(np.uint8).ravel().tolist())
    im.save(f"{OUT}/layers/{name}", transparency=0, optimize=True)
everywhere = np.ones((HD, WD), bool)
Image.fromarray((land * 255).astype(np.uint8)).save(f"{OUT}/layers/0-land-mask.png", optimize=True)
layer(human_sea, everywhere, HS, "human-1-sea.png")
layer(human_land, land, HS, "human-2-land.png")
layer(martian_sea, everywhere, MS, "martian-1-sea.png")
layer(martian_land, land, MS, "martian-2-land.png")
layer(weather, weather_mask, MS, "martian-3-weather.png")
layer(weather, land_weather_mask, MS, "martian-optional-weather-over-land.png")
layer(sky_layer, sky_mask, MS, "martian-4-sky.png")
# the sector map in false colours, with the sectors' numbers and names
pal = (rng.randint(60, 230, (256, 3))).astype(np.uint8); pal[0] = (18, 26, 38)
pal[FIRST_BORDER:] = (250, 250, 250)
for v in range(coast_first, total_values):
    pal[v] = (255, 210, 90)
for v in range(192, 192 + len(SEAS)):
    pass
sc = 4
fc = Image.fromarray(pal[HI]).resize((WD // sc, HD // sc), Image.NEAREST)
dd = ImageDraw.Draw(fc)
fnt = ImageFont.truetype("/home/user/trailer/fonts/IMFeENrm28P.ttf", 15)
info = []
dist = ndi.distance_transform_edt(np.pad(S, 1) > 0)[1:-1, 1:-1]
for i in range(1, N + 1):
    m = S == i
    ys, xs = np.nonzero(m)
    dm = np.where(m, dist, 0)
    wy, wx = np.unravel_index(dm.argmax(), dm.shape)
    lon, lat = unproject(wx + 0.5, wy + 0.5)
    lats = np.radians(unproject(0, np.arange(HD) + 0.5)[1])
    km2 = float((m * (np.cos(lats) * np.cos(0.8 * lats))[:, None]).sum()) * (40075 / WD) ** 2
    info.append({"id": i, "name": SECTORS[i - 1][0], "value": i,
                 "bounds": [int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())],
                 "waypoint": [int(wx), int(wy)], "waypoint_lon_lat": [round(float(lon), 2), round(float(lat), 2)],
                 "area_km2": int(round(km2, -3)), "coastal": i in cidx,
                 "neighbours": sorted({q_ for p_, q_ in pair_list if p_ == i} | {p_ for p_, q_ in pair_list if q_ == i}),
                 "borders": sorted(borders_of[i]),
                 "members": [m_ if isinstance(m_, str) else f"{m_[0]} {list(m_[1])}" for m_ in SECTORS[i - 1][1]]})
    t = f"{i} {SECTORS[i - 1][0]}"
    tw = dd.textlength(t, font=fnt)
    dd.text((wx / sc - tw / 2 + 1, wy / sc - 7 + 1), t, font=fnt, fill=(0, 0, 0))
    dd.text((wx / sc - tw / 2, wy / sc - 7), t, font=fnt, fill=(255, 255, 255))
fc.save(f"{OUT}/preview/sectors-numbered.png")
zp = (rng.randint(40, 120, (256, 3))).astype(np.uint8); zp[0] = (0, 0, 0)
Image.fromarray(zp[CHI]).resize((WD // sc, HD // sc), Image.NEAREST).save(f"{OUT}/preview/sea-areas.png")

# sea links: coastal sectors that touch the same sea area
touch = {}
grown = ndi.maximum_filter(np.where(S > 0, S, 0), size=7)
for j in range(1, len(SEAS) + 1):
    zm = (zones_full == j) & ~land
    ss = set(np.unique(grown[zm]).tolist()) - {0}
    touch[j] = sorted(int(x) for x in ss)
for it in info:
    it["sea_areas"] = sorted(192 + j - 1 for j, ss in touch.items() if it["id"] in ss)
data = {
    "about": "A draft world war map for a future WoWEngine mod. Not usable by the original game or the current engine "
             "(both have 30 sectors built in); see README.md.",
    "map": {"width": WD, "height": HD, "projection": "Miller cylindrical", "central_meridian": LON0,
            "lat_north": LAT_N, "lat_south": LAT_S, "km_per_pixel_at_equator": round(40075 / WD, 3)},
    "values": {"sea": 0, "sectors": [1, N], "unused": N + 1, "borders_between_sectors": [FIRST_BORDER, FIRST_BORDER + BI - 1],
               "unused_2": FIRST_BORDER + BI, "coastlines": [coast_first, total_values - 1], "total_used": total_values,
               "sea_areas_in_CHI": [192, 191 + len(SEAS)]},
    "sectors": info,
    "borders": [{"value": FIRST_BORDER + j, "sectors": [p, q_]} for j, (p, q_) in enumerate(pair_list)] +
               [{"value": v, "sectors": [s, 0], "coastline": True} for s, v in cidx.items()],
    "sea_areas": [{"value": 191 + j, "name": n, "lon_lat": [lo, la], "coastal_sectors": touch[j]} for j, (n, lo, la) in enumerate(SEAS, 1)],
}
json.dump(data, open(f"{OUT}/world.json", "w"), indent=1)
mb = max(len(v) for v in borders_of.values())
print("most borders on one sector:", mb, [SECTORS[i - 1][0] for i, v in borders_of.items() if len(v) == mb])
print("done")
