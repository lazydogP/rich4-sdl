#!/usr/bin/env python3
"""Compare 128 MiB results, retaining the old 256 MiB run as a pixel reference.

An OOM-killed baseline has no complete frame time or peak-RSS measurement.
Keep those values unknown; never present its last memory sample as its peak.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def digest(path):
    text = path.read_text(errors='replace')
    match = re.search(r'SDL smoke: frames=(\d+) scene=(\d+) framebuffer=(\w+) sequence=(\w+)', text)
    return match.groups() if match else None


def last_load(summary):
    values = [s['elapsed_ms'] for s in summary['stages'] if s['name'] == 'assets.total']
    return values[-1] if values else None


def frame_time(stats):
    return round(sum(v for k, v in stats.items() if k.startswith('avg_')), 2)


def initial_session_cache(path):
    match = re.search(r'asset scope: phase=2 .*cache_bytes=(\d+) resources=(\d+)',
                      path.read_text(errors='replace'))
    return {'bytes': int(match[1]), 'resources': int(match[2])} if match else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before128', type=Path)
    parser.add_argument('after128', type=Path)
    parser.add_argument('--reference256', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    before = read(args.before128 / 'validation.json')
    after = read(args.after128 / 'validation.json')
    reference = read(args.reference256 / 'validation.json')
    assert before['configured_memory_mb'] == after['configured_memory_mb'] == 128
    assert reference['configured_memory_mb'] == 256
    assert before['swap_kb'] == after['swap_kb'] == reference['swap_kb'] == 0
    assert not after['oom_killed'] and not reference['oom_killed']
    report = {
        'configured_memory_mb': 128, 'swap_kb': 0,
        'before_oom_killed': before['oom_killed'], 'after_oom_killed': after['oom_killed'],
        'cases': {}, 'complete_old_reference_memory_mb': 256,
        'notes': [
            'The old 128 MiB game/new-game runs were OOM-killed. Their complete elapsed times and true peak RSS are unavailable.',
            'Completed old game measurements and pixel sequences come from the same old binary in 256 MiB QEMU; they are labelled separately.',
            'Smoke replays use a deterministic clock and do not sleep. Real-clock menu runs compare actual waiting using both binaries in the same 128 MiB guest.',
            'These are QEMU measurements, not handheld FPS or a full-game memory guarantee.'],
    }
    for case in ('menu', 'new_game', 'game'):
        a = args.reference256 / 'extracted/results' / case
        b = args.after128 / 'extracted/results' / case
        old_digest, new_digest = digest(a / 'console.log'), digest(b / 'console.log')
        assert old_digest == new_digest and old_digest is not None, case
        captures = sorted(a.glob('*.bmp'))
        for image in captures:
            assert image.read_bytes() == (b / image.name).read_bytes(), image.name
        result = after['cases'][case]
        assert result['exit_status'] == 0
        assert result['maximum_sampled_process_swap_kb'] == 0
        pa = read(args.reference256 / f'performance-{case}.json')
        pb = read(args.after128 / f'performance-{case}.json')
        report['cases'][case] = {
            'before128_exit_status': before['cases'][case]['exit_status'],
            'before128_peak_rss_kb': before['cases'][case]['peak_rss_kb'],
            'after128_exit_status': result['exit_status'],
            'after128_peak_rss_kb': result['peak_rss_kb'],
            'after128_minimum_available_kb': result['minimum_sampled_available_kb'],
            'after128_maximum_process_swap_kb': result['maximum_sampled_process_swap_kb'],
            'reference256_peak_rss_kb': reference['cases'][case]['peak_rss_kb'],
            'reference256_last_scene_load_ms': last_load(pa),
            'after128_last_scene_load_ms': last_load(pb),
            'reference256_initial_session_cache': initial_session_cache(a / 'console.log'),
            'after128_initial_session_cache': initial_session_cache(b / 'console.log'),
            'reference256_frames': pa['frames'], 'after128_frames': pb['frames'],
            'replay_frames': int(new_digest[0]), 'sequence_digest': new_digest[3],
            'identical_bmp_files': len(captures),
        }
    report['realtime_menu128'] = {}
    for name in ('before', 'after'):
        stats = read(args.after128 / f'performance-realtime_{name}.json')['frames']['0']
        report['realtime_menu128'][name] = {**stats, 'avg_total_ms': frame_time(stats)}
    report['scope128_peak_rss_kb'] = after['scope_peak_rss_kb']
    realtime_game = args.after128 / 'extracted/results/realtime_game_after'
    game_profile = read(args.after128 / 'performance-realtime_game_after.json')
    game_console = (realtime_game / 'console.log').read_text(errors='replace')
    peak = re.search(r'QA_END .*peak_rss_kb=(\d+)', game_console)
    report['realtime_game_after128'] = {
        'frames': game_profile['frames'], 'render_parts': game_profile['render_parts'],
        'peak_rss_kb': int(peak[1]) if peak else None,
    }
    report['replay_sequence_digests_identical'] = True
    report['identical_bmp_files'] = sum(c['identical_bmp_files'] for c in report['cases'].values())
    report['replay_frames'] = sum(c['replay_frames'] for c in report['cases'].values())
    report['binaries'] = {
        'before': hashlib.sha256((args.before128 / 'rich4_sdl').read_bytes()).hexdigest(),
        'after': hashlib.sha256(Path('bin/miyoo/rich4_sdl').read_bytes()).hexdigest(),
    }
    previous_comparison = Path('build/miyoo/render-comparison.json')
    assert read(previous_comparison)['binaries']['after'] == report['binaries']['before'], 'old 256 MiB reference binary differs'
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    for case, result in report['cases'].items():
        print(f"{case}: old128 exit={result['before128_exit_status']}, new128 exit=0 RSS={result['after128_peak_rss_kb']} KiB, pixels identical")
    print('real-clock menu128:', report['realtime_menu128'])


if __name__ == '__main__':
    main()
