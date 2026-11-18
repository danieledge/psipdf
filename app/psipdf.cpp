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
#include <eikmenu.hrh>
#include <eikdialg.h>
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

const TUid KUidPsiPdf = { 0x01000AD8 };
_LIT(KPpVersion, "0.05");
_LIT(KPpIni, "C:\\System\\Apps\\PsiPDF\\PsiPDF.ini");
_LIT(KPpLog, "C:\\System\\Apps\\PsiPDF\\PsiPDF.log");
_LIT(KPpLogOld, "C:\\System\\Apps\\PsiPDF\\PsiPDF.old");
_LIT(KPpEngine, "psipdf.exe");

// the zooms, in the order of View > Zoom
enum { EZoomFitWidth = 0, EZoomFitPage, EZoom100, EZoom150, EZoom200, EZoomCount };
const TInt KPpRecent = 8;                    // files whose page and zoom are remembered
// Memory (the netBook has about 16 MB free after it starts, shared by every
// program; the 5mx, 8 or 16 MB). What PsiPDF may take is set when a file
// opens, from the free RAM then:
// - the biggest page bitmap: a quarter of it, from 1 MB (a page fitted to
//   the width: 580 KB) up to 6 MB (200% on an A4 page is 4 MB: with less
//   memory it is drawn a little smaller);
// - MuPDF's store (decoded pictures, fonts): a quarter of it, from 1 MB up
//   to 3 MB (2 MB on the 5mx); a picture is decoded no bigger than this.
const TInt KPpMaxBytes = 6 * 1024 * 1024;
const TInt KPpMinBytes = 1024 * 1024;
const TInt KPpStoreMinKb = 1024;
// the next page is drawn ahead only while it is small (fit width or fit
// page: up to 1 MB) and this much free memory is left after it
const TInt KPpAheadMaxBytes = 1024 * 1024;
const TInt KPpAheadSpare = 3 * 1024 * 1024;
// PsiPDF behind this long with a file open: its engine (MuPDF, 3 to 8 MB with
// its code) is stopped, and the file opened again, at the same page and
// zoom, when PsiPDF comes back; the page on the screen is kept meanwhile
const TInt KPpIdleStop = 5 * 60 * 1000000;
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
	TInt iMs;                    // how long it took
	};

struct TPpRecentFile
	{
	TFileName iFile;
	TInt iPage;
	TInt iZoom;
	};

class CPpAppUi;

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
	void ButtonPictureL(TInt aId, TInt aIcon);
	void Busy(const TDesC& aText);
	void BusyEnd();
	void Problem(const TDesC& aWhat, TInt aResult);
	TBool NoFile();
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
	void Watchdog();
	void EnginePriority();
	TInt FreeRamKb() const;
	static TInt IdleStopCallback(TAny* aSelf);
	void IdleStop(TBool aAsked = EFalse);
private:
	CPpView* iView;
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
	TInt iCentreX, iCentreY;     // ...as a part of the page (x 10000)
	TPpPic iShown;               // the page on the screen
	TPpPic iAhead;               // the next page, drawn ahead
	TBool iToolbarShown;
	TBool iBusy;
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

class CPpGoToDialog : public CEikDialog
	{
public:
	CPpGoToDialog(TInt& aPage, TInt aPages) : iPage(aPage), iPages(aPages) {}
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

class CPpFindDialog : public CEikDialog
	{
public:
	CPpFindDialog(TDes& aText) : iText(aText) {}
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

class CPpPasswordDialog : public CEikDialog
	{
public:
	CPpPasswordDialog(const TDesC& aFile, TDes& aPassword, TBool aAgain) : iFile(aFile), iPassword(aPassword), iAgain(aAgain) {}
private:
	void PreLayoutDynInitL()
		{
		TParsePtrC p(iFile);
		TBuf<32> name;
		Shorten(name, p.NameAndExt(), 28);
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

class CPpAboutDialog : public CEikDialog
	{
private:
	void PreLayoutDynInitL()
		{
		TBuf<20> name;
		name.Format(_L("PsiPDF %S"), &KPpVersion);
		CEikLabel* l = (CEikLabel*)Control(EPpDlgAboutName);
		l->SetTextL(name);
		l->SetFont(iEikonEnv->TitleFont());
		TBuf<80> t(_L("PDF files on the Psion netBook & Series 5mx\n"));
		t.Append(TChar(0xa9));               // (c), as the Psion's fonts have it
		t.Append(_L(" 2026 Dan Edge"));
		((CEikLabel*)Control(EPpDlgAboutText))->SetTextL(t);
		TBuf<60> m(_L("Uses MuPDF 1.3, "));
		m.Append(TChar(0xa9));
		m.Append(_L(" Artifex Software - AGPL-3.0"));
		((CEikLabel*)Control(EPpDlgAboutMuPdf))->SetTextL(m);
		}
	};

class CPpDetailsDialog : public CEikDialog
	{
public:
	CPpDetailsDialog(const TDesC& a1, const TDesC& a2, const TDesC& a3, const TDesC& a4) : i1(a1), i2(a2), i3(a3), i4(a4) {}
private:
	void PreLayoutDynInitL()
		{
		((CEikLabel*)Control(EPpDlgDetails1))->SetTextL(i1);
		((CEikLabel*)Control(EPpDlgDetails2))->SetTextL(i2);
		((CEikLabel*)Control(EPpDlgDetails3))->SetTextL(i3);
		((CEikLabel*)Control(EPpDlgDetails4))->SetTextL(i4);
		}
	const TDesC& i1;
	const TDesC& i2;
	const TDesC& i3;
	const TDesC& i4;
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
	StopEngine();
	delete iIdle;
	delete iSoon;
	delete iIdleStopper;
	delete iTick;
	delete iWaiter;
	delete iWatcher;
	iRinger.Close();
	if (iChunkOpen)
		iChunk.Close();
	DropPic(iShown);
	DropPic(iAhead);
	delete[] iSizes;
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
	iMaxBytes = KPpMaxBytes;
	iWaiter = new(ELeave) CPsiBellWaiter(TCallBack(BellCallback, this));
	iWatcher = new(ELeave) CPpWatcher(*this);
	// the last session's log kept as PsiPDF.old
	iCoeEnv->FsSession().Delete(KPpLogOld);
	iCoeEnv->FsSession().Rename(KPpLog, KPpLogOld);
	LoadSettings();
	TRAPD(err, ButtonPictureL(EPpCmdPrev, EMbmPpPrev);
		ButtonPictureL(EPpCmdNext, EMbmPpNext);
		ButtonPictureL(EPpCmdZoomIn, EMbmPpZoomIn);
		ButtonPictureL(EPpCmdZoomOut, EMbmPpZoomOut));
	(void)err;                           // (words alone, if the pictures are missing)
	iEikonEnv->Process()->SetMainDocFileName(_L("PsiPDF"));
	if (iToolBar)
		{
		CEikFileNameLabel* label = (CEikFileNameLabel*)iToolBar->ControlById(EPpCmdFileName);
		if (label)
			TRAP(err, label->UpdateL());
		if (!iToolbarShown)
			iToolBar->MakeVisible(EFalse);
		}
	iView = new(ELeave) CPpView(*this);
	iView->ConstructL(ViewRect(iToolbarShown));
	AddToStackL(iView);
	TBuf<80> l;
	l.Format(_L("PsiPDF %S, screen %d bits, %d pixels an inch"), &KPpVersion, iMode == EColor256 ? 8 : 4, iPxPerInch);
	Log(l);
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
		OpenPdfL(iRecent[0].iFile);
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
	CEikCommandButton* b = iToolBar ? (CEikCommandButton*)iToolBar->ControlById(aId) : NULL;
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

// ClientRect() keeps the toolbar's width back even when it is hidden
TRect CPpAppUi::ViewRect(TBool aToolbar) const
	{
	TRect r = ClientRect();
	if (!aToolbar || !iToolBar)
		r.iBr.iX = iEikonEnv->ScreenDevice()->SizeInPixels().iWidth;
	return r;
	}

void CPpAppUi::ShowToolBarL(TBool aShow)
	{
	if (!iToolBar)
		return;
	iToolbarShown = aShow;
	iToolBar->MakeVisible(aShow);
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
	iRinger.Ring(&iS->eng_bell);
	for (TInt i = 0; i < 20 && iS->state != PP_STATE_EXITED; i++)
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
			why.Format(_L("MuPDF stopped (%S %d) - the file may be damaged"), &cat, reason);
		else
			why = _L("MuPDF stopped - there may not be enough memory");
		CannotShow(_L("Could not show this page"), why);
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
	CannotShow(_L("Could not show this page"), late ?
		(lowRam ? _L("PsiPDF's engine did not start - close other programs") : _L("PsiPDF's engine did not start - another program may be busy")) :
		(lowRam ? _L("PsiPDF's engine stopped answering - close other programs") : _L("PsiPDF's engine stopped answering - the page may be too complex")));
	}

void CPpAppUi::Send(TInt aCmd)
	{
	iS->cookie.abort = 0;
	iS->cookie.progress = 0;
	iS->cmd = aCmd;
	iJob = aCmd;
	iStopping = EFalse;
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
			t.Format(_L("Finding... page %d"), iFindAt + 1);
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
				CannotShow(_L("Not enough memory to show this page"), _L("Close other programs, then open the file again"));
			else
				Problem(_L("Cannot open this file"), result);
			iFile.Zero();
			iEikonEnv->Process()->SetMainDocFileName(_L("PsiPDF"));
			UpdateLabelL();
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
			DropPic(*pic);               // (half a page: not kept)
			break;
			}
		pic->iDone = ETrue;
		pic->iFailed = result != PP_OK;
		if (pic == &iShown)
			{
			BusyEnd();
			if (pic->iFailed && pic->iRows == 0)
				{
				// nothing of it drawn: said on the page too
				if (result == PP_E_MEMORY)
					CannotShow(_L("Not enough memory to show this page"), _L("Close other programs, then try again"));
				else
					CannotShow(_L("Could not show this page"), _L("Part of it uses a feature PsiPDF does not have"));
				}
			else if (pic->iFailed)
				Problem(result == PP_E_MEMORY ? _L("Not enough memory to show all of this page") : _L("Cannot draw all of this page"), result);
			iView->DrawNow();
			// the 14 standard fonts' file could not be used (the engine drew
			// their text in the one font it keeps): said once
			if (s->font_trouble && !iFontTold)
				{
				iFontTold = ETrue;
				TBool missing = s->font_trouble == PP_FONT_MISSING;
				Log(missing ? _L("fonts: PsiPDF.fnt not found") : _L("fonts: PsiPDF.fnt damaged"));
				TPtrC what(missing ? _L("PsiPDF's font file not found") : _L("PsiPDF's font file is damaged"));
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
				why = _L("psipdf.exe is missing - install PsiPDF again");
			else if (err == KErrNoMemory)
				{
				CannotShow(_L("Not enough memory to show this page"), _L("Close other programs, then open the file again"));
				return;
				}
			else
				why.Format(_L("psipdf.exe did not start (%d) - install PsiPDF again"), err);
			CannotShow(_L("Could not show this page"), why);
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
			Busy(_L("Drawing page..."));
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
		iStoreKb = Max(KPpStoreMinKb, Min(iMode == EColor256 ? 3072 : 2048, free / 4));
		iMaxBytes = Max(KPpMinBytes, Min(KPpMaxBytes, free / 4 * 1024));
		s->store_kb = iStoreKb;
		TBuf<100> l;
		l.Format(_L("memory: free RAM %d KB: store %d KB, pages up to %d KB"), free, iStoreKb, iMaxBytes / 1024);
		Log(l);
		Busy(_L("Opening..."));
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
				if (iZoom == EZoomFitWidth)
					CannotShow(_L("Not enough memory to show this page"), _L("Close other programs, then try again"));
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
			else if (!samePage)
				iOrigin.iY = iAtFoot ? now.iHeight : 0;
			iKeepCentre = EFalse;
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
			Busy(_L("Drawing page..."));
			Send(PP_CMD_RENDER);
			UpdateLabelL();
			iView->DrawNow();
			return;
			}
		}
	if (!iShown.iDone)
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
	Remember();
	SaveSettings();
	CloseDocument();
	iFile = p.FullName();
	iPassword.Zero();
	iEngineFailed = EFalse;
	iNote.Zero();
	iNoteWhy.Zero();
	iOpenPending = ETrue;
	Busy(_L("Opening..."));             // (at once: the engine may take a moment to start)
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
	iPages = iS->pages;
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
	TInt w = (TInt)(iSizes[2 * aPage] / 100.0 * s + 0.999);
	TInt h = (TInt)(iSizes[2 * aPage + 1] / 100.0 * s + 0.999);
	return TSize(Max(1, w), Max(1, h));
	}

TInt CPpAppUi::CreatePic(TPpPic& aPic, TInt aPage, TInt aScale)
	{
	DropPic(aPic);
	CFbsBitmap* b = new CFbsBitmap;
	if (!b)
		return KErrNoMemory;
	TInt r = b->Create(PixelsFor(aPage, aScale), iMode);
	if (r != KErrNone)
		{
		delete b;
		return r;
		}
	aPic.iBmp = b;
	aPic.iPage = aPage;
	aPic.iScale = aScale;
	aPic.iRows = 0;
	aPic.iDone = aPic.iFailed = EFalse;
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
	aPic.iDone = aPic.iFailed = EFalse;
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
	if (!iToolbarShown)
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
	// (zooms of the same size are one step)
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
	if (!iToolBar)
		return;
	CEikFileNameLabel* fl = (CEikFileNameLabel*)iToolBar->ControlById(EPpCmdFileName);
	if (fl)
		fl->UpdateL();
	CEikLabel* l = (CEikLabel*)iToolBar->ControlById(EPpCmdPageLabel);
	if (!l)
		return;
	TBuf<24> t;
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
		aGc.SetBrushColor(back);
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
			line2.Set(_L("Ctrl+O opens a PDF file"));
			}
		if (line1.Length())
			{
			const CFont* f = iEikonEnv->NormalFont();
			aGc.UseFont(f);
			aGc.SetPenStyle(CGraphicsContext::ESolidPen);
			aGc.SetPenColor(KRgbBlack);
			aGc.SetBrushStyle(CGraphicsContext::ENullBrush);
			TInt base = v.iTl.iY + v.Height() / 2;
			aGc.DrawText(line1, TRect(v.iTl.iX, base - 20, v.iBr.iX, base), f->AscentInPixels() + 2, CGraphicsContext::ECenter);
			aGc.DrawText(line2, TRect(v.iTl.iX, base, v.iBr.iX, base + 20), f->AscentInPixels() + 2, CGraphicsContext::ECenter);
			aGc.DiscardFont();
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
		aGc.SetBrushColor(KRgbWhite);
		aGc.DrawRect(TRect(vis.iTl.iX, Max(vis.iTl.iY, drawnFoot), vis.iBr.iX, vis.iBr.iY));
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
		// stops a find, or the page being drawn
		if (iRunning && (iJob == PP_CMD_FIND || (iJob == PP_CMD_RENDER && iJobPic == &iShown)))
			iS->cookie.abort = 1;
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
		Busy(_L("Finding..."));
		return;
		}
	iFindPending = 0;
	iFindFrom = aFromPage;
	iFindAt = -1;
	iS->page = aFromPage;
	ToUtf8(iS->text, sizeof(iS->text), iFindText);
	Busy(_L("Finding..."));
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
	iEikonEnv->InfoMsg(iOpenPending ? _L("Not available while opening") : _L("No file open"));
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

void CPpAppUi::DetailsL()
	{
	if (NoFile())
		return;
	TParsePtrC p(iFile);
	TBuf<40> l1, title, l2, l3, l4;
	Shorten(l1, p.NameAndExt(), 36);
	TBuf<128> t;
	FromUtf8(t, iTitle);
	if (t.Length())
		{
		Shorten(title, t, 30);
		l2.Format(_L("Title: %S"), &title);
		}
	else
		l2 = _L("No title");
	TEntry e;
	TInt kb = iCoeEnv->FsSession().Entry(iFile, e) == KErrNone ? (e.iSize + 1023) / 1024 : 0;
	if (iPages == 1)
		l3.Format(_L("1 page, %d KB"), kb);
	else
		l3.Format(_L("%d pages, %d KB"), iPages, kb);
	if (iShown.iDone && !iShown.iFailed && iShown.iPage == iPage)
		l4.Format(_L("Page %d drawn in %d.%02d s"), iPage + 1, iShown.iMs / 1000, iShown.iMs % 1000 / 10);
	else
		l4.Format(_L("Opened in %d.%02d s"), iOpenMs / 1000, iOpenMs % 1000 / 10);
	CEikDialog* d = new(ELeave) CPpDetailsDialog(l1, l2, l3, l4);
	d->ExecuteLD(R_PP_DETAILS_DIALOG);
	}

void CPpAppUi::HandleCommandL(TInt aCommand)
	{
	switch (aCommand)
		{
	case EPpCmdOpen:
		{
		TFileName name = iFile.Length() ? iFile : TFileName(_L("C:\\Documents\\"));
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
	TRAPD(err, iEikonEnv->BusyMsgL(aText, EHLeftVBottom, TTimeIntervalMicroSeconds32(300000)));
	iBusy = err == KErrNone;
	}

void CPpAppUi::BusyEnd()
	{
	if (iBusy)
		iEikonEnv->BusyMsgCancel();
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
			iToolbarShown = ln.Length() == 0 || ln[0] != '0';
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

void CPpAppUi::SaveSettings()
	{
	RFs& fs = iCoeEnv->FsSession();
	fs.MkDirAll(KPpIni);
	RFile f;
	if (f.Replace(fs, KPpIni, EFileWrite) != KErrNone)
		return;
	TBuf8<300> b;
	b.Format(_L8("PsiPDF 1\r\n%d\r\n"), iToolbarShown ? 1 : 0);
	f.Write(b);
	for (TInt i = 0; i < iRecentCount; i++)
		{
		b.Format(_L8("%d\t%d\t"), iRecent[i].iPage, iRecent[i].iZoom);
		b.Append(iRecent[i].iFile.Left(Min(iRecent[i].iFile.Length(), b.MaxLength() - b.Length() - 2)));
		b.Append(_L8("\r\n"));
		f.Write(b);
		}
	f.Close();
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
	else if (iAsleep)
		{
		// back: the file opened again where it was
		iAsleep = EFalse;
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
// code, MuPDF's heap and the page drawn ahead given back). The page on the
// screen stays, with its page, zoom and Find's hits; coming back opens the
// file again (HandleForegroundEventL, then PumpL).
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
	if (!iShown.iDone || iShown.iFailed)
		DropPic(iShown);             // (half a page: drawn again when back)
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
