#!/usr/bin/env python3
"""Compare completed atlas/TTF ARM runs; report CPU probes separately from wall time."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import tarfile
import zipfile


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8'))


def digest(directory):
    text = (directory / 'console.log').read_text(errors='replace')
    match = re.search(r'SDL smoke: frames=(\d+) scene=(\d+) framebuffer=(\w+) sequence=(\w+)', text)
    assert match, directory
    return match.groups()


def load_image(binary):
    data = bytearray(binary)
    # Stripping changes the section table fields, not the runtime segments.
    data[32:36] = b'\0' * 4
    data[48:52] = b'\0' * 4
    offset = struct.unpack_from('<I', data, 28)[0]
    size, count = struct.unpack_from('<HH', data, 42)
    result = []
    for index in range(count):
        header = struct.unpack_from('<8I', data, offset + index * size)
        if header[0] == 1:
            result.append((header[2:], data[header[1]:header[1] + header[4]]))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('atlas', type=Path)
    parser.add_argument('ttf', type=Path)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--save-benchmark', type=Path, required=True)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    va = read_json(args.atlas / 'validation.json')
    vt = read_json(args.ttf / 'validation.json')
    assert va['configured_memory_mb'] == vt['configured_memory_mb']
    assert va['swap_kb'] == vt['swap_kb'] == 0
    assert not va['oom_killed'] and not vt['oom_killed']
    for v in (va, vt):
        for key in ('scope_passed', 'flc_playback_passed', 'intro_no_skips_or_redecode',
                    'real_clock_intro_completed', 'new_game_loading_order_passed'):
            assert v[key], key
    report = {
        'environment': 'ARM Cortex-A7 QEMU, SDL dummy; not physical Miyoo timing',
        'memory_mb': va['configured_memory_mb'], 'swap_kb': 0,
        'sources': {'atlas': str(args.atlas), 'ttf': str(args.ttf), 'pixel_baseline': str(args.baseline)},
        'timing_note': 'Wall-clock draw aggregates include initial font loading. 1 ms profiling resolution. Do not infer physical-device FPS.',
        'cases': {}, 'pixel_regression': {},
    }
    for case in ('menu', 'new_game', 'game'):
        result = {}
        for mode, directory, validation in (('atlas', args.atlas, va), ('ttf', args.ttf, vt)):
            assert validation['cases'][case]['exit_status'] == 0
            performance = read_json(directory / f'performance-{case}.json')
            result[mode] = {
                'peak_rss_kb': validation['cases'][case]['peak_rss_kb'],
                'minimum_sampled_available_kb': validation['cases'][case]['minimum_sampled_available_kb'],
                'frames': performance['frames'],
                'text_draws': {k: v for k, v in performance['frame_details'].items() if ':text:' in k},
            }
        result['ttf_minus_atlas_peak_rss_kb'] = result['ttf']['peak_rss_kb'] - result['atlas']['peak_rss_kb']
        report['cases'][case] = result
        before = args.baseline / 'extracted/results' / case
        after = args.atlas / 'extracted/results' / case
        assert digest(before) == digest(after), case
        captures = list(before.glob('*.bmp'))
        assert all(p.read_bytes() == (after / p.name).read_bytes() for p in captures), case
        report['pixel_regression'][case] = {'identical_frames': int(digest(after)[0]), 'identical_captures': len(captures)}
    for mode, directory in (('atlas', args.atlas), ('ttf', args.ttf)):
        console = (directory / 'extracted/results/game/console.log').read_text(errors='replace')
        assert f'TEXT_BACKEND name={mode} available=1' in console
        serial = (directory / 'serial.log').read_text(errors='replace')
        assert 'QA_EVENT_WARMUP_PASS first_use_reads=0' in serial
    report['events'] = {
        'recent_cache_limit_bytes': 4 * 1024 * 1024, 'recent_cache_limit_entries': 64,
        'loading_warmup': {'Panel.mkf': [63, 78, 79, 80], 'Data.mkf': [553]},
        'first_use_after_warmup_reads': 0, 'scene_teardown_cached_bytes': 0,
        'note': 'Warm entries can be evicted; cold misses still read and decode synchronously. Original MKF files and decoder are unchanged.',
    }
    save = args.save_benchmark.read_text(errors='replace')
    match = re.search(r'SAVE_EQUIVALENCE_PASS writes=(\d+) exact_bytes=1 failed_write_preserves_slot=1 old_cpu_avg_ms=([\d.]+) new_cpu_avg_ms=([\d.]+)', save)
    assert match
    report['save'] = {
        'source': str(args.save_benchmark), 'writes': int(match[1]),
        'before_cpu_avg_ms': float(match[2]), 'after_cpu_avg_ms': float(match[3]),
        'exact_bytes': True, 'failed_write_preserves_slot': True,
        'fixture_file_bytes': 459340,
        'note': 'Same device save encoded by before/after implementations under ARM QEMU. Process CPU includes I/O calls; this is not SD-card fsync wall time. Flush and replacement semantics are preserved.',
    }
    with zipfile.ZipFile(args.package) as archive:
        files = {entry.filename: entry.file_size for entry in archive.infolist()}
        assert not any('/saves/' in name or name.endswith(('.R4S', '.CFG', '/debug.env')) for name in files)
        added = sum(size for name, size in files.items() if '/lib/' in name or name.endswith('/rich4.ttc'))
        library_hashes = {name: hashlib.sha256(archive.read(name)).hexdigest()
                          for name in files if '/lib/' in name}
        for directory in (args.atlas, args.ttf):
            with tarfile.open(directory / 'assets.tar') as tested:
                for name in library_hashes:
                    assert tested.extractfile('./' + name).read() == archive.read(name), name
                assert load_image(tested.extractfile('./App/Rich4/rich4_sdl').read()) == load_image(archive.read('App/Rich4/rich4_sdl'))
    report['package'] = {'path': str(args.package), 'bytes': args.package.stat().st_size,
                         'sha256': hashlib.sha256(args.package.read_bytes()).hexdigest(),
                         'added_ttf_font_and_libraries_bytes': added, 'files': files,
                         'tested_code_and_data_match_package': True,
                         'tested_library_bytes_match_package': True,
                         'library_sha256': library_hashes}
    report['physical_device_followup'] = 'Install update, collect logs/perf.log and logs/memory.log with RICH4_PROFILE=2 and RICH4_LOG_MEMORY=1. Save stage spans distinguish remaining SD sync stalls.'
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'cases': report['cases'], 'save': report['save'], 'package_bytes': report['package']['bytes']}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
