// (a copy of Coda's psisys/psihelp.h, kept the same in every Coda program: change both)
// psihelp.h: Tools > Help on <program> (Shift+Ctrl+H) for Coda's own
// programs (review R102): a topic list and a read-only text, as PsiMail's,
// PsiTerm's and PsiLinux's Help, so that every Coda program has the same
// Help in the same place. The text is in the program (no file, so it works
// with no card); topics are short: what the program is for, its keys, and
// what is not obvious.
//
//   static const TPsiHelpTopic KHelp[] = { { "Getting started", "..." }, ... };
//   PsiHelpL(KHelp, sizeof(KHelp) / sizeof(KHelp[0]), _L("Help on PsiFiles"), R_PSI_HELP_DIALOG);
//
// In the text, '\n' starts a new paragraph; the topic's title is shown bold
// at the top. ASCII only (the copyright sign and the like as in About).
// The resource: psihelp.rssi (with psihelp.hrh). Libraries: eikon, etext,
// bafl. Shared by the netBook and the Series 5mx builds.
#ifndef PSIHELP_H
#define PSIHELP_H
#include <eikdialg.h>
#include <eikchlst.h>
#include <eikrted.h>
#include <txtrich.h>
#include <badesca.h>
#include "psihelp.hrh"

struct TPsiHelpTopic { const char* iTitle; const char* iText; };

class CPsiHelpDialog : public CEikDialog
	{
public:
	inline CPsiHelpDialog(const TPsiHelpTopic* aTopics, TInt aCount, const TDesC& aTitle)
		: iTopics(aTopics), iCount(aCount), iTitle(aTitle) {}
private:
	inline void SetSizeAndPositionL(const TSize& aSize);
	inline void PreLayoutDynInitL();
	inline void PostLayoutDynInitL() { TryChangeFocusToL(EPsiHelpTopic); }
	inline void HandleControlStateChangeL(TInt aControlId);
	inline TBool OkToExitL(TInt) { return ETrue; }
	inline void ShowTopicL(TInt aTopic);
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
	HBufC* b = HBufC::NewLC(title.Length() + text.Length() + 2);
	TPtr p = b->Des();
	p.Copy(title);
	p.Append(CEditableText::EParagraphDelimiter);
	for (TInt i = 0; i < text.Length(); i++)
		p.Append(text[i] == '\n' ? (TText)CEditableText::EParagraphDelimiter : (TText)text[i]);
	CEikRichTextEditor* ed = (CEikRichTextEditor*)Control(EPsiHelpText);
	ed->SetTextL(b);
	CleanupStack::PopAndDestroy();
	TCharFormat cf;
	TCharFormatMask cm;
	cf.iFontSpec.iFontStyle.SetStrokeWeight(EStrokeWeightBold);
	cm.SetAttrib(EAttFontStrokeWeight);
	ed->RichText()->ApplyCharFormatL(cf, cm, 0, title.Length());
	ed->HandleTextChangedL();
	ed->SetCursorPosL(0, EFalse);
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
inline void PsiHelpL(const TPsiHelpTopic* aTopics, TInt aCount, const TDesC& aTitle, TInt aResource)
	{
	CEikDialog* d = new(ELeave) CPsiHelpDialog(aTopics, aCount, aTitle);
	d->ExecuteLD(aResource);
	}

#endif
