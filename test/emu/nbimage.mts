// nbimage.mts - a scratch netBook image for the emulator (never a real one)
// with PsiPDF in it. The emulator does not mount the card, so PsiPDF goes
// in the ROM: PsiPDF.app (execute in place) with its .rsc, .aif and .mbm in
// Z:\System\Apps\PsiPDF\, the recogniser in Z:\System\Recogs\, and the engine
// (which has writable data, so cannot run in place) as 64 KB data parts in
// Z:\System\Apps\PsiPDF\Engine\. The test launcher (launch/), in Jotter's
// place, joins the parts on C: at boot, where PsiPDF finds its engine and
// runs it from RAM, as it does from the card.
//
//   node --experimental-strip-types test/emu/nbimage.mts OS.IMG
//
// Needs the Coda image tools (romlib.mts: PSION_NETBOOK, standard
// ../psion-netbook). OS.IMG is changed in place.
// Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
import { readFileSync, writeFileSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
const top = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const nb = process.env.PSION_NETBOOK ?? resolve(top, '../psion-netbook');
const { load, find, addFile, removeChecked, write } = await import(resolve(nb, 'tools/romlib.mts'));
const [img] = process.argv.slice(2);
const rom = load(img);
const b = resolve(top, 'build');
// room: the spare wallpapers, Help, the spelling dictionary and samples out of this scratch image
for (const n of removeChecked(rom, (['Z:\\System\\Wallpaper\\Coda psi', 'Z:\\System\\Wallpaper\\Coda hills',
  'Z:\\System\\Wallpaper\\Coda sweep', 'Z:\\System\\Wallpaper\\Coda shapes', 'Z:\\System\\Data\\Help',
  'Z:\\System\\Data\\Iens9522.dat', 'Z:\\System\\Samples\\PsiAmp\\Coda.mp3'] as string[]).filter(p => find(rom, p).length))) console.log(n);
addFile(rom, 'Z:\\System\\Apps\\Jotter\\Jotter.app', readFileSync(`${b}/launch/pplaunch.app`), 'xip');
addFile(rom, 'Z:\\System\\Apps\\Jotter\\Jotter.RSC', readFileSync(`${b}/launch/pplaunch.rsc`));
addFile(rom, 'Z:\\System\\Apps\\PsiPDF\\PsiPDF.app', readFileSync(`${b}/app/psipdf.app`), 'xip');
addFile(rom, 'Z:\\System\\Apps\\PsiPDF\\PsiPDF.rsc', readFileSync(`${b}/app/psipdf.rsc`));
addFile(rom, 'Z:\\System\\Apps\\PsiPDF\\PsiPDF.aif', readFileSync(`${b}/pkg/psipdf.aif`));
addFile(rom, 'Z:\\System\\Apps\\PsiPDF\\PsiPDF.mbm', readFileSync(`${b}/pkg/psipdf.mbm`));
addFile(rom, 'Z:\\System\\Recogs\\PsiPDF.mdl', readFileSync(`${b}/rec/psipdf.mdl`), 'xip');
const exe = readFileSync(`${b}/epoc/psipdf.exe`);
for (let i = 0, o = 0; o < exe.length; i++, o += 65536)
  addFile(rom, `Z:\\System\\Apps\\PsiPDF\\Engine\\psipdf.b${i}`, exe.subarray(o, o + 65536));
const { img: out, notes } = write(rom);
writeFileSync(img, out);
for (const n of notes) console.log(n);
console.log(`PsiPDF in ${img} (the engine in ${Math.ceil(exe.length / 65536)} parts, the test launcher in Jotter's place)`);
