#!/usr/bin/env python3
"""Inventory C text literals and compare their bytes with a local Rich4.exe.

This checks transcription, not whether a caller selected the correct original
string. Near matches are investigation hints only; never automatic replacements.
Commercial executable bytes are read locally and are not embedded in this tool.
"""
from __future__ import annotations

import argparse
import difflib
import json
import re
import struct
import hashlib
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REVIEW = ROOT / 'tools/original_text_deviations.json'
UTF8_METADATA_FILES = {'src/core/ai.c', 'src/core/card.c', 'src/core/tool.c'}
TOKENS = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[^\s]', re.S)
ESCAPES = {'a': 7, 'b': 8, 'f': 12, 'n': 10, 'r': 13, 't': 9,
           'v': 11, '\\': 92, '"': 34, "'": 39, '?': 63}


def literal_bytes(token: str) -> bytes:
    content = token[1:-1]
    result = bytearray()
    pos = 0
    while pos < len(content):
        char = content[pos]
        pos += 1
        if char != '\\':
            result.extend(char.encode('utf-8'))
            continue
        char = content[pos]
        pos += 1
        if char == '\n':
            continue
        if char == 'x':
            match = re.match(r'[0-9a-fA-F]+', content[pos:])
            if not match:
                raise ValueError('empty C hex escape')
            result.append(int(match[0], 16))
            pos += len(match[0])
        elif char in '01234567':
            match = re.match(r'[0-7]{0,2}', content[pos:])
            result.append(int(char + match[0], 8))
            pos += len(match[0])
        else:
            result.append(ESCAPES[char])
    return bytes(result)


def source_literals(path: Path):
    text = path.read_text(encoding='utf-8')
    pending = []
    line = 0
    for match in TOKENS.finditer(text):
        token = match[0]
        if token.startswith(('//', '/*')):
            continue
        if token.startswith('"'):
            if not pending:
                line = text.count('\n', 0, match.start()) + 1
            pending.append(token)
        elif pending:
            yield line, ''.join(pending), b''.join(map(literal_bytes, pending))
            pending = []
    if pending:
        yield line, ''.join(pending), b''.join(map(literal_bytes, pending))


def pe_sections(data: bytes):
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('expected a PE executable')
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
    table = pe + 24 + optional_size
    for i in range(count):
        entry = table + 40 * i
        name = data[entry:entry + 8].rstrip(b'\0').decode('ascii')
        _, rva, size, offset = struct.unpack_from('<IIII', data, entry + 8)
        yield name, base + rva, data[offset:offset + size]


def decode(raw: bytes) -> str:
    return raw.decode('cp950', errors='replace')


def original_candidates(sections):
    candidates = {}
    for name, address, data in sections:
        if name not in ('DGROUP', '.data', '.rdata'):
            continue
        offset = 0
        for chunk in data.split(b'\0'):
            # Some strings follow packed data without an intervening NUL.
            for start in range(min(len(chunk), 32)):
                raw = chunk[start:]
                if not 2 <= len(raw) <= 512:
                    continue
                try:
                    text = raw.decode('cp950')
                except UnicodeDecodeError:
                    continue
                if (any('\u3400' <= c <= '\u9fff' for c in text) and
                        all(c.isprintable() or c in '\n\r\t' for c in text)):
                    candidates.setdefault(raw, address + offset + start)
                    break
            offset += len(chunk) + 1
    return candidates


def audit(executables: list[Path]):
    references = []
    for executable in executables:
        raw = executable.read_bytes()
        references.append((executable, list(pe_sections(raw))))
    primary = references[0][1]
    candidates = original_candidates(primary)
    rows = []
    source_files = 0
    all_literals = 0
    ascii_literals = 0
    for path in sorted((ROOT / 'src').rglob('*')):
        if path.suffix not in ('.c', '.h', '.inc'):
            continue
        source_files += 1
        for line, literal, raw in source_literals(path):
            all_literals += 1
            if not any(b >= 128 for b in raw):
                ascii_literals += 1
                continue
            row = {'file': path.relative_to(ROOT).as_posix(), 'line': line,
                   'hex': raw.hex(), 'text': decode(raw)}
            if any(ord(c) >= 128 for c in literal):
                row.update(status='utf8_metadata' if row['file'] in UTF8_METADATA_FILES
                           else 'utf8_source', text=raw.decode('utf-8'))
            else:
                try:
                    raw.decode('cp950')
                    row['status'] = 'unmatched'
                except UnicodeDecodeError as error:
                    row.update(status='invalid_big5', error=str(error))
                for executable, sections in references:
                    for name, address, data in sections:
                        if name not in ('DGROUP', '.data', '.rdata'):
                            continue
                        found = data.find(raw)
                        if found >= 0:
                            row.update(status='original_bytes', address=hex(address + found),
                                       reference=executable.relative_to(ROOT).as_posix()
                                       if executable.is_relative_to(ROOT) else str(executable))
                            break
                    if row['status'] == 'original_bytes':
                        break
                if row['status'] != 'original_bytes':
                    close = difflib.get_close_matches(raw, candidates, n=3, cutoff=0.5)
                    row['candidates'] = [
                        {'address': hex(candidates[c]), 'hex': c.hex(), 'text': decode(c)}
                        for c in close]
            rows.append(row)
    reviews = json.loads(REVIEW.read_text(encoding='utf-8')) if REVIEW.exists() else []
    for row in rows:
        for entry in reviews:
            if row['file'] == entry['file'] and row['hex'] == entry['hex']:
                row['review'] = entry['reason']
                break
    unresolved = [r for r in rows if r['status'] == 'invalid_big5' or
                  (r['status'] not in ('original_bytes', 'utf8_metadata') and 'review' not in r)]
    return {
        'coverage': {'source_files': source_files, 'all_string_literals': all_literals,
                     'ascii_literals': ascii_literals,
                     'big5_literals': sum(r['status'] not in ('utf8_source', 'utf8_metadata')
                                          for r in rows)},
        'references': [{'path': str(p), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                       for p in executables],
        'counts': dict(Counter(r['status'] for r in rows)),
        'unreviewed': len(unresolved), 'literals': rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT / 'assets/original/Rich4.exe')
    parser.add_argument('--reference-exe', type=Path, action='append', default=[])
    parser.add_argument('--report', type=Path, default=ROOT / 'build/original_text_audit.json')
    parser.add_argument('--check', action='store_true', help='Fail on new/unreviewed text differences')
    args = parser.parse_args()
    executables = [args.exe.resolve()] + [p.resolve() for p in args.reference_exe]
    base_game = ROOT / 'assets/original/SteamV2.06/Game/rich4.exe'
    if base_game.exists() and base_game.resolve() not in executables:
        executables.append(base_game.resolve())
    report = audit(executables)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('coverage', 'counts', 'unreviewed')}, ensure_ascii=False))
    for row in report['literals']:
        if row['status'] == 'invalid_big5' or (
                row['status'] not in ('original_bytes', 'utf8_metadata') and 'review' not in row):
            print(f"{row['file']}:{row['line']} [{row['status']}] {row['text']!r}")
            for candidate in row.get('candidates', []):
                print(f"  {candidate['address']} {candidate['text']!r}")
    if args.check and report['unreviewed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
