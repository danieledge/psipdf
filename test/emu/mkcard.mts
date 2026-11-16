// mkcard.mts - a CompactFlash card for the netBook emulator with PsiPDF on
// it as its .sis would install it on D: (\System\Apps\PsiPDF\, the
// recogniser in \System\Recogs\) and the test PDFs in \Documents\.
//
//   node --experimental-strip-types test/emu/mkcard.mts OS.IMG card.img PDFDIR
//
// OS.IMG is any netBook image (Psion's own, or Coda's); the card's FAT16 code
// is the emulator's (PSION_EMU, standard ../psionEmulators).
// Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const top = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const emu = process.env.PSION_EMU ?? resolve(top, '../psionEmulators');
const fat = await import(resolve(emu, 'frontend/src/lib/fat16.ts'));
const [osImg, out, pdfs] = process.argv.slice(2);
if (!osImg || !out || !pdfs) {
  console.error('usage: mkcard.mts OS.IMG card.img PDFDIR');
  process.exit(1);
}
const img: Uint8Array = fat.createBlankImage(64 * 1024 * 1024);
const put = (path: string, file: string) => {
  const data = new Uint8Array(readFileSync(file));
  const r = path.includes('/') ? fat.writeFileAtPath(img, path, data) : fat.addFile(img, path, data);
  if (!r.ok) throw new Error(`${path}: ${r.reason}`);
};
put('OS.IMG', osImg);
const b = resolve(top, 'build');
put('System/Apps/PsiPDF/PsiPDF.app', `${b}/app/psipdf.app`);
put('System/Apps/PsiPDF/PsiPDF.rsc', `${b}/app/psipdf.rsc`);
put('System/Apps/PsiPDF/PsiPDF.aif', `${b}/pkg/psipdf.aif`);
put('System/Apps/PsiPDF/PsiPDF.mbm', `${b}/pkg/psipdf.mbm`);
put('System/Apps/PsiPDF/psipdf.exe', `${b}/epoc/psipdf.exe`);
put('System/Recogs/PsiPDF.mdl', `${b}/rec/psipdf.mdl`);
const names = readdirSync(pdfs).filter(n => /\.pdf$/i.test(n)).sort();
for (const n of names) put(`Documents/${n}`, resolve(pdfs, n));
writeFileSync(out, Buffer.from(img.buffer, img.byteOffset, img.byteLength));
console.log(`wrote ${out}: PsiPDF and ${names.length} PDFs (${names.join(', ')})`);
