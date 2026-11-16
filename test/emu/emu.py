#!/usr/bin/env python3
"""Runs the netBook emulator on an image with a script of taps and keys, and
saves the screenshots as PNGs.

  python3 test/emu/emu.py CARD.img OUTDIR SCRIPT [--every S] [--until T]

SCRIPT is a file (or '-') of lines:
  at T              set the clock (simulated seconds after the card attach)
  wait S            move the clock on
  tap X Y           a tap (screen x, y; the silkscreen is x 680)
  key CODE          a key (EPOC scan code: Enter 3, Esc 4, Space 5, Menu 148,
                    arrows 14-17, Del 1, letters as capitals)
  ctrl CH / sctrl CH   Ctrl+CH, Shift+Ctrl+CH
  ctrlfn CH         Ctrl+Fn+CH
  shift CODE / fn CODE  a key with Shift or Fn
  type TEXT         letters and digits
  shot NAME         a screenshot at this point, saved as NAME.png
  drag X1 Y1 X2 Y2  a pen drag
Options: --mic (feed the microphone silence), --exc (PSION_EXC_TRACE=1),
  --save-card PATH (the card as the run left it)
Each key moves the clock on by 0.5 s, a tap by 1 s.
"""
# (Dan Edge's netBook emulator driver, as his other programs' tests have it)
import os, subprocess, sys
from PIL import Image

EMU = os.path.expanduser('~/www/psion/psionEmulators')
SHIFT, CTRL, FN = 18, 22, 24
# the netBook's keys for signs (scan codes; found in the emulator)
SYM = {' ': (0, 5), '.': (0, 122), ',': (0, 121), '/': (0, 123), '-': (0, 130), '=': (0, 131),
       '+': (SHIFT, 131), '*': (SHIFT, 56), '(': (SHIFT, 57), ')': (SHIFT, 48), '^': (SHIFT, 54),
       '!': (SHIFT, 49), '%': (SHIFT, 53), '>': (SHIFT, 122)}

def main():
    img, out, script = sys.argv[1:4]
    every = 1.0
    if '--every' in sys.argv: every = float(sys.argv[sys.argv.index('--every') + 1])
    os.makedirs(out, exist_ok=True)
    lines = (sys.stdin if script == '-' else open(script)).read().splitlines()
    t = 100.0
    args, shots = [], []
    def key(code, frames=8):
        args.extend(['--press-key', '%.2f' % t, str(code), str(frames)])
    for ln in lines:
        p = ln.split()
        if not p or p[0].startswith('#'): continue
        c = p[0]
        if c == 'at': t = float(p[1])
        elif c == 'wait': t += float(p[1])
        elif c == 'drag':
            args.extend(['--drag-seq', '%.2f' % t] + p[1:5]); t += 1.5
        elif c == 'tap':
            args.extend(['--tap-seq', '%.2f' % t, p[1], p[2]]); t += 1.0
        elif c == 'key': key(int(p[1])); t += 0.5
        elif c in ('ctrl', 'sctrl'):
            key(CTRL, 30)
            if c == 'sctrl': t0 = t; t += 0.03; key(SHIFT, 26); t = t0
            t += 0.12; key(int(p[1]) if p[1].isdigit() and len(p[1]) > 1 else ord(p[1].upper())); t += 0.6
        elif c == 'ctrlfn':
            # Ctrl+Fn+CH: PsiAmp's keys in every program
            t0 = t
            key(CTRL, 34); t += 0.03; key(FN, 30); t = t0
            t += 0.15; key(ord(p[1].upper())); t += 0.6
        elif c in ('shift', 'fn'):
            key(SHIFT if c == 'shift' else FN, 24); t += 0.08
            key(int(p[1])); t += 0.5
        elif c == 'type':
            for ch in ' '.join(p[1:]):
                if ch in SYM:
                    mod, code = SYM[ch]
                    if mod:
                        key(mod, 24); t += 0.08
                    key(code)
                elif ch.isupper():
                    key(SHIFT, 24); t += 0.08; key(ord(ch))
                else:
                    key(ord(ch.upper()))
                t += 0.4
        elif c == 'shot': shots.append((t, p[1])); t += 0.2
        else: raise SystemExit('unknown: ' + ln)
    end = max([t] + [s[0] for s in shots]) + 2
    env = dict(os.environ, PSION_NB_HOOK_MASK='0xdf', HARNESS_RGB='1')
    if '--exc' in sys.argv: env['PSION_EXC_TRACE'] = '1'
    if '--mic' in sys.argv: args.append('--feed-mic-silence')
    if '--save-ram' in sys.argv: args.extend(['--save-ram-snapshot', os.path.abspath(sys.argv[sys.argv.index('--save-ram') + 1])])
    if '--save-card' in sys.argv: args.extend(['--save-card', os.path.abspath(sys.argv[sys.argv.index('--save-card') + 1])])
    cmd = ['./harness/run', 'roms/netBook/BootLoader/netBook_BL_v011_eng/netBook_BL_v011_eng.bin',
           '--device', 'netbook', '--boot-seconds', '3', '--card-path', os.path.abspath(img),
           '--post-attach-seconds', '%.1f' % end, '--screenshot-every', str(every), os.path.join(os.path.abspath(out), 'f')] + args
    with open(os.path.join(out, 'log.txt'), 'w') as log:
        subprocess.run(cmd, cwd=EMU, stdout=log, stderr=subprocess.STDOUT, env=env)
    # frame k is taken at (k+1)*every seconds of simulated time after attach (approx.)
    frames = sorted(f for f in os.listdir(out) if f.startswith('f-') and f.endswith('.pgm'))
    for ts, name in shots:
        k = min(len(frames) - 1, max(0, int(round(ts / every)) - 1))
        Image.open(os.path.join(out, frames[k])).convert('RGB').save(os.path.join(out, name + '.png'))
    last = Image.open(os.path.join(out, frames[-1])).convert('RGB'); last.save(os.path.join(out, 'end.png'))
    for f in frames: os.remove(os.path.join(out, f))
    print('%d frames, shots: %s' % (len(frames), ', '.join(s[1] for s in shots)))

main()
