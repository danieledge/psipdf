/* ppcore.c - see ppcore.h.
 *
 * Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include "ppcore.h"
#include "ppfont.h"
#include "ppfont.h"

/* pages kept loaded, each with its display list: the one shown, the next
   (drawn ahead) and the one before */
#define PP_CACHE 3

typedef struct pp_heap
{
	unsigned long now, peak;
	int failed;             /* an allocation failed since this was cleared */
} pp_heap;

typedef struct pp_cached
{
	int n;                  /* the page, or -1 */
	pdf_page *page;
	fz_display_list *list;  /* NULL until first drawn */
	fz_rect bounds;
	unsigned long used;     /* for least-recently-used */
} pp_cached;

struct pp_doc
{
	pp_heap heap;
	fz_alloc_context alloc;
	fz_context *ctx;
	pdf_document *doc;
	int pages;
	int locked;             /* it needs a password not given yet */
	pp_cached cache[PP_CACHE];
	unsigned long clock;
	int drawn;              /* the page drawn last (its pictures in the store), or -1 */
};

/* ---- the counted allocator: a size word before each block ---- */

static void *pp_malloc(void *user, unsigned int size)
{
	pp_heap *h = (pp_heap *)user;
	unsigned long *p = (unsigned long *)pp_sys_malloc(size + 2 * sizeof(unsigned long));
	if (!p)
	{
		h->failed = 1;
		return NULL;
	}
	p[0] = size;
	h->now += size;
	if (h->now > h->peak)
		h->peak = h->now;
	return p + 2;
}

static void pp_free(void *user, void *ptr)
{
	pp_heap *h = (pp_heap *)user;
	unsigned long *p;
	if (!ptr)
		return;
	p = (unsigned long *)ptr - 2;
	h->now -= p[0];
	pp_sys_free(p);
}

static void *pp_realloc(void *user, void *old, unsigned int size)
{
	unsigned long *p;
	void *q;
	unsigned int had;
	if (!old)
		return pp_malloc(user, size);
	if (size == 0)
	{
		pp_free(user, old);
		return NULL;
	}
	/* (0.06) the system's own reallocation, which grows a block where it
	   is when it can: a copy every time made a 3 MB stream need 6 to 9 MB */
	p = (unsigned long *)old - 2;
	had = (unsigned int)p[0];
	q = pp_sys_realloc(p, size + 2 * sizeof(unsigned long));
	if (!q)
	{
		((pp_heap *)user)->failed = 1;
		return NULL;
	}
	p = (unsigned long *)q;
	p[0] = size;
	{
		pp_heap *h = (pp_heap *)user;
		h->now += size;
		h->now -= had;
		if (h->now > h->peak)
			h->peak = h->now;
	}
	return p + 2;
}

static void copy_msg(char *msg, int len, const char *s)
{
	int i = 0;
	if (!msg || len <= 0)
		return;
	if (!s)
		s = "";
	while (s[i] && i < len - 1)
	{
		msg[i] = s[i];
		i++;
	}
	msg[i] = 0;
}

/* a caught MuPDF error as a PP_E_ code: memory if any allocation failed or
   the message says so */
static int caught(pp_doc *d, int otherwise, char *msg, int len)
{
	const char *m = fz_caught_message(d->ctx);
	copy_msg(msg, len, m && *m ? m : "failed");
	if (d->heap.failed || (m && (strstr(m, "malloc") || strstr(m, "out of memory") || strstr(m, "cannot allocate"))))
		return PP_E_MEMORY;
	return otherwise;
}

/* ---- the line splitter: between the interpreter and the display list ----
 *
 * A PDF's text object often holds a whole page of lines in one font, and
 * MuPDF 1.3 keeps it as one fz_text, so its box in the display list is the
 * whole page and every band draws (or at least places, in soft floats)
 * every glyph on the page. This device passes everything on to the list
 * device unchanged, except filled, stroked and invisible text, which it
 * passes on a line at a time (a run of glyphs on one baseline), each with
 * its own box: a band then draws only the lines it crosses. (Clipping text
 * is passed whole: a clip made of several parts would not be the same.)
 */

static fz_device *target(fz_device *dev) { return (fz_device *)dev->user; }

/* ---- pictures: decoded no bigger than the store can keep ----
 *
 * MuPDF decodes a picture at the largest power-of-two reduction that is
 * still at least the size it is drawn (image.c), so at 100% and above a
 * scanned page or a photo is decoded at full size: for a 1237 x 1280 JPEG
 * its samples and its RGBA pixmap at once, 11 MB. That is more than the
 * store keeps (store.c refuses an item bigger than its maximum), so it is
 * decoded again for every band, and on the 5mx the allocation fails and
 * the list player swallows the error (the picture is left blank). Each
 * picture is asked for at a reduction whose RGBA pixmap fits in
 * pp_image_budget (the store's size): drawn bigger, it is scaled up.
 */
static unsigned int pp_image_budget = 2048 * 1024;

/*
 * (0.06) MuPDF reduces only a JPEG as it decodes it; any other picture (a
 * fax or Flate scan) it decodes whole and reduces after: a 300 dpi A4 scan
 * took 17 MB (1 bit a pixel) or 26 MB (8 bits) for a picture drawn at
 * 620 x 877. Those are decoded here a few rows at a time instead, each strip
 * reduced as it comes, so no more than the reduced picture and one strip is
 * held. Kept in the store like MuPDF's own (under a key of their own).
 */
typedef struct pp_image_key
{
	int refs;
	fz_image *image;
	int l2factor;
} pp_image_key;

static int pp_hash_image_key(fz_store_hash *hash, void *key_)
{
	pp_image_key *key = (pp_image_key *)key_;
	hash->u.pi.ptr = key->image;
	hash->u.pi.i = key->l2factor + 256;     /* (never one of MuPDF's: 0 to 8) */
	return 1;
}

static void *pp_keep_image_key(fz_context *ctx, void *key_)
{
	pp_image_key *key = (pp_image_key *)key_;
	fz_lock(ctx, FZ_LOCK_ALLOC);
	key->refs++;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	return key;
}

static void pp_drop_image_key(fz_context *ctx, void *key_)
{
	pp_image_key *key = (pp_image_key *)key_;
	int drop;
	if (!key)
		return;
	fz_lock(ctx, FZ_LOCK_ALLOC);
	drop = --key->refs;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	if (drop == 0)
	{
		fz_drop_image(ctx, key->image);
		fz_free(ctx, key);
	}
}

static int pp_cmp_image_key(void *k0_, void *k1_)
{
	pp_image_key *k0 = (pp_image_key *)k0_, *k1 = (pp_image_key *)k1_;
	return k0->image == k1->image && k0->l2factor == k1->l2factor;
}

#ifndef NDEBUG
static void pp_debug_image(FILE *out, void *key_)
{
	pp_image_key *key = (pp_image_key *)key_;
	fprintf(out, "(psipdf image %d x %d sf=%d) ", key->image->w, key->image->h, key->l2factor);
}
#endif

static fz_store_type pp_image_store_type =
{
	pp_hash_image_key,
	pp_keep_image_key,
	pp_drop_image_key,
	pp_cmp_image_key,
#ifndef NDEBUG
	pp_debug_image
#endif
};

/* rows of im at a reduction of 2^l2factor, a strip of 2^l2factor rows at a time */
static fz_pixmap *pp_decode_reduced(fz_context *ctx, fz_image *im, int l2factor)
{
	fz_stream *volatile stm = NULL;
	fz_pixmap *volatile out = NULL;
	fz_pixmap *volatile tile = NULL;
	int f = 1 << l2factor;
	int ow = (im->w + f - 1) >> l2factor, oh = (im->h + f - 1) >> l2factor;
	int native = 0;
	int indexed = fz_colorspace_is_indexed(im->colorspace);
	fz_try(ctx)
	{
		int y, oy = 0;
		stm = fz_open_image_decomp_stream(ctx, im->buffer, &native);
		for (y = 0; y < im->h; y += f, oy++)
		{
			fz_image strip = *im;
			int bytes;
			strip.h = im->h - y < f ? im->h - y : f;
			/* (it closes the stream it is given: a second reference) */
			tile = fz_decomp_image_from_stream(ctx, fz_keep_stream(stm), &strip, 0, indexed, l2factor, 0);
			if (!out)
			{
				out = fz_new_pixmap(ctx, tile->colorspace, ow, oh);
				out->interpolate = im->interpolate;
			}
			if (tile->w != ow || tile->h != 1 || tile->n != out->n)
				fz_throw(ctx, FZ_ERROR_GENERIC, "picture strip %d x %d, not %d x 1", tile->w, tile->h, ow);
			bytes = ow * out->n;
			memcpy(out->samples + oy * bytes, tile->samples, bytes);
			fz_drop_pixmap(ctx, tile);
			tile = NULL;
		}
	}
	fz_always(ctx)
	{
		fz_close(stm);
		fz_drop_pixmap(ctx, tile);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, out);
		fz_rethrow(ctx);
	}
	return out;
}

/* a picture MuPDF would decode whole, reduced as it is decoded (above) */
static fz_pixmap *pp_get_reduced(fz_context *ctx, fz_image *im, int w, int h)
{
	pp_image_key key, *keyp = NULL;
	fz_pixmap *tile, *existing;
	int l2factor;
	if (w > im->w)
		w = im->w;
	if (h > im->h)
		h = im->h;
	/* (as fz_image_get_pixmap chooses it) */
	for (l2factor = 0; im->w >> (l2factor + 1) >= w && im->h >> (l2factor + 1) >= h && l2factor < 8; l2factor++);
	if (l2factor == 0)
		return fz_image_get_pixmap(ctx, im, w, h);
	key.refs = 1;
	key.image = im;
	key.l2factor = l2factor;
	tile = fz_find_item(ctx, fz_free_pixmap_imp, &key, &pp_image_store_type);
	if (tile)
		return tile;
	tile = pp_decode_reduced(ctx, im, l2factor);
	fz_var(keyp);
	fz_try(ctx)
	{
		keyp = fz_malloc_struct(ctx, pp_image_key);
		keyp->refs = 1;
		keyp->image = fz_keep_image(ctx, im);
		keyp->l2factor = l2factor;
		existing = fz_store_item(ctx, keyp, tile, fz_pixmap_size(ctx, tile), &pp_image_store_type);
		if (existing)
		{
			fz_drop_pixmap(ctx, tile);
			tile = existing;
		}
	}
	fz_always(ctx)
	{
		pp_drop_image_key(ctx, keyp);
	}
	fz_catch(ctx)
	{
		/* (not kept: it is still drawn) */
	}
	return tile;
}

static fz_pixmap *pp_get_pixmap(fz_context *ctx, fz_image *im, int w, int h)
{
	int k = 0;
	while (k < 8 && (unsigned int)(im->w >> k) * (unsigned int)(im->h >> k) * (unsigned int)(im->n + 1) > pp_image_budget)
		k++;
	if (k > 0)
	{
		int cw = im->w >> k, ch = im->h >> k;
		int type = im->buffer ? im->buffer->params.type : FZ_IMAGE_UNKNOWN;
		if (w > cw) w = cw > 0 ? cw : 1;
		if (h > ch) h = ch > 0 ? ch : 1;
		/* (a JPEG is reduced by its decoder; PNG and TIFF are not in a PDF) */
		if (im->buffer && type != FZ_IMAGE_JPEG && type != FZ_IMAGE_PNG && type != FZ_IMAGE_TIFF &&
			type != FZ_IMAGE_JPX && type != FZ_IMAGE_JBIG2)
			return pp_get_reduced(ctx, im, w, h);
	}
	return fz_image_get_pixmap(ctx, im, w, h);
}

static void pp_budget_image(fz_image *im)
{
	for (; im; im = im->mask)
		if (im->get_pixmap == fz_image_get_pixmap)
			im->get_pixmap = pp_get_pixmap;
}

static void split_text(fz_device *dev, fz_text *text, int how, fz_stroke_state *stroke,
	const fz_matrix *ctm, fz_colorspace *cs, float *color, float alpha)
{
	fz_context *ctx = dev->ctx;
	fz_device *t = target(dev);
	int i = 0;
	if (text->len < 8 || text->wmode)
	{
		if (how == 0) fz_fill_text(t, text, ctm, cs, color, alpha);
		else if (how == 1) fz_stroke_text(t, text, stroke, ctm, cs, color, alpha);
		else fz_ignore_text(t, text, ctm);
		return;
	}
	while (i < text->len)
	{
		fz_text *volatile part = NULL;
		int j = i + 1, k;
		float y = text->items[i].y;
		while (j < text->len && j - i < 256 && fabsf(text->items[j].y - y) < 0.01f)
			j++;
		fz_try(ctx)
		{
			part = fz_new_text(ctx, text->font, &text->trm, text->wmode);
			for (k = i; k < j; k++)
				fz_add_text(ctx, part, text->items[k].gid, text->items[k].ucs, text->items[k].x, text->items[k].y);
			if (how == 0) fz_fill_text(t, part, ctm, cs, color, alpha);
			else if (how == 1) fz_stroke_text(t, part, stroke, ctm, cs, color, alpha);
			else fz_ignore_text(t, part, ctm);
		}
		fz_always(ctx)
		{
			if (part)
				fz_free_text(ctx, part);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
		i = j;
	}
}

static void s_fill_text(fz_device *d, fz_text *x, const fz_matrix *m, fz_colorspace *cs, float *c, float a) { split_text(d, x, 0, NULL, m, cs, c, a); }
static void s_stroke_text(fz_device *d, fz_text *x, fz_stroke_state *st, const fz_matrix *m, fz_colorspace *cs, float *c, float a) { split_text(d, x, 1, st, m, cs, c, a); }
static void s_ignore_text(fz_device *d, fz_text *x, const fz_matrix *m) { split_text(d, x, 2, NULL, m, NULL, NULL, 0); }
static void s_begin_page(fz_device *d, const fz_rect *r, const fz_matrix *m) { fz_begin_page(target(d), r, m); }
static void s_end_page(fz_device *d) { fz_end_page(target(d)); }
static void s_fill_path(fz_device *d, fz_path *p, int eo, const fz_matrix *m, fz_colorspace *cs, float *c, float a) { fz_fill_path(target(d), p, eo, m, cs, c, a); }
static void s_stroke_path(fz_device *d, fz_path *p, fz_stroke_state *st, const fz_matrix *m, fz_colorspace *cs, float *c, float a) { fz_stroke_path(target(d), p, st, m, cs, c, a); }
static void s_clip_path(fz_device *d, fz_path *p, const fz_rect *r, int eo, const fz_matrix *m) { fz_clip_path(target(d), p, r, eo, m); }
static void s_clip_stroke_path(fz_device *d, fz_path *p, const fz_rect *r, fz_stroke_state *st, const fz_matrix *m) { fz_clip_stroke_path(target(d), p, r, st, m); }
static void s_clip_text(fz_device *d, fz_text *x, const fz_matrix *m, int acc) { fz_clip_text(target(d), x, m, acc); }
static void s_clip_stroke_text(fz_device *d, fz_text *x, fz_stroke_state *st, const fz_matrix *m) { fz_clip_stroke_text(target(d), x, st, m); }
static void s_fill_shade(fz_device *d, fz_shade *sh, const fz_matrix *m, float a) { fz_fill_shade(target(d), sh, m, a); }
static void s_fill_image(fz_device *d, fz_image *im, const fz_matrix *m, float a) { pp_budget_image(im); fz_fill_image(target(d), im, m, a); }
static void s_fill_image_mask(fz_device *d, fz_image *im, const fz_matrix *m, fz_colorspace *cs, float *c, float a) { pp_budget_image(im); fz_fill_image_mask(target(d), im, m, cs, c, a); }
static void s_clip_image_mask(fz_device *d, fz_image *im, const fz_rect *r, const fz_matrix *m) { pp_budget_image(im); fz_clip_image_mask(target(d), im, r, m); }
static void s_pop_clip(fz_device *d) { fz_pop_clip(target(d)); }
static void s_begin_mask(fz_device *d, const fz_rect *r, int lum, fz_colorspace *cs, float *bc) { fz_begin_mask(target(d), r, lum, cs, bc); }
static void s_end_mask(fz_device *d) { fz_end_mask(target(d)); }
static void s_begin_group(fz_device *d, const fz_rect *r, int iso, int ko, int bm, float a) { fz_begin_group(target(d), r, iso, ko, bm, a); }
static void s_end_group(fz_device *d) { fz_end_group(target(d)); }
static int s_begin_tile(fz_device *d, const fz_rect *ar, const fz_rect *v, float xs, float ys, const fz_matrix *m, int id) { return fz_begin_tile_id(target(d), ar, v, xs, ys, m, id); }
static void s_end_tile(fz_device *d) { fz_end_tile(target(d)); }

static fz_device *new_split_device(fz_context *ctx, fz_device *to)
{
	fz_device *dev = fz_new_device(ctx, to);
	dev->hints = to->hints;
	dev->flags = to->flags;
	dev->begin_page = s_begin_page;
	dev->end_page = s_end_page;
	dev->fill_path = s_fill_path;
	dev->stroke_path = s_stroke_path;
	dev->clip_path = s_clip_path;
	dev->clip_stroke_path = s_clip_stroke_path;
	dev->fill_text = s_fill_text;
	dev->stroke_text = s_stroke_text;
	dev->clip_text = s_clip_text;
	dev->clip_stroke_text = s_clip_stroke_text;
	dev->ignore_text = s_ignore_text;
	dev->fill_shade = s_fill_shade;
	dev->fill_image = s_fill_image;
	dev->fill_image_mask = s_fill_image_mask;
	dev->clip_image_mask = s_clip_image_mask;
	dev->pop_clip = s_pop_clip;
	dev->begin_mask = s_begin_mask;
	dev->end_mask = s_end_mask;
	dev->begin_group = s_begin_group;
	dev->end_group = s_end_group;
	dev->begin_tile = s_begin_tile;
	dev->end_tile = s_end_tile;
	return dev;
}

/* ---- pictures of other pages: out of the store ----
 *
 * The store keeps a page's decoded pictures, so each band, a new zoom and
 * Find do not decode them again. Kept after the page, a full-page photo
 * (2.2 MB decoded) stays while the next one is decoded beside it: the
 * engine's peak on a photo document was 7.4 MB. Before a page other than
 * the last one drawn, its decoded pictures go (fonts and the rest stay: the
 * display lists still use them, and loaded again they would be there twice).
 * MuPDF 1.3 has no call for this, and its fz_remove_item does not take the
 * item out of the store's size: the store's layout (store.c, MuPDF 1.3,
 * which fetch.sh pins by its checksum) is repeated here, and each item is
 * taken out as store.c's evict() does it.
 */
typedef struct pp_item
{
	void *key;
	fz_storable *val;
	unsigned int size;
	struct pp_item *next;
	struct pp_item *prev;
	fz_store *store;
	fz_store_type *type;
} pp_item;

typedef struct pp_store
{
	int refs;
	pp_item *head;
	pp_item *tail;
	fz_hash_table *hash;
	unsigned int max;
	unsigned int size;
} pp_store;

static void drop_pictures(fz_context *ctx)
{
	pp_store *store = (pp_store *)ctx->store;
	pp_item *item;
	if (!store)
		return;
	fz_lock(ctx, FZ_LOCK_ALLOC);
	for (item = store->head; item; )
	{
		int drop;
		if (item->val->free != fz_free_pixmap_imp)
		{
			item = item->next;
			continue;
		}
		store->size -= item->size;
		if (item->next)
			item->next->prev = item->prev;
		else
			store->tail = item->prev;
		if (item->prev)
			item->prev->next = item->next;
		else
			store->head = item->next;
		drop = item->val->refs > 0 && --item->val->refs == 0;
		if (item->type->make_hash_key)
		{
			fz_store_hash hash = { NULL };
			hash.free = item->val->free;
			if (item->type->make_hash_key(&hash, item->key))
				fz_hash_remove(ctx, store->hash, &hash);
		}
		fz_unlock(ctx, FZ_LOCK_ALLOC);
		if (drop)
			item->val->free(ctx, item->val);
		item->type->drop_key(ctx, item->key);
		fz_free(ctx, item);
		fz_lock(ctx, FZ_LOCK_ALLOC);
		item = store->head;          /* (from the start: the list may have changed) */
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);
}

/* ---- the document ---- */

static int count_pages(pp_doc *d, int *err, char *msg, int msglen);

pp_doc *pp_open(const char *path, const char *password, unsigned int store, int *err, char *msg, int msglen)
{
	pp_doc *d = (pp_doc *)pp_sys_malloc(sizeof(pp_doc));
	int i;
	*err = PP_OK;
	copy_msg(msg, msglen, "");
	if (!d)
	{
		*err = PP_E_MEMORY;
		copy_msg(msg, msglen, "no memory");
		return NULL;
	}
	memset(d, 0, sizeof *d);
	{
		/* (a file that cannot be read at all, told apart from a damaged one) */
		FILE *f = fopen(path, "rb");
		if (!f)
		{
			pp_sys_free(d);
			*err = PP_E_OPEN;
			copy_msg(msg, msglen, "cannot open the file");
			return NULL;
		}
		fclose(f);
	}
	for (i = 0; i < PP_CACHE; i++)
		d->cache[i].n = -1;
	d->drawn = -1;
	d->alloc.user = &d->heap;
	d->alloc.malloc = pp_malloc;
	d->alloc.realloc = pp_realloc;
	d->alloc.free = pp_free;
	d->ctx = fz_new_context(&d->alloc, NULL, store);
	if (!d->ctx)
	{
		pp_sys_free(d);
		*err = PP_E_MEMORY;
		copy_msg(msg, msglen, "no memory for MuPDF");
		return NULL;
	}
	fz_set_aa_level(d->ctx, 8);
	pp_image_budget = store > 512 * 1024 ? store : 512 * 1024;
	fz_try(d->ctx)
	{
		d->doc = pdf_open_document(d->ctx, path);
	}
	fz_catch(d->ctx)
	{
		const char *m = fz_caught_message(d->ctx);
		*err = d->heap.failed ? PP_E_MEMORY : (m && strstr(m, "cannot open")) ? PP_E_OPEN : PP_E_DAMAGED;
		copy_msg(msg, msglen, m);
		fz_free_context(d->ctx);
		pp_font_reset();
		pp_sys_free(d);
		return NULL;
	}
	if (pdf_needs_password(d->doc) && !pdf_authenticate_password(d->doc, password ? password : ""))
	{
		/* (kept open: the caller asks for the password and tries it with
		   pp_authenticate, without opening the file again) */
		*err = PP_E_PASSWORD;
		copy_msg(msg, msglen, password && *password ? "wrong password" : "needs a password");
		d->locked = 1;
		return d;
	}
	if (count_pages(d, err, msg, msglen) != PP_OK)
	{
		pdf_close_document(d->doc);
		fz_free_context(d->ctx);
		pp_font_reset();
		pp_sys_free(d);
		return NULL;
	}
	return d;
}

static int count_pages(pp_doc *d, int *err, char *msg, int msglen)
{
	fz_try(d->ctx)
	{
		d->pages = pdf_count_pages(d->doc);
	}
	fz_catch(d->ctx)
	{
		d->pages = 0;
	}
	if (d->pages <= 0)
	{
		*err = PP_E_DAMAGED;
		copy_msg(msg, msglen, "no pages found");
		return PP_E_DAMAGED;
	}
	return PP_OK;
}

int pp_authenticate(pp_doc *d, const char *password, char *msg, int msglen)
{
	int err = PP_OK;
	if (!d->locked)
		return PP_OK;
	if (!pdf_authenticate_password(d->doc, password ? password : ""))
	{
		copy_msg(msg, msglen, "wrong password");
		return PP_E_PASSWORD;
	}
	d->locked = 0;
	return count_pages(d, &err, msg, msglen);
}

int pp_locked(pp_doc *d)
{
	return d->locked;
}

static void drop_cached(pp_doc *d, pp_cached *c)
{
	if (c->list)
		fz_drop_display_list(d->ctx, c->list);
	if (c->page)
		pdf_free_page(d->doc, c->page);
	c->list = NULL;
	c->page = NULL;
	c->n = -1;
}

void pp_close(pp_doc *d)
{
	int i;
	if (!d)
		return;
	for (i = 0; i < PP_CACHE; i++)
		drop_cached(d, &d->cache[i]);
	pdf_close_document(d->doc);
	fz_free_context(d->ctx);
	pp_font_reset();          /* (the standard fonts read for this file) */
	pp_sys_free(d);
}

void pp_trim(pp_doc *d)
{
	int i;
	if (!d || d->locked)
		return;
	for (i = 0; i < PP_CACHE; i++)
		if (d->cache[i].n != d->drawn)
			drop_cached(d, &d->cache[i]);
	drop_pictures(d->ctx);
	fz_purge_glyph_cache(d->ctx);
	pp_font_sweep();
}

int pp_pages(pp_doc *d)
{
	return d->pages;
}

void pp_title(pp_doc *d, char *buf, int len)
{
	char *s = NULL;
	copy_msg(buf, len, "");
	fz_var(s);
	fz_try(d->ctx)
	{
		pdf_obj *info = pdf_dict_gets(pdf_trailer(d->doc), "Info");
		pdf_obj *t = info ? pdf_dict_gets(info, "Title") : NULL;
		if (t)
			s = pdf_to_utf8(d->doc, t);
		copy_msg(buf, len, s);
	}
	fz_always(d->ctx)
	{
		fz_free(d->ctx, s);
	}
	fz_catch(d->ctx)
	{
		copy_msg(buf, len, "");
	}
}

/* (with the standard fonts read from the font file, which are not MuPDF's) */
unsigned long pp_heap_now(pp_doc *d) { return d->heap.now + pp_font_bytes(); }
unsigned long pp_heap_peak(pp_doc *d) { return d->heap.peak + pp_font_bytes(); }

/* page n loaded (from the cache, or into its least-recently-used slot) */
static pp_cached *get_page(pp_doc *d, int n)
{
	int i, old = 0;
	pp_cached *c;
	for (i = 0; i < PP_CACHE; i++)
		if (d->cache[i].n == n)
		{
			d->cache[i].used = ++d->clock;
			return &d->cache[i];
		}
	for (i = 1; i < PP_CACHE; i++)
		if (d->cache[i].used < d->cache[old].used)
			old = i;
	c = &d->cache[old];
	drop_cached(d, c);
	c->page = pdf_load_page(d->doc, n);
	c->n = n;
	c->used = ++d->clock;
	pdf_bound_page(d->doc, c->page, &c->bounds);
	return c;
}

int pp_page_size(pp_doc *d, int n, float *w, float *h)
{
	int r = PP_OK;
	if (n < 0 || n >= d->pages)
		return PP_E_ARGS;
	d->heap.failed = 0;
	fz_try(d->ctx)
	{
		pp_cached *c = get_page(d, n);
		*w = c->bounds.x1 - c->bounds.x0;
		*h = c->bounds.y1 - c->bounds.y0;
	}
	fz_catch(d->ctx)
	{
		r = caught(d, PP_E_PAGE, NULL, 0);
	}
	return r;
}

static void page_box(pp_cached *c, float scale, fz_irect *box)
{
	fz_matrix ctm;
	fz_rect r = c->bounds;
	fz_scale(&ctm, scale, scale);
	fz_transform_rect(&r, &ctm);
	fz_round_rect(box, &r);
}

int pp_page_pixels(pp_doc *d, int n, float scale, int *w, int *h)
{
	int r = PP_OK;
	if (n < 0 || n >= d->pages || scale <= 0)
		return PP_E_ARGS;
	fz_try(d->ctx)
	{
		fz_irect box;
		page_box(get_page(d, n), scale, &box);
		*w = box.x1 - box.x0;
		*h = box.y1 - box.y0;
	}
	fz_catch(d->ctx)
	{
		r = caught(d, PP_E_PAGE, NULL, 0);
	}
	return r;
}

/* the page's display list, made the first time (the slow part: the content
   stream interpreted, fonts and pictures loaded) */
static fz_display_list *get_list(pp_doc *d, pp_cached *c, pp_cookie *cookie)
{
	fz_device *dev = NULL, *ldev = NULL;
	fz_display_list *list;
	if (c->list)
		return c->list;
	list = fz_new_display_list(d->ctx);
	fz_var(dev);
	fz_var(ldev);
	fz_try(d->ctx)
	{
		ldev = fz_new_list_device(d->ctx, list);
		dev = new_split_device(d->ctx, ldev);
		/* (the page's start and end in the list: the text device, for Find,
		   puts its lines together at the end) */
		fz_begin_page(dev, &c->bounds, &fz_identity);
		pdf_run_page(d->doc, c->page, dev, &fz_identity, (fz_cookie *)cookie);
		fz_end_page(dev);
	}
	fz_always(d->ctx)
	{
		if (dev)
			fz_free_device(dev);
		if (ldev)
			fz_free_device(ldev);
	}
	fz_catch(d->ctx)
	{
		fz_drop_display_list(d->ctx, list);
		fz_rethrow(d->ctx);
	}
	if (cookie && cookie->abort)
	{
		/* (a list cut short is not kept) */
		fz_drop_display_list(d->ctx, list);
		fz_throw(d->ctx, FZ_ERROR_GENERIC, "stopped");
	}
	c->list = list;
	return list;
}

int pp_render(pp_doc *d, int n, float scale, int depth, int band,
	pp_row_fn row, void *user, pp_cookie *cookie, pp_stats *st, char *msg, int msglen)
{
	fz_context *ctx = d->ctx;
	fz_pixmap *volatile pix = NULL;
	fz_device *volatile dev = NULL;
	pp_quant *volatile q = NULL;
	unsigned char *volatile out = NULL;
	int r = PP_OK;
	long t0;
	pp_stats dummy;
	if (!st)
		st = &dummy;
	memset(st, 0, sizeof *st);
	copy_msg(msg, msglen, "");
	if (n < 0 || n >= d->pages || scale <= 0 || band <= 0)
		return PP_E_ARGS;
	d->heap.failed = 0;
	if (cookie)
		cookie->errors = 0;
	fz_try(ctx)
	{
		pp_cached *c;
		fz_display_list *list;
		fz_matrix ctm;
		fz_irect box;
		int y, w, h, k;
		if (d->drawn != n)
		{
			drop_pictures(ctx);
			d->drawn = n;
		}
		c = get_page(d, n);
		t0 = pp_ms();
		if (!c->list)
		{
			list = get_list(d, c, cookie);
			st->list_ms = pp_ms() - t0;
		}
		else
			list = c->list;
		t0 = pp_ms();
		fz_scale(&ctm, scale, scale);
		page_box(c, scale, &box);
		w = box.x1 - box.x0;
		h = box.y1 - box.y0;
		if (w <= 0 || h <= 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "empty page");
		q = pp_quant_new(w, depth);
		if (!q)
			fz_throw(ctx, FZ_ERROR_GENERIC, "malloc failed (rows)");
		/* (a band's rows in the screen's colours, then handed over: the two timed apart) */
		out = (unsigned char *)fz_malloc(ctx, pp_quant_bytes(q) * (h < band ? h : band));
		for (y = 0; y < h; y += band)
		{
			fz_irect bb;
			fz_rect area;
			int bh = h - y < band ? h - y : band;
			long q0;
			if (cookie && cookie->abort)
				fz_throw(ctx, FZ_ERROR_GENERIC, "stopped");
			bb.x0 = box.x0;
			bb.x1 = box.x1;
			bb.y0 = box.y0 + y;
			bb.y1 = bb.y0 + bh;
			/* (one band's pixmap, made again each time: a short band at the end) */
			if (!pix || fz_pixmap_height(ctx, pix) != bh)
			{
				if (pix)
					fz_drop_pixmap(ctx, pix);
				pix = NULL;
				pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), &bb);
			}
			else
			{
				pix->x = bb.x0;
				pix->y = bb.y0;
			}
			fz_clear_pixmap_with_value(ctx, pix, 0xff);
			area.x0 = (float)bb.x0;
			area.y0 = (float)bb.y0;
			area.x1 = (float)bb.x1;
			area.y1 = (float)bb.y1;
			dev = fz_new_draw_device(ctx, pix);
			fz_run_display_list(list, dev, &ctm, &area, (fz_cookie *)cookie);
			fz_free_device(dev);
			dev = NULL;
			if (cookie && cookie->abort)
				fz_throw(ctx, FZ_ERROR_GENERIC, "stopped");
			q0 = pp_ms();
			for (k = 0; k < bh; k++)
				pp_quant_row(q, y + k, fz_pixmap_samples(ctx, pix) + k * w * 4, out + k * pp_quant_bytes(q));
			st->quant_ms += pp_ms() - q0;
			q0 = pp_ms();
			for (k = 0; k < bh; k++)
				if (row(user, y + k, out + k * pp_quant_bytes(q), pp_quant_bytes(q)) != 0)
					fz_throw(ctx, FZ_ERROR_GENERIC, "stopped");
			st->out_ms += pp_ms() - q0;
		}
		st->draw_ms = pp_ms() - t0;
		/* (fz_run_display_list swallows a failed item, a picture without
		   memory say: the page is all there but for it) */
		if (cookie && cookie->errors)
		{
			copy_msg(msg, msglen, d->heap.failed ? "not enough memory for all of the page" : "part of the page could not be drawn");
			r = d->heap.failed ? PP_E_MEMORY : PP_E_PAGE;
		}
	}
	fz_always(ctx)
	{
		if (dev)
			fz_free_device(dev);
		if (pix)
			fz_drop_pixmap(ctx, pix);
		fz_free(ctx, out);
		pp_quant_free(q);
	}
	fz_catch(ctx)
	{
		if (cookie && cookie->abort)
		{
			copy_msg(msg, msglen, "stopped");
			r = PP_E_STOPPED;
		}
		else
			r = caught(d, PP_E_PAGE, msg, msglen);
	}
	/* (standard fonts the store has let go while drawing) */
	pp_font_sweep();
	st->heap_now = pp_heap_now(d);
	st->heap_peak = pp_heap_peak(d);
	return r;
}

int pp_find(pp_doc *d, int n, const char *needle, float *hits, int max, pp_cookie *cookie)
{
	fz_context *ctx = d->ctx;
	fz_text_sheet *volatile sheet = NULL;
	fz_text_page *volatile text = NULL;
	fz_device *volatile dev = NULL;
	fz_rect *volatile boxes = NULL;
	int count = 0, i;
	if (n < 0 || n >= d->pages || max <= 0)
		return PP_E_ARGS;
	d->heap.failed = 0;
	fz_try(ctx)
	{
		pp_cached *c = get_page(d, n);
		fz_display_list *list = get_list(d, c, cookie);
		boxes = (fz_rect *)fz_malloc(ctx, max * sizeof(fz_rect));
		sheet = fz_new_text_sheet(ctx);
		text = fz_new_text_page(ctx);
		dev = fz_new_text_device(ctx, sheet, text);
		fz_run_display_list(list, dev, &fz_identity, &fz_infinite_rect, (fz_cookie *)cookie);
		fz_free_device(dev);
		dev = NULL;
		count = fz_search_text_page(ctx, text, needle, boxes, max);
		for (i = 0; i < count; i++)
		{
			hits[4 * i] = boxes[i].x0;
			hits[4 * i + 1] = boxes[i].y0;
			hits[4 * i + 2] = boxes[i].x1;
			hits[4 * i + 3] = boxes[i].y1;
		}
	}
	fz_always(ctx)
	{
		if (dev)
			fz_free_device(dev);
		if (text)
			fz_free_text_page(ctx, text);
		if (sheet)
			fz_free_text_sheet(ctx, sheet);
		fz_free(ctx, boxes);
	}
	fz_catch(ctx)
	{
		count = cookie && cookie->abort ? PP_E_STOPPED : caught(d, PP_E_PAGE, NULL, 0);
	}
	pp_font_sweep();
	return count;
}
