/* ppcompat.c - see ppcompat.h: single-precision maths over ESTLIB's double
   functions (floor and ceil in integers, as they are called for every
   glyph and edge), and MuPDF's messages kept for PsiPDF's log (pp_messages). */
/* Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
/* (ppcompat.h is force-included before this file, its names already
   defined as macros: undone here, so these reach ESTLIB's and nsprintf's) */
#undef fprintf
#undef vfprintf
#undef fputs
#undef fputc
#undef putc
#undef fflush
#undef snprintf
#undef vsnprintf
#include <math.h>
#include <string.h>

float fabsf(float x) { return x < 0 ? -x : x; }
float floorf(float x)
{
	int i;
	if (x >= 8388608.0f || x <= -8388608.0f)
		return x;               /* (already whole) */
	i = (int)x;
	if ((float)i > x)
		i--;
	return (float)i;
}
float ceilf(float x)
{
	int i;
	if (x >= 8388608.0f || x <= -8388608.0f)
		return x;
	i = (int)x;
	if ((float)i < x)
		i++;
	return (float)i;
}
float sqrtf(float x) { return (float)sqrt(x); }
float sinf(float x) { return (float)sin(x); }
float cosf(float x) { return (float)cos(x); }
float atan2f(float y, float x) { return (float)atan2(y, x); }

/* the messages: the last 2 KB, for the log */
char pp_msgs[2048];
const char *pp_messages(void) { return pp_msgs; }
static int pp_msgn;

static void keep(const char *s)
{
	int n = strlen(s);
	if (n >= (int)sizeof pp_msgs)
		return;
	if (pp_msgn + n >= (int)sizeof pp_msgs)
		pp_msgn = 0;            /* (start again: the latest matter) */
	memcpy(pp_msgs + pp_msgn, s, n + 1);
	pp_msgn += n;
}

int vsnprintf(char *s, size_t n, const char *fmt, va_list ap);
int pp_vsnprintf(char *s, size_t n, const char *fmt, va_list ap) { return vsnprintf(s, n, fmt, ap); }
int pp_snprintf(char *s, size_t n, const char *fmt, ...)
{
	va_list ap;
	int r;
	va_start(ap, fmt);
	r = vsnprintf(s, n, fmt, ap);
	va_end(ap);
	return r;
}

int pp_vfprintf(FILE *f, const char *fmt, va_list ap)
{
	char b[256];
	if (f != stderr && f != stdout)
		return vfprintf(f, fmt, ap);
	vsnprintf(b, sizeof b, fmt, ap);
	keep(b);
	return strlen(b);
}
int pp_fprintf(FILE *f, const char *fmt, ...)
{
	va_list ap;
	int r;
	va_start(ap, fmt);
	r = pp_vfprintf(f, fmt, ap);
	va_end(ap);
	return r;
}
int pp_fputs(const char *s, FILE *f)
{
	if (f != stderr && f != stdout)
		return fputs(s, f);
	keep(s);
	return 0;
}
int pp_fputc(int c, FILE *f)
{
	char b[2];
	if (f != stderr && f != stdout)
		return fputc(c, f);
	b[0] = (char)c; b[1] = 0;
	keep(b);
	return c;
}
int pp_fflush(FILE *f) { return f == stderr || f == stdout ? 0 : fflush(f); }
float powf(float x, float y) { return (float)pow(x, y); }
float logf(float x) { return (float)log(x); }
float log10f(float x) { return (float)log10(x); }
