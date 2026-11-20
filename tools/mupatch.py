#!/usr/bin/env python3
"""(0.08) Limits put into MuPDF 1.3's own code, as later MuPDF has them,
for a hostile or damaged PDF: third/mupdf stays as fetched (fetch.sh checks
its SHA-256), and the Makefile compiles the copies this writes into
build/gen/mu/ in place of the files they come from.

  mupatch.py NAME SOURCE OUT    NAME: pdf/pdf-xref and the rest below

Each change must find its text exactly once, or nothing is written (a
MuPDF that is not 1.3 stops the build here, not on the Psion).

- pdf-xref.c: an object number is 0..8388607 (PDF_MAX_OBJECT_NUMBER, as
  MuPDF 1.13 has it), in an xref table, an xref stream's /Size and
  /Index, and the entry asked for; an object stream holds no more objects
  (/N) than the file's xref has
- pdf-shade.c: a mesh shading's /Decode gives at most the 32 colours there
  is room for (it was written past them)
- pdf-function.c: a sampled function's size is checked as it is
  multiplied (the product kept in 32 bits could wrap to a few samples,
  read far past)"""
# Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
import sys

MAXNUM = '8388607'

PATCHES = {
    'pdf/pdf-xref': [
        ("""	pdf_xref *xref;
	int i;

	if (doc->num_xref_sections == 0)
	{
		doc->xref_sections = fz_calloc(doc->ctx, 1, sizeof(pdf_xref));""",
         """	pdf_xref *xref;
	int i;

	if (num < 0 || num > %s)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "object number out of range (%%d)", num);

	if (doc->num_xref_sections == 0)
	{
		doc->xref_sections = fz_calloc(doc->ctx, 1, sizeof(pdf_xref));""" % MAXNUM),
        ("""		if (ofs < 0)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "out of range object num in xref: %d", ofs);""",
         """		if (ofs < 0 || ofs > %s || len < 0 || len > %s - ofs)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "out of range object num in xref: %%d", ofs);""" % (MAXNUM, MAXNUM)),
        ("""	if (i0 < 0 || i1 < 0)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "negative xref stream entry index");""",
         """	if (i0 < 0 || i1 < 0)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "negative xref stream entry index");
	if (i0 > %s || i1 > %s - i0)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "xref stream entry index out of range");""" % (MAXNUM, MAXNUM)),
        ("""		size = pdf_to_int(obj);
		/* Access xref entry to assure table size */""",
         """		size = pdf_to_int(obj);
		if (size < 1 || size > %s + 1)
			fz_throw(ctx, FZ_ERROR_GENERIC, "xref stream Size out of range (%%d)", size);
		/* Access xref entry to assure table size */""" % MAXNUM),
        ("""		if (first < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "first object in object stream resides outside stream");""",
         """		if (first < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "first object in object stream resides outside stream");
		if (count > pdf_xref_len(doc))
			fz_throw(ctx, FZ_ERROR_GENERIC, "more objects in object stream than in the file (%d)", count);"""),
    ],
    'pdf/pdf-shade': [
        ("""		n = (pdf_array_len(obj) - 4) / 2;""",
         """		n = fz_mini(FZ_MAX_COLORS, (pdf_array_len(obj) - 4) / 2);"""),
    ],
    'pdf/pdf-function': [
        ("""	for (i = 0, samplecount = func->base.n; i < func->base.m; i++)
		samplecount *= func->u.sa.size[i];""",
         """	for (i = 0, samplecount = func->base.n; i < func->base.m; i++)
	{
		if (samplecount <= 0 || func->u.sa.size[i] > MAX_SAMPLE_FUNCTION_SIZE / samplecount)
			fz_throw(ctx, FZ_ERROR_GENERIC, "sample function too large");
		samplecount *= func->u.sa.size[i];
	}"""),
    ],
}

name, src, out = sys.argv[1:4]
s = open(src).read()
for old, new in PATCHES[name]:
    if s.count(old) != 1:
        sys.exit('mupatch: %s: %d matches for %r' % (name, s.count(old), old[:60]))
    s = s.replace(old, new)
open(out, 'w').write('/* made by tools/mupatch.py from %s: do not edit */\n' % src + s)
