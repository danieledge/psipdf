#!/usr/bin/env python3
"""PsiPDF's hostile-file tests (0.08): small PDFs that each claim a size or
count meant to break the engine. Each should give an error the app shows
(or a page drawn without the bad part), never a crash.

  objstm-wrap.pdf  an object stream with /N 1073741822: 4 bytes each is
                   0xFFFFFFF8, which the size word in front of each block
                   wrapped to 0 (a tiny block, then a memset over the heap)
  objstm-2g.pdf    /N 600000000: 2.4 GB, past 2^31 (USER 47 on the Psion)
  xref-size.pdf    an xref stream with /Size 100000000 (2.4 GB of entries)
  xref-index.pdf   an xref stream with /Index [2147483600 100]
  picture-wrap.pdf a 65536 x 32768 picture, 1 bit a pixel: its pixels times
                   2 bytes is 2^32, which wrapped the picture budget to 0
  mesh-decode.pdf  a mesh shading with a 1000-number /Decode (past the 32
                   colours it has room for)
  sample-wrap.pdf  a sampled function whose /Size times its outputs wraps
                   to 4 samples

  crafted.py OUTDIR"""
# Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
import os, struct, sys, zlib

out = sys.argv[1]
os.makedirs(out, exist_ok=True)


def stream(d, data):
    return d + b' /Length %d >>\nstream\n' % len(data) + data + b'\nendstream'


def classic(path, objs):
    b = bytearray(b'%PDF-1.4\n')
    offs = []
    for i, o in enumerate(objs, 1):
        offs.append(len(b))
        b += b'%d 0 obj\n' % i + o + b'\nendobj\n'
    x = len(b)
    b += b'xref\n0 %d\n0000000000 65535 f \n' % (len(objs) + 1)
    for o in offs:
        b += b'%010d 00000 n \n' % o
    b += b'trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (len(objs) + 1, x)
    open(os.path.join(out, path), 'wb').write(b)


def xrefstm(path, objs, compressed, size=None, index=None):
    """objs: {num: bytes} written whole; compressed: {num: (stream num,
    index)}; an xref stream (W [1 4 2]) as the last object"""
    b = bytearray(b'%PDF-1.5\n')
    offs = {}
    for n in sorted(objs):
        offs[n] = len(b)
        b += b'%d 0 obj\n' % n + objs[n] + b'\nendobj\n'
    xn = max(list(objs) + list(compressed)) + 1
    offs[xn] = len(b)
    rows = bytearray()
    for n in range(xn + 1):
        if n in offs:
            rows += struct.pack('>BIH', 1, offs[n], 0)
        elif n in compressed:
            rows += struct.pack('>BIH', 2, compressed[n][0], compressed[n][1])
        else:
            rows += struct.pack('>BIH', 0, 0, 65535)
    d = b'<< /Type /XRef /Size %d /W [1 4 2] /Root 1 0 R' % (size if size else xn + 1)
    if index:
        d += b' /Index [%d %d]' % index
    b += b'%d 0 obj\n' % xn + stream(d, bytes(rows)) + b'\nendobj\n'
    b += b'startxref\n%d\n%%%%EOF\n' % offs[xn]
    open(os.path.join(out, path), 'wb').write(b)


PAGES = b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>'
PAGE = b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] >>'


def objstm(path, n):
    body = b'1 0 << /Type /Catalog /Pages 2 0 R >>'
    first = len(b'1 0 ')
    xrefstm(path, {2: PAGES, 3: PAGE,
                   4: stream(b'<< /Type /ObjStm /N %d /First %d' % (n, first), body)},
            {1: (4, 0)})


objstm('objstm-wrap.pdf', 1073741822)
objstm('objstm-2g.pdf', 600000000)
xrefstm('xref-size.pdf', {1: b'<< /Type /Catalog /Pages 2 0 R >>', 2: PAGES, 3: PAGE}, {}, size=100000000)
xrefstm('xref-index.pdf', {1: b'<< /Type /Catalog /Pages 2 0 R >>', 2: PAGES, 3: PAGE}, {},
        index=(2147483600, 100))


def picture(path):
    W, H = 65536, 32768
    z = zlib.compressobj(9)
    row = b'\x55' * (W // 8)
    data = bytearray()
    for y in range(H):
        data += z.compress(row)
    data += z.flush()
    page = (b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] '
            b'/Resources << /XObject << /Im0 5 0 R >> >> /Contents 4 0 R >>')
    classic(path, [b'<< /Type /Catalog /Pages 2 0 R >>', PAGES, page,
                   stream(b'<<', b'q 200 0 0 200 0 0 cm /Im0 Do Q'),
                   stream(b'<< /Type /XObject /Subtype /Image /Width %d /Height %d '
                          b'/ColorSpace /DeviceGray /BitsPerComponent 1 /Filter /FlateDecode' % (W, H), bytes(data))])


def shaded(path, shading, extra=None):
    page = (b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] '
            b'/Resources << /Shading << /Sh0 5 0 R >> >> /Contents 4 0 R >>')
    objs = [b'<< /Type /Catalog /Pages 2 0 R >>', PAGES, page,
            stream(b'<<', b'q 0 0 200 200 re W n /Sh0 sh Q'), shading]
    if extra:
        objs.append(extra)
    classic(path, objs)


def mesh(path):
    # a free-form triangle mesh (type 4): flag, x, y, grey per vertex
    tri = bytearray()
    for f, x, y, c in [(0, 0, 0, 0), (0, 255, 0, 128), (0, 0, 255, 255)]:
        tri += bytes([f, x, y, c])
    decode = b' '.join([b'0 200 0 200'] + [b'0 1'] * 498)
    shaded(path, stream(b'<< /ShadingType 4 /ColorSpace /DeviceGray /BitsPerCoordinate 8 '
                        b'/BitsPerComponent 8 /BitsPerFlag 8 /Decode [%s]' % decode, bytes(tri)))


def sample(path):
    # axial shading, its function sampled: 4 outputs x 0x40000001 samples
    # is 0x100000004, kept in 32 bits as 4
    shaded(path, b'<< /ShadingType 2 /ColorSpace /DeviceCMYK /Coords [0 0 200 0] /Function 6 0 R >>',
           stream(b'<< /FunctionType 0 /Domain [0 1] /Range [0 1 0 1 0 1 0 1] /Size [1073741825] '
                  b'/BitsPerSample 8', bytes(range(16))))


picture('picture-wrap.pdf')
mesh('mesh-decode.pdf')
sample('sample-wrap.pdf')
print('made in', out)
