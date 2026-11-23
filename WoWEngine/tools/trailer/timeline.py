#!/usr/bin/env python3
"""The trailer's cut. python3 timeline.py [out.mp4] [segment numbers to preview...]"""
import sys
from compose import *

R = f"{T}/stills"
GAME = os.environ.get("WOW_GAME", "")
MUSIC = [f"{GAME}/Music-Human/05 Track05.wav",  # The Eve of the War
         f"{GAME}/Music-Human/04 Track04.wav"]

def card(text, size=72, extra=0.0, **kw):
    s = Seg(Card(), card=text, card_size=size, **kw)
    s.dur += extra
    return s

def cap(src, text, dur=None, **kw):
    s = Seg(src, caption=text, **kw)
    if dur and dur > s.dur:
        s.dur = dur
    return s

def wipe(a, b, dur, t0=2.4, caption=None, labels=("640 x 480", "1920 x 1080")):
    s = Seg(Wipe(a, b, t0), dur=None if caption else dur, caption=caption, labels=labels)
    s.dur = max(s.dur, dur)
    return s

SD_HD = ("640 x 480", "1920 x 1080")
BEFORE_AFTER = ("Original", "WoWRevived")

TL = [
    # 1. The original
    Seg(Clip("intro", 7.6), dur=7.5, fade=(0.8, 0.6)),
    card("In 1998, Rage Software brought Jeff Wayne's The War Of The Worlds to the PC."),
    cap(Clip("warmap-sd"), "It was made for 640 x 480 screens and Windows 95 and 98.", dur=6.5),
    cap(Clip("london-sd", 2.2), "On a modern PC it struggles to run at all.", dur=5.5),

    # 2. WoWRevived
    card("WoWRevived", size=150, sc=True, dur=3.5),
    cap(Still("launcher.png"), "A new launcher, with every setting in one place and fixes for modern Windows"),
    Seg(Wipe(Still("menu-background-original.png", 1.0, 1.0), Still("menu-background-human.png", 1.0, 1.0), 2.4),
        caption="Menus extended for every resolution", labels=BEFORE_AFTER, dur=6.5),
    Seg(Wipe(Still("custom-human-briefing-original.png", 1.0, 1.0), Still("custom-human-briefing.png", 1.0, 1.0), 2.4),
        caption="Briefing, research and CD player screens redrawn for widescreen", labels=BEFORE_AFTER, dur=7),
    Seg(Wipe(Still("custom-martian-research-original.png", 1.0, 1.0), Still("custom-martian-research.png", 1.0, 1.0), 1.6),
        labels=BEFORE_AFTER, dur=4.5),
    Seg(Wipe(Still("custom-cd-player-original.png", 1.0, 1.0), Still("custom-cd-player.png", 1.0, 1.0), 1.6),
        labels=BEFORE_AFTER, dur=4.5),
    cap(Still("custom-martian-warmap.png", 1.0, 1.08), "A wider war map, with the coast of Europe on the Martian side"),
    cap(Gif("human-comparison.gif", 7), "Enhanced assets: reworked icons and interface sprites", dur=6),
    Seg(Gif("martian-comparison.gif", 7), dur=4),
    cap(Still("building-factory-smokestack-ordering.png"), "Fixes for the game's own files: the Cotton Factory's smokestacks"),
    cap(Still("airship-stretching-fixed.png"), "The Airship model that tore itself apart when it turned"),
    Seg(Wipe(Still("south-wales-terrain.png", 1.0, 1.0), Still("south-wales-terrain-fixed.png", 1.0, 1.0), 2.4),
        caption="The hole in Sector 22's terrain", labels=BEFORE_AFTER, dur=6),
    cap(Still("building-martian-pixels-restored.png", nearest=True), "Missing pixels restored to the building sprites"),
    Seg(Wipe(Still("menu-settings-unedited.png", 1.0, 1.0, nearest=True), Still("menu-settings-edited.png", 1.0, 1.0, nearest=True), 2.4),
        caption="Settings that broke or crashed the game taken out", labels=BEFORE_AFTER, dur=6.5),
    cap(Still("wrapper.png"), "1080p upscales of every movie"),
    cap(Still("discord-rich-presence.png"), "Discord rich presence"),
    cap(Grid(["sprite-viewer.png", "unit-viewer.png", "terrain-viewer.png", "text-editor.png"]),
        "And a modding toolkit for the game's archives, sprites, models, terrain, fonts and text"),
    card("But the patches could only go so far. Underneath, the engine was still from 1998."),

    # 3. WoWEngine
    card("So the engine has been rebuilt."),
    Seg(Clip("intro", 19.0), card="WoWEngine", card_size=170, sc=True, dur=6, dim=0.35, fade=(0.8, 0.5)),
    card("Written routine by routine from the original executable, to play exactly like it."),
    card("Windows. macOS. Linux.\nNative 64-bit, with no wrappers or compatibility settings."),
    cap(Still(f"{R}/launch-inst.png"), "Install from your original discs or disc images. The music is ripped too."),
    Seg(Still(f"{R}/install-1.png"), dur=3, fade=(0.2, 0.2)),
    Seg(Still(f"{R}/launch-main.png"), dur=3, fade=(0.2, 0.4)),
    card("No game data included: it runs on the files from your original discs."),

    # 4. 640 x 480 against 1920 x 1080
    card("From 640 x 480 to 1920 x 1080"),
    wipe(Clip("warmap-sd"), Clip("warmap-hd"), 7),
    wipe(Clip("mwarmap-sd"), Clip("mwarmap-hd"), 5.6, t0=1.8),
    wipe(Clip("london-sd", 2.2), Clip("london-hd", 2.2), 6.3),
    wipe(Clip("menu-sd"), Clip("menu-hd"), 4.3, t0=1.4),
    Seg(Steps([f"{R}/res-{r}.png" for r in ("640x480", "800x600", "1024x768", "1280x768", "1366x768", "1600x900", "1920x1080")], 1.2),
        caption="Change the resolution at any time, without restarting the game", dur=9,
        labels=lambda t: (["640 x 480", "800 x 600", "1024 x 768", "1280 x 768", "1366 x 768", "1600 x 900", "1920 x 1080"][min(6, int(t / 1.2))], None)),
    cap(Clip("research", 1.8), "Extended backgrounds for every screen, chosen for your resolution", dur=7),
    Seg(Clip("mresearch", 1.8), dur=4),
    Seg(Clip("cdplayer", 0.5), dur=4.5),

    # 5. Modern Options
    cap(Clip("modern-sd"), "Every new feature is an option. The original game stays the default.", dur=10),
    cap(Clip("zoom", 3.4), "Battle zoom: + and -, or the mouse wheel", dur=11.5),
    Seg(Wipe(Clip("warmap-hd"), Clip("hud"), 2.0), caption="Scale the panels of both maps from 50% to 300%",
        labels=("HUD 100%", "HUD 200%"), dur=6.5),
    cap(Clip("groups", 3.4), "Group keys: Ctrl and a number keeps a group, Shift adds to the selection", dur=10),
    cap(Clip("popup", 0.3, crop=(632, 300, 960, 540)), "Popup menus that stay open", dur=7),
    cap(Clip("strip", 0.5, crop=(0, 540, 960, 540)), "A status strip on the war map, naming what is under the mouse", dur=8),
    cap(Clip("saves-sd"), "More save slots: pages of five, up to 999", dur=5.5),
    card("Play in a window or full screen, and rebind the keys of both maps."),
    card("Record your input, and play it back."),
    card("Upgraded buildings fight at their upgraded level, and new campaigns never overwrite the start files."),

    # 6. Everything from WoWRevived
    card("Every WoWRevived feature and file fix is built in."),
    Seg(Wipe(Clip("enh-off", 0.3, crop=(1040, 0, 640, 360)), Clip("enh-on", 0.3, crop=(1040, 0, 640, 360)), 1.2),
        caption="Enhanced assets, as an option", labels=("Original", "Enhanced assets"), dur=5),
    cap(Grid(["building-factory-smokestack-ordering.png", "airship-stretching-fixed.png", "south-wales-terrain-fixed.png",
              "custom-human-briefing.png"]),
        "The fixed game files, the extended backgrounds and the updated text in five languages"),
    cap(Clip("intro", 19.0), "WoWRevived's 1080p movie upscales play when they are installed", dur=6.5),
    cap(Still("discord-rich-presence.png"), "Volume settings that are saved, music that keeps playing, and Discord"),

    # 7. The war
    Seg(Clip("pod-zoom", 23.0, crop=(720, 280, 1200, 675)), dur=14, fade=(0.6, 0.8)),
    Seg(Clip("london-hd", 3.0), dur=4, fade=(0.5, 0.8)),

    # 8. End
    Seg(Clip("credits", 4.0), card="WoWEngine", card_size=170, sc=True, dur=5, dim=0.5, fade=(0.8, 0.4)),
    card("Jeff Wayne's The War Of The Worlds, rebuilt for today."),
    card("Requires the original Human and Martian discs."),
    card("github.com/Thor110/WoWRevived\n\nDiscord: discord.gg/bwG6Z3RK8b", size=64, dur=8, fade=(0.4, 1.5)),
]

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else f"{T}/WoWRevived-trailer.mp4"
    preview = set(int(a) for a in sys.argv[2:]) or None
    render(TL, out, MUSIC, preview)
