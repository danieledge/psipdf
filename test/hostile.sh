#!/bin/bash
# hostile.sh PPHOST OUTDIR - (0.08) the hostile files of test/crafted.py
# through the engine on the PC, with blocks limited to 16 MB as psipdf.exe
# limits them to the memory free (-L). Each must end with an error or a
# page, never a crash, and as said below. make TARGET=host hostile runs it.
# Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
set -u
PP=$1 OUT=$2
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
python3 "$HERE/crafted.py" "$OUT" > /dev/null || exit 1
fails=0
# file, a pattern the output must have, and the block limit in KB
check() {
	local f=$1 want=$2 lim=${3:-16384} log=$OUT/$1.log code
	(ulimit -v 2000000; timeout 120 "$PP" -L $lim -w 200 "$OUT/$f" "$OUT/$f.pdf") > "$log" 2>&1
	code=$?
	if [ $code -ge 124 ]; then
		echo "FAIL $f: ended with $code (a crash, or too slow)"; fails=$((fails + 1))
	elif ! grep -q -- "$want" "$log"; then
		echo "FAIL $f: no \"$want\" in $log"; fails=$((fails + 1))
	else
		echo "ok   $f: $(grep -E "^open:|^page" "$log" | tail -1)"
	fi
}
check objstm-wrap  "more objects in object stream than in the file"
check objstm-2g    "more objects in object stream than in the file"
check xref-size    "xref stream Size out of range"
check xref-index   "xref stream entry index out of range"
# (its strips are 65536 pixels wide, 18 MB at the most: more than 16)
check picture-wrap "^page 1: .* px: 0 " 32768
check mesh-decode  "^page 1: "
check sample-wrap  "sample function too large"
# the picture is drawn reduced, not refused
grep -q "^error" "$OUT/picture-wrap.log" && { echo "FAIL picture-wrap: not drawn"; fails=$((fails + 1)); }
[ $fails = 0 ] && echo "hostile: all passed" || echo "hostile: $fails FAILED"
exit $((fails != 0))
