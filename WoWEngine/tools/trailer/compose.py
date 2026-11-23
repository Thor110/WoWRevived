#!/usr/bin/env python3
"""The trailer's compositor: builds every frame from the timeline (timeline.py) and streams them to ffmpeg.

Sources draw a 1920 x 1080 picture for frame i of their segment; segments add captions, labels, titles and fades.
"""
import math, os, subprocess, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageSequence

W, H, FPS = 1920, 1080, 30
T = os.environ.get("TRAILER_DIR", os.path.join(os.getcwd(), "trailer-work"))
# the repository's images folder (the WoWRevived screenshots)
IMG = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "images"))
# IM Fell English (SIL Open Font License), from github.com/google/fonts ofl/imfellenglish and ofl/imfellenglishsc
FONT = f"{T}/fonts/IMFeENrm28P.ttf"
FONT_SC = f"{T}/fonts/IMFeENsc28P.ttf"
CREAM = (236, 226, 200)

def font(size, sc=False):
    return ImageFont.truetype(FONT_SC if sc else FONT, size)

def read_time(text):
    """1.5 s and 0.4 s a word, never under 3 s, to the half second above"""
    words = len(text.replace("\n", " ").split())
    return max(3.0, math.ceil((1.5 + 0.4 * words) * 2) / 2)

# ---------------------------------------------------------------- sources

class Black:
    def frame(self, i, n):
        return Image.new("RGB", (W, H))

def mars_background():
    """a dark red glow in the middle, near black at the edges: the cards' background"""
    small = Image.new("RGB", (192, 108))
    px = small.load()
    for y in range(108):
        for x in range(192):
            d = math.hypot((x - 96) / 96, (y - 54) / 70)
            k = max(0.0, 1 - d) ** 1.6
            px[x, y] = (int(8 + 44 * k), int(5 + 14 * k), int(4 + 8 * k))
    return small.resize((W, H), Image.BICUBIC).filter(ImageFilter.GaussianBlur(6))

class Card:
    bg = None
    def frame(self, i, n):
        if Card.bg is None:
            Card.bg = mars_background()
        return Card.bg.copy()

class Clip:
    """an encoded clip from clips/, read in order from start seconds; it holds its last frame if it runs out"""
    def __init__(self, name, start=0.0, speed=1.0, crop=None):
        self.path = f"{T}/clips/{name}.mp4" if not name.startswith("/") else name
        self.start, self.speed, self.proc, self.last = start, speed, None, None
        self.crop = crop  # (x, y, w, h) of the 1920 x 1080 clip, scaled up to fill
    def frame(self, i, n):
        if self.proc is None:
            vf = f"setpts=PTS/{self.speed}," if self.speed != 1.0 else ""
            if self.crop:
                x, y, w, h = self.crop
                vf += f"crop={w}:{h}:{x}:{y},"
            self.proc = subprocess.Popen(
                ["ffmpeg", "-v", "error", "-ss", str(self.start), "-i", self.path,
                 "-vf", f"{vf}scale={W}:{H}:flags=lanczos,fps={FPS}", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
                stdout=subprocess.PIPE)
        data = self.proc.stdout.read(W * H * 3)
        if len(data) == W * H * 3:
            self.last = Image.frombytes("RGB", (W, H), data)
        if i == n - 1:
            self.proc.kill()
        return self.last.copy() if self.last else Image.new("RGB", (W, H))

def fit(im, w, h, method=Image.LANCZOS):
    s = min(w / im.width, h / im.height)
    return im.resize((max(1, round(im.width * s)), max(1, round(im.height * s))), method)

def backdrop(im):
    """a picture smaller than the screen sits on a blurred, darkened copy of itself"""
    s = max(W / im.width, H / im.height)
    big = im.convert("RGB").resize((round(im.width * s), round(im.height * s)), Image.BILINEAR)
    big = big.crop(((big.width - W) // 2, (big.height - H) // 2, (big.width - W) // 2 + W, (big.height - H) // 2 + H))
    big = big.filter(ImageFilter.GaussianBlur(30))
    return Image.blend(Image.new("RGB", (W, H)), big, 0.35)

class Still:
    """a picture, fitted inside a margin, slowly pushed in (Ken Burns) from z0 to z1"""
    def __init__(self, path, z0=1.0, z1=1.06, margin=0.86, nearest=False, pan=(0, 0)):
        self.im = Image.open(path if path.startswith("/") else f"{IMG}/{path}").convert("RGB")
        full = self.im.size == (W, H)
        self.margin = 1.0 if full else margin
        self.z0, self.z1, self.nearest, self.pan = z0, z1, nearest, pan
        self.base = None if full else backdrop(self.im)
        self.fitted = fit(self.im, W * self.margin, H * self.margin, Image.NEAREST if nearest else Image.LANCZOS)
    def frame(self, i, n):
        t = i / max(1, n - 1)
        z = self.z0 + (self.z1 - self.z0) * t
        if self.base is None:  # a full screen picture: crop in
            cw, ch = W / z, H / z
            cx = W / 2 + self.pan[0] * t * (W - cw) / 2
            cy = H / 2 + self.pan[1] * t * (H - ch) / 2
            return self.im.resize((W, H), Image.BICUBIC, box=(cx - cw / 2, cy - ch / 2, cx + cw / 2, cy + ch / 2))
        out = self.base.copy()
        f = self.fitted
        im = f.resize((round(f.width * z), round(f.height * z)), Image.NEAREST if self.nearest else Image.BICUBIC)
        out.paste(im, ((W - im.width) // 2, (H - im.height) // 2))
        return out

class Gif:
    """an animated GIF, scaled up with whole pixels, played at its own rate"""
    def __init__(self, path, scale=6):
        g = Image.open(f"{IMG}/{path}")
        self.frames, self.times = [], []
        t = 0
        for fr in ImageSequence.Iterator(g):
            im = fr.convert("RGB")
            self.frames.append(im.resize((im.width * scale, im.height * scale), Image.NEAREST))
            t += (fr.info.get("duration", 100) or 100) / 1000
            self.times.append(t)
        self.total = t
        self.base = backdrop(self.frames[0])
    def frame(self, i, n):
        t = (i / FPS) % self.total
        k = next(j for j, e in enumerate(self.times) if t < e)
        out = self.base.copy()
        f = self.frames[k]
        out.paste(f, ((W - f.width) // 2, (H - f.height) // 2))
        return out

class Grid:
    """several pictures side by side, each fitted in its cell"""
    def __init__(self, paths, cols=2, gap=30):
        ims = [Image.open(f"{IMG}/{p}").convert("RGB") for p in paths]
        rows = math.ceil(len(ims) / cols)
        cw, ch = (W - gap * (cols + 1)) / cols, (H - 180 - gap * (rows + 1)) / rows
        self.out = backdrop(ims[0])
        for k, im in enumerate(ims):
            f = fit(im, cw, ch)
            cx = gap + (k % cols) * (cw + gap) + (cw - f.width) / 2
            cy = gap + (k // cols) * (ch + gap) + (ch - f.height) / 2
            self.out.paste(f, (round(cx), round(cy)))
    def frame(self, i, n):
        return self.out.copy()

class Wipe:
    """a before and after: a until t0, then a line moving left to right uncovers b over tw seconds"""
    def __init__(self, a, b, t0, tw=1.4):
        self.a, self.b, self.t0, self.tw = a, b, t0, tw
    def frame(self, i, n):
        fa, fb = self.a.frame(i, n), self.b.frame(i, n)
        t = i / FPS
        if t <= self.t0:
            return fa
        k = min(1.0, (t - self.t0) / self.tw)
        k = k * k * (3 - 2 * k)
        x = round(W * (1 - k))  # b comes in from the right
        if x <= 0:
            return fb
        fa.paste(fb.crop((x, 0, W, H)), (x, 0))
        ImageDraw.Draw(fa).rectangle((x - 2, 0, x + 2, H), fill=CREAM)
        return fa

class Steps:
    """pictures one after another, each for step seconds (the last holds)"""
    def __init__(self, paths, step):
        self.stills = [Still(p, 1.0, 1.0, margin=1.0, nearest=True) for p in paths]
        self.step = step
    def frame(self, i, n):
        k = min(len(self.stills) - 1, int(i / FPS / self.step))
        return self.stills[k].frame(0, 1)

# ---------------------------------------------------------------- overlays

def text_block(text, size, sc=False, width=1500, spacing=14):
    f = font(size, sc)
    lines = []
    for para in text.split("\n"):
        cur = ""
        for word in para.split():
            trial = (cur + " " + word).strip()
            if f.getlength(trial) <= width or not cur:
                cur = trial
            else:
                lines.append(cur); cur = word
        lines.append(cur)
    asc, desc = f.getmetrics()
    lh = asc + desc + spacing
    w = max(f.getlength(l) for l in lines)
    im = Image.new("RGBA", (int(w) + 40, lh * len(lines) + 20))
    d = ImageDraw.Draw(im)
    for k, l in enumerate(lines):
        d.text(((im.width - f.getlength(l)) / 2, 10 + k * lh), l, font=f, fill=CREAM + (255,))
    return im

def shadowed(im, radius=6):
    sh = Image.new("RGBA", im.size)
    sh.putalpha(im.getchannel("A").filter(ImageFilter.GaussianBlur(radius)))
    out = Image.new("RGBA", im.size)
    out.alpha_composite(sh)
    out.alpha_composite(sh)
    out.alpha_composite(im)
    return out

def caption_layer(text):
    """the lower third: a dark band with the caption"""
    tb = text_block(text, 56, width=1600)
    band_h = tb.height + 50
    layer = Image.new("RGBA", (W, H))
    band = Image.new("RGBA", (W, band_h), (0, 0, 0, 170))
    layer.alpha_composite(band, (0, H - band_h - 60))
    layer.alpha_composite(tb, ((W - tb.width) // 2, H - band_h - 60 + 25))
    return layer

def card_layer(text, size=72, sc=False):
    tb = shadowed(text_block(text, size, sc, width=1500))
    layer = Image.new("RGBA", (W, H))
    layer.alpha_composite(tb, ((W - tb.width) // 2, (H - tb.height) // 2))
    return layer

def labels_layer(left, right=None):
    layer = Image.new("RGBA", (W, H))
    f = font(46)
    for k, text in enumerate((left, right)):
        if not text:
            continue
        w = f.getlength(text)
        box = Image.new("RGBA", (int(w) + 50, 74), (0, 0, 0, 175))
        ImageDraw.Draw(box).text((25, 10), text, font=f, fill=CREAM + (255,))
        layer.alpha_composite(box, (40, 40) if k == 0 else (W - box.width - 40, 40))
    return layer

def faded(layer, a):
    if a >= 1:
        return layer
    l = layer.copy()
    l.putalpha(l.getchannel("A").point(lambda v: int(v * a)))
    return l

# ---------------------------------------------------------------- segments

class Seg:
    """dur seconds of src. caption: lower third text; card: middle text (on its own or over the picture);
    labels: (left, right) or a function of time giving them; fade: seconds from and to black at the ends"""
    def __init__(self, src, dur=None, caption=None, card=None, card_size=72, sc=False, labels=None,
                 fade=(0.4, 0.4), text_delay=0.3, dim=0.0):
        self.src, self.caption, self.card, self.labels = src, caption, card, labels
        text = caption or card
        need = read_time(text) + text_delay + 0.4 if text else 0
        self.dur = max(dur or 0, need) if (dur or text) else 3.0
        self.fade, self.text_delay, self.dim = fade, text_delay, dim
        self.cap = caption_layer(caption) if caption else None
        self.cardl = card_layer(card, card_size, sc) if card else None

    def frames(self):
        n = round(self.dur * FPS)
        lab_cache = {}
        for i in range(n):
            t = i / FPS
            im = self.src.frame(i, n).convert("RGBA")
            if self.dim:
                im = Image.blend(im, Image.new("RGBA", (W, H), (0, 0, 0, 255)), self.dim)
            # text fades in after text_delay and out before the end
            a = min(1.0, max(0.0, (t - self.text_delay) / 0.5), max(0.0, (self.dur - 0.35 - t) / 0.5))
            if self.cap:
                im.alpha_composite(faded(self.cap, a))
            if self.cardl:
                im.alpha_composite(faded(self.cardl, a))
            if self.labels:
                lab = self.labels(t) if callable(self.labels) else self.labels
                if lab not in lab_cache:
                    lab_cache[lab] = labels_layer(*lab)
                im.alpha_composite(faded(lab_cache[lab], min(1.0, t / 0.4, (self.dur - t) / 0.4)))
            im = im.convert("RGB")
            fi, fo = self.fade
            k = 1.0
            if fi and t < fi: k = t / fi
            if fo and t > self.dur - fo: k = min(k, (self.dur - t) / fo)
            if k < 1:
                im = Image.blend(Image.new("RGB", (W, H)), im, max(0.0, k))
            yield im

def render(timeline, out, music, preview=None):
    total = sum(s.dur for s in timeline)
    print(f"{len(timeline)} segments, {total:.1f} s", flush=True)
    vid = out + ".video.mp4"
    enc = subprocess.Popen(["ffmpeg", "-y", "-v", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}",
                            "-r", str(FPS), "-i", "-", "-c:v", "libx264", "-crf", "18", "-preset", "medium",
                            "-pix_fmt", "yuv420p", vid], stdin=subprocess.PIPE)
    t = 0.0
    for k, s in enumerate(timeline):
        if preview is not None and k not in preview:
            t += s.dur
            continue
        print(f"{t:7.1f}s  {k:3d}  {(s.caption or s.card or '').splitlines()[0][:70] if (s.caption or s.card) else ''}", flush=True)
        for im in s.frames():
            enc.stdin.write(im.tobytes())
        t += s.dur
    enc.stdin.close()
    enc.wait()
    dur = float(subprocess.check_output(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", vid]))
    # the music under it all, faded out over the last six seconds
    musics = music if isinstance(music, list) else [music]
    ins = sum((["-i", m] for m in musics), [])
    chain = "[1:a]"
    pre = ""
    for k in range(2, len(musics) + 1):
        pre += f"{chain}[{k}:a]acrossfade=d=6:c1=tri:c2=tri[x{k}];"
        chain = f"[x{k}]"
    subprocess.run(["ffmpeg", "-y", "-v", "error", "-i", vid] + ins + ["-filter_complex",
                    pre + f"{chain}atrim=0:{dur},afade=t=in:d=0.5,afade=t=out:st={max(0, dur - 6)}:d=6,volume=0.9[a]",
                    "-map", "0:v", "-map", "[a]", "-c:v", "copy", "-c:a", "aac", "-b:a", "256k", "-shortest", out], check=True)
    os.remove(vid)
    print("wrote", out, f"{dur:.1f} s")
