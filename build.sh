#!/bin/bash
# Builds PsiPDF for EPOC R5 (the Psion netBook and Series 5mx) and packages
# dist/PsiPDF.sis. In the psion-build container (psion-core's tools/docker):
#   PSION_WORK=~/www/psion core/tools/docker/psibuild "bash build.sh"
# then, on the host (needs Python's PIL), the icons and the package:
#   python3 pkg/mkicon.py build/pkg core && PSION_WORK=~/www/psion core/tools/docker/psibuild "bash build.sh sis"
# (build.sh all does both steps in the container when it has PIL.)
#
#   psipdf.exe   the engine: MuPDF 1.3, fetched by ./fetch.sh at a pinned
#                checksum into third/ (Makefile)
#   PsiPDF.fnt the 14 standard PDF fonts, read by the engine when a PDF
#                needs one (build/gen, made with the engine)
#   PsiPDF.app   the program (app/), with its .rsc
#   PsiPDF.mdl   the recogniser: PDF files open in PsiPDF (rec/)
#
# Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
set -e
: "${PSION_SDK:?set PSION_SDK to your psion_cpp_sdk_linux directory}"
HERE=$(cd "$(dirname "$0")" && pwd)
CORE=${PSION_CORE:-$HERE/core}
[ -f "$CORE/ssh/psibell.h" ] || { echo "psion-core is not here: git submodule update --init (or set PSION_CORE)"; exit 1; }
export EPOCROOT=$PSION_SDK/epoc_cpp_sdk
REL=$EPOCROOT/epoc32/release/marm/rel
B=$HERE/build
mkdir -p "$B" "$HERE/dist"
export PATH=/usr/bin:/bin:$PSION_SDK/gcc-3.0-psion-98r2-9/bin:$EPOCROOT/epoc32/tools
export TMP=$EPOCROOT/tmp WINEDEBUG=-all
mkdir -p "$TMP" "$EPOCROOT/ptproj"
link() {
	if [ -e "$EPOCROOT/ptproj/$1" ] && [ ! -L "$EPOCROOT/ptproj/$1" ]; then
		echo "$EPOCROOT/ptproj/$1 exists and is not a link - move it away"; exit 1
	fi
	ln -sfn "$2" "$EPOCROOT/ptproj/$1"
}
mmp() {   # project folder, mmp name, built file, log name
	cd "$EPOCROOT/ptproj/$1"
	makmake "$2" marm > "$B/$4-makmake.log" 2>&1
	make -f "$2.marm" rel > "$B/$4.log" 2>&1 \
		|| { grep -A3 "\.c:[0-9]*:\|\.cpp:[0-9]*:\|rror\|undefined\|No rule" "$B/$4.log" | grep -v "^--" | head -60; exit 1; }
	rm -f "$2.marm"
	# (the netBook's SA-1100 has no BX: core/tools/v4bx.py)
	python3 "$CORE/tools/v4bx.py" --check "$REL/$3"
}

if [ "$1" = sis ]; then
	P=$B/pkg
	mkdir -p "$P"
	cp "$HERE/pkg/psipdf.pkg" "$HERE/LICENSE" "$HERE/NOTICE" "$P/"
	cp "$B/epoc/psipdf.exe" "$B/app/psipdf.app" "$B/app/psipdf.rsc" "$B/rec/psipdf.mdl" "$P/"
	cp "$B/gen/PsiPDF.fnt" "$P/psipdf.fnt"   # the 14 standard PDF fonts (Makefile)
	[ -f "$P/psipdf.aif" ] && [ -f "$P/psipdf.mbm" ] || { echo "no icons: python3 pkg/mkicon.py build/pkg core"; exit 1; }
	install -m 644 "$REL/stdlib.sis" "$P/STDLIB.SIS"   # ESTLIB.DLL, which the engine needs
	(cd "$P" && wine "$EPOCROOT/epoc32/tools/makesis.exe" psipdf.pkg psipdf.sis > "$B/sis.log" 2>&1) || { cat "$B/sis.log"; exit 1; }
	mv "$P/psipdf.sis" "$HERE/dist/PsiPDF.sis"
	ls -la "$HERE/dist/PsiPDF.sis"
	exit 0
fi

if [ "$1" = emu ]; then
	# the netBook emulator's test launcher (test/emu/launch: emulator only)
	L=$B/launch; rm -rf "$L"; mkdir -p "$L"
	cp "$HERE"/test/emu/launch/* "$L/"
	link pplaunch "$L"
	mmp pplaunch pplaunch pplaunch.app launch
	cp "$REL/pplaunch.app" "$REL/pplaunch.rsc" "$L/"
	ls -la "$L/pplaunch.app"
	exit 0
fi

[ -f "$HERE/third/mupdf/Makefile" ] || "$HERE/fetch.sh"

echo "== psipdf.exe (MuPDF)"
(cd "$HERE" && make TARGET=epoc CORE="${CORE#$HERE/}" PSION_SDK="$PSION_SDK" -j4 > "$B/engine.log" 2>&1) \
	|| { grep -E "error|rror:|undefined" "$B/engine.log" | head -40; exit 1; }
ls -la "$B/epoc/psipdf.exe"

echo "== PsiPDF.app"
D=$B/app; rm -rf "$D"; mkdir -p "$D"
# (this SDK's makmake has no SOURCEPATH, and every ptproj project shares one
# object folder: the sources are copied together under names of their own)
cp "$HERE"/app/* "$HERE/psipdf.h" "$HERE/engine/ppcore.h" "$CORE/ssh/psibell.h" "$CORE/ssh/psishared.h" "$CORE/ssh/psimem.h" "$CORE/ssh/psimemlvl.h" "$D/"
sed -i 's#"engine/ppcore.h"#"ppcore.h"#' "$D/psipdf.h"
link psipdf "$D"
mmp psipdf psipdf psipdf.app app
cp "$REL/psipdf.app" "$REL/psipdf.rsc" "$D/"

echo "== PsiPDF.mdl"
R=$B/rec; rm -rf "$R"; mkdir -p "$R"
cp "$HERE"/rec/* "$R/"
link pprec "$R"
mmp pprec pprec pprec.mdl rec
cp "$REL/pprec.mdl" "$R/psipdf.mdl"
ls -la "$D/psipdf.app" "$R/psipdf.mdl"
