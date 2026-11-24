#!/usr/bin/env python3
"""The world at the original war map's scale, built a window at a time.

    worldtile.py calibrate          set the height bands from Great Britain and the noise scales (consts.json)
    worldtile.py britain            Britain beside the original, the same size and scale
    worldtile.py tile <col> <row>   one 8192 x 8192 tile of the world (the last column and row are smaller)

The frame: Miller cylindrical, both poles, scaled separately across and down so that at 54 N a pixel is the
original's 0.40 km across and 0.442 km down; the world is then about 58,800 x 48,400 pixels. Everything made here,
the land, the heights, the texture, the noise of the Martian sea and weather, is worked out from world positions,
so neighbouring tiles join without a seam."""
import glob, json, math, os, sys
import numpy as np
import netCDF4
from PIL import Image, ImageDraw
from scipy import ndimage as ndi
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import spr

D = "/home/user/worldmap"
O = f"{D}/orig/all/"
OUT = f"{D}/mod/WorldMap/world"
CONSTS = f"{D}/tools/consts.json"
R = 6371.0
KX, KY, LAT_REF = 0.40, 0.442, 54.0
EX = KX / math.cos(math.radians(LAT_REF))
EY = KY / math.cos(math.radians(0.8 * LAT_REF))
LON0 = 11.0
WORLD_W = int(round(2 * math.pi * R / EX))
YTOP = 1.25 * math.log(math.tan(math.pi / 4 + 0.4 * math.radians(89.999)))
WORLD_H = int(round(2 * YTOP * R / EY))
TILE = 8192
MARGIN = 384          # extra around a window for the relief's neighbourhood
FLAT_RELIEF = 15.0    # metres: land within this of its surroundings' height is plain

def my(lat):
    return 1.25 * np.log(np.tan(np.pi / 4 + 0.4 * np.radians(lat)))
def world_xy(lon, lat):
    x = ((np.asarray(lon, float) - LON0 + 180) % 360) / 360 * WORLD_W
    return x, (YTOP - my(np.clip(lat, -89.999, 89.999))) * R / EY
def world_lonlat(x, y):
    lon = (np.asarray(x) / WORLD_W * 360 - 180 + LON0 + 180) % 360 - 180
    lat = np.degrees(2.5 * np.arctan(np.exp(0.8 * (YTOP - np.asarray(y) * EY / R))) - 0.625 * np.pi)
    return lon, lat

# ------------------------------------------------------------------ noise from world positions
def lattice(ix, iy, seed):
    h = (ix.astype(np.int64) * 374761393 + iy.astype(np.int64) * 668265263 + seed * 2246822519) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h ^= h >> 16
    return (h & 0xFFFFFF).astype(np.float32) / float(0xFFFFFF)

def value_noise(X, Y, cx, cy, seed):
    """smooth noise at world pixels X, Y (arrays), on a lattice cx by cy pixels that wraps round the world"""
    ncx = max(1, int(round(WORLD_W / cx))); cx = WORLD_W / ncx
    gx = X / cx; gy = Y / cy
    ix0, iy0 = int(np.floor(gx.min())) - 1, int(np.floor(gy.min())) - 1
    ix1, iy1 = int(np.floor(gx.max())) + 3, int(np.floor(gy.max())) + 3
    IX, IY = np.meshgrid(np.arange(ix0, ix1), np.arange(iy0, iy1))
    L = lattice(IX % ncx, IY, seed)
    return ndi.map_coordinates(L, [gy - iy0, gx - ix0], order=3, mode="nearest").astype(np.float32)

def fractal(X, Y, base, octaves, seed, stretch=(1.0, 1.0)):
    out = np.zeros(X.shape, np.float32); amp, tot = 1.0, 0.0
    for o in range(octaves):
        cell = max(2.0, base / 2 ** o)
        out += amp * value_noise(X, Y, cell * stretch[1], cell * stretch[0], seed * 31 + o)
        tot += amp; amp *= 0.5
    return out / tot

def warped(X, Y, base, octaves, push, seed, stretch=(1.0, 2.0)):
    u = (fractal(X, Y, base * 2, 4, seed + 1, stretch) - 0.5) * 2 * push
    v = (fractal(X, Y, base * 2, 4, seed + 2, stretch) - 0.5) * 2 * push * 0.66
    return fractal(X + u, Y + v, base, octaves, seed, stretch)

# the fields' settings: (base, octaves, push, seed, stretch)
SEA_FIELD = (1600, 8, 600, 11, (1.0, 2.0))
WEATHER_FIELD = (2400, 8, 1000, 23, (1.0, 3.0))

# ------------------------------------------------------------------ land, heights
META = None
def land_mask(x0, y0, w, h):
    global META
    if META is None:
        META = [l.split() for l in open(f"{D}/data/pkg/gshhsmeta_f.dat")]
    lon_a, lat_a = world_lonlat(x0, y0); lon_b, lat_b = world_lonlat(x0 + w, y0 + h)
    img = Image.new("L", (w, h)); d = ImageDraw.Draw(img)
    full_width = w >= WORLD_W - 2
    with open(f"{D}/data/pkg/gshhs_f.dat", "rb") as dat:
        for m in META:
            level = int(m[0]); south, north = float(m[3]), float(m[4])
            if north < lat_b - 1 or south > lat_a + 1 or level not in (1, 2, 3, 4, 5, 6):
                continue
            dat.seek(int(m[5]))
            pts = np.frombuffer(dat.read(int(m[6])), "<f4").reshape(-1, 2).astype(np.float64)
            lon = np.degrees(np.unwrap(np.radians(pts[:, 0])))
            x = (lon - LON0 + 180) / 360 * WORLD_W
            _, y = world_xy(0, pts[:, 1])
            for shift in (-WORLD_W, 0, WORLD_W, 2 * WORLD_W):
                xs = x + shift - x0
                if xs.max() < 0 or xs.min() > w:
                    continue
                if len(xs) > 2:
                    d.polygon(list(zip(xs, y - y0)), fill=0 if level in (2, 4) else 255)
    return np.array(img) > 0

ETOPO = None
def heights(x0, y0, w, h):
    global ETOPO
    if ETOPO is None:
        ETOPO = []
        for f in sorted(glob.glob(f"{D}/data/etopo15/*.nc")):
            try:
                ds = netCDF4.Dataset(f)
            except OSError:
                continue  # one still downloading
            la = ds.variables["lat"][:]; lo = ds.variables["lon"][:]
            ETOPO.append((f, float(la.min()), float(la.max()), float(lo.min()), float(lo.max())))
            ds.close()
    out = np.zeros((h, w), np.float32)
    for r0 in range(0, h, 1024):
        rows = np.arange(r0, min(h, r0 + 1024))
        Xc, Yc = np.meshgrid(np.arange(w) + x0 + 0.5, rows + y0 + 0.5)
        lon, lat = world_lonlat(Xc, Yc)
        block = np.zeros(lon.shape, np.float32)
        for f, la0, la1, lo0, lo1 in ETOPO:
            sel = (lat >= la0 - 1e-6) & (lat <= la1 + 1e-6) & (lon >= lo0 - 1e-6) & (lon <= lo1 + 1e-6)
            if not sel.any():
                continue
            ds = netCDF4.Dataset(f)
            la = ds.variables["lat"][:]; lo = ds.variables["lon"][:]
            z = np.asarray(ds.variables["z"][:])
            iy = np.clip(np.round((lat[sel] - la[0]) / (la[1] - la[0])).astype(int), 0, len(la) - 1)
            ix = np.clip(np.round((lon[sel] - lo[0]) / (lo[1] - lo[0])).astype(int), 0, len(lo) - 1)
            block[sel] = z[iy, ix]
            ds.close()
        out[r0:r0 + len(rows)] = block
    return out

REGIONAL = None
def regional(x0, y0, h, w, scale):
    """the region's mean height and spread at each pixel of a window (world pixels x0, y0, at 1/scale), from the
    world's coarse grid (tools/preview_bands.py makes it): seamless, as every tile reads the same grid"""
    global REGIONAL
    if REGIONAL is None:
        z = np.load(f"{D}/tools/regional.npz"); REGIONAL = (z["mean"], z["std"], int(z["step"]))
    mean, std, step = REGIONAL
    ys = (np.arange(h) * scale + y0 + 0.5) / step - 0.5
    xs = (np.arange(w) * scale + x0 + 0.5) / step - 0.5
    out = []
    for grid in (mean, std):
        pad = np.concatenate([grid[:, -2:], grid, grid[:, :2]], 1)   # wraps round the world
        res = np.empty((h, w), np.float32)
        for r0 in range(0, h, 1024):
            Yg, Xg = np.meshgrid(ys[r0:r0 + 1024], xs, indexing="ij")
            res[r0:r0 + 1024] = ndi.map_coordinates(pad, [np.clip(Yg, 0, grid.shape[0] - 1), (Xg % grid.shape[1]) + 2], order=1)
        out.append(res)
    return out

def height_classes(land, hgt, consts=None, lat=None, scale=1, origin=None):
    """0 lowland, 1 upland, 2 highland: relief over a 50 km neighbourhood with the height mixed in, smoothed into
    coherent bands (as the original's are, nested like contours) and cleared of specks"""
    hs = ndi.gaussian_filter(hgt, 2.5 / scale)
    low = np.where(land, hs, 0)
    sg = 120 / scale
    surround = ndi.gaussian_filter(low, sg) / np.maximum(ndi.gaussian_filter(land.astype(np.float32), sg), 1e-3)
    rel = hs - surround
    # height read against its region (about 300 km round): how far above the region's mean, in its spreads (at least
    # 150 m), so every region shows its lowlands, hills and mountains as the original shows Britain's; the local
    # relief adds a little
    if origin is not None and os.path.exists(f"{D}/tools/regional.npz"):
        rmean, rstd = regional(origin[0], origin[1], hs.shape[0], hs.shape[1], scale)
        score = (hs - rmean) / np.maximum(rstd, 150.0) + rel / 600.0
        # and against the world: the great ranges and plateaux are high whatever their region (above 1,500 m the
        # score rises, by a band's width for every 1,000 m)
        score += np.maximum(hs - 1500.0, 0) / 1000.0
        del rmean, rstd
    else:
        score = rel + 0.35 * np.minimum(hs, 400) + 0.08 * np.maximum(hs - 400, 0)
    # fixed by rule, before the cuts: flat land below 350 m (and anything below the plain height) is lowland; flat
    # high land, a plateau (flat allowing more relief the higher it is), upland; land over 2,500 m highland; the polar
    # ice sheets upland, as a pale tint
    fixed = np.full(hs.shape, -1, np.int8)
    flatish = np.abs(rel) < FLAT_RELIEF + 0.04 * np.maximum(hs, 0)
    plain = (consts or {}).get("plain", -1e9)
    fixed[flatish & (hs >= 350) & (score > 0)] = 1
    if lat is not None:
        fixed[(np.abs(lat) > 58) & (hs > 400) & (np.abs(rel) < 150)] = 1
        fixed[(np.abs(lat) > 58) & (hs > 400) & (np.abs(rel) >= 150)] = 2   # the ice's nunataks and ranges
    fixed[(hs < plain) | ((np.abs(rel) < FLAT_RELIEF) & (hs < 350))] = 0
    if consts is None or "t1" not in consts:
        return score, hs, fixed
    t1, t2 = consts["t1"], consts["t2"]
    cls = np.where(score < t1, 0, np.where(score < t2, 1, 2)).astype(np.uint8)
    cls[fixed >= 0] = fixed[fixed >= 0]
    cls[~land] = 0
    # bands as the original draws them: the majority of each neighbourhood, then no island of a band smaller than
    # about a kilometre and a half square
    oh = np.stack([ndi.gaussian_filter((cls == i).astype(np.float32), 1.6 / scale) for i in range(3)])
    cls = oh.argmax(0).astype(np.uint8); cls[~land] = 0
    for band, into in ((2, 1), (1, 0)):
        lab, n = ndi.label(cls == band)
        if n:
            sizes = ndi.sum(np.ones_like(lab, np.int32), lab, np.arange(1, n + 1))
            small = np.zeros(n + 1, bool); small[1:] = sizes < 12 / scale ** 2
            cls[small[lab]] = into
    return cls

# ------------------------------------------------------------------ texture, from the original maps
_orig = {}
def originals():
    if not _orig:
        hw = spr.read(O + "HWM.SPR"); mw = spr.read(O + "MWM.SPR")
        cls = np.load(f"{D}/orig/classes.npy")
        c = spr.shade(O + "HWHW.SHH", 11).astype(int)[hw]
        landlike = ((c[..., 1] - c[..., 2]) > 28) | ((c[..., 0] - c[..., 2]) > 30)
        landlike = ndi.binary_dilation(ndi.binary_opening(landlike, iterations=2), iterations=6)
        cls[0] &= ~landlike
        msea = cls[0].copy(); msea[:420] = False
        rng = np.random.RandomState(1898)
        offs = np.array([(int(rng.randint(hw.shape[0])), int(rng.randint(hw.shape[1]))) for _ in range(16)])
        _orig.update(hw=hw, mw=mw, cls=cls, msea=msea, offs=offs)
    return _orig

def texture(src, masks, target, x0, y0):
    o = originals(); offs = o["offs"]; H0, W0 = src.shape
    h, w = target.shape
    out = np.zeros((h, w), np.uint8)
    for r0 in range(0, h, 512):
        ys = np.arange(r0, min(h, r0 + 512))
        X, Y = np.meshgrid(np.arange(w) + x0, ys + y0)
        X = X % WORLD_W
        tc = target[r0:r0 + len(ys)]
        # each 512 pixel region has offsets of its own, hashed from its place in the world, so the original's
        # texture never repeats in a pattern however large the area of one band
        RX, RY = X // 512, Y // 512
        res = np.zeros(X.shape, np.uint8); done = np.zeros(X.shape, bool)
        for kk in range(16):
            oy_ = (lattice(RX, RY, 1000 + kk) * H0).astype(np.int64)
            ox_ = (lattice(RX, RY, 2000 + kk) * W0).astype(np.int64)
            sy = (Y + oy_) % H0; sx = (X + ox_) % W0
            for c, m in enumerate(masks):
                want = (tc == c) & ~done
                if want.any():
                    hit = want & m[sy, sx]; res[hit] = src[sy[hit], sx[hit]]; done |= hit
            if done.all():
                break
        for c, m in enumerate(masks):
            left_ = (tc == c) & ~done
            if left_.any():
                vals, cnts = np.unique(src[m], return_counts=True); res[left_] = vals[cnts.argmax()]
        out[r0:r0 + len(ys)] = res
    return out

def palette_by_rank(field, ref, vals, cnts, lo, hi, rng):
    edges = np.cumsum(cnts) / cnts.sum()
    rank = np.searchsorted(ref, field) / len(ref)
    rank = np.clip(rank + rng.normal(0, 0.025, rank.shape), 0, 1)
    return vals[np.minimum(np.searchsorted(edges, lo + (hi - lo) * rank), len(vals) - 1)]

# ------------------------------------------------------------------ a window of the world
def build(x0, y0, w, h, consts):
    rng = np.random.RandomState((x0 * 7919 + y0) % (2 ** 31))
    o = originals(); hw, mw, cls_o = o["hw"], o["mw"], o["cls"]
    # land and heights with a margin, for the relief's neighbourhood
    mx0, my0 = x0 - MARGIN, max(0, y0 - MARGIN)
    mw_, mh_ = w + 2 * MARGIN, min(WORLD_H, y0 + h + MARGIN) - my0
    land_m = land_mask(mx0, my0, mw_, mh_)
    if land_m.any():
        _, lat_m = world_lonlat(np.zeros(1), (np.arange(mh_) + my0 + 0.5)[:, None])
        hcls_m = height_classes(land_m, heights(mx0, my0, mw_, mh_), consts, lat_m, 1, (mx0, my0))
    else:
        hcls_m = np.zeros(land_m.shape, np.uint8)
    sy0 = y0 - my0
    land = land_m[sy0:sy0 + h, MARGIN:MARGIN + w]; hcls = hcls_m[sy0:sy0 + h, MARGIN:MARGIN + w]
    del land_m, hcls_m
    target = np.where(land, hcls + 1, 0).astype(np.uint8)
    masks_h = [cls_o[0], cls_o[1], cls_o[2], cls_o[3]]
    masks_m = [o["msea"], cls_o[1], cls_o[2], cls_o[3]]
    human = texture(hw, masks_h, target, x0, y0)
    martian = texture(mw, masks_m, np.where(land, target, 0), x0, y0) if land.any() else np.zeros((h, w), np.uint8)
    # the Martian sea and its weather (in the sea only, as the original's)
    MSH = spr.shade(O + "MWMW.SHH", 11).astype(int)
    vals, cnts = np.unique(mw[o["msea"] & ~ndi.binary_dilation(cls_o[1] | cls_o[2] | cls_o[3], iterations=1)], return_counts=True)
    order = np.argsort(MSH[vals].sum(1)); vals, cnts = vals[order], cnts[order]
    cool = MSH[vals][:, 2] + 12 >= MSH[vals][:, 0]
    sea_ref = np.array(consts["sea_ref"], np.float32); wea_ref = np.array(consts["weather_ref"], np.float32)
    up = lambda a_, n_, m_: np.repeat(np.repeat(a_, 2, 0), 2, 1)[:n_, :m_]
    for r0 in range(0, h, 1024):
        ys = np.arange(r0, min(h, r0 + 1024))
        sea_rows = ~land[r0:r0 + len(ys)]
        if not sea_rows.any():
            continue
        # the fields at half size on a grid of even world pixels (tiles start on even pixels), doubled: they are
        # broad, and dithered after
        X, Y = np.meshgrid((np.arange(0, w, 2) + x0).astype(np.float32), (np.arange(0, len(ys), 2) + r0 + y0).astype(np.float32))
        n_, m_ = len(ys), w
        b, oc, p, sd, st = SEA_FIELD
        f = up(warped(X, Y, b, oc, p, sd, st), n_, m_)
        sea = palette_by_rank(f, sea_ref, vals[cool], cnts[cool], 0.0, 0.62, rng)
        b, oc, p, sd, st = WEATHER_FIELD
        wf = up(warped(X, Y, b, oc, p, sd, st), n_, m_)
        t0, t1 = consts["weather_cut"]
        cover = rng.rand(*wf.shape) < np.clip((wf - t0) / (t1 - t0), 0, 1) ** 1.2
        wea = palette_by_rank(wf, wea_ref, vals[cool], cnts[cool], 0.55, 1.0, rng)
        block = martian[r0:r0 + len(ys)]
        block[sea_rows] = sea[sea_rows]
        cw = cover & sea_rows
        block[cw] = wea[cw]
    # the red sky along the top of the Martian world
    if y0 < 420:
        sky = mw[:420]; W0 = sky.shape[1]
        rows = np.arange(y0, min(420, y0 + h))
        X = (np.arange(w) + x0) % WORLD_W
        tile_i = X // W0; xx = X % W0
        xx = np.where(tile_i % 2 == 1, W0 - 1 - xx, xx)
        for r in rows:
            fade = (1 - r / 420) ** 1.5
            pick = (rng.rand(w) < fade) & ~land[r - y0]
            martian[r - y0][pick] = sky[r][xx[pick]]
    # the coastline drawn into the art, as the original's is: two pixels on the land side of the shore, in each map's
    # own line colours (the commonest of the original's border pixels)
    shore = land & ~ndi.binary_erosion(land, iterations=2, border_value=1)
    for img, src in ((human, hw), (martian, mw)):
        lv, lc = np.unique(src[cls_o[4]], return_counts=True)
        top = np.argsort(-lc)[:6]
        img[shore] = rng.choice(lv[top], shore.sum(), p=lc[top] / lc[top].sum())
    # a faint graticule every 5 degrees on the Human sea
    HSH = spr.shade(O + "HWHW.SHH", 11).astype(int)
    sv = np.unique(hw[cls_o[0]]); by_light = sv[np.argsort(HSH[sv].sum(1))]
    line = by_light[len(by_light) // 6: len(by_light) // 3]
    g = np.zeros((h, w), bool)
    for lon in range(-180, 180, 5):
        x = int(round(float(world_xy(lon, 0)[0]))) - x0
        for xx in (x, x + WORLD_W, x - WORLD_W):
            if 0 <= xx < w: g[:, xx] = True
    for lat in range(-85, 90, 5):
        y = int(round(float(world_xy(0, lat)[1]))) - y0
        if 0 <= y < h: g[y, :] = True
    g &= ~land
    human[g] = rng.choice(line, g.sum())
    return human, martian, land

# ------------------------------------------------------------------ calibration
def calibrate():
    # the bands: Great Britain's land (the island with London on it) cut to the original's shares
    cxw, cyw = world_xy(-3.0, 54.6)
    x0, y0, w, h = int(cxw) - 1400, int(cyw) - 1700, 2800, 3400
    mx0, my0 = x0 - MARGIN, y0 - MARGIN
    land = land_mask(mx0, my0, w + 2 * MARGIN, h + 2 * MARGIN)
    hgt = heights(mx0, my0, w + 2 * MARGIN, h + 2 * MARGIN)
    score, hs, fixed = height_classes(land, hgt, {"plain": -1e9}, None, 1, (mx0, my0))
    lx, ly = world_xy(-0.12, 51.5)
    lab, _ = ndi.label(land)
    gb = lab == lab[int(ly) - my0, int(lx) - mx0]
    cls = np.load(f"{D}/orig/classes.npy")
    shares = np.array([cls[i].sum() for i in (1, 2, 3)], float); shares /= shares.sum()
    plain = float(np.quantile(hs[gb], shares[0] * 0.6))
    _, _, fixed = height_classes(land, hgt, {"plain": plain}, None, 1, (mx0, my0))
    # the cuts are set among the land the rules leave open, so that with the ruled land Britain has the original's
    # shares
    open_ = gb & (fixed < 0)
    n_gb = gb.sum(); n_open = open_.sum()
    want = [max(0.0, shares[i] * n_gb - (gb & (fixed == i)).sum()) for i in range(3)]
    g_rest = want[0] / n_open; t_rest = want[1] / n_open
    t1, t2 = np.quantile(score[open_], [min(0.999, g_rest), min(0.999, g_rest + t_rest)])
    consts = {"t1": float(t1), "t2": float(t2), "plain": plain, "shares": shares.tolist()}
    got = height_classes(land, hgt, consts, None, 1, (mx0, my0))
    consts["britain_shares_after_smoothing"] = [round(float((got[gb] == i).mean()), 3) for i in range(3)]
    # the noise fields' value scales, from a sample of each, so every tile maps them the same way
    Xs, Ys = np.meshgrid(np.arange(0, WORLD_W, 23, dtype=np.float32), np.arange(0, WORLD_H, 23, dtype=np.float32))
    b, oc, p, sd, st = SEA_FIELD
    consts["sea_ref"] = np.quantile(warped(Xs, Ys, b, oc, p, sd, st), np.linspace(0, 1, 2001)).tolist()
    b, oc, p, sd, st = WEATHER_FIELD
    wf = warped(Xs, Ys, b, oc, p, sd, st)
    consts["weather_ref"] = np.quantile(wf, np.linspace(0, 1, 2001)).tolist()
    consts["weather_cut"] = np.quantile(wf, [0.70, 0.96]).tolist()
    json.dump(consts, open(CONSTS, "w"))
    print("bands:", {k: (round(v, 1) if isinstance(v, float) else v) for k, v in consts.items() if not k.endswith(("ref",))})

def shaded(human, martian):
    HS = spr.shade(O + "HWHW.SHH", 11); MS = spr.shade(O + "MWMW.SHH", 11)
    return HS[human], MS[martian]

if __name__ == "__main__":
    mode = sys.argv[1]
    if mode == "calibrate":
        calibrate(); sys.exit()
    consts = json.load(open(CONSTS))
    os.makedirs(OUT, exist_ok=True)
    if mode == "britain":
        hw = originals()["hw"]; hi = spr.read(O + "HWMHI.SPR")
        H0, W0 = hw.shape
        oy, ox = np.argwhere((hi >= 1) & (hi <= 30)).mean(0)
        cxw, cyw = world_xy(-3.0, 54.6)
        x0, y0 = int(cxw) - 1500, int(cyw) - 1900
        human, martian, land = build(x0, y0, 3000, 3800, consts)
        lx, ly = world_xy(-0.12, 51.5)
        lab, _ = ndi.label(land); gbm = lab == lab[int(ly) - y0, int(lx) - x0]
        ty, tx = np.argwhere(gbm).mean(0)
        y1, x1 = int(ty - oy), int(tx - ox)
        hsd, msd = shaded(human, martian)
        HS = spr.shade(O + "HWHW.SHH", 11)
        pair = Image.new("RGB", (W0 * 2 + 40, H0), (20, 20, 20))
        pair.paste(Image.fromarray(HS[hw]), (0, 0)); pair.paste(Image.fromarray(hsd[y1:y1 + H0, x1:x1 + W0]), (W0 + 40, 0))
        pair.save(f"{OUT}/britain-original-and-ours.png")
        pair.resize((pair.width // 3, pair.height // 3), Image.LANCZOS).save(f"{OUT}/britain-original-and-ours-third.png")
        Image.fromarray(msd[y1:y1 + H0, x1:x1 + W0]).resize((W0 // 3, H0 // 3), Image.LANCZOS).save(f"{OUT}/britain-martian-third.png")
        print("britain done")
    elif mode == "tile":
        col, row = int(sys.argv[2]), int(sys.argv[3])
        x0, y0 = col * TILE, row * TILE
        w, h = min(TILE, WORLD_W - x0), min(TILE, WORLD_H - y0)
        human, martian, land = build(x0, y0, w, h, consts)
        hsd, msd = shaded(human, martian)
        os.makedirs(f"{OUT}/tiles", exist_ok=True); os.makedirs(f"{OUT}/thumbs", exist_ok=True)
        for side, img in (("human", hsd), ("martian", msd)):
            im = Image.fromarray(img)
            im.save(f"{OUT}/tiles/{side}-{col}-{row}.png", optimize=False, compress_level=6)
            im.resize((max(1, w // 16), max(1, h // 16)), Image.LANCZOS).save(f"{OUT}/thumbs/{side}-{col}-{row}.png")
        Image.fromarray((land[::16, ::16] * 255).astype(np.uint8)).save(f"{OUT}/thumbs/land-{col}-{row}.png")
        print(f"tile {col} {row} done: {w} x {h}, land {land.mean():.3f}")
