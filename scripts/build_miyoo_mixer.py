#!/usr/bin/env python3
"""Build the native SDL 1.2 mixer runtime with the project-local Mini SDK.

Upstream references: SDL_mixer 1.2.12 configure.in (source/feature lists),
Xiph libogg/Tremor Makefile.am. No SDL2 compatibility layer is used.
"""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile

from miyoo_toolchain import ROOT, LOCAL, GCC, BUILTIN, SYSROOT, STRIP, require_build_tools
from setup_miyoo_toolchain import fetch

WORK = LOCAL / 'mixer'
OUT = ROOT / 'build/miyoo/mixer'
LOCK = ROOT / 'scripts/miyoo_mixer.lock.json'
MIXER = OUT / 'tremor/libSDL_mixer-1.2.so.0'
MAD = OUT / 'tremor/libmad.so.0'
GCC_RUNTIME = OUT / 'tremor/libgcc_s.so.1'


def run(cmd):
    result = subprocess.run([str(x) for x in cmd], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def source(name, spec, offline=False):
    archive = fetch(spec, offline=offline)
    dest = WORK / 'src' / name
    dest.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as ar:
        ar.extractall(dest, filter='data')
    return next(p for p in dest.iterdir() if p.is_dir())


def ensure_runtime(offline=False):
    require_build_tools()
    OUT.mkdir(parents=True, exist_ok=True)
    specs = json.loads(LOCK.read_text())
    # All CI inputs come from pinned sources and the pinned Mini SDK, never SSH.
    madlib = SYSROOT / 'usr/lib/libmad.so.0.2.1'
    signature = {
        'script_sha256': digest(Path(__file__)),
        'sources': specs,
        'sdk_manifest_sha256': digest(ROOT / 'scripts/miyoo_toolchain.lock.json'),
        'compiler_sha256': digest(GCC), 'libmad_sha256': digest(madlib),
        'libgcc_sha256': digest(SYSROOT / 'lib/libgcc_s.so.1'),
        'sdl_sha256': digest(SYSROOT / 'usr/lib/libSDL.so'),
    }
    marker = OUT / 'runtime.json'
    artifacts = [MIXER, MAD, GCC_RUNTIME]
    if marker.is_file() and all(p.is_file() for p in artifacts):
        previous = json.loads(marker.read_text())
        if (previous.get('signature') == signature and
                previous.get('artifacts') == {p.relative_to(OUT).as_posix(): digest(p) for p in artifacts}):
            return MIXER
    src = {name: source(name, spec, offline=offline) for name, spec in specs.items()}
    inc = WORK / 'include'
    (inc / 'ogg').mkdir(parents=True, exist_ok=True)
    (inc / 'tremor').mkdir(parents=True, exist_ok=True)
    (inc / 'ogg/config_types.h').write_text('''#include <stdint.h>
typedef int16_t ogg_int16_t; typedef uint16_t ogg_uint16_t;
typedef int32_t ogg_int32_t; typedef uint32_t ogg_uint32_t;
typedef int64_t ogg_int64_t; typedef uint64_t ogg_uint64_t;
''')
    for name in ('ivorbisfile.h', 'ivorbiscodec.h'):
        shutil.copyfile(src['tremor'] / name, inc / 'tremor' / name)
    # libmad's public ABI on ARM: 32-bit int/long, 64-bit long long.
    header = (src['libmad'] / 'mad.h').read_text()
    (inc / 'mad.h').write_text(header.replace('# define FPM_INTEL', '# define FPM_64BIT'))
    mixer = src['SDL_mixer']
    # Upstream 1.2.12 does not report MAD support through Mix_Init(MP3).
    # Mark this small feature-reporting correction in the redistributed source.
    path = mixer / 'mixer.c'
    text = path.read_text()
    needle = '\t\tMix_SetError("Mixer not built with MP3 support");'
    text = text.replace('#else\n' + needle,
                        '#elif defined(MP3_MAD_MUSIC)\n'
                        '        /* Miyoo build: MAD has no dynamic initialization. */\n'
                        '        result |= MIX_INIT_MP3;\n#else\n' + needle)
    path.write_text(text)
    # Tremor's ov_time_seek takes milliseconds; SDL's public API takes seconds.
    path = mixer / 'music_ogg.c'
    text = path.read_text().replace(
        'vorbis.ov_time_seek( &music->vf, (ogg_int64_t)time );',
        '/* Miyoo build: convert SDL seconds to Tremor milliseconds. */\n'
        '       vorbis.ov_time_seek( &music->vf, (ogg_int64_t)(time * 1000.0) );')
    path.write_text(text)
    common = [GCC, '-O2', '-fPIC', '-fno-strict-aliasing', '-fwrapv',
              '-mcpu=cortex-a7', '-mfpu=neon-vfpv4', '-mfloat-abi=hard', '-marm',
              f'--sysroot={SYSROOT}', '-nostdinc', '-isystem', BUILTIN,
              '-isystem', BUILTIN.parent / 'include-fixed',
              '-isystem', SYSROOT / 'usr/include', '-D_GNU_SOURCE=1',
              '-DHAVE_ALLOCA_H=1', '-I' + str(inc),
              '-I' + str(SYSROOT / 'usr/include/SDL'),
              '-I' + str(mixer), '-I' + str(src['libogg'] / 'include')]

    def objects(name, files, flags):
        dest = OUT / 'objects' / name
        dest.mkdir(parents=True, exist_ok=True)
        pairs = [(f, dest / (f.stem + '.o')) for f in files]
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            list(pool.map(lambda p: run(common + flags + ['-c', p[0], '-o', p[1]]), pairs))
        return [b for a, b in pairs]

    ogg = objects('ogg', [src['libogg'] / 'src' / (x + '.c') for x in ('bitwise', 'framing')],
                  ['-fvisibility=hidden'])
    tremor_names = 'mdct block window synthesis info floor1 floor0 vorbisfile res012 mapping0 registry codebook sharedbook'
    tremor = objects('tremor', [src['tremor'] / (x + '.c') for x in tremor_names.split()],
                     ['-fvisibility=hidden', '-D_ARM_ASSEM_', '-I' + str(src['tremor'])])
    mixer_names = ('effect_position effect_stereoreverse effects_internal load_aiff load_voc '
                   'mixer music wavestream dynamic_ogg load_ogg music_ogg music_mad')
    flags = ['-DWAV_MUSIC', '-DOGG_MUSIC', '-DMP3_MAD_MUSIC', '-DOGG_USE_TREMOR']
    objs = objects('mixer-tremor', [mixer / (x + '.c') for x in mixer_names.split()], flags)
    MIXER.parent.mkdir(exist_ok=True)
    run(common + ['-shared', '-Wl,-soname,libSDL_mixer-1.2.so.0', '-Wl,--no-undefined',
                  '-Wl,-z,relro,-z,now', '-o', MIXER] + objs + tremor + ogg +
        [SYSROOT / 'usr/lib/libSDL.so', madlib, '-lm', '-lpthread'])
    run([STRIP, '--strip-unneeded', MIXER])
    shutil.copyfile(madlib, MAD)
    print('Tremor SDL_mixer', MIXER.stat().st_size, digest(MIXER), flush=True)
    # Do not ship the SDK's roughly 9 MB of libgcc debug information.
    shutil.copyfile(SYSROOT / 'lib/libgcc_s.so.1', GCC_RUNTIME)
    run([STRIP, '--strip-unneeded', GCC_RUNTIME])
    marker.write_text(json.dumps({
        'signature': signature,
        'artifacts': {p.relative_to(OUT).as_posix(): digest(p) for p in artifacts},
        'sources': specs, 'flags': [str(x) for x in common],
        'libmad_sha256': digest(madlib),
        'mixer_sha256': digest(MIXER),
        'patches': ['Mix_Init(MIX_INIT_MP3) reports statically enabled libmad',
                    'Tremor seeking converts SDL seconds to milliseconds'],
    }, indent=2) + '\n')
    return MIXER


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--offline', action='store_true', help='use verified local source archives only')
    args = parser.parse_args()
    ensure_runtime(offline=args.offline)


if __name__ == '__main__':
    main()
