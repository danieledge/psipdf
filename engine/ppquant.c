/* ppquant.c - MuPDF's RGBA rows to the screen's colours (see ppcore.h).
 *
 * Error diffusion (Floyd-Steinberg, serpentine), on a colour screen (the
 * netBook) to the nearest of EPOC's fixed 256 colours (pppal.h: pp_inverse,
 * 5 bits a channel, weighted for the eye), on a grey one (the Series 5mx)
 * to the nearest of 16 greys by brightness. The paper and full black are
 * shown exactly and the error arriving there is dropped, so margins and
 * the space between lines stay plain, with no worms or speckles beside
 * text and headings. The palette's 16 greys are exact steps, so text and
 * its anti-aliased edges stay crisp. (PsiView and PsiWeb dither pictures
 * the same way.)
 *
 * Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#include <stdlib.h>
#include <string.h>
#include "ppcore.h"
#include "pppal.h"

/* paper: every channel at least this (and ink: at most 255 - this) */
#define PP_PAPER 250

struct pp_quant
{
	int w, depth, bytes;
	int *err;               /* two rows of (w + 2) * 3 errors: this row's, the next's */
	int *cur, *nxt;
	int cur_dirty, nxt_dirty;   /* (a row of errors not all 0: cleared before use) */
};

unsigned long pp_palette(int i)
{
	return pp_pal[i & 255];
}

pp_quant *pp_quant_new(int w, int depth)
{
	pp_quant *q = (pp_quant *)pp_sys_malloc(sizeof(pp_quant));
	if (!q)
		return NULL;
	memset(q, 0, sizeof *q);
	q->w = w;
	q->depth = depth;
	q->bytes = depth == PP_DEPTH_GREY ? (w + 1) / 2 : w;
	{
		int n = (w + 2) * 3;
		q->err = (int *)pp_sys_malloc(2 * n * sizeof(int));
		if (!q->err)
		{
			pp_sys_free(q);
			return NULL;
		}
		memset(q->err, 0, 2 * n * sizeof(int));
		q->cur = q->err;
		q->nxt = q->err + n;
	}
	return q;
}

void pp_quant_free(pp_quant *q)
{
	if (!q)
		return;
	if (q->err)
		pp_sys_free(q->err);
	pp_sys_free(q);
}

int pp_quant_bytes(pp_quant *q)
{
	return q->bytes;
}

static int clamp(int v)
{
	return v < 0 ? 0 : v > 255 ? 255 : v;
}

void pp_quant_row(pp_quant *q, int y, const unsigned char *rgba, unsigned char *out)
{
	int w = q->w, x;
	int *cur = q->cur, *nxt = q->nxt, *t;
	int dir = y & 1 ? -1 : 1;
	int grey = q->depth == PP_DEPTH_GREY;
	unsigned char white = pp_grey8[15];
	/* (most rows of a text page are paper and ink only, and carry no
	   error: their row is not cleared again) */
	if (q->nxt_dirty)
		memset(nxt, 0, (w + 2) * 3 * sizeof(int));
	q->nxt_dirty = 0;
	if (grey)
		memset(out, 0, q->bytes);
	for (x = dir > 0 ? 0 : w - 1; x >= 0 && x < w; x += dir)
	{
		const unsigned char *p = rgba + 4 * x;
		int r, g, b;
		if (*(const unsigned int *)p == 0xffffffffu)
		{
			/* (white paper, opaque: most of a page, done at once) */
			if (grey)
				out[x >> 1] |= (unsigned char)(x & 1 ? 0xf0 : 0x0f);
			else
				out[x] = white;
			continue;
		}
		r = p[0]; g = p[1]; b = p[2];
		int *e = cur + 3 * (x + 1);
		int *ahead, *below;
		int er, eg, eb, c;
		if (grey)
		{
			int v = (r * 77 + g * 150 + b * 29) >> 8, k;
			if (v >= PP_PAPER || v <= 255 - PP_PAPER)
			{
				k = v >= PP_PAPER ? 15 : 0;
				er = 0;
			}
			else
			{
				v = clamp(v + e[0] / 16);
				k = (v + 8) / 17;
				er = v - k * 17;
			}
			out[x >> 1] |= (unsigned char)(x & 1 ? k << 4 : k);
			eg = eb = 0;
		}
		else if (r >= PP_PAPER && g >= PP_PAPER && b >= PP_PAPER)
		{
			out[x] = pp_grey8[15];
			continue;
		}
		else if (r <= 255 - PP_PAPER && g <= 255 - PP_PAPER && b <= 255 - PP_PAPER)
		{
			out[x] = pp_grey8[0];
			continue;
		}
		else
		{
			unsigned long pc;
			r = clamp(r + e[0] / 16);
			g = clamp(g + e[1] / 16);
			b = clamp(b + e[2] / 16);
			c = pp_inverse[((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)];
			pc = pp_pal[c];
			out[x] = (unsigned char)c;
			er = r - (int)((pc >> 16) & 255);
			eg = g - (int)((pc >> 8) & 255);
			eb = b - (int)(pc & 255);
		}
		if (!(er | eg | eb))
			continue;
		ahead = e + 3 * dir;
		below = nxt + 3 * (x + 1);
		q->cur_dirty = q->nxt_dirty = 1;
		/* 7/16 ahead; below: 3/16 behind, 5/16 under, 1/16 ahead */
		ahead[0] += er * 7; ahead[1] += eg * 7; ahead[2] += eb * 7;
		below[-3 * dir] += er * 3; below[-3 * dir + 1] += eg * 3; below[-3 * dir + 2] += eb * 3;
		below[0] += er * 5; below[1] += eg * 5; below[2] += eb * 5;
		below[3 * dir] += er; below[3 * dir + 1] += eg; below[3 * dir + 2] += eb;
	}
	t = q->cur;
	q->cur = q->nxt;
	q->nxt = t;
	{
		int d = q->cur_dirty;
		q->cur_dirty = q->nxt_dirty;
		q->nxt_dirty = d;
	}
}
