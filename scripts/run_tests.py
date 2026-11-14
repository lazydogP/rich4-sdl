#!/usr/bin/env python3
"""Run named tests, one process per test.

A name is resolved through the grouped runners first (``--list``) and then as
a standalone executable in the same binary directory.  Every requested test
runs even after a failure; the exit status is non-zero if any test failed.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNNERS = ("rich4_tests_core", "rich4_tests_sdl")
SKIPPED = re.compile(r": skipped \(")


def executable(bin_dir: Path, name: str) -> Path | None:
    for candidate in (bin_dir / (name + ".exe"), bin_dir / name):
        if candidate.is_file():
            return candidate
    return None


def discover(bin_dir: Path) -> dict[str, Path]:
    registry: dict[str, Path] = {}
    for runner in RUNNERS:
        path = executable(bin_dir, runner)
        if path is None:
            continue
        listed = subprocess.run([str(path), "--list"], cwd=ROOT,
                                capture_output=True, text=True, check=False)
        if listed.returncode != 0:
            raise SystemExit(f"run_tests: {path} --list failed "
                             f"(exit {listed.returncode})")
        for name in listed.stdout.split():
            registry[name] = path
    return registry


def command_for(name: str, bin_dir: Path,
                registry: dict[str, Path]) -> list[str] | None:
    if name in registry:
        return [str(registry[name]), name]
    path = executable(bin_dir, name)
    return [str(path)] if path is not None else None


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bin-dir", default="bin")
    parser.add_argument("--timeout", type=float, default=None)
    parser.add_argument("--json", type=Path, default=None)
    parser.add_argument("tests", nargs="+")
    args = parser.parse_args(argv)

    bin_dir = Path(args.bin_dir).resolve()
    registry = discover(bin_dir)
    results = []
    failed: list[str] = []
    for name in args.tests:
        command = command_for(name, bin_dir, registry)
        if command is None:
            code, output = 127, f"run_tests: no runner provides {name}"
        else:
            try:
                completed = subprocess.run(command, cwd=ROOT,
                                           capture_output=True,
                                           timeout=args.timeout, check=False)
                code = completed.returncode
                output = (completed.stdout + completed.stderr).decode(
                    "utf-8", errors="replace")
            except subprocess.TimeoutExpired:
                code, output = -1, f"Test exceeded {args.timeout:g} seconds"
        results.append({"test": name, "exit_code": code, "output": output})
        text = output.rstrip()
        if code != 0:
            failed.append(name)
            print(f"FAIL {name} (exit {code})", flush=True)
            if text:
                print(text[-5000:], flush=True)
            continue
        if text:
            print(text, flush=True)
        for line in text.splitlines():
            if SKIPPED.search(line):
                print(f"SKIP {name}: {line.strip()}", flush=True)

    if args.json is not None:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(results, indent=2, ensure_ascii=False),
                             encoding="utf-8")
    if failed:
        print(f"run_tests: {len(failed)} of {len(results)} failed: "
              + " ".join(failed), flush=True)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
