// (one copy, kept the same in every Coda program: psion-netbook psisys/ is the master)
// psihelp.h: Tools > Help on <program> (Shift+Ctrl+H) for Coda's own
// programs (review R102): a topic list and a read-only text, as PsiMail's,
// PsiTerm's and PsiLinux's Help, so that every Coda program has the same
// Help in the same place. The text is in the program (no file, so it works
// with no card); topics are short: what the program is for, its keys, and
// what is not obvious.
//
//   static const TPsiHelpTopic KHelp[] = { { "Getting started", "..." }, ... };
//   PsiHelpL(KHelp, sizeof(KHelp) / sizeof(KHelp[0]), _L("Help on PsiFiles"), R_PSI_HELP_DIALOG,
//            R_PSI_HELP_DIALOG_TALL);   // (the second: optional; since 21 Nov 2026 both give the same box)
//
// In the text, '\n' starts a new paragraph; the topic's title is shown bold
// at the top. ASCII only (the copyright sign and the like as in About).
// The resource: psihelp.rssi (with psihelp.hrh). Libraries: eikon, etext,
// form, bafl. Shared by the netBook and the Series 5mx builds.
//
// (21 November 2026, the polish round's leftovers) The text is a box of
// whole lines, 12 on a screen of 400 lines or more and 7 on the 5mx's 240,
// whatever the resource says (the Coda sheet 2.13), its height a whole
// number of the text's own lines. It scrolls a whole line at a time (Up and
// Down; Pg Up and Pg Dn a box less a line; Home and End; Up at the top goes
// back to the Topic line), so the top line is never cut. The scroll bar is
// drawn beside the text only when the topic is longer than the box: a topic
// that fits has none, and its text takes the bar's room. Until then the box
// was EIKON's own edwin and bar: the bar showed on opening for a topic that
// fits, and the edwin scrolled by its cursor, which leaves the last line at
// the bottom and a part line at the top.
#ifndef PSIHELP_H
#define PSIHELP_H
#include <eikdialg.h>
#include <eikfctry.h>     // (SEikControlInfo: the text's custom control)
#include <barsread.h>
#include <eikchlst.h>
#include <eikrted.h>
#include <eikenv.h>
#include <eikcolor.h>
#include <frmtlay.h>
#include <frmtview.h>
#include <txtrich.h>
#include <badesca.h>
#include "psihelp.hrh"

struct TPsiHelpTopic { const char* iTitle; const char* iText; };

const TInt KPsiHelpBarW = 23;               // EIKON's scroll bar width
const TInt KPsiHelpButH = 20;               // its arrow buttons

// ----- the text: whole lines, a bar only when it is longer than the box --------

class CPsiHelpBox : public CCoeControl
	{
public:
	inline ~CPsiHelpBox() { delete iEditor; }
	inline CEikRichTextEditor* Editor() const { return iEditor; }
	// Up at the top of the text calls aTop (the dialog: back to its Topic line)
	inline void SetTop(const TCallBack& aTop) { iTop = aTop; }
	// a new text: at its top, the bar shown or not
	inline void TextChangedL();
private:
	// (the helpers are virtual so that GCC 3.0 compiles each once, not into
	// every caller: inlined, the box was 35 KB of code, virtual 6 KB)
	inline void ConstructFromResourceL(TResourceReader& aReader);
	inline void SetContainerWindowL(const CCoeControl& aContainer);
	inline TSize MinimumSize();
	inline void SizeChangedL() { LayoutL(); }
	inline void PositionChanged() { TRAPD(err, LayoutL()); (void)err; }
	inline TInt CountComponentControls() const { return iEditor ? 1 : 0; }
	inline CCoeControl* ComponentControl(TInt /*aIndex*/) const { return iEditor; }
	virtual inline TInt LineHeight() const;
	virtual inline void FormatL();
	virtual inline TInt Count() const;                // the text's lines
	virtual inline TInt Shown() const;                // the lines the box shows
	virtual inline void LayoutL();
	virtual inline void Parts(TRect& aShaft, TRect& aThumb, TRect& aUp, TRect& aDown) const;
	virtual inline void Face(CWindowGc& aGc, const TRect& aRect, TBool aIn) const;
	virtual inline void DrawBar(CWindowGc& aGc) const;
	inline void Draw(const TRect& /*aRect*/) const;
	virtual inline void UpdateBar();
	virtual inline void ScrollToL(TInt aTop);        // aTop: the line at the top, from 0
	inline TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
	inline void HandlePointerEventL(const TPointerEvent& aEvent);
	CEikRichTextEditor* iEditor;
	TCallBack iTop;
	TRect iBar;                              // empty when the text fits
	TInt iLineH;
	TInt iTopLine;
	TInt iPress;                             // the pen: 1 up, 2 down, 3 the thumb
	TInt iGrab;
	};

inline void CPsiHelpBox::ConstructFromResourceL(TResourceReader& aReader)
	{
	iEditor = new(ELeave) CEikRichTextEditor(TEikBorder(TEikBorder::ENone));
	if (DrawableWindow())
		iEditor->SetContainerWindowL(*this);
	iEditor->ConstructFromResourceL(aReader);
	}

inline void CPsiHelpBox::SetContainerWindowL(const CCoeControl& aContainer)
	{
	CCoeControl::SetContainerWindowL(aContainer);
	if (iEditor)
		iEditor->SetContainerWindowL(*this);
	}

// the text's own line height (its first formatted line), else the font's
inline TInt CPsiHelpBox::LineHeight() const
	{
	CTextLayout* lay = iEditor ? iEditor->TextLayout() : NULL;
	TInt h = 0;
	if (lay && lay->NumFormattedLines() > 0)
		{
		TRect line;
		lay->GetLineRect(1, line);
		h = line.Height();
		}
	if (h < 8 || h > 40)
		h = iEikonEnv->NormalFont()->HeightInPixels() + 4;
	return h;
	}

inline TSize CPsiHelpBox::MinimumSize()
	{
	TInt lines = iEikonEnv->ScreenDevice()->SizeInPixels().iHeight >= 400 ? 12 : 7;
	return TSize(iEditor->MinimumSize().iWidth + KPsiHelpBarW + 2, lines * LineHeight() + 2);
	}

// the whole text formatted (an edwin formats only the band in view once
// its text is long), so its lines can be counted and any one put at the top
inline void CPsiHelpBox::FormatL()
	{
	CTextView* view = iEditor->TextView();
	if (!view || !iEditor->TextLayout())
		return;
	iEditor->TextLayout()->SetAmountToFormat(CTextLayout::EFFormatAllText);
	view->FormatTextL();
	}

inline TInt CPsiHelpBox::Count() const
	{
	CTextLayout* lay = iEditor->TextLayout();
	return lay ? lay->NumFormattedLines() : 0;
	}

inline TInt CPsiHelpBox::Shown() const
	{
	return iLineH > 0 ? iEditor->Rect().Height() / iLineH : 0;
	}

// the text's box: whole lines high (any pixels over stay blank below it),
// as wide as the line less the bar when the text is longer than the box
inline void CPsiHelpBox::LayoutL()
	{
	if (!iEditor)
		return;
	TRect r = Rect();
	r.Shrink(1, 1);
	if (r.Width() <= KPsiHelpBarW + 8 || r.Height() <= 8)
		return;
	iLineH = LineHeight();
	TInt h = (r.Height() / iLineH) * iLineH;
	if (h <= 0)
		h = r.Height();
	iEditor->SetRectL(TRect(r.iTl, TSize(r.Width() - KPsiHelpBarW, h)));
	FormatL();
	if (Count() > Shown())
		iBar = TRect(r.iBr.iX - KPsiHelpBarW, r.iTl.iY, r.iBr.iX, r.iBr.iY);
	else
		{
		iBar = TRect();
		iEditor->SetRectL(TRect(r.iTl, TSize(r.Width(), h)));
		FormatL();
		}
	iTopLine = 0;
	}

inline void CPsiHelpBox::TextChangedL()
	{
	iTopLine = 0;
	iPress = 0;
	if (iEditor->TextView())
		iEditor->TextView()->SetCursorVisibilityL(TCursor::EFCursorInvisible, TCursor::EFCursorInvisible);
	LayoutL();
	}

// the bar's parts: the shaft, the thumb in it, the up and down buttons at
// its foot (as EIKON's)
inline void CPsiHelpBox::Parts(TRect& aShaft, TRect& aThumb, TRect& aUp, TRect& aDown) const
	{
	const TRect& r = iBar;
	aDown = TRect(r.iTl.iX, r.iBr.iY - KPsiHelpButH, r.iBr.iX, r.iBr.iY);
	aUp = TRect(r.iTl.iX, r.iBr.iY - 2 * KPsiHelpButH, r.iBr.iX, r.iBr.iY - KPsiHelpButH);
	aShaft = TRect(r.iTl.iX, r.iTl.iY, r.iBr.iX, aUp.iTl.iY);
	TInt total = Count(), shown = Shown();
	if (total < shown)
		total = shown;
	TInt sh = aShaft.Height();
	TInt th = total > 0 ? sh * shown / total : sh;
	if (th < 16) th = 16;
	if (th > sh) th = sh;
	TInt room = total - shown;
	TInt ty = aShaft.iTl.iY + (room > 0 ? (sh - th) * Min(iTopLine, room) / room : 0);
	aThumb = TRect(aShaft.iTl.iX, ty, aShaft.iBr.iX, ty + th);
	}

// a raised face (the thumb, the buttons), in EIKON's button colours so the
// themes reach it; pressed in while held
inline void CPsiHelpBox::Face(CWindowGc& aGc, const TRect& aRect, TBool aIn) const
	{
	aGc.SetPenStyle(CGraphicsContext::ESolidPen);
	aGc.SetPenColor(KRgbBlack);
	aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	aGc.SetBrushColor(iEikonEnv->ControlColor((TEikLogicalColor)(aIn ? EEikColorButtonFaceClearPressed : EEikColorButtonFaceClear), *this));
	aGc.DrawRect(aRect);
	TRect in(aRect);
	in.Shrink(1, 1);
	aGc.SetPenColor(aIn ? KRgbDarkGray : KRgbWhite);
	aGc.DrawLine(in.iTl, TPoint(in.iBr.iX - 1, in.iTl.iY));
	aGc.DrawLine(in.iTl, TPoint(in.iTl.iX, in.iBr.iY - 1));
	aGc.SetPenColor(aIn ? KRgbWhite : KRgbDarkGray);
	aGc.DrawLine(TPoint(in.iTl.iX, in.iBr.iY - 1), TPoint(in.iBr.iX, in.iBr.iY - 1));
	aGc.DrawLine(TPoint(in.iBr.iX - 1, in.iTl.iY), TPoint(in.iBr.iX - 1, in.iBr.iY));
	}

inline void CPsiHelpBox::DrawBar(CWindowGc& aGc) const
	{
	if (iBar.Width() <= 0)
		return;
	TRect shaft, thumb, up, down;
	Parts(shaft, thumb, up, down);
	aGc.SetPenStyle(CGraphicsContext::ESolidPen);
	aGc.SetPenColor(KRgbBlack);
	aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	aGc.SetBrushColor(iEikonEnv->ControlColor((TEikLogicalColor)EEikColorScrollBarShaft, *this));
	aGc.DrawRect(shaft);
	Face(aGc, thumb, iPress == 3);
	for (TInt b = 0; b < 2; b++)
		{
		const TRect& a = b ? down : up;
		Face(aGc, a, iPress == b + 1);
		aGc.SetPenColor(KRgbBlack);
		TInt cx = a.iTl.iX + a.Width() / 2, cy = a.iTl.iY + a.Height() / 2;
		for (TInt i = 0; i < 4; i++)
			{
			TInt y = b ? cy + 1 - i : cy - 2 + i;
			aGc.DrawLine(TPoint(cx - i, y), TPoint(cx + i + 1, y));
			}
		}
	aGc.SetBrushStyle(CGraphicsContext::ENullBrush);
	}

// the line's edge, as the dialog's other lines; white under the last whole
// line (the editor draws the text and its own ground)
inline void CPsiHelpBox::Draw(const TRect& /*aRect*/) const
	{
	CWindowGc& gc = SystemGc();
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(KRgbBlack);
	gc.SetBrushStyle(CGraphicsContext::ENullBrush);
	gc.DrawRect(Rect());
	if (iEditor)
		{
		TRect r = Rect();
		r.Shrink(1, 1);
		TRect e = iEditor->Rect();
		TRect below(r.iTl.iX, e.iBr.iY, iBar.Width() > 0 ? iBar.iTl.iX : r.iBr.iX, r.iBr.iY);
		if (below.Height() > 0)
			{
			gc.SetPenStyle(CGraphicsContext::ENullPen);
			gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
			gc.SetBrushColor(KRgbWhite);
			gc.DrawRect(below);
			}
		}
	DrawBar(gc);
	}

inline void CPsiHelpBox::UpdateBar()
	{
	if (!IsReadyToDraw() || iBar.Width() <= 0)
		return;
	ActivateGc();
	DrawBar(SystemGc());
	DeactivateGc();
	}

// a whole line at the top: never one cut through
inline void CPsiHelpBox::ScrollToL(TInt aTop)
	{
	TInt most = Count() - Shown();
	if (aTop > most)
		aTop = most;
	if (aTop < 0)
		aTop = 0;
	if (aTop == iTopLine || !iEditor->TextView())
		return;
	TPoint moved = iEditor->TextView()->SetViewLineAtTopL(aTop + 1);
	iTopLine = aTop;
	if (moved.iY == CTextLayout::EFScrollRedrawWholeScreen)
		iEditor->DrawNow();
	UpdateBar();
	}

inline TKeyResponse CPsiHelpBox::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
	{
	if (aType != EEventKey)
		return EKeyWasNotConsumed;
	TInt page = Shown() > 1 ? Shown() - 1 : 1;
	switch (aKeyEvent.iCode)
		{
	case EKeyUpArrow:
		if (iTopLine > 0)
			ScrollToL(iTopLine - 1);
		else if (iTop.iFunction)
			iTop.CallBack();
		break;
	case EKeyDownArrow:
		ScrollToL(iTopLine + 1);
		break;
	case EKeyPageUp:
		ScrollToL(iTopLine - page);
		break;
	case EKeyPageDown:
		ScrollToL(iTopLine + page);
		break;
	case EKeyHome:
		ScrollToL(0);
		break;
	case EKeyEnd:
		ScrollToL(Count());
		break;
	default:
		return EKeyWasNotConsumed;
		}
	return EKeyWasConsumed;
	}

// the pen on the bar: the arrows a line, the shaft a box, the thumb dragged
inline void CPsiHelpBox::HandlePointerEventL(const TPointerEvent& aEvent)
	{
	if (iBar.Width() <= 0)
		return;                              // (the text: nothing to select)
	TPoint p = aEvent.iPosition;
	TRect shaft, thumb, up, down;
	Parts(shaft, thumb, up, down);
	TInt page = Shown() > 1 ? Shown() - 1 : 1;
	if (aEvent.iType == TPointerEvent::EButton1Down)
		{
		iPress = 0;
		if (!iBar.Contains(p))
			return;
		if (up.Contains(p)) { iPress = 1; ScrollToL(iTopLine - 1); }
		else if (down.Contains(p)) { iPress = 2; ScrollToL(iTopLine + 1); }
		else if (thumb.Contains(p)) { iPress = 3; iGrab = p.iY - thumb.iTl.iY; }
		else ScrollToL(p.iY < thumb.iTl.iY ? iTopLine - page : iTopLine + page);
		UpdateBar();
		}
	else if (aEvent.iType == TPointerEvent::EDrag && iPress == 3)
		{
		TInt room = shaft.Height() - thumb.Height(), most = Count() - Shown();
		if (room > 0 && most > 0)
			ScrollToL(((p.iY - iGrab - shaft.iTl.iY) * most + room / 2) / room);
		}
	else if (aEvent.iType == TPointerEvent::EButton1Up && iPress)
		{
		iPress = 0;
		UpdateBar();
		}
	}

// ----- the dialog --------------------------------------------------------------

class CPsiHelpDialog : public CEikDialog
	{
public:
	inline CPsiHelpDialog(const TPsiHelpTopic* aTopics, TInt aCount, const TDesC& aTitle)
		: iTopics(aTopics), iCount(aCount), iTitle(aTitle) {}
private:
	inline void SetSizeAndPositionL(const TSize& aSize);
	inline SEikControlInfo CreateCustomControlL(TInt aControlType);
	inline void PreLayoutDynInitL();
	inline void PostLayoutDynInitL() { TryChangeFocusToL(EPsiHelpTopic); }
	inline void HandleControlStateChangeL(TInt aControlId);
	inline TBool OkToExitL(TInt) { return ETrue; }
	virtual inline void ShowTopicL(TInt aTopic);
	static inline TInt BackToTopic(TAny* aDialog);
	const TPsiHelpTopic* iTopics;
	TInt iCount;
	TPtrC iTitle;
	TInt iTopic;
	};

inline void CPsiHelpDialog::SetSizeAndPositionL(const TSize& aSize)
	{
	TSize screen = iEikonEnv->ScreenDevice()->SizeInPixels();
	TSize size(Min(aSize.iWidth, screen.iWidth - 8), Min(aSize.iHeight, screen.iHeight - 8));
	SetCornerAndSizeL(EHCenterVCenter, size);
	}

inline TInt CPsiHelpDialog::BackToTopic(TAny* aDialog)
	{
	TRAPD(err, ((CPsiHelpDialog*)aDialog)->TryChangeFocusToL(EPsiHelpTopic));
	return err;
	}

inline SEikControlInfo CPsiHelpDialog::CreateCustomControlL(TInt aControlType)
	{
	SEikControlInfo info;
	info.iControl = NULL;
	info.iTrailerTextId = 0;
	info.iFlags = 0;
	if (aControlType == EPsiCtHelpBox)
		{
		CPsiHelpBox* box = new(ELeave) CPsiHelpBox;
		box->SetTop(TCallBack(BackToTopic, this));
		info.iControl = box;
		}
	return info;
	}

inline void CPsiHelpDialog::PreLayoutDynInitL()
	{
	SetTitleL(iTitle);
	CDesCArrayFlat* titles = new(ELeave) CDesCArrayFlat(iCount > 0 ? iCount : 1);
	CleanupStack::PushL(titles);
	TBuf<40> t;
	for (TInt i = 0; i < iCount; i++)
		{
		TPtrC8 n((const TUint8*)iTopics[i].iTitle);
		t.Copy(n.Left(Min(n.Length(), t.MaxLength())));   // (ER5: Left past the end panics, USER 22)
		titles->AppendL(t);
		}
	CleanupStack::Pop();
	CEikChoiceList* cl = (CEikChoiceList*)Control(EPsiHelpTopic);
	cl->SetArrayL(titles);
	iTopic = 0;
	cl->SetCurrentItem(0);
	if (iCount > 0)
		ShowTopicL(0);
	}

inline void CPsiHelpDialog::ShowTopicL(TInt aTopic)
	{
	TPtrC8 title((const TUint8*)iTopics[aTopic].iTitle);
	TPtrC8 text((const TUint8*)iTopics[aTopic].iText);
	// (trailing new lines dropped: blank lines after the text, which made a
	// topic that fits look longer than the box and gave it a bar)
	TInt n = text.Length();
	while (n > 0 && text[n - 1] == '\n')
		n--;
	text.Set(text.Left(n));
	HBufC* b = HBufC::NewLC(title.Length() + text.Length() + 2);
	TPtr p = b->Des();
	p.Copy(title);
	p.Append(CEditableText::EParagraphDelimiter);
	for (TInt i = 0; i < text.Length(); i++)
		p.Append(text[i] == '\n' ? (TText)CEditableText::EParagraphDelimiter : (TText)text[i]);
	CPsiHelpBox* box = (CPsiHelpBox*)Control(EPsiHelpText);
	CEikRichTextEditor* ed = box->Editor();
	ed->SetTextL(b);
	CleanupStack::PopAndDestroy();
	TCharFormat cf;
	TCharFormatMask cm;
	cf.iFontSpec.iFontStyle.SetStrokeWeight(EStrokeWeightBold);
	cm.SetAttrib(EAttFontStrokeWeight);
	ed->RichText()->ApplyCharFormatL(cf, cm, 0, title.Length());
	ed->HandleTextChangedL();
	ed->SetCursorPosL(0, EFalse);
	box->TextChangedL();                    // (at its top, a bar only if it is longer than the box)
	if (IsActivated())
		box->DrawDeferred();                // (all of it again: its width may have changed)
	}

inline void CPsiHelpDialog::HandleControlStateChangeL(TInt aControlId)
	{
	if (aControlId != EPsiHelpTopic)
		return;
	TInt t = ((CEikChoiceList*)Control(EPsiHelpTopic))->CurrentItem();
	if (t != iTopic && t >= 0 && t < iCount)
		{
		iTopic = t;
		ShowTopicL(t);
		}
	}

// the dialog, run to its end
// (the tall resource is the same as the other since the text box sizes
// itself by the screen: kept so that callers need not change)
inline void PsiHelpL(const TPsiHelpTopic* aTopics, TInt aCount, const TDesC& aTitle, TInt aResource, TInt aTallResource = 0)
	{
	CEikDialog* d = new(ELeave) CPsiHelpDialog(aTopics, aCount, aTitle);
	if (aTallResource && CEikonEnv::Static()->ScreenDevice()->SizeInPixels().iHeight >= 400)
		aResource = aTallResource;
	d->ExecuteLD(aResource);
	}

#endif
