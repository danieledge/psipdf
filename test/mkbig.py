#!/usr/bin/env python3
"""PsiPDF's memory tests (0.06), nothing private:

  scan1.pdf  an A4 page that is one 300 dpi scan, 1 bit a pixel (Flate):
             MuPDF decoded it whole, 18.7 MB, before reducing it
  scan8.pdf  the same at 8 bits a pixel: 26 MB
  huge.pdf   a page 50,000,000 points wide: no bitmap can hold it

  mkbig.py OUTDIR      (then build/host/pphost -w 640 OUT OUTDIR/scan8.pdf
                        prints the heap's peak)"""
# Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
import zlib, sys, os
out = sys.argv[1]
def pdf(path, objs):
    b = bytearray(b'%PDF-1.4\n')
    offs = []
    for i, o in enumerate(objs, 1):
        offs.append(len(b))
        b += b'%d 0 obj\n' % i + o + b'\nendobj\n'
    x = len(b)
    b += b'xref\n0 %d\n0000000000 65535 f \n' % (len(objs) + 1)
    for o in offs: b += b'%010d 00000 n \n' % o
    b += b'trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (len(objs) + 1, x)
    open(path, 'wb').write(b)
def stream(d, data):
    return d + b' /Length %d >>\nstream\n' % len(data) + data + b'\nendstream'
def scan(path, bpc):
    W, H = 2480, 3508
    rows = []
    for y in range(H):
        if bpc == 1:
            row = bytearray((W + 7) // 8)
            for x in range(0, W, 8):
                row[x // 8] = 0x00 if ((x // 64 + y // 64) & 1) else 0xff
        else:
            row = bytes(((x + y) // 10) & 0xff for x in range(W))
        rows.append(bytes(row))
    data = zlib.compress(b''.join(rows), 6)
    content = b'q 595 0 0 842 0 0 cm /Im0 Do Q'
    pdf(path, [b'<< /Type /Catalog /Pages 2 0 R >>',
               b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
               b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /XObject << /Im0 5 0 R >> >> /Contents 4 0 R >>',
               stream(b'<<', content),
               stream(b'<< /Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /DeviceGray /BitsPerComponent %d /Filter /FlateDecode' % (W, H, bpc), data)])
def huge(path):
    content = b'0 0 1 rg 0 0 50000000 50000000 re f'
    pdf(path, [b'<< /Type /Catalog /Pages 2 0 R >>',
               b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
               b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 50000000 2000] /Contents 4 0 R >>',
               stream(b'<<', content)])
os.makedirs(out, exist_ok=True)
scan(os.path.join(out, 'scan1.pdf'), 1)
scan(os.path.join(out, 'scan8.pdf'), 8)
huge(os.path.join(out, 'huge.pdf'))
