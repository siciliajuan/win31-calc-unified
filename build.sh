#!/bin/sh
# Builds CALC.EXE (Win16) with Open Watcom v2.
# Requires WATCOM, PATH and INCLUDE to be set (INCLUDE=$WATCOM/h:$WATCOM/h/win).
set -e
cd "$(dirname "$0")"
mkdir -p build
python3 tools/make_icon.py build/calc.ico
iconv -f UTF-8 -t CP1252 src/calc.rc.utf8 > build/calc.rc
cp src/resource.h build/

cd build
wcc -q -bt=windows -ms -fpc -ox -w4 -zW -i=../src ../src/engine.c -fo=engine.obj
wcc -q -bt=windows -ms -fpc -ox -w4 -zW -i=../src ../src/sci.c    -fo=sci.obj
wcc -q -bt=windows -ms -fpc -ox -w4 -zW -i=../src ../src/calc.c   -fo=calc.obj
wrc -q -r -bt=windows -i=. -i="$WATCOM/h/win" calc.rc -fo=calc.res
wlink op q system windows name CALC.EXE \
      op heapsize=4096 op stack=8192 \
      op description 'Calc95' \
      op modname=CALC95 \
      file calc.obj,engine.obj,sci.obj library shell
wrc -q -bt=windows calc.res CALC.EXE

# Installer and uninstaller (run from the floppy)
iconv -f UTF-8 -t CP1252 ../setup/setup.rc.utf8 > setup.rc
cp ../setup/setupres.h .
wcc -q -bt=windows -ms -fpc -ox -w4 -zW -i=../setup ../setup/setup.c -fo=setup.obj
wcc -q -bt=windows -ms -fpc -ox -w4 -zW -i=../setup -DUNINSTALL ../setup/setup.c -fo=uninst.obj
wrc -q -r -bt=windows -i=. -i="$WATCOM/h/win" setup.rc -fo=setup.res
wlink op q system windows name SETUP.EXE op heapsize=4096 op stack=8192 \
      op description 'Calc95Setup' file setup.obj library ddeml
wlink op q system windows name UNINSTAL.EXE op heapsize=4096 op stack=8192 \
      op description 'Calc95Uninstall' file uninst.obj library ddeml
wrc -q -bt=windows setup.res SETUP.EXE
wrc -q -bt=windows setup.res UNINSTAL.EXE
ls -l CALC.EXE SETUP.EXE UNINSTAL.EXE
