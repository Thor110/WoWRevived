#!/bin/bash
# stills.sh: the trailer's still pictures, into $TRAILER_DIR/stills:
#   res-<size>.png   the Human war map at seven view sizes (the resolution ladder)
#   launch-*.png     WoWLauncher's launcher and install screens; install-1.png an install a second in
#   WOW_GAME, WOW_ENGINE as for cap.sh; WOW_LAUNCHER the WoWLauncher program (a Debug build has it);
#   WOW_DISC (optional) a disc image for the install picture
T=${TRAILER_DIR:-$PWD/trailer-work}
G=${WOW_GAME:?set WOW_GAME}
E=${WOW_ENGINE:?set WOW_ENGINE}
L=${WOW_LAUNCHER:-$(dirname $E)/WoWLauncher}
mkdir -p $T/stills
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
cp $G/WoWRevived.ini $T/ini.bak
for r in 640x480 800x600 1024x768 1280x768 1366x768 1600x900 1920x1080; do
    cp $T/ini.bak $G/WoWRevived.ini
    printf '\n[Screen]\nSize=%s\n' "${r/x/,}" >> $G/WoWRevived.ini
    $E $G --human --open 0x8A --screenshot $T/stills/res-$r.bmp "wait:30" > /dev/null 2>&1
done
cp $T/ini.bak $G/WoWRevived.ini
$L --target $G --screenshot $T/stills/launch-main.bmp 2 > /dev/null 2>&1
$L --target $G --install --screenshot $T/stills/launch-inst.bmp 2 > /dev/null 2>&1
if [ -n "$WOW_DISC" ]; then
    tmp=$(mktemp -d)
    $L --target $tmp/game --install --image "$WOW_DISC" --start --screenshot $T/stills/install-1.bmp 1 > /dev/null 2>&1
    rm -rf $tmp
fi
cp $T/ini.bak $G/WoWRevived.ini
for f in $T/stills/*.bmp; do convert "$f" "${f%.bmp}.png" && rm "$f"; done
ls $T/stills
