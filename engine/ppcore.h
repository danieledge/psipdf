/* ppcore.h - PsiPDF's engine, the portable part: MuPDF 1.3 (cut down: see
 * the Makefile) opens a PDF, keeps a display list for the last few pages,
 * draws a page in bands of rows at any scale and hands each finished row,
 * already in the screen's colours (ppquant.c), to the caller. The same C
 * runs on the PC (host/pphost.c, for tests) and in psipdf.exe
 * (ppepoc.cpp).
 *
 * Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#ifndef PPCORE_H
#define PPCORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* results (negative), as the app shows them */
enum
{
	PP_OK = 0,
	PP_E_OPEN = -1,         /* the file could not be read */
	PP_E_DAMAGED = -2,      /* not a PDF, or damaged beyond repair */
	PP_E_PASSWORD = -3,     /* it needs a password (or the one given is wrong) */
	PP_E_MEMORY = -4,       /* not enough memory */
	PP_E_PAGE = -5,         /* the page could not be drawn (partly drawn: see the message) */
	PP_E_STOPPED = -6,      /* stopped by the caller (the cookie's abort) */
	PP_E_ARGS = -7
};

/* the output: 8 = EPOC's 256 colours, one byte a pixel (palette indices);
   4 = 16 greys, two pixels a byte, the first in the low nibble, 0 black
   (an EGray16 bitmap's layout) */
#define PP_DEPTH_COLOUR 8
#define PP_DEPTH_GREY 4

#ifdef __cplusplus
struct pp_doc;          /* (gcc 3.0's C++ crashes on a typedef of an incomplete struct) */
struct pp_quant;
#else
typedef struct pp_doc pp_doc;
typedef struct pp_quant pp_quant;
#endif

/* the platform's clock in milliseconds, and its allocator */
long pp_ms(void);
void *pp_sys_malloc(unsigned int n);
void pp_sys_free(void *p);

/* MuPDF's cookie, shared with whoever may stop a job: abort (set to 1 to
   stop), progress. Laid out as fz_cookie (MuPDF 1.3). */
typedef struct pp_cookie
{
	volatile int abort;
	volatile int progress;
	volatile int progress_max;
	volatile int errors;
	volatile int incomplete_ok;
	volatile int incomplete;
} pp_cookie;

typedef struct pp_stats
{
	long list_ms;           /* the page interpreted into its display list (0: it was cached) */
	long draw_ms;           /* the bands drawn */
	long quant_ms;          /* ...of which the change to the screen's colours */
	long out_ms;            /* ...and the rows handed over (into the bitmap) */
	unsigned long heap_now, heap_peak;   /* what MuPDF holds, and the most it held */
} pp_stats;

/* Opens path (password may be NULL or ""). store: bytes MuPDF may keep of
   decoded pictures and fonts between pages. On failure returns NULL, with
   *err one of PP_E_* and msg (msglen bytes) saying why in English. A file
   that needs a password not given (or a wrong one) comes back open, with
   *err PP_E_PASSWORD: try passwords with pp_authenticate. */
pp_doc *pp_open(const char *path, const char *password, unsigned int store, int *err, char *msg, int msglen);
/* PP_OK (the file can now be read), or PP_E_PASSWORD */
int pp_authenticate(pp_doc *d, const char *password, char *msg, int msglen);
/* it still needs its password */
int pp_locked(pp_doc *d);
void pp_close(pp_doc *d);
int pp_pages(pp_doc *d);
/* the document's title from its information dictionary, UTF-8 ("" if none) */
void pp_title(pp_doc *d, char *buf, int len);

/* page n (from 0): its size in points, as shown (rotation applied) */
int pp_page_size(pp_doc *d, int n, float *w, float *h);

/* the size in pixels page n comes out at the given scale (pixels a point) */
int pp_page_pixels(pp_doc *d, int n, float scale, int *w, int *h);

/* Draws page n at scale into rows of depth bits a pixel, band rows at a
   time: row(user, y, data, bytes) for each y from 0 down, in order. The
   anti-aliasing is MuPDF's 8 bits. Returns PP_OK, or PP_E_* with msg set. */
typedef int (*pp_row_fn)(void *user, int y, const unsigned char *data, int bytes);
int pp_render(pp_doc *d, int n, float scale, int depth, int band,
	pp_row_fn row, void *user, pp_cookie *cookie, pp_stats *st, char *msg, int msglen);

/* Finds needle (UTF-8, any case) on page n: up to max hits, each a rect in
   points on the page as shown (x0, y0, x1, y1 in hits[4 * i...]). Returns
   the count, or PP_E_*. */
int pp_find(pp_doc *d, int n, const char *needle, float *hits, int max, pp_cookie *cookie);

/* The 14 standard PDF fonts are in a file beside the program, read when a
   PDF needs one (ppfont.c): where the file is (before pp_open; NULL: there
   is none), and whether it could not be used since then: 0, or PP_FONT_*
   (Helvetica, kept in the program, was used instead). */
enum
{
	PP_FONT_MISSING = 1,    /* the font file is not there */
	PP_FONT_DAMAGED = 2,    /* it is not PsiPDF's font file, or is damaged */
	PP_FONT_MEMORY = 3      /* (inside ppfont.c only: never returned) */
};
void pp_font_file(const char *path);
int pp_font_trouble(void);

/* Memory is short: what can be made again is given back (the decoded
   pictures and glyphs kept, the display lists of the pages not drawn last,
   standard fonts no longer used). The next page costs its time again. */
void pp_trim(pp_doc *d);

/* what MuPDF holds now (bytes), and its peak */
unsigned long pp_heap_now(pp_doc *d);
unsigned long pp_heap_peak(pp_doc *d);
/* the messages MuPDF has written lately (warnings), for the log */
const char *pp_messages(void);

/* ---- ppquant.c: RGBA rows to the screen's colours ---- */

/* w pixels a row, depth PP_DEPTH_*; NULL without memory */
pp_quant *pp_quant_new(int w, int depth);
void pp_quant_free(pp_quant *q);
/* the bytes of one output row */
int pp_quant_bytes(pp_quant *q);
/* one row y (rows must come in order) of RGBA (MuPDF 1.3's pixmaps) to out */
void pp_quant_row(pp_quant *q, int y, const unsigned char *rgba, unsigned char *out);
/* the palette entry of index i as 0xRRGGBB (for the host's pictures) */
unsigned long pp_palette(int i);

#ifdef __cplusplus
}
#endif
#endif
