"""Check SDK extraction and download integrity without network or ARM tools."""
import hashlib
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
import miyoo_toolchain as paths
import setup_miyoo_toolchain as setup
import build_miyoo as build


class MiniSetupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def tar(self, entries):
        path = self.root / 'sdk.tar'
        with tarfile.open(path, 'w') as archive:
            for name, data, link in entries:
                member = tarfile.TarInfo(name)
                if link is not None:
                    member.type = tarfile.SYMTYPE
                    member.linkname = link
                    archive.addfile(member)
                else:
                    member.size = len(data)
                    archive.addfile(member, io.BytesIO(data))
        return path

    def test_sysroot_links_are_materialized_even_before_their_target(self):
        archive = self.tar([
            ('sdk/lib/libSDL.so', b'', 'libSDL.so.0'),
            ('sdk/lib/libSDL.so.0', b'', '/lib/libSDL.so.1'),
            ('sdk/lib/libSDL.so.1', b'ELF library', None),
            ('unrelated/compiler', b'not part of sysroot', None),
        ])
        out = self.root / 'sysroot'
        setup.extract_tar(archive, out, 'sdk/', absolute_links=True)
        self.assertEqual((out / 'lib/libSDL.so').read_bytes(), b'ELF library')
        self.assertFalse((out / 'lib/libSDL.so').is_symlink())
        self.assertFalse((out / 'compiler').exists())

    def test_tar_paths_and_links_cannot_escape(self):
        for entries in ([('sdk/../escape', b'bad', None)],
                        [('sdk/lib/link', b'', '../../escape')]):
            with self.subTest(entries=entries):
                archive = self.tar(entries)
                with self.assertRaises(ValueError):
                    setup.extract_tar(archive, self.root / 'out', 'sdk/')
                self.assertFalse((self.root / 'escape').exists())

    def test_sysroot_excludes_virtual_device_links(self):
        archive = self.tar([('sdk/dev/stdin', b'', '/proc/self/fd/0'),
                            ('sdk/lib/libc.so', b'libc', None)])
        out = self.root / 'out'
        setup.extract_tar(archive, out, 'sdk/', absolute_links=True, subtrees=['lib'])
        self.assertEqual((out / 'lib/libc.so').read_bytes(), b'libc')
        self.assertFalse((out / 'dev').exists())

    def test_zip_paths_cannot_escape(self):
        archive = self.root / 'bad.zip'
        with zipfile.ZipFile(archive, 'w') as output:
            output.writestr('sdk/../../escape', b'bad')
        with self.assertRaises(ValueError):
            setup.extract_zip(archive, self.root / 'out', 'sdk/')

    def test_offline_cache_requires_correct_checksum(self):
        package = {'filename': 'archive', 'url': 'https://invalid.example/archive',
                   'sha256': hashlib.sha256(b'expected').hexdigest()}
        with patch.object(setup, 'CACHE', self.root):
            with self.assertRaises(FileNotFoundError):
                setup.fetch(package, offline=True)
            archive = self.root / 'archive'
            archive.write_bytes(b'wrong')
            with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                setup.fetch(package, offline=True)
            archive.write_bytes(b'expected')
            self.assertEqual(setup.fetch(package, offline=True), archive)

    def test_corrupt_download_is_not_installed_or_cached(self):
        package = {'filename': 'archive', 'url': 'https://invalid.example/archive',
                   'sha256': hashlib.sha256(b'expected').hexdigest()}
        with patch.object(setup, 'CACHE', self.root), \
                patch.object(setup.urllib.request, 'urlopen', return_value=io.BytesIO(b'wrong')):
            with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                setup.fetch(package)
        self.assertFalse((self.root / 'archive').exists())
        self.assertFalse((self.root / 'archive.part').exists())

    def test_completed_install_is_reused_and_missing_probe_requires_repair(self):
        archive = self.root / 'sdk.zip'
        with zipfile.ZipFile(archive, 'w') as output:
            output.writestr('bin/tool.exe', b'fixture')
        package = {'filename': archive.name, 'url': 'https://invalid.example/sdk.zip',
                   'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
                   'kind': 'zip', 'probes': ['sdk/bin/tool.exe']}
        local = self.root / 'local'
        with patch.object(setup, 'LOCAL', local), patch.object(setup, 'CACHE', self.root):
            setup.install('sdk', {'sdk': package}, offline=True)
            self.assertEqual((local / 'sdk/bin/tool.exe').read_bytes(), b'fixture')
            archive.unlink()
            setup.install('sdk', {'sdk': package}, offline=True)
            (local / 'sdk/bin/tool.exe').unlink()
            with self.assertRaises(FileNotFoundError):
                setup.install('sdk', {'sdk': package}, offline=True)

    def test_toolchain_stamp_preserves_incremental_build_and_tracks_changes(self):
        manifest = self.root / 'scripts/miyoo_toolchain.lock.json'
        manifest.parent.mkdir()
        manifest.write_text('first SDK')
        with patch.object(build, 'ROOT', self.root):
            build.update_toolchain_stamp(['build', 'CFLAGS=-O2'])
            stamp = self.root / 'build/miyoo-ttf/.toolchain-stamp'
            before = stamp.stat().st_mtime_ns
            build.update_toolchain_stamp(['build', 'CFLAGS=-O2'])
            self.assertEqual(stamp.stat().st_mtime_ns, before)
            first = stamp.read_bytes()
            build.update_toolchain_stamp(['build', 'CFLAGS=-O3'])
            second = stamp.read_bytes()
            self.assertNotEqual(second, first)
            manifest.write_text('second SDK')
            build.update_toolchain_stamp(['build', 'CFLAGS=-O3'])
            self.assertNotEqual(stamp.read_bytes(), second)

    def test_every_tool_and_manifest_probe_is_inside_the_project(self):
        for path in (paths.GCC, paths.STRIP, paths.BUILTIN, paths.SYSROOT, paths.QEMU,
                     paths.SEVEN_ZIP, paths.MKE2FS, paths.FFMPEG, paths.KERNEL,
                     paths.KERNEL_DEB, paths.BUSYBOX):
            self.assertTrue(path.is_relative_to(paths.LOCAL), path)
        packages = json.loads(setup.MANIFEST.read_text())['packages']
        for package in packages.values():
            self.assertEqual(len(package['sha256']), 64)
            self.assertTrue(package['url'].startswith('https://'))
            for probe in package.get('probes', []):
                setup.child_path(paths.LOCAL, probe)


if __name__ == '__main__':
    unittest.main()
