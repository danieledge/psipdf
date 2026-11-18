/* mkbidi.c - makes build/gen/ppbidi.h: the bidirectional class of every
 * Unicode character as runs, from MuPDF's own ucdn tables (so the answers
 * are ucdn's, character for character). Built and run on the PC by the
 * Makefile:  mkbidi > ppbidi.h   ("mkbidi check" is not needed: ppbidi.c's
 * host test, host/bidicheck.c, compares every character with ucdn).
 *
 * Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#include <stdio.h>
#include "ucdn.c"

int main(void)
{
	unsigned int c, n = 0;
	int last = -1;
	printf("/* made by tools/mkbidi.c from MuPDF's ucdn: (first character << 5) | class */\n");
	printf("static const unsigned int pp_bidi_runs[] = {\n");
	for (c = 0; c < 0x110000; c++)
	{
		int k = ucdn_get_bidi_class(c);
		if (k < 0 || k > 31)
			return 1;
		if (k != last)
		{
			printf("0x%x,%s", (c << 5) | (unsigned int)k, ++n % 8 ? "" : "\n");
			last = k;
		}
	}
	printf("};\n#define PP_BIDI_RUNS %u\n#define PP_BIDI_BEYOND %d\n", n, ucdn_get_bidi_class(0x110000));
	return 0;
}
