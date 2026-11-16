// PPREC.CPP - PsiPDF.mdl: says which files are PDF (application/pdf), so
// that a tap on one on the System screen, or in PsiHub, opens it in PsiPDF
// (whose .aif takes that type). In \System\Recogs, where the recogniser
// server loads every .mdl at start-up.
//
// Certain: "%PDF-" in the first 1024 bytes (where PDF readers look for
// it). Probable: a name ending .pdf, with no bytes to go on; possible:
// such a name on a file without that mark (PsiPDF says what is wrong).
//
// Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.

#include <e32std.h>
#include <apmrec.h>
#include <apmstd.h>
#include <f32file.h>

const TUid KUidPpRec = { 0x01000AD9 };

_LIT8(KPpPdf, "application/pdf");

class CPpRecognizer : public CApaDataRecognizerType
	{
public:
	CPpRecognizer() : CApaDataRecognizerType(KUidPpRec, ENormal) { iCountDataTypes = 1; }
	TUint PreferredBufSize() { return 1024; }
	TDataType SupportedDataTypeL(TInt /*aIndex*/) const { return TDataType(KPpPdf); }
private:
	void DoRecognizeL(const TDesC& aName, const TDesC8& aBuffer);
	};

void CPpRecognizer::DoRecognizeL(const TDesC& aName, const TDesC8& aBuffer)
	{
	iConfidence = ENotRecognized;
	TInt n = aBuffer.Length();
	const TUint8* h = aBuffer.Ptr();
	for (TInt i = 0; i + 5 <= n && i < 1024; i++)
		if (h[i] == '%' && h[i + 1] == 'P' && h[i + 2] == 'D' && h[i + 3] == 'F' && h[i + 4] == '-')
			{
			iDataType = TDataType(KPpPdf);
			iConfidence = ECertain;
			return;
			}
	TParsePtrC p(aName);
	if (!p.Ext().CompareF(_L(".pdf")))
		{
		iDataType = TDataType(KPpPdf);
		iConfidence = n < 5 ? EProbable : EPossible;
		}
	}

// the recogniser server's entry point: ordinal 1
EXPORT_C CApaDataRecognizerType* CreateRecognizer()
	{
	return new CPpRecognizer;
	}

GLDEF_C TInt E32Dll(TDllReason)
	{
	return KErrNone;
	}
