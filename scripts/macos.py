#!/usr/bin/env python3
"""Apple Silicon builds, locked dependencies, and relocatable development bundles."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT / 'scripts/macos-dependencies.lock.json'
LOCAL = ROOT / '.local/macos'
BUILD = ROOT / 'build/macos/arm64'
NATIVE = BUILD / 'native'


def run(args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, cwd=kwargs.pop('cwd', ROOT), **kwargs)


def capture(args):
    return subprocess.check_output([str(a) for a in args], text=True).strip()


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def settings():
    if sys.platform != 'darwin' or platform.machine() != 'arm64':
        raise RuntimeError('macOS 14+ on Apple Silicon is required')
    lock = json.loads(LOCK.read_text())
    for package in lock['packages']:
        for patch in package['patches']:
            if sha(ROOT / patch['file']) != patch['sha256']:
                raise ValueError(f'Patch checksum mismatch: {patch["file"]}')
    compiler = capture(['xcrun', 'clang', '--version'])
    sdk = capture(['xcrun', '--show-sdk-path'])
    recipe = Path(__file__).read_text().split('\ndef native():', 1)[0]
    fingerprint = hashlib.sha256((LOCK.read_text() + recipe + compiler + sdk + str(ROOT)).encode()).hexdigest()[:16]
    prefix = LOCAL / 'arm64' / fingerprint / 'prefix'
    return lock, fingerprint, prefix


def extract(archive, destination):
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as source:
        for member in source.getmembers():
            path = destination / member.name
            if not path.resolve().is_relative_to(destination.resolve()):
                raise ValueError(f'Unsafe archive member: {member.name}')
            if member.issym() or member.islnk():
                target = (path.parent / member.linkname) if member.issym() else (destination / member.linkname)
                if not target.resolve().is_relative_to(destination.resolve()):
                    raise ValueError(f'Unsafe archive link: {member.name}')
            elif not member.isfile() and not member.isdir():
                raise ValueError(f'Unsupported archive entry: {member.name}')
        source.extractall(destination)
    return next(p for p in destination.iterdir() if p.is_dir())


def setup(offline=False):
    lock, fingerprint, prefix = settings()
    marker = prefix / '.complete'
    if marker.is_file() and all((prefix / p).exists() for p in ('include/SDL/SDL_mixer.h', 'include/SDL/SDL_ttf.h', 'lib/libSDL.dylib', 'lib/libSDL2.dylib', 'lib/libSDL_mixer.dylib', 'lib/libSDL_ttf.dylib')):
        return prefix
    sources = {}
    for package in lock['packages']:
        archive = LOCAL / 'downloads' / package['archive']
        if not archive.is_file():
            if offline:
                raise FileNotFoundError(f'Offline cache missing: {archive}')
            archive.parent.mkdir(parents=True, exist_ok=True)
            temporary = archive.with_suffix('.part')
            print(f'Downloading {package["name"]}', flush=True)
            with urllib.request.urlopen(package['url'], timeout=120) as response, temporary.open('wb') as out:
                shutil.copyfileobj(response, out)
            if sha(temporary) != package['sha256']:
                temporary.unlink()
                raise ValueError(f'Download checksum mismatch: {package["name"]}')
            temporary.replace(archive)
        if sha(archive) != package['sha256']:
            raise ValueError(f'Cached checksum mismatch: {archive}')
        patch_key = ''.join(p['sha256'] for p in package['patches'])
        destination = LOCAL / 'sources' / (package['sha256'] + patch_key)
        if not (destination / '.source-complete').is_file():
            if destination.exists():
                shutil.rmtree(destination)
            sources[package['name']] = extract(archive, destination)
            for patch in package['patches']:
                path = ROOT / patch['file']
                if sha(path) != patch['sha256']:
                    raise ValueError(f'Patch checksum mismatch: {path}')
                run(['/usr/bin/patch', '-p1', '-i', path], cwd=sources[package['name']])
            (destination / '.source-complete').write_text(package['sha256'] + patch_key)
        else:
            sources[package['name']] = next(p for p in destination.iterdir() if p.is_dir())
    cmake = sources['cmake'] / 'CMake.app/Contents/bin/cmake'
    env = dict(os.environ)
    env.update(MACOSX_DEPLOYMENT_TARGET='14.0', CC=capture(['xcrun', '-f', 'clang']),
               CFLAGS='-O2 -arch arm64 -mmacosx-version-min=14.0',
               CPPFLAGS=f'-I{shlex.quote(str(prefix / "include"))}',
               LDFLAGS=f'-arch arm64 -mmacosx-version-min=14.0 -L{shlex.quote(str(prefix / "lib"))}',
               PKG_CONFIG='/usr/bin/false', PKG_CONFIG_PATH='', CMAKE_PREFIX_PATH=str(prefix), LIBS='-liconv')
    builds = LOCAL / 'arm64' / fingerprint / 'build'
    for package in lock['packages'][1:]:
        name = package['name']; directory = builds / name
        directory.mkdir(parents=True, exist_ok=True)
        stamp = directory / '.complete'
        if stamp.exists():
            continue
        print(f'Building {name}', flush=True)
        if name in ('SDL_mixer', 'SDL_ttf'):
            if name == 'SDL_ttf':
                # SDL_ttf 2.0.11 predates FreeType's pkg-config-only installation.
                config = prefix / 'bin/freetype-config'
                config.write_text('#!/bin/sh\nfor arg in "$@"; do\ncase "$arg" in\n'
                                  '--cflags) printf "%s\\n" ' + shlex.quote('-I' + str(prefix / 'include/freetype2')) + ';;\n'
                                  '--libs) printf "%s\\n" ' + shlex.quote('-L' + str(prefix / 'lib') + ' -lfreetype') + ';;\n'
                                  'esac\ndone\n')
                config.chmod(0o755)
                env['FREETYPE_CONFIG'] = str(config)
            args = [sources[name] / 'configure', f'--prefix={prefix}', '--host=arm-apple-darwin',
                    '--disable-static', '--enable-shared', '--disable-sdltest', f'--with-sdl-prefix={prefix}'] + package['configure']
            run(args, cwd=directory, env=env)
            run(['/usr/bin/make', '-j8'], cwd=directory, env=env)
            run(['/usr/bin/make', 'install'], cwd=directory, env=env)
        else:
            run([cmake, '-S', sources[name], '-B', directory,
                 '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_OSX_ARCHITECTURES=arm64',
                 '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
                 f'-DCMAKE_INSTALL_PREFIX={prefix}', f'-DCMAKE_PREFIX_PATH={prefix}',
                 '-DBUILD_SHARED_LIBS=ON', *package['cmake']], env=env)
            run([cmake, '--build', directory, '--parallel', '8'], env=env)
            run([cmake, '--install', directory], env=env)
        stamp.write_text(fingerprint)
    prefix.mkdir(parents=True, exist_ok=True)
    licenses = prefix / 'licenses'; licenses.mkdir(exist_ok=True)
    for name, source in sources.items():
        if name == 'cmake':
            continue
        dest = licenses / name; dest.mkdir(exist_ok=True)
        for pattern in ('COPYING*', 'LICENSE*', 'docs/FTL.TXT', 'docs/GPLv2.TXT'):
            for path in source.glob(pattern):
                if path.is_file():
                    shutil.copy2(path, dest / path.name)
        if not list(dest.iterdir()):
            raise RuntimeError(f'License missing: {name}')
    marker.write_text(fingerprint)
    return prefix


def native():
    settings(); NATIVE.mkdir(parents=True, exist_ok=True)
    flags = ['xcrun', 'clang', '-arch', 'arm64', '-mmacosx-version-min=14.0', '-fobjc-arc',
             '-O2', '-g', '-Wall', '-Wextra', '-Wno-deprecated-declarations']
    source = ROOT / 'src/platform/macos'
    version = 'apple-midi-1-' + sha(source / 'midi_render.m')[:16]
    run([*flags, f'-DRICH4_CONVERTER_VERSION="{version}"', source / 'midi_render.m',
         '-framework', 'Foundation', '-framework', 'AudioToolbox', '-o', NATIVE / 'rich4-midi'])
    run([*flags, source / 'launcher.m', source / 'import.m', '-framework', 'AppKit',
         '-framework', 'CoreText', '-o', NATIVE / 'Rich4'])
    run([*flags, ROOT / 'tests/macos/import_probe.m', source / 'import.m', '-I' + str(source),
         '-framework', 'Foundation', '-framework', 'CoreText', '-o', NATIVE / 'import-probe'])


def build():
    prefix = setup(); native()
    _, fingerprint, _ = settings()
    flags = '-O2 -g -arch arm64 -mmacosx-version-min=14.0'
    warnings = '-std=c99 -Wall -Wextra -Wno-typedef-redefinition'
    link = f'-L{shlex.quote(str(prefix / "lib"))} -Wl,-rpath,{shlex.quote(str(prefix / "lib"))}'
    build_key = hashlib.sha256((fingerprint + flags + warnings + link + 'SDL_mixer=1|SDL_ttf=1|desktop').encode()).hexdigest()[:16]
    build_dir = f'build/macos/arm64/{build_key}'
    run([sys.executable, 'scripts/make_minimal.py', 'sdl', '--progress', '-j8', 'CC=xcrun clang',
         f'BUILD_DIR={build_dir}', 'BIN_DIR=bin/macos/arm64', f'CFLAGS={flags}', f'WARNFLAGS={warnings}',
         f'SDL_CFLAGS=-I{shlex.quote(str(prefix / "include"))}',
         f'MIXER_CFLAGS=-I{shlex.quote(str(prefix / "include"))}',
         f'TTF_CFLAGS=-I{shlex.quote(str(prefix / "include"))} -DHAVE_SDL_TTF=1',
         f'SDL_LIBS={link} -lSDL',
         f'MIXER_LIBS=-L{shlex.quote(str(prefix / "lib"))} -lSDL_mixer',
         f'TTF_LIBS=-L{shlex.quote(str(prefix / "lib"))} -lSDL_ttf',
         'SDL_PREFIX=', 'MIXER_PREFIX=', 'TTF_PREFIX='])
    run(['xcrun', 'clang', '-arch', 'arm64', '-mmacosx-version-min=14.0',
         ROOT / 'tests/macos/font_probe.c', '-I' + str(prefix / 'include'),
         '-I' + str(prefix / 'include/freetype2'), '-L' + str(prefix / 'lib'),
         '-lSDL_ttf', '-lSDL', '-lfreetype', '-Wl,-rpath,' + str(prefix / 'lib'), '-o', NATIVE / 'font-probe'])


def dependencies(path):
    return [line.strip().split(' (compatibility')[0] for line in capture(['otool', '-L', path]).splitlines()[1:]]


def package():
    prefix = setup()
    app = BUILD / 'Rich4.app'
    if app.exists():
        shutil.rmtree(app)
    contents = app / 'Contents'; binaries = contents / 'MacOS'; resources = contents / 'Resources'; frameworks = contents / 'Frameworks'
    for directory in (binaries, resources, frameworks):
        directory.mkdir(parents=True)
    for src, name in ((NATIVE / 'Rich4', 'Rich4'), (NATIVE / 'rich4-midi', 'rich4-midi'), (ROOT / 'bin/macos/arm64/rich4_sdl', 'rich4-game')):
        shutil.copy2(src, binaries / name)
    # SDL2 is opened by sdl12-compat with dlopen, so include it explicitly.
    queue = list(binaries.iterdir()) + [prefix / 'lib/libSDL2-2.0.0.dylib']
    copied = set()
    while queue:
        binary = queue.pop(0)
        if binary.parent != binaries and binary.parent != frameworks:
            destination = frameworks / binary.name
            if destination.name in copied:
                continue
            copied.add(destination.name); shutil.copy2(binary.resolve(), destination); binary = destination
        for dep in dependencies(binary):
            if dep.startswith(('/System/Library/', '/usr/lib/')):
                continue
            source = prefix / 'lib' / dep.removeprefix('@rpath/') if dep.startswith('@rpath/') or '/' not in dep else Path(dep)
            if not source.is_relative_to(prefix):
                raise RuntimeError(f'Unexpected non-system dependency: {dep}')
            if source.name not in copied:
                queue.append(source)
            run(['install_name_tool', '-change', dep, '@rpath/' + source.name, binary])
        if binary.parent == frameworks:
            run(['install_name_tool', '-id', '@rpath/' + binary.name, binary])
        load = capture(['otool', '-l', binary])
        for segment in load.split('cmd LC_RPATH')[1:]:
            path = segment.split('path ', 1)[1].split(' (offset', 1)[0]
            run(['install_name_tool', '-delete_rpath', path, binary])
        # SDK-produced binaries have header padding for install_name_tool.
        run(['install_name_tool', '-add_rpath', '@executable_path/../Frameworks', binary])
    shutil.copy2(ROOT / 'packaging/macos/Info.plist', contents / 'Info.plist')
    shutil.copy2(ROOT / 'packaging/macos/input.toml', resources / 'input.toml')
    shutil.copy2(ROOT / 'RICH4-SDL.CFG.example', resources / 'RICH4-SDL.CFG.example')
    shutil.copy2(ROOT / 'LICENSE', resources / 'LICENSE')
    shutil.copytree(prefix / 'licenses', resources / 'ThirdPartyLicenses')
    shutil.copy2(LOCK, resources / LOCK.name)
    iconset = BUILD / 'Rich4.iconset'; iconset.mkdir(exist_ok=True)
    for pixels in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            suffix = '@2x' if scale == 2 else ''
            run(['sips', '-z', str(pixels * scale), str(pixels * scale), ROOT / 'logo.png', '--out', iconset / f'icon_{pixels}x{pixels}{suffix}.png'], stdout=subprocess.DEVNULL)
    run(['iconutil', '-c', 'icns', iconset, '-o', resources / 'Rich4.icns'])
    # Development only until every manual acceptance gate has passed.
    for binary in list(frameworks.iterdir()) + list(binaries.iterdir()):
        run(['codesign', '--force', '--sign', '-', binary])
    run(['codesign', '--force', '--sign', '-', app])
    verify(app)
    release = ROOT / 'build/release'; release.mkdir(parents=True, exist_ok=True)
    archive = release / 'Rich4-macOS-arm64-development.zip'
    run(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', app, archive])
    print(f'Development bundle: {app}\nManual acceptance and notarization: NOT EXECUTED', flush=True)


def verify(app):
    contents = app / 'Contents'
    for binary in list((contents / 'MacOS').iterdir()) + list((contents / 'Frameworks').iterdir()):
        architecture = capture(['lipo', '-archs', binary])
        if architecture != 'arm64':
            raise RuntimeError(f'Unexpected architecture: {binary}: {architecture}')
        for dep in dependencies(binary):
            if dep.startswith('@rpath/'):
                if not (contents / 'Frameworks' / dep.removeprefix('@rpath/')).is_file():
                    raise RuntimeError(f'Missing bundled dependency: {dep}')
            elif not dep.startswith(('/System/Library/', '/usr/lib/')):
                raise RuntimeError(f'Unrelocated dependency: {dep}')
        load = capture(['otool', '-l', binary])
        version = load.split('cmd LC_BUILD_VERSION', 1)
        if len(version) != 2 or 'minos 14.0\n' not in version[1].split('Load command', 1)[0]:
            raise RuntimeError(f'Unexpected deployment target: {binary}')
        # Check all rpaths, including any inherited from a dependency build.
        for segment in load.split('cmd LC_RPATH')[1:]:
            path = segment.split('path ', 1)[1].split(' (offset', 1)[0]
            if path != '@executable_path/../Frameworks':
                raise RuntimeError(f'Unrelocated rpath: {binary}: {path}')
    if not (contents / 'Frameworks/libSDL2-2.0.0.dylib').is_file():
        raise RuntimeError('Missing dlopen SDL2 runtime')
    forbidden = {'.mkf', '.mid', '.wav', '.ogg', '.ttf', '.ttc', '.otf', '.exe', '.avi', '.i64'}
    if any(p.suffix.lower() in forbidden for p in app.rglob('*')):
        raise RuntimeError('Personal or original media found in bundle')
    run(['codesign', '--verify', '--deep', '--strict', app])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', choices=('setup', 'native', 'build', 'package', 'verify', 'test-original'))
    parser.add_argument('--offline', action='store_true')
    args = parser.parse_args()
    try:
        if args.target == 'setup': setup(args.offline)
        elif args.target == 'native': native()
        elif args.target == 'build': build()
        elif args.target == 'package': package()
        elif args.target == 'verify': verify(BUILD / 'Rich4.app')
        else: run([sys.executable, ROOT / 'tests/macos/test_original.py'])
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'macOS build failed: {error}\n')


if __name__ == '__main__':
    main()
