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


OUTLINE, SHADOW, PAPER, FOLD, TEXT = (51, 51, 51), (153, 153, 153), (255, 255, 255), (204, 204, 204), (153, 153, 153)
ARROW, ARROW_LIGHT, ARROW_DARK = (0, 102, 204), (51, 153, 255), (0, 51, 102)
LENS, LENS_LIGHT, HANDLE, SIGN = (204, 204, 255), (255, 255, 255), (153, 102, 51), (0, 51, 102)
KEY = (255, 0, 255)                     # (not drawn: the mask's hole)


def tool(kind):
    """a toolbar picture, 24 x 20, in Coda's icon recipe (0.09): 1-pixel
    outlines in 333333, flat fills from the 6x6x6 cube with a lighter
    top-left and a darker bottom-right edge, a 1-pixel shadow in 999999.
    Previous and Next: a page with a blue arrow up or down over its right
    side; Zoom in and out: a magnifier with + or -."""
    im = Image.new('RGB', (24, 20), KEY)
    d = ImageDraw.Draw(im)
    if kind in ('prev', 'next'):
        # the page: shadow, paper, the turned corner, lines of text
        d.polygon([(3, 3), (10, 3), (13, 6), (13, 19), (3, 19)], fill=SHADOW)
        d.polygon([(2, 2), (9, 2), (12, 5), (12, 18), (2, 18)], fill=PAPER, outline=OUTLINE)
        d.polygon([(9, 2), (9, 5), (12, 5)], fill=FOLD, outline=OUTLINE)
        for y in (8, 11, 14):
            d.line([(4, y), (9, y)], fill=TEXT)
        # the arrow: a head and a shaft
        if kind == 'prev':
            head, shaft = [(9, 10), (16, 3), (23, 10)], [13, 10, 19, 17]
        else:
            head, shaft = [(9, 10), (16, 17), (23, 10)], [13, 3, 19, 10]
        sh = lambda pts: [(x + 1, y + 1) for x, y in pts]
        d.polygon(sh(head), fill=SHADOW)
        d.rectangle([shaft[0] + 1, shaft[1] + 1, shaft[2] + 1, shaft[3] + 1], fill=SHADOW)
        d.rectangle(shaft, fill=ARROW, outline=OUTLINE)
        d.polygon(head, fill=ARROW, outline=OUTLINE)
        # the light and dark edges, inside the outline
        if kind == 'prev':
            d.line([(11, 9), (16, 4)], fill=ARROW_LIGHT)
            d.line([(14, 11), (14, 16)], fill=ARROW_LIGHT)
            d.line([(18, 11), (18, 16)], fill=ARROW_DARK)
        else:
            d.line([(14, 4), (14, 10)], fill=ARROW_LIGHT)
            d.line([(18, 4), (18, 10)], fill=ARROW_DARK)
            d.line([(17, 15), (21, 11)], fill=ARROW_DARK)
        # (the arrow's head drawn over the page's edge: the outline again)
        d.polygon(head, outline=OUTLINE)
    else:
        # the handle, then the lens over it
        d.line([(15, 13), (21, 19)], fill=SHADOW, width=4)
        d.line([(14, 12), (20, 18)], fill=OUTLINE, width=4)
        d.line([(14, 12), (20, 18)], fill=HANDLE, width=2)
        d.ellipse([3, 2, 17, 16], fill=SHADOW)
        d.ellipse([2, 1, 16, 15], fill=LENS, outline=OUTLINE)
        d.arc([4, 3, 14, 13], 160, 260, fill=LENS_LIGHT)
        d.line([(5, 8), (13, 8)], fill=SIGN, width=2)
        if kind == 'zin':
            d.line([(9, 4), (9, 12)], fill=SIGN, width=2)
    mk = Image.new('1', (24, 20), 1)
    for y in range(20):
        for x in range(24):
            if im.getpixel((x, y)) == KEY:
                im.putpixel((x, y), WHITE)
            else:
                mk.putpixel((x, y), 0)
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
