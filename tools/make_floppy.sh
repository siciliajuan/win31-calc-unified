#!/bin/sh
# Builds the 1.44 MB setup floppy image and a zip with the same files.
# Requires mtools and zip. Run after build.sh.
set -e
cd "$(dirname "$0")/.."
OUT=build/dist
rm -rf "$OUT" && mkdir -p "$OUT/disk"
cp build/SETUP.EXE build/UNINSTAL.EXE "$OUT/disk/"
cp build/CALC.EXE "$OUT/disk/CALC95.EXE"

IMG="$OUT/CALC95.IMG"
mformat -C -f 1440 -v CALC95 -i "$IMG" ::
mcopy -i "$IMG" "$OUT/disk/SETUP.EXE" "$OUT/disk/UNINSTAL.EXE" "$OUT/disk/CALC95.EXE" ::
mdir -i "$IMG" ::

(cd "$OUT/disk" && zip -q -9 ../calc95-disk.zip SETUP.EXE UNINSTAL.EXE CALC95.EXE)
ls -l "$OUT"
