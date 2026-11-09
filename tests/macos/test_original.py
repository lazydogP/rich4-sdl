"""Integration checks using only locally owned original files and their copies."""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import wave
import re
import runpy

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'build/macos/universal'
NATIVE = BUILD / 'native'
SOURCE = Path(os.environ.get('RICH4_ORIGINAL_DIR', ROOT / 'assets/original/V3.11')).resolve()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def probe(*args, success=True):
    p = subprocess.run([str(NATIVE / 'import-probe'), *map(str, args)], capture_output=True, text=True, timeout=90)
    if success:
        assert p.returncode == 0, p.stderr
        return json.loads(p.stdout)
    assert p.returncode != 0
    return p.stderr


def render(source, output, cache):
    p = subprocess.run([str(NATIVE / 'rich4-midi'), str(source), str(output), '--cache-dir', str(cache)],
                       capture_output=True, text=True, timeout=90)
    assert p.returncode == 0, p.stderr
    return json.loads(p.stdout.splitlines()[-1])


def bundled_smoke(installation, home, report):
    baseline = runpy.run_path(str(ROOT / 'tests/sdl/test_runtime_smoke.py'))
    original = BUILD / 'Rich4.app'
    if not original.is_dir():
        report['checks'].append('relocated bundle smoke: NOT EXECUTED (bundle not built)')
        return
    moved = home / '移动后的 应用' / 'Rich4.app'
    moved.parent.mkdir()
    shutil.copytree(original, moved)
    font = probe('font', home)
    executable = moved / 'Contents/MacOS/rich4-game'
    env = os.environ.copy()
    for name in ('DYLD_LIBRARY_PATH', 'DYLD_FALLBACK_LIBRARY_PATH', 'R4DIFF_LINK',
                 'RICH4_TEXT_BACKEND', 'RICH4_PROFILE', 'RICH4_PROFILE_LOG', 'RICH4_UI_QA_BMP'):
        env.pop(name, None)
    env.update(PATH='/usr/bin:/bin:/usr/sbin:/sbin', SDL_VIDEODRIVER='dummy',
               SDL_AUDIODRIVER='dummy', SDL12COMPAT_HIGHDPI='0', DYLD_PRINT_LIBRARIES='1')
    summaries = []
    smoke_work = home / 'smoke-working'
    smoke_work.mkdir()
    for option, frames in (('--smoke-frames', 2), ('--smoke-game-frames', 120)):
        runs = []
        for iteration in range(2):
            p = subprocess.run([str(executable), '--data-dir', str(installation / 'original'),
                                '--music-dir', str(installation / 'music'), '--font', font['path'],
                                '--font-face-index', str(font['index']), '--input-config',
                                str(ROOT / 'packaging/macos/input.toml'), option, str(frames)],
                               cwd=smoke_work, env=env, capture_output=True, text=True, timeout=45)
            assert p.returncode == 0, p.stdout + p.stderr
            loaded = '\n'.join(line for line in p.stderr.splitlines() if line.startswith('dyld['))
            assert str(moved / 'Contents/Frameworks/libSDL2-2.0.0.dylib') in loaded, loaded
            assert '/opt/homebrew/' not in loaded and '/.local/' not in loaded, loaded
            # Menu artwork can complete this bounded run before text initializes.
            assert 'TEXT_BACKEND name=ttf available=0' not in p.stderr, p.stderr
            matches = re.findall(r'SDL smoke: frames=(\d+) scene=(\d+) framebuffer=([0-9a-f]+) sequence=([0-9a-f]+)', p.stdout)
            assert matches and int(matches[-1][0]) == frames, p.stdout + p.stderr
            runs.append(matches[-1])
        assert runs[0] == runs[1], runs
        key = (option == '--smoke-game-frames', frames)
        if SOURCE == (ROOT / 'assets/original/V3.11').resolve():
            assert runs[0][2] == baseline['EXPECTED_FRAMEBUFFER'][key], 'Framebuffer changed; font-dependent differences require review'
            assert runs[0][3] == baseline['EXPECTED_SEQUENCE'][key], 'Frame sequence changed; review required'
        summaries.append(dict(zip(('frames', 'scene', 'framebuffer', 'sequence'), runs[0])))
    report['relocated_bundle_smoke'] = summaries
    report['checks'].append('relocated self-contained app: menu/game 2x deterministic smoke, bundled dlopen SDL2, no Homebrew/DYLD overrides, source removed')
    if SOURCE == (ROOT / 'assets/original/V3.11').resolve():
        report['checks'].append('reference resource fixture: unchanged golden digests')


def main():
    if not SOURCE.is_dir():
        print('macOS original-resource acceptance: NOT EXECUTED (original resources absent)')
        return
    candidate = probe('scan', SOURCE)[0]
    assert len(candidate['files']) == 32 and not candidate['problems'], candidate
    assert 'version' not in candidate
    original_hashes = {name: digest(Path(path)) for name, path in candidate['files'].items()}
    report = {'tracks': [], 'checks': []}
    output = BUILD / 'original-validation'; output.mkdir(parents=True, exist_ok=True)
    cache = output / 'cache'
    midis = list(candidate['files'])
    midis = ['Rich08.mid'] + [f'Rich{i}.mid' for i in range(16, 23)] + [f'Midi{i:02d}.mid' for i in range(1, 14)] + ['Midi14-1.mid', 'Midi14-2.mid', 'Midi15.mid', 'Midi16.mid']
    for number, name in enumerate(midis, 2):
        wav = output / f'track{number:02d}.wav'
        status = render(Path(candidate['files'][name]), wav, cache)
        with wave.open(str(wav), 'rb') as audio:
            assert (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) == (2, 2, 44100)
            assert audio.getnframes() == status['frames']
            assert abs(audio.getnframes() / 44100 - status['seconds']) <= 0.5 / 44100 + 1e-9
            assert status['peak'] > 0 and status['rms'] > 0, name
            assert any(audio.readframes(audio.getnframes())), f'{name}: silent PCM payload'
        status['source'] = name
        report['tracks'].append(status)
    first = output / 'track02.wav'
    old = digest(first)
    reused = render(Path(candidate['files']['Rich08.mid']), first, cache)
    assert reused['cached'] and digest(first) == old
    report['checks'].append('all 25 tracks: PCM format, non-silent, frame count, sequence duration; cache reuse')
    # Send a cancellation at the first real render progress event. The previous WAV must survive.
    p = subprocess.Popen([str(NATIVE / 'rich4-midi'), candidate['files']['Rich08.mid'], str(first)],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    assert p.stdout.readline()
    p.send_signal(signal.SIGTERM)
    stdout, stderr = p.communicate(timeout=30)
    assert p.returncode == 130 and digest(first) == old, (p.returncode, stdout, stderr)
    assert not list(output.glob('*.tmp'))
    report['checks'].append('cancelled rendering preserves previous WAV and removes temporary file')
    from test_music import validate_music
    report['midi_playback'] = validate_music(ROOT, SOURCE, output)
    with tempfile.TemporaryDirectory(prefix='真实资源 test ', dir=output) as scratch:
        scratch = Path(scratch); source = scratch / '原版 目录'; source.mkdir()
        for name, path in candidate['files'].items():
            shutil.copy2(path, source / name)
        copied = probe('scan', source)[0]
        assert not copied['problems']
        # Two candidate roots; no cross-directory merging.
        other = scratch / '另一目录'; shutil.copytree(source, other)
        assert len(probe('scan', scratch)) == 2
        shutil.rmtree(other)
        (source / 'Midi01.mid').unlink()
        assert 'Midi01.mid' in ' '.join(probe('scan', source)[0]['problems'])
        shutil.copy2(candidate['files']['Midi01.mid'], source / 'Midi01.mid')
        # Original bytes copied under a case-only alternate filename. APFS may be case insensitive.
        alternate = source / 'DATA.MKF'
        if not alternate.exists():
            shutil.copy2(source / 'Data.mkf', alternate)
            assert '大小写冲突' in ' '.join(probe('scan', source)[0]['problems'])
            alternate.unlink()
            report['checks'].append('case-only filename collision rejected')
        else:
            report['checks'].append('case-only collision: NOT EXECUTED (case-insensitive filesystem)')
        home = scratch / '用户 数据'
        probe('import', source, home, '-')
        first_install = probe('current', home)
        assert first_install and first_install['manifest']['music_banks'] == ['MultiverseJourney']
        assert 'version' not in first_install['manifest']
        assert len(list((Path(first_install['path']) / 'music').rglob('*.mid'))) == 25
        assert not list((Path(first_install['path']) / 'music').rglob('*.wav'))
        assert len(first_install['manifest']['files']) == 57
        assert not list(Path(first_install['path']).rglob('*.exe'))
        # Preserve real pre-existing config/save bytes (no invented game fixtures).
        preserved = {}
        shutil.copy2(ROOT / 'RICH4-SDL.CFG.example', home / 'RICH4-SDL.CFG')
        preserved['RICH4-SDL.CFG'] = digest(home / 'RICH4-SDL.CFG')
        existing = Path(os.environ.get('RICH4_EXISTING_USER_DIR', Path.home() / 'Library/Application Support/Rich4'))
        for name in ('RICH4.CFG', 'RICH4-SDL.CFG'):
            original = existing / name
            if original.is_file():
                shutil.copy2(original, home / name); preserved[name] = digest(home / name)
        saves = existing / 'saves'
        if saves.is_dir():
            shutil.copytree(saves, home / 'saves')
            preserved.update((str(p.relative_to(home)), digest(p)) for p in (home / 'saves').rglob('*') if p.is_file())
        pointer_before = (home / 'current.json').read_bytes()
        # Launch with an unavailable helper: a real import failure must preserve the active installation.
        probe('import', source, home, scratch / 'missing-helper', success=False)
        assert (home / 'current.json').read_bytes() == pointer_before
        probe('import-cancel', source, home, '-', success=False)
        assert (home / 'current.json').read_bytes() == pointer_before
        assert not list(home.glob('.import-*'))
        # Explicit offline conversion remains available and readable, while
        # the launcher/default import uses the native MIDI files.
        probe('import', source, home, NATIVE / 'rich4-midi')
        legacy = probe('current', home)
        assert len(list((Path(legacy['path']) / 'music').rglob('*.wav'))) == 25
        probe('import', source, home, '-')
        current = probe('current', home)
        assert current['path'] != first_install['path']
        for name, hash_value in preserved.items():
            assert digest(home / name) == hash_value
        shutil.rmtree(source)
        assert probe('current', home)
        for relative, expected in current['manifest']['files'].items():
            assert digest(Path(current['path']) / relative) == expected
        bundled_smoke(Path(current['path']), home, report)
        report['checks'].append('Chinese/space paths, multiple roots, missing files, failed import rollback, reimport, source removal, manifest hashes')
    assert original_hashes == {name: digest(Path(path)) for name, path in candidate['files'].items()}
    report['font'] = probe('font', output)
    assert report['font']
    font_probe = NATIVE / 'font-probe'
    if font_probe.exists():
        import sys
        sys.path.insert(0, str(ROOT / 'tools'))
        from audit_original_text import source_literals
        chars = set()
        for path in (ROOT / 'src').rglob('*.c'):
            for _, _, raw in source_literals(path):
                if not any(c >= 128 for c in raw):
                    continue
                try:
                    text = raw.decode('cp950')
                except UnicodeDecodeError:
                    continue
                chars.update(ord(c) for c in text if 0x2e80 <= ord(c) <= 0xffff)
        corpus = BUILD / 'game-text-codepoints.txt'
        corpus.write_text('\n'.join(f'{c:04x}' for c in sorted(chars)))
        font = report['font']
        subprocess.run([str(font_probe), font['path'], str(font['index']), font['postscript'], str(corpus)], check=True)
        report['checks'].append(f'CoreText/FreeType face identity, TTF_OpenFontIndex and {len(chars)} real text glyphs')
    report['not_executed'] = ['human listening and bank identity', 'disk full/unreadable source', 'clean OS X 10.10 Intel, macOS 11 arm64 and current system Finder acceptance', 'Retina/external display pointer positioning', 'audio devices and gameplay/save desktop acceptance', 'Developer ID signing/notarization/stapling']
    (BUILD / 'original-validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print('macOS original-resource checks: PASS (25 tracks, cancellation, cache, import/reimport/rollback)')
    print('Desktop/listening/release acceptance: NOT EXECUTED; see build/macos/universal/original-validation.json')


if __name__ == '__main__':
    main()
