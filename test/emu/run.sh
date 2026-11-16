#!/bin/bash
# The netBook emulator test: PsiPDF put in a copy of a scratch Coda image
# (nbimage.mts), then SCRIPT (emu.py's language) run into OUT, its shots as
# PNGs, and PsiPDF's log found in the RAM at the end (OUT/psipdf.log).
#   test/emu/run.sh BASE_OS.IMG SCRIPT OUT [emu.py options]
# Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$HERE/../.." && pwd)
BASE=$(readlink -f "$1") SCRIPT=$(readlink -f "$2") OUT=$(readlink -m "$3")
shift 3
mkdir -p "$OUT"
cp "$BASE" "$OUT/OS.IMG"
node --experimental-strip-types "$HERE/nbimage.mts" "$OUT/OS.IMG" > "$OUT/image.txt"
mkdir -p "$OUT/none"
node --experimental-strip-types "$HERE/mkcard.mts" "$OUT/OS.IMG" "$OUT/card.img" "$OUT/none" > /dev/null
python3 "$HERE/emu.py" "$OUT/card.img" "$OUT" "$SCRIPT" --exc --save-ram "$OUT/ram.bin" "$@"
strings -n 8 "$OUT/ram.bin" | grep -aE "^[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{2} (PsiPDF|opening|engine|open:|page |pump|find)" | awk '!seen[$0]++' > "$OUT/psipdf.log" || true
rm -f "$OUT/ram.bin" "$OUT/card.img"
grep -i -E 'panic|exception' "$OUT/log.txt" | grep -v unwedge | head -5 || true
cat "$OUT/psipdf.log"
