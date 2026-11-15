#!/usr/bin/env python3
"""Protect user-data isolation, ABI declarations and PortMaster launcher cleanup."""
import importlib.util
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('rich4_portmaster_package', ROOT / 'packaging/portmaster/package.py')
pm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pm)


def elf_fixture(machine=183, glibc='2.17', dependency='libc.so.6'):
    names = b'\0.shstrtab\0.dynstr\0.dynamic\0'
    strings = b'\0' + dependency.encode() + b'\0GLIBC_' + glibc.encode() + b'\0'
    dynamic = struct.pack('<QQQQ', 1, 1, 0, 0)
    offset = 64 + 4 * 64
    sections = [b'\0' * 64]
    for name, kind, payload in ((b'.shstrtab', 3, names), (b'.dynstr', 3, strings), (b'.dynamic', 6, dynamic)):
        sections.append(struct.pack('<IIQQQQIIQQ', names.index(name), kind, 0, 0,
                                    offset, len(payload), 0, 0, 1, 0))
        offset += len(payload)
    header = struct.pack('<16sHHIQQQIHHHHHH', b'\x7fELF\x02\x01' + b'\0' * 10,
                         3, machine, 1, 0, 0, 64, 0, 64, 56, 0, 64, 4, 1)
    return header + b''.join(sections) + names + strings + dynamic


class PortMasterPackageTests(unittest.TestCase):
    def test_abi_rejects_wrong_arch_new_glibc_and_accidental_sdl2_dependency(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / 'runtime'
            binary.write_bytes(elf_fixture())
            self.assertEqual(pm.elf_info(binary)['glibc_versions'], ['2.17'])
            for options in ({'machine': 62}, {'glibc': '2.34'}, {'dependency': 'libSDL2-2.0.so.0'}):
                with self.subTest(options=options), self.assertRaises(ValueError):
                    binary.write_bytes(elf_fixture(**options))
                    pm.elf_info(binary)

    def test_payload_and_corresponding_source_exclude_private_resources(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'project'
            shutil.copytree(ROOT / 'packaging/portmaster', root / 'packaging/portmaster',
                            ignore=shutil.ignore_patterns('__pycache__'))
            for name in ('Makefile', 'LICENSE', 'README.md', 'RICH4-SDL.CFG.example'):
                shutil.copyfile(ROOT / name, root / name)
            (root / 'scripts').mkdir()
            shutil.copyfile(ROOT / 'scripts/portmaster.lock.json', root / 'scripts/portmaster.lock.json')
            for name in pm.RUNTIME_LIBS:
                path = root / '.local/portmaster/prefix/lib' / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(elf_fixture())
            binary = root / 'bin/portmaster/rich4_sdl'
            binary.parent.mkdir(parents=True)
            binary.write_bytes(elf_fixture())
            for component, name in pm.LIB_LICENSES.items():
                path = root / '.local/portmaster/src' / component / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('Dependency license')
            source = root / '.local/portmaster/cache/SDL_mixer.tar.gz'
            source.parent.mkdir(parents=True)
            source.write_bytes(b'upstream SDL_mixer source archive')
            for name in ('assets/original/Data.mkf', 'assets/fonts/rich4.ttc',
                         'assets/music/track10.ogg', 'saves/SAVE0.R4S', 'RICH4.CFG',
                         'RICH4-SDL.CFG', 'build/stale-package/private.mkf'):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'private-user-content')
            (root / 'src').mkdir()
            (root / 'src/main.c').write_text('/* corresponding engine source */')
            stage = root / 'build/staging'
            port = pm.stage_payload(root, stage)
            for path in port.rglob('*'):
                if path.is_file():
                    self.assertNotIn(b'private-user-content', path.read_bytes(), str(path))
            with tarfile.open(port / 'rich4/sources/rich4-sdl-source.tar.gz') as archive:
                self.assertIn('rich4-sdl/src/main.c', archive.getnames())
                self.assertFalse(any('/assets/' in name or '/.local/' in name or '/build/' in name
                                     for name in archive.getnames()))
            self.assertNotIn(b'\r', (port / 'Rich4 SDL.sh').read_bytes())

    @unittest.skipIf(os.name == 'nt', 'Bash launch simulation runs in the Linux/WSL check')
    def test_launcher_checks_requirements_and_retains_exit_status_with_cleanup(self):
        for architecture, font, data, expected in (('armhf', False, False, 1),
                                                  ('aarch64', False, False, 1),
                                                  ('aarch64', True, False, 1),
                                                  ('aarch64', True, True, 7)):
            with self.subTest(architecture=architecture, font=font, data=data):
                with tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    game = root / 'ports/rich4'
                    game.mkdir(parents=True)
                    trace = root / 'trace'
                    binary = game / 'rich4_sdl.aarch64'
                    binary.write_text('#!/bin/sh\necho "game:$*" >> "$TEST_TRACE"\nexit 7\n')
                    if font:
                        path = game / 'assets/fonts/rich4.ttc'
                        path.parent.mkdir(parents=True)
                        path.write_bytes(b'user font')
                    if data:
                        path = game / 'assets/original/DATA.MKF'
                        path.parent.mkdir(parents=True)
                        path.write_bytes(b'user data')
                    control = root / 'PortMaster/control.txt'
                    control.parent.mkdir()
                    control.write_text('CFW_NAME=rich4_unit_test\n'
                        'get_controls() { directory="${TEST_ROOT#/}"; DEVICE_ARCH="$TEST_ARCH"; }\n'
                        'pm_show_error() { echo "error:$*" >> "$TEST_TRACE"; }\n'
                        'pm_finish() { echo finish >> "$TEST_TRACE"; }\n'
                        'pm_platform_helper() { echo platform >> "$TEST_TRACE"; }\n'
                        'fake_pad() { echo pad >> "$TEST_TRACE"; }\nGPTOKEYB=fake_pad\n')
                    script = (ROOT / 'packaging/portmaster/Rich4 SDL.sh').read_text()
                    launcher = game.parent / 'Rich4 SDL.sh'
                    launcher.write_text(script)
                    env = dict(os.environ, TEST_CONTROL=str(control), TEST_ROOT=str(root),
                               TEST_TRACE=str(trace), TEST_ARCH=architecture,
                               controlfolder=str(control.parent),
                               XDG_DATA_HOME=str(root / 'xdg'))
                    result = subprocess.run(['bash', str(launcher)], env=env,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                            text=True, timeout=10)
                    self.assertEqual(result.returncode, expected, result.stdout)
                    events = trace.read_text()
                    self.assertIn('finish', events)
                    self.assertIn('error:', events)
                    log = (game / 'log.txt').read_text()
                    self.assertIn('Rich4 startup:', log)
                    self.assertEqual('game:' in events, expected == 7)
                    if expected == 7:
                        self.assertIn('--data-dir ./assets/original', events)
                        self.assertIn('platform', events)
                        self.assertIn('Rich4 exit status: 7', log)


if __name__ == '__main__':
    unittest.main()
