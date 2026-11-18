/* ppfont.c - the 14 standard PDF fonts out of the program.
 *
 * MuPDF's pdf-fontfile.c keeps all 14 (URW's, as CFF: 308 KB) in the
 * program's code, and a program on the card costs its whole size in RAM
 * while it runs. Here they are in a file beside the program (PsiPDF.fnt,
 * made by tools/fontdump.py) and this file takes pdf-fontfile.c's place:
 * a font is read when a PDF asks for it, shared by every use of it in the
 * document, and freed when the last FreeType face on it has gone (MuPDF's
 * store dropped the font, or the document closed). Most PDFs embed their
 * fonts and need none.
 *
 * Helvetica alone (16 KB) stays in the program. It is the font for a file
 * that is missing or damaged, so text still shows, in one typeface;
 * pp_font_trouble says so and the program tells the user.
 *
 * The font file is read as untrusted: its length, the table's offsets and
 * lengths and each font's checksum are checked before a byte is used.
 *
 * Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include "ppcore.h"
#include "ppfont.h"
#include "gen_font_spare.h"

#define PP_FONTS 14
#define PP_FONT_HEAD (16 + 12 * PP_FONTS)
#define PP_FONT_MAX (256 * 1024L)       /* no font of the 14 is near this */
#define PP_SPARE 4                      /* Helvetica: pp_font_spare */

/* (the order of tools/fontdump.py's FILES) */
static const char *const pp_font_names[PP_FONTS] =
{
	"Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
	"Helvetica", "Helvetica-Bold", "Helvetica-Oblique", "Helvetica-BoldOblique",
	"Times-Roman", "Times-Bold", "Times-Italic", "Times-BoldItalic",
	"Symbol", "ZapfDingbats"
};

typedef struct pp_slot
{
	unsigned char *data;    /* NULL: not in memory */
	unsigned int len;
	int refs;               /* FreeType faces reading it */
} pp_slot;

static pp_slot pp_slots[PP_FONTS];
static char pp_path[260];
static int pp_table;            /* 0 not read yet, 1 good, -1 no use */
static unsigned long pp_off[PP_FONTS], pp_len[PP_FONTS], pp_sum[PP_FONTS];
static int pp_trouble;

int pp_font_trouble(void)
{
	return pp_trouble;
}

void pp_font_file(const char *path)
{
	unsigned int i = 0;
	pp_font_reset();
	while (path && path[i] && i < sizeof pp_path - 1)
	{
		pp_path[i] = path[i];
		i++;
	}
	pp_path[i] = 0;
	pp_table = 0;
	pp_trouble = 0;
}

void pp_font_sweep(void)
{
	int i;
	for (i = 0; i < PP_FONTS; i++)
		if (pp_slots[i].data && pp_slots[i].refs <= 0)
		{
			pp_sys_free(pp_slots[i].data);
			pp_slots[i].data = NULL;
			pp_slots[i].refs = 0;
		}
}

void pp_font_reset(void)
{
	int i;
	for (i = 0; i < PP_FONTS; i++)
		pp_slots[i].refs = 0;
	pp_font_sweep();
}

unsigned long pp_font_bytes(void)
{
	unsigned long n = 0;
	int i;
	for (i = 0; i < PP_FONTS; i++)
		if (pp_slots[i].data)
			n += pp_slots[i].len;
	return n;
}

static unsigned long le32(const unsigned char *p)
{
	return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static unsigned long adler(const unsigned char *p, unsigned long n)
{
	unsigned long a = 1, b = 0;
	while (n)
	{
		unsigned long k = n > 5552 ? 5552 : n;
		n -= k;
		while (k--)
		{
			a += *p++;
			b += a;
		}
		a %= 65521;
		b %= 65521;
	}
	return (b << 16) | a;
}

/* the file's table read and checked: 1 it can be used, PP_FONT_MISSING or
   PP_FONT_DAMAGED otherwise */
static int read_table(FILE *f)
{
	unsigned char h[PP_FONT_HEAD];
	long size;
	unsigned long flen;
	int i;
	if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0)
		return -PP_FONT_DAMAGED;
	if (size < PP_FONT_HEAD || fread(h, 1, PP_FONT_HEAD, f) != PP_FONT_HEAD)
		return -PP_FONT_DAMAGED;
	flen = le32(h + 12);
	if (memcmp(h, "PPFN", 4) || le32(h + 4) != 1 || le32(h + 8) != PP_FONTS || flen != (unsigned long)size)
		return -PP_FONT_DAMAGED;
	for (i = 0; i < PP_FONTS; i++)
	{
		unsigned long off = le32(h + 16 + 12 * i), len = le32(h + 20 + 12 * i);
		if (off < PP_FONT_HEAD || off > flen || len < 4 || len > PP_FONT_MAX || len > flen - off)
			return -PP_FONT_DAMAGED;
		pp_off[i] = off;
		pp_len[i] = len;
		pp_sum[i] = le32(h + 24 + 12 * i);
	}
	return 1;
}

/* font i into its slot: 0, or why not (PP_FONT_*) */
static int load(int i)
{
	FILE *f;
	unsigned char *p = NULL;
	int r = 0;
	if (pp_table < 0)
		return pp_trouble ? pp_trouble : PP_FONT_DAMAGED;
	if (!pp_path[0] || !(f = fopen(pp_path, "rb")))
	{
		pp_table = -1;
		return PP_FONT_MISSING;
	}
	if (pp_table == 0)
	{
		int t = read_table(f);
		pp_table = t == 1 ? 1 : -1;
		if (t != 1)
			r = -t;
	}
	if (!r)
	{
		p = (unsigned char *)pp_sys_malloc((unsigned int)pp_len[i]);
		if (!p)
			r = PP_FONT_MEMORY;
		else if (fseek(f, (long)pp_off[i], SEEK_SET) != 0 || fread(p, 1, pp_len[i], f) != pp_len[i]
			|| adler(p, pp_len[i]) != pp_sum[i])
			r = PP_FONT_DAMAGED;    /* (this font only: the others may be whole) */
	}
	fclose(f);
	if (r)
	{
		if (p)
			pp_sys_free(p);
		return r;
	}
	pp_slots[i].data = p;
	pp_slots[i].len = (unsigned int)pp_len[i];
	pp_slots[i].refs = 0;
	return 0;
}

/* ---- what MuPDF's pdf-fontfile.c gave pdf-font.c ---- */

unsigned char *pdf_lookup_builtin_font(char *name, unsigned int *len)
{
	int i, r;
	pp_font_sweep();
	for (i = 0; i < PP_FONTS; i++)
		if (!strcmp(pp_font_names[i], name))
			break;
	if (i == PP_FONTS)
	{
		*len = 0;
		return NULL;
	}
	if (i != PP_SPARE)
	{
		if (pp_slots[i].data)
		{
			*len = pp_slots[i].len;
			return pp_slots[i].data;
		}
		r = load(i);
		if (!r)
		{
			*len = pp_slots[i].len;
			return pp_slots[i].data;
		}
		/* (no memory for it is not the file's fault, and is not said) */
		if (r != PP_FONT_MEMORY && !pp_trouble)
			pp_trouble = r;
	}
	*len = sizeof pp_font_spare;
	return (unsigned char *)pp_font_spare;
}

/* (as pdf-fontfile.c with NODROIDFONT: the 14 stand in for other fonts) */
unsigned char *pdf_lookup_substitute_font(int mono, int serif, int bold, int italic, unsigned int *len)
{
	static const char *const sub[3][4] =
	{
		{ "Courier", "Courier-Oblique", "Courier-Bold", "Courier-BoldOblique" },
		{ "Times-Roman", "Times-Italic", "Times-Bold", "Times-BoldItalic" },
		{ "Helvetica", "Helvetica-Oblique", "Helvetica-Bold", "Helvetica-BoldOblique" }
	};
	return pdf_lookup_builtin_font((char *)sub[mono ? 0 : serif ? 1 : 2][(bold ? 2 : 0) + (italic ? 1 : 0)], len);
}

unsigned char *pdf_lookup_substitute_cjk_font(int ros, int serif, unsigned int *len)
{
	*len = 0;
	return NULL;
}

/* FreeType calls this as a face on one of the slots goes */
static void face_gone(void *object)
{
	FT_Face face = (FT_Face)object;
	pp_slot *s = (pp_slot *)face->generic.data;
	/* (not freed here: FreeType is still taking the face apart) */
	if (s && s >= pp_slots && s < pp_slots + PP_FONTS && s->refs > 0)
		s->refs--;
	face->generic.data = NULL;
	face->generic.finalizer = NULL;
}

/* pdf-font.c is built with fz_new_font_from_memory named this (Makefile):
   a font made on one of the slots counts as a user of it until FreeType
   lets its face go. Embedded fonts and the spare pass straight through. */
fz_font *pp_new_font_from_memory(fz_context *ctx, char *name, unsigned char *data, int len, int index, int use_glyph_bbox)
{
	fz_font *font = NULL;
	pp_slot *s = NULL;
	int i;
	for (i = 0; i < PP_FONTS; i++)
		if (data && pp_slots[i].data == data)
			s = &pp_slots[i];
	if (!s)
		return fz_new_font_from_memory(ctx, name, data, len, index, use_glyph_bbox);
	s->refs++;                      /* (held while the face is made) */
	fz_var(font);
	fz_try(ctx)
	{
		font = fz_new_font_from_memory(ctx, name, data, len, index, use_glyph_bbox);
	}
	fz_catch(ctx)
	{
		s->refs--;
		pp_font_sweep();
		fz_rethrow(ctx);
	}
	{
		FT_Face face = (FT_Face)font->ft_face;
		face->generic.data = s;
		face->generic.finalizer = face_gone;
	}
	return font;
}
