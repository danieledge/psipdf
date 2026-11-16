// pplaunch.cpp - for the netBook emulator only, never a real image. The
// emulator does not mount the card, so PsiPDF cannot be tested there as it
// is installed (on D:). In a scratch test image PsiPDF.app is in the ROM
// and its engine beside it in 64 KB parts (Z:\System\Apps\PsiPDF\Engine\
// psipdf.b0, .b1...). This program, built under Jotter's UID and put in
// Jotter's place so that the Shell starts it at boot, joins the parts into
// C:\System\Apps\PsiPDF\psipdf.exe and closes. PsiPDF finds its engine
// there and runs it from RAM, as it runs one on the card.
// Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
#include <eikapp.h>
#include <eikdoc.h>
#include <eikappui.h>
#include <eikenv.h>
#include <f32file.h>
#include <eikcmds.hrh>

const TUid KUidLauncher = { 0x10000CEA };
_LIT(KFrom, "Z:\\System\\Apps\\PsiPDF\\Engine\\psipdf.b%d");
_LIT(KDir, "C:\\System\\Apps\\PsiPDF\\");
_LIT(KTo, "C:\\System\\Apps\\PsiPDF\\psipdf.exe");

class CPlAppUi : public CEikAppUi
	{
public:
	void ConstructL()
		{
		BaseConstructL();
		RFs& fs = iEikonEnv->FsSession();
		fs.MkDirAll(KDir);
		RFile out;
		if (out.Replace(fs, KTo, EFileWrite) == KErrNone)
			{
			HBufC8* buf = HBufC8::NewL(65536);
			TPtr8 b = buf->Des();
			for (TInt i = 0;; i++)
				{
				TFileName name;
				name.Format(KFrom, i);
				RFile in;
				if (in.Open(fs, name, EFileRead | EFileShareReadersOnly) != KErrNone)
					break;
				for (;;)
					{
					in.Read(b);
					if (b.Length() == 0)
						break;
					out.Write(b);
					}
				in.Close();
				}
			delete buf;
			out.Close();
			}
		// (closed once it is up: Exit() is not for ConstructL)
		iIdle = CIdle::NewL(CActive::EPriorityIdle);
		iIdle->Start(TCallBack(Close, this));
		}
	~CPlAppUi() { delete iIdle; }
	static TInt Close(TAny* aSelf)
		{
		((CPlAppUi*)aSelf)->Exit();
		return 0;
		}
	void HandleCommandL(TInt aCommand)
		{
		if (aCommand == EEikCmdExit)
			Exit();
		}
	CIdle* iIdle;
	};

class CPlDocument : public CEikDocument
	{
public:
	CPlDocument(CEikApplication& aApp) : CEikDocument(aApp) {}
private:
	CEikAppUi* CreateAppUiL() { return new(ELeave) CPlAppUi; }
	};

class CPlApplication : public CEikApplication
	{
private:
	CApaDocument* CreateDocumentL() { return new(ELeave) CPlDocument(*this); }
	TUid AppDllUid() const { return KUidLauncher; }
	};

EXPORT_C CApaApplication* NewApplication()
	{
	return new CPlApplication;
	}

GLDEF_C TInt E32Dll(TDllReason)
	{
	return KErrNone;
	}
