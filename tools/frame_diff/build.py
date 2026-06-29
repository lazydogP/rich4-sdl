"""Build the isolated x86 observational tools (invoked through Make)."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / 'bin/frame_diff'

def main():
    cc = os.environ.get('R4DIFF_CC') or shutil.which('gcc')
    if not cc:
        raise SystemExit('error: set R4DIFF_CC to an x86 or multilib MinGW GCC')
    OUT.mkdir(parents=True, exist_ok=True)
    flags = ['-m32', '-O2', '-g', '-Wall', '-Wextra', '-D__USE_MINGW_ANSI_STDIO=1', '-static-libgcc']
    for output, sources, extra in [
        ('original_probe.dll', ['probe.c', 'probe_ddraw.c', 'probe_asm.S', 'lockstep_wire.c', 'original_lockstep.c'], ['-shared', '-lwinmm', '-luser32']),
        ('original_launcher.exe', ['launcher.c'], ['-luser32']),
    ]:
        subprocess.run([cc, *flags, *[str(HERE / s) for s in sources], '-o', str(OUT / output), *extra], check=True)
    print('frame-diff: built x86 launcher and observation probe')

if __name__ == '__main__':
    main()
