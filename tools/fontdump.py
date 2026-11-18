#!/usr/bin/env python3
# Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
"""Makes PsiPDF's font file from MuPDF's base 14 .cff files, and the one
font that stays in the program.

  fontdump.py OUT.fonts OUT.h FONTDIR

OUT.fonts (installed as \\System\\Apps\\PsiPDF\\PsiPDF.fnt, read by
engine/ppfont.c one font at a time, when a PDF needs it). All numbers
little-endian, 32 bits:

  0   'PPFN'
  4   1 (the format)
  8   14 (the fonts, in the order of ppfont.c's names: Courier, Courier-Bold,
      Courier-Oblique, Courier-BoldOblique, Helvetica..., Times-Roman...,
      Symbol, ZapfDingbats)
  12  the file's length
  16  14 x (offset, length, Adler-32 of the font)
  184 the fonts (CFF), each on a 4-byte boundary

OUT.h: Helvetica (Nimbus Sans L, 16 KB) as a const array, which stays in
the program's code for when the font file is missing or damaged."""
import os, struct, sys, zlib

out, hdr, fontdir = sys.argv[1], sys.argv[2], sys.argv[3]
# (the order of pp_font_names in engine/ppfont.c)
FILES = ['NimbusMonL-Regu', 'NimbusMonL-Bold', 'NimbusMonL-ReguObli', 'NimbusMonL-BoldObli',
         'NimbusSanL-Regu', 'NimbusSanL-Bold', 'NimbusSanL-ReguItal', 'NimbusSanL-BoldItal',
         'NimbusRomNo9L-Regu', 'NimbusRomNo9L-Medi', 'NimbusRomNo9L-ReguItal', 'NimbusRomNo9L-MediItal',
         'StandardSymL', 'Dingbats']
SPARE = 'NimbusSanL-Regu'
fonts = [open(os.path.join(fontdir, n + '.cff'), 'rb').read() for n in FILES]
head = 16 + 12 * len(fonts)
table, body, at = b'', b'', head
for d in fonts:
    table += struct.pack('<III', at, len(d), zlib.adler32(d) & 0xffffffff)
    d += b'\0' * (-len(d) % 4)
    body += d
    at += len(d)
with open(out, 'wb') as f:
    f.write(b'PPFN' + struct.pack('<III', 1, len(fonts), at) + table + body)
data = fonts[FILES.index(SPARE)]
with open(hdr, 'w') as f:
    f.write('/* made by tools/fontdump.py: %s, the font kept in the program */\n' % SPARE)
    f.write('static const unsigned char pp_font_spare[%d] = {\n' % len(data))
    for i in range(0, len(data), 24):
        f.write(','.join(str(b) for b in data[i:i + 24]) + ',\n')
    f.write('};\n')
print('%s: %d bytes, %d fonts; %s: %d bytes in the program' % (out, at, len(fonts), hdr, len(data)))
