/* ppcompat.h - force-included into every C file of the EPOC build (netBook and Series 5mx): what
   EPOC R5's C library (ESTLIB) and gcc 3.0 lack for MuPDF 1.3, FreeType and
   libjpeg. Messages MuPDF writes to stderr go to PsiPDF's log instead
   (ESTLIB would open a console window for them). */
/* Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#ifndef PPCOMPAT_H
#define PPCOMPAT_H
#define restrict __restrict__
#include <_ansi.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#ifndef va_copy
#define va_copy(d, s) ((d) = (s))   /* APCS: va_list is a plain pointer */
#endif
#include <stdio.h>
#include <math.h>
/* ESTLIB's math.h has only the double functions: without these the float
   ones would be implicit int functions, and their results garbage */
float fabsf(float); float floorf(float); float ceilf(float); float sqrtf(float);
float sinf(float); float cosf(float); float atan2f(float, float); float powf(float, float);
float logf(float); float log10f(float);
int pp_fprintf(FILE *f, const char *fmt, ...);
int pp_vfprintf(FILE *f, const char *fmt, va_list ap);
int pp_fputs(const char *s, FILE *f);
int pp_fputc(int c, FILE *f);
int pp_fflush(FILE *f);
int pp_snprintf(char *s, size_t n, const char *fmt, ...);
int pp_vsnprintf(char *s, size_t n, const char *fmt, va_list ap);
#define fprintf pp_fprintf
#define vfprintf pp_vfprintf
#define fputs pp_fputs
#undef fputc
#define fputc pp_fputc
#undef putc
#define putc pp_fputc
#define fflush pp_fflush
#define snprintf pp_snprintf
#define vsnprintf pp_vsnprintf
#endif
