"""Exercise macOS dependency setup without Xcode or network access."""
from contextlib import redirect_stderr, redirect_stdout
import hashlib
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
import macos


class MacosToolchainTests(unittest.TestCase):
    def setUp(self):
        scratch = tempfile.TemporaryDirectory()
        self.addCleanup(scratch.cleanup)
        self.local = Path(scratch.name) / 'local'
        self.prefix = self.local / 'installed prefix'
        (self.prefix / 'bin').mkdir(parents=True)
        self.sdk = str(self.local / 'Xcode SDKs/MacOSX.sdk')
        packages = []
        for name in ('cmake', 'SDL2', 'SDL_mixer', 'SDL_ttf'):
            archive = self.local / 'downloads' / f'{name}.tar.gz'
            archive.parent.mkdir(exist_ok=True)
            archive.write_bytes(name.encode())
            checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
            destination = self.local / 'sources' / checksum
            source = destination / name
            source.mkdir(parents=True)
            (source / 'LICENSE').write_text('fixture license')
            (destination / '.source-complete').write_text(checksum)
            packages.append(dict(name=name, archive=archive.name, sha256=checksum,
                                 patches=[], cmake=[], configure=[]))
        for mock in (
                patch.object(macos, 'LOCAL', self.local),
                patch.object(macos, 'settings', return_value=({'packages': packages}, 'fixture', self.prefix)),
                patch.object(macos, 'capture', side_effect=self.capture),
                patch.object(macos.platform, 'machine', return_value='arm64'),
                patch.object(macos.os, 'sysconf', return_value=262144, create=True),
                patch.dict(macos.os.environ, SDKROOT='/wrong/inherited/sdk')):
            mock.start()
            self.addCleanup(mock.stop)

    def capture(self, args):
        self.assertIn('macosx', args)
        if '--show-sdk-path' in args:
            return self.sdk
        self.assertIn('clang', args)
        return '/Xcode/usr/bin/clang'

    def test_cmake_and_autoconf_use_the_selected_macos_sdk(self):
        calls = []

        def record(args, **kwargs):
            calls.append(([str(arg) for arg in args], dict(kwargs['env'])))

        with patch.object(macos, 'run', side_effect=record), redirect_stdout(io.StringIO()):
            self.assertEqual(macos.setup(offline=True), self.prefix)
        self.assertTrue((self.prefix / '.complete').is_file())
        for args, env in calls:
            self.assertEqual(env['SDKROOT'], self.sdk, args)
        cmake = [args for args, _ in calls if '-S' in args]
        self.assertEqual(len(cmake), 1)
        self.assertIn('-DCMAKE_OSX_SYSROOT=' + self.sdk, cmake[0])
        configure = [args for args, _ in calls if Path(args[0]).name == 'configure']
        self.assertEqual(len(configure), 2)

    def test_configure_failure_reports_compiler_error_and_keeps_install_incomplete(self):
        diagnostic = 'ld: library not found for -liconv\n'
        failure = subprocess.CalledProcessError(77, ['configure'])
        log = None

        def fail_configure(args, **kwargs):
            nonlocal log
            if Path(args[0]).name == 'configure':
                log = kwargs['cwd'] / 'config.log'
                log.write_text(diagnostic)
                raise failure

        errors = io.StringIO()
        with patch.object(macos, 'run', side_effect=fail_configure), \
                redirect_stdout(io.StringIO()), redirect_stderr(errors):
            with self.assertRaises(subprocess.CalledProcessError) as caught:
                macos.setup(offline=True)
        self.assertIs(caught.exception, failure)
        self.assertIn(diagnostic, errors.getvalue())
        self.assertIn(str(log), errors.getvalue())
        self.assertFalse((self.prefix / '.complete').exists())
        self.assertFalse((log.parent / '.complete').exists())


if __name__ == '__main__':
    unittest.main()
