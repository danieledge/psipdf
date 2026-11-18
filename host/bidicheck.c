/* bidicheck.c - a test on the PC: engine/mu/ppbidi.c gives ucdn's
 * bidirectional class for every character (make TARGET=host bidicheck).
 * Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#include <stdio.h>
#define ucdn_get_bidi_class real_bidi_class
#include "ucdn.c"
#undef ucdn_get_bidi_class
int ucdn_get_bidi_class(unsigned int code);

int main(void)
{
	unsigned int c, bad = 0, i;
	static const unsigned int odd[] = { 0x110000, 0x110001, 0x7fffffff, 0x80000000u, 0xffffffffu };
	/* up, then down and by strides (the run found last is tried first) */
	for (c = 0; c < 0x110000; c++)
		bad += ucdn_get_bidi_class(c) != real_bidi_class(c);
	for (c = 0x110000; c-- > 0; )
		bad += ucdn_get_bidi_class(c) != real_bidi_class(c);
	for (i = 0, c = 1; i < 3000000; i++, c = (c * 1103515245u + 12345u) % 0x110000)
		bad += ucdn_get_bidi_class(c) != real_bidi_class(c);
	for (i = 0; i < sizeof odd / sizeof odd[0]; i++)
		bad += ucdn_get_bidi_class(odd[i]) != real_bidi_class(odd[i]);
	printf("bidicheck: %u differences from ucdn\n", bad);
	return bad != 0;
}
