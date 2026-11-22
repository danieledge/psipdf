// ppepoc.cpp - psipdf.exe, PsiPDF's engine on EPOC R5 (the netBook and the
// Series 5mx): the shared chunk (psipdf.h), the command loop, and the rows
// of a page written into the bitmap PsiPDF.app hands over.
//
// Started by PsiPDF.app with the chunk's name as its command line. It waits
// on its doorbell (psibell.h), so it costs nothing while idle, and ends
// when the app asks or the app's thread has gone.
//
// Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
#include <e32std.h>
#include <e32base.h>
#include <e32hal.h>
#include <f32file.h>
#include <fbs.h>
#include <string.h>
#include "psibell.h"
#include "psipdf.h"

extern "C" {
#include "ppfont.h"
}

extern "C" {
long pp_ms(void)
	{
	TUint t = User::TickCount();                 // 1/64 s
	return (long)((t / 64) * 1000 + (t % 64) * 1000 / 64);
	}

// (0.06) The heap may grow to 24 MB (the Makefile), more than the machine
// has to spare: a block that would grow it past what was free when the
// command began, less KPpReserve for the system and the other programs, is
// refused, and MuPDF gives back what it keeps and tries again, or says
// there is not enough memory. 0: no limit (between commands).
static TInt gHeapLimit = 0;
const TInt KPpReserve = 1024 * 1024;

static TBool OverLimit()
	{
	return gHeapLimit > 0 && User::Heap().Size() > gHeapLimit;
	}

// (0.08) PP_SYS_REFUSE (ppcore.h) compares unsigned: "(TInt)n >
// gHeapLimit" let 2^31 and more through to User::Alloc, which panicked
void* pp_sys_malloc(unsigned int n)
	{
	if (PP_SYS_REFUSE(n, gHeapLimit))
		return NULL;
	void* p = User::Alloc(n);
	if (p && OverLimit())
		{
		User::Free(p);
		return NULL;
		}
	return p;
	}

void pp_sys_free(void* p)
	{
	User::Free(p);
	}

void* pp_sys_realloc(void* p, unsigned int n)
	{
	if (PP_SYS_REFUSE(n, gHeapLimit))
		return NULL;
	TInt had = User::AllocLen(p);
	void* q = User::ReAlloc(p, n);
	if (q == p && OverLimit() && n > (TUint)had)
		{
		// (grown in place past the limit: back to its old size, which
		// neither moves nor fails; moved, the old block is already free
		// and the new one is kept)
		User::ReAlloc(q, had);
		return NULL;
		}
	return q;
	}
}

// the limit for the command about to run: the heap as it is, and what the
// machine has free beyond the reserve
static void SetHeapLimit()
	{
	TMemoryInfoV1Buf mem;
	UserHal::MemoryInfo(mem);
	TInt spare = mem().iFreeRamInBytes - KPpReserve;
	gHeapLimit = User::Heap().Size() + Max(spare, 256 * 1024);
	}

// the engine's state
struct TPpEngine
	{
	PpShared* iS;
	pp_doc* iDoc;
	TPsiBellRinger iRinger;              // rings the app's bell
	CFbsBitmap* iBitmap;                 // PP_CMD_RENDER's, duplicated
	TUint8* iRow;                        // one row as the bitmap holds it
	TInt iRowBytes;                      // the bitmap's scan line
	TInt iRows;                          // its height
	TInt iFill;                          // a white byte at its depth
	TUint iLastRing;
	char iPath[256];                     // iDoc's file
	TRequestStatus* iGone;               // completes when the app's thread ends
	};

static void Copy(char* aDst, int aMax, const char* aSrc)
	{
	int i = 0;
	while (aSrc && aSrc[i] && i < aMax - 1)
		{
		aDst[i] = aSrc[i];
		i++;
		}
	aDst[i] = 0;
	}

static void RingApp(TPpEngine& e)
	{
	e.iRinger.Ring(&e.iS->app_bell);
	e.iLastRing = User::TickCount();
	}

// one row from ppcore: into the bitmap (clipped, or padded with white, to
// its width); the app is told after each band
extern "C" int PpRow(void* aUser, int aY, const unsigned char* aData, int aBytes)
	{
	TPpEngine& e = *(TPpEngine*)aUser;
	if (aY >= e.iRows)
		return 0;                        // (a row more than the bitmap has: dropped)
	TInt n = aBytes < e.iRowBytes ? aBytes : e.iRowBytes;
	Mem::Copy(e.iRow, aData, n);
	if (n < e.iRowBytes)
		Mem::Fill(e.iRow + n, e.iRowBytes - n, e.iFill);
	TPtr8 p(e.iRow, e.iRowBytes, e.iRowBytes);
	e.iBitmap->SetScanLine(p, aY);
	e.iS->rows_done = aY + 1;
	// (the app redraws what has come: at most 8 times a second)
	if (User::TickCount() - e.iLastRing >= 8)
		RingApp(e);
	// (the app gone: stopped, so the engine ends now, not after the page)
	return e.iS->cookie.abort || *e.iGone != KRequestPending ? 1 : 0;
	}

// (0.10) the engine's heap cells before its first file, and the most a
// closed file may leave above it (MuPDF's own small state): more means
// blocks lost on an earlier unwind (an out-of-memory or damaged file), which
// nothing counts any more once the document has gone; the app then starts
// the engine afresh before the next file
static TInt gBaseCells = -1;
const TInt KPpStaleCells = 32;

static void CheckStale(PpShared* s)
	{
	if (gBaseCells >= 0 && User::Heap().Count() > gBaseCells + KPpStaleCells)
		s->stale = 1;
	}

static void DoOpen(TPpEngine& e)
	{
	PpShared* s = e.iS;
	int err;
	// the file open already, waiting for its password: tried, not opened again
	if (e.iDoc && pp_locked(e.iDoc) && !strncmp(e.iPath, s->path, sizeof(e.iPath)))
		err = pp_authenticate(e.iDoc, s->text, s->msg, sizeof(s->msg));
	else
		{
		if (e.iDoc)
			{
			pp_close(e.iDoc);
			e.iDoc = NULL;
			CheckStale(s);
			}
		if (gBaseCells < 0)
			gBaseCells = User::Heap().Count();
		unsigned int store = (s->store_kb > 0 ? s->store_kb : 1024) * 1024u;
		e.iDoc = pp_open(s->path, s->text, store, &err, s->msg, sizeof(s->msg));
		Copy(e.iPath, sizeof(e.iPath), s->path);
		}
	Mem::FillZ(s->text, sizeof(s->text));        // (the password, gone once used)
	if (!e.iDoc || err != PP_OK)
		{
		s->result = err;
		return;
		}
	s->pages = pp_pages(e.iDoc);
	pp_title(e.iDoc, s->title, sizeof(s->title));
	s->result = PP_OK;
	}

static void DoSize(TPpEngine& e)
	{
	PpShared* s = e.iS;
	float w = 0, h = 0;
	s->result = e.iDoc ? pp_page_size(e.iDoc, s->page, &w, &h) : PP_E_ARGS;
	s->page_w = (int)(w * 100 + 0.5f);
	s->page_h = (int)(h * 100 + 0.5f);
	}

static void DoRender(TPpEngine& e)
	{
	PpShared* s = e.iS;
	s->rows_done = 0;
	if (!e.iDoc || s->scale <= 0)
		{
		s->result = PP_E_ARGS;
		return;
		}
	CFbsBitmap* b = new CFbsBitmap;
	if (!b || b->Duplicate(s->bitmap) != KErrNone)
		{
		delete b;
		s->result = PP_E_ARGS;
		Copy(s->msg, sizeof(s->msg), "no bitmap");
		return;
		}
	TDisplayMode mode = b->DisplayMode();
	TSize size = b->SizeInPixels();
	TInt depth = mode == EGray16 ? PP_DEPTH_GREY : PP_DEPTH_COLOUR;
	e.iBitmap = b;
	e.iRows = size.iHeight;
	e.iRowBytes = depth == PP_DEPTH_GREY ? (size.iWidth + 1) / 2 : size.iWidth;
	e.iFill = 0xff;                      // (white: index 255, or two greys of 15)
	e.iRow = (TUint8*)User::Alloc(e.iRowBytes + 4);
	if (!e.iRow)
		s->result = PP_E_MEMORY;
	else
		{
		pp_stats st;
		float scale = s->scale / 10000.0f;
		s->result = pp_render(e.iDoc, s->page, scale, depth, s->band > 0 ? s->band : 32,
			PpRow, &e, &s->cookie, &st, s->msg, sizeof(s->msg));
		s->list_ms = st.list_ms;
		s->draw_ms = st.draw_ms;
		s->quant_ms = st.quant_ms;
		s->out_ms = st.out_ms;
		// rows the page did not reach (a bitmap a row taller): white
		if (s->result == PP_OK)
			{
			Mem::Fill(e.iRow, e.iRowBytes, e.iFill);
			TPtr8 p(e.iRow, e.iRowBytes, e.iRowBytes);
			for (TInt y = s->rows_done; y < e.iRows; y++)
				b->SetScanLine(p, y);
			s->rows_done = e.iRows;
			}
		User::Free(e.iRow);
		}
	e.iRow = NULL;
	e.iBitmap = NULL;
	delete b;                            // (the app keeps its own)
	}

static void DoFind(TPpEngine& e)
	{
	PpShared* s = e.iS;
	s->found_page = -1;
	s->hits = 0;
	if (!e.iDoc)
		{
		s->result = PP_E_ARGS;
		return;
		}
	TInt pages = pp_pages(e.iDoc);
	TInt from = s->page < 0 || s->page >= pages ? 0 : s->page;
	float hits[PP_MAX_HITS * 4];
	s->result = PP_OK;
	for (TInt k = 0; k < pages; k++)
		{
		TInt n = (from + k) % pages;
		s->find_at = n;
		if (k % 4 == 3)
			RingApp(e);
		TInt c = pp_find(e.iDoc, n, s->text, hits, PP_MAX_HITS, &s->cookie);
		if (c == PP_E_STOPPED)
			{
			s->result = c;
			return;
			}
		if (c > 0)
			{
			s->found_page = n;
			s->hits = c;
			for (TInt i = 0; i < c; i++)
				for (TInt j = 0; j < 4; j++)
					s->hit[i][j] = (int)(hits[4 * i + j] * 100);
			return;
			}
		if (s->cookie.abort || *e.iGone != KRequestPending)
			{
			s->result = PP_E_STOPPED;
			return;
			}
		}
	}

static void Do(TPpEngine& e)
	{
	PpShared* s = e.iS;
	s->msg[0] = 0;
	SetHeapLimit();
	switch (s->cmd)
		{
	case PP_CMD_OPEN: DoOpen(e); break;
	case PP_CMD_SIZE: DoSize(e); break;
	case PP_CMD_RENDER: DoRender(e); break;
	case PP_CMD_FIND: DoFind(e); break;
	case PP_CMD_CLOSE:
		if (e.iDoc)
			{
			pp_close(e.iDoc);
			CheckStale(s);
			}
		e.iDoc = NULL;
		s->result = PP_OK;
		break;
	case PP_CMD_TRIM:
		if (e.iDoc)
			pp_trim(e.iDoc);
		s->result = PP_OK;
		break;
	default:
		s->result = PP_E_ARGS;
		break;
		}
	gHeapLimit = 0;
	s->font_trouble = pp_font_trouble();
	// what a page needed while it was drawn (a picture decoded, the band's
	// pixmap) given back to the system: EPOC's heap never shrinks by itself,
	// and the engine stayed at its peak for as long as the file was open
	if (s->cmd != PP_CMD_SIZE)
		User::Heap().Compress();
	if (e.iDoc)
		{
		s->heap_kb = (int)(pp_heap_now(e.iDoc) / 1024);
		s->heap_peak_kb = (int)(pp_heap_peak(e.iDoc) / 1024);
		}
	Copy(s->mu_msg, sizeof(s->mu_msg), pp_messages());
	}

// The font file (the 14 standard PDF fonts, read when a PDF needs one:
// ppfont.c): beside this program, wherever it is installed; else in
// PsiPDF's folder on any disk (the engine put on another disk than the
// rest). Not there: text in those fonts shows in Helvetica, and PsiPDF says.
static void FindFontFile()
	{
	_LIT(KFonts, "PsiPDF.fnt");
	TFileName me = RProcess().FileName();
	TParse p;
	TFileName file;
	RFs fs;
	if (fs.Connect() != KErrNone)
		return;
	TEntry entry;
	if (p.Set(KFonts, &me, NULL) == KErrNone && fs.Entry(p.FullName(), entry) == KErrNone)
		file = p.FullName();
	else
		{
		TFindFile find(fs);
		if (find.FindByDir(_L("\\System\\Apps\\PsiPDF\\PsiPDF.fnt"), KNullDesC) == KErrNone)
			file = find.File();
		}
	fs.Close();
	char path[260];
	TInt n = Min(file.Length(), (TInt)sizeof(path) - 1);
	Mem::Copy(path, file.Ptr(), n);          // (a narrow build: one byte a character)
	path[n] = 0;
	pp_font_file(path);
	}

static TInt RunL()
	{
	FindFontFile();
	TCommand name = RProcess().CommandLine();
	name.Trim();
	RChunk chunk;
	User::LeaveIfError(chunk.OpenGlobal(name, EFalse));
	CleanupClosePushL(chunk);
	PpShared* s = (PpShared*)chunk.Base();
	if (s->magic != PP_MAGIC || s->protocol != PP_PROTOCOL)
		User::Leave(KErrNotSupported);
	// the app's thread: when it goes, so does the engine
	RThread app;
	TThreadId id;
	TUint tid = s->app_tid;
	Mem::Copy(&id, &tid, sizeof(id));
	User::LeaveIfError(app.Open(id));
	CleanupClosePushL(app);
	TRequestStatus gone;
	app.Logon(gone);
	User::LeaveIfError(RFbsSession::Connect());

	TPpEngine e;
	Mem::FillZ(&e, sizeof(e));
	// (all zero is TPsiBellRinger's state as constructed)
	e.iS = s;
	e.iGone = &gone;
	// a step below the program's own thread: the app sets this process's
	// priority to its own (Foreground in front, Background behind), and keys,
	// the pen and the screen still never wait for MuPDF
	RThread().SetPriority(EPriorityMuchLess);
	// (taken before saying so: the app runs above the engine, and its first
	// command may come before this thread runs again)
	TUint done = s->cmd_seq;
	s->state = PP_STATE_READY;
	RingApp(e);
	TBool quit = EFalse;
	PsiBell* bell = &s->eng_bell;
	while (!quit)
		{
		if (s->cmd_seq != done)
			{
			done = s->cmd_seq;
			if (s->cmd == PP_CMD_QUIT || gone != KRequestPending)
				quit = ETrue;
			else
				Do(e);
			s->done_seq = done;
			RingApp(e);
			continue;
			}
		// wait for the app's bell (or its end)
		TRequestStatus st = KRequestPending;
		bell->status = &st;
		bell->tid = PsiBellMyTid();
		TUint seen = bell->seq;
		bell->armed = 1;
		if (s->cmd_seq != done || bell->seq != seen)
			{
			if (PsiBellSwap(&bell->armed, 0) != 1)
				PsiBellTakeBack(bell, st);   // (the app has taken it: its completion)
			continue;
			}
		if (bell->broken)
			{
			// (the app could not reach this thread: look ten times a second)
			if (PsiBellSwap(&bell->armed, 0) != 1)
				PsiBellTakeBack(bell, st);
			User::After(100000);
			}
		else
			User::WaitForRequest(st, gone);
		if (gone != KRequestPending)
			{
			if (PsiBellSwap(&bell->armed, 0) != 1 && st == KRequestPending)
				PsiBellTakeBack(bell, st);
			break;                           // the app has gone
			}
		}
	if (gone == KRequestPending)
		{
		app.LogonCancel(gone);
		User::WaitForRequest(gone);
		}
	// (0.14, RN-25) the file is not closed piece by piece: the process's end
	// gives its heap back whole, at once. pp_close freed every block of
	// MuPDF's heap one at a time, at a priority below every program's, and
	// on Dan's netBook PsiPDF's close outwaited it ("engine did not end:
	// killed")
	e.iRinger.Close();
	s->state = PP_STATE_EXITED;
	RFbsSession::Disconnect();
	CleanupStack::PopAndDestroy(2);      // app, chunk
	return KErrNone;
	}

extern "C" int main(int, char**)
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TInt r = KErrNoMemory;
	if (cleanup)
		{
		TRAPD(err, r = RunL());
		if (err != KErrNone)
			r = err;
		delete cleanup;
		}
	return r;
	}
