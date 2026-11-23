#!/usr/bin/env python3
"""The trailer's engine shots. Usage: shots.py name [name...] (or 'all'). Each runs cap.sh."""
import os, subprocess, sys

def glide(x0, y0, x1, y1, n):
    """the mouse moved from one point to another over n updates, eased"""
    out = []
    for i in range(1, n + 1):
        t = i / n
        t = t * t * (3 - 2 * t)
        out.append(f"move:{round(x0 + (x1 - x0) * t)}:{round(y0 + (y1 - y0) * t)}")
        out.append("wait:1")
    return out

def path(points, n):
    out = []
    for (a, b), (c, d) in zip(points, points[1:]):
        out += glide(a, b, c, d, n)
    return out

def s(*parts):
    flat = []
    for p in parts:
        flat += p if isinstance(p, list) else [p]
    return ",".join(flat)

HD = "1920x1080"
SD = "640x480"
# the London battle from a new Human campaign: the war map, London's popup, Attack, the briefing, the battle
LONDON = ["wait:10", "click:300:250", "wait:5", "rdown:300:250", "wait:20", "click:300:292"]
# a new Martian campaign: the clock to speed 8, the pod's box, Enter for Attack, the briefing dismissed
POD = ["wait:30", "8", "wait:450", "enter", "wait:200", "click:320:240"]
LORRIES = (985, 605)

SHOTS = {
    # 640 x 480 against 1920 x 1080
    "warmap-sd": (SD, "", "--no-loose --human --open 0x8A",
                  s("wait:20", path([(320, 240), (150, 150), (330, 235), (450, 330)], 50), "wait:40")),
    "warmap-hd": (HD, "", "--human --open 0x8A",
                  s("wait:20", path([(960, 540), (450, 340), (990, 530), (1350, 740)], 50), "wait:40")),
    "london-sd": (SD, "", "--no-loose --human --open 0x8A",
                  s(LONDON, "wait:120", glide(320, 240, 330, 300, 40), "wait:60")),
    "london-hd": (HD, "", "--human --open 0x8A",
                  s(LONDON, "wait:120", glide(960, 540, *LORRIES, 40), "wait:60")),
    "mwarmap-sd": (SD, "", "--no-loose --martian --open 0x8A",
                   s("wait:20", path([(320, 240), (200, 160), (420, 300)], 60), "wait:30")),
    "mwarmap-hd": (HD, "", "--martian --open 0x8A",
                   s("wait:20", path([(960, 540), (600, 360), (1500, 700)], 60), "wait:30")),
    # resolution changed live from Display Settings
    "resolution": (SD, "", "--open 0x3EF",
                   s("wait:30", glide(320, 400, 170, 187, 30), "wait:10", "down:170:187", "wait:5",
                     [f"move:{x}:187,wait:12" for x in range(180, 500, 10)], "up:490:187", "wait:60")),
    # Modern Options
    "modern": (HD, "", "--open 0x9001",
               s("wait:20", glide(960, 900, 800, 420, 30),
                 path([(800, 420), (800, 646), (1080, 646), (1080, 420), (960, 700)], 70), "wait:30")),
    "zoom": (HD, "", "--human --open 0x8A",
             s(LONDON, "wait:100", glide(960, 540, *LORRIES, 30), "wait:20",
               ["plus,wait:25"] * 4, "wait:40", ["minus,wait:25"] * 2, "wait:30", "=r", "wait:50")),
    "hud": (HD, "[Modern]\\nHUD scale=200", "--human --open 0x8A",
            s("wait:20", path([(960, 540), (700, 400), (1200, 600)], 60), "wait:30")),
    "strip": (HD, "[Modern]\\nWar map status strip=1", "--human --open 0x8A",
              s("wait:20", path([(300, 600), (600, 500), (900, 400), (1100, 560), (900, 750), (600, 800)], 45), "wait:30")),
    "popup": (HD, "[Modern]\\nSticky popup menus=1", "--human --open 0x8A",
              s(LONDON, "wait:100", glide(960, 540, *LORRIES, 30), "wait:10", f"rdown:{LORRIES[0]}:{LORRIES[1]}", "wait:3",
                f"rup:{LORRIES[0]}:{LORRIES[1]}", "wait:30", glide(*LORRIES, LORRIES[0] + 40, LORRIES[1] + 90, 50), "wait:60")),
    "groups": (HD, "", "--human --open 0x8A",
               s(LONDON, "wait:100", glide(960, 540, 920, 550, 25), "down:920:550", glide(920, 550, 1050, 660, 25), "up:1050:660",
                 "wait:20", "ctrl", "1", "nomods", "wait:20", glide(1050, 660, 1300, 800, 25), "click:1300:800", "wait:60",
                 "1", "wait:30", "1", "wait:60")),
    "saves": (HD, "", "--open 0x6F",
              s("wait:30", glide(960, 900, 960, 632, 40), "wait:20", "click:960:632", "wait:60")),
    "pod": (HD, "", "--martian --open 0x8A", s(POD, "wait:700")),
    "research": (HD, "", "--human --open 0x8A", s("wait:20", "=r", "wait:40", glide(960, 540, 300, 200, 60), "wait:60")),
    "mresearch": (HD, "", "--martian --open 0x8A", s("wait:20", "=r", "wait:40", glide(960, 540, 300, 200, 60), "wait:60")),
    "cdplayer": (HD, "", "--open 0x803E", s("wait:20", path([(400, 400), (95, 65), (95, 160)], 50), "wait:40")),
    "menu-hd": (HD, "", "--open 0x90", s("wait:20", path([(960, 800), (960, 470), (960, 510)], 40), "wait:30")),
    "menu-sd": (SD, "", "--no-loose --open 0x90", s("wait:20", path([(320, 400), (320, 168), (320, 210)], 40), "wait:30")),
    "credits": (HD, "", "--open 0x80CA", "wait:900"),
    "enh-off": (SD, "", "--human --open 0x8A", "wait:40"),
    "enh-on": (SD, "[Modern]\\nEnhanced assets=1", "--human --open 0x8A", "wait:40"),
}

SHOTS.update({
    "popup": (HD, "[Modern]\\nSticky popup menus=1", "--human --open 0x8A",
              s("wait:20", glide(960, 540, 1112, 553, 30), "click:1112:553", "wait:10", "rdown:1112:553", "wait:20",
                "rup:1112:553", "wait:30", glide(1112, 553, 1120, 580, 25), "wait:20", glide(1120, 580, 1120, 610, 25), "wait:40")),
    "modern-sd": (SD, "", "--open 0x9001",
                  s("wait:20", glide(320, 460, 160, 120, 30),
                    path([(160, 120), (160, 346), (440, 346), (440, 120), (320, 410)], 70), "wait:30")),
    "saves-sd": (SD, "", "--open 0x6F",
                 s("wait:30", glide(320, 450, 320, 332, 40), "wait:20", "click:320:332", "wait:75")),
    "pod-zoom": (HD, "", "--martian --open 0x8A",
                 s(POD, "wait:30", "move:2:540", "wait:22", "move:960:540", "plus", "wait:4", "plus", "wait:600")),
})

def make_saves():
    """twelve named saves for the Load menu's pages (copies of the Human start file, the name at 0x0C), if the
    install has no SaveGame folder: a test install's, never a player's"""
    game = os.environ["WOW_GAME"]
    folder = os.path.join(game, "SaveGame")
    if os.path.isdir(folder):
        return
    os.makedirs(folder)
    names = ["The Defence of London", "Holding the Thames", "Woking Burns", "The Road to Leatherhead",
             "Ironclads at Dover", "The Last Stand at Oxford", "Retaking Kent", "Thunder Child",
             "Black Smoke over Surrey", "The Fall of Birmingham", "Cardiff Reinforced", "Edinburgh Holds"]
    start = open(os.path.join(game, "Human.000"), "rb").read()
    for k, name in enumerate(names, 1):
        b = bytearray(start)
        nb = name.encode("latin-1")[:35]
        b[0x0C:0x0C + 36] = nb + bytes(36 - len(nb))
        open(os.path.join(folder, f"Human.{k:03d}"), "wb").write(b)

names = sys.argv[1:]
if names == ["all"]:
    names = list(SHOTS)
for n in names:
    if n.startswith("saves"):
        make_saves()
    size, extra, args, script = SHOTS[n]
    subprocess.run([os.path.join(os.path.dirname(os.path.abspath(__file__)), "cap.sh"), n, size, extra, args, script])
