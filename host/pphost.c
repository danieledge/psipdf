/* pphost.c - PsiPDF's engine on the PC, for tests: draws pages exactly as
 * psipdf.exe does (the same ppcore.c and ppquant.c), and writes them as
 * PPM pictures in the screen's colours, with the times and heap.
 *
 *   pphost [-w WIDTH | -s SCALE] [-d 8|4] [-b BAND] [-p FIRST[-LAST]]
 *          [-f TEXT] [-P PASSWORD] [-S KB] [-F FONTFILE] [-L KB] OUTPREFIX FILE.pdf
 *
 * -w: fitted to WIDTH pixels (standard 640, the screen); -s: pixels a point;
 * -d 8: EPOC's 256 colours (the netBook), 4: 16 greys (the Series 5mx);
 * -f: find TEXT on each page and print the hits instead; -S: MuPDF's store
 * in KB (standard 1024); -F: the font file (standard: PsiPDF.fnt in
 * ../gen from this program, as the Makefile leaves it; "" for none);
 * -L: no block bigger than KB (as psipdf.exe limits them to the memory free).
 * A font file that could not be used is said ("fonts: missing").
 *
 * Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "ppcore.h"

long pp_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}
/* (0.08) the engine's own refusal (PP_SYS_REFUSE), with -L as the limit
   psipdf.exe sets from the memory free (here a size per block) */
static unsigned long sys_limit;
void *pp_sys_malloc(unsigned int n) { return PP_SYS_REFUSE(n, sys_limit) ? NULL : malloc(n); }
void pp_sys_free(void *p) { free(p); }
void *pp_sys_realloc(void *p, unsigned int n) { return PP_SYS_REFUSE(n, sys_limit) ? NULL : realloc(p, n); }
const char *pp_messages(void) { return ""; }

typedef struct out
{
	FILE *f;
	int depth, w;
	unsigned char *rgb;
} out;

static int row(void *user, int y, const unsigned char *data, int bytes)
{
	out *o = (out *)user;
	int x;
	for (x = 0; x < o->w; x++)
	{
		unsigned long c;
		if (o->depth == PP_DEPTH_GREY)
		{
			int k = (data[x >> 1] >> (x & 1 ? 4 : 0)) & 15;
			c = (unsigned long)(k * 17) * 0x010101UL;
		}
		else
			c = pp_palette(data[x]);
		o->rgb[3 * x] = (unsigned char)(c >> 16);
		o->rgb[3 * x + 1] = (unsigned char)(c >> 8);
		o->rgb[3 * x + 2] = (unsigned char)c;
	}
	fwrite(o->rgb, 3, o->w, o->f);
	return 0;
}

int main(int argc, char **argv)
{
	int width = 640, depth = 8, band = 64, first = 0, last = -1, i, err, store = 1024;
	float scale = 0;
	const char *find = NULL, *pw = NULL, *fonts = NULL;
	char fontpath[600];
	char msg[200];
	pp_doc *d;
	for (i = 1; i + 2 < argc && argv[i][0] == '-'; i += 2)
	{
		const char *v = argv[i + 1];
		switch (argv[i][1])
		{
		case 'w': width = atoi(v); break;
		case 's': scale = (float)atof(v); break;
		case 'd': depth = atoi(v); break;
		case 'b': band = atoi(v); break;
		case 'p': first = atoi(v) - 1; last = strchr(v, '-') ? atoi(strchr(v, '-') + 1) - 1 : first; break;
		case 'f': find = v; break;
		case 'P': pw = v; break;
		case 'S': store = atoi(v); break;
		case 'F': fonts = v; break;
		case 'L': sys_limit = strtoul(v, NULL, 10) * 1024; break;
		default: fprintf(stderr, "unknown option %s\n", argv[i]); return 2;
		}
	}
	if (i + 2 != argc)
	{
		fprintf(stderr, "usage: pphost [-w W | -s S] [-d 8|4] [-b BAND] [-p A[-B]] [-f TEXT] [-P PW] [-S KB] [-F FONTS] [-L KB] OUTPREFIX FILE.pdf\n");
		return 2;
	}
	if (!fonts)
	{
		const char *sl = strrchr(argv[0], '/');
		snprintf(fontpath, sizeof fontpath, "%.*s../gen/PsiPDF.fnt", sl ? (int)(sl - argv[0] + 1) : 0, argv[0]);
		fonts = fontpath;
	}
	pp_font_file(fonts);
	{
		long t = pp_ms();
		d = pp_open(argv[i + 1], NULL, store * 1024u, &err, msg, sizeof msg);
		if (d && err == PP_E_PASSWORD)
		{
			err = pp_authenticate(d, pw, msg, sizeof msg);
			printf("password: %d %s\n", err, err ? msg : "");
			if (err)
			{
				pp_close(d);
				return 1;
			}
		}
		if (!d)
		{
			printf("open: %d %s\n", err, msg);
			return 1;
		}
		printf("open: %d pages, %ld ms\n", pp_pages(d), pp_ms() - t);
	}
	{
		char title[128];
		pp_title(d, title, sizeof title);
		printf("title: \"%s\"\n", title);
	}
	if (last < 0 || last >= pp_pages(d))
		last = last < 0 ? pp_pages(d) - 1 : pp_pages(d) - 1;
	for (; first <= last; first++)
	{
		float pw_, ph_, s = scale;
		int w, h, r;
		if (pp_page_size(d, first, &pw_, &ph_) != PP_OK)
		{
			printf("page %d: no size\n", first + 1);
			continue;
		}
		if (s <= 0)
			s = width / pw_;
		pp_page_pixels(d, first, s, &w, &h);
		if (find)
		{
			float hits[64 * 4];
			int k, n = pp_find(d, first, find, hits, 64, NULL);
			printf("page %d: %d hits for \"%s\"\n", first + 1, n, find);
			for (k = 0; k < n; k++)
				printf("  %.1f %.1f %.1f %.1f\n", hits[4 * k], hits[4 * k + 1], hits[4 * k + 2], hits[4 * k + 3]);
			continue;
		}
		{
			char name[512];
			out o;
			pp_stats st;
			snprintf(name, sizeof name, "%s-%d.ppm", argv[i], first + 1);
			o.f = fopen(name, "wb");
			if (!o.f)
				return 1;
			o.depth = depth;
			o.w = w;
			o.rgb = (unsigned char *)malloc(3 * w);
			fprintf(o.f, "P6\n%d %d\n255\n", w, h);
			r = pp_render(d, first, s, depth, band, row, &o, NULL, &st, msg, sizeof msg);
			fclose(o.f);
			free(o.rgb);
			printf("page %d: %.0f x %.0f pt, %d x %d px: %d %s list %ld ms, draw %ld ms (colours %ld), heap %lu KB (peak %lu KB)\n",
				first + 1, pw_, ph_, w, h, r, r ? msg : "", st.list_ms, st.draw_ms, st.quant_ms,
				st.heap_now / 1024, st.heap_peak / 1024);
		}
	}
	if (pp_font_trouble())
		printf("fonts: %s\n", pp_font_trouble() == PP_FONT_MISSING ? "missing" : "damaged");
	pp_close(d);
	return 0;
}
