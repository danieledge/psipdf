#!/usr/bin/env python3
# Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
"""pkg/mkicon.py - PsiPDF's pictures, in EPOC's 256 colours (the netBook
shows them in colour; the Series 5mx in its greys, each colour by its
brightness):

    python3 pkg/mkicon.py OUTDIR CORE [PREVIEW.png]
        OUTDIR/psipdf.aif: the name, the icon (24, 32 and 48: a page with
            its corner turned and a red band), and the kind of file it
            opens: application/pdf (PsiPDF.mdl recognises PDF files, and
            the System screen then opens them in PsiPDF)
        OUTDIR/psipdf.mbm: the toolbar's pictures (Previous, Next, Zoom in,
            Zoom out), each followed by its mask (black is drawn)

The .aif's layout is psion-core's tools/icon/aif.py, with 8-bit colour
icons and, after the capability, the data types as apparc writes them in
EPOC R5: the version (1), then each type's stream (the MIME type) and its
16-bit priority."""
import os
import struct
import sys
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '../tools'))
from mkpal import PAL  # noqa: E402  EPOC's 256 colours, 0xRRGGBB

UID = 0x01000AD8
BLACK, WHITE, GREY, DARK = (0, 0, 0), (255, 255, 255), (204, 204, 204), (102, 102, 102)
RED, BLUE = (204, 0, 0), (0, 102, 204)
RGB = [((v >> 16) & 255, (v >> 8) & 255, v & 255) for v in PAL]


def nearest(c):
    r, g, b = c[:3]
    return min(range(256), key=lambda i: 2 * (r - RGB[i][0]) ** 2 + 4 * (g - RGB[i][1]) ** 2 + 3 * (b - RGB[i][2]) ** 2)


def bitmap8(img):
    """CFbsBitmap::ExternalizeL, uncompressed, 8 bits a pixel (EColor256)"""
    img = img.convert('RGB')
    w, h = img.size
    stride = (w + 3) & ~3
    cache, data = {}, bytearray()
    for y in range(h):
        row = bytearray(stride)
        for x in range(w):
            c = img.getpixel((x, y))
            if c not in cache:
                cache[c] = nearest(c)
            row[x] = cache[c]
        data += row
    return struct.pack('<IIiiiiIIII', 40 + len(data), 40, w, h, 0, 0, 8, 1, 0, 0) + bytes(data)


def canvas(w, h):
    im = Image.new('RGB', (w, h), WHITE)
    mk = Image.new('1', (w, h), 1)
    return im, mk, ImageDraw.Draw(im), ImageDraw.Draw(mk)


def app_icon(n):
    """a page with its top right corner turned down, lines of text, and a
    red band with the letters' place"""
    im, mk, d, m = canvas(n, n)
    s = n / 48.0
    R = lambda v: int(round(v * s))
    fold = R(11)
    x0, y0, x1, y1 = R(8), R(2), R(40), R(46)
    page = [(x0, y0), (x1 - fold, y0), (x1, y0 + fold), (x1, y1), (x0, y1)]
    d.polygon(page, fill=WHITE, outline=BLACK)
    m.polygon(page, fill=0, outline=0)
    d.polygon([(x1 - fold, y0), (x1 - fold, y0 + fold), (x1, y0 + fold)], fill=GREY, outline=BLACK)
    # lines of text
    for k, y in enumerate((R(15), R(19), R(23))):
        d.line([(x0 + R(4), y), (x1 - R(5) - (R(8) if k == 2 else 0), y)], fill=DARK)
    # the red band, with three white marks for "PDF"
    band = [x0 - R(5), R(28), x1 - R(3), R(39)]
    d.rectangle(band, fill=RED, outline=BLACK)
    m.rectangle(band, fill=0)
    if n >= 32:
        for i in range(3):
            bx = band[0] + R(4) + i * R(9)
            d.rectangle([bx, R(31), bx + R(5), R(36)], fill=WHITE)
    else:
        d.line([(band[0] + 2, R(33)), (band[2] - 2, R(33))], fill=WHITE)
    for y in (R(42),):
        d.line([(x0 + R(4), y), (x1 - R(9), y)], fill=DARK)
    return im, mk


def tool(kind):
    """a toolbar picture: Previous and Next 16 x 20 (room for their words in
    the toolbar's dense font), Zoom in and out 24 x 20"""
    if kind in ('prev', 'next'):
        im, mk, d, m = canvas(16, 20)
        # a page's corner with an arrow: up for the page before, down for the next
        pts = [(2, 12), (8, 5), (14, 12)] if kind == 'prev' else [(2, 8), (8, 15), (14, 8)]
        for dd, col in ((d, BLUE), (m, 0)):
            dd.polygon(pts, fill=col, outline=0 if dd is m else BLACK)
            dd.rectangle([5, 12, 11, 17] if kind == 'prev' else [5, 3, 11, 8], fill=col, outline=0 if dd is m else BLACK)
        return im, mk
    im, mk, d, m = canvas(24, 20)
    for dd, mm in ((d, False), (m, True)):
        dd.ellipse([3, 1, 16, 14], fill=0 if mm else WHITE, outline=0 if mm else BLACK, width=2)
        dd.line([(14, 12), (20, 18)], fill=0 if mm else DARK, width=3)
    d.line([(6, 7), (13, 7)], fill=BLACK, width=2)
    if kind == 'zin':
        d.line([(9, 4), (9, 11)], fill=BLACK, width=2)
    return im, mk


def main():
    out, core = sys.argv[1], sys.argv[2]
    sys.path.insert(0, os.path.join(core, 'tools/icon'))
    from aif import bitmap_stream, des8, uid_checksum
    os.makedirs(out, exist_ok=True)
    # the .aif
    icons = [app_icon(n) for n in (24, 32, 48)]
    captions = [(1, 'PsiPDF'), (10, 'PsiPDF')]
    types = [('application/pdf', 10000)]
    aif = bytearray(20)
    cap_hdr, icon_hdr, dt_hdr = [], [], []
    for lang, text in captions:
        cap_hdr.append((len(aif), lang)); aif += des8(text)
    for img, mask in icons:
        icon_hdr.append((len(aif), img.size[0])); aif += bitmap8(img) + bitmap_stream(mask, bpp=1)
    for mime, prio in types:
        dt_hdr.append((len(aif), prio)); aif += des8(mime)
    root = len(aif)
    aif += bytes([len(cap_hdr) << 1])
    for sid, lang in cap_hdr:
        aif += struct.pack('<Ih', sid, lang)
    aif += bytes([len(icon_hdr) << 1])
    for sid, side in icon_hdr:
        aif += struct.pack('<Ih', sid, side)
    aif += struct.pack('<iiii', 1, 0, 0, 0)      # capability: not embeddable, no new file, shown
    aif += struct.pack('<i', 1)                   # EAifVersionAddsDataType
    aif += bytes([len(dt_hdr) << 1])
    for sid, prio in dt_hdr:
        aif += struct.pack('<Ih', sid, prio)
    struct.pack_into('<IIIII', aif, 0, 0x10000037, 0x1000006A, UID, uid_checksum(0x10000037, 0x1000006A, UID), root)
    open(os.path.join(out, 'psipdf.aif'), 'wb').write(aif)
    # the toolbar's .mbm
    body, offs = b'', []
    for kind in ('prev', 'next', 'zin', 'zout'):
        im, mk = tool(kind)
        for data in (bitmap8(im), bitmap_stream(mk, bpp=1)):
            offs.append(20 + len(body)); body += data
    head = struct.pack('<IIII', 0x10000037, 0x10000042, 0, uid_checksum(0x10000037, 0x10000042, 0))
    trailer = struct.pack('<I', len(offs)) + b''.join(struct.pack('<I', o) for o in offs)
    open(os.path.join(out, 'psipdf.mbm'), 'wb').write(head + struct.pack('<I', 20 + len(body)) + body + trailer)
    print('psipdf.aif %d bytes, psipdf.mbm %d bytes' % (len(aif), 24 + len(body) + len(trailer)))
    if len(sys.argv) > 3:
        pics = [i for i, _ in icons] + [tool(k)[0] for k in ('prev', 'next', 'zin', 'zout')]
        w = sum(p.size[0] for p in pics) + 10 * len(pics)
        prev = Image.new('RGB', (w, 48), (200, 200, 200))
        x = 0
        for p in pics:
            prev.paste(p, (x, 0)); x += p.size[0] + 10
        prev.resize((w * 3, 144), Image.NEAREST).save(sys.argv[3])


if __name__ == '__main__':
    main()
