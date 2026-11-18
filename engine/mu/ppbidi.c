/* ppbidi.c - ucdn_get_bidi_class without ucdn's tables.
 *
 * The cut-down MuPDF uses one thing of ucdn (102 KB of tables in the
 * program's code): the bidirectional class of a character, as Find reads a
 * page's text (stext-device.c). Here it comes from the same data as runs
 * of characters of one class (6 KB, made at build time from ucdn's
 * own tables by tools/mkbidi.c), found by a binary search; the run found
 * last is tried first, as text stays in one script for long stretches.
 * host/bidicheck.c compares all 1,114,112 characters with ucdn.
 *
 * Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#include "ppbidi.h"

static unsigned int pp_bidi_last;       /* the run found last */

int ucdn_get_bidi_class(unsigned int code)
{
	unsigned int lo, hi, i = pp_bidi_last;
	if (code >= 0x110000)
		return PP_BIDI_BEYOND;
	if ((pp_bidi_runs[i] >> 5) <= code && (i + 1 == PP_BIDI_RUNS || code < (pp_bidi_runs[i + 1] >> 5)))
		return (int)(pp_bidi_runs[i] & 31);
	lo = 0;
	hi = PP_BIDI_RUNS - 1;          /* the run is in lo..hi (run 0 starts at 0) */
	while (lo < hi)
	{
		unsigned int mid = (lo + hi + 1) / 2;
		if ((pp_bidi_runs[mid] >> 5) <= code)
			lo = mid;
		else
			hi = mid - 1;
	}
	pp_bidi_last = lo;
	return (int)(pp_bidi_runs[lo] & 31);
}
