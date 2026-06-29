"""Project-local paths shared by Mini builds, packaging and QEMU checks."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / '.local/miyoo'
TRIPLE = 'arm-none-linux-gnueabihf'
GCC_VERSION = '10.3.1'
GCC = LOCAL / 'gcc/bin' / f'{TRIPLE}-gcc.exe'
STRIP = GCC.with_name(f'{TRIPLE}-strip.exe')
BUILTIN = LOCAL / f'gcc/lib/gcc/{TRIPLE}/{GCC_VERSION}/include'
SYSROOT = LOCAL / 'sysroot'
QEMU = LOCAL / 'qemu/qemu-system-arm.exe'
SEVEN_ZIP = LOCAL / '7zip/7z.exe'
MKE2FS = LOCAL / 'platform-tools/mke2fs.exe'
FFMPEG = LOCAL / 'ffmpeg/bin/ffmpeg.exe'
KERNEL = LOCAL / 'vm/zImage'
KERNEL_DEB = LOCAL / 'vm/kernel.deb'
BUSYBOX = LOCAL / 'vm/busybox'


def require_files(paths, setup_args=''):
    missing = [path.relative_to(ROOT).as_posix() for path in paths if not path.is_file()]
    if missing:
        command = f'python scripts/setup_miyoo_toolchain.py{setup_args}'
        raise FileNotFoundError(f'Missing local Mini tools: {", ".join(missing)}. Run: {command}')


def require_build_tools():
    require_files((GCC, STRIP, BUILTIN / 'stddef.h',
                   SYSROOT / 'usr/include/SDL/SDL.h', SYSROOT / 'usr/lib/libSDL.so',
                   SYSROOT / 'usr/lib/libSDL_mixer.so', SYSROOT / 'usr/lib/libSDL_ttf.so'))


def require_qemu_tools():
    require_build_tools()
    require_files((QEMU, SEVEN_ZIP, MKE2FS, KERNEL, KERNEL_DEB, BUSYBOX), ' --qemu')
