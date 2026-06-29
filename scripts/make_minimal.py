#!/usr/bin/env python3
"""Run Make while keeping the user-facing output intentionally small.

Normal mode captures the complete Make transcript and emits only warnings,
failure diagnostics, and a final status line.  ``--raw-output`` is provided
for machine consumers which need to inspect Make's actual output, such as the
SDL object-manifest architecture check.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LOCAL_MAKE = ROOT / ".local" / "toolchain" / "mingw64" / "bin" / "mingw32-make.exe"

DIAGNOSTIC = re.compile(
    r"(?:\berror\b|\bfatal\b|\bfailed?\b|\bfailure\b|"
    r"undefined reference|collect2\.exe?: error|recipe for target|"
    r"No such file|not found|cannot |can't |assert(?:ion)?|"
    r"AddressSanitizer|UndefinedBehaviorSanitizer|runtime error|"
    r"segmentation fault|access violation|aborted|\bstop(?:ping)?\b)",
    re.IGNORECASE,
)
WARNING = re.compile(r"\bwarning\b", re.IGNORECASE)
FORCE_REBUILD = re.compile(r"^(?:--always-make|-[^-]*B[^=]*)$")
LONG_TARGETS = {
    "sdl",
    "sdl-handheld",
    "test",
    "test-unit",
    "test-resource",
    "test-sanitize",
    "test-original-data",
}
PROGRESS_DELAY_SECONDS = 1.0
PROGRESS_INTERVAL_SECONDS = 20.0


def find_make() -> str | None:
    configured = os.environ.get("RICH4_MAKE")
    if configured:
        return configured
    if LOCAL_MAKE.is_file():
        return str(LOCAL_MAKE)
    return shutil.which("mingw32-make") or shutil.which("make")


def wrapper_args(argv: list[str]) -> tuple[bool, bool, bool, list[str]]:
    raw = False
    allow_force_rebuild = False
    progress = False
    make_args: list[str] = []
    for arg in argv:
        if arg == "--raw-output":
            raw = True
        elif arg == "--allow-force-rebuild":
            allow_force_rebuild = True
        elif arg == "--progress":
            progress = True
        elif arg == "--":
            continue
        else:
            make_args.append(arg)
    return raw, allow_force_rebuild, progress, make_args


def line_numbered_output(output: str) -> list[str]:
    return output.replace("\r\n", "\n").replace("\r", "\n").splitlines()


def selected_diagnostics(lines: list[str], failed: bool) -> list[str]:
    """Keep diagnostic lines plus a small amount of surrounding context."""

    selected: set[int] = set()
    for index, line in enumerate(lines):
        if DIAGNOSTIC.search(line) or (not failed and WARNING.search(line)):
            selected.update(range(max(0, index - 1), min(len(lines), index + 2)))

    if failed:
        # The final lines usually contain Make's target and exit summary, and
        # also cover test programs whose failure text has no standard keyword.
        selected.update(range(max(0, len(lines) - 24), len(lines)))

    return [lines[index] for index in sorted(selected) if lines[index].strip()]


def has_force_rebuild_flag(make_args: list[str]) -> bool:
    return any(FORCE_REBUILD.fullmatch(arg) for arg in make_args)


def is_long_target(make_args: list[str]) -> bool:
    return any(arg in LONG_TARGETS for arg in make_args)


def progress_stage(line: str) -> str:
    if " -c " in line:
        return "compile"
    if " -o " in line:
        return "link"
    if "bin/" in line or "bin\\" in line:
        return "test"
    return "make"


def progress_line(stage: str, elapsed: float, events: int, position: int) -> str:
    width = 24
    marker = position % (width * 2 - 2)
    if marker >= width:
        marker = width * 2 - 2 - marker
    bar = "-" * marker + ">" + "-" * (width - marker - 1)
    return f"make-minimal: [{bar}] {stage} {elapsed:.0f}s ({events} events)"


def run_captured(make: str, make_args: list[str], show_progress: bool) -> tuple[int, str]:
    try:
        process = subprocess.Popen(
            [make, *make_args],
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            errors="replace",
            bufsize=1,
        )
    except OSError as exc:
        print(f"make-minimal: cannot execute {make}: {exc}", file=sys.stderr)
        return 127, ""

    lines: list[str] = []
    state = {"stage": "make"}

    def collect_output() -> None:
        assert process.stdout is not None
        for line in process.stdout:
            lines.append(line)
            state["stage"] = progress_stage(line)
        process.stdout.close()

    reader = threading.Thread(target=collect_output, daemon=True)
    reader.start()
    started = time.monotonic()
    next_report = started + PROGRESS_DELAY_SECONDS
    position = 0
    progress_displayed = False
    interactive = sys.stdout.isatty()

    while reader.is_alive():
        now = time.monotonic()
        if show_progress and now >= next_report:
            message = progress_line(
                state["stage"], now - started, len(lines), position
            )
            if interactive:
                print(f"\r{message}", end="", flush=True)
                progress_displayed = True
            else:
                print(message, flush=True)
            position += 1
            next_report = now + (0.25 if interactive else PROGRESS_INTERVAL_SECONDS)
        reader.join(0.1)

    reader.join()
    if interactive and progress_displayed:
        print("\r" + " " * 96 + "\r", end="", flush=True)
    return process.wait(), "".join(lines)


def main(argv: list[str]) -> int:
    raw, allow_force_rebuild, requested_progress, make_args = wrapper_args(argv)
    make = find_make()
    if make is None:
        print(
            "make-minimal: cannot find mingw32-make or make "
            "(set RICH4_MAKE to override)",
            file=sys.stderr,
        )
        return 127
    if not make_args:
        make_args = ["--help"]

    if (
        not raw
        and has_force_rebuild_flag(make_args)
        and not allow_force_rebuild
    ):
        print(
            "make-minimal: refusing -B/--always-make in normal mode; "
            "use an incremental target, or add --allow-force-rebuild "
            "for an intentional clean rebuild",
            file=sys.stderr,
        )
        return 2

    if raw:
        try:
            completed = subprocess.run(
                [make, *make_args],
                cwd=ROOT,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                errors="replace",
                check=False,
            )
        except OSError as exc:
            print(f"make-minimal: cannot execute {make}: {exc}", file=sys.stderr)
            return 127
        output = completed.stdout or ""
        sys.stdout.write(output)
        return completed.returncode

    show_progress = requested_progress or is_long_target(make_args)
    returncode, output = run_captured(make, make_args, show_progress)

    lines = line_numbered_output(output)
    if returncode != 0:
        print(f"make-minimal: FAILED (exit {returncode})")
        for line in selected_diagnostics(lines, failed=True):
            print(line)
        return returncode

    for line in selected_diagnostics(lines, failed=False):
        print(line)
    print(f"make-minimal: OK ({' '.join(make_args)})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
