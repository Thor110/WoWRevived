#!/bin/bash
# review.sh name [fps]: a contact sheet of clips/<name>.mp4 at one frame every 1/fps seconds
f=${2:-1}
ffmpeg -y -v error -i ${TRAILER_DIR:-$PWD/trailer-work}/clips/$1.mp4 -vf "fps=$f,scale=320:-1,tile=6x4:padding=4" -frames:v 1 ${TRAILER_DIR:-$PWD/trailer-work}/sheet-$1.png
