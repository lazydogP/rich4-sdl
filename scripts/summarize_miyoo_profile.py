#!/usr/bin/env python3
"""Summarize optional runtime perf.log diagnostics; no game clock assumptions."""
import argparse
import collections
import json
from pathlib import Path
import re


def summarize(path):
    text = path.read_text(encoding='utf-8', errors='replace')
    resources = collections.defaultdict(lambda: {'reads': 0, 'bytes': 0, 'read_decode_ms': 0})
    largest = []
    repeated = collections.Counter()
    for archive, index, size, elapsed, error in re.findall(
            r'RESOURCE .*archive=(.*?) index=(\d+) bytes=(\d+) read_decode_ms=(\d+) error=(\d+)', text):
        name = archive.replace('\\', '/').rsplit('/', 1)[-1]
        stats = resources[name]
        stats['reads'] += 1
        stats['bytes'] += int(size)
        stats['read_decode_ms'] += int(elapsed)
        repeated[(name, int(index))] += 1
        largest.append({'archive': name, 'index': int(index), 'bytes': int(size),
                        'read_decode_ms': int(elapsed), 'error': int(error)})
    frames = {}
    for scene, count, maximum, values in re.findall(
            r'FRAMES scene=(\d+) count=(\d+) max_ms=(\d+) (.*)', text):
        scene = int(scene)
        count = int(count)
        stats = frames.setdefault(scene, {'count': 0, 'max_ms': 0})
        stats['count'] += count
        stats['max_ms'] = max(stats['max_ms'], int(maximum))
        for key, value in re.findall(r'(avg_\w+_ms)=([\d.]+)', values):
            stats[key] = stats.get(key, 0) + float(value) * count
    for stats in frames.values():
        for key in list(stats):
            if key.startswith('avg_'):
                stats[key] = round(stats[key] / stats['count'], 2)
    render_parts = {}
    for scene, name, count, average, maximum in re.findall(
            r'RENDER_PART scene=(\d+) name=(\S+) count=(\d+) avg_ms=([\d.]+) max_ms=(\d+)', text):
        stats = render_parts.setdefault(scene + ':' + name, {'count': 0, 'total_ms': 0, 'max_ms': 0})
        stats['count'] += int(count)
        stats['total_ms'] += int(count) * float(average)
        stats['max_ms'] = max(stats['max_ms'], int(maximum))
    for stats in render_parts.values():
        stats['avg_ms'] = round(stats.pop('total_ms') / stats['count'], 2)
    details = {}
    for scene, group, name, count, total, maximum in re.findall(
            r'DETAIL scene=(-?\d+) group=(\S+) name=(\S+) count=(\d+) total_ms=(\d+) avg_ms=[\d.]+ max_ms=(\d+)', text):
        stats = details.setdefault(scene + ':' + group + ':' + name,
                                   {'count': 0, 'total_ms': 0, 'max_ms': 0})
        stats['count'] += int(count)
        stats['total_ms'] += int(total)
        stats['max_ms'] = max(stats['max_ms'], int(maximum))
    for stats in details.values():
        stats['avg_ms'] = round(stats['total_ms'] / stats['count'], 3)
    spans = {}
    slow_spans = []
    for timestamp, group, name, elapsed, resource, size in re.findall(
            r'SPAN t_ms=(\d+) group=(\S+) name=(\S+) elapsed_ms=(\d+) resource=(-?\d+) bytes=(\d+)', text):
        key = group + ':' + name
        stats = spans.setdefault(key, {'count': 0, 'total_ms': 0, 'max_ms': 0})
        stats['count'] += 1
        stats['total_ms'] += int(elapsed)
        stats['max_ms'] = max(stats['max_ms'], int(elapsed))
        slow_spans.append({'t_ms': int(timestamp), 'group': group, 'name': name,
                          'elapsed_ms': int(elapsed), 'resource': int(resource), 'bytes': int(size)})
    for stats in spans.values():
        stats['avg_ms'] = round(stats['total_ms'] / stats['count'], 2)
    return {
        'source': str(path),
        'resource_details': 'PROFILE level=2' in text,
        'stages': [{'name': name, 'elapsed_ms': int(elapsed)} for name, elapsed in
                   re.findall(r'STAGE .*name=(\S+) elapsed_ms=(\d+)', text)],
        'archives': dict(resources), 'frames': frames,
        'render_parts': render_parts,
        'frame_details': details,
        'detail_reporting_policy': 'Five-second or scene-change aggregates; UI details are inside render, presentation details inside present_ms. Do not add them to parent totals.',
        'diagnostic_spans': spans,
        'span_reporting_policy': 'At profile 2, audio spans include every call; other spans log >=2 ms. At profile 1, spans log >=50 ms. Nested spans overlap.',
        'slowest_spans': sorted(slow_spans, key=lambda item: item['elapsed_ms'], reverse=True)[:20],
        'slowest_resources': sorted(largest, key=lambda r: r['read_decode_ms'], reverse=True)[:10],
        'repeated_reads': [{'archive': archive, 'index': index, 'reads': count}
                           for (archive, index), count in repeated.most_common() if count > 1],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--json', type=Path, dest='output')
    args = parser.parse_args()
    summary = summarize(args.log)
    if args.output:
        args.output.write_text(json.dumps(summary, indent=2), encoding='utf-8')
    for stage in summary['stages']:
        print(f"{stage['name']}: {stage['elapsed_ms']} ms")
    for archive, stats in summary['archives'].items():
        print(f"{archive}: reads={stats['reads']} bytes={stats['bytes']} read/decode={stats['read_decode_ms']} ms")
    for scene, stats in summary['frames'].items():
        print(f"scene={scene}: {stats}")
    for part, stats in summary['render_parts'].items():
        print(f"render={part}: {stats}")
    for part, stats in summary['frame_details'].items():
        print(f"detail={part}: {stats}")
    if not summary['resource_details']:
        print('Resource list includes only slow/error reads; use RICH4_PROFILE=2 for complete totals.')
    for resource in summary['slowest_resources']:
        print(f"slow resource: {resource}")
    for span in summary['slowest_spans'][:10]:
        print(f"slow span: {span}")


if __name__ == '__main__':
    main()
