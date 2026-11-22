// PSIPDF.CPP - PsiPDF: PDF files on the Psion netBook and Series 5mx, drawn
// by MuPDF. Tap a PDF on the System screen (PsiPDF.mdl, the recogniser,
// names it application/pdf, which PsiPDF.aif takes) or open it from PsiHub
// or File > Open, and its first page (or the page it was left at) shows,
// fitted to the width of the screen.
//
// - Pages: Fn+Down and Fn+Up (Page down, Page up), Shift+Ctrl+N and
//   Shift+Ctrl+B, the toolbar, Space and Backspace at the foot and the head
//   of a page, or the dog-ears in the bottom corners. Ctrl+Fn+Up and Down:
//   the first and last page. Edit > Go to page (Ctrl+G).
// - Zoom: Ctrl+M and Shift+Ctrl+M cycle round fit page, fit width, 100%,
//   150% and 200% (100% is the page's real size on this screen). Ctrl+W:
//   fit width (the standard); Shift+Ctrl+W: fit page.
// - Move round a page: the arrow keys (Ctrl: a screen at a time), Home and
//   End (its top and foot), or drag with the pen.
// - Edit > Find (Ctrl+F) and Find next (Ctrl+J): text, in any case.
//
// MuPDF runs in psipdf.exe (../engine, ../psipdf.h): EPOC R5 lets no DLL
// (such as this program) have writable static data, which MuPDF needs. The
// engine draws each page band by band into a bitmap made here, already in
// the screen's colours, and the page shows as it comes. While a page is on
// the screen the next is drawn ahead, if there is the memory, so that
// turning the page is immediate.
//
// The last file, and the page and zoom of the last 8, are remembered
// (C:\System\Apps\PsiPDF\PsiPDF.ini).
//
// Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.

#include <e32std.h>
#include <e32hal.h>
#include <e32math.h>
#include <f32file.h>
#include <apparc.h>
#include <eikenv.h>
#include <eikappui.h>
#include <eikapp.h>
#include <eikdoc.h>
#include <eikmenup.h>
#include <eiktbar.h>
#include <eikcmbut.h>
#include <eikfnlab.h>
#include <eikimage.h>
#include <eiklabel.h>
#include <eikproc.h>
#include <eikon.rsg>
#include <eikcfdlg.h>
#include <eikcmds.hrh>
#include <eikdialg.hrh>
#include <eikmenu.hrh>
#include <eikdialg.h>
#include <eikmsg.h>
#include <clock.h>
#include <eikedwin.h>
#include <eikmfne.h>
#include <eikseced.h>
#include <coecntrl.h>
#include <fbs.h>
#include <bautils.h>

#include "psibell.h"
#include "psipdf.h"
#include <psipdf.rsg>
#include "psipdf.hrh"
#include "psimem.h"                  // core/ssh: short of memory, what the programs say to each other
                                     // (0.10: its words and PsiMemAnswer from core, no copy here)
#include "psihelp.h"                 // Tools > Help on PsiPDF (Coda's Help dialog)
#include "psierr.h"                  // an error as words for a message (0.12: psion-core's, build.sh copies it)

const TUid KUidPsiPdf = { 0x01000AD8 };
_LIT(KPpVersion, "0.15");
_LIT(KPpIni, "C:\\System\\Apps\\PsiPDF\\PsiPDF.ini");
_LIT(KPpLog, "C:\\System\\Apps\\PsiPDF\\PsiPDF.log");
_LIT(KPpLogOld, "C:\\System\\Apps\\PsiPDF\\PsiPDF.old");
_LIT(KPpIniNew, "C:\\System\\Apps\\PsiPDF\\PsiPDF.new");
_LIT(KPpDiagOn, "C:\\System\\Data\\PsiDiag.on");   // Coda's debug mode (Control panel > Diagnostics)
const TInt KPpMaxPages = 99999;      // (the most Go to page takes; a file that says more is shown to there)

// Tools > Help on PsiPDF (Shift+Ctrl+H). British English, the user's words,
// the keys as the keyboard prints them; what the code does, nothing more
static const TPsiHelpTopic KPpHelp[] =
	{
	{ "Reading",
	  "PsiPDF shows PDF files. Tap one on the System screen, or use File > Open file (Ctrl+O): "
	  "the first time, it opens in the Example files folder on the card. "
	  "Started from its icon, PsiPDF opens the last file again, at the page and zoom it was left at.\n\n"
	  "Fn+Down and Fn+Up (or Shift+Ctrl+N and Shift+Ctrl+B, the toolbar, or the dog-ears in the "
	  "bottom corners) turn the page; Ctrl+Fn+Up and Ctrl+Fn+Down go to the first and last page, "
	  "and Edit > Go to page (Ctrl+G) to any. The arrow keys, or the pen, move round the page.\n\n"
	  "A file opens fitted to the width of the screen. Ctrl+M zooms in and Shift+Ctrl+M out, a step "
	  "at a time in order of size: the whole page, the width, 100%, 150% and 200%, then round again. "
	  "Ctrl+W fits the width, Shift+Ctrl+W the whole page. View > Zoom to has them all." },
	{ "Finding text",
	  "Edit > Find (Ctrl+F) finds text, in any case, from the page shown onwards; Find next (Ctrl+J) "
	  "finds the next. Esc stops it.\n\n"
	  "File > File details (Shift+Ctrl+D) shows the file's title, its pages, its size and its folder." },
	{ "Big pages",
	  "A page appears as it is drawn, from the top down. Esc stops the drawing; Enter draws the page "
	  "again. While you read, PsiPDF draws the next page ahead if there is the memory.\n\n"
	  "A page that cannot be shown says why, on the screen. Pages with many pictures need the most "
	  "memory: one there was not enough memory for is shown by itself once memory is free (on Coda, "
	  "the message's Free memory button helps), or at once with a tap or Enter.\n\n"
	  "Tools > Troubleshooting > Keep a log writes what PsiPDF does to a log, to send with a "
	  "question about a problem." },
	};
// (0.06) the file being opened, until its first page is drawn: a file that
// ran out of memory, or ended the engine, is not opened again at the next
// start (the Open dialog still opens it)
_LIT(KPpTrying, "C:\\System\\Apps\\PsiPDF\\PsiPDF.try");
// the recogniser (PDF files open in PsiPDF from the System screen): on C:,
// or in the netBook's ROM; a spare copy beside the program puts it back
// when C: has been cleared
_LIT(KPpRecogC, "C:\\System\\Recogs\\PsiPDF.mdl");
_LIT(KPpRecogZ, "Z:\\System\\Recogs\\PsiPDF.mdl");
_LIT(KPpEngine, "psipdf.exe");

// the zooms, in the order of View > Zoom
enum { EZoomFitWidth = 0, EZoomFitPage, EZoom100, EZoom150, EZoom200, EZoomCount };
const TInt KPpRecent = 8;                    // files whose page and zoom are remembered
// Memory (the netBook has about 16 MB free after it starts, shared by every
// program; the 5mx, 8 or 16 MB). What PsiPDF may take is set when a file
// opens, from the free RAM then:
// - the biggest page bitmap: a fifth of it (0.14; a quarter before), from
//   1 MB (a page fitted to the width: 580 KB) up to 6 MB (200% on an A4
//   page is 4 MB: with less memory it is drawn a little smaller);
// - MuPDF's store (decoded pictures, fonts): a fifth of it, from 1 MB up
//   to 3 MB (2 MB on the 5mx); a picture is decoded no bigger than this.
// What it holds is bounded by these: in the netBook emulator, 12 pages
// forward and back at fit width and at 200%, twice, the engine's heap
// stayed at 1.2 MB (peak 1.9 MB) and the free memory came back to the same
// figures in the second round as in the first.
const TInt KPpMaxBytes = 6 * 1024 * 1024;
// (0.06) no page bitmap wider or taller than this, whatever the page's size
const TInt KPpMaxSide = 16384;
const TInt KPpMinBytes = 1024 * 1024;
const TInt KPpStoreMinKb = 1024;
// the next page is drawn ahead only while it is small (fit width or fit
// page: up to 1 MB) and this much free memory is left after it
const TInt KPpAheadMaxBytes = 1024 * 1024;
const TInt KPpAheadSpare = 4 * 1024 * 1024;   // (0.14: 3 MB before)
// (0.14, RN-25) the store and the biggest page bitmap: each a fifth of the
// memory free as the file opens (a quarter before)
const TInt KPpShareOfFree = 5;
// PsiPDF behind this long with a file open: its engine (MuPDF, 3 to 8 MB with
// its code) is stopped, and the file opened again, at the same page and
// zoom, when PsiPDF comes back; the page on the screen is kept meanwhile
const TInt KPpIdleStop = 5 * 60 * 1000000;
// (0.13) a page that could not be shown for want of memory is tried again
// by itself when memory comes back: looked at every 2 s while PsiPDF is in
// front, tried once the memory service's level has dropped or this much
// more is free than when it failed (on a Psion with no service)
const TInt KPpMemLookUs = 2000000;
const TInt KPpMemRetryKb = 1024;
// MuPDF draws a page in bands of about this much RGBA: on the netBook 512 KB
// (200 rows 640 wide, a quarter of a fitted A4 page, where the whole page's
// pixmap would be 2.3 MB), on the 5mx, with half the memory, 160 KB (64
// rows). Each band runs the page's display list again: ppcore.c's line
// splitter gives each line of text its own box, so a band draws only the
// lines it crosses, but a picture is scaled again for each band it is in.
const TInt KPpBandBytesColour = 524288;
const TInt KPpBandBytesGrey = 163840;
const TInt KPpDogEar = 20;                   // the dog-ears' size
const TInt KPpPollTick = 250000;             // the engine looked at (besides its bell) while busy
const TInt KPpSoon = 200000;                 // a new page or zoom drawn this long after the last key
// The engine not ready this long after it was started, or a command with no
// progress (no band, no page found) this long, while PsiPDF is in front: it
// is stopped and the page says why, never a blank page for ever. (A page
// takes 1 to 3 s on the netBook, 8 s on the 5mx; a band, much less.)
const TInt KPpStartTicks = 20 * 64;          // (1/64 s)
const TInt KPpStuckTicks = 60 * 64;

// the toolbar pictures in PsiPDF.mbm (pkg/mkicon.py), each then its mask
enum { EMbmPpPrev = 0, EMbmPpNext = 2, EMbmPpZoomIn = 4, EMbmPpZoomOut = 6 };

// a page drawn, or being drawn
struct TPpPic
	{
	CFbsBitmap* iBmp;
	TInt iPage;
	TInt iScale;                 // pixels a point x 10000
	TInt iRows;                  // rows drawn so far
	TBool iDone;                 // all drawn (or failed: iFailed)
	TBool iFailed;
	TBool iStopped;              // (0.06) stopped with Esc: not drawn again until asked (Enter)
	TInt iMs;                    // how long it took
	};

struct TPpRecentFile
	{
	TFileName iFile;
	TInt iPage;
	TInt iZoom;
	};

class CPpAppUi;
class CPpMemoryDialog;             // (0.13) Not enough memory, with Free memory...

// ---------------------------------------------------------------------------
// The page on the screen
// ---------------------------------------------------------------------------

class CPpView : public CCoeControl
	{
public:
	CPpView(CPpAppUi& aUi) : iUi(aUi) {}
	void ConstructL(const TRect& aRect);
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
private:
	void Draw(const TRect& aRect) const;
	void HandlePointerEventL(const TPointerEvent& aPointerEvent);
private:
	CPpAppUi& iUi;
	TPoint iDragFrom;
	TBool iDragged;
	};

// the engine's end (a crash, say)
class CPpWatcher : public CActive
	{
public:
	CPpWatcher(CPpAppUi& aUi) : CActive(EPriorityStandard), iUi(aUi) { CActiveScheduler::Add(this); }
	~CPpWatcher() { Cancel(); }
	void Watch(RProcess& aProcess) { iProcess = &aProcess; aProcess.Logon(iStatus); SetActive(); }
private:
	void RunL();
	void DoCancel() { if (iProcess) iProcess->LogonCancel(iStatus); }
	CPpAppUi& iUi;
	RProcess* iProcess;
	};

// ---------------------------------------------------------------------------
// The program
// ---------------------------------------------------------------------------

// (0.15, RR-10) the busy message, as EIKON's in every way (its window,
// font, colours, border and corner) but steady: EIKON's flashes, and half
// the time it is an empty dark box. On the netBook, where a page takes
// several seconds, that was what showed after a zoom
class CPpBusyWin : public CEikBusyMsgWin
	{
public:
	CPpBusyWin(CEikonEnv& aEnv) : CEikBusyMsgWin(aEnv) {}
	void Show(const TDesC& aText, TTimeIntervalMicroSeconds32 aDelay)
		{
		iDelay = aDelay;
		StartDisplay(aText, EHLeftVBottom);
		}
private:
	void DoStartDisplay(const TDesC& aText)
		{
		iMessageWindow->StartDisplay(EFalse, iDelay, aText);
		}
	TTimeIntervalMicroSeconds32 iDelay;
	};

class CPpAppUi : public CEikAppUi
	{
public:
	~CPpAppUi();
	void ConstructL();
	void HandleCommandL(TInt aCommand);
	void DynInitMenuPaneL(TInt aMenuId, CEikMenuPane* aMenuPane);
	TBool ProcessCommandParametersL(TApaCommand aCommand, TFileName& aDocumentName, const TDesC8& aTail);
	void OpenFileL(const TDesC& aFileName);
	// for the view
	void DrawPage(CWindowGc& aGc, const TRect& aRect) const;
	TKeyResponse KeyL(const TKeyEvent& aKeyEvent);
	void PanL(TInt aDx, TInt aDy);
	void TapL(const TPoint& aPoint);
	// for the watcher
	void EngineEnded();
private:
	// the engine
	void StartEngineL();
	void StopEngine();
	void Send(TInt aCmd);
	static TInt BellCallback(TAny* aSelf);
	static TInt TickCallback(TAny* aSelf);
	void Poll();
	void JobDoneL(TInt aCmd);
	void Pump();
	void PumpL();
	void PumpSoon();
	static TInt SoonCallback(TAny* aSelf);
	TBool AheadIsWanted() const;
	TBool WantJob() const;
	// the file
	void OpenPdfL(const TDesC& aName);
	void CloseDocument();
	void OpenDoneL();
	static TInt AskPasswordCallback(TAny* aSelf);
	static TInt StopIdleCallback(TAny* aSelf);
	void AskPasswordL();
	// pages
	TBool SizeKnown(TInt aPage) const { return iSizes && iSizes[2 * aPage] > 0; }
	TInt ScaleFor(TInt aPage, TInt aZoom) const;
	TSize PixelsFor(TInt aPage, TInt aScale) const;
	TInt BandFor(TInt aWidth) const
		{
		TInt bytes = iMode == EColor256 ? KPpBandBytesColour : KPpBandBytesGrey;
		return Max(16, Min(256, bytes / (Max(1, aWidth) * 4)));
		}
	TInt CreatePic(TPpPic& aPic, TInt aPage, TInt aScale);
	void DropPic(TPpPic& aPic);
	void SwapPics();
	void GoToPageL(TInt aPage, TBool aAtFoot = EFalse);
	void TurnL(TInt aStep);
	void SetZoomL(TInt aZoom);
	void ZoomStepL(TInt aDir);
	void ClampOrigin();
	TSize ShownSize() const;
	TRect PageRect() const;
	void UpdateLabelL();
	// find
	void FindL();
	void FindNextL();
	void StartFind(TInt aFromPage);
	void ShowHit();
	// dialogs and the rest
	void GoToL();
	void DetailsL();
	void ShowToolBarL(TBool aShow);
	TRect ViewRect(TBool aToolbar) const;
	CEikToolBar* Bar() const { return iToolBar; }
	void MakeToolBarL();
	void ButtonPictureL(TInt aId, TInt aIcon);
	void Busy(const TDesC& aText);
	void BusyEnd();
	void Problem(const TDesC& aWhat, TInt aResult);
	void PartDrawn(TInt aResult);
	TBool NoFile();
	void MarkTrying(TBool aOn);
	TBool WasTrying(const TDesC& aFile);
	void RestoreRecogniser();
	void LoadSettings();
	void SaveSettings();
	void Remember();
	void Log(const TDesC& aText);
	void HandleForegroundEventL(TBool aForeground);
	void ProcessMessageL(TUid aUid, const TDesC8& aParams);
	void Trim();
	void TellShort();
	// a page that cannot be shown: said in the dialog and on the page itself
	void CannotShow(const TDesC& aWhat, const TDesC& aWhy);
	// (0.13) not enough memory for the page: said, and tried again when
	// memory comes back (MemRetry), at the next return to the front or a tap
	enum TPpMemShort { EPpMemNone, EPpMemPage, EPpMemOpen };
	void MemoryShort(TInt aKind, const TDesC& aWhat);
	void MemoryDialog(const TDesC& aWhat, const TDesC& aWhy);
	void MemRetry(TBool aAsked);
	static TInt MemLookCallback(TAny* aSelf);
	static TInt MemAgainCallback(TAny* aSelf);
	void Watchdog();
	void EnginePriority();
	TInt FreeRamKb() const;
	static TInt IdleStopCallback(TAny* aSelf);
	void IdleStop(TBool aAsked = EFalse);
	void DoCommandL(TInt aCommand);
	static void CommandDone(TAny* aSelf);
private:
	CPpView* iView;
	RChunk iMemChunk;            // (0.10) registered with the memory watcher (psimemlvl.h)
	TInt iCommands;              // (0.10) commands running (a dialog up): busy
	// the engine
	RChunk iChunk;
	TBool iChunkOpen;
	PpShared* iS;
	RProcess iProcess;
	TBool iRunning;              // started (and not ended)
	TBool iReady;                // it has said so
	TPsiBellRinger iRinger;
	CPsiBellWaiter* iWaiter;
	CPeriodic* iTick;
	CPpWatcher* iWatcher;
	CIdle* iIdle;                // the password asked for after an open
	CPeriodic* iSoon;            // a page or zoom drawn a moment after the key (quick presses make one)
	CPeriodic* iIdleStopper;     // the engine stopped after KPpIdleStop behind
	TBool iAsleep;               // the engine stopped behind: the file is opened again in front
	TBool iFontTold;             // the font file missing or damaged: said once
	TBool iPartTold;             // (0.06) a page partly drawn: a dialog once a file, then infoprints
	TBool iEscStopped;           // Esc pressed while the page shown was being drawn
	TBool iTryingMarked;         // PsiPDF.try written for iFile (its first page not drawn yet)
	TInt iTrimFree;              // free RAM (KB) as a trim was asked of the engine
	TBool iReopen;               // the file to be opened again (the engine started afresh)
	TBool iReopening;            // ...and its open sent
	TInt iStoreKb;               // MuPDF's store for this file
	TInt iMaxBytes;              // the biggest page bitmap for this file
	TInt iJob;                   // the command the engine has, or 0
	TPpPic* iJobPic;             // PP_CMD_RENDER: the picture it draws into
	TUint iJobStart;             // (ticks)
	TBool iStopping;             // its job has been told to stop
	TBool iEngineFailed;         // the engine ended by itself: not started again for this file
	TUint iStartTick;            // when the engine was started (ticks)
	TUint iProgressTick;         // when its job last moved on (ticks)
	TInt iSeenRows, iSeenProgress, iSeenFind;
	TBool iForeground;           // PsiPDF is in front
	TBuf<64> iNote;              // why the page is not shown ("Could not show this page"...)
	TBuf<100> iNoteWhy;          // ...and what to do
	TInt iMemShort;              // (0.13) TPpMemShort: what failed for want of memory, to be tried again
	TInt iMemShortKb;            // ...the free RAM then (KB)
	TInt iMemShortLevel;         // ...and the memory service's level (-1: no service)
	TBool iMemAuto;              // ...tried again by itself: a second failure says so on the page only
	TBool iMemDialogUp;          // ...the dialog is up (nothing tried under it)
	TInt iMemDialogAgain;        // ...times shown again after a message closed it
	TBuf<64> iMemWhat;           // ...its first line
	CPeriodic* iMemAgain;        // ...to show it again
	CPpMemoryDialog* iMemDialog; // ...the dialog, while it is up
	TInt iMemAgainWait;          // ...seconds waited for the front
	TBool iMemAnswered;          // ...a button of it pressed (not closed by a message)
	TFileName iMemFile;          // ...the file whose open failed
	CPeriodic* iMemLook;         // ...memory looked at while it waits
	TDisplayMode iMode;          // the pages' bitmaps: EColor256 or EGray16
	TInt iPxPerInch;
	// the file
	TFileName iFile;             // the file (open, or on its way)
	TBool iOpenPending;          // iFile is to be opened
	TBool iOpen;                 // it is open
	TBuf<64> iPassword;          // the password for iFile (kept while it is open)
	TInt iPages;
	TInt* iSizes;                // each page's width and height in points x 100 (0: not known yet)
	char iTitle[128];
	TInt iOpenMs;
	// the view
	TInt iPage;
	TInt iZoom;
	TPoint iOrigin;              // the view's top left on the page as drawn
	TBool iAtFoot;               // show the foot of the page when it comes
	TBool iKeepCentre;           // a new zoom: keep what was in the middle
	TBool iKeepOrigin;           // (0.10) the page dropped asleep: drawn again where it was
	TInt iCentreX, iCentreY;     // ...as a part of the page (x 10000)
	TPpPic iShown;               // the page on the screen
	TPpPic iAhead;               // the next page, drawn ahead
	TBool iToolbarShown;
	TBool iKeepLog;              // (0.06) Tools > Keep a log
	TBool iDiagOn;               // (0.06) Coda's debug mode (PsiDiag.on) at the start
	TBool iLogOn;                // either: PsiPDF.log is written
	TBool iBusy;
	CPpBusyWin* iBusyWin;        // (0.15) the steady busy message; 0: EIKON's
	// find
	TBuf<60> iFindText;
	TInt iHitPage;               // the page iHit is on, or -1
	TInt iHits, iHitIndex;
	TInt iHit[PP_MAX_HITS][4];
	TInt iFindFrom;              // the page the search started from
	TInt iFindPending;           // a find to start when the engine is free: its first page + 1, else 0
	TInt iFindAt;                // the page the busy message names
	// settings
	TPpRecentFile iRecent[KPpRecent];
	TInt iRecentCount;
	};

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------

// a name for a dialog line: at most aMax characters, "..." (0x85) in the middle
static void Shorten(TDes& aOut, const TDesC& aIn, TInt aMax)
	{
	if (aIn.Length() <= aMax)
		{
		aOut.Copy(aIn.Left(Min(aIn.Length(), aOut.MaxLength())));
		return;
		}
	TInt head = (aMax - 1) / 2, tail = aMax - 1 - head;
	aOut.Copy(aIn.Left(head));
	aOut.Append(TChar(0x85));
	aOut.Append(aIn.Right(tail));
	}

// UTF-8 to the Psion's code page (Latin-1; anything past it '?')
static void FromUtf8(TDes& aOut, const char* aIn)
	{
	aOut.Zero();
	const TUint8* p = (const TUint8*)aIn;
	while (*p && aOut.Length() < aOut.MaxLength())
		{
		TUint c = *p++;
		if (c >= 0x80)
			{
			TInt more = (c & 0xe0) == 0xc0 ? 1 : (c & 0xf0) == 0xe0 ? 2 : (c & 0xf8) == 0xf0 ? 3 : 0;
			c &= more == 1 ? 0x1f : more == 2 ? 0x0f : 0x07;
			for (TInt i = 0; i < more && (*p & 0xc0) == 0x80; i++)
				c = (c << 6) | (*p++ & 0x3f);
			if (c > 0xff || c < 0x20)
				c = '?';
			}
		aOut.Append(TChar(c));
		}
	}

static void ToUtf8(char* aOut, TInt aMax, const TDesC& aIn)
	{
	TInt n = 0;
	for (TInt i = 0; i < aIn.Length() && n < aMax - 3; i++)
		{
		TUint c = aIn[i];
		if (c < 0x80)
			aOut[n++] = (char)c;
		else
			{
			aOut[n++] = (char)(0xc0 | (c >> 6));
			aOut[n++] = (char)(0x80 | (c & 0x3f));
			}
		}
	aOut[n] = 0;
	}

// (0.09) Arial bold at the height of EIKON's normal font: the system's
// font, so Coda's choice of text applies (as Coda's own programs make it);
// NULL if there is none. Released with ReleaseFont.
static CFont* PpBoldFont()
	{
	CEikonEnv* env = CEikonEnv::Static();
	TFontSpec spec(_L("Arial"), env->ScreenDevice()->VerticalPixelsToTwips(env->NormalFont()->HeightInPixels()));
	spec.iFontStyle.SetStrokeWeight(EStrokeWeightBold);
	CFont* f = NULL;
	if (env->ScreenDevice()->GetNearestFontInTwips(f, spec) != KErrNone)
		return NULL;
	return f;
	}

// (0.09) PsiPDF's dialogs: a fixed width each (never sized by what is in
// them, Dan's rule), centred, inside the screen on either machine
class CPpFixedDialog : public CEikDialog
	{
public:
	CPpFixedDialog(TInt aWidth) : iWidth(aWidth) {}
protected:
	void SetSizeAndPositionL(const TSize& aSize)
		{
		TSize screen = iEikonEnv->ScreenDevice()->SizeInPixels();
		TSize size(Min(iWidth, screen.iWidth - 16), Min(aSize.iHeight, screen.iHeight - 8));
		SetCornerAndSizeL(EHCenterVCenter, size);
		}
	TInt iWidth;
	};

class CPpGoToDialog : public CPpFixedDialog
	{
public:
	CPpGoToDialog(TInt& aPage, TInt aPages) : CPpFixedDialog(300), iPage(aPage), iPages(aPages) {}
private:
	void PreLayoutDynInitL()
		{
		CEikNumberEditor* e = (CEikNumberEditor*)Control(EPpDlgPage);
		e->SetMinimumAndMaximum(1, iPages);
		e->SetNumber(iPage);
		TBuf<40> t;
		if (iPages == 1)
			t = _L("This file has 1 page");
		else
			t.Format(_L("This file has %d pages"), iPages);
		((CEikLabel*)Control(EPpDlgPageCount))->SetTextL(t);
		}
	TBool OkToExitL(TInt /*aButtonId*/)
		{
		iPage = ((CEikNumberEditor*)Control(EPpDlgPage))->Number();
		return ETrue;
		}
	TInt& iPage;
	TInt iPages;
	};

class CPpFindDialog : public CPpFixedDialog
	{
public:
	CPpFindDialog(TDes& aText) : CPpFixedDialog(440), iText(aText) {}
private:
	void PreLayoutDynInitL()
		{
		CEikEdwin* e = (CEikEdwin*)Control(EPpDlgFindText);
		e->SetTextL(&iText);
		e->SelectAllL();
		}
	TBool OkToExitL(TInt /*aButtonId*/)
		{
		TBuf<60> t;
		((CEikEdwin*)Control(EPpDlgFindText))->GetText(t);
		t.TrimAll();
		if (t.Length() == 0)
			{
			iEikonEnv->InfoMsg(_L("No text entered"));
			return EFalse;
			}
		iText = t;
		return ETrue;
		}
	TDes& iText;
	};

class CPpPasswordDialog : public CPpFixedDialog
	{
public:
	CPpPasswordDialog(const TDesC& aFile, TDes& aPassword, TBool aAgain) : CPpFixedDialog(480), iFile(aFile), iPassword(aPassword), iAgain(aAgain) {}
private:
	void PreLayoutDynInitL()
		{
		// (fits the label's 40: "Wrong password for" and the quotes take 21)
		TParsePtrC p(iFile);
		TBuf<32> name;
		Shorten(name, p.NameAndExt(), 19);
		TBuf<64> t;
		t.Format(iAgain ? _L("Wrong password for \"%S\"") : _L("\"%S\" needs a password"), &name);
		((CEikLabel*)Control(EPpDlgPasswordFile))->SetTextL(t);
		}
	TBool OkToExitL(TInt /*aButtonId*/)
		{
		((CEikSecretEditor*)Control(EPpDlgPassword))->GetText(iPassword);
		return ETrue;
		}
	const TDesC& iFile;
	TDes& iPassword;
	TBool iAgain;
	};

// Tools > About PsiPDF: the same layout as every Coda program's About
// (0.09): the name and version in bold, what it is, the copyright, the
// coffee, then the credits and licences in the small font. A fixed width.
class CPpAboutDialog : public CPpFixedDialog
	{
public:
	CPpAboutDialog() : CPpFixedDialog(520) {}
	~CPpAboutDialog()
		{
		if (iBold)
			iEikonEnv->ScreenDevice()->ReleaseFont(iBold);
		}
private:
	void PreLayoutDynInitL()
		{
		TBuf<20> name;
		name.Format(_L("PsiPDF %S"), &KPpVersion);
		CEikLabel* l = (CEikLabel*)Control(EPpDlgAboutName);
		l->SetTextL(name);
		iBold = PpBoldFont();
		if (iBold)
			l->SetFont(iBold);
		TBuf<60> t;
		t.Append(TChar(0xa9));               // the copyright sign, in the Psion's fonts
		t.Append(_L(" Dan Edge 2026"));
		// "part of Coda" only on Coda's netBook (its image has PsiBattHook.bin),
		// as every bundled program's About (psion-core psiabout.h)
		TEntry e;
		if (iEikonEnv->FsSession().Entry(_L("Z:\\System\\Libs\\PsiBattHook.bin"), e) == KErrNone)
			t.Append(_L(" - part of Coda for the Psion netBook"));
		((CEikLabel*)Control(EPpDlgAboutCopy))->SetTextL(t);
		TBuf<60> m(_L("Uses MuPDF 1.3, "));
		m.Append(TChar(0xa9));
		m.Append(_L(" Artifex Software - AGPL-3.0"));
		((CEikLabel*)Control(EPpDlgAboutMuPdf))->SetTextL(m);
		TBuf<100> sy(_L("This product includes software licensed from Symbian Ltd "));
		sy.Append(TChar(0xa9));
		sy.Append(_L(" Symbian Ltd 1998-9"));
		((CEikLabel*)Control(EPpDlgAboutSymbian))->SetTextL(sy);
		}
	CFont* iBold;
	};

class CPpDetailsDialog : public CPpFixedDialog
	{
public:
	CPpDetailsDialog(const TDesC& a1, const TDesC& a2, const TDesC& a3, const TDesC& a4, const TDesC& a5) : CPpFixedDialog(440), i1(a1), i2(a2), i3(a3), i4(a4), i5(a5) {}
private:
	void PreLayoutDynInitL()
		{
		((CEikLabel*)Control(EPpDlgDetails1))->SetTextL(i1);
		((CEikLabel*)Control(EPpDlgDetails2))->SetTextL(i2);
		((CEikLabel*)Control(EPpDlgDetails3))->SetTextL(i3);
		((CEikLabel*)Control(EPpDlgDetails4))->SetTextL(i4);
		((CEikLabel*)Control(EPpDlgDetails5))->SetTextL(i5);
		}
	const TDesC& i1;
	const TDesC& i2;
	const TDesC& i3;
	const TDesC& i4;
	const TDesC& i5;
	};

class CPpDocument : public CEikDocument
	{
public:
	CPpDocument(CEikApplication& aApp) : CEikDocument(aApp) {}
	// a PDF is not an EPOC document: it is not opened as a store
	CFileStore* OpenFileL(TBool /*aDoOpen*/, const TDesC& /*aFilename*/, RFs& /*aFs*/) { return NULL; }
private:
	CEikAppUi* CreateAppUiL() { return new(ELeave) CPpAppUi; }
	};

class CPpApplication : public CEikApplication
	{
private:
	CApaDocument* CreateDocumentL() { return new(ELeave) CPpDocument(*this); }
	TUid AppDllUid() const { return KUidPsiPdf; }
	};

// ===========================================================================
// The view
// ===========================================================================

void CPpView::ConstructL(const TRect& aRect)
	{
	CreateWindowL();
	EnableDragEvents();
	SetRectL(aRect);
	ActivateL();
	}

void CPpView::Draw(const TRect& aRect) const
	{
	iUi.DrawPage(SystemGc(), aRect);
	}

TKeyResponse CPpView::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
	{
	if (aType != EEventKey)
		return EKeyWasNotConsumed;
	return iUi.KeyL(aKeyEvent);
	}

// the pen: drag to move round the page; a tap on a dog-ear turns it
void CPpView::HandlePointerEventL(const TPointerEvent& aPointerEvent)
	{
	switch (aPointerEvent.iType)
		{
	case TPointerEvent::EButton1Down:
		iDragFrom = aPointerEvent.iPosition;
		iDragged = EFalse;
		break;
	case TPointerEvent::EDrag:
		{
		TPoint d = iDragFrom - aPointerEvent.iPosition;
		if (!iDragged && Abs(d.iX) < 4 && Abs(d.iY) < 4)
			break;
		iDragged = ETrue;
		iDragFrom = aPointerEvent.iPosition;
		iUi.PanL(d.iX, d.iY);
		break;
		}
	case TPointerEvent::EButton1Up:
		if (!iDragged)
			iUi.TapL(aPointerEvent.iPosition);
		break;
	default:
		break;
		}
	}

void CPpWatcher::RunL()
	{
	iUi.EngineEnded();
	}

// ===========================================================================
// The program
// ===========================================================================

CPpAppUi::~CPpAppUi()
	{
	PsiMemUnregister(iMemChunk, KUidPsiPdf.iUid);
	StopEngine();
	delete iIdle;
	delete iSoon;
	delete iIdleStopper;
	delete iMemLook;
	delete iMemAgain;
	delete iTick;
	delete iWaiter;
	delete iWatcher;
	iRinger.Close();
	if (iChunkOpen)
		iChunk.Close();
	DropPic(iShown);
	DropPic(iAhead);
	delete[] iSizes;
	delete iBusyWin;
	if (iView)
		{
		RemoveFromStack(iView);
		delete iView;
		}
	}

void CPpAppUi::ConstructL()
	{
	BaseConstructL();
	iShown.iPage = iAhead.iPage = -1;
	iHitPage = -1;
	iForeground = ETrue;
	TDisplayMode mode = iEikonEnv->ScreenDevice()->DisplayMode();
	TBool colour = mode == EColor256 || mode == EColor4K || mode == EColor64K || mode == EColor16M;
	iMode = colour ? EColor256 : EGray16;
	iPxPerInch = iEikonEnv->ScreenDevice()->HorizontalTwipsToPixels(1440);
	if (iPxPerInch < 40 || iPxPerInch > 300)
		iPxPerInch = 96;
	iTick = CPeriodic::NewL(CActive::EPriorityStandard);
	iIdle = CIdle::NewL(CActive::EPriorityStandard);
	iSoon = CPeriodic::NewL(CActive::EPriorityStandard);
	iIdleStopper = CPeriodic::NewL(CActive::EPriorityIdle);
	iMemLook = CPeriodic::NewL(CActive::EPriorityIdle);
	iMemAgain = CPeriodic::NewL(CActive::EPriorityIdle);
	iMaxBytes = KPpMaxBytes;
	iWaiter = new(ELeave) CPsiBellWaiter(TCallBack(BellCallback, this));
	iWatcher = new(ELeave) CPpWatcher(*this);
	LoadSettings();
	TEntry diag;
	iDiagOn = iCoeEnv->FsSession().Entry(KPpDiagOn, diag) == KErrNone;
	iLogOn = iKeepLog || iDiagOn;
	// the last session's log kept as PsiPDF.old (when there is a log)
	if (iLogOn)
		{
		iCoeEnv->FsSession().Delete(KPpLogOld);
		iCoeEnv->FsSession().Rename(KPpLog, KPpLogOld);
		}
	RestoreRecogniser();
	TRAPD(err, MakeToolBarL());
	TRAP(err, ButtonPictureL(EPpCmdPrev, EMbmPpPrev);
		ButtonPictureL(EPpCmdNext, EMbmPpNext);
		ButtonPictureL(EPpCmdZoomIn, EMbmPpZoomIn);
		ButtonPictureL(EPpCmdZoomOut, EMbmPpZoomOut));
	(void)err;                           // (words alone, if the pictures are missing)
	iEikonEnv->Process()->SetMainDocFileName(_L("PsiPDF"));
	if (Bar())
		{
		CEikFileNameLabel* label = (CEikFileNameLabel*)Bar()->ControlById(EPpCmdFileName);
		if (label)
			TRAP(err, label->UpdateL());
		if (!iToolbarShown)
			Bar()->MakeVisible(EFalse);
		}
	iView = new(ELeave) CPpView(*this);
	iView->ConstructL(ViewRect(iToolbarShown));
	AddToStackL(iView);
	TBuf<80> l;
	l.Format(_L("PsiPDF %S, screen %d bits, %d pixels an inch"), &KPpVersion, iMode == EColor256 ? 8 : 4, iPxPerInch);
	Log(l);
	// (0.10) on the memory watcher's list (psimemlvl.h): PsiPDF takes
	// "trim" and "sleep" (ProcessMessageL), and says when it is busy
	PsiMemRegister(iMemChunk, KUidPsiPdf.iUid);
	}

// The System screen (or PsiHub) starts PsiPDF with a file's name; started
// from its icon it reopens the last file. There is no EPOC document.
TBool CPpAppUi::ProcessCommandParametersL(TApaCommand aCommand, TFileName& aDocumentName, const TDesC8& /*aTail*/)
	{
	TBool named = aCommand != EApaCommandCreate && aDocumentName.Length() &&
		BaflUtils::FileExists(iCoeEnv->FsSession(), aDocumentName);
	if (named)
		OpenPdfL(aDocumentName);
	else if (iRecentCount && BaflUtils::FileExists(iCoeEnv->FsSession(), iRecent[0].iFile))
		{
		// (0.06) not the file that ran out of memory, or ended the engine,
		// last time before its page was shown: it would only do it again
		if (WasTrying(iRecent[0].iFile))
			{
			TBuf<300> l;
			l.Format(_L("not reopened (its page was not shown last time): %S"), &iRecent[0].iFile);
			Log(l);
			iCoeEnv->FsSession().Delete(KPpTrying);
			iNote = _L("Last file not opened again");
			iNoteWhy = _L("It did not show last time - open it with Ctrl+O");
			}
		else
			OpenPdfL(iRecent[0].iFile);
		}
	aDocumentName.Zero();
	return EFalse;
	}

// another PDF tapped while PsiPDF is open
void CPpAppUi::OpenFileL(const TDesC& aFileName)
	{
	OpenPdfL(aFileName);
	}

void CPpAppUi::ButtonPictureL(TInt aId, TInt aIcon)
	{
	CEikCommandButton* b = Bar() ? (CEikCommandButton*)Bar()->ControlById(aId) : NULL;
	if (!b)
		return;
	TFileName mbm = Application()->BitmapStoreName();
	CFbsBitmap* bmp = iEikonEnv->CreateBitmapL(mbm, aIcon);
	CleanupStack::PushL(bmp);
	CFbsBitmap* mask = iEikonEnv->CreateBitmapL(mbm, aIcon + 1);
	CleanupStack::PushL(mask);
	b->SetPictureL(bmp, mask);
	CleanupStack::Pop(2);
	if (b->Picture())
		b->Picture()->SetAlignment(EHCenterVCenter);
	if (b->Label())
		b->Label()->SetAlignment(EHLeftVCenter);
	b->LayoutComponentsL();
	}

// (0.09) The toolbar for the machine PsiPDF runs on (the Coda sheet 2.2,
// 2.13; the same choice as psion-core's psitbar.h, which the other
// bundled programs use): the 5mx's 240 lines take r_pp_toolbar, buttons
// of 37 and no page label; 480 lines take buttons of 50 and the page,
// with, on Coda's image, Coda's battery, gauges and clock (r_pp_toolbar_coda).
// Coda is told by Z:\System\Libs\PsiBattHook.bin, which the image has
// only together with the hook in Eikon that makes those three controls.
// EIK_APP_INFO has no toolbar: this one is iToolBar, which CEikAppUi owns.
_LIT(KPpCodaHook, "Z:\\System\\Libs\\PsiBattHook.bin");
void CPpAppUi::MakeToolBarL()
	{
	TSize screen = iEikonEnv->ScreenDevice()->SizeInPixels();
	TInt res = R_PP_TOOLBAR;
	if (screen.iHeight >= 400)
		{
		TEntry e;
		res = iCoeEnv->FsSession().Entry(KPpCodaHook, e) == KErrNone ? R_PP_TOOLBAR_CODA : R_PP_TOOLBAR_NB;
		}
	CEikToolBar* bar = new(ELeave) CEikToolBar;
	CleanupStack::PushL(bar);
	bar->ConstructL(this, res, TRect(TPoint(0, 0), screen));
	CleanupStack::Pop();
	iToolBar = bar;
	}

// the view's room: the screen, less the toolbar's width when it is shown
TRect CPpAppUi::ViewRect(TBool aToolbar) const
	{
	TRect r = ClientRect();
	r.iBr.iX = iEikonEnv->ScreenDevice()->SizeInPixels().iWidth;
	if (aToolbar && Bar())
		r.iBr.iX -= Bar()->Size().iWidth;
	return r;
	}

void CPpAppUi::ShowToolBarL(TBool aShow)
	{
	if (!Bar())
		return;
	iToolbarShown = aShow;
	Bar()->MakeVisible(aShow);
	iView->SetRectL(ViewRect(aShow));
	ClampOrigin();
	iView->DrawNow();
	SaveSettings();
	PumpSoon();                          // (a fitted zoom is fitted to the new room)
	}

// ----- the engine ------------------------------------------------------------

void CPpAppUi::StartEngineL()
	{
	if (iRunning)
		return;
	if (!iChunkOpen)
		{
		TBuf<32> name;
		TInt r = KErrAlreadyExists;
		for (TInt k = 0; k < 8 && r == KErrAlreadyExists; k++)
			{
			name.Format(_L("PsiPdf%08x"), User::TickCount() + k);
			r = iChunk.CreateGlobal(name, sizeof(PpShared), sizeof(PpShared));
			}
		User::LeaveIfError(r);
		iChunkOpen = ETrue;
		iS = (PpShared*)iChunk.Base();
		}
	Mem::FillZ(iS, sizeof(PpShared));
	iS->magic = PP_MAGIC;
	iS->protocol = PP_PROTOCOL;
	iS->app_tid = PsiBellMyTid();
	iRinger.Close();
	iWaiter->Cancel();
	// the engine: beside the program, else in PsiPDF's folder on any disk
	// (installed on another disk than the program)
	TParse p;
	TFileName app = Application()->AppFullName();
	p.Set(KPpEngine, &app, NULL);
	TFileName exe = p.FullName();
	if (!BaflUtils::FileExists(iCoeEnv->FsSession(), exe))
		{
		TFindFile find(iCoeEnv->FsSession());
		if (find.FindByDir(_L("\\System\\Apps\\PsiPDF\\psipdf.exe"), KNullDesC) == KErrNone)
			exe = find.File();
		}
	TInt r = iProcess.Create(exe, iChunk.Name());
	if (r != KErrNone)
		{
		TBuf<120> l;
		l.Format(_L("engine: could not start %S (%d), free RAM %d KB"), &exe, r, FreeRamKb());
		Log(l);
		User::Leave(r);
		}
	iRunning = ETrue;
	iReady = EFalse;
	iJob = 0;
	EnginePriority();
	iWatcher->Watch(iProcess);
	iStartTick = iProgressTick = User::TickCount();
	iProcess.Resume();
	iWaiter->Arm(&iS->app_bell);
	if (!iTick->IsActive())
		iTick->Start(KPpPollTick, KPpPollTick, TCallBack(TickCallback, this));
	TBuf<120> l;
	l.Format(_L("engine: started %S, free RAM %d KB"), &exe, FreeRamKb());
	Log(l);
	}

// The engine's process at PsiPDF's own level: Foreground while PsiPDF is in
// front, else Background (its thread is a step below the program's: keys,
// the pen and the screen still never wait for MuPDF). It was always Low,
// below every program in the background: one of them busy (a sync, a web
// page) and the engine never ran at all, not even to say it was ready, and
// the page stayed blank.
void CPpAppUi::EnginePriority()
	{
	if (iRunning)
		iProcess.SetPriority(iForeground ? ::EPriorityForeground : ::EPriorityBackground);
	}

TInt CPpAppUi::FreeRamKb() const
	{
	TMemoryInfoV1Buf mem;
	UserHal::MemoryInfo(mem);
	return mem().iFreeRamInBytes / 1024;
	}

void CPpAppUi::StopEngine()
	{
	if (!iRunning)
		return;
	iWatcher->Cancel();
	iS->cookie.abort = 1;
	iS->cmd = PP_CMD_QUIT;
	iS->cmd_seq++;
	// (0.14, RN-25) its last steps at the program's own priority: below
	// every program's, as while it draws, it could wait behind a busy one
	// (PsiLinux, PsiCast) past the time given here, and was killed
	iProcess.SetPriority(::EPriorityForeground);
	iRinger.Ring(&iS->eng_bell);
	TInt start = User::TickCount();
	for (TInt i = 0; i < 30 && iS->state != PP_STATE_EXITED; i++)
		User::After(100000);
	// (PP_STATE_EXITED is said just before the engine's last steps: it has
	// a second more to be gone, else it is ended here, never left behind)
	for (TInt j = 0; j < 10 && iS->state == PP_STATE_EXITED && iProcess.ExitType() == EExitPending; j++)
		User::After(100000);
	if (iProcess.ExitType() == EExitPending)
		{
		TBuf<60> l;
		l.Format(_L("engine: did not end (state %d): killed"), iS->state);
		Log(l);
		iProcess.Kill(0);
		}
	else
		{
		TBuf<40> l;
		l.Format(_L("engine: ended in %d ms"), (TInt)((User::TickCount() - start) * 1000 / 64));
		Log(l);
		}
	iProcess.Close();
	// (the bell's handle on the engine's thread kept the ended engine's
	// process, and so its 1.4 MB of code, in memory until PsiPDF closed)
	iRinger.Close();
	iRunning = EFalse;
	iReady = EFalse;
	iJob = 0;
	iWaiter->Cancel();
	iTick->Cancel();
	}

void CPpAppUi::EngineEnded()
	{
	TExitType type = iProcess.ExitType();
	TExitCategoryName cat = iProcess.ExitCategory();
	TInt reason = iProcess.ExitReason();
	iProcess.Close();
	iRinger.Close();
	iRunning = EFalse;
	iReady = EFalse;
	TInt job = iJob;
	iJob = 0;
	iWaiter->Cancel();
	iTick->Cancel();
	BusyEnd();
	TBuf<80> l;
	l.Format(_L("engine: ended (%d %S %d) during command %d"), type, &cat, reason, job);
	Log(l);
	if (iJobPic)
		{
		iJobPic->iDone = iJobPic->iFailed = ETrue;
		iJobPic = NULL;
		}
	if (iOpen || iOpenPending)
		{
		// (not started again for this file: the same page would end it again)
		iEngineFailed = ETrue;
		iOpen = EFalse;
		iOpenPending = EFalse;
		DropPic(iShown);
		DropPic(iAhead);
		TBuf<100> why;
		if (type == EExitPanic)
			why = _L("The file may be damaged");   // (the panic is in the log)
		else
			why = _L("There may not be enough memory - close other programs");
		CannotShow(_L("Cannot show this page"), why);
		}
	}

// The page cannot be shown: the dialog says why, and so does the page's
// place on the screen once it has gone (never a blank page)
void CPpAppUi::CannotShow(const TDesC& aWhat, const TDesC& aWhy)
	{
	BusyEnd();
	// (Left() panics when asked for more than there is)
	iNote = aWhat.Left(Min(aWhat.Length(), iNote.MaxLength()));
	iNoteWhy = aWhy.Left(Min(aWhy.Length(), iNoteWhy.MaxLength()));
	TBuf<180> l;
	l.Format(_L("shown: %S - %S"), &iNote, &iNoteWhy);
	Log(l);
	iView->DrawNow();
	if (aWhat.Find(_L("Not enough memory")) == 0)
		TellShort();
	TRAPD(err, iEikonEnv->InfoWinL(iNote, iNoteWhy));
	}

// (0.13) Not enough memory for the page (Dan's netBook, BETA 1.08: Linux
// held 13.5 MB behind). Said in a dialog and on the page; the dialog's
// "Free memory..." opens Coda's Free memory (PsiHub) where it runs, which
// can put the programs behind to sleep or close them. The page is tried
// again by itself once memory comes back (MemLookCallback), on coming back
// to the front, or at a tap or Enter: before, it stayed blank until the
// page was turned and turned back.
void CPpAppUi::MemoryShort(TInt aKind, const TDesC& aWhat)
	{
	BusyEnd();
	iMemShort = aKind;
	iMemShortKb = FreeRamKb();
	// (the level as it is now: the service's may be from before the page
	// began, when less or more was free)
	iMemShortLevel = PsiMemLevelNow() >= 0 ? PsiMemLevelOf(iMemShortKb * 1024) : -1;
	iNote = aWhat.Left(Min(aWhat.Length(), iNote.MaxLength()));
	iNoteWhy = _L("It shows by itself when memory is free - tap to try now");
	TBuf<180> l;
	l.Format(_L("shown: %S - %S (free RAM %d KB, level %d)"), &iNote, &iNoteWhy, iMemShortKb, iMemShortLevel);
	Log(l);
	iView->DrawNow();
	if (!iMemLook->IsActive())
		iMemLook->Start(KPpMemLookUs, KPpMemLookUs, TCallBack(MemLookCallback, this));
	// tried again by itself and still too little: the page says so, no
	// second dialog (unless the first was closed before it was answered)
	TBool again = iMemAuto;
	iMemAuto = EFalse;
	if (again && (iMemAnswered || iMemDialogAgain >= 3))
		return;
	if (again)
		iMemDialogAgain++;
	else
		{
		iMemDialogAgain = 0;
		iMemAnswered = EFalse;
		}
	MemoryDialog(aWhat, _L("Free memory from other programs - the page then shows by itself"));
	}

// (0.13) Not enough memory, with Coda's Free memory one button away. A
// fixed width, as PsiPDF's other dialogs.
class CPpMemoryDialog : public CPpFixedDialog
	{
public:
	CPpMemoryDialog(const TDesC& aWhat, const TDesC& aWhy, TBool& aFree, TBool& aPressed) : CPpFixedDialog(500), iWhat(aWhat), iWhy(aWhy), iFree(aFree), iPressed(aPressed), iAnswered(EFalse) {}
	// memory came back while it was up: it goes, and the page is shown
	void Close()
		{
		TRAPD(err, TryExitL(EEikBidOk));
		}
	// (Esc is Continue too: there is no Cancel)
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
		{
		// (EIKON closes a waiting dialog with Esc when a window-group
		// message comes: Esc is not counted as an answer)
		if (aType == EEventKey && aKeyEvent.iCode != EKeyEscape)
			iAnswered = ETrue;
		if (aType == EEventKey && aKeyEvent.iCode == EKeyEscape)
			{
			TryExitL(EEikBidOk);
			return EKeyWasConsumed;
			}
		return CPpFixedDialog::OfferKeyEventL(aKeyEvent, aType);
		}
	void HandlePointerEventL(const TPointerEvent& aPointerEvent)
		{
		if (aPointerEvent.iType == TPointerEvent::EButton1Down)
			iAnswered = ETrue;
		CPpFixedDialog::HandlePointerEventL(aPointerEvent);
		}
private:
	void PreLayoutDynInitL()
		{
		// (the title stays the resource's: SetTitleL writes into its buffer)
		((CEikLabel*)Control(EPpDlgMemoryWhat))->SetTextL(iWhat.Left(Min(iWhat.Length(), 48)));
		((CEikLabel*)Control(EPpDlgMemoryWhy))->SetTextL(iWhy.Left(Min(iWhy.Length(), 80)));
		}
	TBool OkToExitL(TInt aButtonId)
		{
		// (an answer is a key other than Esc, or a tap)
		iFree = aButtonId == EPpBidFreeMemory;
		iPressed = iAnswered || iFree;
		return ETrue;
		}
	const TDesC& iWhat;
	const TDesC& iWhy;
	TBool& iFree;
	TBool& iPressed;
	TBool iAnswered;            // a key or a tap reached it
	};

void CPpAppUi::MemoryDialog(const TDesC& aWhat, const TDesC& aWhy)
	{
	const TUid watcher = { KPsiMemWatcherUid };
	TApaTaskList tasks(iCoeEnv->WsSession());
	if (!tasks.FindApp(watcher).Exists())
		{
		// (no Free memory to offer: the 5mx without Coda's PsiHub)
		TRAPD(err, iEikonEnv->InfoWinL(aWhat, _L("Close other programs - the page then shows by itself")));
		return;
		}
	TBool free = EFalse, pressed = EFalse;
	CPpMemoryDialog* d = new CPpMemoryDialog(aWhat, aWhy, free, pressed);
	if (!d)
		return;
	iMemWhat = aWhat.Left(Min(aWhat.Length(), iMemWhat.MaxLength()));
	iMemDialogUp = ETrue;                    // (not tried again under it)
	iMemDialog = d;
	TRAPD(err, d->ExecuteLD(R_PP_MEMORY_DIALOG));
	iMemDialog = NULL;
	iMemDialogUp = EFalse;
	if (pressed)
		iMemAnswered = ETrue;
	// (EIKON closes a waiting dialog, with no button, when a window-group
	// message comes, and PsiHub's "trim" and "sleep" come just as memory
	// runs out: shown again a moment later, once the message has been
	// read, a few times while the page still waits)
	if (err == KErrNone && !pressed && iMemShort && iMemDialogAgain < 3)
		{
		iMemDialogAgain++;
		iMemAgainWait = 0;
		iMemAgain->Cancel();
		iMemAgain->Start(1000000, 1000000, TCallBack(MemAgainCallback, this));
		}
	if (err != KErrNone)
		{
		TBuf<40> l;
		l.Format(_L("memory: the dialog left %d"), err);
		Log(l);
		}
	if (err == KErrNone && free)
		{
		Log(_L("memory: Free memory asked for"));
		PsiMemTell(iCoeEnv->WsSession(), KPsiMemDialog);
		PsiMemFront(iCoeEnv->WsSession());   // (its dialog up already, behind: to the front)
		}
	}

TInt CPpAppUi::MemAgainCallback(TAny* aSelf)
	{
	CPpAppUi* ui = (CPpAppUi*)aSelf;
	// (PsiHub's "Memory low" takes the front for a moment as it comes: a
	// few seconds are waited for PsiPDF to be in front again)
	if (!ui->iForeground && ++ui->iMemAgainWait < 6)
		return 1;
	ui->iMemAgain->Cancel();
	ui->iMemAgainWait = 0;
	if (ui->iMemShort && ui->iForeground && !ui->iMemDialogUp && !ui->iCommands)
		ui->MemoryDialog(ui->iMemWhat, _L("Free memory from other programs - the page then shows by itself"));
	return 0;
	}

// (0.13) the page (or the file's open) tried again: asked (a tap, Enter),
// or by itself (memory back, PsiPDF in front again)
void CPpAppUi::MemRetry(TBool aAsked)
	{
	TInt kind = iMemShort;
	if (kind == EPpMemNone)
		return;
	iMemShort = EPpMemNone;
	iMemLook->Cancel();
	iMemAuto = !aAsked;
	TPtrC how(aAsked ? _L("asked") : _L("memory back"));
	TBuf<80> l;
	l.Format(_L("memory: tried again (%S), free RAM %d KB"), &how, FreeRamKb());
	Log(l);
	iNote.Zero();
	iNoteWhy.Zero();
	if (kind == EPpMemOpen)
		{
		TFileName f(iMemFile);
		TRAPD(err, OpenPdfL(f));
		iMemAuto = !aAsked;                  // (OpenPdfL starts afresh)
		return;
		}
	if (!iOpen)
		return;
	DropPic(iShown);
	iView->DrawNow();
	Pump();
	}

// (0.13) every 2 s while a page waits for memory: in front, it is tried
// again once the memory service's level has dropped, or 1 MB more is free
TInt CPpAppUi::MemLookCallback(TAny* aSelf)
	{
	CPpAppUi* ui = (CPpAppUi*)aSelf;
	if (ui->iMemShort == EPpMemNone)
		{
		ui->iMemLook->Cancel();
		return 0;
		}
	if (!ui->iForeground || ui->iCommands)
		return 1;
	// (a lower level, with more free than at the failure; or 1 MB more)
	TInt level = PsiMemLevelNow(), free = ui->FreeRamKb();
	TBool back = (level >= 0 && ui->iMemShortLevel >= 0 && level < ui->iMemShortLevel && free > ui->iMemShortKb + KPpMemRetryKb / 4)
		|| free >= ui->iMemShortKb + KPpMemRetryKb;
	if (!back)
		{
		// (the free memory measured at the failure can be the engine's
		// worst moment: the mark follows what is free once it has settled,
		// so only memory that really comes back counts)
		if (free > ui->iMemShortKb)
			{
			ui->iMemShortKb = free;
			if (ui->iMemShortLevel >= 0)
				ui->iMemShortLevel = PsiMemLevelOf(free * 1024);
			}
		return 1;
		}
	if (ui->iMemDialogUp)
		{
		// (its dialog still up: closed, and the page tried at the next look)
		ui->iMemAgain->Cancel();
		ui->iMemDialogAgain = 3;
		if (ui->iMemDialog)
			ui->iMemDialog->Close();
		return 1;
		}
	ui->MemRetry(EFalse);
	return 1;
	}

// The engine not ready long after it was started, or its job stuck: stopped
// (killed: it may not be getting the processor at all, so it cannot be asked)
// and the page says why. Only while PsiPDF is in front: behind, the engine
// waits for the programs in front, as it should.
void CPpAppUi::Watchdog()
	{
	if (!iRunning)
		return;
	TUint now = User::TickCount();
	if (!iForeground)
		{
		iStartTick = iProgressTick = now;
		return;
		}
	if (iJob)
		{
		TInt rows = iS->rows_done, progress = iS->cookie.progress, find = iS->find_at;
		if (rows != iSeenRows || progress != iSeenProgress || find != iSeenFind)
			{
			iSeenRows = rows;
			iSeenProgress = progress;
			iSeenFind = find;
			iProgressTick = now;
			}
		}
	TBool late = !iReady && now - iStartTick > (TUint)KPpStartTicks;
	TBool stuck = iReady && iJob && now - iProgressTick > (TUint)KPpStuckTicks;
	if (!late && !stuck)
		return;
	TBuf<120> l;
	TPtrC what(late ? _L("not ready") : _L("no progress"));
	l.Format(_L("engine: %S after %d s (command %d, state %d), free RAM %d KB: stopped"),
		&what, (now - (late ? iStartTick : iProgressTick)) / 64,
		iJob, iS->state, FreeRamKb());
	Log(l);
	TBool wasOpen = iOpen || iOpenPending;
	iWatcher->Cancel();
	iProcess.Kill(KErrTimedOut);
	iProcess.Close();
	iRinger.Close();
	iRunning = EFalse;
	iReady = EFalse;
	iJob = 0;
	iJobPic = NULL;
	iWaiter->Cancel();
	iTick->Cancel();
	iEngineFailed = ETrue;
	iOpen = EFalse;
	iOpenPending = EFalse;
	iFindPending = 0;
	DropPic(iShown);
	DropPic(iAhead);
	if (!wasOpen)
		return;
	TBool lowRam = FreeRamKb() < 3072;
	CannotShow(_L("Cannot show this page"), late ?
		(lowRam ? _L("PsiPDF cannot start drawing - close other programs") : _L("PsiPDF cannot start drawing - another program may be busy")) :
		(lowRam ? _L("PsiPDF stopped drawing - close other programs") : _L("PsiPDF stopped drawing - the page may be too complex")));
	}

void CPpAppUi::Send(TInt aCmd)
	{
	iS->cookie.abort = 0;
	iS->cookie.progress = 0;
	iS->cmd = aCmd;
	iJob = aCmd;
	iStopping = EFalse;
	iEscStopped = EFalse;
	iJobStart = iProgressTick = User::TickCount();
	iSeenRows = iS->rows_done;
	iSeenProgress = 0;
	iSeenFind = iS->find_at;
	iS->cmd_seq++;
	iRinger.Ring(&iS->eng_bell);
	if (!iTick->IsActive())
		iTick->Start(KPpPollTick, KPpPollTick, TCallBack(TickCallback, this));
	}

TInt CPpAppUi::BellCallback(TAny* aSelf)
	{
	((CPpAppUi*)aSelf)->Poll();
	return 0;
	}

TInt CPpAppUi::TickCallback(TAny* aSelf)
	{
	CPpAppUi* ui = (CPpAppUi*)aSelf;
	ui->Poll();
	ui->Watchdog();
	// (the tick is a safety net for a missed ring: only while something is on)
	if (ui->iRunning && ui->iReady && !ui->iJob)
		ui->iTick->Cancel();
	return 0;
	}

// what the engine has done: ready, a band drawn, a command finished
void CPpAppUi::Poll()
	{
	if (!iRunning)
		return;
	do
		{
		if (!iReady && iS->state == PP_STATE_READY)
			{
			iReady = ETrue;
			Log(_L("engine: ready"));
			Pump();
			}
		if (iJob && iS->done_seq == iS->cmd_seq)
			{
			TInt job = iJob;
			iJob = 0;
			TRAPD(err, JobDoneL(job));
			Pump();
			}
		else if (iJob == PP_CMD_FIND && iS->find_at != iFindAt && iS->find_at != iFindFrom)
			{
			iFindAt = iS->find_at;
			TBuf<32> t;
			t.Format(_L("Finding on page %d\x85"), iFindAt + 1);
			Busy(t);
			}
		else if (iJob == PP_CMD_RENDER && iJobPic)
			{
			TInt rows = iS->rows_done;
			if (rows != iJobPic->iRows)
				{
				iJobPic->iRows = rows;
				if (iJobPic == &iShown)
					iView->DrawNow();
				}
			}
		}
	while (iRunning && iWaiter->Arm(&iS->app_bell));
	}

void CPpAppUi::JobDoneL(TInt aCmd)
	{
	PpShared* s = iS;
	TInt ms = (TInt)((User::TickCount() - iJobStart) * 1000 / 64);
	TInt result = s->result;
	TBuf<200> l;
	switch (aCmd)
		{
	case PP_CMD_OPEN:
		if (iReopening)
			{
			iReopening = EFalse;
			l.Format(_L("open again: %d, %d pages, %d ms, heap %d KB, free RAM %d KB"), result, s->pages, ms, s->heap_kb, FreeRamKb());
			Log(l);
			if (result == PP_OK && s->pages == iPages)
				break;                   // (as it was: the page shown, its zoom, Find's hits)
			// (the file has changed, or gone: opened as a new file)
			iOpen = EFalse;
			DropPic(iShown);
			DropPic(iAhead);
			iHitPage = -1;
			iHits = 0;
			iView->DrawNow();
			}
		BusyEnd();
		iOpenMs = ms;
		l.Format(_L("open: %d, %d pages, %d ms, heap %d KB"), result, s->pages, ms, s->heap_kb);
		Log(l);
		if (result == PP_E_PASSWORD)
			{
			iIdle->Cancel();
			iIdle->Start(TCallBack(AskPasswordCallback, this));
			break;
			}
		if (result != PP_OK)
			{
			if (result == PP_E_MEMORY)
				{
				iMemFile = iFile;
				MemoryShort(EPpMemOpen, _L("Not enough memory to show this page"));
				}
			else
				Problem(_L("Cannot open this file"), result);
			iFile.Zero();
			iEikonEnv->Process()->SetMainDocFileName(_L("PsiPDF"));
			UpdateLabelL();
			// (0.10) nothing open: the engine (1.1 MB of code and its heap)
			// is not kept waiting; the next file starts it again
			iIdle->Cancel();
			iIdle->Start(TCallBack(StopIdleCallback, this));
			break;
			}
		OpenDoneL();
		break;
	case PP_CMD_SIZE:
		if (result == PP_OK && s->page >= 0 && s->page < iPages)
			{
			iSizes[2 * s->page] = Max(1, s->page_w);
			iSizes[2 * s->page + 1] = Max(1, s->page_h);
			}
		else if (s->page >= 0 && s->page < iPages)
			{
			// (a page that cannot be loaded: shown as an A4 page, and its drawing fails)
			iSizes[2 * s->page] = 59500;
			iSizes[2 * s->page + 1] = 84200;
			}
		break;
	case PP_CMD_RENDER:
		{
		TPpPic* pic = iJobPic;
		iJobPic = NULL;
		if (!pic)
			break;
		pic->iRows = s->rows_done;
		pic->iMs = ms;
		TPtrC ahead(pic == &iAhead ? _L(" (ahead)") : _L(""));
		l.Format(_L("page %d%S: %d, %d ms (list %d, draw %d: colours %d, rows %d), %dx%d, band %d, heap %d KB, peak %d KB"),
			pic->iPage + 1, &ahead, result, ms, s->list_ms, s->draw_ms,
			s->quant_ms, s->out_ms, pic->iBmp ? pic->iBmp->SizeInPixels().iWidth : 0,
			pic->iBmp ? pic->iBmp->SizeInPixels().iHeight : 0, s->band, s->heap_kb, s->heap_peak_kb);
		Log(l);
		if (result == PP_E_STOPPED)
			{
			if (pic == &iShown && iEscStopped)
				{
				// (asked for: what was drawn is kept, and the rest left white)
				iEscStopped = EFalse;
				pic->iDone = pic->iFailed = pic->iStopped = ETrue;
				BusyEnd();
				MarkTrying(EFalse);
				if (pic->iRows == 0)
					{
					iNote = _L("Drawing stopped");
					iNoteWhy = _L("Enter draws this page again");
					}
				iView->DrawNow();   // (the page says it stopped: no infoprint as well)
				break;
				}
			DropPic(*pic);               // (half a page: not kept)
			break;
			}
		pic->iDone = ETrue;
		pic->iFailed = result != PP_OK;
		if (pic == &iShown)
			{
			BusyEnd();
			// the file showed a page: opened again at the next start
			if (result != PP_E_MEMORY)
				MarkTrying(EFalse);
			if (pic->iFailed && pic->iRows == 0)
				{
				// nothing of it drawn: said on the page too
				if (result == PP_E_MEMORY)
					MemoryShort(EPpMemPage, _L("Not enough memory to show this page"));
				else
					CannotShow(_L("Cannot show this page"), _L("Part of it uses a feature PsiPDF does not have"));
				}
			else if (pic->iFailed)
				PartDrawn(result);
			else
				{
				// (0.13) drawn: nothing waits for memory, and a later failure is said again
				iMemAuto = EFalse;
				iMemShort = EPpMemNone;
				iMemLook->Cancel();
				}
			iView->DrawNow();
			// the 14 standard fonts' file could not be used (the engine drew
			// their text in the one font it keeps): said once
			if (s->font_trouble && !iFontTold)
				{
				iFontTold = ETrue;
				TBool missing = s->font_trouble == PP_FONT_MISSING;
				Log(missing ? _L("fonts: PsiPDF.fnt not found") : _L("fonts: PsiPDF.fnt damaged"));
				TPtrC what(missing ? _L("Part of PsiPDF is missing") : _L("Part of PsiPDF is damaged"));
				TRAPD(err2, iEikonEnv->InfoWinL(what, _L("Some text shows in another typeface - install PsiPDF again")));
				}
			}
		break;
		}
	case PP_CMD_FIND:
		BusyEnd();
		l.Format(_L("find: %d, page %d, %d hits, %d ms"), result, s->found_page + 1, s->hits, ms);
		Log(l);
		if (result == PP_E_STOPPED)
			{
			iEikonEnv->InfoMsg(_L("Find stopped"));
			break;
			}
		if (s->found_page < 0)
			{
			iEikonEnv->InfoMsg(_L("Text not found"));
			break;
			}
		iHitPage = s->found_page;
		iHits = Min(s->hits, PP_MAX_HITS);
		Mem::Copy(iHit, s->hit, sizeof(iHit));
		iHitIndex = 0;
		if (iHitPage < iFindFrom || (iHitPage == iFindFrom && iFindFrom != iPage))
			iEikonEnv->InfoMsg(_L("Found from the start of the file"));
		if (iHitPage != iPage)
			GoToPageL(iHitPage);
		ShowHit();
		break;
	case PP_CMD_TRIM:
		l.Format(_L("trim: free RAM %d -> %d KB, heap %d KB"), iTrimFree, FreeRamKb(), s->heap_kb);
		Log(l);
		break;
	default:
		break;
		}
	}

// the job the engine has is still wanted (else it is stopped)
TBool CPpAppUi::WantJob() const
	{
	if (iJob != PP_CMD_RENDER || !iJobPic)
		return ETrue;
	TInt scale = SizeKnown(iPage) ? ScaleFor(iPage, iZoom) : 0;
	if (iJobPic == &iShown)
		return iShown.iPage == iPage && iShown.iScale == scale;
	// drawing ahead: the page now wanted (it becomes the one shown), or the
	// one after a page on the screen that is done
	if (AheadIsWanted())
		return ETrue;
	return iShown.iDone && iShown.iPage == iPage && iShown.iScale == scale &&
		iAhead.iPage == iPage + 1 && SizeKnown(iPage + 1) && iAhead.iScale == ScaleFor(iPage + 1, iZoom);
	}

// the page drawn ahead (done or not) is the one now wanted
TBool CPpAppUi::AheadIsWanted() const
	{
	return iAhead.iBmp && !iAhead.iFailed && iAhead.iPage == iPage && SizeKnown(iPage) &&
		iAhead.iScale == ScaleFor(iPage, iZoom);
	}

// After a key or a tap: the page drawn ahead shows at once; anything else
// is drawn a moment later, so that several quick presses (Zoom in, Next...)
// draw once, the last
void CPpAppUi::PumpSoon()
	{
	if (AheadIsWanted())
		{
		iSoon->Cancel();
		Pump();
		return;
		}
	if (iRunning && iJob && !WantJob() && !iStopping)
		{
		iS->cookie.abort = 1;            // (stopped now: its end is quick)
		iStopping = ETrue;
		}
	iSoon->Cancel();
	iSoon->Start(KPpSoon, KPpSoon, TCallBack(SoonCallback, this));
	}

TInt CPpAppUi::SoonCallback(TAny* aSelf)
	{
	CPpAppUi* ui = (CPpAppUi*)aSelf;
	ui->iSoon->Cancel();
	ui->Pump();
	return 0;
	}

void CPpAppUi::Pump()
	{
	TRAPD(err, PumpL());
	if (err != KErrNone)
		{
		TBuf<40> l;
		l.Format(_L("pump: left %d"), err);
		Log(l);
		}
	}

// what to ask the engine for next: start it, open the file, a page's size,
// the page shown, or the next one ahead
void CPpAppUi::PumpL()
	{
	if (!iOpenPending && !iOpen)
		return;
	if (!iRunning)
		{
		if (iEngineFailed || iAsleep)
			return;
		TRAPD(err, StartEngineL());
		if (err != KErrNone)
			{
			iEngineFailed = ETrue;
			iOpenPending = EFalse;
			TBuf<80> why;
			if (err == KErrNotFound || err == KErrPathNotFound)
				why = _L("Part of PsiPDF is missing - install PsiPDF again");
			else if (err == KErrNoMemory)
				{
				iMemFile = iFile;
				MemoryShort(EPpMemOpen, _L("Not enough memory to show this page"));
				return;
				}
			else
				{
				why = _L("PsiPDF cannot start drawing");
				PsiErrTail(why, err);       // (0.12: nothing more when the reason is not known)
				TBuf<48> l;
				l.Format(_L("engine: not started (%d)"), err);
				Log(l);
				}
			CannotShow(_L("Cannot show this page"), why);
			}
		return;
		}
	if (!iReady)
		return;
	if (iSoon->IsActive() && !iOpenPending && !AheadIsWanted())
		return;                          // (a key a moment ago: drawn when it fires)
	if (iJob)
		{
		// the page it draws ahead may be the one now wanted
		if (iJob == PP_CMD_RENDER && iJobPic == &iAhead && AheadIsWanted())
			{
			SwapPics();
			Busy(_L("Drawing the page\x85"));
			iView->DrawNow();
			}
		if (!WantJob() && !iStopping)
			{
			iS->cookie.abort = 1;
			iStopping = ETrue;
			}
		return;
		}
	PpShared* s = iS;
	if (iReopen)
		{
		// the engine stopped behind: the file opened again, as it was (the
		// page on the screen stays meanwhile)
		iReopen = EFalse;
		iReopening = ETrue;
		TPtr8 path((TUint8*)s->path, sizeof(s->path) - 1);
		path.Copy(iFile.Left(Min(iFile.Length(), (TInt)sizeof(s->path) - 1)));
		path.ZeroTerminate();
		ToUtf8(s->text, sizeof(s->text), iPassword);
		s->store_kb = iStoreKb;
		Send(PP_CMD_OPEN);
		return;
		}
	if (iReopening)
		return;
	if (iOpenPending && s->stale)
		{
		// (0.10) the last file closed left memory behind in the engine:
		// started afresh for this one
		Log(_L("engine: started again (a closed file left memory behind)"));
		StopEngine();
		Pump();
		return;
		}
	if (iOpenPending)
		{
		iOpenPending = EFalse;
		iOpen = EFalse;
		TPtr8 path((TUint8*)s->path, sizeof(s->path) - 1);
		path.Copy(iFile.Left(Min(iFile.Length(), (TInt)sizeof(s->path) - 1)));
		path.ZeroTerminate();
		ToUtf8(s->text, sizeof(s->text), iPassword);
		// what MuPDF keeps between bands and pages (decoded pictures, fonts):
		// enough for a full-page photo, so it is not decoded again for each
		// band, but no more than a quarter of the free memory (from 1 MB);
		// and the biggest page bitmap, the same
		TInt free = FreeRamKb();
		// (0.14, RN-25) a fifth of the free memory each, not a quarter: on
		// Dan's netBook, open at 8 MB free, PsiPDF took 3.3 MB with a page
		// zoomed (8.0 -> 4.7 MB), the most of any program that night
		iStoreKb = Max(KPpStoreMinKb, Min(iMode == EColor256 ? 3072 : 2048, free / KPpShareOfFree));
		iMaxBytes = Max(KPpMinBytes, Min(KPpMaxBytes, free / KPpShareOfFree * 1024));
		s->store_kb = iStoreKb;
		TBuf<100> l;
		l.Format(_L("memory: free RAM %d KB: store %d KB, pages up to %d KB"), free, iStoreKb, iMaxBytes / 1024);
		Log(l);
		Busy(_L("Opening\x85"));
		Send(PP_CMD_OPEN);
		return;
		}
	if (iFindPending && iOpen)
		{
		StartFind(iFindPending - 1);
		return;
		}
	if (!SizeKnown(iPage))
		{
		s->page = iPage;
		Send(PP_CMD_SIZE);
		return;
		}
	TInt scale = ScaleFor(iPage, iZoom);
	if (iShown.iPage != iPage || iShown.iScale != scale || (!iShown.iDone && !iShown.iBmp))
		{
		// the page ahead, if it is this one
		if (iAhead.iBmp && iAhead.iPage == iPage && iAhead.iScale == scale && iAhead.iDone && !iAhead.iFailed)
			{
			SwapPics();
			DropPic(iAhead);
			ClampOrigin();
			UpdateLabelL();
			iView->DrawNow();
			}
		else
			{
			// what was in the middle stays there (a new zoom); a new page
			// starts at its top (or its foot, going back)
			TPoint was = iOrigin;
			TSize old = ShownSize();
			TBool samePage = iShown.iPage == iPage && iShown.iBmp;
			DropPic(iShown);
			// (the page ahead goes too unless it is the next page at this zoom:
			// one bitmap at a time while a new one is drawn)
			if (iAhead.iBmp && (iAhead.iPage != iPage + 1 || !SizeKnown(iPage + 1) || iAhead.iScale != ScaleFor(iPage + 1, iZoom)))
				DropPic(iAhead);
			TInt r = CreatePic(iShown, iPage, scale);
			if (r == KErrNoMemory && iAhead.iBmp)
				{
				DropPic(iAhead);
				r = CreatePic(iShown, iPage, scale);
				}
			if (r != KErrNone)
				{
				TBuf<80> l;
				l.Format(_L("page %d: no bitmap (%d), free RAM %d KB"), iPage + 1, r, FreeRamKb());
				Log(l);
				if (r == KErrTooBig)
					{
					CannotShow(_L("This page is too big to show"), _L("Its size is more than PsiPDF can draw"));
					return;
					}
				if (iZoom == EZoomFitWidth)
					MemoryShort(EPpMemPage, _L("Not enough memory to show this page"));
				else
					Problem(_L("Not enough memory to show this page"), PP_E_MEMORY);
				if (iZoom != EZoomFitWidth)
					{
					iZoom = EZoomFitWidth;
					Pump();
					}
				return;
				}
			TSize now = ShownSize();
			TSize view = iView->Size();
			if (samePage && iKeepCentre && old.iWidth > 0 && old.iHeight > 0)
				{
				iOrigin.iX = (TInt)((TInt64(was.iX + view.iWidth / 2) * now.iWidth / old.iWidth).GetTInt()) - view.iWidth / 2;
				iOrigin.iY = (TInt)((TInt64(was.iY + view.iHeight / 2) * now.iHeight / old.iHeight).GetTInt()) - view.iHeight / 2;
				}
			else if (!samePage && !iKeepOrigin)
				iOrigin.iY = iAtFoot ? now.iHeight : 0;
			iKeepCentre = EFalse;
			iKeepOrigin = EFalse;
			iAtFoot = EFalse;
			ClampOrigin();
			s->page = iPage;
			s->scale = scale;
			s->bitmap = iShown.iBmp->Handle();
			s->depth = iMode == EColor256 ? PP_DEPTH_COLOUR : PP_DEPTH_GREY;
			s->band = BandFor(iShown.iBmp->SizeInPixels().iWidth);
			iJobPic = &iShown;
			iNote.Zero();                    // (a page that could not be shown before: tried afresh)
			iNoteWhy.Zero();
			Busy(_L("Drawing the page\x85"));
			Send(PP_CMD_RENDER);
			UpdateLabelL();
			iView->DrawNow();
			return;
			}
		}
	if (!iShown.iDone || iShown.iStopped)
		return;
	// drawn: the next page ahead, if there is room for it
	TInt next = iPage + 1;
	if (next >= iPages)
		return;
	if (!SizeKnown(next))
		{
		s->page = next;
		Send(PP_CMD_SIZE);
		return;
		}
	TInt nscale = ScaleFor(next, iZoom);
	if (iAhead.iBmp && iAhead.iPage == next && iAhead.iScale == nscale)
		return;
	DropPic(iAhead);
	TSize ns = PixelsFor(next, nscale);
	TInt bytes = iMode == EColor256 ? ns.iWidth * ns.iHeight : (ns.iWidth + 1) / 2 * ns.iHeight;
	TMemoryInfoV1Buf mem;
	UserHal::MemoryInfo(mem);
	if (bytes > KPpAheadMaxBytes || mem().iFreeRamInBytes < bytes + KPpAheadSpare)
		return;
	if (CreatePic(iAhead, next, nscale) != KErrNone)
		return;
	s->page = next;
	s->scale = nscale;
	s->bitmap = iAhead.iBmp->Handle();
	s->depth = iMode == EColor256 ? PP_DEPTH_COLOUR : PP_DEPTH_GREY;
	s->band = BandFor(ns.iWidth);
	iJobPic = &iAhead;
	Send(PP_CMD_RENDER);
	}

// ----- the file --------------------------------------------------------------

void CPpAppUi::OpenPdfL(const TDesC& aName)
	{
	TParse p;
	if (p.Set(aName, NULL, NULL) != KErrNone)
		return;
	TEntry e;
	TInt there = iCoeEnv->FsSession().Entry(p.FullName(), e);
	if (there == KErrNotReady || there == KErrDisMounted)
		{
		// (the card it is on is out: said, the file shown stays)
		TBuf<64> m;
		PsiErrMsg(m, _L("Not opened"), KErrNotReady);
		iEikonEnv->InfoMsg(m);
		return;
		}
	Remember();
	SaveSettings();
	CloseDocument();
	iFile = p.FullName();
	iPassword.Zero();
	iEngineFailed = EFalse;
	iMemShort = EPpMemNone;                  // (0.13) a page waiting for memory: this file now
	iMemLook->Cancel();
	iPartTold = EFalse;
	MarkTrying(ETrue);
	iNote.Zero();
	iNoteWhy.Zero();
	iOpenPending = ETrue;
	Busy(_L("Opening\x85"));             // (at once: the engine may take a moment to start)
	// its page and zoom from last time
	iPage = 0;
	iZoom = EZoomFitWidth;
	for (TInt i = 0; i < iRecentCount; i++)
		if (iRecent[i].iFile.CompareF(iFile) == 0)
			{
			iPage = iRecent[i].iPage;
			iZoom = iRecent[i].iZoom;
			}
	iOrigin = TPoint(0, 0);
	iEikonEnv->Process()->SetMainDocFileName(iFile);
	UpdateLabelL();
	iView->DrawNow();
	TBuf<300> l;
	l.Format(_L("opening %S"), &iFile);
	Log(l);
	// (a command the engine has: stopped; the open follows)
	if (iRunning && iJob)
		{
		iS->cookie.abort = 1;
		iStopping = ETrue;
		}
	Pump();
	}

void CPpAppUi::CloseDocument()
	{
	iOpen = EFalse;
	iOpenPending = EFalse;
	iAsleep = iReopen = iReopening = EFalse;
	iIdleStopper->Cancel();
	DropPic(iShown);
	DropPic(iAhead);
	delete[] iSizes;
	iSizes = NULL;
	iPages = 0;
	iHitPage = -1;
	iHits = 0;
	iFindPending = 0;
	iTitle[0] = 0;
	if (iRunning && iReady && !iJob)
		Send(PP_CMD_CLOSE);
	}

void CPpAppUi::OpenDoneL()
	{
	// (the file's own count, from MuPDF: kept to what the dialogs and the
	// toolbar's label can show)
	iPages = iS->pages < 0 ? 0 : iS->pages > KPpMaxPages ? KPpMaxPages : iS->pages;
	delete[] iSizes;
	iSizes = NULL;
	iSizes = new(ELeave) TInt[2 * iPages];
	Mem::FillZ(iSizes, 2 * iPages * sizeof(TInt));
	Mem::Copy(iTitle, iS->title, sizeof(iTitle));
	iTitle[sizeof(iTitle) - 1] = 0;
	iOpen = ETrue;
	if (iPage >= iPages || iPage < 0)
		iPage = 0;
	UpdateLabelL();
	}

TInt CPpAppUi::AskPasswordCallback(TAny* aSelf)
	{
	TRAPD(err, ((CPpAppUi*)aSelf)->AskPasswordL());
	return 0;
	}

// (0.10) an open that failed, or a password not given: the engine stopped
// while no file is open or asked for
TInt CPpAppUi::StopIdleCallback(TAny* aSelf)
	{
	CPpAppUi* ui = (CPpAppUi*)aSelf;
	if (ui->iRunning && !ui->iOpen && !ui->iOpenPending && !ui->iJob)
		{
		ui->StopEngine();
		ui->Log(_L("engine: stopped (no file open)"));
		}
	return 0;
	}

void CPpAppUi::AskPasswordL()
	{
	TBuf<64> pw;
	TBool again = iPassword.Length() > 0;
	CEikDialog* d = new(ELeave) CPpPasswordDialog(iFile, pw, again);
	if (d->ExecuteLD(R_PP_PASSWORD_DIALOG) && pw.Length())
		{
		iPassword = pw;
		iOpenPending = ETrue;
		Pump();
		}
	else
		{
		iFile.Zero();
		iPassword.Zero();
		iEikonEnv->Process()->SetMainDocFileName(_L("PsiPDF"));
		UpdateLabelL();
		iView->DrawNow();
		// (0.10) the locked file was left open in the engine, with the
		// engine running for nothing
		StopIdleCallback(this);
		}
	}

// ----- pages -----------------------------------------------------------------

// pixels a point x 10000 for page aPage at aZoom (its size known), kept
// within iMaxBytes (set when the file opened)
TInt CPpAppUi::ScaleFor(TInt aPage, TInt aZoom) const
	{
	TSize view = iView->Size();
	TReal w = iSizes[2 * aPage] / 100.0, h = iSizes[2 * aPage + 1] / 100.0;
	TReal ppi = iPxPerInch / 72.0;
	TReal s;
	switch (aZoom)
		{
	case EZoomFitPage:
		s = view.iWidth / w;
		if (h * s > view.iHeight)
			s = view.iHeight / h;
		break;
	case EZoom100: s = ppi; break;
	case EZoom150: s = ppi * 1.5; break;
	case EZoom200: s = ppi * 2.0; break;
	default: s = view.iWidth / w; break;
		}
	TReal bytes = w * s * h * s * (iMode == EColor256 ? 1.0 : 0.5);
	if (bytes > iMaxBytes)
		{
		TReal f;
		Math::Sqrt(f, iMaxBytes / bytes);
		s *= f;
		}
	TInt r = (TInt)(s * 10000.0);
	return r < 100 ? 100 : r;
	}

// the bitmap for aPage at aScale: as MuPDF rounds the page's box
TSize CPpAppUi::PixelsFor(TInt aPage, TInt aScale) const
	{
	TReal s = aScale / 10000.0;
	// (0.06) worked out in TReal and kept short of what a TInt holds: a
	// page can say it is any size; CreatePic refuses one past KPpMaxSide
	TReal rw = iSizes[2 * aPage] / 100.0 * s + 0.999;
	TReal rh = iSizes[2 * aPage + 1] / 100.0 * s + 0.999;
	TInt w = rw > KPpMaxSide + 1 ? KPpMaxSide + 1 : (TInt)rw;
	TInt h = rh > KPpMaxSide + 1 ? KPpMaxSide + 1 : (TInt)rh;
	return TSize(Max(1, w), Max(1, h));
	}

TInt CPpAppUi::CreatePic(TPpPic& aPic, TInt aPage, TInt aScale)
	{
	DropPic(aPic);
	// (0.06) a huge page: ScaleFor's smallest scale can still be more than
	// the bitmap may take (a page metres long), and is refused, not tried
	TSize size = PixelsFor(aPage, aScale);
	// (each side at most KPpMaxSide + 1: the product fits in a TInt)
	TInt bytes = (iMode == EColor256 ? size.iWidth : (size.iWidth + 1) / 2) * size.iHeight;
	if (size.iWidth > KPpMaxSide || size.iHeight > KPpMaxSide || bytes > iMaxBytes + iMaxBytes / 16)
		return KErrTooBig;
	CFbsBitmap* b = new CFbsBitmap;
	if (!b)
		return KErrNoMemory;
	TInt r = b->Create(size, iMode);
	if (r != KErrNone)
		{
		delete b;
		return r;
		}
	aPic.iBmp = b;
	aPic.iPage = aPage;
	aPic.iScale = aScale;
	aPic.iRows = 0;
	aPic.iDone = aPic.iFailed = aPic.iStopped = EFalse;
	return KErrNone;
	}

void CPpAppUi::DropPic(TPpPic& aPic)
	{
	if (iJobPic == &aPic && iJob == PP_CMD_RENDER && iRunning)
		{
		// (the engine still writes into it: stopped first, and waited for)
		iS->cookie.abort = 1;
		for (TInt i = 0; i < 50 && iS->done_seq != iS->cmd_seq; i++)
			User::After(20000);
		iJobPic = NULL;
		}
	delete aPic.iBmp;
	aPic.iBmp = NULL;
	aPic.iPage = -1;
	aPic.iScale = 0;
	aPic.iRows = 0;
	aPic.iDone = aPic.iFailed = aPic.iStopped = EFalse;
	}

void CPpAppUi::SwapPics()
	{
	TPpPic t = iShown;
	iShown = iAhead;
	iAhead = t;
	if (iJobPic == &iShown)
		iJobPic = &iAhead;
	else if (iJobPic == &iAhead)
		iJobPic = &iShown;
	}

TSize CPpAppUi::ShownSize() const
	{
	return iShown.iBmp && iShown.iPage == iPage ? iShown.iBmp->SizeInPixels() : TSize(0, 0);
	}

// the page's rectangle in the view: centred where it is narrower or
// shorter, else moved by iOrigin
TRect CPpAppUi::PageRect() const
	{
	TRect v = iView->Rect();
	TSize s = ShownSize();
	TPoint tl;
	tl.iX = s.iWidth <= v.Width() ? v.iTl.iX + (v.Width() - s.iWidth) / 2 : v.iTl.iX - iOrigin.iX;
	tl.iY = s.iHeight <= v.Height() ? v.iTl.iY + (v.Height() - s.iHeight) / 2 : v.iTl.iY - iOrigin.iY;
	return TRect(tl, s);
	}

void CPpAppUi::ClampOrigin()
	{
	TSize s = ShownSize();
	TSize v = iView->Size();
	iOrigin.iX = Max(0, Min(iOrigin.iX, s.iWidth - v.iWidth));
	iOrigin.iY = Max(0, Min(iOrigin.iY, s.iHeight - v.iHeight));
	}

void CPpAppUi::GoToPageL(TInt aPage, TBool aAtFoot)
	{
	if (NoFile())
		return;
	aPage = Max(0, Min(aPage, iPages - 1));
	if (aPage == iPage)
		{
		iOrigin.iY = aAtFoot ? ShownSize().iHeight : 0;
		ClampOrigin();
		iView->DrawNow();
		return;
		}
	iPage = aPage;
	iAtFoot = aAtFoot;
	iOrigin.iY = 0;
	UpdateLabelL();
	PumpSoon();
	iView->DrawNow();
	if (!iToolbarShown || !Bar() || !Bar()->ControlById(EPpCmdPageLabel))   // (the 5mx's toolbar has no page label)
		{
		TBuf<32> t;
		t.Format(_L("Page %d of %d"), iPage + 1, iPages);
		iEikonEnv->InfoMsg(t);
		}
	}

void CPpAppUi::TurnL(TInt aStep)
	{
	if (NoFile())
		return;
	TInt to = iPage + aStep;
	if (to < 0)
		iEikonEnv->InfoMsg(_L("This is the first page"));
	else if (to >= iPages)
		iEikonEnv->InfoMsg(_L("This is the last page"));
	else
		GoToPageL(to);
	}

void CPpAppUi::SetZoomL(TInt aZoom)
	{
	if (NoFile())
		return;
	iZoom = aZoom;
	iKeepCentre = ETrue;
	PumpSoon();
	static const TText* const names[EZoomCount] = { _S("Fit width"), _S("Fit page"), _S("100%"), _S("150%"), _S("200%") };
	iEikonEnv->InfoMsg(TPtrC(names[aZoom]));
	}

// Zoom in and out: round the zooms in the order of their size on this page
void CPpAppUi::ZoomStepL(TInt aDir)
	{
	if (NoFile())
		return;
	if (!SizeKnown(iPage))
		return;
	TInt order[EZoomCount], scale[EZoomCount], i, j;
	for (i = 0; i < EZoomCount; i++)
		{
		order[i] = i;
		scale[i] = ScaleFor(iPage, i);
		}
	for (i = 1; i < EZoomCount; i++)
		for (j = i; j > 0 && scale[order[j]] < scale[order[j - 1]]; j--)
			{
			TInt t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
			}
	TInt at = 0;
	for (i = 0; i < EZoomCount; i++)
		if (order[i] == iZoom)
			at = i;
	// (zooms of the same size are one step; it wraps round at the ends, as
	// the EIKON style guide has zooms do)
	TInt cur = scale[iZoom];
	for (i = 1; i <= EZoomCount; i++)
		{
		TInt k = order[(at + aDir * i + 2 * EZoomCount) % EZoomCount];
		if (Abs(scale[k] - cur) * 50 > cur)
			{
			SetZoomL(k);
			return;
			}
		}
	}

void CPpAppUi::UpdateLabelL()
	{
	if (!Bar())
		return;
	CEikFileNameLabel* fl = (CEikFileNameLabel*)Bar()->ControlById(EPpCmdFileName);
	if (fl)
		fl->UpdateL();
	CEikLabel* l = (CEikLabel*)Bar()->ControlById(EPpCmdPageLabel);
	if (!l)
		return;
	TBuf<32> t;
	if (iOpen && iPages > 0)
		t.Format(_L("Page %d\nof %d"), iPage + 1, iPages);
	l->SetTextL(t);
	l->DrawNow();
	}

// ----- drawing ---------------------------------------------------------------

void CPpAppUi::DrawPage(CWindowGc& aGc, const TRect& /*aRect*/) const
	{
	TRect v = iView->Rect();
	TRgb back = iMode == EColor256 ? TRgb(0x999999) : TRgb::Gray4(2);
	aGc.SetPenStyle(CGraphicsContext::ENullPen);
	aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	TSize s = ShownSize();
	// no page to show (or none of it could be drawn): what is happening, or why not
	TBool nothing = iShown.iDone && iShown.iFailed && iShown.iRows == 0 && iNote.Length();
	if (!iShown.iBmp || iShown.iPage != iPage || nothing)
		{
		// (0.09) the workspace's white, as the other programs' empty views
		aGc.SetBrushColor(KRgbWhite);
		aGc.DrawRect(v);
		TPtrC line1, line2;
		if (iNote.Length() && !iOpenPending)
			{
			line1.Set(iNote);
			line2.Set(iNoteWhy);
			}
		else if (!iOpen && !iOpenPending)
			{
			line1.Set(_L("No file open"));
			line2.Set(_L("Tap a PDF file on the System screen, or open one with Ctrl+O"));
			}
		if (line1.Length())
			{
			// two centred lines, the first bold (the Coda sheet's empty state)
			const CFont* f = iEikonEnv->NormalFont();
			CFont* bold = PpBoldFont();
			aGc.SetPenStyle(CGraphicsContext::ESolidPen);
			aGc.SetPenColor(KRgbBlack);
			aGc.SetBrushStyle(CGraphicsContext::ENullBrush);
			TInt base = v.iTl.iY + v.Height() / 2;
			const CFont* f1 = bold ? bold : f;
			aGc.UseFont(f1);
			aGc.DrawText(line1, TRect(v.iTl.iX, base - 22, v.iBr.iX, base), f1->AscentInPixels() + 2, CGraphicsContext::ECenter);
			aGc.UseFont(f);
			aGc.DrawText(line2, TRect(v.iTl.iX, base + 2, v.iBr.iX, base + 24), f->AscentInPixels() + 2, CGraphicsContext::ECenter);
			aGc.DiscardFont();
			if (bold)
				iEikonEnv->ScreenDevice()->ReleaseFont(bold);
			}
		return;
		}
	TRect page = PageRect();
	// round the page: the background, in four strips
	aGc.SetBrushColor(back);
	if (page.iTl.iY > v.iTl.iY)
		aGc.DrawRect(TRect(v.iTl.iX, v.iTl.iY, v.iBr.iX, page.iTl.iY));
	if (page.iBr.iY < v.iBr.iY)
		aGc.DrawRect(TRect(v.iTl.iX, page.iBr.iY, v.iBr.iX, v.iBr.iY));
	TInt top = Max(page.iTl.iY, v.iTl.iY), foot = Min(page.iBr.iY, v.iBr.iY);
	if (page.iTl.iX > v.iTl.iX)
		aGc.DrawRect(TRect(v.iTl.iX, top, page.iTl.iX, foot));
	if (page.iBr.iX < v.iBr.iX)
		aGc.DrawRect(TRect(page.iBr.iX, top, v.iBr.iX, foot));
	// the rows drawn so far, then white
	TRect vis(page);
	vis.Intersection(v);
	TInt drawnFoot = page.iTl.iY + iShown.iRows;
	if (drawnFoot > vis.iTl.iY)
		{
		TRect src(vis.iTl - page.iTl, TSize(vis.Width(), Min(vis.iBr.iY, drawnFoot) - vis.iTl.iY));
		aGc.BitBlt(vis.iTl, iShown.iBmp, src);
		}
	if (drawnFoot < vis.iBr.iY)
		{
		TRect rest(vis.iTl.iX, Max(vis.iTl.iY, drawnFoot), vis.iBr.iX, vis.iBr.iY);
		aGc.SetBrushColor(KRgbWhite);
		aGc.DrawRect(rest);
		// (0.06) stopped with Esc: the part not drawn says so
		if (iShown.iStopped && iShown.iPage == iPage && rest.Height() >= 44)
			{
			const CFont* f = iEikonEnv->NormalFont();
			aGc.UseFont(f);
			aGc.SetPenStyle(CGraphicsContext::ESolidPen);
			aGc.SetPenColor(KRgbBlack);
			aGc.SetBrushStyle(CGraphicsContext::ENullBrush);
			TInt mid = rest.iTl.iY + rest.Height() / 2;
			aGc.DrawText(_L("Drawing stopped"), TRect(v.iTl.iX, mid - 20, v.iBr.iX, mid), f->AscentInPixels() + 2, CGraphicsContext::ECenter);
			aGc.DrawText(_L("Enter draws this page again"), TRect(v.iTl.iX, mid, v.iBr.iX, mid + 20), f->AscentInPixels() + 2, CGraphicsContext::ECenter);
			aGc.DiscardFont();
			aGc.SetPenStyle(CGraphicsContext::ENullPen);
			aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
			}
		}
	// what Find found: shown inverted
	if (iHitPage == iPage && iHits > 0)
		{
		TReal sc = iShown.iScale / 1000000.0;          // (points x 100 to pixels)
		aGc.SetDrawMode(CGraphicsContext::EDrawModeNOTSCREEN);
		aGc.SetBrushColor(KRgbBlack);
		for (TInt i = 0; i < iHits; i++)
			{
			TRect r(page.iTl.iX + (TInt)(iHit[i][0] * sc) - 1, page.iTl.iY + (TInt)(iHit[i][1] * sc) - 1,
				page.iTl.iX + (TInt)(iHit[i][2] * sc + 0.999) + 1, page.iTl.iY + (TInt)(iHit[i][3] * sc + 0.999) + 1);
			r.Intersection(v);
			if (!r.IsEmpty())
				aGc.DrawRect(r);
			}
		aGc.SetDrawMode(CGraphicsContext::EDrawModePEN);
		}
	// the dog-ears: the next page bottom right, the one before bottom left
	aGc.SetPenStyle(CGraphicsContext::ESolidPen);
	aGc.SetPenColor(KRgbBlack);
	aGc.SetBrushColor(iMode == EColor256 ? TRgb(0xcccccc) : TRgb::Gray4(2));
	const TInt e = KPpDogEar;
	if (iPage + 1 < iPages)
		{
		TPoint p[3] = { TPoint(v.iBr.iX - 1, v.iBr.iY - e), TPoint(v.iBr.iX - 1, v.iBr.iY - 1), TPoint(v.iBr.iX - e, v.iBr.iY - 1) };
		aGc.DrawPolygon(p, 3);
		}
	if (iPage > 0)
		{
		TPoint p[3] = { TPoint(v.iTl.iX, v.iBr.iY - e), TPoint(v.iTl.iX, v.iBr.iY - 1), TPoint(v.iTl.iX + e - 1, v.iBr.iY - 1) };
		aGc.DrawPolygon(p, 3);
		}
	}

// ----- keys and the pen ------------------------------------------------------

TKeyResponse CPpAppUi::KeyL(const TKeyEvent& aKeyEvent)
	{
	TBool ctrl = (aKeyEvent.iModifiers & EModifierCtrl) != 0;
	if (aKeyEvent.iCode == EKeyEscape)
		{
		// stops a find, or the page being drawn (0.06: and it stays
		// stopped, what was drawn of it shown, until Enter or another page
		// or zoom; it was drawn again at once)
		if (iRunning && (iJob == PP_CMD_FIND || (iJob == PP_CMD_RENDER && iJobPic == &iShown)))
			{
			iS->cookie.abort = 1;
			if (iJob == PP_CMD_RENDER)
				iEscStopped = ETrue;
			}
		return EKeyWasConsumed;
		}
	if (!iOpen)
		return EKeyWasNotConsumed;
	TSize v = iView->Size();
	TSize s = ShownSize();
	TInt hstep = ctrl ? v.iWidth - 32 : Max(16, v.iWidth / 8);
	TInt vstep = ctrl ? v.iHeight - 32 : Max(16, v.iHeight / 8);
	TBool atTop = iOrigin.iY <= 0 || s.iHeight <= v.iHeight;
	TBool atFoot = iOrigin.iY >= s.iHeight - v.iHeight || s.iHeight <= v.iHeight;
	TBool across = s.iWidth <= v.iWidth;
	switch (aKeyEvent.iCode)
		{
	case EKeyUpArrow:
		if (atTop && iPage > 0)
			GoToPageL(iPage - 1, ETrue);
		else
			PanL(0, -vstep);
		break;
	case EKeyDownArrow:
		if (atFoot && iPage + 1 < iPages)
			GoToPageL(iPage + 1);
		else
			PanL(0, vstep);
		break;
	case EKeyLeftArrow:
		if (across && !ctrl)
			TurnL(-1);
		else
			PanL(-hstep, 0);
		break;
	case EKeyRightArrow:
		if (across && !ctrl)
			TurnL(1);
		else
			PanL(hstep, 0);
		break;
	case EKeyPageUp:
		if (ctrl)
			GoToPageL(0);
		else
			TurnL(-1);
		break;
	case EKeyPageDown:
		if (ctrl)
			GoToPageL(iPages - 1);
		else
			TurnL(1);
		break;
	case EKeyHome:
		if (ctrl)
			GoToPageL(0);
		else
			PanL(0, -0x100000);
		break;
	case EKeyEnd:
		if (ctrl)
			GoToPageL(iPages - 1);
		else
			PanL(0, 0x100000);
		break;
	case ' ':
		if (atFoot)
			TurnL(1);
		else
			PanL(0, v.iHeight - 32);
		break;
	case EKeyBackspace:
		if (atTop)
			{
			if (iPage > 0)
				GoToPageL(iPage - 1, ETrue);
			}
		else
			PanL(0, -(v.iHeight - 32));
		break;
	case EKeyEnter:
		// (0.13) not enough memory before: tried again now
		if (iMemShort)
			{
			MemRetry(ETrue);
			break;
			}
		// a page stopped with Esc, or not all drawn: drawn again
		if (iShown.iPage == iPage && iShown.iDone && iShown.iFailed && !(iJob == PP_CMD_RENDER && iJobPic == &iShown))
			{
			DropPic(iShown);
			iNote.Zero();
			iNoteWhy.Zero();
			Pump();
			}
		else
			return EKeyWasNotConsumed;
		break;
	case '+':
	case '=':
		ZoomStepL(1);
		break;
	case '-':
		ZoomStepL(-1);
		break;
	default:
		return EKeyWasNotConsumed;
		}
	return EKeyWasConsumed;
	}

void CPpAppUi::PanL(TInt aDx, TInt aDy)
	{
	TPoint was = iOrigin;
	iOrigin += TPoint(aDx, aDy);
	ClampOrigin();
	if (iOrigin != was)
		iView->DrawNow();
	}

void CPpAppUi::TapL(const TPoint& aPoint)
	{
	// (0.13) a tap on a page that did not fit in memory tries it again (on
	// Dan's netBook only Next then Back drew it, with 15 MB free)
	if (iMemShort)
		{
		MemRetry(ETrue);
		return;
		}
	if (!iOpen)
		return;
	TRect v = iView->Rect();
	const TInt e = KPpDogEar + 6;            // (a little more than is drawn: finger-sized)
	if (aPoint.iY >= v.iBr.iY - e)
		{
		if (aPoint.iX >= v.iBr.iX - e && iPage + 1 < iPages)
			GoToPageL(iPage + 1);
		else if (aPoint.iX < v.iTl.iX + e && iPage > 0)
			GoToPageL(iPage - 1);
		}
	}

// ----- find ------------------------------------------------------------------

void CPpAppUi::FindL()
	{
	if (NoFile())
		return;
	TBuf<60> t(iFindText);
	CEikDialog* d = new(ELeave) CPpFindDialog(t);
	if (!d->ExecuteLD(R_PP_FIND_DIALOG))
		return;
	iFindText = t;
	iHitPage = -1;
	iHits = 0;
	StartFind(iPage);
	}

void CPpAppUi::FindNextL()
	{
	if (NoFile())
		return;
	if (iFindText.Length() == 0)
		{
		FindL();
		return;
		}
	if (iHitPage == iPage && iHitIndex + 1 < iHits)
		{
		iHitIndex++;
		ShowHit();
		return;
		}
	StartFind(iHitPage == iPage ? iPage + 1 : iPage);
	}

// Find from aFromPage: now, or (while the engine draws a page, which is
// stopped) as soon as it is free
void CPpAppUi::StartFind(TInt aFromPage)
	{
	if (aFromPage >= iPages)
		aFromPage = 0;
	if (!iRunning || !iReady || iJob)
		{
		iFindPending = aFromPage + 1;
		if (iRunning && iJob == PP_CMD_RENDER && !iStopping)
			{
			iS->cookie.abort = 1;
			iStopping = ETrue;
			}
		Busy(_L("Finding\x85"));
		return;
		}
	iFindPending = 0;
	iFindFrom = aFromPage;
	iFindAt = -1;
	iS->page = aFromPage;
	ToUtf8(iS->text, sizeof(iS->text), iFindText);
	Busy(_L("Finding\x85"));
	Send(PP_CMD_FIND);
	}

// the hit iHitIndex in view (the page is shown, or on its way)
void CPpAppUi::ShowHit()
	{
	if (iHitPage != iPage || iHitIndex >= iHits || !SizeKnown(iPage))
		return;
	TReal sc = ScaleFor(iPage, iZoom) / 1000000.0;
	TSize v = iView->Size();
	TInt x = (TInt)(iHit[iHitIndex][0] * sc), y = (TInt)(iHit[iHitIndex][1] * sc);
	if (iShown.iPage == iPage && iShown.iBmp)
		{
		if (y < iOrigin.iY + 8 || y > iOrigin.iY + v.iHeight - 40)
			iOrigin.iY = y - v.iHeight / 3;
		if (x < iOrigin.iX || x > iOrigin.iX + v.iWidth - 40)
			iOrigin.iX = x - v.iWidth / 3;
		ClampOrigin();
		}
	else
		iOrigin = TPoint(x - v.iWidth / 3, y - v.iHeight / 3);   // (clamped when it comes)
	iView->DrawNow();
	TBuf<40> t;
	if (iHits == 1)
		t.Format(_L("Found on page %d"), iPage + 1);
	else
		t.Format(_L("Found on page %d (%d of %d)"), iPage + 1, iHitIndex + 1, iHits);
	iEikonEnv->InfoMsg(t);
	}

// ----- commands --------------------------------------------------------------

TBool CPpAppUi::NoFile()
	{
	if (iOpen)
		return EFalse;
	iEikonEnv->InfoMsg(iOpenPending ? _L("Not available - the file is still opening") : _L("No file open"));
	return ETrue;
	}

void CPpAppUi::GoToL()
	{
	if (NoFile())
		return;
	TInt page = iPage + 1;
	CEikDialog* d = new(ELeave) CPpGoToDialog(page, iPages);
	if (d->ExecuteLD(R_PP_GOTO_DIALOG))
		GoToPageL(page - 1);
	}

// a size as the Coda sheet writes it: "768 bytes", "2 KB", "19.8 MB"
static void FormatSize(TDes& aOut, TInt aBytes)
	{
	if (aBytes < 1024)
		aOut.Format(_L("%d bytes"), aBytes);
	else if (aBytes < 1024 * 1024)
		aOut.Format(_L("%d KB"), (aBytes + 1023) / 1024);
	else
		{
		TInt tenths = ((aBytes / 1024) * 10 + 512) / 1024;   // (in range: a file is under 2 GB)
		if (tenths >= 100)
			aOut.Format(_L("%d MB"), (tenths + 5) / 10);
		else
			aOut.Format(_L("%d.%d MB"), tenths / 10, tenths % 10);
		}
	}

// File > File details: Name, Title, Pages, Size and Folder, each on its
// own line with its prompt (a fixed width: long ones shortened with "...")
void CPpAppUi::DetailsL()
	{
	if (NoFile())
		return;
	TParsePtrC p(iFile);
	TBuf<40> l1, l2, l3, l4, l5;
	Shorten(l1, p.NameAndExt(), 36);
	TBuf<128> t;
	FromUtf8(t, iTitle);
	t.TrimAll();
	if (t.Length())
		Shorten(l2, t, 36);
	else
		l2 = _L("(none)");
	if (iPages == 1)
		l3 = _L("1");
	else
		l3.Num(iPages);
	TEntry e;
	if (iCoeEnv->FsSession().Entry(iFile, e) == KErrNone)
		FormatSize(l4, e.iSize);
	else
		l4 = _L("-");
	TPtrC dir(p.DriveAndPath());
	if (dir.Length() > 1 && dir[dir.Length() - 1] == '\\')
		dir.Set(dir.Left(dir.Length() - 1));
	Shorten(l5, dir, 36);
	CEikDialog* d = new(ELeave) CPpDetailsDialog(l1, l2, l3, l4, l5);
	d->ExecuteLD(R_PP_DETAILS_DIALOG);
	}

// (0.10) while a command runs (a dialog up) PsiPDF is busy for the memory
// watcher: not put to sleep or closed then
void CPpAppUi::HandleCommandL(TInt aCommand)
	{
	if (!iCommands++)
		PsiMemBusy(iMemChunk, KUidPsiPdf.iUid, ETrue);
	CleanupStack::PushL(TCleanupItem(CommandDone, this));
	DoCommandL(aCommand);
	CleanupStack::PopAndDestroy();
	}

void CPpAppUi::CommandDone(TAny* aSelf)
	{
	CPpAppUi* ui = (CPpAppUi*)aSelf;
	if (!--ui->iCommands)
		PsiMemBusy(ui->iMemChunk, KUidPsiPdf.iUid, EFalse);
	}

void CPpAppUi::DoCommandL(TInt aCommand)
	{
	switch (aCommand)
		{
	case EPpCmdOpen:
		{
		// (0.09) the first time (no file yet), the card's Example files
		// folder when it is there: it has PDF files to try
		_LIT(KPpExamples, "D:\\Documents\\Example files");
		TFileName name = iFile.Length() ? iFile : TFileName(_L("C:\\Documents\\"));
		TEntry ex;
		if (!iFile.Length() && !iRecentCount && iCoeEnv->FsSession().Entry(KPpExamples, ex) == KErrNone && ex.IsDir())
			{
			name = KPpExamples;
			name.Append('\\');
			}
		CEikFileOpenDialog* d = new(ELeave) CEikFileOpenDialog(&name, R_PP_OPEN_TITLE);
		_LIT(KPdf, ".pdf");
		d->SetRequiredExtension(&KPdf);
		if (d->ExecuteLD(R_EIK_DIALOG_FILE_OPEN))
			OpenPdfL(name);
		break;
		}
	case EPpCmdDetails: DetailsL(); break;
	case EPpCmdFind: FindL(); break;
	case EPpCmdFindNext: FindNextL(); break;
	case EPpCmdGoTo: GoToL(); break;
	case EPpCmdNext: TurnL(1); break;
	case EPpCmdPrev: TurnL(-1); break;
	case EPpCmdFirst: if (!NoFile()) GoToPageL(0); break;
	case EPpCmdLast: if (!NoFile()) GoToPageL(iPages - 1); break;
	case EEikCmdZoomIn:                      // (the Zoom icons beside the screen)
	case EPpCmdZoomIn: ZoomStepL(1); break;
	case EEikCmdZoomOut:
	case EPpCmdZoomOut: ZoomStepL(-1); break;
	case EPpCmdFitWidth: SetZoomL(EZoomFitWidth); break;
	case EPpCmdFitPage: SetZoomL(EZoomFitPage); break;
	case EPpCmdZoom100: SetZoomL(EZoom100); break;
	case EPpCmdZoom150: SetZoomL(EZoom150); break;
	case EPpCmdZoom200: SetZoomL(EZoom200); break;
	case EPpCmdToolbar: ShowToolBarL(!iToolbarShown); break;
	case EPpCmdKeepLog:
		// (what is in PsiPDF.log already stays)
		iKeepLog = !iKeepLog;
		iLogOn = ETrue;
		if (iKeepLog)
			Log(_L("log: kept from here (Tools > Troubleshooting > Keep a log)"));
		iLogOn = iKeepLog || iDiagOn;
		SaveSettings();             // (the tick says so: no infoprint)
		break;
	case EPpCmdHelp:
		PsiHelpL(KPpHelp, sizeof(KPpHelp) / sizeof(KPpHelp[0]), _L("Help on PsiPDF"), R_PSI_HELP_DIALOG, R_PSI_HELP_DIALOG_TALL);
		break;
	case EPpCmdAbout:
		(new(ELeave) CPpAboutDialog)->ExecuteLD(R_PP_ABOUT_DIALOG);
		break;
	case EEikCmdExit:
		Remember();
		SaveSettings();
		StopEngine();
		Exit();
		break;
	default:
		break;
		}
	}

void CPpAppUi::DynInitMenuPaneL(TInt aMenuId, CEikMenuPane* aMenuPane)
	{
	TBool open = iOpen;
	if (aMenuId == R_PP_FILE_MENU)
		aMenuPane->SetItemDimmed(EPpCmdDetails, !open);
	else if (aMenuId == R_PP_EDIT_MENU)
		{
		aMenuPane->SetItemDimmed(EPpCmdFind, !open);
		aMenuPane->SetItemDimmed(EPpCmdFindNext, !open);
		aMenuPane->SetItemDimmed(EPpCmdGoTo, !open);
		}
	else if (aMenuId == R_PP_VIEW_MENU)
		{
		aMenuPane->SetItemDimmed(EPpCmdNext, !open || iPage + 1 >= iPages);
		aMenuPane->SetItemDimmed(EPpCmdPrev, !open || iPage <= 0);
		aMenuPane->SetItemDimmed(EPpCmdFirst, !open || iPage <= 0);
		aMenuPane->SetItemDimmed(EPpCmdLast, !open || iPage + 1 >= iPages);
		aMenuPane->SetItemDimmed(EPpCmdZoomIn, !open);
		aMenuPane->SetItemDimmed(EPpCmdZoomOut, !open);
		aMenuPane->SetItemButtonState(EPpCmdToolbar, iToolbarShown ? EEikMenuItemSymbolOn : EEikMenuItemSymbolIndeterminate);
		}
	else if (aMenuId == R_PP_TROUBLE_MENU)
		aMenuPane->SetItemButtonState(EPpCmdKeepLog, iKeepLog ? EEikMenuItemSymbolOn : EEikMenuItemSymbolIndeterminate);
	else if (aMenuId == R_PP_ZOOM_MENU)
		{
		static const TInt cmds[EZoomCount] = { EPpCmdFitWidth, EPpCmdFitPage, EPpCmdZoom100, EPpCmdZoom150, EPpCmdZoom200 };
		aMenuPane->SetItemButtonState(cmds[iZoom], EEikMenuItemSymbolOn);
		}
	}

// ----- messages --------------------------------------------------------------

void CPpAppUi::Busy(const TDesC& aText)
	{
	BusyEnd();
	if (!iBusyWin)
		{
		CPpBusyWin* w = new CPpBusyWin(*iEikonEnv);
		TInt err = KErrNoMemory;
		if (w)
			TRAP(err, w->ConstructL(iCoeEnv->RootWin()));
		if (err == KErrNone)
			iBusyWin = w;
		else
			delete w;
		}
	if (iBusyWin)
		{
		iBusyWin->Show(aText.Left(Min(aText.Length(), KEikBusyMsgMaxLen)), TTimeIntervalMicroSeconds32(300000));
		iBusy = ETrue;
		return;
		}
	// (short of memory for the window: EIKON's own, which flashes)
	TRAPD(err, iEikonEnv->BusyMsgL(aText, EHLeftVBottom, TTimeIntervalMicroSeconds32(300000)));
	iBusy = err == KErrNone;
	}

void CPpAppUi::BusyEnd()
	{
	if (iBusy)
		{
		if (iBusyWin)
			iBusyWin->CancelDisplay();
		else
			iEikonEnv->BusyMsgCancel();
		}
	iBusy = EFalse;
	}

void CPpAppUi::Problem(const TDesC& aWhat, TInt aResult)
	{
	TPtrC why;
	switch (aResult)
		{
	case PP_E_OPEN: why.Set(_L("File not found, or it is open in another program")); break;
	case PP_E_DAMAGED: why.Set(_L("It is not a PDF file, or it is damaged")); break;
	case PP_E_PASSWORD: why.Set(_L("It needs a password")); break;
	case PP_E_MEMORY: why.Set(_L("Not enough memory - close other programs")); break;
	default: why.Set(_L("Part of it uses a feature PsiPDF does not have")); break;
		}
	if (aResult == PP_E_MEMORY)
		TellShort();
	TRAPD(err, iEikonEnv->InfoWinL(aWhat, why));
	}

// A page drawn only in part: why, as far as MuPDF's last warning says; a
// dialog the first time in a file, then an infoprint (one for every page
// stopped the reading)
void CPpAppUi::PartDrawn(TInt aResult)
	{
	TPtrC what(aResult == PP_E_MEMORY ? _L("Not enough memory to show all of this page") : _L("Cannot show all of this page"));
	if (aResult == PP_E_MEMORY)
		{
		// (0.13) drawn again, all of it, when memory comes back; the
		// dialog offers Coda's Free memory (MemoryShort)
		TBool told = iPartTold;
		iPartTold = ETrue;
		if (told || iMemAuto)
			{
			iMemAuto = EFalse;
			iMemShort = EPpMemPage;
			iMemShortKb = FreeRamKb();
			iMemShortLevel = PsiMemLevelNow() >= 0 ? PsiMemLevelOf(iMemShortKb * 1024) : -1;
			if (!iMemLook->IsActive())
				iMemLook->Start(KPpMemLookUs, KPpMemLookUs, TCallBack(MemLookCallback, this));
			if (told)
				iEikonEnv->InfoMsg(what);
			return;
			}
		MemoryShort(EPpMemPage, what);
		return;
		}
	if (iPartTold)
		{
		iEikonEnv->InfoMsg(what);
		return;
		}
	iPartTold = ETrue;
	TPtrC why;
	if (aResult == PP_E_MEMORY)
		why.Set(_L("Close other programs, then press Enter"));
	else
		{
		TBuf<160> m;
		TPtrC8 mu((const TUint8*)iS->mu_msg);
		m.Copy(mu.Left(Min(mu.Length(), m.MaxLength())));
		m.LowerCase();
		if (m.Find(_L("jbig2")) >= 0 || m.Find(_L("jpx")) >= 0)
			why.Set(_L("A picture in it is in a form PsiPDF cannot read"));
		else if (m.Find(_L("font")) >= 0)
			why.Set(_L("A font in it cannot be read"));
		else if (m.Find(_L("image")) >= 0)
			why.Set(_L("A picture in it cannot be read"));
		else if (m.Find(_L("syntax")) >= 0 || m.Find(_L("xref")) >= 0 || m.Find(_L("object")) >= 0 || m.Find(_L("repair")) >= 0)
			why.Set(_L("Part of the file is damaged"));
		else
			why.Set(_L("Part of it uses a feature PsiPDF does not have"));
		}
	TRAPD(err, iEikonEnv->InfoWinL(what, why));
	}

// ----- settings: the toolbar, and the last files with their page and zoom ----
// PsiPDF.ini: "PsiPDF 1", the toolbar (0 or 1), then one line a file:
// page TAB zoom TAB name, the latest first

void CPpAppUi::LoadSettings()
	{
	iToolbarShown = ETrue;
	iRecentCount = 0;
	RFile f;
	if (f.Open(iCoeEnv->FsSession(), KPpIni, EFileRead) != KErrNone)
		return;
	TBuf8<4096> b;
	f.Read(b);
	f.Close();
	TInt line = 0;
	while (b.Length())
		{
		TInt end = b.Locate('\n');
		TPtrC8 ln = end < 0 ? TPtrC8(b) : b.Left(end);
		if (ln.Length() && ln[ln.Length() - 1] == '\r')
			ln.Set(ln.Left(ln.Length() - 1));
		if (line == 0 && ln.Compare(_L8("PsiPDF 1")) != 0)
			return;
		if (line == 1)
			{
			iToolbarShown = ln.Length() == 0 || ln[0] != '0';
			iKeepLog = ln.Length() >= 3 && ln[1] == ' ' && ln[2] == '1';   // (0.06: "1 1", toolbar and log)
			}
		if (line >= 2 && iRecentCount < KPpRecent)
			{
			TLex8 lex(ln);
			TInt page = 0, zoom = 0;
			if (lex.Val(page) == KErrNone && lex.Get() == '\t' && lex.Val(zoom) == KErrNone && lex.Get() == '\t')
				{
				TPpRecentFile& r = iRecent[iRecentCount];
				TPtrC8 name = lex.Remainder();
				r.iFile.Copy(name.Left(Min(name.Length(), r.iFile.MaxLength())));
				r.iPage = Max(0, page);
				r.iZoom = zoom >= 0 && zoom < EZoomCount ? zoom : EZoomFitWidth;
				iRecentCount++;
				}
			}
		line++;
		if (end < 0)
			break;
		b.Delete(0, end + 1);
		}
	}

// (0.06) written whole to PsiPDF.new, then put in PsiPDF.ini's place: a
// full disk or a write that failed leaves the old settings as they were
// (PsiPDF.ini was cut to nothing first, and the writes not checked)
void CPpAppUi::SaveSettings()
	{
	RFs& fs = iCoeEnv->FsSession();
	fs.MkDirAll(KPpIni);
	RFile f;
	if (f.Replace(fs, KPpIniNew, EFileWrite) != KErrNone)
		return;
	TBuf8<300> b;
	b.Format(_L8("PsiPDF 1\r\n%d %d\r\n"), iToolbarShown ? 1 : 0, iKeepLog ? 1 : 0);
	TInt r = f.Write(b);
	for (TInt i = 0; i < iRecentCount && r == KErrNone; i++)
		{
		b.Format(_L8("%d\t%d\t"), iRecent[i].iPage, iRecent[i].iZoom);
		b.Append(iRecent[i].iFile.Left(Min(iRecent[i].iFile.Length(), b.MaxLength() - b.Length() - 2)));
		b.Append(_L8("\r\n"));
		r = f.Write(b);
		}
	if (r == KErrNone)
		r = f.Flush();
	f.Close();
	if (r == KErrNone)
		r = fs.Replace(KPpIniNew, KPpIni);
	if (r != KErrNone)
		{
		fs.Delete(KPpIniNew);
		TBuf<60> l;
		l.Format(_L("settings: not saved (%d)"), r);
		Log(l);
		}
	}

// PsiPDF.try: the file being opened, until its first page is shown
void CPpAppUi::MarkTrying(TBool aOn)
	{
	RFs& fs = iCoeEnv->FsSession();
	if (!aOn)
		{
		if (iTryingMarked)
			fs.Delete(KPpTrying);
		iTryingMarked = EFalse;
		return;
		}
	fs.MkDirAll(KPpTrying);
	RFile f;
	if (f.Replace(fs, KPpTrying, EFileWrite) != KErrNone)
		return;
	TBuf8<256> b;
	b.Copy(iFile.Left(Min(iFile.Length(), b.MaxLength())));
	iTryingMarked = f.Write(b) == KErrNone;
	f.Close();
	}

TBool CPpAppUi::WasTrying(const TDesC& aFile)
	{
	RFile f;
	if (f.Open(iCoeEnv->FsSession(), KPpTrying, EFileRead) != KErrNone)
		return EFalse;
	TBuf8<256> b;
	TInt r = f.Read(b);
	f.Close();
	if (r != KErrNone)
		return EFalse;
	TBuf<256> name;
	name.Copy(b);
	return name.CompareF(aFile.Left(Min(aFile.Length(), name.MaxLength()))) == 0;
	}

// The recogniser on C: (installed there, wherever the program is) is gone
// when C: has been cleared, and a tap on a PDF on the System screen does
// nothing: put back from the copy beside the program. The System screen
// reads it at its next start.
void CPpAppUi::RestoreRecogniser()
	{
	RFs& fs = iCoeEnv->FsSession();
	if (BaflUtils::FileExists(fs, KPpRecogZ) || BaflUtils::FileExists(fs, KPpRecogC))
		return;
	TParse p;
	TFileName app = Application()->AppFullName();
	if (p.Set(_L("PsiPDF.mdl"), &app, NULL) != KErrNone || !BaflUtils::FileExists(fs, p.FullName()))
		return;
	fs.MkDirAll(KPpRecogC);
	CFileMan* fm = NULL;
	TRAPD(r, fm = CFileMan::NewL(fs));
	if (r == KErrNone)
		r = fm->Copy(p.FullName(), KPpRecogC);
	delete fm;
	TBuf<80> l;
	l.Format(_L("recogniser: put back on C: (%d), read at the next start"), r);
	Log(l);
	}

// the open file at the top of the list, with its page and zoom
void CPpAppUi::Remember()
	{
	if (!iOpen || !iFile.Length())
		return;
	TInt i, at = iRecentCount;
	for (i = 0; i < iRecentCount; i++)
		if (iRecent[i].iFile.CompareF(iFile) == 0)
			at = i;
	if (at == iRecentCount)
		{
		if (iRecentCount < KPpRecent)
			iRecentCount++;
		at = iRecentCount - 1;
		}
	for (i = at; i > 0; i--)
		iRecent[i] = iRecent[i - 1];
	iRecent[0].iFile = iFile;
	iRecent[0].iPage = iPage;
	iRecent[0].iZoom = iZoom;
	}

void CPpAppUi::HandleForegroundEventL(TBool aForeground)
	{
	iForeground = aForeground;
	EnginePriority();
	iIdleStopper->Cancel();
	if (!aForeground)
		{
		Remember();
		SaveSettings();
		// no file: the engine (1.5 MB) is not kept waiting behind
		if (iRunning && !iOpen && !iOpenPending)
			{
			StopEngine();
			Log(_L("engine: stopped (no file, PsiPDF behind)"));
			}
		// a file: stopped if PsiPDF stays behind (IdleStop)
		else if (iRunning && iOpen)
			iIdleStopper->Start(KPpIdleStop, KPpIdleStop, TCallBack(IdleStopCallback, this));
		}
	else if (iMemShort && !iAsleep && !iMemDialogUp)
		MemRetry(EFalse);                    // (0.13) back in front: the page tried again
	else if (iAsleep)
		{
		// back: the file opened again where it was
		iAsleep = EFalse;
		iMemShort = EPpMemNone;              // (0.13) (the page is drawn afresh anyway)
		iMemLook->Cancel();
		iReopen = ETrue;
		TBuf<60> l;
		l.Format(_L("engine: back in front, free RAM %d KB"), FreeRamKb());
		Log(l);
		Pump();
		}
	}

TInt CPpAppUi::IdleStopCallback(TAny* aSelf)
	{
	CPpAppUi* ui = (CPpAppUi*)aSelf;
	ui->iIdleStopper->Cancel();
	ui->IdleStop();
	return 0;
	}

// PsiPDF behind for KPpIdleStop with a file open: the engine stopped (its
// code, MuPDF's heap and the page drawn ahead given back). (0.10) The page's
// bitmap goes too: it was kept, 0.5 MB fitted to the screen and up to 6 MB at
// 200%, for as long as PsiPDF stayed behind. Its page, zoom, place and Find's
// hits stay; coming back opens the file again (HandleForegroundEventL, then
// PumpL) and draws the page where it was.
void CPpAppUi::IdleStop(TBool aAsked)
	{
	if (iForeground || !iRunning || !iOpen || iOpenPending || iReopening)
		return;
	TInt before = FreeRamKb();
	StopEngine();
	iJobPic = NULL;
	iFindPending = 0;
	BusyEnd();
	DropPic(iAhead);
	if (iShown.iBmp && iShown.iPage == iPage)
		iKeepOrigin = ETrue;
	DropPic(iShown);
	iView->DrawNow();
	iAsleep = ETrue;
	iReopen = iReopening = EFalse;
	iIdleStopper->Cancel();
	TBuf<100> l;
	if (aAsked)
		l.Format(_L("engine: stopped behind (memory is short), free RAM %d -> %d KB"), before, FreeRamKb());
	else
		l.Format(_L("engine: stopped after %d min behind, free RAM %d -> %d KB"), KPpIdleStop / 60000000, before, FreeRamKb());
	Log(l);
	}

// Short of memory (core/ssh/psimem.h): the program that watches the free
// memory asks. "trim": what can be made again is given back. "sleep": behind
// another program with a file open, the engine is stopped now, as after
// KPpIdleStop behind (never in front: there it is a trim).
void CPpAppUi::ProcessMessageL(TUid aUid, const TDesC8& aParams)
	{
	if (aUid.iUid != KPsiMemMsgUid)
		{
		CEikAppUi::ProcessMessageL(aUid, aParams);
		return;
		}
	// (0.07) "sleepnow": PsiHub's Free memory dialog, "Put PsiPDF to sleep"
	// ticked (it offers it only behind another program); answered, with
	// why not in words if not
	if (aParams == KPsiMemSleepNow)
		{
		TBuf<48> why;
		if (iAsleep && !iRunning)
			;                                // (asleep already)
		else if (iForeground)
			why.Copy(_L("it is in use"));
		else if (!iRunning)
			why.Copy(_L("its engine has stopped"));
		else if (!iOpen)
			why.Copy(_L("no file is open"));
		else if (iOpenPending || iReopening)
			why.Copy(_L("a file is still opening"));
		else
			IdleStop(ETrue);
		if (!why.Length() && !iAsleep)
			why.Copy(_L("its engine did not stop"));
		if (why.Length())
			Trim();
		PsiMemAnswer(iCoeEnv->WsSession(), KUidPsiPdf.iUid, why.Length() ? KPsiMemCannot : KPsiMemSlept, why);
		return;
		}
	if (aParams == KPsiMemSleep && !iForeground && iRunning && iOpen && !iOpenPending && !iReopening)
		IdleStop(ETrue);
	else if (aParams == KPsiMemSleep || aParams == KPsiMemTrim)
		Trim();
	}

// The page drawn ahead goes (a page turn draws it again); the engine drops
// the decoded pictures and glyphs it keeps and the other pages' display
// lists, and gives back its heap's free end; so does the program.
void CPpAppUi::Trim()
	{
	iTrimFree = FreeRamKb();
	if (iRunning && iJob == PP_CMD_RENDER && iJobPic == &iAhead && !AheadIsWanted())
		{
		if (!iStopping)
			{
			iS->cookie.abort = 1;            // (its half page is dropped as it ends)
			iStopping = ETrue;
			}
		}
	else if (iAhead.iBmp && iJobPic != &iAhead && !AheadIsWanted())
		DropPic(iAhead);
	if (iRunning && iReady && !iJob && iOpen && !iOpenPending && !iReopen && !iReopening)
		Send(PP_CMD_TRIM);
	else
		{
		TBuf<60> l;
		l.Format(_L("trim: free RAM %d -> %d KB"), iTrimFree, FreeRamKb());
		Log(l);
		}
	User::Heap().Compress();
	}

// PsiPDF has had to say "Not enough memory": the program that watches the
// free memory (if there is one) may offer its Free memory dialog
void CPpAppUi::TellShort()
	{
	PsiMemTell(iCoeEnv->WsSession(), KPsiMemShort);
	}

void CPpAppUi::Log(const TDesC& aText)
	{
	// (0.06) only with Tools > Keep a log ticked, or Coda's debug mode on:
	// it was written on every page of every session
	if (!iLogOn)
		return;
	RFs& fs = iCoeEnv->FsSession();
	RFile f;
	if (f.Open(fs, KPpLog, EFileWrite | EFileShareAny) != KErrNone)
		{
		fs.MkDirAll(KPpLog);
		if (f.Create(fs, KPpLog, EFileWrite | EFileShareAny) != KErrNone)
			return;
		}
	TInt size = 0;
	f.Size(size);
	if (size < 32768)
		{
		TBuf8<320> b;
		TTime now;
		now.HomeTime();
		TDateTime t = now.DateTime();
		b.Format(_L8("%02d:%02d:%02d.%02d "), t.Hour(), t.Minute(), t.Second(), t.MicroSecond() / 10000);
		b.Append(aText.Left(Min(aText.Length(), b.MaxLength() - b.Length() - 2)));
		b.Append(_L8("\r\n"));
		TInt end = 0;
		f.Seek(ESeekEnd, end);
		f.Write(b);
		}
	f.Close();
	}

// ===========================================================================

EXPORT_C CApaApplication* NewApplication()
	{
	return new CPpApplication;
	}

GLDEF_C TInt E32Dll(TDllReason)
	{
	return KErrNone;
	}
