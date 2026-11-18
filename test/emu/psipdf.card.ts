// psipdf.card.ts - PsiPDF's card for the Series 5mx emulator (psion-core's
// tools/emu/mkcard.ts loads it): PsiPDF on D: as its .sis installs it, the
// recogniser in \System\Recogs\, and the test PDFs (build/pdfs) in the
// card's top folder.
//   node --experimental-strip-types core/tools/emu/mkcard.ts psipdf
// Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
import { readdirSync, existsSync } from 'node:fs';
export const cards = {
	psipdf: (c: any) => {
		const b = c.REPO + 'build/';
		c.w('SYSTEM/APPS/PSIPDF/PSIPDF.APP', c.f(b + 'app/psipdf.app'));
		c.w('SYSTEM/APPS/PSIPDF/PSIPDF.RSC', c.f(b + 'app/psipdf.rsc'));
		c.w('SYSTEM/APPS/PSIPDF/PSIPDF.AIF', c.f(b + 'pkg/psipdf.aif'));
		c.w('SYSTEM/APPS/PSIPDF/PSIPDF.MBM', c.f(b + 'pkg/psipdf.mbm'));
		c.w('SYSTEM/APPS/PSIPDF/PSIPDF.EXE', c.f(b + 'epoc/psipdf.exe'));
		c.w('SYSTEM/APPS/PSIPDF/PSIPDF.FNT', c.f(b + 'gen/PsiPDF.fnt'));
		c.w('SYSTEM/RECOGS/PSIPDF.MDL', c.f(b + 'rec/psipdf.mdl'));
		const d = b + 'pdfs/';
		if (existsSync(d))
			for (const n of readdirSync(d).filter((n: string) => /^(coda|text|photo|multi|locked)\.pdf$/.test(n)).sort())
				c.w(n.toUpperCase(), c.f(d + n));
	},
};
