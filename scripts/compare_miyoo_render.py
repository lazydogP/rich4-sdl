#!/usr/bin/env python3
"""Compare identical QEMU replays, frame pixels, elapsed time and RSS."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def console_digest(path):
    text = path.read_text(errors='replace')
    match = re.search(r'SDL smoke: frames=(\d+) scene=(\d+) framebuffer=(\w+) sequence=(\w+)', text)
    if not match:
        raise ValueError(f'no complete replay: {path}')
    return match.groups()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    before = json.loads((args.before / 'validation.json').read_text())
    after = json.loads((args.after / 'validation.json').read_text())
    assert before['configured_memory_mb'] == after['configured_memory_mb'] == 256
    assert before['swap_kb'] == after['swap_kb'] == 0
    assert not before['oom_killed'] and not after['oom_killed']
    report = {'memory_mb': 256, 'swap_kb': 0, 'cases': {}, 'render_parts': {}}
    for case in ('menu', 'new_game', 'game'):
        a = args.before / 'extracted/results' / case
        b = args.after / 'extracted/results' / case
        assert before['cases'][case]['exit_status'] == after['cases'][case]['exit_status'] == 0
        da, db = console_digest(a / 'console.log'), console_digest(b / 'console.log')
        assert da == db, f'full frame sequence changed: {case}: {da} != {db}'
        captures = sorted(a.glob('*.bmp'))
        for source in captures:
            # SDL writes deterministic BMPs with no timestamps; equality here
            # also validates the actual 32-bit presentation conversion.
            assert source.read_bytes() == (b / source.name).read_bytes(), f'capture changed: {case}/{source.name}'
        report['cases'][case] = {
            'frames': int(da[0]), 'sequence_digest': da[3], 'identical_captures': len(captures),
            'before_peak_rss_kb': before['cases'][case]['peak_rss_kb'],
            'after_peak_rss_kb': after['cases'][case]['peak_rss_kb'],
        }
        pa = json.loads((args.before / f'performance-{case}.json').read_text())
        pb = json.loads((args.after / f'performance-{case}.json').read_text())
        for part, stats in pa['render_parts'].items():
            new = pb['render_parts'][part]
            assert stats['count'] == new['count']
            report['render_parts'][case + ':' + part] = {
                'frames': stats['count'], 'before_avg_ms': stats['avg_ms'],
                'after_avg_ms': new['avg_ms'],
                'speedup': round(stats['avg_ms'] / new['avg_ms'], 2) if new['avg_ms'] else None,
            }
    report['binaries'] = {}
    for name, path in [('before', args.before / 'rich4_sdl'), ('after', Path('bin/miyoo/rich4_sdl'))]:
        report['binaries'][name] = hashlib.sha256(path.read_bytes()).hexdigest()
    report['replay_sequence_digests_identical'] = True
    report['no_oom'] = True
    args.output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    for name, stats in report['render_parts'].items():
        print(f"{name}: {stats['before_avg_ms']} -> {stats['after_avg_ms']} ms; speedup={stats['speedup']}")
    for name, stats in report['cases'].items():
        print(f"{name}: RSS {stats['before_peak_rss_kb']} -> {stats['after_peak_rss_kb']} KiB; frames={stats['frames']}, pixels identical")


if __name__ == '__main__':
    main()
