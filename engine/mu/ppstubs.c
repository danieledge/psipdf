/* ptstubs.c - what the cut-down MuPDF leaves out, as stubs that fail
   cleanly: JBIG2 and JPEG 2000 pictures, PNG and TIFF pictures (only for
   MuPDF's image documents), and the document kinds other than PDF. */
/* Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#include "mupdf/fitz.h"

fz_stream *fz_open_jbig2d(fz_stream *chain, fz_buffer *globals)
{
	fz_context *ctx = chain->ctx;
	fz_close(chain);
	fz_throw(ctx, FZ_ERROR_GENERIC, "JBIG2 pictures are not supported by PsiPDF");
	return NULL;
}

fz_pixmap *fz_load_jpx(fz_context *ctx, unsigned char *data, int size, fz_colorspace *cs, int indexed)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "JPEG 2000 pictures are not supported by PsiPDF");
	return NULL;
}

fz_pixmap *fz_load_png(fz_context *ctx, unsigned char *data, int size)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "PNG pictures are not supported by PsiPDF");
	return NULL;
}

fz_pixmap *fz_load_tiff(fz_context *ctx, unsigned char *data, int size)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "TIFF pictures are not supported by PsiPDF");
	return NULL;
}

void fz_load_png_info(fz_context *ctx, unsigned char *data, int size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "PNG pictures are not supported by PsiPDF");
}

void fz_load_tiff_info(fz_context *ctx, unsigned char *data, int size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "TIFF pictures are not supported by PsiPDF");
}

/* forms (pdf-field.c, pdf-form.c, pdf-appearance.c): fields are drawn from
   the appearance streams the file has; none are made here */
#include "mupdf/pdf.h"

int pdf_field_type(pdf_document *doc, pdf_obj *field)
{
	return -1;              /* (none of the PDF_WIDGET_TYPE_ kinds) */
}

void pdf_update_appearance(pdf_document *doc, pdf_annot *annot)
{
}

/* writing (pdf-write.c) */
void pdf_write_document(pdf_document *doc, char *filename, fz_write_options *opts)
{
	fz_throw(doc->ctx, FZ_ERROR_GENERIC, "saving is not supported by PsiPDF");
}

/* zlib's compress, for MuPDF's PNG writer (never called here: zlib's
   inflate only is built) */
unsigned long compressBound(unsigned long n)
{
	return n + 64;
}

int compress(unsigned char *dst, unsigned long *dlen, const unsigned char *src, unsigned long slen)
{
	return -2;              /* Z_STREAM_ERROR */
}
