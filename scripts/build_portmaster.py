#!/usr/bin/env python3
"""Build the experimental aarch64 PortMaster runtime on Linux x86_64 or WSL."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / '.local/portmaster'
LOCK_PATH = ROOT / 'scripts/portmaster.lock.json'
LOCK = json.loads(LOCK_PATH.read_text(encoding='utf-8'))
PREFIX = LOCAL / 'prefix'
SYSROOT = LOCAL / 'sysroot'
TRIPLE = 'aarch64-none-linux-gnu'
COMPILER = LOCAL / 'gcc/bin' / (TRIPLE + '-gcc')
TARGET_CC = LOCAL / 'bin' / (TRIPLE + '-gcc')
RUNTIME_LIBS = ('libSDL-1.2.so.0', 'libSDL_mixer-1.2.so.0', 'libSDL_ttf-2.0.so.0')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def fetch(name, offline=False):
    spec = LOCK['packages'][name]
    path = LOCAL / 'cache' / spec['filename']
    if not path.is_file():
        if offline:
            raise FileNotFoundError('Missing offline download: ' + str(path))
        path.parent.mkdir(parents=True, exist_ok=True)
        print('Downloading ' + name, flush=True)
        temporary = path.with_suffix(path.suffix + '.tmp')
        with urllib.request.urlopen(spec['url'], timeout=120) as response, temporary.open('wb') as output:
            shutil.copyfileobj(response, output)
        if digest(temporary) != spec['sha256']:
            raise ValueError('SHA-256 mismatch: ' + name)
        temporary.replace(path)
    if digest(path) != spec['sha256']:
        raise ValueError('SHA-256 mismatch: ' + str(path))
    return path


def unpack(name):
    destination = LOCAL / ('gcc' if name == 'gcc' else 'src/' + name)
    marker = destination / '.archive-sha256'
    signature = LOCK['packages'][name]['sha256']
    if marker.is_file() and marker.read_text().strip() == signature:
        return destination
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(LOCAL / 'cache' / LOCK['packages'][name]['filename']) as archive:
        # All source/toolchain archives have exactly one enclosing directory.
        roots = {member.name.split('/')[0] for member in archive.getmembers()}
        if len(roots) != 1:
            raise ValueError('Unexpected archive root: ' + name)
        for member in archive.getmembers():
            member.name = member.name.partition('/')[2]
            if not member.name:
                continue
            if member.islnk():
                member.linkname = member.linkname.partition('/')[2]
            archive.extract(member, destination, filter='data')
    marker.write_text(signature + '\n')
    return destination


def run(command, cwd=ROOT, env=None):
    command = [str(value) for value in command]
    if cwd.resolve().is_relative_to((LOCAL / 'build').resolve()):
        print('PortMaster dependency: ' + cwd.name + ' / ' + Path(command[0]).name, flush=True)
        log = cwd / 'build.log'
        with log.open('a') as output:
            output.write('\n' + repr(command) + '\n')
            output.flush()
            result = subprocess.run(command, cwd=cwd, env=env, stdout=output, stderr=subprocess.STDOUT)
        if result.returncode:
            print('\n'.join(log.read_text(errors='replace').splitlines()[-35:]), flush=True)
            result.check_returncode()
    else:
        subprocess.run(command, cwd=cwd, env=env, check=True)


def build_environment():
    env = os.environ.copy()
    env.pop('OS', None)
    env.update({
        'PATH': str(COMPILER.parent) + ':' + str(PREFIX / 'bin') + ':' + env['PATH'],
        'CC': str(TARGET_CC), 'AR': str(COMPILER.with_name(TRIPLE + '-ar')),
        'RANLIB': str(COMPILER.with_name(TRIPLE + '-ranlib')),
        'STRIP': str(COMPILER.with_name(TRIPLE + '-strip')),
        'CFLAGS': '-O2 -fPIC -march=armv8-a',
        'CPPFLAGS': '-I' + str(PREFIX / 'include'),
        'LDFLAGS': '-L' + str(PREFIX / 'lib') + ' -L' + str(SYSROOT / 'usr/lib/aarch64-linux-gnu'),
        'PKG_CONFIG_LIBDIR': str(PREFIX / 'lib/pkgconfig'), 'PKG_CONFIG_PATH': '',
        'SDL_CONFIG': str(PREFIX / 'bin/sdl-config'),
        'FREETYPE_CONFIG': str(PREFIX / 'bin/freetype-config'),
        'LC_ALL': 'C',
        # make_minimal otherwise discovers the adjacent Windows .exe in WSL.
        'RICH4_MAKE': shutil.which('make') or 'make',
        'PYTHON': sys.executable,
    })
    return env


def prepare_sysroot():
    names = ('libc6', 'libc6-dev', 'linux-libc-dev')
    signature = ''.join(LOCK['packages'][name]['sha256'] for name in names)
    marker = SYSROOT / '.archive-sha256'
    if marker.is_file() and marker.read_text().strip() == signature:
        return
    SYSROOT.mkdir(parents=True, exist_ok=True)
    for name in names:
        run(['dpkg-deb', '-x', LOCAL / 'cache' / LOCK['packages'][name]['filename'], SYSROOT])
    # Debian's absolute links must refer to this sysroot, never the host /lib.
    for path in SYSROOT.rglob('*'):
        if path.is_symlink() and os.readlink(path).startswith('/'):
            target = SYSROOT / os.readlink(path).lstrip('/')
            path.unlink()
            path.symlink_to(os.path.relpath(target, path.parent))
    marker.write_text(signature + '\n')


def prepare_compiler():
    # Old SDL1 libtool drops unfamiliar linker flags. Put the sysroot in the
    # compiler command itself so EVERY compile/link/configure probe uses it.
    command = [str(COMPILER), '--sysroot=' + str(SYSROOT),
               '-B' + str(SYSROOT / 'usr/lib/aarch64-linux-gnu') + '/',
               '-nostdinc', '-isystem',
               str(LOCAL / 'gcc/lib/gcc' / TRIPLE / '10.3.1/include'),
               '-isystem', str(SYSROOT / 'usr/include/aarch64-linux-gnu'),
               '-isystem', str(SYSROOT / 'usr/include'),
               # Keep GCC's limits.h fallback after the sysroot so its fixed
               # pthread.h cannot override the older libc headers.
               '-isystem', str(LOCAL / 'gcc/lib/gcc' / TRIPLE / '10.3.1/include-fixed'),
               '-Wl,-rpath-link,' + str(SYSROOT / 'lib/aarch64-linux-gnu'),
               '-Wl,-rpath-link,' + str(SYSROOT / 'usr/lib/aarch64-linux-gnu'),
               '-Wl,-rpath-link,' + str(PREFIX / 'lib')]
    source = '#!/bin/sh\nexec ' + ' '.join(map(shlex.quote, command)) + ' "$@"\n'
    TARGET_CC.parent.mkdir(parents=True, exist_ok=True)
    if not TARGET_CC.is_file() or TARGET_CC.read_text() != source:
        TARGET_CC.write_text(source)
        TARGET_CC.chmod(0o755)


def autotools(name, options, env, jobs, prefix=PREFIX):
    build = LOCAL / 'build' / (name + ('-qa' if prefix != PREFIX else ''))
    build.mkdir(parents=True, exist_ok=True)
    configuration = {
        'source': LOCK['packages'][name]['sha256'], 'options': options,
        'flags': {key: env[key] for key in ('CC', 'CFLAGS', 'CPPFLAGS', 'LDFLAGS')},
        'sysroot': LOCK['packages']['libc6']['sha256'],
        'compiler': digest(TARGET_CC),
    }
    if env.get('LIBS'):
        configuration['libs'] = env['LIBS']
    signature = hashlib.sha256(json.dumps(configuration, sort_keys=True).encode()).hexdigest()
    marker = build / '.build-signature'
    if marker.is_file() and marker.read_text().strip() == signature:
        return
    if (build / 'Makefile').is_file():
        run(['make', 'clean'], build, env)
    run([LOCAL / 'src' / name / 'configure', '--host=' + TRIPLE,
         '--build=x86_64-linux-gnu', '--prefix=' + str(prefix), *options], build, env)
    if name == 'SDL_mixer' and '-DOGG_MUSIC' not in (build / 'Makefile').read_text():
        raise ValueError('SDL_mixer was configured without the required OGG decoder')
    if name == 'SDL2' and '#define SDL_THREAD_PTHREAD 1' not in (build / 'include/SDL_config.h').read_text():
        raise ValueError('Verification SDL2 was configured without pthread support')
    run(['make', '-j' + str(jobs)], build, env)
    run(['make', 'install'], build, env)
    marker.write_text(signature + '\n')


def build_dependencies(jobs):
    dependency_names = ('gcc', 'sdl12-compat', 'SDL2', 'SDL_mixer', 'SDL_ttf', 'freetype', 'libogg', 'libvorbis',
                        'libc6', 'libc6-dev', 'linux-libc-dev')
    signature = 'aarch64-sdl1-v6:' + hashlib.sha256(json.dumps(
        {name: LOCK['packages'][name]['sha256'] for name in dependency_names}, sort_keys=True).encode()).hexdigest()
    marker = PREFIX / '.build-signature'
    if marker.is_file() and marker.read_text().strip() == signature:
        return
    for name in dependency_names:
        if not name.startswith(('libc6', 'linux-libc')):
            unpack(name)
    prepare_sysroot()
    prepare_compiler()
    # These SDL1 releases predate aarch64. Reuse the pinned SDL2 GNU config
    # helpers; no library/game code changes are needed for the new CPU.
    for name in ('SDL_mixer', 'SDL_ttf'):
        for helper in ('config.sub', 'config.guess'):
            directory = LOCAL / 'src' / name
            if (directory / 'build-scripts').is_dir():
                directory /= 'build-scripts'
            shutil.copyfile(LOCAL / 'src/SDL2/build-scripts' / helper,
                            directory / helper)
    env = build_environment()
    build = LOCAL / 'build/sdl12-compat-focal-v2'
    build.mkdir(parents=True, exist_ok=True)
    run(['cmake', LOCAL / 'src/sdl12-compat', '-DCMAKE_SYSTEM_NAME=Linux',
         '-DCMAKE_SYSTEM_PROCESSOR=aarch64', '-DCMAKE_C_COMPILER=' + str(TARGET_CC),
         '-DCMAKE_C_FLAGS=' + env['CFLAGS'], '-DCMAKE_SHARED_LINKER_FLAGS=' + env['LDFLAGS'],
         '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_INSTALL_PREFIX=' + str(PREFIX),
         '-DCMAKE_INSTALL_LIBDIR=lib', '-DSDL12TESTS=OFF', '-DSDL12DEVEL=ON',
         '-DSDL2_INCLUDE_DIR=' + str(LOCAL / 'src/SDL2/include')], build, env)
    run(['cmake', '--build', '.', '--', '-j' + str(jobs)], build, env)
    run(['cmake', '--build', '.', '--target', 'install'], build, env)
    static = ['--disable-shared', '--enable-static']
    autotools('freetype', static + ['--enable-freetype-config', '--without-zlib',
              '--without-bzip2', '--without-png', '--without-harfbuzz', '--without-brotli'], env, jobs)
    autotools('libogg', static, env, jobs)
    autotools('libvorbis', static + ['--disable-docs', '--disable-examples', '--disable-oggtest'], env, jobs)
    shared = ['--enable-shared', '--disable-static', '--disable-sdltest']
    mixer_env = dict(env, LIBS='-lvorbis -logg -lm')
    autotools('SDL_mixer', shared + ['--disable-music-cmd', '--disable-music-mod',
              '--disable-music-mod-modplug', '--disable-music-midi', '--disable-music-mp3',
              '--disable-music-flac', '--enable-music-ogg', '--disable-music-ogg-shared'], mixer_env, jobs)
    autotools('SDL_ttf', shared, env, jobs)
    marker.write_text(signature + '\n')


def build_game(jobs, qa=False):
    build = ROOT / 'build/portmaster-aarch64'
    build.mkdir(parents=True, exist_ok=True)
    flags = '-Isrc -DRICH4_PLATFORM_HANDHELD -DRICH4_PLATFORM_PORTMASTER -D_GNU_SOURCE=1 -D_REENTRANT'
    env = build_environment()
    signature = digest(LOCK_PATH) + flags + env['CFLAGS'] + digest(TARGET_CC)
    stamp = build / '.toolchain-stamp'
    if not stamp.is_file() or stamp.read_text() != signature:
        stamp.write_text(signature)
    targets = ['sdl'] + (['build/portmaster-aarch64/libmiyoo_qa.so'] if qa else [])
    run([sys.executable, 'scripts/make_minimal.py', *targets, '--progress', '-j' + str(jobs),
         'CC=' + str(TARGET_CC), 'BUILD_DIR=build/portmaster-aarch64', 'BIN_DIR=bin/portmaster',
         'CPPFLAGS=' + flags, 'CFLAGS=' + env['CFLAGS'] + ' -g1',
         'SDL_PREFIX=' + str(PREFIX), 'MIXER_PREFIX=' + str(PREFIX), 'TTF_PREFIX=' + str(PREFIX),
         'LOCAL_TOOLCHAIN_BIN=', '.EXTRA_PREREQS=build/portmaster-aarch64/.toolchain-stamp'],
        env=env)


def verify(jobs):
    sys.path.insert(0, str(ROOT / 'packaging/portmaster'))
    from package import inspect_elfs
    report = inspect_elfs(ROOT)
    # This dummy-only SDL2 is a verification dependency and is NEVER packaged.
    qa_prefix = LOCAL / 'qa'
    autotools('SDL2', ['--enable-shared', '--disable-static', '--disable-video-x11',
              '--disable-video-wayland', '--disable-video-kmsdrm', '--disable-video-opengl',
              '--disable-video-opengles', '--disable-alsa', '--disable-pulseaudio',
              '--disable-jack', '--disable-esd', '--disable-arts', '--disable-sndio',
              '--disable-oss', '--disable-dbus', '--disable-ibus', '--disable-haptic',
              '--disable-joystick'], build_environment(), jobs, qa_prefix)
    qemu = LOCAL / 'qemu-aarch64-static'
    shutil.copyfile(LOCAL / 'cache/qemu-aarch64-static', qemu)
    qemu.chmod(0o755)
    archive_path = ROOT / 'build/portmaster/rich4.zip'
    install = tempfile.TemporaryDirectory(prefix='installed-', dir=ROOT / 'build/portmaster-aarch64')
    install_root = Path(install.name).resolve()
    if not install_root.is_relative_to((ROOT / 'build').resolve()):
        raise ValueError('Install check escaped the build root')
    with zipfile.ZipFile(archive_path) as archive:
        for name in archive.namelist():
            if not (install_root / name).resolve().is_relative_to(install_root):
                raise ValueError('Unsafe archive member: ' + name)
        archive.extractall(install_root)
    payload = install_root / 'rich4'
    binary = payload / 'rich4_sdl.aarch64'
    binary.chmod(0o755)
    if digest(binary) != report['elf']['rich4_sdl.aarch64']['sha256']:
        raise ValueError('Package binary is stale; run package-portmaster first')
    report['archive_sha256'] = digest(archive_path)
    report['tested_payload'] = 'rich4.zip, extracted binary and bundled libraries'
    command = [qemu, '-L', SYSROOT,
               '-E', 'LD_LIBRARY_PATH=' + str(payload / 'libs.aarch64') + ':' + str(qa_prefix / 'lib'),
               '-E', 'SDL_VIDEODRIVER=dummy', '-E', 'SDL_AUDIODRIVER=dummy',
               '-E', 'SDL_RENDER_DRIVER=software',
               binary, '--data-dir', ROOT / 'assets/original',
               '--music-dir', ROOT / 'assets/music', '--font', ROOT / 'assets/fonts/rich4.ttc']
    report['qemu'] = []
    if (ROOT / 'assets/original/Data.mkf').is_file():
        for option, frames in (('--smoke-frames', 4), ('--smoke-game-frames', 2400)):
            captures = ROOT / 'build/portmaster-aarch64' / option[2:]
            captures.mkdir(parents=True, exist_ok=True)
            run_directory = captures / 'userdata'
            run_directory.mkdir(exist_ok=True)
            guest_options = ['-E', 'LD_PRELOAD=' + str(ROOT / 'build/portmaster-aarch64/libmiyoo_qa.so'),
                             '-E', 'RICH4_QA_CAPTURE=' + str(frames),
                             '-E', 'RICH4_QA_OUTPUT=' + str(captures)]
            guest_command = [*command[:1], *guest_options, *command[1:], option, frames]
            result = subprocess.run([str(value) for value in guest_command],
                                    cwd=run_directory, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=180)
            log = ROOT / 'build/portmaster-aarch64' / (option[2:] + '.log')
            log.write_text(result.stdout)
            if result.returncode:
                print(result.stdout[-6000:])
                result.check_returncode()
            match = re.search(r'SDL smoke: frames=(\d+) scene=(\d+) framebuffer=([0-9a-f]+) sequence=([0-9a-f]+)',
                              result.stdout)
            if match is None or int(match[1]) != frames or int(match[2]) != (4 if 'game' in option else 0):
                raise ValueError('Unexpected smoke endpoint; see ' + str(log))
            if 'game' in option and (ROOT / 'assets/fonts/rich4.ttc').is_file():
                if 'TEXT_BACKEND name=ttf available=1' not in result.stdout:
                    raise ValueError('Game smoke did not reach a usable TTF text backend; see ' + str(log))
            report['qemu'].append({'option': option, 'frames': frames, 'exit_code': result.returncode,
                                   'log': str(log.relative_to(ROOT)),
                                   'framebuffer': match[3], 'sequence': match[4]})
    else:
        print('SKIP PortMaster real-resource smoke: user-supplied MKFs are unavailable.')
    destination = ROOT / 'build/portmaster-aarch64/validation.json'
    destination.write_text(json.dumps(report, indent=2) + '\n')
    install.cleanup()
    print('PortMaster verification: ' + str(destination))


def windows_dispatch(args):
    fetch('host-python', args.offline)
    wsl = ['wsl'] + (['--distribution', args.wsl_distro] if args.wsl_distro else [])
    linux_root = subprocess.check_output([*wsl, '--exec', 'wslpath', '-a', str(ROOT)], text=True).strip()
    host = linux_root + '/.local/portmaster/host'
    # Paths are positional shell arguments, never interpolated into shell source.
    run([*wsl, '--exec', 'sh', '-c',
         'if [ ! -x "$1/bin/python3" ]; then mkdir -p "$1" && tar -xf "$2" -C "$1" --strip-components=1; fi',
         'bootstrap-python', host, linux_root + '/.local/portmaster/cache/host-python.tar.gz'])
    forwarded = ['--offline', '--jobs', str(args.jobs)]
    if args.dependencies_only:
        forwarded.append('--dependencies-only')
    if args.verify:
        forwarded.append('--verify')
    run([*wsl, '--exec', host + '/bin/python3', linux_root + '/scripts/build_portmaster.py', *forwarded])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--download-only', action='store_true')
    parser.add_argument('--dependencies-only', action='store_true')
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 2, 8))
    parser.add_argument('--wsl-distro', default=os.environ.get('RICH4_PORTMASTER_WSL'))
    args = parser.parse_args()
    if sys.version_info < (3, 12):
        parser.error('Python 3.12 or newer is required.')
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    try:
        names = ['gcc', 'sdl12-compat', 'SDL2', 'SDL_mixer', 'SDL_ttf', 'freetype', 'libogg', 'libvorbis']
        if args.verify:
            names.append('qemu')
        names.extend(['libc6', 'libc6-dev', 'linux-libc-dev', 'pm-builder', 'pm-util'])
        for name in names:
            fetch(name, args.offline)
        if args.download_only:
            if os.name == 'nt':
                fetch('host-python', args.offline)
            return
        if os.name == 'nt':
            windows_dispatch(args)
            return
        if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'amd64'):
            parser.error('This pinned toolchain requires Linux x86_64 (or Windows with WSL).')
        build_dependencies(args.jobs)
        if not args.dependencies_only:
            build_game(args.jobs, args.verify)
        if args.verify:
            verify(args.jobs)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, 'PortMaster build failed: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
