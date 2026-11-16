#!/usr/bin/env python3
# Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
"""Writes MuPDF's gen_font_base14.h (what its own scripts/fontdump.c makes)
from the base 14 .cff files: one const array per font, so they stay in the
program's code (read-only, nothing writable).

  fontdump.py OUT.h FONTDIR [--slim]

--slim: only the three regular faces (Helvetica, Times, Courier) and Symbol
and Dingbats; the bold and italic names point at the regular face (to
measure what the 9 others cost)."""
import os, sys

out, fontdir = sys.argv[1], sys.argv[2]
slim = '--slim' in sys.argv
NAMES = ['Dingbats', 'NimbusMonL-Bold', 'NimbusMonL-BoldObli', 'NimbusMonL-Regu', 'NimbusMonL-ReguObli',
         'NimbusRomNo9L-Medi', 'NimbusRomNo9L-MediItal', 'NimbusRomNo9L-Regu', 'NimbusRomNo9L-ReguItal',
         'NimbusSanL-Bold', 'NimbusSanL-BoldItal', 'NimbusSanL-Regu', 'NimbusSanL-ReguItal', 'StandardSymL']
KEEP = {'Dingbats', 'NimbusMonL-Regu', 'NimbusRomNo9L-Regu', 'NimbusSanL-Regu', 'StandardSymL'}
total = 0
with open(out, 'w') as f:
    f.write('/* made by tools/fontdump.py */\n')
    for n in NAMES:
        c = 'pdf_font_' + n.replace('-', '_')
        if slim and n not in KEEP:
            continue
        data = open(os.path.join(fontdir, n + '.cff'), 'rb').read()
        total += len(data)
        f.write('static const unsigned char %s[%d] = {\n' % (c, len(data)))
        for i in range(0, len(data), 24):
            f.write(','.join(str(b) for b in data[i:i + 24]) + ',\n')
        f.write('};\n')
    if slim:
        for n in NAMES:
            if n not in KEEP:
                fam = n.split('-')[0]
                f.write('#define pdf_font_%s pdf_font_%s_Regu\n' % (n.replace('-', '_'), fam))
print('%s: %d bytes of fonts%s' % (out, total, ' (slim)' if slim else ''))
