/* alloctest.c - (0.08) the engine's allocator and picture budget with the
 * sizes a hostile PDF can give: ppcore.c is compiled in whole, so its own
 * pp_malloc, pp_realloc and pp_over_budget run, over a pretend platform
 * allocator that refuses as psipdf.exe's does (PP_SYS_REFUSE) and records
 * what it was asked for.
 *
 *   make TARGET=host alloctest
 *
 * Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#include "../engine/ppcore.c"
#include <stdio.h>
#include <sys/time.h>

long pp_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}
const char *pp_messages(void) { return ""; }

/* the pretend platform: limit as psipdf.exe's gHeapLimit; a block it would
   give that is bigger than REAL is not really allocated (NULL, as if the
   machine were out of memory), so huge sizes can be tried safely */
static long sys_limit;
static unsigned long sys_asked;
static int sys_calls;
#define REAL (1024UL * 1024)
void *pp_sys_malloc(unsigned int n)
{
	sys_calls++;
	sys_asked = n;
	if (PP_SYS_REFUSE(n, sys_limit) || n > REAL)
		return NULL;
	return malloc(n);
}
void pp_sys_free(void *p) { free(p); }
void *pp_sys_realloc(void *p, unsigned int n)
{
	sys_calls++;
	sys_asked = n;
	if (PP_SYS_REFUSE(n, sys_limit) || n > REAL)
		return NULL;
	return realloc(p, n);
}

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void test_malloc(void)
{
	static const unsigned int huge[] = { 0xFFFFFFFFu, 0xFFFFFFF8u, 0xFFFFFFF9u, 0x80000000u,
		600000000u * 4u, 0x40000000u, (unsigned int)PP_BLOCK_MAX + 1 };
	pp_heap h;
	unsigned int i;
	void *p;
	for (i = 0; i < sizeof huge / sizeof huge[0]; i++)
	{
		memset(&h, 0, sizeof h);
		sys_calls = 0;
		p = pp_malloc(&h, huge[i]);
		CHECK(p == NULL);
		CHECK(sys_calls == 0);          /* refused before the size word is added */
		CHECK(h.toobig && h.failed);
		CHECK(h.now == 0);
	}
	/* PP_BLOCK_MAX itself goes to the platform, the size word added without wrapping */
	memset(&h, 0, sizeof h);
	p = pp_malloc(&h, (unsigned int)PP_BLOCK_MAX);
	CHECK(p == NULL && sys_asked == PP_BLOCK_MAX + 2 * sizeof(unsigned long));
	CHECK(h.failed && !h.toobig);
	/* an ordinary block */
	memset(&h, 0, sizeof h);
	p = pp_malloc(&h, 1000);
	CHECK(p != NULL && h.now == 1000 && !h.failed);
	memset(p, 0xAA, 1000);
	/* grown past the cap: refused, the old block kept */
	CHECK(pp_realloc(&h, p, 0xFFFFFFF8u) == NULL && h.toobig && h.now == 1000);
	CHECK(((unsigned char *)p)[999] == 0xAA);
	pp_free(&h, p);
	CHECK(h.now == 0);
}

static void test_realloc(void)
{
	pp_heap h;
	unsigned char *p, *q;
	memset(&h, 0, sizeof h);
	p = (unsigned char *)pp_malloc(&h, 64);
	CHECK(p != NULL);
	memset(p, 7, 64);
	sys_calls = 0;
	q = (unsigned char *)pp_realloc(&h, p, 0xFFFFFFFFu);
	CHECK(q == NULL && sys_calls == 0 && h.toobig && h.now == 64);
	q = (unsigned char *)pp_realloc(&h, p, 0x80000000u);
	CHECK(q == NULL && sys_calls == 0);
	q = (unsigned char *)pp_realloc(&h, p, 4096);
	CHECK(q != NULL && h.now == 4096 && q[63] == 7);
	pp_free(&h, q);
	CHECK(h.now == 0);
}

static void test_refuse(void)
{
	/* psipdf.exe's test, as it was: (TInt)n > gHeapLimit */
	long limit = 16L * 1024 * 1024;
	CHECK(PP_SYS_REFUSE(0x80000000u, limit));      /* passed as a negative TInt */
	CHECK(PP_SYS_REFUSE(0xFFFFFFF8u, limit));
	CHECK(PP_SYS_REFUSE(2400000000u, limit));
	CHECK(PP_SYS_REFUSE(limit + 1, limit));
	CHECK(!PP_SYS_REFUSE(limit, limit));
	CHECK(!PP_SYS_REFUSE(100, limit));
	/* no limit (between commands): the cap still holds */
	CHECK(PP_SYS_REFUSE(0x80000000u, 0));
	CHECK(PP_SYS_REFUSE(0xFFFFFFFFu, 0));
	CHECK(!PP_SYS_REFUSE(24u * 1024 * 1024, 0));
	/* through the pretend platform */
	sys_limit = limit;
	CHECK(pp_sys_malloc(0x80000000u) == NULL);
	sys_limit = 0;
}

static void test_budget(void)
{
	unsigned int b = 2048 * 1024;
	/* the products that 32 bits wrapped */
	CHECK((unsigned int)65536 * 32768u * 2u == 0);  /* (as the old test computed it) */
	CHECK(pp_over_budget(65536, 32768, 1, b));
	CHECK(pp_over_budget(65536, 65536, 1, b));
	CHECK(pp_over_budget(32768, 32768, 3, b));
	CHECK(pp_over_budget(37838, 37838, 3, b));
	CHECK(pp_over_budget(65536, 65536, 32, b));
	CHECK(pp_over_budget(0x7FFFFFFF, 0x7FFFFFFF, 32, b));
	/* the boundary, exactly */
	CHECK(!pp_over_budget(1024, 1024, 1, b));       /* 2 MB: within */
	CHECK(pp_over_budget(1024, 1025, 1, b));
	CHECK(!pp_over_budget(1, 1048576, 1, b));
	CHECK(pp_over_budget(1, 1048577, 1, b));
	CHECK(!pp_over_budget(100, 100, 3, b));
	CHECK(!pp_over_budget(0, 100, 3, b));
	CHECK(!pp_over_budget(100, -5, 3, b));
	/* the reduction the loop in pp_get_pixmap now chooses for 65536 x 32768 */
	{
		int k = 0;
		while (k < 8 && pp_over_budget(65536 >> k, 32768 >> k, 1, b))
			k++;
		CHECK(k == 6);                          /* 1024 x 512 x 2 = 1 MB (2048 x 1024 x 2 is 4) */
	}
}

int main(void)
{
	test_malloc();
	test_realloc();
	test_refuse();
	test_budget();
	printf(fails ? "alloctest: %d FAILED\n" : "alloctest: all passed\n", fails);
	return fails != 0;
}
