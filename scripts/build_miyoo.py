#!/usr/bin/env python3
"""Build against the project's prepared Miyoo Mini SDK."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from miyoo_toolchain import GCC, BUILTIN, SYSROOT, require_build_tools

ROOT = Path(__file__).resolve().parents[1]


def update_toolchain_stamp(command):
    """Recompile when the SDK, compiler path or build options change."""
    stamp = ROOT / 'build/miyoo-ttf/.toolchain-stamp'
    manifest = ROOT / 'scripts/miyoo_toolchain.lock.json'
    signature = json.dumps({'command': command,
                            'sdk': hashlib.sha256(manifest.read_bytes()).hexdigest()}, indent=2)
    if not stamp.is_file() or stamp.read_text(encoding='utf-8') != signature:
        stamp.parent.mkdir(parents=True, exist_ok=True)
        stamp.write_text(signature, encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qa', action='store_true', help='also build separate QEMU test helpers')
    args = parser.parse_args()
    try:
        require_build_tools()
    except FileNotFoundError as error:
        parser.error(str(error))
    gcc, sdk = GCC, SYSROOT
    # Use the Mini's glibc headers, not the GCC distribution's newer libc.
    builtin = BUILTIN
    sdk_path = sdk.as_posix()
    flags = (f'-O2 -g1 -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -marm '
             f'--sysroot="{sdk_path}" -nostdinc -isystem "{builtin.as_posix()}" '
             f'-isystem "{sdk_path}/usr/include" '
             f'-isystem "{builtin.parent.as_posix()}/include-fixed" -no-pie')
    cmd = [sys.executable, 'scripts/make_minimal.py', 'sdl', '--progress', '-j8',
           f'CC="{gcc.as_posix()}"', 'BUILD_DIR=build/miyoo-ttf', 'BIN_DIR=bin/miyoo',
           'CPPFLAGS=-Isrc -DRICH4_PLATFORM_HANDHELD -DRICH4_PLATFORM_MIYOO -D_GNU_SOURCE=1 -D_REENTRANT',
           f'CFLAGS={flags}', 'SDL_CFLAGS=',
           'SDL_PREFIX=', 'MIXER_PREFIX=', 'TTF_PREFIX=', 'LOCAL_TOOLCHAIN_BIN=',
           f'SDL_LIBS="{sdk_path}/usr/lib/libSDL.so" -lm -lpthread',
           'MIXER_CFLAGS=',
           f'MIXER_LIBS="{sdk_path}/usr/lib/libSDL_mixer.so"',
           'TTF_CFLAGS=-DHAVE_SDL_TTF=1',
           f'TTF_LIBS="{sdk_path}/usr/lib/libSDL_ttf.so" -Wl,-rpath-link,"{sdk_path}/usr/lib"']
    # GNU Make excludes .EXTRA_PREREQS from $^, so link recipes keep their
    # existing inputs. The stamp also invalidates objects from the old SDK path.
    cmd.append('.EXTRA_PREREQS=build/miyoo-ttf/.toolchain-stamp')
    update_toolchain_stamp(cmd)
    if args.qa:
        cmd.extend(['bin/miyoo/test_miyoo_qemu_probe', 'bin/miyoo/test_miyoo_asset_scope', 'bin/miyoo/test_flc_playback', 'bin/miyoo/test_property_map_presentation', 'bin/miyoo/test_present_pixels', 'bin/miyoo/test_sdl1_blit', 'build/miyoo-ttf/libmiyoo_qa.so'])
    return subprocess.call(cmd, cwd=ROOT)


if __name__ == '__main__':
    sys.exit(main())
