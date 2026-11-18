/* ppfont.h - the 14 standard PDF fonts, read from PsiPDF's font file when
 * a PDF needs one (ppfont.c).
 *
 * Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#ifndef PPFONT_H
#define PPFONT_H

/* the font file's full name (copied; NULL or "": there is none). Call with
   no file open. */
void pp_font_file(const char *path);
/* fonts no FreeType face uses any more are freed (cheap: call it often) */
void pp_font_sweep(void);
/* every font freed: only once MuPDF's context has gone */
void pp_font_reset(void);
/* bytes of font data held now */
unsigned long pp_font_bytes(void);

#endif
