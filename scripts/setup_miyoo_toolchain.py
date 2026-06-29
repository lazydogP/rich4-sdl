#!/usr/bin/env python3
"""Prepare the Windows-hosted Mini SDK entirely under .local/miyoo."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import shutil
import subprocess
import tarfile
import urllib.request
import zipfile

from miyoo_toolchain import (LOCAL, ROOT, SEVEN_ZIP, KERNEL, KERNEL_DEB, BUSYBOX,
                             require_build_tools, require_qemu_tools)

MANIFEST = ROOT / 'scripts/miyoo_toolchain.lock.json'
CACHE = LOCAL / 'cache'


def sha256(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def fetch(package, offline=False):
    path = CACHE / package['filename']
    if path.is_file():
        if sha256(path) != package['sha256']:
            raise ValueError(f'Cached download checksum mismatch: {path}; remove it and retry')
        return path
    if offline:
        raise FileNotFoundError(f'Offline cache missing: {path}')
    CACHE.mkdir(parents=True, exist_ok=True)
    partial = path.with_name(path.name + '.part')
    print(f'Downloading {package["filename"]}', flush=True)
    request = urllib.request.Request(package['url'], headers={'User-Agent': 'rich4-sdl-toolchain'})
    with urllib.request.urlopen(request, timeout=60) as source, partial.open('wb') as output:
        shutil.copyfileobj(source, output)
    if sha256(partial) != package['sha256']:
        partial.unlink()
        raise ValueError(f'Download checksum mismatch: {package["filename"]}')
    partial.replace(path)
    return path


def child_path(destination, name):
    """Reject archive paths that escape their project-local destination."""
    rel = PurePosixPath(name)
    if rel.is_absolute() or '..' in rel.parts or ':' in name or '\\' in name:
        raise ValueError(f'Unsafe archive path: {name}')
    path = destination.joinpath(*rel.parts)
    if not path.resolve().is_relative_to(destination.resolve()):
        raise ValueError(f'Archive path escapes destination: {name}')
    return path


def extract_tar(archive, destination, prefix, absolute_links=False, subtrees=()):
    """Materialize library links as files, without Windows symlink privileges."""
    links = []
    with tarfile.open(archive, mode='r|*') as source:
        for member in source:
            if not member.name.startswith(prefix):
                continue
            relative = member.name[len(prefix):]
            if subtrees and not any(relative.rstrip('/') == tree or relative.startswith(tree + '/')
                                    for tree in subtrees):
                continue
            path = child_path(destination, relative)
            if member.isdir():
                path.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                path.parent.mkdir(parents=True, exist_ok=True)
                with source.extractfile(member) as content, path.open('wb') as output:
                    shutil.copyfileobj(content, output)
            elif member.issym() or member.islnk():
                if member.linkname.startswith('/') and absolute_links:
                    target = prefix + member.linkname.lstrip('/')
                elif member.islnk():
                    target = member.linkname
                else:
                    target = posixpath.normpath(posixpath.join(posixpath.dirname(member.name), member.linkname))
                if not target.startswith(prefix):
                    raise ValueError(f'Archive link escapes tree: {member.name} -> {member.linkname}')
                links.append((path, child_path(destination, target[len(prefix):])))
    while links:
        pending = []
        for path, target in links:
            if target.is_file():
                path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(target, path)
            elif target.is_dir():
                # The SDK's optional directory aliases are not needed by GCC.
                continue
            else:
                pending.append((path, target))
        if len(pending) == len(links):
            raise ValueError(f'Unresolved archive links: {pending[:3]}')
        links = pending


def extract_zip(archive, destination, prefix=''):
    with zipfile.ZipFile(archive) as source:
        for member in source.infolist():
            if not member.filename.startswith(prefix):
                continue
            path = child_path(destination, member.filename[len(prefix):])
            if member.is_dir():
                path.mkdir(parents=True, exist_ok=True)
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                with source.open(member) as content, path.open('wb') as output:
                    shutil.copyfileobj(content, output)


def extract_deb_file(archive, destination, select):
    data = archive.read_bytes()
    if data[:8] != b'!<arch>\n':
        raise ValueError(f'Invalid Debian archive: {archive}')
    offset = 8
    while offset < len(data):
        name = data[offset:offset + 16].decode().strip().rstrip('/')
        size = int(data[offset + 48:offset + 58])
        body = data[offset + 60:offset + 60 + size]
        offset += 60 + size + size % 2
        if name.startswith('data.tar'):
            with tarfile.open(fileobj=io.BytesIO(body)) as source:
                for member in source:
                    if member.isfile() and select(member.name):
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        with source.extractfile(member) as content, destination.open('wb') as output:
                            shutil.copyfileobj(content, output)
                        return
    raise ValueError(f'Required payload missing in {archive}')


def run_extract(executable, archive, destination):
    destination.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(executable), 'x', str(archive), '-o' + str(destination), '-y'],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(result.stdout.decode(errors='replace'))


def install(name, packages, offline):
    package = packages[name]
    marker = LOCAL / '.installed' / (name + '.json')
    probes = [LOCAL / path for path in package['probes']]
    if marker.is_file() and json.loads(marker.read_text()) == package and all(p.is_file() for p in probes):
        return
    archive = fetch(package, offline)
    print(f'Preparing {name}', flush=True)
    destination = LOCAL / package.get('destination', name)
    kind = package['kind']
    if kind == 'tar':
        extract_tar(archive, destination, package['prefix'], package.get('absolute_links', False),
                    package.get('subtrees', ()))
    elif kind == 'zip':
        extract_zip(archive, destination, package.get('prefix', ''))
    elif kind == '7zip':
        run_extract(CACHE / packages['7zr']['filename'], archive, destination)
    elif kind == 'qemu':
        run_extract(SEVEN_ZIP, archive, destination)
    elif kind == 'ffmpeg':
        run_extract(SEVEN_ZIP, archive, destination)
        # Upstream wraps its binaries, license and readme in a versioned folder.
        nested = destination / package['prefix']
        shutil.copytree(nested, destination, dirs_exist_ok=True)
    elif kind == 'kernel':
        KERNEL_DEB.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(archive, KERNEL_DEB)
        extract_deb_file(archive, KERNEL, lambda name: '/boot/vmlinuz-' in name)
    elif kind == 'busybox':
        extract_deb_file(archive, BUSYBOX, lambda name: name.endswith('/bin/busybox'))
    else:
        raise ValueError(f'Unknown package kind: {kind}')
    missing = [str(p) for p in probes if not p.is_file()]
    if missing:
        raise FileNotFoundError(f'Incomplete {name} installation: {missing}')
    marker.parent.mkdir(parents=True, exist_ok=True)
    marker.write_text(json.dumps(package, indent=2), encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', action='store_true', help='also prepare ARM QEMU, kernel, BusyBox and ext4 tools')
    parser.add_argument('--package', action='store_true', help='also prepare FFmpeg for full music packages')
    parser.add_argument('--offline', action='store_true', help='use installed tools or verified project-local downloads only')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This SDK uses Windows host tools; run setup on Windows.')
    packages = json.loads(MANIFEST.read_text(encoding='utf-8'))['packages']
    try:
        for name in ('gcc', 'sysroot'):
            install(name, packages, args.offline)
        require_build_tools()
        if args.qemu or args.package:
            fetch(packages['7zr'], args.offline)
            install('7zip', packages, args.offline)
        if args.package:
            install('ffmpeg', packages, args.offline)
        if args.qemu:
            for name in ('qemu', 'platform-tools', 'kernel', 'busybox'):
                install(name, packages, args.offline)
            require_qemu_tools()
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'Mini SDK setup failed: {error}\n')
    print(f'Mini SDK ready: {LOCAL.relative_to(ROOT).as_posix()}', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
