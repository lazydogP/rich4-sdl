"""Check native Mini package contents using synthetic game/audio fixtures."""
import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
spec = importlib.util.spec_from_file_location('personal_mini_package', ROOT / 'packaging/miyoo/package.py')
personal = importlib.util.module_from_spec(spec)
spec.loader.exec_module(personal)
from stage_miyoo_original_assets import MKF_NAMES
from miyoo_runtime import runtime_libraries


class MiniPackageTests(unittest.TestCase):
    def test_full_package_uses_new_mixer_and_original_ogg_without_stale_wav(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = {
                'bin/miyoo/rich4_sdl': b'ARM program',
                'assets/fonts/rich4.ttc': b'font fixture',
                'assets/music/track02.ogg': b'OggS original compressed music',
                'assets/music/Original/track03.ogg': b'OggS nested music',
                'assets/music/Instok.wav': b'RIFF original UI sound',
                'build/miyoo/package/App/Rich4/assets/music/track02.wav': b'stale converted music',
                'build/miyoo/package/App/Rich4/assets/music/removed.ogg': b'stale deleted track',
                'build/miyoo/package/App/Rich4/RICH4-SDL.CFG': b'private config',
                'build/miyoo/package/App/Rich4/saves/old.sav': b'private save',
                'build/miyoo/mixer/tremor/libSDL_mixer-1.2.so.0': b'Tremor mixer',
                'build/miyoo/mixer/tremor/libmad.so.0': b'MP3 decoder',
                '.local/miyoo/sysroot/usr/lib/libSDL_mixer-1.2.so.0': b'old SDK mixer',
                'build/miyoo/mixer/tremor/libgcc_s.so.1': b'libgcc',
                'RICH4-SDL.CFG.example': b'public config example',
            }
            for name in MKF_NAMES:
                files['assets/original/' + name] = b'MKF fixture ' + name.encode()
            for name in ('libSDL_ttf-2.0.so.0', 'libpng16.so.16', 'libz.so.1', 'libfreetype.so.6'):
                files['.local/miyoo/sysroot/usr/lib/' + name] = b'SDK library ' + name.encode()
            for name, folder in (('SDL_mixer', 'SDL_mixer-1.2.12'), ('libogg', 'libogg-1.3.5'),
                                 ('tremor', 'libvorbisidec-1.2.1+git20180316'), ('libmad', 'libmad-0.15.1b')):
                files[f'.local/miyoo/mixer/src/{name}/{folder}/COPYING'] = b'upstream license'
            for rel, data in files.items():
                path = root / rel
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            output = root / 'out/full.zip'
            with patch.object(personal, 'ROOT', root), \
                    patch.object(personal, 'require_build_tools'), \
                    patch.object(personal.subprocess, 'run'), \
                    patch.object(sys, 'argv', ['package.py', '--data-dir', str(root / 'assets/original'),
                                              '--output', str(output)]), \
                    contextlib.redirect_stdout(io.StringIO()):
                personal.main()
            with zipfile.ZipFile(output) as archive:
                names = archive.namelist()
                for name in ('track02.ogg', 'Original/track03.ogg', 'Instok.wav'):
                    self.assertEqual(archive.read('App/Rich4/assets/music/' + name),
                                     files['assets/music/' + name])
                self.assertNotIn('App/Rich4/assets/music/track02.wav', names)
                self.assertNotIn('App/Rich4/assets/music/removed.ogg', names)
                self.assertNotIn('App/Rich4/RICH4-SDL.CFG', names)
                self.assertNotIn('App/Rich4/saves/old.sav', names)
                for name, source in runtime_libraries(root).items():
                    self.assertEqual(archive.read('App/Rich4/lib/' + name), source.read_bytes())
                for name in MKF_NAMES:
                    self.assertEqual(archive.read('App/Rich4/assets/original/' + name),
                                     files['assets/original/' + name])
            self.assertFalse((root / 'build/miyoo/package/App/Rich4/assets/music/track02.wav').exists())
            self.assertNotIn(b'\r', output.with_suffix('.zip.sha256').read_bytes())

if __name__ == '__main__':
    unittest.main()
