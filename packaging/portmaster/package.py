#!/usr/bin/env python3
"""Stage only allowlisted runtime files and build rich4.zip with PortMaster's builder."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import struct
import sys
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from build_portmaster import LOCAL, LOCK, RUNTIME_LIBS, digest, fetch

LIB_LICENSES = {
    'sdl12-compat': 'LICENSE.txt', 'SDL_mixer': 'COPYING', 'SDL_ttf': 'COPYING',
    'freetype': 'docs/FTL.TXT', 'libogg': 'COPYING', 'libvorbis': 'COPYING',
    'SDL2': 'LICENSE.txt',
}
SYSTEM_LIBS = {'libc.so.6', 'libm.so.6', 'libdl.so.2', 'libpthread.so.0',
               'librt.so.1', 'libgcc_s.so.1'}


def elf_info(path):
    data = path.read_bytes()
    if data[:6] != b'\x7fELF\x02\x01':
        raise ValueError('Expected little-endian ELF64: ' + str(path))
    header = struct.unpack_from('<16sHHIQQQIHHHHHH', data)
    if header[2] != 183:
        raise ValueError('Expected aarch64 ELF: ' + str(path))
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, header[6] + i * header[11])
                for i in range(header[12])]
    names_section = sections[header[13]]
    names = data[names_section[4]:names_section[4] + names_section[5]]
    section_map = {names[s[0]:].split(b'\0', 1)[0].decode(): s for s in sections}
    strings_section = section_map['.dynstr']
    strings = data[strings_section[4]:strings_section[4] + strings_section[5]]
    dynamic = section_map['.dynamic']
    needed, rpaths = [], []
    for offset in range(dynamic[4], dynamic[4] + dynamic[5], 16):
        tag, value = struct.unpack_from('<QQ', data, offset)
        if tag in (1, 15, 29):
            text = strings[value:].split(b'\0', 1)[0].decode()
            (needed if tag == 1 else rpaths).append(text)
    interpreter = None
    for index in range(header[10]):
        segment = struct.unpack_from('<IIQQQQQQ', data, header[5] + index * header[9])
        if segment[0] == 3:
            interpreter = data[segment[2]:segment[2] + segment[5]].rstrip(b'\0').decode()
    versions = sorted({v.decode() for v in re.findall(rb'GLIBC_(\d+\.\d+(?:\.\d+)?)\x00', strings)},
                      key=lambda value: tuple(map(int, value.split('.'))))
    if rpaths:
        raise ValueError('Build-specific runtime search path: ' + str(path) + ' ' + str(rpaths))
    if any(tuple(map(int, value.split('.'))) > (2, 31) for value in versions):
        raise ValueError('glibc requirement exceeds advertised 2.31: ' + str(path))
    if set(needed) - SYSTEM_LIBS - set(RUNTIME_LIBS):
        raise ValueError('Unexpected runtime dependencies: ' + str(path) + ' ' + str(needed))
    if interpreter not in (None, '/lib/ld-linux-aarch64.so.1'):
        raise ValueError('Unexpected ELF interpreter: ' + str(interpreter))
    return {'sha256': hashlib.sha256(data).hexdigest(), 'machine': 'aarch64',
            'interpreter': interpreter, 'needed': needed, 'glibc_versions': versions}


def inspect_elfs(root):
    local = root / '.local/portmaster'
    paths = {'rich4_sdl.aarch64': root / 'bin/portmaster/rich4_sdl'}
    paths.update({name: local / 'prefix/lib' / name for name in RUNTIME_LIBS})
    return {'elf': {name: elf_info(path) for name, path in paths.items()}}


def source_snapshot(root, destination):
    files = [root / name for name in ('Makefile', 'LICENSE', 'README.md', 'RICH4-SDL.CFG.example')]
    for directory in ('src', 'tests', 'scripts', 'tools', 'packaging/portmaster'):
        files.extend(path for path in (root / directory).rglob('*')
                     if path.is_file() and '__pycache__' not in path.parts
                     and (path.suffix in ('.c', '.h', '.inc', '.py', '.sh', '.json', '.md', '.gptk', '.xml', '.mk', '.S')
                          or path.name == 'screenshot.png' or path.name.startswith('LICENSE')))
    with tarfile.open(destination, 'w:gz') as archive:
        for path in sorted(set(files)):
            archive.add(path, arcname='rich4-sdl/' + path.relative_to(root).as_posix(), recursive=False)


def stage_payload(root, stage):
    templates = root / 'packaging/portmaster'
    port = stage / 'ports/rich4'
    payload = port / 'rich4'
    payload.mkdir(parents=True)
    for name in ('port.json', 'README.md', 'gameinfo.xml', 'screenshot.png', 'Rich4 SDL.sh'):
        source = templates / name
        if name.endswith(('.json', '.md', '.xml', '.sh')):
            (port / name).write_bytes(source.read_bytes().replace(b'\r\n', b'\n'))
        else:
            shutil.copyfile(source, port / name)
    (port / 'Rich4 SDL.sh').chmod(0o644)
    shutil.copyfile(templates / 'rich4.gptk', payload / 'rich4.gptk')
    shutil.copyfile(root / 'RICH4-SDL.CFG.example', payload / 'RICH4-SDL.CFG.example')
    user_directories = {
        'original': 'Supply legally obtained V3.11 files here: Data.mkf, Map.mkf, Panel.mkf, Effect.mkf, Speaking.mkf, Jump.mkf, Help.mkf.\n',
        'fonts': 'Supply your own CJK TrueType/OpenType font here as rich4.ttc. See ../../rich4.md.\n',
        'music': 'Optional user-supplied OGG/WAV tracks go here. See ../../rich4.md for filenames and music banks.\n',
    }
    for name, instructions in user_directories.items():
        directory = payload / 'assets' / name
        directory.mkdir(parents=True)
        (directory / 'README.txt').write_text(instructions, encoding='utf-8')
    binary = payload / 'rich4_sdl.aarch64'
    shutil.copyfile(root / 'bin/portmaster/rich4_sdl', binary)
    binary.chmod(0o755)
    libs = payload / 'libs.aarch64'
    libs.mkdir()
    for name in RUNTIME_LIBS:
        shutil.copyfile(root / '.local/portmaster/prefix/lib' / name, libs / name)
    licenses = payload / 'licenses'
    licenses.mkdir()
    shutil.copyfile(root / 'LICENSE', licenses / 'LICENSE.rich4-sdl.txt')
    shutil.copyfile(templates / 'LICENSE.gptokeyb.txt', licenses / 'LICENSE.gptokeyb.txt')
    for name, path in LIB_LICENSES.items():
        shutil.copyfile(root / '.local/portmaster/src' / name / path,
                        licenses / ('LICENSE.' + name + '.txt'))
    # libvorbis also carries the Xiph BSD terms used by libvorbisfile.
    sources = payload / 'sources'
    sources.mkdir()
    source_snapshot(root, sources / 'rich4-sdl-source.tar.gz')
    shutil.copyfile(root / '.local/portmaster/cache/SDL_mixer.tar.gz', sources / 'SDL_mixer-1.2.12.tar.gz')
    shutil.copyfile(root / 'scripts/portmaster.lock.json', sources / 'portmaster.lock.json')
    return port


def official_builder():
    upstream = LOCAL / 'upstream'
    (upstream / 'libs').mkdir(parents=True, exist_ok=True)
    for name, relative in (('pm-builder', 'build_release.py'), ('pm-util', 'libs/util.py')):
        shutil.copyfile(fetch(name), upstream / relative)
    spec = importlib.util.spec_from_file_location('portmaster_builder', upstream / 'build_release.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def package(root=ROOT, output_dir=None):
    output_dir = output_dir or root / 'build/portmaster'
    report = inspect_elfs(root)
    output_dir.mkdir(parents=True, exist_ok=True)
    staging_parent = root / 'build/portmaster-staging'
    staging_parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rich4-', dir=staging_parent) as temporary:
        stage = Path(temporary).resolve()
        if not stage.is_relative_to((root / 'build').resolve()):
            raise ValueError('Staging directory escaped the build root')
        stage_payload(root, stage)
        (stage / 'SOURCE_SETUP.txt').write_text(
            'RELEASE_ORG="lazydogP"\nRELEASE_REPO="rich4-sdl"\n'
            'REPO_NAME="Rich4 SDL experimental"\nREPO_PREFIX="rich4"\n')
        previous = Path.cwd()
        try:
            os.chdir(stage)
            builder = official_builder()
            manifest, status = {}, {}
            builder.MESSAGES.clear()
            port = Path('ports/rich4')
            info = builder.load_port(port, manifest, {'scripts': {}, 'dirs': {}}, status)
            messages = dict(builder.MESSAGES)
            if info is None or any(value['errors'] for value in messages.values()):
                raise ValueError('Official PortMaster checks: ' + json.dumps(messages))
            builder.build_port_zip(output_dir.resolve(), port, info, manifest, status)
            report['official_checks'] = messages
        finally:
            os.chdir(previous)
    output = output_dir / 'rich4.zip'
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise ValueError('Corrupt PortMaster archive')
        if any(name.lower().endswith(('.mkf', '.ttc', '.ttf', '.ogg', '.wav', '.mid', '.avi', '.r4s'))
               for name in archive.namelist()):
            raise ValueError('User data entered the archive')
        if b'\r' in archive.read('Rich4 SDL.sh') or b'\r' in archive.read('rich4/rich4.gptk'):
            raise ValueError('CRLF in launcher or controls')
    report['archive_sha256'] = digest(output)
    report['portmaster_commit'] = LOCK['portmaster_commit']
    report['device_verified'] = False
    (output_dir / 'package-validation.json').write_text(json.dumps(report, indent=2) + '\n')
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/portmaster')
    args = parser.parse_args()
    try:
        output = package(output_dir=args.output_dir.resolve())
    except (OSError, ValueError) as error:
        parser.exit(1, 'PortMaster packaging failed: ' + str(error) + '\n')
    print('PortMaster experimental package: ' + str(output))


if __name__ == '__main__':
    main()
