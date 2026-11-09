"""Check Universal 2 package validation against real Mach-O bundle copies."""
from pathlib import Path
import os
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from macos import BUILD, DEPLOYMENT_TARGETS, capture, run, verify


class UniversalPackageTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix='rich4-universal-')
        self.addCleanup(self.scratch.cleanup)
        self.directory = Path(self.scratch.name).resolve()
        self.app = self.directory / 'Rich4.app'
        shutil.copytree(BUILD / 'Rich4.app', self.app)

    def thin(self, binary, arch):
        output = self.directory / f'{binary.name}-{arch}'
        run(['lipo', binary, '-thin', arch, '-output', output])
        return output

    def test_rejects_single_architecture_dlopen_library(self):
        binary = self.app / 'Contents/Frameworks/libSDL2-2.0.0.dylib'
        shutil.copy2(self.thin(binary, 'arm64'), binary)
        with self.assertRaisesRegex(RuntimeError, 'Unexpected architecture:.*libSDL2'):
            verify(self.app)

    def test_rejects_single_architecture_launcher(self):
        binary = self.app / 'Contents/MacOS/Rich4'
        shutil.copy2(self.thin(binary, 'x86_64'), binary)
        with self.assertRaisesRegex(RuntimeError, 'Unexpected architecture:.*Rich4'):
            verify(self.app)

    def test_rejects_rpath_in_x86_64_slice(self):
        binary = self.app / 'Contents/MacOS/Rich4'
        arm = self.thin(binary, 'arm64')
        intel = self.thin(binary, 'x86_64')
        run(['install_name_tool', '-rpath', '@executable_path/../Frameworks', '/tmp/rich4-external', intel])
        run(['lipo', '-create', arm, intel, '-output', binary])
        with self.assertRaisesRegex(RuntimeError, r'Unrelocated rpath:.*\(x86_64\)'):
            verify(self.app)

    def test_rejects_dependency_in_x86_64_slice(self):
        binary = self.app / 'Contents/MacOS/Rich4'
        arm = self.thin(binary, 'arm64')
        intel = self.thin(binary, 'x86_64')
        # Change an existing system framework only in the Intel slice.
        framework = next(line.strip().split(' (compatibility')[0]
                         for line in capture(['otool', '-L', intel]).splitlines()[1:]
                         if '/System/Library/Frameworks/' in line)
        run(['install_name_tool', '-change', framework, '/tmp/rich4-external.dylib', intel])
        run(['lipo', '-create', arm, intel, '-output', binary])
        with self.assertRaisesRegex(RuntimeError, r'Unrelocated dependency:.*\(x86_64\)'):
            verify(self.app)

    def test_rejects_deployment_target_in_x86_64_slice(self):
        binary = self.app / 'Contents/MacOS/Rich4'
        arm = self.thin(binary, 'arm64')
        intel = self.thin(binary, 'x86_64')
        changed = self.directory / 'intel-newer-macos'
        sdk = capture(['xcrun', '--show-sdk-version'])
        run(['xcrun', 'vtool', '-set-build-version', 'macos', '15.0', sdk,
             '-replace', '-output', changed, intel])
        run(['lipo', '-create', arm, changed, '-output', binary])
        with self.assertRaisesRegex(RuntimeError, r'Unexpected deployment target:.*\(x86_64\)'):
            verify(self.app)

    def test_rejects_deployment_target_in_arm64_slice(self):
        binary = self.app / 'Contents/MacOS/Rich4'
        arm = self.thin(binary, 'arm64')
        intel = self.thin(binary, 'x86_64')
        changed = self.directory / 'arm-newer-macos'
        sdk = capture(['xcrun', '--show-sdk-version'])
        run(['xcrun', 'vtool', '-set-build-version', 'macos', '12.0', sdk,
             '-replace', '-output', changed, arm])
        run(['lipo', '-create', changed, intel, '-output', binary])
        with self.assertRaisesRegex(RuntimeError, r'Unexpected deployment target:.*\(arm64\)'):
            verify(self.app)

    def test_rejects_bundle_minimum_above_yosemite(self):
        path = self.app / 'Contents/Info.plist'
        info = plistlib.loads(path.read_bytes())
        info['LSMinimumSystemVersion'] = '10.13'
        path.write_bytes(plistlib.dumps(info))
        with self.assertRaisesRegex(RuntimeError, 'Unexpected bundle deployment targets'):
            verify(self.app)

    def test_rejects_wrong_architecture_minimum_in_plist(self):
        path = self.app / 'Contents/Info.plist'
        info = plistlib.loads(path.read_bytes())
        info['LSMinimumSystemVersionByArchitecture'] = dict(DEPLOYMENT_TARGETS, x86_64='10.13')
        path.write_bytes(plistlib.dumps(info))
        with self.assertRaisesRegex(RuntimeError, 'Unexpected bundle deployment targets'):
            verify(self.app)

    def test_rejects_signature_without_yosemite_digest(self):
        binary = self.app / 'Contents/Frameworks/libSDL2-2.0.0.dylib'
        run(['codesign', '--force', '--sign', '-', '--digest-algorithm=sha256', binary])
        with self.assertRaisesRegex(RuntimeError, 'Missing macOS 10.10 signature digest:'):
            verify(self.app)

    def launch(self, architecture):
        env = {name: value for name, value in os.environ.items() if not name.startswith('DYLD_')}
        env.update(PATH='/usr/bin:/bin:/usr/sbin:/sbin')
        available = subprocess.run(['/usr/bin/arch', f'-{architecture}', '/usr/bin/true'],
                                   env=env, capture_output=True, timeout=15)
        if available.returncode:
            self.skipTest(f'{architecture} execution unavailable on this host')
        # SIP strips inherited DYLD_* variables from /usr/bin/arch itself.
        result = subprocess.run(['/usr/bin/arch', f'-{architecture}', '-e', 'DYLD_PRINT_LIBRARIES=1',
                                 str(self.app / 'Contents/MacOS/rich4-game'), '--help'],
                                cwd=self.directory, env=env, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('usage: rich4_sdl', result.stdout)
        loaded = '\n'.join(line for line in result.stderr.splitlines() if line.startswith('dyld['))
        self.assertIn(str(self.app / 'Contents/Frameworks/libSDL2-2.0.0.dylib'), loaded)
        self.assertNotIn('/.local/', loaded)
        self.assertNotIn('/opt/homebrew/', loaded)
        self.assertNotIn('/usr/local/', loaded)

    def test_launch_arm64(self):
        self.launch('arm64')

    def test_launch_x86_64(self):
        self.launch('x86_64')


if __name__ == '__main__':
    unittest.main()
