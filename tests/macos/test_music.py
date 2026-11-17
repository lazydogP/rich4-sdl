"""MIDI/OGG/WAV playback integration using only real owned music files."""
from __future__ import annotations
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def validate_music(root, source, output):
    executable = root / 'bin/macos/universal/music_probe'
    if not executable.is_file():
        raise RuntimeError('Build macos before real-resource MIDI playback validation')

    def run(*args, expected=None, env=None, success=True, timeout=45):
        p = subprocess.run([str(executable), *map(str, args)], cwd=output,
                           capture_output=True, text=True, env=env, timeout=timeout)
        if success:
            assert p.returncode == 0, p.stdout + p.stderr
            if expected:
                assert f'MUSIC type={expected}' in p.stdout, p.stdout + p.stderr
        else:
            assert p.returncode != 0, p.stdout + p.stderr
        return p.stdout + p.stderr

    capabilities = run('capabilities')
    if 'MIDI available=0' in capabilities:
        print('Original-resource MIDI playback: NOT EXECUTED (native MIDI backend not visible in current execution environment)')
        return ['native MIDI playback: NOT EXECUTED (native MIDI backend not visible in current execution environment)']
    assert 'MIDI available=1' in capabilities
    names = ['Rich08.mid'] + [f'Rich{i}.mid' for i in range(16, 23)] + [f'Midi{i:02d}.mid' for i in range(1, 14)] + ['Midi14-1.mid', 'Midi14-2.mid', 'Midi15.mid', 'Midi16.mid']
    files = {p.name.lower(): p for p in source.iterdir() if p.is_file()}
    checks = []
    with tempfile.TemporaryDirectory(prefix='MIDI 音乐 ', dir=output) as temporary:
        music = Path(temporary)
        bank = music / 'MultiverseJourney'; bank.mkdir()
        for number, name in enumerate(names, 2):
            target = (bank if number < 10 else music) / f'track{number:02d}.mid'
            shutil.copy2(files[name.lower()], target)
            if number < 10:
                run('bank', music, 1, number - 2, expected='MID')
            else:
                run('fixed', music, number - 10, expected='MID')
        checks.append('all 25 real MIDI files load/play/stop through numbered fixed and bank paths')
        ogg = root / 'assets/original/SteamV3.11/Music/track10.ogg'
        wav = output / 'track10.wav'
        if ogg.is_file() and wav.is_file():
            shutil.copy2(ogg, music / 'track10.OGG')
            shutil.copy2(wav, music / 'track10.wav')
            run('fixed', music, 0, expected='OGG')
            (music / 'track10.OGG').unlink()
            run('fixed', music, 0, expected='WAV')
            (music / 'track10.wav').unlink()
            run('fixed', music, 0, expected='MID')
            checks.append('coexisting real files: OGG > WAV > MID, including uppercase extension')
        else:
            checks.append('OGG/WAV precedence: NOT EXECUTED (real matching audio absent)')
        (music / 'track10.mid').unlink()
        shutil.copy2(files['midi01.mid'], music / 'Midi01.mid')
        run('fixed', music, 0, expected='MID')
        if ogg.is_file():
            shutil.copy2(ogg, music / 'Midi01.ogg')
            run('fixed', music, 0, expected='OGG')
            (music / 'Midi01.ogg').unlink()
        (bank / 'track02.mid').unlink()
        shutil.copy2(files['rich08.mid'], music / 'Rich08.mid')
        run('bank', music, 1, 0, expected='MID')
        run('bank', music, 0, 0, success=False)
        checks.append('original MIDI filename aliases; MultiverseJourney MIDI does not fill Original bank')
        # Exercise a real build with no MIDI backend when an older local build is available.
        for makefile in sorted((root / '.local/macos/arm64').glob('*/build/SDL_mixer/Makefile')):
            prefix = makefile.parents[2] / 'prefix'
            if (prefix / '.complete').is_file() and '-DMID_MUSIC' not in makefile.read_text():
                env = dict(os.environ, DYLD_LIBRARY_PATH=str(prefix / 'lib'))
                assert 'MIDI available=0' in run('capabilities', env=env)
                run('fixed', music, 0, env=env, success=False)
                if ogg.is_file():
                    shutil.copy2(ogg, music / 'track10.ogg')
                    run('fixed', music, 0, env=env, expected='OGG')
                checks.append('actual MIDI-disabled SDL_mixer: MID skipped; OGG still plays')
                break
        else:
            checks.append('MIDI-disabled library playback: NOT EXECUTED (local comparison build absent)')
    # The shortest real original cue is about 16 seconds. End detection proves
    # that the system MusicPlayer clock advances, beyond a successful load call.
    run('finish', files['midi03.mid'], 22000, expected='MID')
    run('loop', files['midi03.mid'], 17500, expected='MID')
    restored = run('restore', files['midi03.mid'], 12, expected='MID')
    assert 'MUSIC restored=12.000' in restored
    checks.append('real MIDI natural end, repeat beyond cue length, saved-position restore, volume API and explicit stop')
    print('Original-resource MIDI playback: PASS (25 tracks, format priority, controls)')
    return checks
