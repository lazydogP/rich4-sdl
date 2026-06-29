#!/usr/bin/env python3
"""Bake actual CP950 bytes, including the game's runtime Big5 strings."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    # Scanning C source bytes scans ASCII escape sequences / UTF-8 comments,
    # not the Big5 strings those sources produce at runtime. Cover CP950's
    # valid double-byte repertoire to include resource and generated text.
    corpus = bytearray(range(0x20, 0x7f))
    count = 0
    for lead in range(0x81, 0xff):
        for trail in (*range(0x40, 0x7f), *range(0xa1, 0xff)):
            code = bytes((lead, trail))
            try:
                code.decode('cp950')
            except UnicodeDecodeError:
                continue
            corpus.extend(code)
            count += 1
    path = ROOT / 'build/miyoo/font-corpus.big5'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(corpus)
    executable = ROOT / ('bin/font_bake.exe' if os.name == 'nt' else 'bin/font_bake')
    env = os.environ.copy()
    env['SDL_VIDEODRIVER'] = 'dummy'
    subprocess.run([str(executable), 'assets/fonts/rich4.ttc', '16',
                    'assets/fonts/rich4_font.r4atlas', str(path)], cwd=ROOT, env=env, check=True)
    print(f'Miyoo font: {count} CP950 double-byte codes plus printable ASCII')


if __name__ == '__main__':
    main()
