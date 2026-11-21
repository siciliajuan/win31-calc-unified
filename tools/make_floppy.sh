#!/bin/sh
# Builds the 1.44 MB setup floppy image and a zip with the same files.
# Requires mtools and zip. Run after build.sh.
set -e
cd "$(dirname "$0")/.."
OUT=build/dist
rm -rf "$OUT" && mkdir -p "$OUT/disk"
cp build/SETUP.EXE build/UNINSTAL.EXE "$OUT/disk/"
cp build/CALC.EXE "$OUT/disk/CALCNEW.EXE"

VERSION=${VERSION:-2.0}
IMG="$OUT/win31-calc-unified-$VERSION.img"
mformat -C -f 1440 -v CALCUNIFIED -i "$IMG" ::
mcopy -i "$IMG" "$OUT/disk/SETUP.EXE" "$OUT/disk/UNINSTAL.EXE" "$OUT/disk/CALCNEW.EXE" ::
mdir -i "$IMG" ::

(cd "$OUT/disk" && zip -q -9 "../win31-calc-unified-$VERSION.zip" SETUP.EXE UNINSTAL.EXE CALCNEW.EXE)
ls -l "$OUT"
