#!/usr/bin/env python3
"""Exercise the architecture guard's fail-then-recover behavior."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "tests" / "architecture" / "check_architecture.py"
FIXTURE = ROOT / "src" / "app" / "__architecture_guard_fixture.c"
PLATFORM_FIXTURE = ROOT / "src" / "platform" / "sdl1" / "__architecture_guard_fixture.c"


def run_checker() -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(CHECKER)],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def main() -> int:
    for fixture in (FIXTURE, PLATFORM_FIXTURE):
        if fixture.exists():
            raise SystemExit(f"fixture already exists: {fixture}")
    FIXTURE.write_text('#include "platform/sdl1/runtime_bootstrap.h"\n', encoding="utf-8")
    try:
        failed = run_checker()
        if failed.returncode == 0 or "new violation" not in failed.stderr:
            raise SystemExit("architecture guard did not reject the temporary violation")
    finally:
        FIXTURE.unlink(missing_ok=True)

    recovered = run_checker()
    if recovered.returncode != 0:
        raise SystemExit(recovered.stderr or "architecture guard did not recover")

    PLATFORM_FIXTURE.write_text(
        'void fixture(GameCommand *command) { command->value = 1; }\n',
        encoding="utf-8",
    )
    try:
        failed = run_checker()
        if failed.returncode == 0 or "command-payload-write" not in failed.stderr:
            raise SystemExit("architecture guard did not reject direct command payload write")
    finally:
        PLATFORM_FIXTURE.unlink(missing_ok=True)

    recovered = run_checker()
    if recovered.returncode != 0:
        raise SystemExit(recovered.stderr or "architecture guard did not recover")
    print("architecture guard fixture: fail/recover ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
