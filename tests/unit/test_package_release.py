#!/usr/bin/env python3
"""Verify public archive boundaries and launcher permissions."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from package_release import package


class ReleasePackageTests(unittest.TestCase):
    def test_packages_exclude_local_data(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            payloads = {
                'bin/rich4_sdl.exe': b'windows executable',
                'bin/miyoo/rich4_sdl': b'arm executable',
                'packaging/miyoo/launch.sh': b'#!/bin/sh\r\nexec ./rich4_sdl\r\n',
                '.local/sdl1/mingw64/bin/zlib1.dll': b'zlib',
                'assets/original/Data.mkf': b'private game data',
                'assets/fonts/rich4.ttc': b'private font',
                'assets/fonts/rich4_font.r4atlas': b'private font atlas',
                'assets/music/track.wav': b'private music',
                'saves/SAVE0.R4S': b'private save',
                'build/miyoo/package/App/Rich4/stale.mkf': b'stale private data',
                'bin/unrelated.exe': b'unrelated tool',
                'RICH4.CFG': b'private settings',
                'RICH4-SDL.CFG': b'private SDL settings',
                'build/miyoo/package/App/Rich4/RICH4-SDL.CFG': b'stale SDL settings',
                'RICH4-SDL.CFG.example': (Path(__file__).resolve().parents[2] /
                                          'RICH4-SDL.CFG.example').read_bytes(),
                'logo.png': b'unrelated full-size README logo',
                'build/miyoo/package/App/Rich4/logo.png': b'stale full-size logo',
            }
            templates = Path(__file__).resolve().parents[2] / 'packaging/miyoo'
            for name in ('config.json', 'icon.png'):
                payloads['packaging/miyoo/' + name] = (templates / name).read_bytes()
            for name in ('SDL.dll', 'SDL_ttf.dll', 'libSDL_mixer-1-2-0.dll',
                         'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll',
                         'zlib1.dll'):
                payloads['bin/' + name] = b'runtime library'
            for name, data in payloads.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            for platform in ('windows-x64', 'miyoo-mini'):
                with self.subTest(platform=platform):
                    output = package(root, root / 'out', platform, 'v1.2.3-rc.1')
                    with zipfile.ZipFile(output) as archive:
                        self.assertIsNone(archive.testzip())
                        prefix = 'App/Rich4/' if platform == 'miyoo-mini' else ''
                        self.assertEqual(archive.read(prefix + 'RICH4-SDL.CFG.example'),
                                         payloads['RICH4-SDL.CFG.example'])
                        self.assertNotIn(prefix + 'RICH4-SDL.CFG', archive.namelist())
                        for data in payloads.values():
                            if data.startswith((b'private', b'stale', b'unrelated')):
                                self.assertNotIn(data, [archive.read(n) for n in archive.namelist()])
                        if platform == 'miyoo-mini':
                            self.assertEqual(set(archive.namelist()), {
                                'App/Rich4/rich4_sdl', 'App/Rich4/launch.sh',
                                'App/Rich4/config.json', 'App/Rich4/icon.png',
                                'App/Rich4/RICH4-SDL.CFG.example',
                                'README.txt'})
                            for name in ('config.json', 'icon.png'):
                                self.assertEqual(archive.read('App/Rich4/' + name),
                                                 payloads['packaging/miyoo/' + name])
                            config = json.loads(archive.read('App/Rich4/config.json'))
                            self.assertEqual(config['label'], '大富翁4')
                            self.assertEqual(config['launch'], 'launch.sh')
                            self.assertEqual(config['icon'], 'icon.png')
                            icon = archive.read('App/Rich4/' + config['icon'])
                            self.assertEqual(icon[:8], b'\x89PNG\r\n\x1a\n')
                            self.assertEqual(struct.unpack('>II', icon[16:24]), (64, 64))
                            self.assertEqual(icon[25], 6)  # RGBA PNG
                            self.assertNotIn(b'\r', archive.read('App/Rich4/launch.sh'))
                            for name in ('App/Rich4/rich4_sdl', 'App/Rich4/launch.sh'):
                                self.assertEqual(archive.getinfo(name).external_attr >> 16 & 0o777, 0o755)
                        else:
                            self.assertIn('zlib1.dll', archive.namelist())
                            self.assertIn('rich4_sdl.exe', archive.namelist())
                        self.assertFalse(any(name.endswith('.zip') for name in archive.namelist()))
                    self.assertTrue(all(path.suffix == '.zip' for path in output.parent.iterdir()))

    def test_missing_build_fails_without_creating_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for platform in ('windows-x64', 'miyoo-mini'):
                with self.assertRaises(FileNotFoundError):
                    package(root, root / 'out', platform, 'v1.0.0')
            self.assertFalse((root / 'out').exists())

    def test_version_cannot_escape_output_directory(self):
        for version in ('../escape', '/absolute', '', 'v1/other', 'v1\\other'):
            with self.subTest(version=version), self.assertRaises(ValueError):
                package(Path('.'), Path('.'), 'miyoo-mini', version)


if __name__ == '__main__':
    unittest.main()
