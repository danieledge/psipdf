#!/usr/bin/env python3
"""PsiPDF's test PDFs (nothing private or copyrighted):

  coda.pdf   a PDF from Coda's own Print to PDF (copied: --coda FILE)
  text.pdf   a typical text document with embedded TrueType fonts: two
             columns of our own words in Liberation Serif and Sans (subset,
             FontFile2), made by Ghostscript from PostScript written here
  photo.pdf  a page with a photo (--photo FILE.jpg, passed through as
             DCTDecode: NASA's AS11-40-5875, public domain) under a heading
  multi.pdf  a 12-page document (Ghostscript, the base 14 fonts): a heading,
             running text, a coloured box and a page number on each page;
             the word "aubergine" on pages 4 and 9, for Find
  locked.pdf text.pdf with a password to open it: "psion" (Ghostscript's
             standard security, 128-bit RC4)

  mkpdfs.py OUTDIR --coda FILE.pdf --photo FILE.jpg"""
# Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
import os, shutil, subprocess, sys, zlib

out = sys.argv[1]
coda = sys.argv[sys.argv.index('--coda') + 1]
photo = sys.argv[sys.argv.index('--photo') + 1]
os.makedirs(out, exist_ok=True)
shutil.copy(coda, os.path.join(out, 'coda.pdf'))

# ---- text.pdf: Ghostscript, TrueType fonts from a Fontmap of our own ----
LIB = '/usr/share/fonts/truetype/liberation/'
fm = os.path.join(out, 'Fontmap.test')
with open(fm, 'w') as f:
    for n, file in [('TSerif', 'LiberationSerif-Regular'), ('TSerifB', 'LiberationSerif-Bold'),
                    ('TSerifI', 'LiberationSerif-Italic'), ('TSans', 'LiberationSans-Bold')]:
        f.write('/%s (%s%s.ttf) ;\n' % (n, LIB, file))
WORDS = ('The netBook reads its documents from the card or the internal disk. A viewer '
         'must open a file quickly, draw the first page while the rest waits, and keep '
         'within the memory a small machine has to spare. Text is the common case: '
         'paragraphs in one or two typefaces, headings, a table of figures, perhaps a '
         'rule or a box. Pictures come second, and forms and annotations a distant third. '
         'This page is typical of a report saved from a word processor: two columns, '
         'justified, with embedded fonts subset to the letters used.').split()
ps = ['%!PS', '/F { findfont exch scalefont setfont } def']
y = 760
ps.append('24 /TSans F 56 %d moveto (A typical text page) show' % y)
ps.append('11 /TSerifI F 56 %d moveto (Embedded TrueType fonts, two columns, a table) show' % (y - 20))
ps.append('0.5 setlinewidth 56 %d moveto 556 %d lineto stroke' % (y - 30, y - 30))
def column(x0, ytop, ybot, seed):
    lines, cur, n, k = [], [], 0, seed
    width = 230
    ps.append('10 /TSerif F')
    yy = ytop
    while yy > ybot:
        w = WORDS[k % len(WORDS)]; k += 1
        trial = ' '.join(cur + [w])
        # (an estimate of the width: 0.47 em a letter at 10 pt)
        if len(trial) * 4.7 > width and cur:
            s = ' '.join(cur).replace('(', '').replace(')', '')
            ps.append('%d %d moveto (%s) show' % (x0, yy, s))
            yy -= 12.5; cur = [w]
            if k % 37 == 0:
                yy -= 8
                ps.append('12 /TSerifB F %d %d moveto (Section %d) show 10 /TSerif F' % (x0, yy, k // 37)); yy -= 16
        else:
            cur.append(w)
column(56, y - 50, 260, 0)
column(326, y - 50, 260, 11)
# a table
ps.append('10 /TSerifB F')
cols = ['Item', 'Size', 'Time', 'Memory']
for i, c in enumerate(cols):
    ps.append('%d 236 moveto (%s) show' % (56 + i * 125, c))
ps.append('0.5 setlinewidth 56 230 moveto 556 230 lineto stroke 10 /TSerif F')
for r in range(8):
    vals = ['Row %d' % (r + 1), '%d KB' % (40 + r * 17), '%.1f s' % (0.4 + r * 0.3), '%d KB' % (300 + r * 41)]
    for i, v in enumerate(vals):
        ps.append('%d %d moveto (%s) show' % (56 + i * 125, 216 - r * 14, v))
ps.append('56 96 moveto 556 96 lineto stroke')
ps.append('9 /TSerifI F 56 80 moveto (Made for the PsiPDF tests. Liberation fonts, SIL Open Font Licence.) show')
ps.append('showpage')
psf = os.path.join(out, 'text.ps')
open(psf, 'w').write('\n'.join(ps) + '\n')
subprocess.run(['gs', '-q', '-dSAFER', '-dBATCH', '-dNOPAUSE', '-sDEVICE=pdfwrite', '-sPAPERSIZE=a4',
                '-dCompatibilityLevel=1.4', '-sFONTMAP=' + os.path.abspath(fm), '--permit-file-read=' + LIB, '--permit-file-read=' + os.path.abspath(out) + '/', '-sOutputFile=' + os.path.join(out, 'text.pdf'), psf],
               check=True)
os.remove(psf); os.remove(fm)

# ---- photo.pdf: written here, the JPEG passed through ----
jpg = open(photo, 'rb').read()
def jpeg_size(b):
    i = 2
    while i < len(b):
        m, ln = b[i + 1], int.from_bytes(b[i + 2:i + 4], 'big')
        if m in (0xC0, 0xC1, 0xC2):
            return int.from_bytes(b[i + 7:i + 9], 'big'), int.from_bytes(b[i + 5:i + 7], 'big')
        i += 2 + ln
    raise SystemExit('no SOF')
w, h = jpeg_size(jpg)
iw = 480; ih = iw * h / w
content = ('BT /F1 22 Tf 57 780 Td (A page with a photo) Tj ET\n'
           'BT /F2 11 Tf 57 760 Td (Apollo 11, July 1969: NASA photo AS11-40-5875, public domain.) Tj ET\n'
           'q %.2f 0 0 %.2f %.2f %.2f cm /Im1 Do Q\n'
           'BT /F2 10 Tf 57 %.2f Td (The picture is a %d x %d JPEG \\(DCTDecode\\), drawn to fit the column.) Tj ET\n'
           % (iw, ih, (595 - iw) / 2, 740 - ih, 740 - ih - 18, w, h)).encode()
objs = [
    b'<< /Type /Catalog /Pages 2 0 R >>',
    b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
    b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents 4 0 R '
    b'/Resources << /Font << /F1 5 0 R /F2 6 0 R >> /XObject << /Im1 7 0 R >> >> >>',
    None, b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>',
    b'<< /Type /Font /Subtype /Type1 /BaseFont /Times-Roman /Encoding /WinAnsiEncoding >>', None]
z = zlib.compress(content)
pdf = bytearray(b'%PDF-1.4\n%\xe2\xe3\xcf\xd3\n')
offs = []
for i, o in enumerate(objs, 1):
    offs.append(len(pdf))
    pdf += b'%d 0 obj\n' % i
    if i == 4:
        pdf += b'<< /Length %d /Filter /FlateDecode >>\nstream\n' % len(z) + z + b'\nendstream'
    elif i == 7:
        pdf += (b'<< /Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /DeviceRGB '
                b'/BitsPerComponent 8 /Filter /DCTDecode /Length %d >>\nstream\n' % (w, h, len(jpg))) + jpg + b'\nendstream'
    else:
        pdf += o
    pdf += b'\nendobj\n'
x = len(pdf)
pdf += b'xref\n0 %d\n0000000000 65535 f \n' % (len(objs) + 1)
for o in offs:
    pdf += b'%010d 00000 n \n' % o
pdf += b'trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (len(objs) + 1, x)
open(os.path.join(out, 'photo.pdf'), 'wb').write(pdf)

# ---- multi.pdf: 12 pages ----
ps = ['%!PS', '/F { findfont exch scalefont setfont } def']
COL = [(0.8, 0.1, 0.1), (0.1, 0.5, 0.1), (0.1, 0.3, 0.8), (0.9, 0.6, 0.0)]
for pg in range(1, 13):
    ps.append('20 /Helvetica-Bold F 56 770 moveto (Chapter %d) show' % pg)
    r, g, b = COL[pg % 4]
    ps.append('%.2f %.2f %.2f setrgbcolor 430 740 120 60 rectfill 0 setgray' % (r, g, b))
    ps.append('11 /Times-Roman F')
    y, k = 730, pg * 7
    while y > 90:
        words, line = [], ''
        while len(line) < 70:
            w = WORDS[k % len(WORDS)].replace('(', '').replace(')', ''); k += 1
            line = (line + ' ' + w).strip()
        if pg in (4, 9) and y == 730 - 14 * 10:
            line = 'Here is the aubergine, found by Find on page %d.' % pg
        ps.append('56 %d moveto (%s) show' % (y, line))
        y -= 14
    ps.append('9 /Helvetica F 280 50 moveto (Page %d of 12) show' % pg)
    ps.append('showpage')
psf = os.path.join(out, 'multi.ps')
open(psf, 'w').write('\n'.join(ps) + '\n')
subprocess.run(['gs', '-q', '-dSAFER', '-dBATCH', '-dNOPAUSE', '-sDEVICE=pdfwrite', '-sPAPERSIZE=a4',
                '-dCompatibilityLevel=1.4', '-sOutputFile=' + os.path.join(out, 'multi.pdf'), psf], check=True)
os.remove(psf)

# ---- locked.pdf: text.pdf with a password ----
subprocess.run(['gs', '-q', '-dSAFER', '-dBATCH', '-dNOPAUSE', '-sDEVICE=pdfwrite', '-sOwnerPassword=owner',
                '-sUserPassword=psion', '-dEncryptionR=3', '-dKeyLength=128', '-dPermissions=-4',
                '-sOutputFile=' + os.path.join(out, 'locked.pdf'), os.path.join(out, 'text.pdf')], check=True)

for n in ('coda.pdf', 'text.pdf', 'photo.pdf', 'multi.pdf', 'locked.pdf'):
    print(n, os.path.getsize(os.path.join(out, n)))
