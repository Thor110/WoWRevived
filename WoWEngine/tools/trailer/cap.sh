#!/bin/bash
# cap.sh <name> <WxH> "<extra ini lines, \n separated>" "<engine args>" "<script>"
# Records every update of a scripted WoWEngine run (--frames) into $TRAILER_DIR/raw/<name>/ and encodes
# $TRAILER_DIR/clips/<name>.mp4 at 1920 x 1080, 30 fps. The install's WoWRevived.ini is put back afterwards.
#   WOW_GAME     the game folder (an English install: docs/TRAILER.md)
#   WOW_ENGINE   the WoWEngine program (a Release build)
#   TRAILER_DIR  where the clips go (default ./trailer-work)
T=${TRAILER_DIR:-$PWD/trailer-work}
G=${WOW_GAME:?set WOW_GAME to the game folder}
E=${WOW_ENGINE:?set WOW_ENGINE to the WoWEngine program}
name=$1; size=$2; extra=$3; args=$4; script=$5
mkdir -p $T/clips $T/logs
rm -rf $T/raw/$name
cp $G/WoWRevived.ini $T/ini.bak
printf '\n[Screen]\nSize=%s\n' "${size/x/,}" >> $G/WoWRevived.ini
[ -n "$extra" ] && printf "\n$extra\n" >> $G/WoWRevived.ini
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
timeout 1800 $E $G $args --frames $T/raw/$name --screenshot $T/raw/last-$name.bmp "$script" > $T/logs/$name.log 2>&1
rc=$?
cp $T/ini.bak $G/WoWRevived.ini
n=$(ls $T/raw/$name | wc -l)
# every frame to 1920 x 1080: a smaller one scaled up with whole pixels and bars, as a 1080p screen shows it
python3 -I - "$T/raw/$name" <<'PY'
import sys, os
from PIL import Image
d = sys.argv[1]
for f in sorted(os.listdir(d)):
    p = os.path.join(d, f)
    im = Image.open(p)
    if im.size == (1920, 1080):
        continue
    s = min(1920 / im.width, 1080 / im.height)
    big = im.resize((round(im.width * s), round(im.height * s)), Image.NEAREST)
    out = Image.new("RGB", (1920, 1080))
    out.paste(big, ((1920 - big.width) // 2, (1080 - big.height) // 2))
    out.save(p)
PY
ffmpeg -y -v error -framerate 30 -i $T/raw/$name/%06d.bmp -c:v libx264 -crf 16 -preset medium -pix_fmt yuv420p $T/clips/$name.mp4
echo "$name rc=$rc frames=$n $(ffprobe -v error -show_entries format=duration -of csv=p=0 $T/clips/$name.mp4)s"
rm -rf $T/raw/$name $T/raw/last-$name.bmp
