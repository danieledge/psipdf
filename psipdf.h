/* psipdf.h - the memory PsiPDF.app and psipdf.exe share.
 *
 * PsiPDF.app (app/: the EIKON program: menus, dialogs, keys, the pen, the
 * screen) makes a small global chunk with this layout, named on the engine's
 * command line, and starts psipdf.exe (engine/: MuPDF), which stays running
 * while a file is open. The program is a DLL, and EPOC R5 lets no DLL have
 * writable static data, which MuPDF, FreeType and libjpeg all have: an .exe
 * may, wherever it is installed, so the engine is one. It also keeps the
 * program responsive (the engine runs below it) and safe: a file that
 * crashes MuPDF ends the engine, not PsiPDF.
 *
 * One command at a time: the app fills in the command's fields, adds 1 to
 * cmd_seq and rings eng_bell; the engine does it, fills in the results,
 * sets done_seq to cmd_seq and rings app_bell (psion-core's ssh/psibell.h).
 * While it draws, rows_done grows band by band, and app_bell rings after
 * each band. The app stops a command by setting cookie.abort.
 *
 * Pages come back in a CFbsBitmap the app makes (EColor256 on the netBook,
 * EGray16 on the Series 5mx) and hands over by its handle; the engine writes
 * its rows. If either side changes this file, both must be rebuilt.
 *
 * Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE. */
#ifndef PSIPDF_H
#define PSIPDF_H

#include "psishared.h"          /* core/ssh: PsiBell */
#include "engine/ppcore.h"      /* pp_cookie, PP_E_*, PP_DEPTH_* */

#define PP_MAGIC        0x46445050u     /* 'PPDF' */
#define PP_PROTOCOL     4
#define PP_MAX_HITS     32

enum
{
	PP_STATE_STARTING = 0,
	PP_STATE_READY,
	PP_STATE_EXITED
};

enum
{
	PP_CMD_NONE = 0,
	PP_CMD_OPEN,            /* path, text (the password): pages, title */
	PP_CMD_SIZE,            /* page: page_w, page_h (points x 100) */
	PP_CMD_RENDER,          /* page, scale, bitmap, depth: rows_done, times */
	PP_CMD_FIND,            /* page (the first to look at), text: found_page, hits */
	PP_CMD_CLOSE,           /* the file closed (the engine stays) */
	PP_CMD_TRIM,            /* what can be made again given back (memory is short) */
	PP_CMD_QUIT
};

typedef struct
	{
	unsigned int magic;
	int protocol;
	PsiBell app_bell;               /* the engine rings, the app waits */
	PsiBell eng_bell;               /* the app rings, the engine waits */
	unsigned int app_tid;           /* app: its thread (the engine ends with it) */
	volatile int state;             /* PP_STATE_* (engine) */

	/* the command (app) */
	volatile unsigned int cmd_seq;
	int cmd;
	int page;                       /* from 0 */
	int scale;                      /* pixels a point x 10000 */
	int bitmap;                     /* the CFbsBitmap's handle */
	int depth;                      /* PP_DEPTH_* */
	int band;                       /* rows drawn at a time */
	int store_kb;                   /* PP_CMD_OPEN: what MuPDF may keep between pages */
	char path[256];                 /* PP_CMD_OPEN: the file (the Psion's code page) */
	char text[128];                 /* the password, or the text to find (UTF-8) */
	pp_cookie cookie;               /* app: abort = 1 stops the command */

	/* the result (engine) */
	volatile unsigned int done_seq; /* = cmd_seq once done */
	volatile int result;            /* PP_OK or PP_E_* */
	char msg[160];                  /* why, in English (for the log) */
	int pages;
	char title[128];                /* PP_CMD_OPEN: the document's title, UTF-8 */
	int page_w, page_h;             /* PP_CMD_SIZE: points x 100 */
	volatile int rows_done;         /* PP_CMD_RENDER: rows written so far */
	volatile int find_at;           /* PP_CMD_FIND: the page being searched */
	int found_page;                 /* PP_CMD_FIND: the page found, or -1 */
	int hits;
	int hit[PP_MAX_HITS][4];        /* x0, y0, x1, y1 in points x 100 */
	int list_ms, draw_ms, quant_ms; /* PP_CMD_RENDER's times */
	int out_ms;
	int heap_kb, heap_peak_kb;      /* MuPDF's memory */
	char mu_msg[160];               /* MuPDF's last warning (for the log) */
	volatile int font_trouble;      /* PP_FONT_* once the font file could not be used, else 0 */
	volatile int stale;             /* (0.10) a file closed left heap cells behind: start the engine afresh */
	} PpShared;

#endif
